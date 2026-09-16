#ifndef MODBUS_RESULT_LAYOUT_H
#define MODBUS_RESULT_LAYOUT_H

/**
 * @file modbus_result_layout.h
 * @brief The Modbus vision-result register layout: the single place the wire contract lives.
 *
 * @warning **This is a wire contract, not an implementation detail.** Once a customer PLC
 *          program reads these registers, the offsets, the scale factor, the word order and the
 *          handshake cannot be changed without rewriting ladder code on a machine that is
 *          already running. Treat every constant below the way `VisionOutputPosition::toString()`
 *          treats its field format: it is a promise, and the only safe change is an additive one
 *          into the reserved header word.
 *
 * The full contract, including what a PLC programmer must **not** assume, is written down in
 * `docs/domains/task_localization/modbus_result_contract.md`. That document is the specification;
 * this header is its implementation, and the two are expected to be read together.
 *
 * Layout summary (all offsets relative to the configured result start address, in the
 * holding-register area):
 *
 *     +0            count      number of valid positions; 0 while a publish is in progress
 *     +1            sequence   1..32767, increments on every publish, wraps to 1
 *     +2            flags      bit0 = low area, bit1 = truncated
 *     +3            reserved   written as 0
 *     +4 .. +4+12N  positions  N positions, 12 registers each
 *
 * Each position is six axes in the order x, y, z, rx, ry, rz. Each axis is a signed 32-bit
 * fixed-point value scaled by 100 (so 12.34 mm is 1234), stored **high word first**.
 */

#include "device/output_device/vision_output_request.h"

#include <QList>
#include <QVector>
#include <QtGlobal>

namespace vc::device {

/**
 * @class ModbusResultLayout
 * @brief Encodes and decodes the vision-result register block. Stateless; all members static.
 */
class ModbusResultLayout {
public:
    // ── Shape ────────────────────────────────────────────────────────────────────────────────
    static constexpr int kHeaderRegisters     = 4;   ///< count, sequence, flags, reserved.
    static constexpr int kAxesPerPosition     = 6;   ///< x, y, z, rx, ry, rz.
    static constexpr int kRegistersPerAxis    = 2;   ///< One signed 32-bit value per axis.
    static constexpr int kRegistersPerPosition = kAxesPerPosition * kRegistersPerAxis;  ///< 12.

    // ── Header offsets ───────────────────────────────────────────────────────────────────────
    static constexpr int kOffsetCount    = 0;  ///< Valid position count; written LAST.
    static constexpr int kOffsetSequence = 1;  ///< Publish counter, 1..32767.
    static constexpr int kOffsetFlags    = 2;  ///< Bit flags, see kFlag*.
    static constexpr int kOffsetReserved = 3;  ///< Always written as 0; the only additive room.

    // ── Flag bits ────────────────────────────────────────────────────────────────────────────
    /// The matcher reported the matched area below its configured limit for this cycle.
    static constexpr quint16 kFlagLowArea   = 0x0001;
    /// More positions were produced than the configured block can hold; the extra ones are
    /// **not** in the registers. `count` reports what fits, never what was found.
    static constexpr quint16 kFlagTruncated = 0x0002;

    // ── Encoding ─────────────────────────────────────────────────────────────────────────────
    /// Fixed-point scale: the register value is the millimetre/degree value multiplied by this
    /// and rounded. 100 gives two decimals, which is exactly the resolution the TCP/IP result
    /// frame has always sent ("%08.2f"), so the same commissioned pose reads identically on
    /// both transports.
    static constexpr int kScale = 100;

    /// Highest publish sequence number before wrapping back to 1. Kept inside the positive
    /// range of a signed 16-bit word so a PLC reading the register as a signed integer — which
    /// most ladder code does by default — never sees it go negative.
    static constexpr quint16 kMaxSequence = 32767;

    /// Largest position count whose complete block (header + payload) still fits in one Modbus
    /// "write multiple registers" request, whose payload ceiling is 123 registers. Above this
    /// the publish necessarily spans two requests, which is exactly why `count` is written last.
    static constexpr int kMaxPositionsInSingleRequest = 9;

    /// Total registers occupied by a result block holding up to `maxPositions` positions.
    static int registerCount(int maxPositions)
    {
        return kHeaderRegisters + qMax(0, maxPositions) * kRegistersPerPosition;
    }

    /**
     * @brief Encodes the position payload (everything after the header).
     * @param[in]  positions    the cycle's positions, in send order
     * @param[in]  maxPositions capacity of the configured block
     * @param[out] truncated    set to true when `positions` did not fit; may be null
     * @return exactly `maxPositions * kRegistersPerPosition` registers, zero-filled past the
     *         last encoded position.
     * @note The payload is always full length. Writing only the occupied part would leave the
     *       previous cycle's poses visible in the tail, where a PLC program that trusts a stale
     *       `count` would read them as current.
     */
    static QList<quint16> encodePayload(const QVector<VisionOutputPosition> &positions,
                                        int maxPositions,
                                        bool *truncated = nullptr);

    /// Encodes the four header registers, `count` included.
    /// @note The caller must still write the header AFTER the payload, and must clear `count`
    ///       to 0 before starting; see the handshake described in the contract doc.
    static QList<quint16> encodeHeader(int count, quint16 sequence, quint16 flags);

    /// Decodes `count` positions from a payload produced by encodePayload(). Used by the tests
    /// and by the server device when reading its own block back for display.
    static QVector<VisionOutputPosition> decodePayload(const QList<quint16> &payload, int count);

    /// Advances a publish sequence number, wrapping `kMaxSequence` back to 1. Zero is never a
    /// valid sequence, so a PLC can treat 0 as "nothing published since power-on".
    static quint16 nextSequence(quint16 current)
    {
        return (current >= kMaxSequence) ? 1 : static_cast<quint16>(current + 1);
    }

    /// Encodes one axis value into its two registers, high word first.
    static void encodeAxis(double value, quint16 *highWord, quint16 *lowWord);
    /// Decodes one axis value from its two registers, high word first.
    static double decodeAxis(quint16 highWord, quint16 lowWord);
};

} // namespace vc::device

#endif // MODBUS_RESULT_LAYOUT_H
