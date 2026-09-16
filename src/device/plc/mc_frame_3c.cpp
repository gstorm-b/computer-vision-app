#include "mc_frame_3c.h"

#include "device/plc/mc_ascii_utils.h"
#include "device/plc/mc_define.h"

#include "core/logger/app_logger.h"

/// Frame3C implementation: builds and parses the ASCII Mitsubishi computer-link 3C-frame
/// requests/responses (QnA-compatible read/write, bit and word) described by Context_Mc3C.
namespace vc::device {

/// Shorthand for MCFrameAbstract's frame build/parse result codes, used throughout this file.
using RtCode = MCFrameAbstract::FrameReturnCode;

namespace {

/// Frame ID of the 3C frame, as the two ASCII characters that travel on the wire.
constexpr char kFrameId[] = "F9";
/// Characters in the frame ID field.
constexpr int kFrameIdChars = 2;
/// Characters in the access route: station(2) + network(2) + PC(2) + self-station(2).
constexpr int kRouteChars = 8;
/// Characters in a sum check.
constexpr int kSumChars = 2;
/// Characters in a NAK error code.
constexpr int kErrorChars = 4;
/// Characters in a device count field.
constexpr int kCountDigits = 4;
/// Characters in one word value.
constexpr int kWordDigits = 4;
/// Decimal digits in a device address for the Q and L series.
constexpr int kAddressDigitsQL = 6;
/// Decimal digits in a device address for the iQ-R series.
constexpr int kAddressDigitsIQR = 8;
/// Bit points carried by one word.
constexpr int kBitsPerWord = 16;

} // namespace

/// Constructs the frame codec with an empty last-error string.
Frame3C::Frame3C() {
    m_last_error = "";
}

/// True when `request` is a bit read that must travel as a word read.
bool Frame3C::readsBitsAsWords(const vc::device::MCRequest *request) {
    if (request->m_type != MCRequest::RqType::ReadBit) {
        return false;
    }
    if (request->m_amount <= 8) {
        return false;
    }
    return (request->m_device_type == 'X') ||
           (request->m_device_type == 'Y') ||
           (request->m_device_type == 'M');
}

/// Number of words a bit read of `amount` points occupies.
int Frame3C::bitWordCount(int amount) {
    return (amount / kBitsPerWord) + (((amount % kBitsPerWord) != 0) ? 1 : 0);
}

/// Appends the frame ID and the four access-route numbers to `out`.
void Frame3C::appendRouteHeader(Context_Mc3C *ctx, QByteArray &out) {
    out.append(kFrameId, kFrameIdChars);
    mc_ascii::appendHex(out, static_cast<quint32>(ctx->m_stationNumber), 2);
    mc_ascii::appendHex(out, static_cast<quint32>(ctx->m_networkNumber), 2);
    mc_ascii::appendHex(out, static_cast<quint32>(ctx->m_pcNumber), 2);
    mc_ascii::appendHex(out, static_cast<quint32>(ctx->m_selfStationNumber), 2);
}

/// Appends the device header for `device` ("M*" for Q/L, "M***" for iQ-R).
bool Frame3C::appendDeviceHeader(Context_Mc3C *ctx, char device, QByteArray &out) {
    switch (device) {
    case 'X':
    case 'Y':
    case 'M':
    case 'D':
        break;
    default:
        return false;
    }

    out.append(device);
    // The header is padded with '*' to a fixed width, which differs by series: the iQ-R device
    // name field is two characters wider than the Q/L one.
    out.append(ctx->m_plcSeries == mc::McPlcSeries::PlcSeries_iQR ? "***" : "*");
    return true;
}

/// Appends the device address with the width the PLC series requires.
void Frame3C::appendDeviceAddress(Context_Mc3C *ctx, int address, QByteArray &out) {
    mc_ascii::appendDec(out, address,
                        ctx->m_plcSeries == mc::McPlcSeries::PlcSeries_iQR ? kAddressDigitsIQR
                                                                           : kAddressDigitsQL);
}

/// Appends the command and the series-dependent sub-command.
void Frame3C::appendCommand(Context_Mc3C *ctx, bool write, bool bitUnits, QByteArray &out) {
    // 0401 = batch read, 1401 = batch write.
    out.append(write ? "1401" : "0401");

    if (ctx->m_plcSeries == mc::McPlcSeries::PlcSeries_iQR) {
        // iQ-R sub-commands: 0002 word units, 0003 bit units.
        out.append(bitUnits ? "0003" : "0002");
    } else {
        // Q/L sub-commands: 0000 word units, 0001 bit units.
        out.append(bitUnits ? "0001" : "0000");
    }
}

/// Wraps `requestData` in the 3C envelope and writes the complete frame to `out`.
///
/// The sum check covers everything after the control code — frame ID, access route and request
/// data — which is the region a receiver can re-add from the bytes it holds.
void Frame3C::buildEnvelope(Context_Mc3C *ctx, const QByteArray &requestData, QByteArray &out) {
    out.clear();
    out.append(static_cast<char>(MC_C_ENQ));

    const int sumFrom = out.size();
    appendRouteHeader(ctx, out);
    out.append(requestData);

    if (ctx->m_useSumCheck) {
        mc_ascii::appendSumCheck(out, sumFrom, out.size() - sumFrom);
    }

    // Format 4 is format 1 plus this terminator; nothing else differs between them.
    if (ctx->m_frameFormat == mc::McFrameFormat::Format_4) {
        out.append(static_cast<char>(MC_C_CR));
        out.append(static_cast<char>(MC_C_LF));
    }
}

/// Builds a 3C request frame for `request`.
RtCode Frame3C::makeSendFrame(vc::device::MCRequest *request, McContext *ctx, QByteArray &data) {
    if ((request == nullptr) || (ctx == nullptr)) {
        m_last_error = "null request or context";
        return RtCode::ObjectError;
    }

    if (ctx->frameType() != mc::McFrameType::Frame_3C) {
        m_last_error = "context is not a 3C context";
        return RtCode::ObjectError;
    }

    Context_Mc3C *ctx_3c = static_cast<Context_Mc3C *>(ctx);

    QByteArray requestData;
    const bool write = (request->m_type == MCRequest::RqType::WriteBit) ||
                       (request->m_type == MCRequest::RqType::WriteWord);
    const bool bitUnits = (request->m_type == MCRequest::RqType::ReadBit && !readsBitsAsWords(request)) ||
                          (request->m_type == MCRequest::RqType::WriteBit);

    appendCommand(ctx_3c, write, bitUnits, requestData);

    if (!appendDeviceHeader(ctx_3c, request->m_device_type, requestData)) {
        m_last_error = QStringLiteral("unsupported device type '%1'")
                           .arg(QChar(request->m_device_type));
        return RtCode::RequestFrameError;
    }
    appendDeviceAddress(ctx_3c, request->m_start_address, requestData);

    switch (request->m_type) {
    case MCRequest::RqType::ReadBit: {
        if (request->m_amount < 1) {
            m_last_error = "bit read amount below 1";
            return RtCode::RequestFrameError;
        }
        // Redirected to a word read for more than 8 points, exactly as Frame3E does, so the
        // polling loop's request shapes stay identical across frame types.
        const int count = readsBitsAsWords(request) ? bitWordCount(request->m_amount)
                                                    : request->m_amount;
        mc_ascii::appendHex(requestData, static_cast<quint32>(count), kCountDigits);
        break;
    }

    case MCRequest::RqType::ReadWord:
        if (request->m_amount < 1) {
            m_last_error = "word read amount below 1";
            return RtCode::RequestFrameError;
        }
        mc_ascii::appendHex(requestData, static_cast<quint32>(request->m_amount), kCountDigits);
        break;

    case MCRequest::RqType::WriteBit: {
        // The amount is the payload length, not whatever the caller left in m_amount.
        request->m_amount = request->m_value.size();
        if (request->m_amount < 1) {
            m_last_error = "write bit request carries no values";
            return RtCode::RequestFrameError;
        }
        mc_ascii::appendHex(requestData, static_cast<quint32>(request->m_amount), kCountDigits);
        for (const quint16 value : request->m_value) {
            requestData.append((value == 0) ? '0' : '1');
        }
        break;
    }

    case MCRequest::RqType::WriteWord: {
        request->m_amount = request->m_value.size();
        if (request->m_amount < 1) {
            m_last_error = "write word request carries no values";
            return RtCode::RequestFrameError;
        }
        mc_ascii::appendHex(requestData, static_cast<quint32>(request->m_amount), kCountDigits);
        for (const quint16 value : request->m_value) {
            mc_ascii::appendHex(requestData, value, kWordDigits);
        }
        break;
    }
    }

    buildEnvelope(ctx_3c, requestData, data);
    return RtCode::RequestFrameOK;
}

/// Locates one complete response frame in `data` and validates its framing.
RtCode Frame3C::locateFrame(Context_Mc3C *ctx, const QByteArray &data, quint8 &control,
                            QByteArray &payload, quint16 &errorCode) {
    control = 0;
    payload.clear();
    errorCode = 0;

    // Skip anything before the control code: a retried request can leave the tail of an
    // abandoned response in the buffer, and parsing that tail as a header is how a recovered
    // link starts reporting values that were never read.
    int start = -1;
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

    const int headerEnd = start + 1 + kFrameIdChars + kRouteChars;
    if (data.size() < headerEnd) {
        return RtCode::WaitingReceive;
    }

    if (data.mid(start + 1, kFrameIdChars) != QByteArray(kFrameId, kFrameIdChars)) {
        m_last_error = "response frame ID is not F9";
        return RtCode::ResponseInvalid;
    }

    // The access route must be the one we sent. A frame that came back with another station's
    // route is another station's answer, and taking its payload would write a different PLC's
    // register values into this device's map.
    QByteArray expectedRoute;
    mc_ascii::appendHex(expectedRoute, static_cast<quint32>(ctx->m_stationNumber), 2);
    mc_ascii::appendHex(expectedRoute, static_cast<quint32>(ctx->m_networkNumber), 2);
    mc_ascii::appendHex(expectedRoute, static_cast<quint32>(ctx->m_pcNumber), 2);
    mc_ascii::appendHex(expectedRoute, static_cast<quint32>(ctx->m_selfStationNumber), 2);

    if (data.mid(start + 1 + kFrameIdChars, kRouteChars) != expectedRoute) {
        m_last_error = QStringLiteral("response access route %1 does not match the request's %2")
                           .arg(QString::fromLatin1(data.mid(start + 1 + kFrameIdChars, kRouteChars)),
                                QString::fromLatin1(expectedRoute));
        return RtCode::ResponseInvalid;
    }

    const int sumFrom = start + 1;   // the sum covers everything after the control code
    int sumLen = 0;
    int afterBody = headerEnd;
    bool ok = false;

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
        errorCode = static_cast<quint16>(mc_ascii::parseHex(data, headerEnd, kErrorChars, &ok));
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
            // A bad sum means the bytes are corrupt, so the payload must not be believed.
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

/// Parses a 3C response frame for `request`, storing read values into the context's device map.
RtCode Frame3C::parseReceiveFrame(vc::device::MCRequest *request, McContext *ctx, QByteArray &data) {
    if ((request == nullptr) || (ctx == nullptr)) {
        m_last_error = "null request or context";
        return RtCode::ObjectError;
    }

    if (ctx->frameType() != mc::McFrameType::Frame_3C) {
        m_last_error = "context is not a 3C context";
        return RtCode::ObjectError;
    }

    Context_Mc3C *ctx_3c = static_cast<Context_Mc3C *>(ctx);

    quint8 control = 0;
    quint16 errorCode = 0;
    QByteArray payload;

    const RtCode located = locateFrame(ctx_3c, data, control, payload, errorCode);
    if (located != RtCode::ResponseOk) {
        return located;
    }

    if (control == MC_C_NAK) {
        m_last_error = QStringLiteral("PLC returned NAK, end code 0x%1")
                           .arg(errorCode, 4, 16, QLatin1Char('0'));
        return RtCode::ResponseError;
    }

    switch (request->m_type) {
    case MCRequest::RqType::WriteBit:
    case MCRequest::RqType::WriteWord:
        // A write is acknowledged either bare (ACK) or as an empty data frame, depending on the
        // PLC's setting. Both are success; only NAK, handled above, is not.
        return RtCode::ResponseOk;

    case MCRequest::RqType::ReadBit:
        if (control != MC_C_STX) {
            m_last_error = "read request answered without a data frame";
            return RtCode::ResponseInvalid;
        }
        return readsBitsAsWords(request) ? parseReadBitFromWord(request, ctx_3c, payload)
                                         : parseReadBit(request, ctx_3c, payload);

    case MCRequest::RqType::ReadWord:
        if (control != MC_C_STX) {
            m_last_error = "read request answered without a data frame";
            return RtCode::ResponseInvalid;
        }
        return parseReadWord(request, ctx_3c, payload);
    }

    m_last_error = "unsupported request type";
    return RtCode::ResponseInvalid;
}

/// Decodes an ASCII bit-read payload ('0'/'1' per device) into the context's M-device map.
RtCode Frame3C::parseReadBit(vc::device::MCRequest *request, Context_Mc3C *ctx,
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

    if (request->m_device_type != 'M') {
        return RtCode::ResponseOk;
    }

    for (int i = 0; i < request->m_amount; ++i) {
        dv_map->device_map_m[request->m_start_address + i] =
            (payload.at(i) == '0') ? 0 : 1;
    }
    return RtCode::ResponseOk;
}

/// Decodes a word-packed bit-read payload (4 hex characters per 16 bits) into the M-device map.
///
/// @note The Python reference walks the four hexadecimal characters of each word individually
///       and parses each as a DECIMAL digit, which cannot represent the values A-F. That is
///       read here as a defect in the reference rather than as the protocol: the field is
///       hexadecimal everywhere else in the same frame. Each group of four characters is parsed
///       as one 16-bit word and unpacked least-significant bit first, matching what
///       Frame3E::parse_read_bit_from_word does for the binary frame.
RtCode Frame3C::parseReadBitFromWord(vc::device::MCRequest *request, Context_Mc3C *ctx,
                                     const QByteArray &payload) {
    const int words = bitWordCount(request->m_amount);
    const int expected = words * kWordDigits;
    if (payload.size() < expected) {
        m_last_error = QStringLiteral("bit read returned %1 characters, expected %2")
                           .arg(payload.size()).arg(expected);
        return RtCode::ResponseInvalid;
    }

    McDeviceMap *dv_map = ctx->getDeviceMap();
    if (dv_map == nullptr) {
        m_last_error = "context has no device map";
        return RtCode::ResponseInvalid;
    }

    if (request->m_device_type != 'M') {
        return RtCode::ResponseOk;
    }

    int written = 0;
    for (int w = 0; w < words; ++w) {
        bool ok = false;
        const quint32 raw = mc_ascii::parseHex(payload, w * kWordDigits, kWordDigits, &ok);
        if (!ok) {
            m_last_error = "bit word is not hexadecimal";
            return RtCode::ResponseInvalid;
        }
        const quint16 word = static_cast<quint16>(raw);
        for (int bit = 0; bit < kBitsPerWord; ++bit) {
            if (written >= request->m_amount) {
                break;
            }
            dv_map->device_map_m[request->m_start_address + written] =
                static_cast<quint8>((word >> bit) & 0x01);
            ++written;
        }
    }
    return RtCode::ResponseOk;
}

/// Decodes an ASCII word-read payload (4 hex characters per word) into the context's D map.
///
/// @note The Python reference splits each word's four characters into two separate byte values
///       and appends both, which suits its own byte-oriented storage but is not what a register
///       map holds. One 16-bit value per device address is used here, matching the 1C reference
///       (`fx3communicator.cpp` parses `data.mid(0, 4)` as a single hexadecimal number) and the
///       protocol's own definition of the field.
RtCode Frame3C::parseReadWord(vc::device::MCRequest *request, Context_Mc3C *ctx,
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
QString Frame3C::lastErrorDescription() {
    return m_last_error;
}

}
