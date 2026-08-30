#ifndef DATACODEC_H
#define DATACODEC_H

//
// Conversion between raw Modbus 16 bit registers and the typed values PLC
// vendors expose on top of them (Mitsubishi K/H/E/BCD, IEC 61131 INT/DINT/REAL,
// ASCII strings, ...).
//
// A multi register value is stored as a byte sequence A B C D ... where A is
// the most significant byte. Vendors disagree on how those bytes are laid out
// across registers, so every conversion takes a ByteOrder:
//
//   ABCD  big endian          reg0 = AB, reg1 = CD   (Modbus specification)
//   CDAB  word swapped        reg0 = CD, reg1 = AB   (most PLCs, Mitsubishi D registers)
//   BADC  byte swapped        reg0 = BA, reg1 = DC
//   DCBA  little endian       reg0 = DC, reg1 = BA
//
// All four transforms are their own inverse, so the same helper is used for
// encoding and decoding.
//

#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace DataCodec {

enum Type {
    Int16 = 0,      //!< signed word            (IEC INT,  Mitsubishi K)
    UInt16,         //!< unsigned word
    Hex16,          //!< word shown in hex      (Mitsubishi H)
    Bcd16,          //!< 4 BCD digits           (Mitsubishi BCD)
    Int32,          //!< signed double word     (IEC DINT)
    UInt32,
    Hex32,
    Bcd32,          //!< 8 BCD digits
    Int64,          //!< IEC LINT
    UInt64,
    Float32,        //!< IEEE 754 single        (IEC REAL, Mitsubishi E)
    Float64,        //!< IEEE 754 double        (IEC LREAL)
    AsciiString,    //!< 2 characters per register
    TypeCount
};

enum ByteOrder {
    OrderABCD = 0,  //!< big endian, Modbus standard
    OrderCDAB,      //!< word swapped
    OrderBADC,      //!< byte swapped
    OrderDCBA,      //!< little endian
    ByteOrderCount
};

//! Combo box labels, index == enum value.
QStringList typeNames();
QStringList byteOrderNames();

QString typeName(int type);
QString byteOrderName(int order);

//! true when the caller chooses how many registers the value spans.
bool hasVariableLength(int type);

//! Registers occupied. requestedLength is only used for variable length types.
int registerCount(int type, int requestedLength = 1);

//! Human readable value. Returns an empty string when regs is too short.
QString decode(int type, int order, const QList<quint16> &regs);

//! Parses text into exactly registerCount(type, length) registers.
//! Returns false and fills error when the text is not valid for the type.
bool encode(int type, int order, const QString &text, int length,
            QList<quint16> *out, QString *error);

} // namespace DataCodec

#endif // DATACODEC_H
