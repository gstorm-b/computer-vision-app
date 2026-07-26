#include "mc_fame_3e.h"
#include "device/plc/mc_request.h"
#include "device/plc/memory_utils.h"
#include "device/plc/mc_define.h"

#include "core/logger/app_logger.h"

/// Frame3E implementation: builds and parses the binary Mitsubishi MC 3E-frame requests/responses
/// (bit and word read/write) described by Context_Mc3E, plus the internal frame-encoding helpers
/// (command header, device addressing, and send-frame envelope) used to assemble them.
namespace vc::device {

/// Shorthand for MCFrameAbstract's frame build/parse result codes, used throughout this file.
using RtCode = MCFrameAbstract::FrameReturnCode;

/// Constructs the frame codec with an empty last-error string.
Frame3E::Frame3E() {
    m_last_error = "";
}

/// Builds a 3E request frame for `request` (dispatches by MCRequest::RqType to the
/// matching build_* helper). For ReadBit requests of more than 8 bits on an X/Y/M device,
/// transparently switches to a word-aligned read via build_read_word and records the
/// equivalent word length in m_total_bit_word_len.
RtCode Frame3E::makeSendFrame(vc::device::MCRequest *request, McContext* ctx, QByteArray &data) {
    if ((request == nullptr) || (ctx == nullptr)) {
        return RtCode::ObjectError;
    }


    // MCRequest *mc_req = static_cast<MCRequest*>(request);
    MCRequest *mc_req = request;
    Context_Mc3E *ctx_3e = static_cast<Context_Mc3E*>(ctx);

    switch (mc_req->m_type) {
    case MCRequest::RqType::ReadBit:
        if (mc_req->m_amount > 8) {
            if ((mc_req->m_device_type == 'X') ||
                (mc_req->m_device_type == 'Y') ||
                (mc_req->m_device_type == 'M')) {

                int read_size = (mc_req->m_amount / 16);
                if ((mc_req->m_amount % 16) != 0) {
                    read_size += 1;
                }
                m_total_bit_word_len = read_size;
                return build_read_word(mc_req->m_device_type, mc_req->m_start_address, read_size, ctx_3e, data);
            }
        }

        return build_read_bit(mc_req->m_device_type, mc_req->m_start_address, mc_req->m_amount, ctx_3e, data);

    case MCRequest::RqType::WriteBit:
        mc_req->m_amount = mc_req->m_value.size();
        return build_write_bit(mc_req, ctx_3e, data);

        break;
    case MCRequest::RqType::ReadWord:
        return build_read_word(mc_req->m_device_type, mc_req->m_start_address, mc_req->m_amount, ctx_3e, data);

    case MCRequest::RqType::WriteWord:
        mc_req->m_amount = mc_req->m_value.size();
        return build_write_word(mc_req, ctx_3e, data);

    }

    return RtCode::RequestFrameError;
}

/// Parses a 3E response frame for `request` (dispatches by MCRequest::RqType to the matching
/// parse_* helper; ReadBit further routes to parse_read_bit_from_word when more than 8 bits of
/// an X/Y/M device were requested, mirroring the word-aligned build in makeSendFrame).
RtCode Frame3E::parseReceiveFrame(vc::device::MCRequest *request, McContext* ctx, QByteArray &data) {
    if ((request == nullptr) || (ctx == nullptr)) {
        return RtCode::ObjectError;
    }

    // MCRequest *mc_req = static_cast<MCRequest*>(request);
    MCRequest *mc_req = request;
    Context_Mc3E *ctx_3e = static_cast<Context_Mc3E*>(ctx);

    if (data.size() < 9) {
        m_last_error = "total receive len under 9 bytes";
        return RtCode::WaitingReceive;
    }

    switch (mc_req->m_type) {
    case MCRequest::RqType::ReadBit:
        if (mc_req->m_amount > 8) {
            if ((mc_req->m_device_type == 'X') ||
                (mc_req->m_device_type == 'Y') ||
                (mc_req->m_device_type == 'M')) {
                return parse_read_bit_from_word(mc_req, ctx_3e, data);
            }
        }

        return parse_read_bit(mc_req, ctx_3e, data);

    case MCRequest::RqType::WriteBit:
        return parse_write(mc_req, ctx_3e, data);

    case MCRequest::RqType::ReadWord:
        return parse_read_word(mc_req, ctx_3e, data);

    case MCRequest::RqType::WriteWord:
        return parse_write(mc_req, ctx_3e, data);
    }

    return RtCode::WaitingReceive;
}

/// Returns the description of the most recent build/parse error recorded in m_last_error.
QString Frame3E::lastErrorDescription() {
    return m_last_error;
}


/// Appends the little-endian command and sub-command code pair to `data` (4 bytes total).
/// @param cmd the MC command code (e.g. 0x0401 read, 0x1401 write)
/// @param sub_cmd the MC sub-command code (bit vs word variant)
/// @param data output buffer; the 4 encoded bytes are appended to it
static void make_command_data(quint16 cmd, quint16 sub_cmd, QByteArray &data) {
    // ALL CODE IS LITTLE ENDIAN

    // add command code to frame
    data.append(static_cast<char>(cmd & 0xFF));         // LSB
    data.append(static_cast<char>((cmd >> 8) & 0xFF));  // MSB

    // add sub command code to frame
    data.append(static_cast<char>(sub_cmd & 0xFF));         // LSB
    data.append(static_cast<char>((sub_cmd >> 8) & 0xFF));  // MSB
}

/// Appends the device address (3 bytes, little-endian) and device code (1 byte) to `data`,
/// per the 3E frame's binary device-addressing format for IQ-L series CPUs.
/// @param device_type the device letter (e.g. 'X', 'Y', 'M', 'D'); converted to its binary
///        device code via Eframe_Binary_Device_Code
/// @param address the starting device address
/// @param data output buffer; the 4 encoded bytes are appended to it
static void make_device_data(char &device_type, int &address, QByteArray &data) {
    // WITH IQ-L CPU - SUBCOMMAND CODE IS 0x0001
    // DEVICE NUM + DEVICE CODE = 3 + 1

    // BASE ON SUBCOMMAND DEVICE CODE HAVE DIFFRENT ARRANGE METHOD
    quint32 code = 0x00;
    Eframe_Binary_Device_Code(device_type, code);
    appendToByteArray_uint32(data, (quint32)address, 3, true);
    appendToByteArray_uint32(data, code, 1, true);
}

/// Reads the 3E end-code at the fixed status offset and records it via m_last_error;
/// true when the end code is 0x0000 (success).
bool Frame3E::checkErrorStatus(QByteArray &data) {
    const int resp_status_index = 9;
    quint16 end_code = convert_uint16_FromBytes(data, resp_status_index, true);

    if (end_code == 0x0000) {
        m_last_error = "Status OK";
        return true;
    }

    m_last_error = QString("Reponse error, error code: 0x%1, frame: %2")
                       .arg(end_code, 4, 16, QChar('0'))
                       .arg(QString::fromLatin1(data.toHex(' ')));
    return false;
}

/// Wraps `data` (an already-built request payload) in the full 3E send envelope — sub-header,
/// network, PC, destination module IO number, station number, request length, and monitoring
/// time, all read from `ctx` — then replaces `data` with the resulting complete frame.
void Frame3E::make_send_data(Context_Mc3E* ctx, QByteArray &data) {
    QByteArray send_frame;
    send_frame.clear();
    // sub header -> big endian - 2 bytes
    appendToByteArray_uint16(send_frame, ctx->m_subHeader, false);
    // network - 1 byte
    send_frame.append(ctx->m_network);
    // pc - ff - 1 byte
    send_frame.append(ctx->m_pc);
    // module io number - little endian - 2 byte
    appendToByteArray_uint16(send_frame, ctx->m_destModuleIo, true);
    // module station number - 1 byte
    send_frame.append(ctx->m_multiStation);

    // add request size
    // (monitoring time + request data) length - little endian - 2 byte
    // m_current_data_len = 2 + data.length();
    appendToByteArray_uint16(send_frame, 2 + data.length(), true);
    // add monitoring time
    appendToByteArray_uint16(send_frame, ctx->m_monitoringTime, true);

    // append request data
    send_frame.append(data);
    data.clear();
    data.append(send_frame);
}

/// Builds a 3E bit-read request frame (command 0x0401/0x0001) for `amount` bits of `device`
/// starting at `start`.
RtCode Frame3E::build_read_bit(char &device, int &start, int &amount, Context_Mc3E *ctx, QByteArray &data) {

    // temporary test for IQL series
    // clear data
    data.clear();

    // Request data build
    // command and sub command
    make_command_data(0x0401, 0x0001, data);
    // device data [device number + device_code]
    make_device_data(device, start, data);
    // add amount of device
    appendToByteArray_uint16(data, (quint16)amount, true);

    // make send data include data length
    make_send_data(ctx, data);

    return RtCode::RequestFrameOK;
}

/// Builds a 3E bit-write request frame (command 0x1401/0x0001) for `request`, packing each
/// pair of values from request->m_value into a single byte (the first value in bit 4, the
/// second in bit 0; a trailing odd value gets its own byte with bit 0 unset).
RtCode Frame3E::build_write_bit(vc::device::MCRequest *request, Context_Mc3E *ctx, QByteArray &data) {
    char &device = request->m_device_type;
    int &start = request->m_start_address;
    int &amount = request->m_amount;

    // temporary test for IQL series
    // clear data
    data.clear();

    // Request data build
    // command and sub command
    make_command_data(0x1401, 0x0001, data);
    // device data [device number + device_code]
    make_device_data(device, start, data);
    // add amount of device
    // add amount of device
    appendToByteArray_uint16(data, (quint16)amount, true);

    for (int index=0;index<amount;index+=2) {
        quint8 write_byte = (0x01 & request->m_value.at(index)) << 4;

        if ((index + 1) >= amount) {
            data.append(write_byte);
            break;
        }
        write_byte = write_byte | (0x01 & request->m_value.at(index+1));
        data.append(write_byte);
    }

    // make send data include data length
    make_send_data(ctx, data);

    return RtCode::RequestFrameOK;
}

/// Builds a 3E word-read request frame (command 0x0401/0x0000) for `amount` words of `device`
/// starting at `start`.
RtCode Frame3E::build_read_word(char &device, int &start, int &amount, Context_Mc3E *ctx, QByteArray &data) {
    // temporary test for IQL series
    // clear data
    data.clear();

    // Request data build
    // command and sub command
    make_command_data(0x0401, 0x0000, data);
    // device data [device number + device_code]
    make_device_data(device, start, data);
    // add amount of device
    appendToByteArray_uint16(data, (quint16)amount, true);

    // make send data include data length
    make_send_data(ctx, data);

    return RtCode::RequestFrameOK;
}

/// Builds a 3E word-write request frame (command 0x1401/0x0000) for `request`, appending each
/// value in request->m_value as a little-endian 16-bit word.
RtCode Frame3E::build_write_word(vc::device::MCRequest *request, Context_Mc3E *ctx, QByteArray &data) {
    char &device = request->m_device_type;
    int &start = request->m_start_address;
    int &amount = request->m_amount;

    // temporary test for IQL series
    // clear data
    data.clear();

    // Request data build
    // command and sub command
    make_command_data(0x1401, 0x0000, data);
    // device data [device number + device_code]
    make_device_data(device, start, data);
    // add amount of device
    // add amount of device
    appendToByteArray_uint16(data, (quint16)amount, true);

    for (int index=0;index<amount;index+=1) {
        data.append(static_cast<char>(request->m_value.at(index) & 0xFF));         // LSB
        data.append(static_cast<char>((request->m_value.at(index) >> 8) & 0xFF));  // MSB
    }

    // make send data include data length
    make_send_data(ctx, data);

    return RtCode::RequestFrameOK;
}

/// Parses a 3E bit-read response for reads of 8 or fewer bits per device: after validating the
/// end code, unpacks each response byte into up to two bit values (the 0x10 nibble bit, then
/// the 0x01 bit) until request->m_amount values are collected, then — only for device_type
/// 'M' — stores them into ctx's device map starting at request->m_start_address.
RtCode Frame3E::parse_read_bit(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data) {
    if (data.size() < 9) {
        m_last_error = "total receive len under 9 bytes";
        return RtCode::ResponseInvalid;
    }

    if (!checkErrorStatus(data)) {
        return RtCode::ResponseError;
    }

    const int resp_data_index = 11;
    quint16 data_len = convert_uint16_FromBytes(data, 7, true) - 2;

    if ((data.size() - 11) < data_len) {
        LOG_USER_ERR << "parse read bit waiting for receive full frame" << data_len << data.size();
        return RtCode::WaitingReceive;
    }

    QList<quint8> value;
    int device_count = 0;
    for (int index=0;index<data_len;index++) {
        quint8 value_byte = data.at(index + resp_data_index);
        value.append( ( (value_byte & 0x10) >> 4 ) );   // 0x10 = 0001 0000
        device_count += 2;
        if (device_count >= request->m_amount) {
            break;
        }
        value.append( (value_byte & 0x01) );            // 0x01 = 0000 0001
    }

    McDeviceMap *dv_map = ctx->getDeviceMap();
    if (request->m_device_type == 'M') {
        for (int idx = 0; idx < value.size(); idx++) {
            dv_map->device_map_m[request->m_start_address + idx] = value.at(idx);
            // qInfo() << "Value at" << QString::number(request->m_start_address + idx)
            //         << "is " << value.at(idx);
        }
    }

    return RtCode::ResponseOk;
}

/// Parses a 3E bit-read response for reads of more than 8 bits from an X/Y/M device (the
/// request was sent word-aligned via build_read_word): after validating the end code, unpacks
/// each response byte into 8 bit values (LSB first), trims the result to request->m_amount,
/// then — only for device_type 'M' — stores them into ctx's device map starting at
/// request->m_start_address.
RtCode Frame3E::parse_read_bit_from_word(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data) {
    if (data.size() < 9) {
        m_last_error = "total receive len under 9 bytes";
        return RtCode::ResponseInvalid;
    }

    if (!checkErrorStatus(data)) {
        return RtCode::ResponseError;
    }

    const int resp_data_index = 11;
    // minus for 2 byte of end code at index 9 + 10
    quint16 data_len = convert_uint16_FromBytes(data, 7, true) - 2;

    if ((data.size() - 11) < data_len) {
        LOG_USER_ERR << "parse read bit from word waiting for receive full frame" << data_len << data.size();
        return RtCode::WaitingReceive;
    }

    QList<quint8> value;
    int device_count = 0;
    for (int index=0;index<data_len;index++) {
        quint8 value_byte = data.at(index + resp_data_index);
        for (int i = 0; i < 8; ++i) {
            value.append((value_byte >> i) & 0x01);
        }
        device_count += 8;
    }

    if (device_count >= request->m_amount) {
        value.erase(value.begin() + request->m_amount, value.end());
    }

    McDeviceMap *dv_map = ctx->getDeviceMap();
    if (request->m_device_type == 'M') {
        for (int idx = 0; idx < value.size(); idx++) {
            dv_map->device_map_m[request->m_start_address + idx] = value.at(idx);
            // qInfo() << "Value at" << QString::number(request->m_start_address + idx)
            //         << "is " << value.at(idx);
        }
    }

    return RtCode::ResponseOk;
}


/// Parses a 3E word-read response: after validating the end code, decodes each little-endian
/// 16-bit word from the response payload, trims the result to request->m_amount, then — only
/// for device_type 'D' — stores them into ctx's device map starting at request->m_start_address.
RtCode Frame3E::parse_read_word(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data) {
    if (data.size() < 9) {
        m_last_error = "total receive len under 9 bytes";
        return RtCode::ResponseInvalid;
    }

    if (!checkErrorStatus(data)) {
        return RtCode::ResponseError;
    }

    const int resp_data_index = 11;
    // minus for 2 byte of end code at index 9 + 10
    quint16 data_len = convert_uint16_FromBytes(data, 7, true) - 2;

    if ((data.size() - 11) < data_len) {
        LOG_USER_ERR << "parse read word waiting for receive full frame" << data_len << data.size();
        return RtCode::WaitingReceive;
    }

    QList<quint16> value;
    for (int index=0;index<data_len;index+=2) {
        quint8 byte1 = data[resp_data_index + index];
        quint8 byte2 = data[resp_data_index + index + 1];
        value.append(byte1 | (byte2 << 8));
    }

    if (value.size() > request->m_amount) {
        value.erase(value.begin() + request->m_amount, value.end());
    }

    McDeviceMap *dv_map = ctx->getDeviceMap();
    if (request->m_device_type == 'D') {
        for (int idx = 0; idx < value.size(); idx++) {
            // dv_map->device_map_d[request->m_start_address + idx] = static_cast<qint16>(value.at(idx));
            dv_map->device_map_d[request->m_start_address + idx] = value.at(idx);
            // qInfo() << "Value at" << QString::number(request->m_start_address + idx)
            //         << "is " << static_cast<qint16>(value.at(idx));
        }
    }

    return RtCode::ResponseOk;
}

/// Parses a response to any write request (bit or word) — both share the same fixed-length
/// envelope, so only the end-code check below is needed; `request` and `ctx` are unused,
/// kept only for a uniform parse_* signature.
RtCode Frame3E::parse_write(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data) {
    // A complete 3E write response is 11 bytes: 9-byte header + the 2-byte end
    // code that checkErrorStatus() reads at offset 9. Accepting a 9-10 byte
    // fragment would silently read as end_code 0x0000 (success) past the
    // available bytes, desyncing the request/response pairing.
    if (data.size() < 11) {
        m_last_error = "total receive len under 11 bytes";
        return RtCode::ResponseInvalid;
    }

    if (!checkErrorStatus(data)) {
        return RtCode::ResponseError;
    }

    return RtCode::ResponseOk;
}

}
