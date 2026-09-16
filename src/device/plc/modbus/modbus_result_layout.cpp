/**
 * @file modbus_result_layout.cpp
 * @brief Implementation of the Modbus vision-result register layout.
 */

#include "device/plc/modbus/modbus_result_layout.h"

#include <cmath>
#include <limits>

namespace vc::device {

namespace {

/// Converts a millimetre/degree value to the scaled 32-bit fixed-point form used on the wire.
///
/// A non-finite value encodes as 0 rather than propagating a NaN into a register a robot will
/// act on: `qRound()` on a NaN is undefined, and a NaN that reached the PLC as an arbitrary bit
/// pattern would be a pose command. The clamp is the same argument for the finite case — the
/// scaled range is ±21,474,836.47, four orders of magnitude beyond any real workspace, so a
/// value that exceeds it is a fault upstream, and saturating is the least dangerous answer that
/// still keeps the field well-formed.
qint32 toFixedPoint(double value)
{
    if (!std::isfinite(value)) {
        return 0;
    }
    const double scaled = value * ModbusResultLayout::kScale;
    if (scaled >= static_cast<double>(std::numeric_limits<qint32>::max())) {
        return std::numeric_limits<qint32>::max();
    }
    if (scaled <= static_cast<double>(std::numeric_limits<qint32>::min())) {
        return std::numeric_limits<qint32>::min();
    }
    return static_cast<qint32>(std::llround(scaled));
}

} // namespace

void ModbusResultLayout::encodeAxis(double value, quint16 *highWord, quint16 *lowWord)
{
    const quint32 raw = static_cast<quint32>(toFixedPoint(value));
    if (highWord) {
        *highWord = static_cast<quint16>((raw >> 16) & 0xFFFFu);
    }
    if (lowWord) {
        *lowWord = static_cast<quint16>(raw & 0xFFFFu);
    }
}

double ModbusResultLayout::decodeAxis(quint16 highWord, quint16 lowWord)
{
    const quint32 raw = (static_cast<quint32>(highWord) << 16) | static_cast<quint32>(lowWord);
    return static_cast<double>(static_cast<qint32>(raw)) / kScale;
}

QList<quint16> ModbusResultLayout::encodePayload(const QVector<VisionOutputPosition> &positions,
                                                 int maxPositions,
                                                 bool *truncated)
{
    const int capacity = qMax(0, maxPositions);
    const int encoded = qMin(capacity, static_cast<int>(positions.size()));
    if (truncated) {
        *truncated = positions.size() > capacity;
    }

    QList<quint16> payload;
    payload.fill(0, capacity * kRegistersPerPosition);

    for (int i = 0; i < encoded; ++i) {
        const VisionOutputPosition &position = positions.at(i);
        const double axes[kAxesPerPosition] = {
            position.x, position.y, position.z, position.rx, position.ry, position.rz
        };
        const int base = i * kRegistersPerPosition;
        for (int axis = 0; axis < kAxesPerPosition; ++axis) {
            quint16 high = 0;
            quint16 low = 0;
            encodeAxis(axes[axis], &high, &low);
            payload[base + axis * kRegistersPerAxis]     = high;
            payload[base + axis * kRegistersPerAxis + 1] = low;
        }
    }

    return payload;
}

QList<quint16> ModbusResultLayout::encodeHeader(int count, quint16 sequence, quint16 flags)
{
    QList<quint16> header;
    header.fill(0, kHeaderRegisters);
    header[kOffsetCount]    = static_cast<quint16>(qMax(0, count));
    header[kOffsetSequence] = sequence;
    header[kOffsetFlags]    = flags;
    header[kOffsetReserved] = 0;
    return header;
}

QVector<VisionOutputPosition> ModbusResultLayout::decodePayload(const QList<quint16> &payload,
                                                                int count)
{
    QVector<VisionOutputPosition> positions;
    const int available = payload.size() / kRegistersPerPosition;
    const int decoded = qBound(0, count, available);
    positions.reserve(decoded);

    for (int i = 0; i < decoded; ++i) {
        const int base = i * kRegistersPerPosition;
        double axes[kAxesPerPosition] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        for (int axis = 0; axis < kAxesPerPosition; ++axis) {
            axes[axis] = decodeAxis(payload.at(base + axis * kRegistersPerAxis),
                                    payload.at(base + axis * kRegistersPerAxis + 1));
        }
        VisionOutputPosition position;
        position.x  = axes[0];
        position.y  = axes[1];
        position.z  = axes[2];
        position.rx = axes[3];
        position.ry = axes[4];
        position.rz = axes[5];
        positions.append(position);
    }

    return positions;
}

} // namespace vc::device
