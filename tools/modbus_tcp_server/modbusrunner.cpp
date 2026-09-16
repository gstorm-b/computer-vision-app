#include "modbusrunner.h"
#include "loggingmodbusserver.h"

#include <QHostAddress>
#include <QModbusPdu>
#include <QModbusTcpServer>
#include <QRandomGenerator>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

using namespace ModbusDemo;

namespace {

QModbusDataUnit::RegisterType registerTypeFor(int area)
{
    switch (area) {
    case Coils:            return QModbusDataUnit::Coils;
    case DiscreteInputs:   return QModbusDataUnit::DiscreteInputs;
    case InputRegisters:   return QModbusDataUnit::InputRegisters;
    case HoldingRegisters: return QModbusDataUnit::HoldingRegisters;
    default:               return QModbusDataUnit::Invalid;
    }
}

int areaFor(QModbusDataUnit::RegisterType type)
{
    switch (type) {
    case QModbusDataUnit::Coils:            return Coils;
    case QModbusDataUnit::DiscreteInputs:   return DiscreteInputs;
    case QModbusDataUnit::InputRegisters:   return InputRegisters;
    case QModbusDataUnit::HoldingRegisters: return HoldingRegisters;
    default:                                return -1;
    }
}

QString functionCodeName(int code)
{
    switch (code) {
    case QModbusPdu::ReadCoils:                  return QStringLiteral("FC01 ReadCoils");
    case QModbusPdu::ReadDiscreteInputs:         return QStringLiteral("FC02 ReadDiscreteInputs");
    case QModbusPdu::ReadHoldingRegisters:       return QStringLiteral("FC03 ReadHoldingRegisters");
    case QModbusPdu::ReadInputRegisters:         return QStringLiteral("FC04 ReadInputRegisters");
    case QModbusPdu::WriteSingleCoil:            return QStringLiteral("FC05 WriteSingleCoil");
    case QModbusPdu::WriteSingleRegister:        return QStringLiteral("FC06 WriteSingleRegister");
    case QModbusPdu::ReadExceptionStatus:        return QStringLiteral("FC07 ReadExceptionStatus");
    case QModbusPdu::Diagnostics:                return QStringLiteral("FC08 Diagnostics");
    case QModbusPdu::WriteMultipleCoils:         return QStringLiteral("FC15 WriteMultipleCoils");
    case QModbusPdu::WriteMultipleRegisters:     return QStringLiteral("FC16 WriteMultipleRegisters");
    case QModbusPdu::ReportServerId:             return QStringLiteral("FC17 ReportServerId");
    case QModbusPdu::MaskWriteRegister:          return QStringLiteral("FC22 MaskWriteRegister");
    case QModbusPdu::ReadWriteMultipleRegisters: return QStringLiteral("FC23 ReadWriteMultipleRegisters");
    default:
        return QStringLiteral("FC%1").arg(code, 2, 10, QLatin1Char('0'));
    }
}

QString exceptionName(int code)
{
    switch (code) {
    case QModbusPdu::IllegalFunction:     return QStringLiteral("IllegalFunction");
    case QModbusPdu::IllegalDataAddress:  return QStringLiteral("IllegalDataAddress");
    case QModbusPdu::IllegalDataValue:    return QStringLiteral("IllegalDataValue");
    case QModbusPdu::ServerDeviceFailure: return QStringLiteral("ServerDeviceFailure");
    case QModbusPdu::Acknowledge:         return QStringLiteral("Acknowledge");
    case QModbusPdu::ServerDeviceBusy:    return QStringLiteral("ServerDeviceBusy");
    case QModbusPdu::NegativeAcknowledge: return QStringLiteral("NegativeAcknowledge");
    case QModbusPdu::MemoryParityError:   return QStringLiteral("MemoryParityError");
    default:                              return QStringLiteral("Exception");
    }
}

//! index-th 16 bit word of a PDU payload, 0 when the payload is too short.
quint16 pduWord(const QByteArray &data, int index)
{
    const qsizetype offset = qsizetype(index) * 2;
    if (offset + 1 >= data.size())
        return 0;
    return quint16((quint16(quint8(data.at(offset))) << 8) | quint8(data.at(offset + 1)));
}

bool pduHasWords(const QByteArray &data, int count)
{
    return data.size() >= qsizetype(count) * 2;
}

QString hex16(quint16 value)
{
    return QStringLiteral("0x")
           + QString::number(value, 16).toUpper().rightJustified(4, QLatin1Char('0'));
}

//! Restores m_localWrite even if the guarded call leaves early.
class WriteGuard
{
public:
    explicit WriteGuard(bool *flag) : m_flag(flag) { *m_flag = true; }
    ~WriteGuard() { *m_flag = false; }

    WriteGuard(const WriteGuard &) = delete;
    WriteGuard &operator=(const WriteGuard &) = delete;

private:
    bool *m_flag;
};

//! What a function code does, so the log and the counters agree.
struct RequestKind
{
    bool isRead = false;
    bool isWrite = false;
};

RequestKind classifyFunctionCode(int functionCode)
{
    RequestKind kind;
    switch (functionCode) {
    case QModbusPdu::ReadCoils:
    case QModbusPdu::ReadDiscreteInputs:
    case QModbusPdu::ReadHoldingRegisters:
    case QModbusPdu::ReadInputRegisters:
        kind.isRead = true;
        break;
    case QModbusPdu::WriteSingleCoil:
    case QModbusPdu::WriteSingleRegister:
    case QModbusPdu::WriteMultipleCoils:
    case QModbusPdu::WriteMultipleRegisters:
    case QModbusPdu::MaskWriteRegister:
        kind.isWrite = true;
        break;
    case QModbusPdu::ReadWriteMultipleRegisters:
        kind.isRead = true;
        kind.isWrite = true;
        break;
    default:
        break;
    }
    return kind;
}

//! Keeps long client writes readable in the log.
QString describeValues(int area, const QList<quint16> &values)
{
    const int shown = int(qMin<qsizetype>(values.size(), 8));
    QStringList parts;
    parts.reserve(shown);
    for (int i = 0; i < shown; ++i) {
        parts << (isBitArea(area)
                      ? QString::number(values.at(i))
                      : QStringLiteral("0x%1").arg(QString::number(values.at(i), 16)
                                                       .toUpper()
                                                       .rightJustified(4, QLatin1Char('0'))));
    }
    QString text = parts.join(QLatin1String(", "));
    if (values.size() > shown)
        text += QStringLiteral(", ... (%1 values)").arg(values.size());
    return text;
}

//! Bridges QModbusTcpServer connection notifications back into the runner.
class ClientConnectionObserver : public QModbusTcpConnectionObserver
{
public:
    explicit ClientConnectionObserver(ModbusRunner *runner)
        : m_runner(runner) {}

    bool acceptNewConnection(QTcpSocket *newClient) override
    {
        return m_runner && m_runner->handleNewConnection(newClient);
    }

private:
    ModbusRunner *m_runner = nullptr;
};

} // namespace

ModbusRunner::ModbusRunner(QObject *parent)
    : QObject(parent)
{
    for (int area = 0; area < AreaCount; ++area)
        m_shadow[area] = QList<quint16>(kAddressCount, 0);
}

ModbusRunner::~ModbusRunner()
{
    if (m_server) {
        m_server->disconnect(this);
        m_server->installConnectionObserver(nullptr);
        m_server->disconnectDevice();
        delete m_server;
        m_server = nullptr;
    }
}

void ModbusRunner::initialise()
{
    // Runs in the worker thread, so the timer belongs to that thread too.
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(250);          // at most 4 counter updates per second
    connect(m_statsTimer, &QTimer::timeout, this, &ModbusRunner::flushStats);
    m_statsTimer->start();

    log(LogInfo, tr("Worker thread ready - 4 areas x %1 addresses (%2..%3).")
                     .arg(kAddressCount)
                     .arg(kStartAddress)
                     .arg(kStartAddress + kAddressCount - 1));
    emit serverStateChanged(int(QModbusDevice::UnconnectedState), tr("Stopped"), false);
    requestSnapshot();
}

// ---------------------------------------------------------------- server ---

void ModbusRunner::startServer(const QString &address, int port, int unitId)
{
    if (m_server) {
        if (m_server->state() != QModbusDevice::UnconnectedState) {
            log(LogWarning, tr("Server is already running."));
            return;
        }
        destroyServer();   // stale instance left over from a failed start
    }

    // Qt builds the listening URL from this string. A host name or an IPv6
    // literal fails deep inside QTcpServer::listen() with a useless
    // "Unknown error", so reject anything that is not an IPv4 literal here.
    QHostAddress parsed;
    if (!parsed.setAddress(address) || parsed.protocol() != QAbstractSocket::IPv4Protocol) {
        log(LogError, tr("\"%1\" is not an IPv4 address. Use 0.0.0.0 for every "
                         "interface or a literal address such as 192.168.1.10.")
                          .arg(address));
        emit serverStateChanged(int(QModbusDevice::UnconnectedState), tr("Stopped"), false);
        return;
    }

    m_server = new LoggingModbusServer(this);

    // Seed the four areas with the values currently held by the HMI, so the
    // data survives a stop/start cycle.
    QModbusDataUnitMap map;
    for (int area = 0; area < AreaCount; ++area) {
        const QModbusDataUnit::RegisterType type = registerTypeFor(area);
        map.insert(type, QModbusDataUnit(type, kStartAddress, m_shadow[area]));
    }

    if (!m_server->setMap(map)) {
        log(LogError, tr("setMap() failed: %1").arg(m_server->errorString()));
        destroyServer();
        emit serverStateChanged(int(QModbusDevice::UnconnectedState), tr("Stopped"), false);
        return;
    }

    m_server->setServerAddress(unitId);
    m_server->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    m_server->setConnectionParameter(QModbusDevice::NetworkAddressParameter, address);

    // The server takes ownership of the observer.
    m_server->installConnectionObserver(new ClientConnectionObserver(this));

    connect(m_server, &QModbusServer::dataWritten,
            this, &ModbusRunner::onDataWritten);
    connect(m_server, &LoggingModbusServer::requestStarted,
            this, &ModbusRunner::onRequestStarted);
    connect(m_server, &LoggingModbusServer::requestRejected,
            this, &ModbusRunner::onRequestRejected);
    connect(m_server, &QModbusTcpServer::modbusClientDisconnected,
            this, &ModbusRunner::onClientDisconnected);
    connect(m_server, &QModbusDevice::stateChanged,
            this, &ModbusRunner::onDeviceStateChanged);
    connect(m_server, &QModbusDevice::errorOccurred,
            this, &ModbusRunner::onDeviceError);

    log(LogInfo, tr("Starting Modbus TCP server on %1:%2 (unit id %3) ...")
                     .arg(address)
                     .arg(port)
                     .arg(unitId));

    if (!m_server->connectDevice()) {
        log(LogError, tr("Cannot start server: %1").arg(m_server->errorString()));
        destroyServer();
        emit serverStateChanged(int(QModbusDevice::UnconnectedState), tr("Stopped"), false);
        return;
    }

    log(LogInfo, tr("Server is listening, waiting for clients ..."));
    log(LogWarning, tr("Clients must address unit id %1 - Qt drops frames carrying "
                       "any other unit id without answering.").arg(unitId));
}

void ModbusRunner::stopServer()
{
    if (!m_server) {
        emit serverStateChanged(int(QModbusDevice::UnconnectedState), tr("Stopped"), false);
        return;
    }

    log(LogInfo, tr("Stopping server ..."));
    m_server->disconnectDevice();
    destroyServer();

    emit statsChanged(m_stats);
    emit serverStateChanged(int(QModbusDevice::UnconnectedState), tr("Stopped"), false);
    log(LogInfo, tr("Server stopped."));
}

void ModbusRunner::destroyServer()
{
    if (!m_server)
        return;

    m_server->disconnect(this);                    // no more callbacks while tearing down
    m_server->installConnectionObserver(nullptr);  // deletes the observer
    m_server->deleteLater();
    m_server = nullptr;

    m_clients.clear();
    m_stats.connectedClients = 0;
}

void ModbusRunner::setAcceptingConnections(bool accept)
{
    if (m_accepting == accept)
        return;
    m_accepting = accept;
    log(LogInfo, accept ? tr("New client connections are now accepted.")
                        : tr("New client connections are now rejected."));
}

// --------------------------------------------------------------- clients ---

bool ModbusRunner::handleNewConnection(QTcpSocket *client)
{
    const QString peer = client
            ? QStringLiteral("%1:%2").arg(client->peerAddress().toString())
                                     .arg(client->peerPort())
            : tr("<unknown>");

    if (!m_accepting) {
        log(LogWarning, tr("Connection from %1 rejected (new clients blocked).").arg(peer));
        return false;
    }

    m_clients.insert(client, peer);
    m_stats.connectedClients = int(m_clients.size());
    log(LogInfo, tr("Client connected: %1 (%2 connected).")
                     .arg(peer).arg(m_clients.size()));
    markStatsDirty();
    return true;
}

void ModbusRunner::onClientDisconnected(QTcpSocket *client)
{
    // Never dereference the socket here - it is already shutting down.
    const QString peer = m_clients.take(client);
    m_stats.connectedClients = int(m_clients.size());
    log(LogInfo, tr("Client disconnected: %1 (%2 still connected).")
                     .arg(peer.isEmpty() ? tr("<unknown>") : peer)
                     .arg(m_clients.size()));
    markStatsDirty();
}

// ------------------------------------------------------------ HMI writes ---

void ModbusRunner::writeValue(int area, int address, int value)
{
    applyLocal(area, address, QList<quint16>() << quint16(qBound(0, value, 0xFFFF)));
}

void ModbusRunner::fillArea(int area, int value)
{
    if (area < 0 || area >= AreaCount)
        return;
    const quint16 v = quint16(qBound(0, value, 0xFFFF));
    applyLocal(area, kStartAddress, QList<quint16>(kAddressCount, v));
    log(LogInfo, tr("HMI filled %1 with %2.").arg(areaName(area)).arg(v));
}

void ModbusRunner::writeBlock(const ModbusBlock &block)
{
    applyLocal(block.area, block.startAddress, block.values);
    log(LogInfo, tr("HMI wrote %1[%2%3] = %4")
                     .arg(areaName(block.area))
                     .arg(block.startAddress)
                     .arg(block.values.size() > 1
                              ? QStringLiteral("..%1").arg(block.startAddress + int(block.values.size()) - 1)
                              : QString())
                     .arg(describeValues(block.area, block.values)));
}

void ModbusRunner::randomizeArea(int area)
{
    if (area < 0 || area >= AreaCount)
        return;

    QList<quint16> values(kAddressCount, 0);
    QRandomGenerator *rng = QRandomGenerator::global();
    const quint32 bound = isBitArea(area) ? 2u : 0x10000u;
    for (int i = 0; i < kAddressCount; ++i)
        values[i] = quint16(rng->bounded(bound));

    applyLocal(area, kStartAddress, values);
    log(LogInfo, tr("HMI randomised %1.").arg(areaName(area)));
}

void ModbusRunner::applyLocal(int area, int startAddress, QList<quint16> values)
{
    if (area < 0 || area >= AreaCount)
        return;
    if (startAddress < kStartAddress || startAddress >= kStartAddress + kAddressCount)
        return;
    if (values.isEmpty())
        return;

    const int offset = startAddress - kStartAddress;
    if (offset + values.size() > kAddressCount)
        values = values.mid(0, kAddressCount - offset);

    if (isBitArea(area)) {
        for (qsizetype i = 0; i < values.size(); ++i)
            values[i] = values.at(i) ? 1 : 0;
    }

    for (qsizetype i = 0; i < values.size(); ++i)
        m_shadow[area][offset + i] = values.at(i);

    if (m_server) {
        const QModbusDataUnit unit(registerTypeFor(area), startAddress, values);
        bool ok = false;
        {
            const WriteGuard guard(&m_localWrite);   // suppress the dataWritten() echo
            ok = m_server->setData(unit);
        }
        if (!ok) {
            log(LogError, tr("Writing %1 failed: %2")
                              .arg(areaName(area), m_server->errorString()));
        }
    }

    ModbusBlock block;
    block.area = area;
    block.startAddress = startAddress;
    block.values = values;
    block.fromClient = false;
    emit blockChanged(block);
}

void ModbusRunner::requestSnapshot()
{
    for (int area = 0; area < AreaCount; ++area) {
        ModbusBlock block;
        block.area = area;
        block.startAddress = kStartAddress;
        block.values = m_shadow[area];
        block.fromClient = false;
        emit blockChanged(block);
    }
    emit statsChanged(m_stats);
}

// --------------------------------------------------------- server events ---

void ModbusRunner::onDataWritten(QModbusDataUnit::RegisterType table, int address, int size)
{
    if (m_localWrite)      // our own write - the HMI already shows these values
        return;
    if (!m_server || size <= 0)
        return;

    const int area = areaFor(table);
    if (area < 0)
        return;

    QModbusDataUnit unit(table, address, quint16(size));
    if (!m_server->data(&unit)) {
        log(LogWarning, tr("Could not read back %1[%2..%3] after a client write.")
                            .arg(areaName(area)).arg(address).arg(address + size - 1));
        return;
    }

    // FC05 stores the raw 0xFF00 coil constant, so normalise bit areas to 0/1
    // before the value reaches the HMI or the shadow copy.
    QList<quint16> values = unit.values();
    if (isBitArea(area)) {
        for (qsizetype i = 0; i < values.size(); ++i)
            values[i] = values.at(i) ? 1 : 0;
    }

    const int offset = address - kStartAddress;
    for (qsizetype i = 0; i < values.size(); ++i) {
        const qsizetype index = offset + i;
        if (index >= 0 && index < kAddressCount)
            m_shadow[area][index] = values.at(i);
    }

    ModbusBlock block;
    block.area = area;
    block.startAddress = address;
    block.values = values;
    block.fromClient = true;
    emit blockChanged(block);

    log(LogInfo, tr("Client wrote %1[%2%3] = %4")
                     .arg(areaName(area))
                     .arg(address)
                     .arg(size > 1 ? QStringLiteral("..%1").arg(address + size - 1) : QString())
                     .arg(describeValues(area, values)));
}

QString ModbusRunner::describeRequest(int functionCode, const QByteArray &data) const
{
    switch (functionCode) {
    case QModbusPdu::ReadCoils:
    case QModbusPdu::ReadDiscreteInputs:
    case QModbusPdu::ReadHoldingRegisters:
    case QModbusPdu::ReadInputRegisters:
    case QModbusPdu::WriteMultipleCoils:
    case QModbusPdu::WriteMultipleRegisters:
        if (pduHasWords(data, 2))
            return tr("address %1, quantity %2").arg(pduWord(data, 0)).arg(pduWord(data, 1));
        break;

    case QModbusPdu::WriteSingleCoil:
    case QModbusPdu::WriteSingleRegister:
        if (pduHasWords(data, 2))
            return tr("address %1, value %2").arg(pduWord(data, 0)).arg(hex16(pduWord(data, 1)));
        break;

    case QModbusPdu::MaskWriteRegister:
        if (pduHasWords(data, 3)) {
            return tr("address %1, AND mask %2, OR mask %3")
                .arg(QString::number(pduWord(data, 0)),
                     hex16(pduWord(data, 1)),
                     hex16(pduWord(data, 2)));
        }
        break;

    case QModbusPdu::ReadWriteMultipleRegisters:
        if (pduHasWords(data, 4)) {
            return tr("read address %1 quantity %2, write address %3 quantity %4")
                .arg(pduWord(data, 0))
                .arg(pduWord(data, 1))
                .arg(pduWord(data, 2))
                .arg(pduWord(data, 3));
        }
        break;

    case QModbusPdu::Diagnostics:
        if (pduHasWords(data, 2)) {
            return tr("sub-function %1, data %2")
                .arg(hex16(pduWord(data, 0)), hex16(pduWord(data, 1)));
        }
        break;

    case QModbusPdu::ReadExceptionStatus:
    case QModbusPdu::GetCommEventCounter:
    case QModbusPdu::GetCommEventLog:
    case QModbusPdu::ReportServerId:
        return tr("no operands");

    default:
        break;
    }

    if (data.isEmpty())
        return tr("no operands");
    return tr("%1 payload bytes").arg(data.size());
}

void ModbusRunner::onRequestStarted(int functionCode, const QByteArray &data)
{
    const RequestKind kind = classifyFunctionCode(functionCode);

    // FC23 reads and writes, so the counters are independent, not exclusive.
    if (kind.isRead)
        ++m_stats.readRequests;
    if (kind.isWrite)
        ++m_stats.writeRequests;
    if (!kind.isRead && !kind.isWrite)
        ++m_stats.otherRequests;
    markStatsDirty();

    // Plain reads go to Debug level: a polling client would otherwise drown
    // every other message. Everything else deserves an Info line.
    const bool onlyRead = kind.isRead && !kind.isWrite;
    log(onlyRead ? LogDebug : LogInfo,
        tr("%1 - %2").arg(functionCodeName(functionCode),
                          describeRequest(functionCode, data)));
}

void ModbusRunner::onRequestRejected(int functionCode, const QByteArray &data, int exceptionCode)
{
    ++m_stats.exceptions;
    markStatsDirty();

    log(LogWarning,
        tr("%1 refused - %2 -> exception 0x%3 (%4)")
            .arg(functionCodeName(functionCode),
                 describeRequest(functionCode, data),
                 QString::number(exceptionCode, 16).rightJustified(2, QLatin1Char('0')),
                 exceptionName(exceptionCode)));
}

void ModbusRunner::onDeviceStateChanged(QModbusDevice::State state)
{
    QString text;
    switch (state) {
    case QModbusDevice::UnconnectedState: text = tr("Stopped");     break;
    case QModbusDevice::ConnectingState:  text = tr("Starting..."); break;
    case QModbusDevice::ConnectedState:   text = tr("Listening");   break;
    case QModbusDevice::ClosingState:     text = tr("Stopping..."); break;
    }

    log(LogDebug, tr("Server state: %1").arg(text));
    emit serverStateChanged(int(state), text, state == QModbusDevice::ConnectedState);
}

void ModbusRunner::onDeviceError(QModbusDevice::Error error)
{
    if (error == QModbusDevice::NoError)
        return;
    log(LogError, tr("Modbus error (%1): %2")
                      .arg(int(error))
                      .arg(m_server ? m_server->errorString() : tr("unknown")));
}

// ------------------------------------------------------------ statistics ---

void ModbusRunner::markStatsDirty()
{
    m_statsDirty = true;
}

void ModbusRunner::setLogLevel(int level)
{
    m_logLevel = qBound(int(LogDebug), level, int(LogError));
}

//! Single funnel for every log line. Dropping filtered messages here rather
//! than in the HMI keeps the GUI event queue from growing without bound when a
//! client polls at several thousand requests per second with Debug selected.
void ModbusRunner::log(int level, const QString &message)
{
    if (level < m_logLevel)
        return;
    emit logMessage(level, message);
}

void ModbusRunner::flushStats()
{
    if (!m_statsDirty)
        return;
    m_statsDirty = false;
    emit statsChanged(m_stats);
}
