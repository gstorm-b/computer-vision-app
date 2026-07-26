#ifndef MC_DEFINE_H
#define MC_DEFINE_H

#include "core/utils/meta_utils.h"

/// ASCII-frame (1C/1E) protocol control characters.
#define MC_C_STX                    0x02
#define MC_C_ETX                    0x03
#define MC_C_EOT                    0x04
#define MC_C_ENQ                    0x05
#define MC_C_ACK                    0x06
#define MC_C_LF                     0x0A
#define MC_C_CL                     0x0C
#define MC_C_CR                     0x0D
#define MC_C_NAK                    0x15

/// ASCII-frame command codes (high/low byte pair) for bit read (BR).
#define MC_C_BR_H                   0x42
#define MC_C_BR_L                   0x52

/// ASCII-frame command codes (high/low byte pair) for word read (WR).
#define MC_C_WR_H                   0x57
#define MC_C_WR_L                   0x52

/// ASCII-frame command codes (high/low byte pair) for bit write (BW).
#define MC_C_BW_H                   0x42
#define MC_C_BW_L                   0x57

/// ASCII-frame command codes (high/low byte pair) for word write (WW).
#define MC_C_WW_H                   0x57
#define MC_C_WW_L                   0x57

/// ASCII-frame NAK error codes returned by the PLC in a NAK response.
#define MC_C_NAK_NONE_ERR           0x00
#define MC_C_NAK_CHECKSUM           0x02
#define MC_C_NAK_PROTOCOL           0x03
#define MC_C_NAK_AREA_ERR           0x06
#define MC_C_NAK_CHARACTER          0x07
#define MC_C_NAK_PLC_NUM            0x0A
#define MC_C_NAK_PLC_NUM_1          0x10
#define MC_C_NAK_REMOETE_CONTROL    0x18

/// ASCII-frame device type codes (single ASCII byte, e.g. 'X' = 0x58).
#define MC_C_DEIVCE_X               0x58
#define MC_C_DEIVCE_Y               0x59
#define MC_C_DEIVCE_M               0x4D
#define MC_C_DEIVCE_D               0x44

/// Binary/Ethernet-frame (3E) device codes, looked up via
/// Eframe_Binary_Device_Code() and encoded as the low byte of the device field.
#define MC_E_DEIVCE_X               0x009C
#define MC_E_DEIVCE_Y               0x009D
#define MC_E_DEIVCE_M               0x0090
#define MC_E_DEIVCE_D               0x00A8

/// Human-readable names for the McMsgItfType message-interface types.
#define MC_MSG_ETHERNET_TCP         "Ethernet TCP/IP"
#define MC_MSG_ETHERNET_UDP         "Ethernet UDP"
#define MC_MSG_SERIAL               "Serial (COM Port)"

/// Human-readable names for the McDataCode data-encoding types.
#define MC_DATA_CODE_BINARY         "Binary"
#define MC_DATA_CODE_ASCII          "Ascii"

/// Human-readable names for the McFrameType frame variants.
#define MC_FRAME_1E                 "1E"
#define MC_FRAME_3E                 "3E"
#define MC_FRAME_1C                 "1C"
#define MC_FRAME_3C                 "3C"

/// Mitsubishi MC-protocol enum/constant definitions, exposed to QML/Qt meta
/// system via Q_NAMESPACE so the enums below are usable as Q_ENUM_NS.
namespace vc::device::mc {
Q_NAMESPACE

/// MC protocol frame variants (1E/3E ASCII+binary, 1C/3C) supported by the factory.
enum McFrameType {
    Frame_User,
    Frame_1E,
    Frame_3E,
    Frame_1C,
    Frame_3C
};
Q_ENUM_NS(McFrameType)

/// Transport/message-interface types used to reach the PLC.
enum McMsgItfType {
    MsgItf_User,
    EthernetTCPIP,
    EthernetUDP,
    SerialPort
};
Q_ENUM_NS(McMsgItfType)

/// Data encoding used on the wire for a frame (binary vs. ASCII).
enum McDataCode {
    DataCode_User,
    Binary,
    Ascii
};
Q_ENUM_NS(McDataCode)

// only use for lingust
/// String table of the McFrameType/McMsgItfType/McDataCode enum key names,
/// wrapped in QT_TR_NOOP so `lupdate` picks them up for translation.
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

/// Looks up the binary/Ethernet-frame (3E) device code for an ASCII device
/// type letter ('X', 'Y', 'M', or 'D').
/// @param device_type ASCII device-type letter
/// @param code output; set to the matching MC_E_DEIVCE_* constant on success
/// @return true if `device_type` was recognized, false otherwise (code left unset)
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

/// Converts a McFrameType value to its enum key name (e.g. "Frame_3E").
[[maybe_unused]] static QString McFrameTypeToString(McFrameType t) {
    return qenumToString(t);
};

/// Parses a McFrameType enum key name back into its value.
/// @return the matching McFrameType, or Frame_User if `t` is not a valid key
[[maybe_unused]] static McFrameType McFrameTypeFromString(QString t) {
    return stringToQEnum(t, McFrameType::Frame_User);
};

/// Converts a McMsgItfType value to its enum key name (e.g. "EthernetTCPIP").
[[maybe_unused]] static QString McMsgItfTypeToString(McMsgItfType t) {
    return qenumToString(t);
};

/// Parses a McMsgItfType enum key name back into its value.
/// @return the matching McMsgItfType, or MsgItf_User if `t` is not a valid key
[[maybe_unused]] static McMsgItfType McMsgItfTypeFromString(QString t) {
    return stringToQEnum(t, McMsgItfType::MsgItf_User);
};


/// Converts a McDataCode value to its enum key name (e.g. "Binary").
[[maybe_unused]] static QString McDataCodeToString(McDataCode t) {
    return qenumToString(t);
};

/// Parses a McDataCode enum key name back into its value.
/// @return the matching McDataCode, or DataCode_User if `t` is not a valid key
[[maybe_unused]] static McDataCode McDataCodeFromString(QString t) {
    return stringToQEnum(t, McDataCode::DataCode_User);

};

}

Q_DECLARE_METATYPE(vc::device::mc::McFrameType)
Q_DECLARE_METATYPE(vc::device::mc::McMsgItfType)
Q_DECLARE_METATYPE(vc::device::mc::McDataCode)

#endif // MC_DEFINE_H
