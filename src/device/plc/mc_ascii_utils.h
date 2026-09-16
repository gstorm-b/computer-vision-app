#ifndef MC_ASCII_UTILS_H
#define MC_ASCII_UTILS_H

/**
 * @file mc_ascii_utils.h
 * @brief ASCII encoding helpers shared by the computer-link frame codecs (1C and 3C).
 *
 * The C-frames are ASCII on the wire: every number travels as zero-padded decimal or
 * upper-case hexadecimal characters, and the optional sum check is the low byte of a plain
 * character sum. Both codecs need exactly the same five operations, which is why they live
 * here rather than being written twice — two concrete users, not a speculative abstraction.
 */

#include <QByteArray>

namespace vc::device::mc_ascii {

/// Appends `value` to `out` as `width` zero-padded decimal characters.
/// A value too large for `width` is truncated to its low-order digits, which is what the
/// protocol does — the field width is fixed by the frame, not by the value.
inline void appendDec(QByteArray &out, int value, int width) {
    out.append(QByteArray::number(value).rightJustified(width, '0').right(width));
}

/// Appends `value` to `out` as `width` zero-padded UPPER-case hexadecimal characters.
/// @note Upper case is not cosmetic: the MC computer-link specification requires it, and a
///       lower-case digit is rejected by the PLC as a character error.
inline void appendHex(QByteArray &out, quint32 value, int width) {
    out.append(QByteArray::number(value, 16).toUpper().rightJustified(width, '0').right(width));
}

/// Parses `len` hexadecimal characters starting at `offset` in `data`.
/// @param[out] ok set to false when the range is out of bounds or not valid hexadecimal
/// @return the parsed value, or 0 when `ok` is false
inline quint32 parseHex(const QByteArray &data, int offset, int len, bool *ok) {
    if (offset < 0 || len <= 0 || (offset + len) > data.size()) {
        if (ok) *ok = false;
        return 0;
    }
    bool converted = false;
    const quint32 value = data.mid(offset, len).toUInt(&converted, 16);
    if (ok) *ok = converted;
    return converted ? value : 0;
}

/**
 * @brief Computes a computer-link sum check: the low byte of the arithmetic sum of the
 *        character codes in `data` over [from, from + len).
 * @return the sum's low byte, which the frame carries as two hexadecimal characters
 */
inline quint8 sumCheck(const QByteArray &data, int from, int len) {
    quint32 sum = 0;
    const int end = from + len;
    for (int i = from; i < end && i < data.size(); ++i) {
        sum += static_cast<quint8>(data.at(i));
    }
    return static_cast<quint8>(sum & 0xFF);
}

/// Appends the two-character sum check for [from, from + len) of `out` to `out` itself.
/// Call it once the summed region is complete and before any trailing terminator.
inline void appendSumCheck(QByteArray &out, int from, int len) {
    appendHex(out, sumCheck(out, from, len), 2);
}

} // namespace vc::device::mc_ascii

#endif // MC_ASCII_UTILS_H
