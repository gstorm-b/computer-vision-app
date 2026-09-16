/**
 * @file modbus_tcp_client_device.cpp
 * @brief Implementation of the Modbus TCP client PLC device.
 */

#include "device/plc/modbus/modbus_tcp_client_device.h"

#include "core/logger/app_logger.h"
#include "device/plc/modbus/modbus_result_layout.h"
#include "device/plc/modbus/modbus_trace.h"

#include <QElapsedTimer>

#include <QEventLoop>
#include <QModbusDataUnit>
#include <QModbusPdu>
#include <QModbusReply>
#include <QModbusTcpClient>
#include <QPointer>
#include <QScopeGuard>
#include <QThread>
#include <QVariant>

#include <utility>

namespace vc::device {

namespace {

/// Modbus per-request ceilings, from the protocol specification. Exceeding them is not a
/// performance question — the slave rejects the request.
constexpr int kMaxBitsPerRead      = 2000;
constexpr int kMaxRegistersPerRead = 125;
constexpr int kMaxRegistersPerWrite = 123;

/// Ceiling on writes replayed in one drain. A localization cycle's largest burst is
/// publishInitialReadyOutputs() at ten, so this is far above any legitimate load and exists only
/// so a task publishing pathologically fast cannot spin the device thread.
constexpr int kMaxDeferredWriteReplays = 256;

/// Builds the failure text for a reply that came back in error.
///
/// Qt's own errorString() for a protocol failure is the bare "Modbus Exception Response." — it
/// says an exception arrived and not which one, which is the single most useful fact there is.
/// The exception code is carried on the raw PDU, so it is pulled out here and named.
///
/// @note Gated on isException()/functionCode(), NOT on QModbusPdu::isValid(): an exception
///       response does not satisfy isValid(), so gating on it silently dropped exactly the
///       detail this function exists to add.
QString describeFailure(const QString &reason, const QModbusResponse &raw)
{
    QString detail = reason;
    if (raw.isException()) {
        detail += QStringLiteral(" | %1").arg(modbus_trace::exceptionName(raw.exceptionCode()));
    }
    if (raw.functionCode() != QModbusPdu::Invalid) {
        detail += QStringLiteral(" | response %1").arg(modbus_trace::describePdu(raw));
    }
    return detail;
}

/// Maps our area enum onto Qt's.
QModbusDataUnit::RegisterType toQtRegisterType(ModbusArea area)
{
    switch (area) {
    case ModbusArea::Coils:            return QModbusDataUnit::Coils;
    case ModbusArea::DiscreteInputs:   return QModbusDataUnit::DiscreteInputs;
    case ModbusArea::HoldingRegisters: return QModbusDataUnit::HoldingRegisters;
    case ModbusArea::InputRegisters:   return QModbusDataUnit::InputRegisters;
    }
    return QModbusDataUnit::Invalid;
}

} // namespace

ModbusTcpClientDevice::ModbusTcpClientDevice(QString id, QString name, QObject *parent)
    : PlcDevice(std::move(id), std::move(name), parent)
{
    // IDevice keeps a NON-owning config pointer and writes through it without calling
    // setDeviceConfig(), so the concrete device must publish its own member here or a loaded
    // project would write into nothing. See src/device/AGENTS.md.
    setDeviceConfig(&m_config);
    rebuildRegisterMap();
}

ModbusTcpClientDevice::~ModbusTcpClientDevice()
{
    // The client is torn down by deviceTerminate()/deviceDisconnect() on the worker thread
    // before destruction; this destructor may run on another thread, where deleting a
    // thread-affine QModbusTcpClient would be undefined.
    delete m_client;
    m_client = nullptr;
}

// ── Configuration ────────────────────────────────────────────────────────────────────────────

void ModbusTcpClientDevice::setDeviceConfig(IDeviceCfg *cfg)
{
    auto *modbusCfg = dynamic_cast<ModbusTcpClientCfg *>(cfg);
    if (!modbusCfg) {
        // Not an error worth failing on when called from the constructor with our own member.
        if (cfg != &m_config) {
            LOG_DEV_ERR << "ModbusTcpClientDevice: config is not a ModbusTcpClientCfg."
                        << "deviceId=" << id();
            return;
        }
    } else if (isDeviceConnected()) {
        // One exception to "settings are ignored while connected": the protocol trace. A
        // debugging switch you have to drop the link to use is a switch you cannot use on the
        // fault you are chasing.
        if (m_config.m_traceProtocol != modbusCfg->m_traceProtocol) {
            m_config.m_traceProtocol = modbusCfg->m_traceProtocol;
            LOG_USER_INFO << "Modbus protocol trace"
                          << (m_config.m_traceProtocol ? "ENABLED" : "disabled")
                          << "deviceId=" << id();
        }
        LOG_USER_WARN << "Modbus client configuration ignored while connected"
                      << "(protocol trace excepted)." << "deviceId=" << id();
        return;
    } else {
        m_config = *modbusCfg;
    }

    PlcDevice::setDeviceConfig(&m_config);
    rebuildRegisterMap();
}

bool ModbusTcpClientDevice::fromJson(const QJsonObject &obj)
{
    const bool ok = IDevice::fromJson(obj);
    rebuildRegisterMap();
    return ok;
}

void ModbusTcpClientDevice::rebuildRegisterMap()
{
    m_config.configureRegisterMap(m_map);
    rebuildPollPlan();

    if (m_config.overlapsPolledResultRange()) {
        // Allowed, but almost never wanted: the poll would read back the words the vision result
        // just wrote and the signal map would fill with encoded pose halves.
        LOG_USER_WARN << "Modbus result block overlaps the polled holding range."
                      << "deviceId=" << id()
                      << "resultStart=" << m_config.m_resultStartAddress
                      << "holdingStart=" << m_config.m_holdingStart
                      << "holdingCount=" << m_config.m_holdingCount;
    }
}

void ModbusTcpClientDevice::rebuildPollPlan()
{
    m_pollPlan.clear();

    for (const ModbusArea area : kModbusAreas) {
        const ModbusRange range = m_config.range(area);
        if (range.count <= 0) {
            continue;
        }
        const int chunkSize =
            modbus_tags::isBitArea(area) ? kMaxBitsPerRead : kMaxRegistersPerRead;
        for (int offset = 0; offset < range.count; offset += chunkSize) {
            ReadChunk chunk;
            chunk.area = area;
            chunk.start = range.start + offset;
            chunk.count = qMin(chunkSize, range.count - offset);
            m_pollPlan.append(chunk);
        }
    }

    // Logged once per (re)configure, not per poll: this is the single most useful line when a
    // peer answers IllegalDataAddress, because it says exactly which ranges we will ask for. An
    // area the peer does not map — very common on a real PLC, which rarely exposes all four —
    // shows up here as a range to delete from the configuration.
    QStringList plan;
    plan.reserve(m_pollPlan.size());
    for (const ReadChunk &chunk : std::as_const(m_pollPlan)) {
        plan << modbus_trace::describeRange(chunk.area, chunk.start, chunk.count);
    }
    LOG_USER_INFO << "Modbus client poll plan."
                  << "deviceId=" << id()
                  << "unitId=" << m_config.m_unitId
                  << "requests=" << m_pollPlan.size()
                  << "ranges=" << plan.join(QStringLiteral("; "));
}

// ── Connection lifecycle ─────────────────────────────────────────────────────────────────────

bool ModbusTcpClientDevice::deviceConnect()
{
    if (isDeviceConnected()) {
        setConnectionStatus(ConnectStatus::Connected);
        return true;
    }

    delete m_client;
    m_client = new QModbusTcpClient(this);
    m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                     m_config.m_hostAddress);
    m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, m_config.m_port);
    m_client->setTimeout(m_config.m_responseTimeout);
    // Qt's own retry is per request; ours counts consecutive failed transactions and decides
    // when the link is gone. Leaving Qt's at 0 keeps the two from multiplying.
    m_client->setNumberOfRetries(0);

    connect(m_client, &QModbusDevice::errorOccurred, this,
            [this](QModbusDevice::Error error) {
        if (error != QModbusDevice::NoError) {
            LOG_USER_WARN << "Modbus client transport error."
                          << "deviceId=" << id()
                          << "error=" << int(error)
                          << "reason=" << (m_client ? m_client->errorString() : QString());
        }
    });

    connect(m_client, &QModbusDevice::stateChanged, this,
            [this](QModbusDevice::State state) {
        // Every transition, not only the loss: a link that flaps is invisible otherwise.
        LOG_DEV_INFO << "Modbus client transport state." << "deviceId=" << id()
                     << "state=" << int(state);
        if (state == QModbusDevice::UnconnectedState && isDeviceConnected()) {
            // The slave dropped us. LostConnected (not Disconnected) is what starts the
            // runtime's recovery policy — the same contract McProtocolDevice meets.
            LOG_USER_WARN << "Modbus client link lost." << "deviceId=" << id();
            teardown(ConnectStatus::LostConnected);
        }
    });

    setConnectionStatus(ConnectStatus::Connecting);

    if (!m_client->connectDevice()) {
        const QString reason = m_client->errorString();
        LOG_USER_ERR << "Modbus client connect failed."
                     << "deviceId=" << id()
                     << "host=" << m_config.m_hostAddress
                     << "port=" << m_config.m_port
                     << "reason=" << reason;
        delete m_client;
        m_client = nullptr;
        setConnectionStatus(ConnectStatus::ConnectFailed, reason);
        return false;
    }

    // connectDevice() is asynchronous; wait for the state to settle so the caller's return value
    // means what it says. Bounded by the response timeout.
    if (m_client->state() != QModbusDevice::ConnectedState) {
        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(m_client, &QModbusDevice::stateChanged, &loop,
                [&loop](QModbusDevice::State state) {
            if (state == QModbusDevice::ConnectedState
                || state == QModbusDevice::UnconnectedState) {
                loop.quit();
            }
        });
        guard.start(m_config.m_responseTimeout);
        loop.exec();
    }

    if (!m_client || m_client->state() != QModbusDevice::ConnectedState) {
        const QString reason = m_client ? m_client->errorString()
                                        : QStringLiteral("Modbus client was torn down.");
        LOG_USER_ERR << "Modbus client did not reach the connected state."
                     << "deviceId=" << id() << "reason=" << reason;
        delete m_client;
        m_client = nullptr;
        setConnectionStatus(ConnectStatus::ConnectFailed, reason);
        return false;
    }

    // Values read from a freshly-connected slave are a starting state, not edges: without this
    // every task signal that happens to be true would fire a rising edge at connect.
    m_map.resetChangeBaseline();
    m_consecutiveFailures = 0;

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &ModbusTcpClientDevice::onPollTimeout);
    }
    m_pollTimer->start(m_config.m_refreshInterval);

    LOG_USER_INFO << "Modbus client connected."
                  << "deviceId=" << id()
                  << "host=" << m_config.m_hostAddress
                  << "port=" << m_config.m_port
                  << "unitId=" << m_config.m_unitId;

    setConnectionStatus(ConnectStatus::Connected);
    return true;
}

bool ModbusTcpClientDevice::deviceDisconnect()
{
    teardown(ConnectStatus::Disconnected);
    return true;
}

void ModbusTcpClientDevice::deviceTerminate()
{
    if (connectStatus() == ConnectStatus::Connected
        || connectStatus() == ConnectStatus::Connecting
        || connectStatus() == ConnectStatus::LostConnected) {
        deviceDisconnect();
    }
}

void ModbusTcpClientDevice::teardown(ConnectStatus finalStatus)
{
    if (m_inTransaction) {
        // A nested wait is running on this thread. Deleting the client now would destroy the
        // reply that wait is holding; defer instead and let the transaction unwind first.
        m_teardownPending = true;
        m_pendingStatus = finalStatus;
        return;
    }

    if (m_pollTimer) {
        m_pollTimer->stop();
    }
    if (m_client) {
        m_client->disconnect(this);
        m_client->disconnectDevice();
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_map.resetChangeBaseline();
    m_consecutiveFailures = 0;
    // Anything parked is now unreplayable — the client is gone. Failing it is what stops the
    // caller waiting on a write that can never be attempted.
    failPendingIoWrites(
        finalStatus == ConnectStatus::LostConnected
            ? QStringLiteral("Abandoned: the Modbus link was lost before the write was sent.")
            : QStringLiteral("Abandoned: the device was disconnected before the write was sent."));
    setConnectionStatus(finalStatus);
}

// ── Transactions ─────────────────────────────────────────────────────────────────────────────

bool ModbusTcpClientDevice::assertOnDeviceThread(const QString &what) const
{
    if (QThread::currentThread() == thread()) {
        return true;
    }
    // QModbusTcpClient owns a QTcpSocket that belongs to this device's worker thread. Sending
    // from another thread raises no error — the frame simply never reaches the wire, so the peer
    // logs nothing (nothing arrived) and the caller waits out the full response timeout. Worse, it
    // is intermittent. That shipped once: the device panel called writeDigitalIoByName() straight
    // from the GUI thread instead of going through PlcRunner's queued write triggers, and the
    // symptom was "the server does not answer my coil write". One comparison turns that into an
    // immediate, attributable failure.
    LOG_USER_ERR << "Modbus request issued from the wrong thread; it must be queued onto the"
                 << "device thread (use PlcRunner)."
                 << "deviceId=" << id() << "request=" << what;
    return false;
}

bool ModbusTcpClientDevice::transact(const QModbusDataUnit &unit, bool write,
                                     QModbusDataUnit *replyUnit, QString *error)
{
    // Every failure path logs the request that produced it. Without this the log says only that
    // "the request failed" and the reader has to guess which of a dozen requests per second it
    // was, and against which address range.
    const QString requestText =
        QStringLiteral("%1 %2 unitId=%3")
            .arg(write ? QStringLiteral("WRITE") : QStringLiteral("READ"),
                 modbus_trace::describeUnit(unit, write))
            .arg(m_config.m_unitId);

    const auto fail = [this, error, &requestText](const QString &reason) {
        if (error) {
            *error = reason;
        }
        LOG_USER_ERR << "Modbus request failed."
                     << "deviceId=" << id()
                     << "request=" << requestText
                     << "reason=" << reason;
        return false;
    };

    // Backstop for the entry-point guards: nothing may reach the socket from another thread.
    if (!assertOnDeviceThread(requestText)) {
        return fail(QStringLiteral("Modbus request issued from the wrong thread; it must be "
                                   "queued onto the device thread (use PlcRunner)."));
    }
    if (!m_client || m_client->state() != QModbusDevice::ConnectedState) {
        return fail(QStringLiteral("Modbus client is not connected."));
    }
    if (m_inTransaction) {
        // Re-entrancy DOES happen, contrary to what this comment used to claim. transact() waits
        // in a nested QEventLoop, and exec(ExcludeUserInputEvents) still dispatches the queued
        // meta-calls PlcRunner posts for every other write — so a burst of publishes lands inside
        // the first write's wait. publishInitialReadyOutputs() alone is ten of them.
        //
        // Flagged as a LOCAL collision so noteTransactionFailure() does not spend the link's retry
        // budget on it: the socket is healthy and the peer was never contacted. Counting it lost
        // the link on collision four and turned "a write was refused" into "the client
        // disconnected itself". pollOnce() has always treated the same condition as "not now"
        // rather than as a failure; this makes the write path agree with it.
        m_lastFailureWasLocalCollision = true;
        return fail(QStringLiteral("A Modbus transaction is already in flight."));
    }

    if (m_config.m_traceProtocol) {
        LOG_DEV_INFO << "Modbus ->" << "deviceId=" << id() << requestText;
    }

    QElapsedTimer elapsed;
    elapsed.start();

    QModbusReply *reply = write
        ? m_client->sendWriteRequest(unit, m_config.m_unitId)
        : m_client->sendReadRequest(unit, m_config.m_unitId);

    if (!reply) {
        // No reply object at all: the request was rejected locally before anything went out.
        // The usual cause is a data unit the protocol cannot express — for example a write to a
        // register type a master has no function code for.
        return fail(QStringLiteral("Request was not sent: %1").arg(m_client->errorString()));
    }
    if (reply->isFinished()) {
        // Broadcast replies complete immediately and carry no data.
        const bool ok = reply->error() == QModbusDevice::NoError;
        const QString reason = reply->errorString();
        const QModbusResponse raw = reply->rawResult();
        reply->deleteLater();
        if (ok) {
            return true;
        }
        return fail(describeFailure(reason, raw));
    }

    QPointer<QModbusReply> guardedReply(reply);
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
    connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);

    m_inTransaction = true;
    guard.start(m_config.m_responseTimeout + 100);
    // ExcludeUserInputEvents keeps stray INPUT out; it does NOT keep queued meta-calls out, and
    // the runner posts one per write. Those are parked by the write methods while m_inTransaction
    // is set, and replayed the moment this wait unwinds.
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    m_inTransaction = false;

    if (m_teardownPending) {
        // A disconnect arrived while we were waiting; honour it now that the wait has unwound.
        m_teardownPending = false;
        if (guardedReply) {
            guardedReply->deleteLater();
        }
        teardown(m_pendingStatus);
        return fail(QStringLiteral("Modbus client was disconnected during the request."));
    }

    if (!guardedReply) {
        return fail(QStringLiteral("Modbus reply was destroyed before completing."));
    }
    if (!guardedReply->isFinished()) {
        guardedReply->deleteLater();
        return fail(QStringLiteral("Modbus request timed out after %1 ms")
                        .arg(elapsed.elapsed()));
    }
    if (guardedReply->error() != QModbusDevice::NoError) {
        // The decisive line for an exception response: the peer answered, and said why. The raw
        // PDU carries the exception code, which names what is wrong far better than Qt's generic
        // "Modbus Exception Response" string does.
        const QString reason = guardedReply->errorString();
        const QModbusResponse raw = guardedReply->rawResult();
        guardedReply->deleteLater();
        return fail(QStringLiteral("%1 (after %2 ms)")
                        .arg(describeFailure(reason, raw))
                        .arg(elapsed.elapsed()));
    }

    const QModbusDataUnit result = guardedReply->result();
    if (m_config.m_traceProtocol) {
        LOG_DEV_INFO << "Modbus <-" << "deviceId=" << id()
                     << (write ? QStringLiteral("WRITE ok") : modbus_trace::describeUnit(result, true))
                     << "elapsedMs=" << elapsed.elapsed();
    }
    if (replyUnit) {
        *replyUnit = result;
    }
    guardedReply->deleteLater();
    return true;
}

void ModbusTcpClientDevice::noteTransactionFailure(const QString &reason)
{
    if (m_lastFailureWasLocalCollision) {
        // Consumed here, one shot. A collision means the request never reached the wire, so it
        // says nothing about the link's health and must not count toward the teardown budget.
        // Only this one condition is exempt: a genuine timeout or exception response still counts,
        // or a dead link is never declared lost and the runtime's recovery never starts.
        m_lastFailureWasLocalCollision = false;
        LOG_DEV_INFO << "Modbus write collided with a transaction already in flight; not counted"
                     << "against the link retry budget."
                     << "deviceId=" << id() << "reason=" << reason;
        return;
    }

    ++m_consecutiveFailures;
    if (m_consecutiveFailures <= m_config.m_retryCount) {
        LOG_DEV_INFO << "Modbus transaction failed, retrying."
                     << "deviceId=" << id()
                     << "attempt=" << m_consecutiveFailures
                     << "of=" << m_config.m_retryCount
                     << "reason=" << reason;
        return;
    }

    LOG_USER_WARN << "Modbus link declared lost after repeated failures."
                  << "deviceId=" << id()
                  << "attempts=" << m_consecutiveFailures
                  << "reason=" << reason;
    teardown(ConnectStatus::LostConnected);
}

void ModbusTcpClientDevice::noteTransactionSuccess()
{
    m_consecutiveFailures = 0;
}

// ── Polling ──────────────────────────────────────────────────────────────────────────────────

void ModbusTcpClientDevice::onPollTimeout()
{
    if (m_inTransaction || !isDeviceConnected()) {
        return;
    }

    // A poll pass is several transactions, and a write arriving inside any of their nested waits
    // is parked. Draining on the way out is what stops a write from waiting a whole poll interval
    // — or, if the pass is abandoned below, from waiting indefinitely.
    const auto drainOnExit = qScopeGuard([this]() { drainPendingIoWrites(); });

    QElapsedTimer passTimer;
    passTimer.start();

    int chunkIndex = 0;
    for (const ReadChunk &chunk : std::as_const(m_pollPlan)) {
        ++chunkIndex;
        QModbusDataUnit request(toQtRegisterType(chunk.area), chunk.start, chunk.count);
        QModbusDataUnit response;
        QString error;
        if (!transact(request, /*write*/ false, &response, &error)) {
            // Naming the chunk matters: the pass is abandoned here, so every later range in the
            // plan goes unread, and without this line it looks as though the whole peer is down
            // when in fact one area is unmapped.
            LOG_USER_ERR << "Modbus poll pass abandoned."
                         << "deviceId=" << id()
                         << "failedAt=" << modbus_trace::describeRange(chunk.area, chunk.start,
                                                                       chunk.count)
                         << "request=" << chunkIndex << "of" << m_pollPlan.size()
                         << "remainingRangesUnread=" << (m_pollPlan.size() - chunkIndex);
            noteTransactionFailure(error);
            return;   // abandon the pass; the next tick starts a fresh one
        }

        if (modbus_tags::isBitArea(chunk.area)) {
            QList<bool> bits;
            bits.reserve(int(response.valueCount()));
            for (uint i = 0; i < response.valueCount(); ++i) {
                bits.append(response.value(i) != 0);
            }
            m_map.setBits(chunk.area, int(response.startAddress()), bits);
        } else {
            QList<quint16> words;
            words.reserve(int(response.valueCount()));
            for (uint i = 0; i < response.valueCount(); ++i) {
                words.append(response.value(i));
            }
            m_map.setWords(chunk.area, int(response.startAddress()), words);
        }
    }

    noteTransactionSuccess();

    if (m_config.m_traceProtocol) {
        LOG_DEV_INFO << "Modbus poll pass complete." << "deviceId=" << id()
                     << "requests=" << m_pollPlan.size()
                     << "elapsedMs=" << passTimer.elapsed();
    }

    publishPollResults();
}

void ModbusTcpClientDevice::publishPollResults()
{
    const QMap<QString, QVariant> changed = m_map.takeChangedValues();
    if (!changed.isEmpty()) {
        // The task's signal edges come from here, so what changed is worth seeing without
        // enabling the full protocol trace.
        LOG_DEV_INFO << "Modbus values changed." << "deviceId=" << id()
                     << "count=" << changed.size()
                     << "values=" << modbus_trace::describeValues(changed);
        emit valueChanged(changed);
    }
    emit pollingUpdate(m_map.clone());
}

// ── Tag provider / writer ────────────────────────────────────────────────────────────────────

QStringList ModbusTcpClientDevice::availableDigitalIoNames() const
{
    return m_map.digitalTagNames();
}

QStringList ModbusTcpClientDevice::availableWordIoNames() const
{
    return m_map.wordTagNames();
}

bool ModbusTcpClientDevice::writeDigitalIoByName(const QString &tag, bool value)
{
    // Captured FIRST, before anything below can re-enter this method through transact()'s nested
    // event loop and clobber the member. Every exit path below resolves this local, and
    // resolveIoWrite(0, ...) is a no-op, so an untracked caller behaves exactly as before.
    const quint64 trackedId = takeIncomingTrackedId();

    ModbusArea area = ModbusArea::Coils;
    int address = 0;
    if (!modbus_tags::parse(tag, &area, &address) || !modbus_tags::isBitArea(area)) {
        LOG_USER_ERR << "Modbus digital write rejected: not a bit tag."
                     << "deviceId=" << id() << "tag=" << tag;
        resolveIoWrite(trackedId, false,
                       QStringLiteral("\"%1\" is not a bit tag.").arg(tag));
        return false;
    }
    if (!modbus_tags::isMasterWritable(area)) {
        // Not our restriction: the protocol defines no function code by which a master can write
        // a discrete input. Only the server that owns the space can put a value there.
        LOG_USER_ERR << "Modbus digital write rejected: a master cannot write discrete inputs."
                     << "deviceId=" << id() << "tag=" << tag;
        resolveIoWrite(trackedId, false,
                       QStringLiteral("A Modbus master cannot write discrete input \"%1\".")
                           .arg(tag));
        return false;
    }
    if (!m_map.isConfigured(area, address)) {
        LOG_USER_ERR << "Modbus digital write rejected: address outside the configured span."
                     << "deviceId=" << id() << "tag=" << tag;
        resolveIoWrite(trackedId, false,
                       QStringLiteral("\"%1\" is outside the configured span.").arg(tag));
        return false;
    }

    // Checked here as well as in transact(): a caller on the wrong thread is a wiring mistake, not
    // a link fault, and must not spend the retry budget that tears a healthy link down.
    if (!assertOnDeviceThread(QStringLiteral("digital write %1").arg(tag))) {
        resolveIoWrite(trackedId, false,
                       QStringLiteral("The write was issued from the wrong thread."));
        return false;
    }

    // Deferred, not refused, when the wire is busy. transact() waits in a nested QEventLoop and
    // that loop dispatches the queued meta-calls PlcRunner posts for every other write, so a burst
    // of publishes lands inside the first write's wait. Refusing them here — which is what used to
    // happen, one layer down in transact() — silently dropped nine writes out of ten in a
    // publishInitialReadyOutputs() burst, and the runtime's outputs simply never reached the PLC.
    //
    // Parked AFTER validation so a malformed or out-of-span tag still fails immediately and
    // loudly; only a well-formed write to a busy wire waits.
    if (m_inTransaction) {
        // NOT resolved here: the write has not been attempted. The id rides the park and is
        // resolved by the replay, so what the caller hears is the outcome of the write that
        // actually happened rather than the "queued" the bool return has always meant.
        m_pendingIoWrites.append({ /*isBit*/ true, tag, value, 0, trackedId });
        return true;
    }

    QModbusDataUnit unit(QModbusDataUnit::Coils, address, 1);
    unit.setValue(0, value ? 1 : 0);
    QString error;
    if (!transact(unit, /*write*/ true, nullptr, &error)) {
        // A FAILED write is still reported, never queued: retrying it when the link returns would
        // fire at an unpredictable moment against a machine that has moved on. Deferring a write
        // that collided with our own in-flight transaction (above) is a different thing entirely —
        // that one has not been attempted yet and the wire is healthy.
        LOG_USER_ERR << "Modbus digital write failed."
                     << "deviceId=" << id() << "tag=" << tag << "reason=" << error;
        noteTransactionFailure(error);
        resolveIoWrite(trackedId, false, error);
        drainPendingIoWrites();
        return false;
    }
    noteTransactionSuccess();
    m_map.setBit(area, address, value);
    resolveIoWrite(trackedId, true, QStringLiteral("OK"));
    drainPendingIoWrites();
    return true;
}

bool ModbusTcpClientDevice::writeWordIoByName(const QString &tag, qint16 value)
{
    const quint64 trackedId = takeIncomingTrackedId();   // see writeDigitalIoByName()

    ModbusArea area = ModbusArea::HoldingRegisters;
    int address = 0;
    if (!modbus_tags::parse(tag, &area, &address) || modbus_tags::isBitArea(area)) {
        LOG_USER_ERR << "Modbus word write rejected: not a register tag."
                     << "deviceId=" << id() << "tag=" << tag;
        resolveIoWrite(trackedId, false,
                       QStringLiteral("\"%1\" is not a register tag.").arg(tag));
        return false;
    }
    if (!modbus_tags::isMasterWritable(area)) {
        LOG_USER_ERR << "Modbus word write rejected: a master cannot write input registers."
                     << "deviceId=" << id() << "tag=" << tag;
        resolveIoWrite(trackedId, false,
                       QStringLiteral("A Modbus master cannot write input register \"%1\".")
                           .arg(tag));
        return false;
    }
    if (!m_map.isConfigured(area, address)) {
        LOG_USER_ERR << "Modbus word write rejected: address outside the configured span."
                     << "deviceId=" << id() << "tag=" << tag;
        resolveIoWrite(trackedId, false,
                       QStringLiteral("\"%1\" is outside the configured span.").arg(tag));
        return false;
    }

    if (!assertOnDeviceThread(QStringLiteral("word write %1").arg(tag))) {
        resolveIoWrite(trackedId, false,
                       QStringLiteral("The write was issued from the wrong thread."));
        return false;
    }

    // Deferred rather than refused while the wire is busy — see the note in writeDigitalIoByName().
    if (m_inTransaction) {
        m_pendingIoWrites.append({ /*isBit*/ false, tag, false, value, trackedId });
        return true;
    }

    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, address, 1);
    unit.setValue(0, static_cast<quint16>(value));
    QString error;
    if (!transact(unit, /*write*/ true, nullptr, &error)) {
        LOG_USER_ERR << "Modbus word write failed."
                     << "deviceId=" << id() << "tag=" << tag << "reason=" << error;
        noteTransactionFailure(error);
        resolveIoWrite(trackedId, false, error);
        drainPendingIoWrites();
        return false;
    }
    noteTransactionSuccess();
    m_map.setWord(area, address, static_cast<quint16>(value));
    resolveIoWrite(trackedId, true, QStringLiteral("OK"));
    drainPendingIoWrites();
    return true;
}

void ModbusTcpClientDevice::writeDigitalIoTracked(quint64 id, const QString &tag, bool value)
{
    m_incomingTrackedId = id;
    writeDigitalIoByName(tag, value);   // resolves the id itself, or parks it for the replay
    m_incomingTrackedId = 0;            // belt and braces if an early return skipped the take
}

void ModbusTcpClientDevice::writeWordIoTracked(quint64 id, const QString &tag, qint16 value)
{
    m_incomingTrackedId = id;
    writeWordIoByName(tag, value);
    m_incomingTrackedId = 0;
}

/// Fails every parked write with `reason`. Called where the queue is abandoned rather than
/// replayed — a dropped write that reports nothing leaves its caller waiting forever, which is
/// the whole failure mode this phase exists to remove.
void ModbusTcpClientDevice::failPendingIoWrites(const QString &reason)
{
    if (m_pendingIoWrites.isEmpty()) {
        return;
    }
    const QList<PendingIoWrite> abandoned = m_pendingIoWrites;
    m_pendingIoWrites.clear();
    for (const PendingIoWrite &pending : abandoned) {
        resolveIoWrite(pending.trackedId, false, reason);
    }
}

void ModbusTcpClientDevice::drainPendingIoWrites()
{
    // Re-entrant calls are no-ops. A replayed write runs its own transact(), and writes arriving
    // inside THAT nested wait are parked again — the while loop below picks them up, so the queue
    // is drained iteratively rather than by recursing one stack frame per deferred write.
    if (m_draining || m_suppressPendingDrain || m_pendingIoWrites.isEmpty()) {
        return;
    }

    m_draining = true;
    int replayed = 0;
    while (!m_pendingIoWrites.isEmpty() && replayed < kMaxDeferredWriteReplays) {
        const PendingIoWrite pending = m_pendingIoWrites.takeFirst();
        ++replayed;
        // The id rides the replay, so the completion reports what the write actually did.
        m_incomingTrackedId = pending.trackedId;
        if (pending.isBit) {
            writeDigitalIoByName(pending.tag, pending.bitValue);
        } else {
            writeWordIoByName(pending.tag, pending.wordValue);
        }
        m_incomingTrackedId = 0;
    }

    if (!m_pendingIoWrites.isEmpty()) {
        // A bound rather than a spin. Reaching it means writes are being posted faster than the
        // link can carry them, which is a commissioning problem (poll interval, timeout, or a
        // task publishing far too often) and not something to hide by looping forever.
        LOG_USER_WARN << "Modbus client dropped deferred writes: they are arriving faster than the"
                      << "link can carry them."
                      << "deviceId=" << id()
                      << "replayed=" << replayed
                      << "dropped=" << m_pendingIoWrites.size();
        failPendingIoWrites(
            QStringLiteral("Dropped: deferred writes are arriving faster than the link can "
                           "carry them."));
    }
    m_draining = false;
}

bool ModbusTcpClientDevice::pushRequest(IRequest *request)
{
    Q_UNUSED(request);
    LOG_DEV_ERR << "ModbusTcpClientDevice::pushRequest is not used; results go through"
                << "IResultOutputDevice and IO through IPlcIoWriter." << "deviceId=" << id();
    return false;
}

// ── Result publication ───────────────────────────────────────────────────────────────────────

bool ModbusTcpClientDevice::sendVisionResult(const QVector<VisionOutputPosition> &positions,
                                             QString *message)
{
    const auto fail = [this, message](const QString &reason) {
        if (message) {
            *message = reason;
        }
        LOG_USER_ERR << "Modbus result publish failed."
                     << "deviceId=" << id() << "reason=" << reason;
        return false;
    };

    if (!isDeviceConnected()) {
        return fail(QStringLiteral("Modbus client is not connected."));
    }
    if (m_config.m_resultMaxPositions <= 0) {
        return fail(QStringLiteral("Result block capacity is zero; nothing can be published."));
    }
    if (!assertOnDeviceThread(QStringLiteral("result publish"))) {
        return fail(QStringLiteral("Result publish issued from the wrong thread."));
    }

    const int base = m_config.m_resultStartAddress;

    // Polling must not interleave with the publish: a master that read the block between our
    // register writes would see a half-written result, and the count word is the only thing that
    // makes the sequence safe. Stopping the timer is not enough on its own — transact() runs a
    // nested event loop, which is exactly where a timer tick would fire.
    const bool wasPolling = m_pollTimer && m_pollTimer->isActive();
    if (wasPolling) {
        m_pollTimer->stop();
    }
    const auto restorePolling = qScopeGuard([this, wasPolling]() {
        if (wasPolling && m_pollTimer && isDeviceConnected()) {
            m_pollTimer->start(m_config.m_refreshInterval);
        }
    });

    // Deferred tag writes must not interleave either, and for exactly the same reason as the poll.
    // Each transact() below unwinds fully — m_inTransaction returns to false between them — so
    // without this the drain at the end of a write would fire BETWEEN the count write and the
    // payload write, which is the half-written block this whole function is arranged to prevent.
    // Held across all four writes and released, with the queue drained, only once the block is
    // consistent.
    m_suppressPendingDrain = true;
    const auto releaseDrain = qScopeGuard([this]() {
        m_suppressPendingDrain = false;
        drainPendingIoWrites();
    });

    QString error;

    // 1. Clear the count. A master reading 0 knows a publish is in progress.
    {
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,
                             base + ModbusResultLayout::kOffsetCount, 1);
        unit.setValue(0, 0);
        if (!transact(unit, /*write*/ true, nullptr, &error)) {
            noteTransactionFailure(error);
            return fail(error);
        }
    }

    // 2. Write the full payload, in chunks that respect the write ceiling. Full length always:
    //    the zero-filled tail is what stops a stale count from exposing the previous cycle.
    bool truncated = false;
    const QList<quint16> payload = ModbusResultLayout::encodePayload(
        positions, m_config.m_resultMaxPositions, &truncated);
    const int payloadBase = base + ModbusResultLayout::kHeaderRegisters;

    for (int offset = 0; offset < payload.size(); offset += kMaxRegistersPerWrite) {
        const int chunk = qMin(kMaxRegistersPerWrite, int(payload.size()) - offset);
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, payloadBase + offset, chunk);
        for (int i = 0; i < chunk; ++i) {
            unit.setValue(i, payload.at(offset + i));
        }
        if (!transact(unit, /*write*/ true, nullptr, &error)) {
            noteTransactionFailure(error);
            return fail(error);
        }
    }

    // 3. Sequence and flags, then 4. the count, last.
    m_resultSequence = ModbusResultLayout::nextSequence(m_resultSequence);
    quint16 flags = 0;
    if (m_lowArea) {
        flags |= ModbusResultLayout::kFlagLowArea;
    }
    if (truncated) {
        flags |= ModbusResultLayout::kFlagTruncated;
    }

    {
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,
                             base + ModbusResultLayout::kOffsetSequence, 2);
        unit.setValue(0, m_resultSequence);
        unit.setValue(1, flags);
        if (!transact(unit, /*write*/ true, nullptr, &error)) {
            noteTransactionFailure(error);
            return fail(error);
        }
    }

    const int count = qMin(int(positions.size()), m_config.m_resultMaxPositions);
    {
        QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters,
                             base + ModbusResultLayout::kOffsetCount, 1);
        unit.setValue(0, static_cast<quint16>(count));
        if (!transact(unit, /*write*/ true, nullptr, &error)) {
            noteTransactionFailure(error);
            return fail(error);
        }
    }

    noteTransactionSuccess();

    if (truncated) {
        LOG_USER_WARN << "Modbus result truncated: more positions than the block holds."
                      << "deviceId=" << id()
                      << "produced=" << positions.size()
                      << "published=" << count;
    }

    if (message) {
        *message = QStringLiteral("Modbus result published: %1 position(s), sequence %2.")
                       .arg(count)
                       .arg(m_resultSequence);
    }
    return true;
}

} // namespace vc::device
