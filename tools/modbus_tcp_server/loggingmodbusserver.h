#ifndef LOGGINGMODBUSSERVER_H
#define LOGGINGMODBUSSERVER_H

//
// QModbusTcpServer subclass that reports every request so the HMI can log what
// a client asked for. It changes no protocol behaviour: the request is still
// handled by the stock Qt implementation.
//
// requestStarted() is emitted *before* Qt processes the PDU so that the log
// reads in the natural order (request first, resulting data change second).
// requestRejected() follows only when Qt answered with an exception.
//
// The raw PDU payload is passed on untouched because the operand layout
// differs per function code (FC22 carries two masks, FC23 two address/quantity
// pairs, FC08 a sub-function, FC07/FC11/FC17 nothing at all). Decoding it is
// the receiver's job.
//
// The object lives in the worker thread, therefore both signals are emitted
// from that thread and reach the GUI through queued connections.
//

#include <QModbusTcpServer>

class LoggingModbusServer : public QModbusTcpServer
{
    Q_OBJECT

public:
    explicit LoggingModbusServer(QObject *parent = nullptr);

Q_SIGNALS:
    //! functionCode is a QModbusPdu::FunctionCode, data is the PDU payload
    //! without the function code byte.
    void requestStarted(int functionCode, const QByteArray &data);
    void requestRejected(int functionCode, const QByteArray &data, int exceptionCode);

protected:
    QModbusResponse processRequest(const QModbusPdu &request) override;
};

#endif // LOGGINGMODBUSSERVER_H
