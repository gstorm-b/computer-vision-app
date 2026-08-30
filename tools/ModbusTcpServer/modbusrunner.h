#ifndef MODBUSRUNNER_H
#define MODBUSRUNNER_H

//
// ModbusRunner owns the QModbusTcpServer instance and every piece of Modbus
// state. It is created in the GUI thread but immediately moved into a worker
// QThread, so all protocol work (accepting sockets, decoding PDUs, reading and
// writing the register maps) happens off the GUI thread and can never block
// the HMI.
//
// Communication with MainWindow is exclusively through queued signals/slots:
//   GUI  -> runner : startServer/stopServer/writeValue/fillArea/...
//   runner -> GUI  : logMessage/serverStateChanged/blockChanged/statsChanged
//
// The runner also keeps a "shadow" copy of all four areas. It is the master
// copy of the data: it survives a server restart and lets the HMI edit values
// while the server is stopped.
//

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <QModbusDataUnit>
#include <QModbusDevice>

#include "modbusdefs.h"

class LoggingModbusServer;
class QTcpSocket;
class QTimer;

class ModbusRunner : public QObject
{
    Q_OBJECT

public:
    explicit ModbusRunner(QObject *parent = nullptr);
    ~ModbusRunner() override;

    //! Called by the connection observer (worker thread). Internal use.
    bool handleNewConnection(QTcpSocket *client);

public Q_SLOTS:
    //! Runs once when the worker thread has started.
    void initialise();

    void startServer(const QString &address, int port, int unitId);
    void stopServer();

    //! HMI edits. Values are clamped, bit areas are normalised to 0/1.
    void writeValue(int area, int address, int value);
    void fillArea(int area, int value);
    void randomizeArea(int area);

    //! Writes a whole run of registers at once, used by the typed data monitor
    //! when a FLOAT32 / INT32 / ASCII value spans several addresses.
    void writeBlock(const ModbusBlock &block);

    //! Push the whole data set to the HMI (used at start-up).
    void requestSnapshot();

    //! When false, new TCP connections are rejected by the observer.
    void setAcceptingConnections(bool accept);

    //! Messages below this ModbusDemo::LogLevel are dropped here, in the worker
    //! thread, instead of flooding the GUI event queue with read notifications.
    void setLogLevel(int level);

Q_SIGNALS:
    void logMessage(int level, const QString &message);
    void serverStateChanged(int state, const QString &text, bool running);
    void blockChanged(const ModbusBlock &block);
    void statsChanged(const ModbusStats &stats);

private Q_SLOTS:
    void onDataWritten(QModbusDataUnit::RegisterType table, int address, int size);
    void onRequestStarted(int functionCode, const QByteArray &data);
    void onRequestRejected(int functionCode, const QByteArray &data, int exceptionCode);
    void onDeviceStateChanged(QModbusDevice::State state);
    void onDeviceError(QModbusDevice::Error error);
    void onClientDisconnected(QTcpSocket *client);
    void flushStats();

private:
    void applyLocal(int area, int startAddress, QList<quint16> values);
    void destroyServer();
    void markStatsDirty();
    void log(int level, const QString &message);

    //! Operand text for a request, decoded according to its function code.
    QString describeRequest(int functionCode, const QByteArray &data) const;

    LoggingModbusServer *m_server = nullptr;
    QTimer *m_statsTimer = nullptr;

    QList<quint16> m_shadow[ModbusDemo::AreaCount];
    QHash<QTcpSocket *, QString> m_clients;

    ModbusStats m_stats;
    int m_logLevel = ModbusDemo::LogInfo;
    bool m_localWrite = false;   //!< guards dataWritten() against our own writes
    bool m_accepting = true;
    bool m_statsDirty = false;
};

#endif // MODBUSRUNNER_H
