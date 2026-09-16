#ifndef MODBUS_TRACE_H
#define MODBUS_TRACE_H

/**
 * @file modbus_trace.h
 * @brief Human-readable renderings of Modbus frames, function codes and exception codes, for the
 *        device logs.
 *
 * A Modbus failure is almost always diagnosable from three facts: which function code was sent, to
 * which address range, and which exception came back. Without them the log says "the request
 * failed" and the reader is left guessing — which is exactly the position this file exists to
 * remove. Exception codes in particular are useless as bare numbers and decisive as names:
 * *IllegalDataAddress* points at the register map, *IllegalFunction* at the peer's capabilities,
 * *ServerDeviceFailure* at the peer itself.
 *
 * @note Rendering is deliberately cheap for the common case. The per-frame trace is gated behind
 *       the config's `traceProtocol` flag, because a 100 ms poll over four areas is 40 lines a
 *       second forever. **Failures are dumped in full regardless of the flag** — the moment you
 *       need the bytes is the moment something went wrong, and being told to reproduce it with
 *       tracing enabled is a wasted round trip.
 */

#include "device/plc/modbus/modbus_register_map.h"

#include <QMap>
#include <QModbusDataUnit>
#include <QModbusPdu>
#include <QString>
#include <QVariant>

namespace vc::device::modbus_trace {

/// Renders a Modbus function code as "ReadHoldingRegisters (0x03)".
QString functionName(QModbusPdu::FunctionCode code);

/// Renders a Modbus exception code as "IllegalDataAddress (0x02)", with the plain-language
/// consequence appended for the codes an integrator actually hits.
QString exceptionName(QModbusPdu::ExceptionCode code);

/// Renders bytes as space-separated uppercase hex, e.g. "03 00 00 00 40".
QString hexDump(const QByteArray &bytes);

/// Renders a PDU as "fn=ReadHoldingRegisters (0x03) data=[00 00 00 40]", including the exception
/// code when the PDU is an exception response.
QString describePdu(const QModbusPdu &pdu);

/// Renders a data unit as "HR[100..163] count=64", optionally with its values.
QString describeUnit(const QModbusDataUnit &unit, bool withValues = false);

/// Renders our own area/address/count triple the same way describeUnit() renders Qt's, so a
/// request logged before it is built and a reply logged after it arrives read alike.
QString describeRange(ModbusArea area, int start, int count);

/// Renders a changed-value batch as "COIL00005=false, HR00010=4321".
/// @note The logger writes through a QTextStream, which has no operator for a QMap — so the
///       rendering has to happen here rather than at the log site.
QString describeValues(const QMap<QString, QVariant> &values);

} // namespace vc::device::modbus_trace

#endif // MODBUS_TRACE_H
