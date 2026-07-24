#include "hy_aligntoz.h"

namespace rb {

HyAlignToZ::HyAlignToZ(QString &tcp_name, QString &ucs_name) :
    HuayanCommand(), m_tcp_name(tcp_name), m_ucs_name(ucs_name) {

}

void HyAlignToZ::execute() {
    if (this->m_interface == nullptr) {
        qCritical() << "HyAlignToZ m_interface pointer null.";
        return;
    }


    _start_point:
    switch (m_cmd_state) {
        case AlignToZState::UnExecute:
            // set command state to executing
            m_execute_state = ExecuteState::Executing;
            m_cmd_state = AlignToZState::SendAlignCommand;
            this->m_interface->flag.set_flag_start_jog();
            goto _start_point;
            break;

        case AlignToZState::SendAlignCommand:
            send_align_to_z_command();
            break;

        case AlignToZState::WaitAlignCommandResponse:
            wait_align_to_z_command_response();
            break;

        case AlignToZState::SendLongMoveEventCommand:
            send_long_move_command();
            break;

        case AlignToZState::WaitLongMoveEventCommandResponse:
            wait_long_move_comamnd_response();
            break;

        case AlignToZState::WaitCycle:
            if (m_interface->flag.has_jog_cmd_stop_raised() ||
                (!m_interface->flag.is_robot_moving())) {

                m_cmd_state = AlignToZState::Finish;
                goto _start_point;
                break;
            }

            if (m_time_count.StartTimeCounter(120)) {
                m_cmd_state = AlignToZState::SendLongMoveEventCommand;
            }
            break;

        case AlignToZState::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case AlignToZState::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
    }
}

inline void HyAlignToZ::send_align_to_z_command() {
    m_command_header = "MoveAlignToZ";
    m_command_str = m_command_header + "," +
                    m_interface->flag.get_robot_id_str() + "," +
                    m_tcp_name + "," +
                    m_ucs_name + ",;";

    m_interface->msg.send_msg(m_command_str);
    // set command state to wait for response
    m_cmd_state = AlignToZState::WaitAlignCommandResponse;
}

inline void HyAlignToZ::wait_align_to_z_command_response() {
    if (m_interface->msg.is_response_received()) {
        HuayanMsgReturn *response = m_interface->msg.retrieve_response();
        if (response != nullptr) {
            if (response->GetCommandHeader() == m_command_header) {
                if (response->isCommandOK()) {
                    qDebug() << "Huayan robot controller [HyAlignToZ]: found response, return success";
                    // set command state to send long move event command
                    m_cmd_state = HyAlignToZ::SendLongMoveEventCommand;
                    return;
                } else {
                    qDebug() << "Huayan robot controller [HyAlignToZ]: found response, return error"
                             << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain() ;

                    /// set command state to error
                }
            } else {
                qDebug() << "Huayan robot controller [HyAlignToZ]: wrong command response header";
            }
        } else {
            qCritical() << "HyJogCommand fail, ressponse instance return null pointer.";
        }
        m_cmd_state = AlignToZState::Error;
    }
}

inline void HyAlignToZ::send_long_move_command() {
    QString long_move_cmd = "LongMoveEvent," + m_interface->flag.get_robot_id_str() + ",;";
    m_interface->msg.send_msg(long_move_cmd);
    // set command state to wait for response
    m_cmd_state = AlignToZState::WaitLongMoveEventCommandResponse;
}

inline void HyAlignToZ::wait_long_move_comamnd_response() {
    if (m_interface->msg.is_response_received()) {
        HuayanMsgReturn *response = m_interface->msg.retrieve_response();
        if (response != nullptr) {
            if (response->GetCommandHeader() == "LongMoveEvent") {
                if (response->isCommandOK()) {
                    qDebug() << "Huayan robot controller [HyAlignToZ]: found response, return success.";
                    // set command state to send long move event command
                    if (m_interface->flag.has_jog_cmd_stop_raised()) {
                        m_cmd_state = AlignToZState::Finish;
                    } else {
                        m_time_count.StopTimeCounter();
                        m_cmd_state = AlignToZState::WaitCycle;
                    }
                    return;
                } else {
                    qDebug() << "Huayan robot controller [HyAlignToZ]: found response, return error"
                             << "[" << response->ErrorCode() << "]:" << response->GetErrorExplain();

                    /// set command state to error
                }
            } else {
                qDebug() << "Huayan robot controller [HyAlignToZ]: wrong command response header";
            }
        } else {
            qCritical() << "HyAlignToZ fail, ressponse instance return null pointer.";
        }
        m_cmd_state = AlignToZState::Error;
    }
}

}

/*

Hãy sử dụng Qt6 để viết một tree widget theo các yêu cầu sau:
- mô tả: đây là widget dùng cho bảng lệnh lập trình thao tác chạy tự động của robot
- mục đích: quản lí danh sách lệnh sẽ thực hiện, tương tác với người dùng để thay đổi danh sách lệnh (chèn, thêm xóa, di chuyển vị trí trong bảng), parameter của lệnh
- các event signal cần có: event user click vào lệnh, để link với widget hiển thị thông tin và chỉnh sửa thông tin (tôi sẽ tự làm widget chỉnh sửa lệnh, bạn chỉ cần pass pointer hoặc reference của lệnh mà user chọn là được), event xóa lệnh, chèn lệnh,...
- widget được build theo hướng có thể custom được nhiều loại lệnh để có thể mở rộng trong tương lai
- tree widget không hiển thị header ngang, header dọc
- luôn luôn có lệnh Start, hiển thị ở vị trí đầu tiên (index 0), lệnh này sẽ hiển thị thông tin chung, tôi sẽ tự quyết định thông tin hiển thị sau
- thử tự hiển thị của một row là: số thứ tự, tên lệnh, một số thông tin cơ bản của lệnh, nút di chuyển lên, nút di chuyển xuống, nút xóa
- thứ tự của lệnh bắt đầu từ 0
- các lệnh có thể chèn các child và hiển thị theo thứ tự level thụt ra thụt vào như tree



 */
