#ifndef MC_DEFINE_H
#define MC_DEFINE_H

/**
 * @file mc_define.h
 * @brief Mitsubishi MC-protocol enum/constant definitions, IEEE-754 helpers, and
 *        string conversion helpers for the McFrameType / McMsgItfType / McDataCode enums.
 */

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

namespace vc::device::mc {
Q_NAMESPACE

/**
 * @enum McFrameType
 * @brief MC protocol frame variants (1E/3E ASCII+binary, 1C/3C) supported by the factory.
 */
enum McFrameType {
    Frame_User,
    Frame_1E,
    Frame_3E,
    Frame_1C,
    Frame_3C
};
Q_ENUM_NS(McFrameType)

/**
 * @enum McMsgItfType
 * @brief Transport/message-interface types used to reach the PLC.
 */
enum McMsgItfType {
    MsgItf_User,
    EthernetTCPIP,
    EthernetUDP,
    SerialPort
};
Q_ENUM_NS(McMsgItfType)

/**
 * @enum McDataCode
 * @brief Data encoding used on the wire for a frame (binary vs. ASCII).
 */
enum McDataCode {
    DataCode_User,
    Binary,
    Ascii
};
Q_ENUM_NS(McDataCode)

/**
 * @enum McFrameFormat
 * @brief Message format of a computer-link (1C/3C) frame.
 *
 * Only the two formats the reference implementations use are declared. Format 4 is format 1
 * plus a CR+LF terminator, which is the only difference between them; formats 2, 3 and 5 change
 * the control-code and handshake structure and no reference exists for them here. Declaring one
 * would put an option in front of an operator that the codec has to reject at connect time.
 */
enum McFrameFormat {
    Format_1 = 1,
    Format_4 = 4
};
Q_ENUM_NS(McFrameFormat)

/**
 * @enum McPlcSeries
 * @brief PLC series a 3C frame addresses. Decides the device-code width (Q/L use a 2-byte
 *        device header, iQ-R a 4-byte one) and the sub-command value.
 *
 * The A series is deliberately absent: the 3C reference implementation rejects it
 * (`mc_frame_3c.py` accepts only Q, L and iQ-R), so offering it would be an option that cannot
 * work.
 */
enum McPlcSeries {
    PlcSeries_Q,
    PlcSeries_L,
    PlcSeries_iQR
};
Q_ENUM_NS(McPlcSeries)

/// Serial line settings for the computer-link (1C/3C) transport.
///
/// Declared here rather than reusing QSerialPort's own enums so that no header outside the
/// serial transport has to pull in QtSerialPort, and so the translated key labels land in the
/// `vc::device::mc` context alongside every other MC enum. McMsgSerialPort converts them to
/// the QSerialPort values at the one point that talks to the port.

/**
 * @enum McSerialDataBits
 * @brief Character length on the serial line. MC computer-link normally runs 7 data bits.
 */
enum McSerialDataBits {
    DataBits_5 = 5,
    DataBits_6 = 6,
    DataBits_7 = 7,
    DataBits_8 = 8
};
Q_ENUM_NS(McSerialDataBits)

/**
 * @enum McSerialParity
 * @brief Parity scheme on the serial line.
 */
enum McSerialParity {
    Parity_None,
    Parity_Even,
    Parity_Odd,
    Parity_Space,
    Parity_Mark
};
Q_ENUM_NS(McSerialParity)

/**
 * @enum McSerialStopBits
 * @brief Number of stop bits on the serial line.
 */
enum McSerialStopBits {
    StopBits_One,
    StopBits_OneAndHalf,
    StopBits_Two
};
Q_ENUM_NS(McSerialStopBits)

/**
 * @enum McSerialFlowControl
 * @brief Flow-control scheme on the serial line.
 */
enum McSerialFlowControl {
    FlowControl_None,
    FlowControl_Hardware,
    FlowControl_Software
};
Q_ENUM_NS(McSerialFlowControl)

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
    QT_TR_NOOP("Ascii"),

    // McFrameFormat
    QT_TR_NOOP("Format_1"),
    QT_TR_NOOP("Format_4"),

    // McPlcSeries
    QT_TR_NOOP("PlcSeries_Q"),
    QT_TR_NOOP("PlcSeries_L"),
    QT_TR_NOOP("PlcSeries_iQR"),

    // McSerialDataBits
    QT_TR_NOOP("DataBits_5"),
    QT_TR_NOOP("DataBits_6"),
    QT_TR_NOOP("DataBits_7"),
    QT_TR_NOOP("DataBits_8"),

    // McSerialParity
    QT_TR_NOOP("Parity_None"),
    QT_TR_NOOP("Parity_Even"),
    QT_TR_NOOP("Parity_Odd"),
    QT_TR_NOOP("Parity_Space"),
    QT_TR_NOOP("Parity_Mark"),

    // McSerialStopBits
    QT_TR_NOOP("StopBits_One"),
    QT_TR_NOOP("StopBits_OneAndHalf"),
    QT_TR_NOOP("StopBits_Two"),

    // McSerialFlowControl
    QT_TR_NOOP("FlowControl_None"),
    QT_TR_NOOP("FlowControl_Hardware"),
    QT_TR_NOOP("FlowControl_Software")
};

/**
 * @brief Looks up the binary/Ethernet-frame (3E) device code for an ASCII device
 *        type letter ('X', 'Y', 'M', or 'D').
 * @param[in] device_type ASCII device-type letter
 * @param[out] code set to the matching MC_E_DEIVCE_* constant on success
 * @return true if `device_type` was recognized, false otherwise (code left unset)
 */
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
Q_DECLARE_METATYPE(vc::device::mc::McFrameFormat)
Q_DECLARE_METATYPE(vc::device::mc::McPlcSeries)
Q_DECLARE_METATYPE(vc::device::mc::McSerialDataBits)
Q_DECLARE_METATYPE(vc::device::mc::McSerialParity)
Q_DECLARE_METATYPE(vc::device::mc::McSerialStopBits)
Q_DECLARE_METATYPE(vc::device::mc::McSerialFlowControl)

#endif // MC_DEFINE_H
