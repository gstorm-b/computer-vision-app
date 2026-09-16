#include "bench_runner.h"

#include "device/plc/mc_fame_3e.h"
#include "device/plc/mc_frame_1c.h"
#include "device/plc/mc_frame_3c.h"
#include "device/plc/mc_msg_serial_port.h"
#include "device/plc/mc_msg_tcp_client.h"

#include <QElapsedTimer>
#include <QThread>

#include <algorithm>

using namespace vc::device;
using RtCode = MCFrameAbstract::FrameReturnCode;

namespace {

/// Failures kept with their bytes, per command. Enough to see a pattern without turning the
/// report into a packet dump.
constexpr int kMaxFailureSamples = 5;

/// Poll interval while waiting for a response, in milliseconds. Small enough not to blur the
/// measurement, large enough not to spin the CPU on a slow serial line.
constexpr int kReceivePollMs = 1;

} // namespace

QString benchStageName(BenchStage stage) {
    switch (stage) {
    case BenchStage::Connect: return QStringLiteral("Connect");
    case BenchStage::Build:   return QStringLiteral("Build");
    case BenchStage::Send:    return QStringLiteral("Send");
    case BenchStage::Timeout: return QStringLiteral("Timeout");
    case BenchStage::Parse:   return QStringLiteral("Parse");
    case BenchStage::Ok:      return QStringLiteral("Ok");
    }
    return QStringLiteral("Unknown");
}

qint64 BenchCommandStat::percentileUs(int percentile) const {
    if (latenciesUs.isEmpty()) {
        return 0;
    }
    QVector<qint64> sorted = latenciesUs;
    std::sort(sorted.begin(), sorted.end());
    int index = (sorted.size() * percentile) / 100;
    if (index >= sorted.size()) {
        index = sorted.size() - 1;
    }
    return sorted.at(index);
}

qint64 BenchCommandStat::meanUs() const {
    if (latenciesUs.isEmpty()) {
        return 0;
    }
    qint64 sum = 0;
    for (const qint64 value : latenciesUs) {
        sum += value;
    }
    return sum / latenciesUs.size();
}

qint64 BenchCommandStat::minUs() const {
    if (latenciesUs.isEmpty()) return 0;
    return *std::min_element(latenciesUs.cbegin(), latenciesUs.cend());
}

qint64 BenchCommandStat::maxUs() const {
    if (latenciesUs.isEmpty()) return 0;
    return *std::max_element(latenciesUs.cbegin(), latenciesUs.cend());
}

BenchRunner::BenchRunner(QObject *parent)
    : QObject(parent) {
}

BenchRunner::~BenchRunner() {
    if (m_transport) {
        m_transport->DestroyMsgPort();
    }
}

void BenchRunner::setConfig(const BenchConfig &config) {
    m_config = config;
    // Cloned rather than shared: the GUI thread keeps editing its own context while the run is
    // in flight, and a half-applied setting mid-benchmark would make the numbers meaningless.
    m_context.reset(config.context ? config.context->clone() : nullptr);
}

void BenchRunner::requestStop() {
    m_stopRequested.storeRelaxed(1);
}

std::unique_ptr<MCFrameAbstract> BenchRunner::makeFrame() const {
    switch (m_context->frameType()) {
    case mc::McFrameType::Frame_3E: return std::make_unique<Frame3E>();
    case mc::McFrameType::Frame_1C: return std::make_unique<Frame1C>();
    case mc::McFrameType::Frame_3C: return std::make_unique<Frame3C>();
    default: return nullptr;
    }
}

std::unique_ptr<McMsgInterface> BenchRunner::makeTransport() const {
    switch (m_context->msgConfig()->type()) {
    case mc::McMsgItfType::EthernetTCPIP: return std::make_unique<McEthernetTcpPort>();
    case mc::McMsgItfType::SerialPort:    return std::make_unique<McMsgSerialPort>();
    default: return nullptr;
    }
}

/// Runs one request to completion: build, send, then poll for a response until the codec parses
/// it or the configured response timeout expires.
BenchStage BenchRunner::runOne(MCRequest &request, int iteration, BenchCommandStat &stat) {
    ++stat.sent;

    const auto record = [&](BenchStage stage, const QString &detail,
                            const QByteArray &req, const QByteArray &resp) {
        stat.failuresByStage[benchStageName(stage)] += 1;
        if (stat.samples.size() < kMaxFailureSamples) {
            stat.samples.append({iteration, stage, detail, req, resp});
        }
        return stage;
    };

    QByteArray frame;
    const RtCode built = m_frame->makeSendFrame(&request, m_context.get(), frame);
    if (built != RtCode::RequestFrameOK) {
        return record(BenchStage::Build, m_frame->lastErrorDescription(), QByteArray(), QByteArray());
    }

    // Anything still sitting in the port from a previous timeout would be read as the head of
    // this response. Clearing here is what keeps one slow answer from poisoning every
    // measurement after it.
    m_transport->clearBuffer();

    QElapsedTimer timer;
    timer.start();

    QByteArray sendBuffer = frame;
    if (m_transport->SendMsg(sendBuffer) != McMsgInterface::MsgErrorState::NoError) {
        return record(BenchStage::Send, m_transport->GetErrorDescription(), frame, QByteArray());
    }

    const int timeoutMs = m_context->msgConfig()->m_responseTimeout;
    QByteArray response;

    while (timer.elapsed() < timeoutMs) {
        m_transport->ReceiveMsg(response, kReceivePollMs);

        const RtCode parsed = m_frame->parseReceiveFrame(&request, m_context.get(), response);
        if (parsed == RtCode::WaitingReceive) {
            continue;
        }

        if (parsed == RtCode::ResponseOk) {
            stat.latenciesUs.append(timer.nsecsElapsed() / 1000);
            ++stat.ok;
            return BenchStage::Ok;
        }

        return record(BenchStage::Parse, m_frame->lastErrorDescription(), frame, response);
    }

    return record(BenchStage::Timeout,
                  QStringLiteral("no complete response within %1 ms").arg(timeoutMs),
                  frame, response);
}

void BenchRunner::run() {
    BenchReport report;
    QElapsedTimer wall;
    wall.start();

    m_stopRequested.storeRelaxed(0);

    if (!m_context) {
        report.connectError = QStringLiteral("no protocol context configured");
        emit finished(report);
        return;
    }

    m_frame = makeFrame();
    if (!m_frame) {
        report.connectError = QStringLiteral("no codec for frame %1")
                                  .arg(mc::McFrameTypeToString(m_context->frameType()));
        emit finished(report);
        return;
    }

    m_transport = makeTransport();
    if (!m_transport) {
        report.connectError = QStringLiteral("no transport for interface %1")
                                  .arg(mc::McMsgItfTypeToString(m_context->msgConfig()->type()));
        emit finished(report);
        return;
    }

    m_context->setDeviceMap(&m_deviceMap);
    m_transport->SetConfig(m_context->msgConfig());

    emit logLine(QStringLiteral("Opening %1 over %2…")
                     .arg(mc::McFrameTypeToString(m_context->frameType()),
                          mc::McMsgItfTypeToString(m_context->msgConfig()->type())));

    if (m_transport->ConnectToPort() != McMsgInterface::MsgIfState::Connected) {
        report.connectError = m_transport->GetErrorDescription();
        emit logLine(QStringLiteral("Connect failed: %1").arg(report.connectError));
        m_transport->DestroyMsgPort();
        m_transport.reset();
        emit finished(report);
        return;
    }

    report.connected = true;
    emit logLine(QStringLiteral("Connected. Running %1 iterations.").arg(m_config.iterations));

    const int bitStart = m_context->startMAddress();
    const int bitAmount = m_context->amountMAddress();
    const int wordStart = m_context->startDAddress();
    const int wordAmount = m_context->amountDAddress();

    BenchCommandStat readBit;
    readBit.name = QStringLiteral("Read bit M%1 x%2").arg(bitStart).arg(bitAmount);
    BenchCommandStat readWord;
    readWord.name = QStringLiteral("Read word D%1 x%2").arg(wordStart).arg(wordAmount);
    BenchCommandStat writeBit;
    writeBit.name = QStringLiteral("Write bit M%1 = %2").arg(bitStart).arg(m_config.writeBitValue);
    BenchCommandStat writeWord;
    writeWord.name = QStringLiteral("Write word D%1 = %2").arg(wordStart).arg(m_config.writeWordValue);

    for (int i = 0; i < m_config.iterations; ++i) {
        if (m_stopRequested.loadRelaxed() != 0) {
            emit logLine(QStringLiteral("Stopped after %1 iterations.").arg(i));
            break;
        }

        if (m_config.readBit) {
            MCRequest request(MCRequest::RqType::ReadBit, 'M', bitStart, bitAmount);
            runOne(request, i, readBit);
        }
        if (m_config.readWord) {
            MCRequest request(MCRequest::RqType::ReadWord, 'D', wordStart, wordAmount);
            runOne(request, i, readWord);
        }
        if (m_config.writeBit) {
            MCRequest request(MCRequest::RqType::WriteBit, 'M', bitStart, 1);
            request.buildWriteData_Bit_Device(m_config.writeBitValue);
            runOne(request, i, writeBit);
        }
        if (m_config.writeWord) {
            MCRequest request(MCRequest::RqType::WriteWord, 'D', wordStart, 1);
            request.buildWriteData_Word_Device_Word(m_config.writeWordValue);
            runOne(request, i, writeWord);
        }

        emit progress(i + 1, m_config.iterations);
    }

    if (m_config.readBit)    report.commands.append(readBit);
    if (m_config.readWord)   report.commands.append(readWord);
    if (m_config.writeBit)   report.commands.append(writeBit);
    if (m_config.writeWord)  report.commands.append(writeWord);

    for (const BenchCommandStat &stat : report.commands) {
        report.totalRequests += stat.sent;
        report.totalOk += stat.ok;
    }
    report.wallClockMs = wall.elapsed();

    m_transport->DisconnectFromPort();
    m_transport->DestroyMsgPort();
    m_transport.reset();
    m_frame.reset();

    emit logLine(QStringLiteral("Done: %1 of %2 requests completed in %3 ms.")
                     .arg(report.totalOk).arg(report.totalRequests).arg(report.wallClockMs));
    emit finished(report);
}
