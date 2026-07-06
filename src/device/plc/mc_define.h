#ifndef MC_DEFINE_H
#define MC_DEFINE_H

#include "core/utils/meta_utils.h"

#define MC_C_STX                    0x02
#define MC_C_ETX                    0x03
#define MC_C_EOT                    0x04
#define MC_C_ENQ                    0x05
#define MC_C_ACK                    0x06
#define MC_C_LF                     0x0A
#define MC_C_CL                     0x0C
#define MC_C_CR                     0x0D
#define MC_C_NAK                    0x15

#define MC_C_BR_H                   0x42
#define MC_C_BR_L                   0x52

#define MC_C_WR_H                   0x57
#define MC_C_WR_L                   0x52

#define MC_C_BW_H                   0x42
#define MC_C_BW_L                   0x57

#define MC_C_WW_H                   0x57
#define MC_C_WW_L                   0x57

#define MC_C_NAK_NONE_ERR           0x00
#define MC_C_NAK_CHECKSUM           0x02
#define MC_C_NAK_PROTOCOL           0x03
#define MC_C_NAK_AREA_ERR           0x06
#define MC_C_NAK_CHARACTER          0x07
#define MC_C_NAK_PLC_NUM            0x0A
#define MC_C_NAK_PLC_NUM_1          0x10
#define MC_C_NAK_REMOETE_CONTROL    0x18

#define MC_C_DEIVCE_X               0x58
#define MC_C_DEIVCE_Y               0x59
#define MC_C_DEIVCE_M               0x4D
#define MC_C_DEIVCE_D               0x44

#define MC_E_DEIVCE_X               0x009C
#define MC_E_DEIVCE_Y               0x009D
#define MC_E_DEIVCE_M               0x0090
#define MC_E_DEIVCE_D               0x00A8

#define MC_MSG_ETHERNET_TCP         "Ethernet TCP/IP"
#define MC_MSG_ETHERNET_UDP         "Ethernet UDP"
#define MC_MSG_SERIAL               "Serial (COM Port)"

#define MC_DATA_CODE_BINARY         "Binary"
#define MC_DATA_CODE_ASCII          "Ascii"

#define MC_FRAME_1E                 "1E"
#define MC_FRAME_3E                 "3E"
#define MC_FRAME_1C                 "1C"
#define MC_FRAME_3C                 "3C"

namespace vc::device::mc {
Q_NAMESPACE

enum McFrameType {
    Frame_User,
    Frame_1E,
    Frame_3E,
    Frame_1C,
    Frame_3C
};
Q_ENUM_NS(McFrameType)

enum McMsgItfType {
    MsgItf_User,
    EthernetTCPIP,
    EthernetUDP,
    SerialPort
};
Q_ENUM_NS(McMsgItfType)

enum McDataCode {
    DataCode_User,
    Binary,
    Ascii
};
Q_ENUM_NS(McDataCode)

// only use for lingust
static inline const char* enum_keys_mc_defines[] = {
    // McFrameType
    QT_TR_NOOP("Frame_User"),
    QT_TR_NOOP("Frame_1E"),
    QT_TR_NOOP("Frame_3E"),
    QT_TR_NOOP("Frame_1C"),
    QT_TR_NOOP("Frame_3C"),

    // McMsgItfType
    QT_TR_NOOP("MsgItf_User"),
    QT_TR_NOOP("EthernetTCPIP"),
    QT_TR_NOOP("EthernetUDP"),
    QT_TR_NOOP("SerialPort"),

    // McDataCode
    QT_TR_NOOP("DataCode_User"),
    QT_TR_NOOP("Binary"),
    QT_TR_NOOP("Ascii")
};

[[maybe_unused]] static bool Eframe_Binary_Device_Code(char device_type, quint32 &code) {
    switch (device_type) {
    case 'X':
        code = MC_E_DEIVCE_X;
        return true;
    case 'Y':
        code = MC_E_DEIVCE_Y;
        return true;
    case 'M':
        code = MC_E_DEIVCE_M;
        return true;
    case 'D':
        code = MC_E_DEIVCE_D;
        return true;
    }
    return false;
}

[[maybe_unused]] static QString McFrameTypeToString(McFrameType t) {
    return qenumToString(t);
};

[[maybe_unused]] static McFrameType McFrameTypeFromString(QString t) {
    return stringToQEnum(t, McFrameType::Frame_User);
};

[[maybe_unused]] static QString McMsgItfTypeToString(McMsgItfType t) {
    return qenumToString(t);
};

[[maybe_unused]] static McMsgItfType McMsgItfTypeFromString(QString t) {
    return stringToQEnum(t, McMsgItfType::MsgItf_User);
};


[[maybe_unused]] static QString McDataCodeToString(McDataCode t) {
    return qenumToString(t);
};

[[maybe_unused]] static McDataCode McDataCodeFromString(QString t) {
    return stringToQEnum(t, McDataCode::DataCode_User);

};

}

Q_DECLARE_METATYPE(vc::device::mc::McFrameType)
Q_DECLARE_METATYPE(vc::device::mc::McMsgItfType)
Q_DECLARE_METATYPE(vc::device::mc::McDataCode)

#endif // MC_DEFINE_H
