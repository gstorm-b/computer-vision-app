#include "datacodec.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QtEndian>

#include <algorithm>
#include <cstring>

namespace {

using namespace DataCodec;

QString msg(const char *text)
{
    return QCoreApplication::translate("DataCodec", text);
}

//! Every one of the four layouts is its own inverse, so this helper is used
//! both when reading registers and when writing them.
QByteArray applyOrder(QByteArray raw, int order)
{
    switch (order) {
    case OrderABCD:
        break;
    case OrderDCBA:
        std::reverse(raw.begin(), raw.end());
        break;
    case OrderBADC:
        for (qsizetype i = 0; i + 1 < raw.size(); i += 2)
            std::swap(raw[i], raw[i + 1]);
        break;
    case OrderCDAB: {
        QByteArray swapped;
        swapped.reserve(raw.size());
        for (qsizetype i = raw.size() - 2; i >= 0; i -= 2) {
            swapped.append(raw.at(i));
            swapped.append(raw.at(i + 1));
        }
        raw = swapped;
        break;
    }
    default:
        break;
    }
    return raw;
}

//! Word order is meaningless for text, so a string only honours the byte swap.
int effectiveOrder(int type, int order)
{
    if (type != AsciiString)
        return order;
    return (order == OrderBADC || order == OrderDCBA) ? OrderBADC : OrderABCD;
}

QByteArray registersToBytes(const QList<quint16> &regs, int type, int order)
{
    QByteArray raw;
    raw.reserve(regs.size() * 2);
    for (quint16 value : regs) {
        raw.append(char(quint8(value >> 8)));
        raw.append(char(quint8(value & 0xFF)));
    }
    return applyOrder(raw, effectiveOrder(type, order));
}

QList<quint16> bytesToRegisters(const QByteArray &normalised, int type, int order)
{
    const QByteArray raw = applyOrder(normalised, effectiveOrder(type, order));

    QList<quint16> regs;
    regs.reserve(raw.size() / 2);
    for (qsizetype i = 0; i + 1 < raw.size(); i += 2)
        regs.append(quint16((quint16(quint8(raw.at(i))) << 8) | quint8(raw.at(i + 1))));
    return regs;
}

QByteArray bigEndianBytes(quint64 value, int byteCount)
{
    QByteArray raw(byteCount, char(0));
    for (int i = byteCount - 1; i >= 0; --i) {
        raw[i] = char(quint8(value & 0xFF));
        value >>= 8;
    }
    return raw;
}

QString hexText(quint64 value, int digits)
{
    return QStringLiteral("0x")
           + QString::number(value, 16).toUpper().rightJustified(digits, QLatin1Char('0'));
}

bool bcdToValue(const QByteArray &raw, quint64 *value)
{
    quint64 result = 0;
    for (char c : raw) {
        const quint8 byte = quint8(c);
        const quint8 high = byte >> 4;
        const quint8 low = byte & 0x0F;
        if (high > 9 || low > 9)
            return false;
        result = result * 100 + high * 10 + low;
    }
    *value = result;
    return true;
}

bool valueToBcd(quint64 value, int byteCount, QByteArray *raw)
{
    QByteArray out(byteCount, char(0));
    for (int i = byteCount - 1; i >= 0; --i) {
        const quint8 low = quint8(value % 10);
        value /= 10;
        const quint8 high = quint8(value % 10);
        value /= 10;
        out[i] = char((high << 4) | low);
    }
    if (value != 0)
        return false;
    *raw = out;
    return true;
}

//! Accepts plain decimal, 0x1234 and $1234.
qint64 parseSigned(const QString &text, bool *ok)
{
    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        return qint64(trimmed.mid(2).toULongLong(ok, 16));
    if (trimmed.startsWith(QLatin1Char('$')))
        return qint64(trimmed.mid(1).toULongLong(ok, 16));
    return trimmed.toLongLong(ok);
}

quint64 parseUnsigned(const QString &text, bool *ok)
{
    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        return trimmed.mid(2).toULongLong(ok, 16);
    if (trimmed.startsWith(QLatin1Char('$')))
        return trimmed.mid(1).toULongLong(ok, 16);
    return trimmed.toULongLong(ok);
}

bool fail(QString *error, const QString &text)
{
    if (error)
        *error = text;
    return false;
}

} // namespace

QStringList DataCodec::typeNames()
{
    return {
        msg("INT16 - signed word"),
        msg("UINT16 - unsigned word"),
        msg("HEX16 - word in hex"),
        msg("BCD16 - 4 BCD digits"),
        msg("INT32 - signed double word"),
        msg("UINT32 - unsigned double word"),
        msg("HEX32 - double word in hex"),
        msg("BCD32 - 8 BCD digits"),
        msg("INT64 - signed quad word"),
        msg("UINT64 - unsigned quad word"),
        msg("FLOAT32 - IEEE 754 single (REAL)"),
        msg("FLOAT64 - IEEE 754 double (LREAL)"),
        msg("ASCII text"),
    };
}

QStringList DataCodec::byteOrderNames()
{
    return {
        msg("ABCD - big endian (Modbus standard)"),
        msg("CDAB - word swapped (most PLCs)"),
        msg("BADC - byte swapped"),
        msg("DCBA - little endian"),
    };
}

QString DataCodec::typeName(int type)
{
    const QStringList names = typeNames();
    return (type >= 0 && type < names.size()) ? names.at(type) : QString();
}

QString DataCodec::byteOrderName(int order)
{
    const QStringList names = byteOrderNames();
    return (order >= 0 && order < names.size()) ? names.at(order) : QString();
}

bool DataCodec::hasVariableLength(int type)
{
    return type == AsciiString;
}

int DataCodec::registerCount(int type, int requestedLength)
{
    switch (type) {
    case Int16:
    case UInt16:
    case Hex16:
    case Bcd16:
        return 1;
    case Int32:
    case UInt32:
    case Hex32:
    case Bcd32:
    case Float32:
        return 2;
    case Int64:
    case UInt64:
    case Float64:
        return 4;
    case AsciiString:
        return qMax(1, requestedLength);
    default:
        return 0;
    }
}

QString DataCodec::decode(int type, int order, const QList<quint16> &regs)
{
    const int needed = registerCount(type, int(regs.size()));
    if (needed <= 0 || regs.size() < needed)
        return QString();

    const QByteArray raw = registersToBytes(regs.mid(0, needed), type, order);

    switch (type) {
    case Int16:
        return QString::number(qint16(qFromBigEndian<quint16>(raw.constData())));
    case UInt16:
        return QString::number(qFromBigEndian<quint16>(raw.constData()));
    case Hex16:
        return hexText(qFromBigEndian<quint16>(raw.constData()), 4);
    case Int32:
        return QString::number(qint32(qFromBigEndian<quint32>(raw.constData())));
    case UInt32:
        return QString::number(qFromBigEndian<quint32>(raw.constData()));
    case Hex32:
        return hexText(qFromBigEndian<quint32>(raw.constData()), 8);
    case Int64:
        return QString::number(qint64(qFromBigEndian<quint64>(raw.constData())));
    case UInt64:
        return QString::number(qFromBigEndian<quint64>(raw.constData()));
    case Bcd16:
    case Bcd32: {
        quint64 value = 0;
        if (!bcdToValue(raw, &value))
            return msg("<invalid BCD>");
        return QString::number(value);
    }
    case Float32: {
        const quint32 bits = qFromBigEndian<quint32>(raw.constData());
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof value);
        return QString::number(double(value), 'g', 7);
    }
    case Float64: {
        const quint64 bits = qFromBigEndian<quint64>(raw.constData());
        double value = 0.0;
        std::memcpy(&value, &bits, sizeof value);
        return QString::number(value, 'g', 16);
    }
    case AsciiString: {
        QByteArray text = raw;
        const qsizetype terminator = text.indexOf('\0');
        if (terminator >= 0)
            text.truncate(terminator);
        return QString::fromLatin1(text);
    }
    default:
        return QString();
    }
}

bool DataCodec::encode(int type, int order, const QString &text, int length,
                       QList<quint16> *out, QString *error)
{
    if (!out)
        return false;

    const int needed = registerCount(type, length);
    if (needed <= 0)
        return fail(error, msg("Unknown data type."));

    QByteArray raw;
    bool ok = true;

    switch (type) {
    case Int16: {
        const qint64 value = parseSigned(text, &ok);
        if (!ok)
            return fail(error, msg("Not a whole number."));
        if (value < -32768 || value > 32767)
            return fail(error, msg("INT16 must be between -32768 and 32767."));
        raw = bigEndianBytes(quint16(qint16(value)), 2);
        break;
    }
    case UInt16:
    case Hex16: {
        const quint64 value = parseUnsigned(text, &ok);
        if (!ok)
            return fail(error, msg("Not a whole number."));
        if (value > 0xFFFF)
            return fail(error, msg("Value must be between 0 and 65535."));
        raw = bigEndianBytes(value, 2);
        break;
    }
    case Int32: {
        const qint64 value = parseSigned(text, &ok);
        if (!ok)
            return fail(error, msg("Not a whole number."));
        if (value < -2147483648LL || value > 2147483647LL)
            return fail(error, msg("INT32 must be between -2147483648 and 2147483647."));
        raw = bigEndianBytes(quint32(qint32(value)), 4);
        break;
    }
    case UInt32:
    case Hex32: {
        const quint64 value = parseUnsigned(text, &ok);
        if (!ok)
            return fail(error, msg("Not a whole number."));
        if (value > 0xFFFFFFFFULL)
            return fail(error, msg("Value must be between 0 and 4294967295."));
        raw = bigEndianBytes(value, 4);
        break;
    }
    case Int64: {
        const qint64 value = parseSigned(text, &ok);
        if (!ok)
            return fail(error, msg("Not a whole number."));
        raw = bigEndianBytes(quint64(value), 8);
        break;
    }
    case UInt64: {
        const quint64 value = parseUnsigned(text, &ok);
        if (!ok)
            return fail(error, msg("Not a whole number."));
        raw = bigEndianBytes(value, 8);
        break;
    }
    case Bcd16:
    case Bcd32: {
        const quint64 value = parseUnsigned(text, &ok);
        if (!ok)
            return fail(error, msg("Not a whole number."));
        if (!valueToBcd(value, needed * 2, &raw)) {
            return fail(error, type == Bcd16
                                   ? msg("BCD16 holds 0 to 9999.")
                                   : msg("BCD32 holds 0 to 99999999."));
        }
        break;
    }
    case Float32: {
        const float value = text.trimmed().toFloat(&ok);
        if (!ok)
            return fail(error, msg("Not a floating point number."));
        quint32 bits = 0;
        std::memcpy(&bits, &value, sizeof bits);
        raw = bigEndianBytes(bits, 4);
        break;
    }
    case Float64: {
        const double value = text.trimmed().toDouble(&ok);
        if (!ok)
            return fail(error, msg("Not a floating point number."));
        quint64 bits = 0;
        std::memcpy(&bits, &value, sizeof bits);
        raw = bigEndianBytes(bits, 8);
        break;
    }
    case AsciiString: {
        QByteArray latin1 = text.toLatin1();
        const int capacity = needed * 2;
        if (latin1.size() > capacity) {
            return fail(error, msg("Text is longer than the reserved registers "
                                   "(2 characters per register)."));
        }
        latin1.append(QByteArray(capacity - int(latin1.size()), char(0)));
        raw = latin1;
        break;
    }
    default:
        return fail(error, msg("Unknown data type."));
    }

    *out = bytesToRegisters(raw, type, order);
    if (out->size() != needed)
        return fail(error, msg("Internal conversion error."));
    return true;
}
