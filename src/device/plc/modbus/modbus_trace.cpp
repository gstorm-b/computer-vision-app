/**
 * @file modbus_trace.cpp
 * @brief Implementation of the Modbus log renderings.
 */

#include "device/plc/modbus/modbus_trace.h"

#include <QStringList>

namespace vc::device::modbus_trace {

namespace {

/// Formats "<name> (0x<hex>)" once, so every renderer here reads the same in the log.
QString named(const QString &name, int code)
{
    return QStringLiteral("%1 (0x%2)")
        .arg(name, QString::number(code, 16).rightJustified(2, QLatin1Char('0')).toUpper());
}

/// Maps our area enum onto Qt's, for describeRange().
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

/// Tag prefix for a Qt register type, so a logged reply names the area the same way a tag does.
QString prefixFor(QModbusDataUnit::RegisterType type)
{
    switch (type) {
    case QModbusDataUnit::Coils:            return modbus_tags::prefix(ModbusArea::Coils);
    case QModbusDataUnit::DiscreteInputs:   return modbus_tags::prefix(ModbusArea::DiscreteInputs);
    case QModbusDataUnit::HoldingRegisters: return modbus_tags::prefix(ModbusArea::HoldingRegisters);
    case QModbusDataUnit::InputRegisters:   return modbus_tags::prefix(ModbusArea::InputRegisters);
    case QModbusDataUnit::Invalid:          break;
    }
    return QStringLiteral("INVALID");
}

} // namespace

QString functionName(QModbusPdu::FunctionCode code)
{
    switch (code) {
    case QModbusPdu::ReadCoils:                    return named(QStringLiteral("ReadCoils"), code);
    case QModbusPdu::ReadDiscreteInputs:           return named(QStringLiteral("ReadDiscreteInputs"), code);
    case QModbusPdu::ReadHoldingRegisters:         return named(QStringLiteral("ReadHoldingRegisters"), code);
    case QModbusPdu::ReadInputRegisters:           return named(QStringLiteral("ReadInputRegisters"), code);
    case QModbusPdu::WriteSingleCoil:              return named(QStringLiteral("WriteSingleCoil"), code);
    case QModbusPdu::WriteSingleRegister:          return named(QStringLiteral("WriteSingleRegister"), code);
    case QModbusPdu::ReadExceptionStatus:          return named(QStringLiteral("ReadExceptionStatus"), code);
    case QModbusPdu::Diagnostics:                  return named(QStringLiteral("Diagnostics"), code);
    case QModbusPdu::GetCommEventCounter:          return named(QStringLiteral("GetCommEventCounter"), code);
    case QModbusPdu::GetCommEventLog:              return named(QStringLiteral("GetCommEventLog"), code);
    case QModbusPdu::WriteMultipleCoils:           return named(QStringLiteral("WriteMultipleCoils"), code);
    case QModbusPdu::WriteMultipleRegisters:       return named(QStringLiteral("WriteMultipleRegisters"), code);
    case QModbusPdu::ReportServerId:               return named(QStringLiteral("ReportServerId"), code);
    case QModbusPdu::ReadFileRecord:               return named(QStringLiteral("ReadFileRecord"), code);
    case QModbusPdu::WriteFileRecord:              return named(QStringLiteral("WriteFileRecord"), code);
    case QModbusPdu::MaskWriteRegister:            return named(QStringLiteral("MaskWriteRegister"), code);
    case QModbusPdu::ReadWriteMultipleRegisters:   return named(QStringLiteral("ReadWriteMultipleRegisters"), code);
    case QModbusPdu::ReadFifoQueue:                return named(QStringLiteral("ReadFifoQueue"), code);
    case QModbusPdu::EncapsulatedInterfaceTransport:
        return named(QStringLiteral("EncapsulatedInterfaceTransport"), code);
    case QModbusPdu::Invalid:
    case QModbusPdu::UndefinedFunctionCode:
        break;
    }
    return named(QStringLiteral("Unknown"), code);
}

QString exceptionName(QModbusPdu::ExceptionCode code)
{
    // The trailing hint is the point of this function. A bare "exception 2" sends the reader to a
    // specification; "the peer does not have that address mapped" sends them to the register
    // settings, which is where the fix is.
    switch (code) {
    case QModbusPdu::IllegalFunction:
        return named(QStringLiteral("IllegalFunction"), code)
               + QStringLiteral(" - the peer does not support that function code at all");
    case QModbusPdu::IllegalDataAddress:
        return named(QStringLiteral("IllegalDataAddress"), code)
               + QStringLiteral(" - the peer does not have that address range mapped;"
                                " check start address and count against the peer's own map");
    case QModbusPdu::IllegalDataValue:
        return named(QStringLiteral("IllegalDataValue"), code)
               + QStringLiteral(" - the quantity or a value is outside what the peer accepts");
    case QModbusPdu::ServerDeviceFailure:
        return named(QStringLiteral("ServerDeviceFailure"), code)
               + QStringLiteral(" - the peer failed internally while handling the request");
    case QModbusPdu::Acknowledge:
        return named(QStringLiteral("Acknowledge"), code);
    case QModbusPdu::ServerDeviceBusy:
        return named(QStringLiteral("ServerDeviceBusy"), code)
               + QStringLiteral(" - the peer is busy; retry later");
    case QModbusPdu::NegativeAcknowledge:
        return named(QStringLiteral("NegativeAcknowledge"), code);
    case QModbusPdu::MemoryParityError:
        return named(QStringLiteral("MemoryParityError"), code);
    case QModbusPdu::GatewayPathUnavailable:
        return named(QStringLiteral("GatewayPathUnavailable"), code)
               + QStringLiteral(" - a gateway could not route to the unit id");
    case QModbusPdu::GatewayTargetDeviceFailedToRespond:
        return named(QStringLiteral("GatewayTargetDeviceFailedToRespond"), code)
               + QStringLiteral(" - a gateway reached no device at that unit id;"
                                " check the unit / slave id");
    case QModbusPdu::ExtendedException:
        return named(QStringLiteral("ExtendedException"), code);
    }
    return named(QStringLiteral("UnknownException"), code);
}

QString hexDump(const QByteArray &bytes)
{
    QStringList parts;
    parts.reserve(bytes.size());
    for (const char byte : bytes) {
        parts << QString::number(static_cast<quint8>(byte), 16)
                     .rightJustified(2, QLatin1Char('0')).toUpper();
    }
    return parts.join(QLatin1Char(' '));
}

QString describePdu(const QModbusPdu &pdu)
{
    QString text = QStringLiteral("fn=%1 data=[%2]")
                       .arg(functionName(pdu.functionCode()), hexDump(pdu.data()));
    if (pdu.isException()) {
        // Name and code only. The explanatory hint belongs to the one line that leads with the
        // exception; repeating it inside an embedded PDU rendering made the log line twice as
        // long and said nothing twice.
        text += QStringLiteral(" EXCEPTION=%1")
                    .arg(exceptionName(pdu.exceptionCode()).section(QStringLiteral(" - "), 0, 0));
    }
    return text;
}

QString describeUnit(const QModbusDataUnit &unit, bool withValues)
{
    const int start = int(unit.startAddress());
    const int count = int(unit.valueCount());
    QString text = QStringLiteral("%1[%2..%3] count=%4")
                       .arg(prefixFor(unit.registerType()))
                       .arg(start)
                       .arg(count > 0 ? start + count - 1 : start)
                       .arg(count);

    if (withValues && count > 0) {
        QStringList values;
        // Capped: a 125-register dump is already long, and the tail is rarely what is wrong.
        const int shown = qMin(count, 32);
        values.reserve(shown);
        for (int i = 0; i < shown; ++i) {
            values << QString::number(unit.value(i));
        }
        text += QStringLiteral(" values=[%1%2]")
                    .arg(values.join(QStringLiteral(", ")),
                         count > shown ? QStringLiteral(", …") : QString());
    }
    return text;
}

QString describeRange(ModbusArea area, int start, int count)
{
    return describeUnit(QModbusDataUnit(toQtRegisterType(area), start, quint16(qMax(0, count))));
}

QString describeValues(const QMap<QString, QVariant> &values)
{
    QStringList parts;
    parts.reserve(values.size());
    // Capped for the same reason describeUnit() caps: a full poll can change hundreds of tags at
    // once on the first pass after a peer restarts, and the log line stops being readable.
    int shown = 0;
    for (auto it = values.cbegin(); it != values.cend() && shown < 32; ++it, ++shown) {
        parts << QStringLiteral("%1=%2").arg(it.key(), it.value().toString());
    }
    if (values.size() > shown) {
        parts << QStringLiteral("… (%1 more)").arg(values.size() - shown);
    }
    return parts.join(QStringLiteral(", "));
}

} // namespace vc::device::modbus_trace
