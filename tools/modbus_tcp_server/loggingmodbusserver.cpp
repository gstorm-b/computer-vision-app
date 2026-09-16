#include "loggingmodbusserver.h"

#include <QModbusPdu>

LoggingModbusServer::LoggingModbusServer(QObject *parent)
    : QModbusTcpServer(parent)
{
}

QModbusResponse LoggingModbusServer::processRequest(const QModbusPdu &request)
{
    const int functionCode = static_cast<int>(request.functionCode());
    const QByteArray data = request.data();

    emit requestStarted(functionCode, data);

    // Qt does the real protocol work, including writing the data store, which
    // in turn makes QModbusServer::dataWritten() fire.
    const QModbusResponse response = QModbusTcpServer::processRequest(request);

    if (response.isException()) {
        emit requestRejected(functionCode, data,
                             static_cast<int>(response.exceptionCode()));
    }
    return response;
}
