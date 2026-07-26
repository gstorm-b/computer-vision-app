#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <QByteArray>

/// Byte-array and IEEE-754 bit-pattern conversion helpers shared by the MC
/// protocol codec (framing/parsing register values on the wire).
namespace vc::device {

/// Appends the two bytes of `value` to `array` in the requested byte order.
/// @param littleEndian true appends LSB then MSB; false appends MSB then LSB
inline void appendToByteArray_uint16(QByteArray &array, quint16 value, bool littleEndian = true) {
    if (littleEndian) {
        array.append(static_cast<char>(value & 0xFF));         // LSB
        array.append(static_cast<char>((value >> 8) & 0xFF));  // MSB
    } else {
        array.append(static_cast<char>((value >> 8) & 0xFF));  // MSB
        array.append(static_cast<char>(value & 0xFF));         // LSB
    }
}

/// Appends the low `byteCount` bytes of `value` to `array` in the requested
/// byte order. No-op if `byteCount` is outside [1, 4].
/// @param byteCount number of bytes of `value` to emit (1-4)
/// @param littleEndian true emits least-significant byte first; false emits most-significant byte first
inline void appendToByteArray_uint32(QByteArray &array, quint32 value, int byteCount, bool littleEndian = true) {
    if (byteCount < 1 || byteCount > 4) {
        return;
    }

    for (int i = 0; i < byteCount; ++i) {
        int shift = littleEndian ? (8 * i) : (8 * (byteCount - 1 - i));
        char byte = static_cast<char>((value >> shift) & 0xFF);
        array.append(byte);
    }
}

/// Reads two bytes at `data[index]`/`data[index+1]` and combines them into a
/// quint16 using the requested byte order.
/// @return 0 if `index` is negative or `index + 1` is out of bounds
inline quint16 convert_uint16_FromBytes(const QByteArray& data, int index, bool littleEndian = true) {
    if (index < 0 || index + 1 >= data.size()) {
        return 0;
    }

    quint8 byte1 = static_cast<quint8>(data[index]);
    quint8 byte2 = static_cast<quint8>(data[index + 1]);

    if (littleEndian) {
        return static_cast<quint16>(byte1 | (byte2 << 8));
    } else {
        return static_cast<quint16>((byte1 << 8) | byte2);
    }
}

/// Reinterprets the raw 32-bit pattern `dword` as an IEEE-754 float (bitwise
/// memcpy, no numeric conversion).
inline float real32ToFloat(quint32 dword) {
    float result;
    std::memcpy(&result, &dword, sizeof(float));
    return result;
}

/// Reinterprets `value`'s IEEE-754 bit pattern as a 32-bit unsigned integer
/// (bitwise memcpy, no numeric conversion).
inline quint32 floatToReal32(float value) {
    quint32 result;
    std::memcpy(&result, &value, sizeof(quint32));
    return result;
}

/// Reinterprets the raw 64-bit pattern `dword` as an IEEE-754 double (bitwise
/// memcpy, no numeric conversion).
inline double real64ToDouble(quint64 dword) {
    double result;
    std::memcpy(&result, &dword, sizeof(result));
    return result;
}

/// Reinterprets `value`'s IEEE-754 bit pattern as a 64-bit unsigned integer
/// (bitwise memcpy, no numeric conversion).
inline quint64 doubleToReal64(double value) {
    quint64 result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

} // namespace vc::device

#endif // MEMORY_UTILS_H
