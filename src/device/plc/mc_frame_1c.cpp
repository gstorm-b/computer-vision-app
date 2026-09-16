#include "mc_frame_1c.h"

#include "device/plc/mc_ascii_utils.h"
#include "device/plc/mc_define.h"

#include "core/logger/app_logger.h"

/// Frame1C implementation: builds and parses the ASCII Mitsubishi computer-link 1C-frame
/// requests/responses (BR/BW/WR/WW) described by Context_Mc1C.
namespace vc::device {

/// Shorthand for MCFrameAbstract's frame build/parse result codes, used throughout this file.
using RtCode = MCFrameAbstract::FrameReturnCode;

namespace {

/// Number of ASCII characters in the station-number field.
constexpr int kStationChars = 2;
/// Number of ASCII characters in the PLC-number field.
constexpr int kPlcChars = 2;
/// Characters between the control code and the data field: station + PLC number.
constexpr int kHeaderChars = kStationChars + kPlcChars;
/// Number of ASCII characters in a sum check.
constexpr int kSumChars = 2;
/// Number of ASCII characters in a NAK error code.
constexpr int kErrorChars = 2;
/// Decimal digits in a device address, e.g. "M0100".
constexpr int kAddressDigits = 4;
/// Hexadecimal digits in a device count.
constexpr int kCountDigits = 2;
/// Hexadecimal digits in one word value.
constexpr int kWordDigits = 4;

} // namespace

/// Constructs the frame codec with an empty last-error string.
Frame1C::Frame1C() {
    m_last_error = "";
}

/// Maps a 1C NAK error code to a human-readable description.
QString Frame1C::nakErrorDescription(quint8 code) {
    switch (code) {
    case MC_C_NAK_NONE_ERR:        return QStringLiteral("no error");
    case MC_C_NAK_CHECKSUM:        return QStringLiteral("sum check error");
    case MC_C_NAK_PROTOCOL:        return QStringLiteral("protocol error");
    case MC_C_NAK_AREA_ERR:        return QStringLiteral("character area error");
    case MC_C_NAK_CHARACTER:       return QStringLiteral("character error");
    case MC_C_NAK_PLC_NUM:
    case MC_C_NAK_PLC_NUM_1:       return QStringLiteral("PLC number error");
    case MC_C_NAK_REMOETE_CONTROL: return QStringLiteral("remote control error");
    }
    return QStringLiteral("unknown error code");
}

/// Appends the two-character command code for `type`; false when the type has no 1C command.
bool Frame1C::appendCommand(MCRequest::RqType type, QByteArray &out) {
    switch (type) {
    case MCRequest::RqType::ReadBit:
        out.append(MC_C_BR_H).append(MC_C_BR_L);
        return true;
    case MCRequest::RqType::WriteBit:
        out.append(MC_C_BW_H).append(MC_C_BW_L);
        return true;
    case MCRequest::RqType::ReadWord:
        out.append(MC_C_WR_H).append(MC_C_WR_L);
        return true;
    case MCRequest::RqType::WriteWord:
        out.append(MC_C_WW_H).append(MC_C_WW_L);
        return true;
    }
    return false;
}

/// Appends the area field: device letter, 4-digit decimal address and 2-digit hex count.
void Frame1C::appendArea(QByteArray &out, char device, int address, int amount) {
    out.append(device);
    mc_ascii::appendDec(out, address, kAddressDigits);
    mc_ascii::appendHex(out, static_cast<quint32>(amount), kCountDigits);
}

/// Wraps `area` in the 1C envelope and writes the complete frame to `out`.
///
/// The sum check covers everything after the control code up to the end of the area — the
/// region the specification calls the message, i.e. exactly what a receiver can re-add.
void Frame1C::buildEnvelope(Context_Mc1C *ctx, const QByteArray &command, const QByteArray &area,
                            QByteArray &out) {
    out.clear();
    out.append(static_cast<char>(MC_C_ENQ));

    const int sumFrom = out.size();
    mc_ascii::appendHex(out, static_cast<quint32>(ctx->m_stationNumber), kStationChars);
    mc_ascii::appendHex(out, static_cast<quint32>(ctx->m_plcNumber), kPlcChars);
    out.append(command);
    mc_ascii::appendHex(out, static_cast<quint32>(ctx->m_messageWaitTime), 1);
    out.append(area);

    if (ctx->m_useSumCheck) {
        mc_ascii::appendSumCheck(out, sumFrom, out.size() - sumFrom);
    }

    // Format 4 is format 1 plus this terminator; nothing else differs between them.
    if (ctx->m_frameFormat == mc::McFrameFormat::Format_4) {
        out.append(static_cast<char>(MC_C_CR));
        out.append(static_cast<char>(MC_C_LF));
    }
}

/// Builds a 1C request frame for `request` (BR/WR/BW/WW by MCRequest::RqType).
RtCode Frame1C::makeSendFrame(vc::device::MCRequest *request, McContext *ctx, QByteArray &data) {
    if ((request == nullptr) || (ctx == nullptr)) {
        m_last_error = "null request or context";
        return RtCode::ObjectError;
    }

    if (ctx->frameType() != mc::McFrameType::Frame_1C) {
        m_last_error = "context is not a 1C context";
        return RtCode::ObjectError;
    }

    Context_Mc1C *ctx_1c = static_cast<Context_Mc1C *>(ctx);

    QByteArray command;
    if (!appendCommand(request->m_type, command)) {
        m_last_error = "unsupported request type";
        return RtCode::RequestFrameError;
    }

    QByteArray area;
    switch (request->m_type) {
    case MCRequest::RqType::ReadBit:
    case MCRequest::RqType::ReadWord:
        if ((request->m_amount < 1) || (request->m_amount > 255)) {
            m_last_error = "device count outside the 1 to 255 the count field can carry";
            return RtCode::RequestFrameError;
        }
        appendArea(area, request->m_device_type, request->m_start_address, request->m_amount);
        break;

    case MCRequest::RqType::WriteBit: {
        // The amount is the payload length, not whatever the caller left in m_amount: a write
        // request is defined by the values it carries.
        request->m_amount = request->m_value.size();
        if ((request->m_amount < 1) || (request->m_amount > 255)) {
            m_last_error = "write bit value count outside 1 to 255";
            return RtCode::RequestFrameError;
        }
        appendArea(area, request->m_device_type, request->m_start_address, request->m_amount);
        for (const quint16 value : request->m_value) {
            area.append((value == 0) ? '0' : '1');
        }
        break;
    }

    case MCRequest::RqType::WriteWord: {
        request->m_amount = request->m_value.size();
        if ((request->m_amount < 1) || (request->m_amount > 255)) {
            m_last_error = "write word value count outside 1 to 255";
            return RtCode::RequestFrameError;
        }
        appendArea(area, request->m_device_type, request->m_start_address, request->m_amount);
        for (const quint16 value : request->m_value) {
            mc_ascii::appendHex(area, value, kWordDigits);
        }
        break;
    }
    }

    buildEnvelope(ctx_1c, command, area, data);
    return RtCode::RequestFrameOK;
}

/// Locates one complete response frame in `data`.
RtCode Frame1C::locateFrame(Context_Mc1C *ctx, const QByteArray &data, quint8 &control,
                            int &start, QByteArray &payload, quint8 &errorCode) {
    control = 0;
    start = -1;
    payload.clear();
    errorCode = 0;

    // Skip anything before the control code. A retried request can leave the tail of an
    // abandoned response in the buffer, and treating that tail as a frame header is how a
    // recovered link starts reporting nonsense values instead of reconnecting.
    for (int i = 0; i < data.size(); ++i) {
        const quint8 byte = static_cast<quint8>(data.at(i));
        if ((byte == MC_C_STX) || (byte == MC_C_ACK) || (byte == MC_C_NAK)) {
            control = byte;
            start = i;
            break;
        }
    }

    if (start < 0) {
        m_last_error = "no control code in the received bytes yet";
        return RtCode::WaitingReceive;
    }

    const int headerEnd = start + 1 + kHeaderChars;
    if (data.size() < headerEnd) {
        return RtCode::WaitingReceive;
    }

    bool ok = false;
    const quint32 station = mc_ascii::parseHex(data, start + 1, kStationChars, &ok);
    if (!ok) {
        m_last_error = "station number field is not hexadecimal";
        return RtCode::ResponseInvalid;
    }
    const quint32 plc = mc_ascii::parseHex(data, start + 1 + kStationChars, kPlcChars, &ok);
    if (!ok) {
        m_last_error = "PLC number field is not hexadecimal";
        return RtCode::ResponseInvalid;
    }

    // A response carrying another station's numbers is not our answer. Accepting it would
    // write another PLC's register values into this device's map.
    if ((static_cast<int>(station) != ctx->m_stationNumber) ||
        (static_cast<int>(plc) != ctx->m_plcNumber)) {
        m_last_error = QStringLiteral("response addressed to station %1/PLC %2, expected %3/%4")
                           .arg(station).arg(plc)
                           .arg(ctx->m_stationNumber).arg(ctx->m_plcNumber);
        return RtCode::ResponseInvalid;
    }

    int sumFrom = start + 1;   // the sum covers everything after the control code
    int sumLen = 0;
    int afterBody = headerEnd;

    if (control == MC_C_STX) {
        int etx = -1;
        for (int i = headerEnd; i < data.size(); ++i) {
            if (static_cast<quint8>(data.at(i)) == MC_C_ETX) {
                etx = i;
                break;
            }
        }
        if (etx < 0) {
            return RtCode::WaitingReceive;
        }
        payload = data.mid(headerEnd, etx - headerEnd);
        sumLen = (etx + 1) - sumFrom;   // ETX is inside the summed region
        afterBody = etx + 1;
    } else if (control == MC_C_NAK) {
        if (data.size() < (headerEnd + kErrorChars)) {
            return RtCode::WaitingReceive;
        }
        errorCode = static_cast<quint8>(mc_ascii::parseHex(data, headerEnd, kErrorChars, &ok));
        if (!ok) {
            m_last_error = "NAK error code is not hexadecimal";
            return RtCode::ResponseInvalid;
        }
        sumLen = (headerEnd + kErrorChars) - sumFrom;
        afterBody = headerEnd + kErrorChars;
    } else {
        // ACK: header only.
        sumLen = headerEnd - sumFrom;
        afterBody = headerEnd;
    }

    if (ctx->m_useSumCheck) {
        if (data.size() < (afterBody + kSumChars)) {
            return RtCode::WaitingReceive;
        }
        const quint32 received = mc_ascii::parseHex(data, afterBody, kSumChars, &ok);
        if (!ok) {
            m_last_error = "sum check field is not hexadecimal";
            return RtCode::ResponseInvalid;
        }
        const quint8 expected = mc_ascii::sumCheck(data, sumFrom, sumLen);
        if (static_cast<quint8>(received) != expected) {
            // A bad sum means the bytes are corrupt, so the payload must not be believed —
            // silently accepting it writes garbage into the device map.
            m_last_error = QStringLiteral("sum check mismatch: received %1, computed %2")
                               .arg(received, 2, 16, QLatin1Char('0'))
                               .arg(expected, 2, 16, QLatin1Char('0'));
            return RtCode::ResponseInvalid;
        }
        afterBody += kSumChars;
    }

    if (ctx->m_frameFormat == mc::McFrameFormat::Format_4) {
        if (data.size() < (afterBody + 2)) {
            return RtCode::WaitingReceive;
        }
        if ((static_cast<quint8>(data.at(afterBody)) != MC_C_CR) ||
            (static_cast<quint8>(data.at(afterBody + 1)) != MC_C_LF)) {
            m_last_error = "format 4 frame does not end with CR LF";
            return RtCode::ResponseInvalid;
        }
    }

    return RtCode::ResponseOk;
}

/// Parses a 1C response frame for `request`, storing read values into the context's device map.
RtCode Frame1C::parseReceiveFrame(vc::device::MCRequest *request, McContext *ctx, QByteArray &data) {
    if ((request == nullptr) || (ctx == nullptr)) {
        m_last_error = "null request or context";
        return RtCode::ObjectError;
    }

    if (ctx->frameType() != mc::McFrameType::Frame_1C) {
        m_last_error = "context is not a 1C context";
        return RtCode::ObjectError;
    }

    Context_Mc1C *ctx_1c = static_cast<Context_Mc1C *>(ctx);

    quint8 control = 0;
    quint8 errorCode = 0;
    int start = -1;
    QByteArray payload;

    const RtCode located = locateFrame(ctx_1c, data, control, start, payload, errorCode);
    if (located != RtCode::ResponseOk) {
        return located;
    }

    if (control == MC_C_NAK) {
        m_last_error = QStringLiteral("PLC returned NAK: %1 (0x%2)")
                           .arg(nakErrorDescription(errorCode))
                           .arg(errorCode, 2, 16, QLatin1Char('0'));
        return RtCode::ResponseError;
    }

    switch (request->m_type) {
    case MCRequest::RqType::WriteBit:
    case MCRequest::RqType::WriteWord:
        // A write is answered with ACK and no data. An STX frame here means the PLC answered
        // a different request than the one in flight.
        if (control != MC_C_ACK) {
            m_last_error = "write request answered with a data frame";
            return RtCode::ResponseInvalid;
        }
        return RtCode::ResponseOk;

    case MCRequest::RqType::ReadBit:
        if (control != MC_C_STX) {
            m_last_error = "read request answered without a data frame";
            return RtCode::ResponseInvalid;
        }
        return parseReadBit(request, ctx_1c, payload);

    case MCRequest::RqType::ReadWord:
        if (control != MC_C_STX) {
            m_last_error = "read request answered without a data frame";
            return RtCode::ResponseInvalid;
        }
        return parseReadWord(request, ctx_1c, payload);
    }

    m_last_error = "unsupported request type";
    return RtCode::ResponseInvalid;
}

/// Decodes an ASCII bit-read payload ('0'/'1' per device) into the context's M-device map.
RtCode Frame1C::parseReadBit(vc::device::MCRequest *request, Context_Mc1C *ctx,
                             const QByteArray &payload) {
    if (payload.size() < request->m_amount) {
        m_last_error = QStringLiteral("bit read returned %1 values, expected %2")
                           .arg(payload.size()).arg(request->m_amount);
        return RtCode::ResponseInvalid;
    }

    McDeviceMap *dv_map = ctx->getDeviceMap();
    if (dv_map == nullptr) {
        m_last_error = "context has no device map";
        return RtCode::ResponseInvalid;
    }

    // Only M values are tracked, matching Frame3E: the polled map holds M (bit) and D (word)
    // devices, and an X/Y read is used for its acknowledgement only.
    if (request->m_device_type != 'M') {
        return RtCode::ResponseOk;
    }

    for (int i = 0; i < request->m_amount; ++i) {
        dv_map->device_map_m[request->m_start_address + i] =
            (payload.at(i) == '0') ? 0 : 1;
    }
    return RtCode::ResponseOk;
}

/// Decodes an ASCII word-read payload (4 hex characters per word) into the context's D map.
RtCode Frame1C::parseReadWord(vc::device::MCRequest *request, Context_Mc1C *ctx,
                              const QByteArray &payload) {
    const int expected = request->m_amount * kWordDigits;
    if (payload.size() < expected) {
        m_last_error = QStringLiteral("word read returned %1 characters, expected %2")
                           .arg(payload.size()).arg(expected);
        return RtCode::ResponseInvalid;
    }

    McDeviceMap *dv_map = ctx->getDeviceMap();
    if (dv_map == nullptr) {
        m_last_error = "context has no device map";
        return RtCode::ResponseInvalid;
    }

    if (request->m_device_type != 'D') {
        return RtCode::ResponseOk;
    }

    for (int i = 0; i < request->m_amount; ++i) {
        bool ok = false;
        const quint32 raw = mc_ascii::parseHex(payload, i * kWordDigits, kWordDigits, &ok);
        if (!ok) {
            m_last_error = "word value is not hexadecimal";
            return RtCode::ResponseInvalid;
        }
        // Through quint16 first: the register is 16 bits and the map stores it signed, so
        // 0xFFFF must land as -1 rather than as 65535.
        dv_map->device_map_d[request->m_start_address + i] =
            static_cast<qint16>(static_cast<quint16>(raw));
    }
    return RtCode::ResponseOk;
}

/// Returns the description of the most recent build/parse error recorded in m_last_error.
QString Frame1C::lastErrorDescription() {
    return m_last_error;
}

}
