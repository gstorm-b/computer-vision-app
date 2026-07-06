#include "mc_fame_3e.h"
#include "device/plc/mc_request.h"
#include "device/plc/memory_utils.h"
#include "device/plc/mc_define.h"

#include "core/logger/app_logger.h"

namespace vc::device {

using RtCode = MCFrameAbstract::FrameReturnCode;

Frame3E::Frame3E() {
    m_last_error = "";
}

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

QString Frame3E::lastErrorDescription() {
    return m_last_error;
}


static void make_command_data(quint16 cmd, quint16 sub_cmd, QByteArray &data) {
    // ALL CODE IS LITTLE ENDIAN

    // add command code to frame
    data.append(static_cast<char>(cmd & 0xFF));         // LSB
    data.append(static_cast<char>((cmd >> 8) & 0xFF));  // MSB

    // add sub command code to frame
    data.append(static_cast<char>(sub_cmd & 0xFF));         // LSB
    data.append(static_cast<char>((sub_cmd >> 8) & 0xFF));  // MSB
}

static void make_device_data(char &device_type, int &address, QByteArray &data) {
    // WITH IQ-L CPU - SUBCOMMAND CODE IS 0x0001
    // DEVICE NUM + DEVICE CODE = 3 + 1

    // BASE ON SUBCOMMAND DEVICE CODE HAVE DIFFRENT ARRANGE METHOD
    quint32 code = 0x00;
    Eframe_Binary_Device_Code(device_type, code);
    appendToByteArray_uint32(data, (quint32)address, 3, true);
    appendToByteArray_uint32(data, code, 1, true);
}

bool Frame3E::checkErrorStatus(QByteArray &data) {
    const int resp_status_index = 9;
    quint16 end_code = convert_uint16_FromBytes(data, resp_status_index, true);

    if (end_code == 0x0000) {
        m_last_error = "Status OK";
        return true;
    }

    m_last_error = QString("Reponse error, error code: 0x%1, frame: %2")
                       .arg(end_code, 4, 16, QChar('0'))
                       .arg(data.toHex(' '));
    return false;
}

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

RtCode Frame3E::parse_write(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data) {
    if (data.size() < 9) {
        m_last_error = "total receive len under 9 bytes";
        return RtCode::ResponseInvalid;
    }

    if (!checkErrorStatus(data)) {
        return RtCode::ResponseError;
    }

    return RtCode::ResponseOk;
}

}
