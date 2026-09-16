/**
 * @file modbus_tcp_server_device.cpp
 * @brief Implementation of the Modbus TCP server PLC device.
 */

#include "device/plc/modbus/modbus_tcp_server_device.h"

#include "core/logger/app_logger.h"
#include "device/plc/modbus/modbus_result_layout.h"
#include "device/plc/modbus/modbus_trace.h"

#include <QHostAddress>
#include <QModbusDataUnitMap>
#include <QModbusTcpConnectionObserver>
#include <QModbusTcpServer>
#include <QStringList>
#include <QTcpSocket>
#include <QThread>
#include <QVariant>

namespace vc::device {

namespace {

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

/// Maps Qt's area enum onto ours.
/// @return false if `table` is not one of the four data areas
bool fromQtRegisterType(QModbusDataUnit::RegisterType table, ModbusArea *area)
{
    switch (table) {
    case QModbusDataUnit::Coils:            *area = ModbusArea::Coils; return true;
    case QModbusDataUnit::DiscreteInputs:   *area = ModbusArea::DiscreteInputs; return true;
    case QModbusDataUnit::HoldingRegisters: *area = ModbusArea::HoldingRegisters; return true;
    case QModbusDataUnit::InputRegisters:   *area = ModbusArea::InputRegisters; return true;
    case QModbusDataUnit::Invalid:          break;
    }
    return false;
}

/**
 * @class LoggingModbusTcpServer
 * @brief QModbusTcpServer that reports every request it handles and every response it produces.
 *
 * This is the only place the *server's own* view of a conversation is visible. Qt answers a master
 * from inside the library: without overriding processRequest() we see the effect of a write (via
 * dataWritten) and nothing at all of a read, or of a request we rejected. An exception response
 * the server generated — the master sees it, we would not — is exactly the case an integrator
 * needs and could not previously get at.
 *
 * @note Successful requests are logged only when the trace flag is on; **exception responses are
 *       always logged**, because that is the moment someone is trying to find out what went wrong.
 */
class LoggingModbusTcpServer : public QModbusTcpServer {
public:
    LoggingModbusTcpServer(QString deviceId, QObject *parent)
        : QModbusTcpServer(parent)
        , m_deviceId(std::move(deviceId)) {}

    /// Pushed by the device rather than read from a pointer into its config.
    ///
    /// It held a `const bool *` into the device's config member at first, so that toggling the
    /// flag took effect without a reconnect. That pointer outlived what it pointed at: the server
    /// is torn down with deleteLater(), so it is still alive — and can still be handed a queued
    /// request — after the device's own members are gone. A copied value cannot dangle.
    void setTraceEnabled(bool enabled) { m_trace = enabled; }

protected:
    QModbusResponse processRequest(const QModbusPdu &request) override
    {
        const QModbusResponse response = QModbusTcpServer::processRequest(request);

        if (response.isException()) {
            // The server's side of the story. The master logs "the peer answered an exception";
            // this says which request produced it, so the two ends can be matched up.
            LOG_USER_ERR << "Modbus server answered an EXCEPTION."
                         << "deviceId=" << m_deviceId
                         << "request=" << modbus_trace::describePdu(request)
                         << "response=" << modbus_trace::describePdu(response)
                         << "hint=" << QStringLiteral(
                                "compare the requested address range against this server's "
                                "register spans, logged at connect");
        } else if (m_trace) {
            LOG_DEV_INFO << "Modbus server request." << "deviceId=" << m_deviceId
                         << "request=" << modbus_trace::describePdu(request)
                         << "response=" << modbus_trace::describePdu(response);
        }

        return response;
    }

private:
    QString m_deviceId;     ///< For the log lines; the server has no id of its own.
    bool m_trace{false};    ///< Copied from the config; see setTraceEnabled().
};

/**
 * @class LoggingConnectionObserver
 * @brief Logs every master that connects, and accepts all of them.
 *
 * Accepting everything is Qt's own default. The value here is the log line: "no master ever
 * connected" and "a master connected and then asked for something we refused" are completely
 * different faults that look identical from the panel.
 */
class LoggingConnectionObserver : public QModbusTcpConnectionObserver {
public:
    explicit LoggingConnectionObserver(QString deviceId) : m_deviceId(std::move(deviceId)) {}

    bool acceptNewConnection(QTcpSocket *newClient) override
    {
        if (newClient) {
            LOG_USER_INFO << "Modbus master connected."
                          << "deviceId=" << m_deviceId
                          << "peer=" << newClient->peerAddress().toString()
                          << "port=" << newClient->peerPort();
            QObject::connect(newClient, &QTcpSocket::disconnected, newClient,
                             [deviceId = m_deviceId, newClient]() {
                LOG_USER_INFO << "Modbus master disconnected."
                              << "deviceId=" << deviceId
                              << "peer=" << newClient->peerAddress().toString();
            });
        }
        return true;
    }

private:
    QString m_deviceId;
};

} // namespace

ModbusTcpServerDevice::ModbusTcpServerDevice(QString id, QString name, QObject *parent)
    : PlcDevice(std::move(id), std::move(name), parent)
{
    // IDevice holds a non-owning config pointer and writes through it without calling
    // setDeviceConfig(); publishing our own member here is what makes a loaded project land
    // somewhere. See src/device/AGENTS.md.
    setDeviceConfig(&m_config);
    rebuildRegisterMap();
}

ModbusTcpServerDevice::~ModbusTcpServerDevice()
{
    delete m_server;
    m_server = nullptr;
}

// ── Configuration ────────────────────────────────────────────────────────────────────────────

void ModbusTcpServerDevice::setDeviceConfig(IDeviceCfg *cfg)
{
    auto *modbusCfg = dynamic_cast<ModbusTcpServerCfg *>(cfg);
    if (!modbusCfg) {
        if (cfg != &m_config) {
            LOG_DEV_ERR << "ModbusTcpServerDevice: config is not a ModbusTcpServerCfg."
                        << "deviceId=" << id();
            return;
        }
    } else if (isDeviceConnected()) {
        // One exception to "settings are ignored while connected": the protocol trace. A
        // debugging switch you have to drop the link to use is a switch you cannot use on the
        // fault you are chasing.
        if (m_config.m_traceProtocol != modbusCfg->m_traceProtocol) {
            m_config.m_traceProtocol = modbusCfg->m_traceProtocol;
            if (auto *logging = dynamic_cast<LoggingModbusTcpServer *>(m_server)) {
                logging->setTraceEnabled(m_config.m_traceProtocol);
            }
            LOG_USER_INFO << "Modbus protocol trace"
                          << (m_config.m_traceProtocol ? "ENABLED" : "disabled")
                          << "deviceId=" << id();
        }
        LOG_USER_WARN << "Modbus server configuration ignored while listening"
                      << "(protocol trace excepted)." << "deviceId=" << id();
        return;
    } else {
        m_config = *modbusCfg;
    }

    PlcDevice::setDeviceConfig(&m_config);
    rebuildRegisterMap();
}

bool ModbusTcpServerDevice::fromJson(const QJsonObject &obj)
{
    const bool ok = IDevice::fromJson(obj);
    rebuildRegisterMap();
    return ok;
}

void ModbusTcpServerDevice::rebuildRegisterMap()
{
    m_config.configureRegisterMap(m_map);

    if (m_config.overlapsPolledResultRange()) {
        // Permitted: unlike the client, a server that overlaps them is merely showing its own
        // published result in the signal map, not wasting a poll. Still worth saying out loud.
        LOG_USER_WARN << "Modbus result block overlaps the mapped span of its own area."
                      << "deviceId=" << id()
                      << "resultArea=" << modbus_tags::prefix(m_config.resultArea())
                      << "resultStart=" << m_config.m_resultStartAddress;
    }
}

ModbusRange ModbusTcpServerDevice::servedRange(ModbusArea area) const
{
    const ModbusRange mapped = m_config.range(area);
    if (area != m_config.resultArea()) {
        return mapped;
    }

    const ModbusRange result = m_config.resultRange();
    if (mapped.count <= 0) {
        return result;
    }
    if (result.count <= 0) {
        return mapped;
    }

    // The server must own every register a master may touch. The result block is configured
    // independently of the mapped span — by default it sits well clear of it — so the served
    // space is the union, not one or the other. Getting this wrong would make the master's read
    // of the result block fail with an illegal-data-address exception.
    const int start = qMin(mapped.start, result.start);
    const int end = qMax(mapped.last(), result.last());
    return { start, end - start + 1 };
}

void ModbusTcpServerDevice::applyRegisterSpace()
{
    if (!m_server) {
        return;
    }

    QModbusDataUnitMap space;
    QStringList served;
    QStringList absent;
    for (const ModbusArea area : kModbusAreas) {
        const ModbusRange range = servedRange(area);
        if (range.count <= 0) {
            // An area with no configured addresses is deliberately absent: a master reading it
            // then gets an illegal-data-address exception, which is a clearer answer than a
            // block of zeros that looks like real data.
            absent << modbus_tags::prefix(area);
            continue;
        }
        space.insert(toQtRegisterType(area),
                     { toQtRegisterType(area), range.start, quint16(range.count) });
        served << modbus_trace::describeRange(area, range.start, range.count);
    }

    m_server->setMap(space);
    m_server->setServerAddress(m_config.m_unitId);

    // The counterpart of the client's poll-plan line, and the first thing to read when a master
    // gets IllegalDataAddress: anything it asks for outside these spans is refused, and any area
    // listed as absent is refused entirely.
    LOG_USER_INFO << "Modbus server register space."
                  << "deviceId=" << id()
                  << "unitId=" << m_config.m_unitId
                  << "served=" << served.join(QStringLiteral("; "))
                  << "absent=" << (absent.isEmpty() ? QStringLiteral("none")
                                                    : absent.join(QStringLiteral(", ")));
}

// ── Listen lifecycle ─────────────────────────────────────────────────────────────────────────

bool ModbusTcpServerDevice::deviceConnect()
{
    if (isDeviceConnected()) {
        setConnectionStatus(ConnectStatus::Connected);
        return true;
    }

    delete m_server;
    auto *loggingServer = new LoggingModbusTcpServer(id(), this);
    loggingServer->setTraceEnabled(m_config.m_traceProtocol);
    m_server = loggingServer;
    m_server->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                     m_config.m_listenAddress);
    m_server->setConnectionParameter(QModbusDevice::NetworkPortParameter, m_config.m_port);
    applyRegisterSpace();

    // Handed over, not held: Qt owns the observer and deletes it with the server.
    m_server->installConnectionObserver(new LoggingConnectionObserver(id()));

    connect(m_server, &QModbusDevice::errorOccurred, this,
            [this](QModbusDevice::Error error) {
        if (error != QModbusDevice::NoError) {
            LOG_USER_WARN << "Modbus server transport error."
                          << "deviceId=" << id()
                          << "error=" << int(error)
                          << "reason=" << (m_server ? m_server->errorString() : QString());
        }
    });
    connect(m_server, &QModbusDevice::stateChanged, this,
            [this](QModbusDevice::State state) {
        LOG_DEV_INFO << "Modbus server transport state." << "deviceId=" << id()
                     << "state=" << int(state);
    });

    connect(m_server, &QModbusServer::dataWritten,
            this, &ModbusTcpServerDevice::onDataWritten);

    setConnectionStatus(ConnectStatus::Connecting);

    if (!m_server->connectDevice()) {
        // This is where a port already in use lands. Failing here, loudly, is the whole point:
        // a server that silently did not bind would look connected and never answer a master.
        const QString reason = m_server->errorString();
        LOG_USER_ERR << "Modbus server could not listen."
                     << "deviceId=" << id()
                     << "address=" << m_config.m_listenAddress
                     << "port=" << m_config.m_port
                     << "reason=" << reason;
        delete m_server;
        m_server = nullptr;
        setConnectionStatus(ConnectStatus::ConnectFailed, reason);
        return false;
    }

    // Adopt, do not reset. The client resets its baseline because the first poll brings a slave
    // state it has never seen; a server has no such moment — it owns the space and the space
    // starts at zero. Resetting here would swallow the first write a master makes after
    // connecting, and for a handshake bit that is precisely the write that matters.
    m_map.adoptCurrentAsBaseline();

    LOG_USER_INFO << "Modbus server listening."
                  << "deviceId=" << id()
                  << "address=" << m_config.m_listenAddress
                  << "port=" << m_config.m_port
                  << "unitId=" << m_config.m_unitId;

    setConnectionStatus(ConnectStatus::Connected);
    return true;
}

bool ModbusTcpServerDevice::deviceDisconnect()
{
    if (m_server) {
        m_server->disconnect(this);
        m_server->disconnectDevice();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_map.resetChangeBaseline();
    setConnectionStatus(ConnectStatus::Disconnected);
    return true;
}

void ModbusTcpServerDevice::deviceTerminate()
{
    if (connectStatus() == ConnectStatus::Connected
        || connectStatus() == ConnectStatus::Connecting) {
        deviceDisconnect();
    }
}

// ── Thread affinity ──────────────────────────────────────────────────────────────────────────

bool ModbusTcpServerDevice::assertOnDeviceThread(const QString &what) const
{
    if (QThread::currentThread() == thread()) {
        return true;
    }
    // QModbusTcpServer owns the listening socket and every accepted client socket, all affine
    // to this device's worker thread. setData() from another thread does not report an error and
    // does not reliably take effect — a master reads a stale value, or nothing changes at all,
    // with nothing in the log to say why. The client sub-type has the matching guard in
    // transact(), where the same mistake cost a mystery response timeout.
    LOG_USER_ERR << "Modbus server operation issued from the wrong thread; it must be queued onto"
                 << "the device thread (use PlcRunner)."
                 << "deviceId=" << id() << "operation=" << what;
    return false;
}

// ── Master writes ────────────────────────────────────────────────────────────────────────────

void ModbusTcpServerDevice::onDataWritten(QModbusDataUnit::RegisterType table,
                                          int address, int size)
{
    ModbusArea area = ModbusArea::Coils;
    if (!m_server || !fromQtRegisterType(table, &area)) {
        return;
    }

    if (m_config.m_traceProtocol) {
        LOG_DEV_INFO << "Modbus server data written."
                     << "deviceId=" << id()
                     << "range=" << modbus_trace::describeRange(area, address, size);
    }

    int mirrored = 0;
    for (int offset = 0; offset < size; ++offset) {
        quint16 raw = 0;
        if (!m_server->data(table, quint16(address + offset), &raw)) {
            continue;
        }
        if (m_map.isConfigured(area, address + offset)) {
            ++mirrored;
        }
        // Addresses outside the configured spans are dropped by the map itself. That is how the
        // result block stays out of the task signal map when it sits beyond the mapped span.
        if (modbus_tags::isBitArea(area)) {
            m_map.setBit(area, address + offset, raw != 0);
        } else {
            m_map.setWord(area, address + offset, raw);
        }
    }

    // The vision-result block is outside the mapped span BY DESIGN — that is how it stays out of
    // the task signal map — and this device writes it itself, four times per localization cycle
    // (sendVisionResult()). Every one of those re-enters here through QModbusServer::dataWritten.
    // Warning about them would put four lines per cycle in front of the operator and teach them
    // to ignore this message, which is the opposite of why it was raised to user level.
    const ModbusRange resultBlock = m_config.resultRange();
    const bool writeIsEntirelyTheResultBlock =
        (area == m_config.resultArea())
        && resultBlock.contains(address)
        && resultBlock.contains(address + size - 1);

    if (mirrored < size && !writeIsEntirelyTheResultBlock) {
        // Raised from LOG_DEV_INFO to user level 2026-09-04, after a master wrote
        // nActiveCamera and nActivePatternGroup in one FC16 block and only the first of the two
        // took effect. The write is ACKed on the wire — the master is told it succeeded — and
        // then silently discarded here, so without this line the only symptom is a task signal
        // that never moves. A write the device accepts and drops is exactly what an operator
        // must be able to see.
        //
        // The usual cause is the shape of the request rather than a wrong address: FC16 writes a
        // CONTIGUOUS block, so a master batching two non-adjacent signals into one request puts
        // the second value on whatever register follows the first.
        LOG_USER_WARN << "Modbus server accepted a write it could not deliver: part of it falls"
                      << "outside the mapped span, so those addresses never reach the task signal"
                      // ASCII only in the message itself: this line is read from a log file and
                      // from consoles whose codepage is not UTF-8 (Shift-JIS here), where a
                      // non-ASCII dash arrives as a replacement character.
                      << "map. A multi-register write covers a CONTIGUOUS block, so check that"
                      << "the master is not batching signals that are not adjacent."
                      << "deviceId=" << id()
                      << "range=" << modbus_trace::describeRange(area, address, size)
                      << "delivered=" << mirrored << "of" << size
                      << "mappedSpan=" << modbus_trace::describeRange(area,
                                                                      m_config.range(area).start,
                                                                      m_config.range(area).count);
    }

    publishChanges();
}

void ModbusTcpServerDevice::publishChanges()
{
    const QMap<QString, QVariant> changed = m_map.takeChangedValues();
    if (!changed.isEmpty()) {
        LOG_DEV_INFO << "Modbus values changed." << "deviceId=" << id()
                     << "count=" << changed.size()
                     << "values=" << modbus_trace::describeValues(changed);
        emit valueChanged(changed);
    }
    emit pollingUpdate(m_map.clone());
}

// ── Tag provider / writer ────────────────────────────────────────────────────────────────────

QStringList ModbusTcpServerDevice::availableDigitalIoNames() const
{
    return m_map.digitalTagNames();
}

QStringList ModbusTcpServerDevice::availableWordIoNames() const
{
    return m_map.wordTagNames();
}

bool ModbusTcpServerDevice::writeDigitalIoByName(const QString &tag, bool value)
{
    ModbusArea area = ModbusArea::Coils;
    int address = 0;
    if (!modbus_tags::parse(tag, &area, &address) || !modbus_tags::isBitArea(area)) {
        LOG_USER_ERR << "Modbus digital write rejected: not a bit tag."
                     << "deviceId=" << id() << "tag=" << tag;
        return false;
    }
    // Both bit areas are ours to write: we own the register space. A discrete input is a
    // **one-way** tag — the task writes it and the master may only read it — where a coil is
    // two-way. That is a difference to understand when mapping a signal, not a reason to refuse
    // the write; refusing it made this device unable to publish into the very area it is the only
    // legitimate writer of.
    if (!modbus_tags::isServerWritable(area)) {
        LOG_USER_ERR << "Modbus digital write rejected: area is not server-writable."
                     << "deviceId=" << id() << "tag=" << tag;
        return false;
    }
    if (!m_map.isConfigured(area, address)) {
        LOG_USER_ERR << "Modbus digital write rejected: address outside the configured span."
                     << "deviceId=" << id() << "tag=" << tag;
        return false;
    }
    if (!m_server || !isDeviceConnected()) {
        LOG_USER_ERR << "Modbus digital write rejected: server is not listening."
                     << "deviceId=" << id() << "tag=" << tag;
        return false;
    }
    if (!assertOnDeviceThread(QStringLiteral("digital write %1").arg(tag))) {
        return false;
    }

    const QModbusDataUnit::RegisterType table = toQtRegisterType(area);
    if (!m_server->setData(table, quint16(address), value ? 1 : 0)) {
        LOG_USER_ERR << "Modbus digital write failed."
                     << "deviceId=" << id() << "tag=" << tag
                     << "reason=" << m_server->errorString();
        return false;
    }
    m_map.setBit(area, address, value);
    return true;
}

bool ModbusTcpServerDevice::writeWordIoByName(const QString &tag, qint16 value)
{
    ModbusArea area = ModbusArea::HoldingRegisters;
    int address = 0;
    if (!modbus_tags::parse(tag, &area, &address) || modbus_tags::isBitArea(area)) {
        LOG_USER_ERR << "Modbus word write rejected: not a register tag."
                     << "deviceId=" << id() << "tag=" << tag;
        return false;
    }
    // Input registers are ours to write for the same reason discrete inputs are: we own the
    // space. The tag is one-way — the task writes it, the master may only read it.
    if (!modbus_tags::isServerWritable(area)) {
        LOG_USER_ERR << "Modbus word write rejected: area is not server-writable."
                     << "deviceId=" << id() << "tag=" << tag;
        return false;
    }
    if (!m_map.isConfigured(area, address)) {
        LOG_USER_ERR << "Modbus word write rejected: address outside the configured span."
                     << "deviceId=" << id() << "tag=" << tag;
        return false;
    }

    QString error;
    if (!writeServedRegisters(area, address, { static_cast<quint16>(value) }, &error)) {
        LOG_USER_ERR << "Modbus word write failed."
                     << "deviceId=" << id() << "tag=" << tag << "reason=" << error;
        return false;
    }
    return true;
}

bool ModbusTcpServerDevice::writeServedRegisters(ModbusArea area, int address,
                                                 const QList<quint16> &values, QString *error)
{
    if (modbus_tags::isBitArea(area)) {
        if (error) {
            *error = QStringLiteral("Not a word area.");
        }
        return false;
    }
    if (!m_server || !isDeviceConnected()) {
        if (error) {
            *error = QStringLiteral("Modbus server is not listening.");
        }
        return false;
    }
    if (!assertOnDeviceThread(modbus_trace::describeRange(area, address, values.size()))) {
        if (error) {
            *error = QStringLiteral("Modbus server write issued from the wrong thread.");
        }
        return false;
    }

    const QModbusDataUnit::RegisterType table = toQtRegisterType(area);
    for (int i = 0; i < values.size(); ++i) {
        if (!m_server->setData(table, quint16(address + i), values.at(i))) {
            if (error) {
                *error = m_server->errorString();
            }
            return false;
        }
        // Mirrored into the map so the monitor and the signal path see the same values a master
        // would read. Addresses outside the configured span are dropped there, by design.
        m_map.setWord(area, address + i, values.at(i));
    }
    return true;
}

bool ModbusTcpServerDevice::pushRequest(IRequest *request)
{
    Q_UNUSED(request);
    LOG_DEV_ERR << "ModbusTcpServerDevice::pushRequest is not used; results go through"
                << "IResultOutputDevice and IO through IPlcIoWriter." << "deviceId=" << id();
    return false;
}

// ── Result publication ───────────────────────────────────────────────────────────────────────

bool ModbusTcpServerDevice::sendVisionResult(const QVector<VisionOutputPosition> &positions,
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

    if (!m_server || !isDeviceConnected()) {
        return fail(QStringLiteral("Modbus server is not listening."));
    }
    if (m_config.m_resultMaxPositions <= 0) {
        return fail(QStringLiteral("Result block capacity is zero; nothing can be published."));
    }

    const int base = m_config.m_resultStartAddress;
    // Holding registers by default; input registers when the operator chose the layout a master
    // cannot overwrite. The offsets, the encoding and the handshake are identical either way —
    // only the reference class the PLC program reads from differs (4x versus 3x).
    const ModbusArea area = m_config.resultArea();
    QString error;

    // The same count-last handshake the client device uses. Writing our own memory is not a
    // network transaction, so nothing can tear between the steps here — but a master polling
    // us can still land between them, which is exactly the case the handshake is for. Keeping
    // the identical order also means one PLC program serves both sub-types.

    // 1. Clear the count.
    if (!writeServedRegisters(area, base + ModbusResultLayout::kOffsetCount, { 0 }, &error)) {
        return fail(error);
    }

    // 2. Full payload, zero-filled tail included.
    bool truncated = false;
    const QList<quint16> payload = ModbusResultLayout::encodePayload(
        positions, m_config.m_resultMaxPositions, &truncated);
    if (!writeServedRegisters(area, base + ModbusResultLayout::kHeaderRegisters,
                              payload, &error)) {
        return fail(error);
    }

    // 3. Sequence and flags.
    m_resultSequence = ModbusResultLayout::nextSequence(m_resultSequence);
    quint16 flags = 0;
    if (m_lowArea) {
        flags |= ModbusResultLayout::kFlagLowArea;
    }
    if (truncated) {
        flags |= ModbusResultLayout::kFlagTruncated;
    }
    if (!writeServedRegisters(area, base + ModbusResultLayout::kOffsetSequence,
                              { m_resultSequence, flags }, &error)) {
        return fail(error);
    }

    // 4. The count, last.
    const int count = qMin(int(positions.size()), m_config.m_resultMaxPositions);
    if (!writeServedRegisters(area, base + ModbusResultLayout::kOffsetCount,
                              { static_cast<quint16>(count) }, &error)) {
        return fail(error);
    }

    if (truncated) {
        LOG_USER_WARN << "Modbus result truncated: more positions than the block holds."
                      << "deviceId=" << id()
                      << "produced=" << positions.size()
                      << "published=" << count;
    }

    publishChanges();

    if (message) {
        *message = QStringLiteral("Modbus result published: %1 position(s), sequence %2.")
                       .arg(count)
                       .arg(m_resultSequence);
    }
    return true;
}

} // namespace vc::device
