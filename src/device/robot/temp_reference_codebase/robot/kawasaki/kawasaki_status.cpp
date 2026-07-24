#include "kawasaki_status.h"
#include "log_helper/log_wrapper.h"

#define LOG_NAME "[Kawasaki robot] status port: "
#define RETURN_IF_NOT_NUMBER  if (!is_number) return false

namespace rb {

KawasakiStatusPort::KawasakiStatusPort() : TCPClient() {
    m_read_timeout = false;
    m_read_time_counter.StopTimeCounter();
    m_wait_timeout = 500;
}

inline bool KawasakiStatusPort::IsDataHasRead() const {
    return m_isDataHasRead;
}

const KawasakiSheet& KawasakiStatusPort::GetFeedbackData() {
    return m_feedbackData;
}

bool KawasakiStatusPort::polling() {
    if (this->m_socket == nullptr) {
        return false;
    }

    try {
        m_read_timeout = this->m_socket->waitForReadyRead(20);
        if (!m_read_timeout) {
            return !(m_read_time_counter.StartTimeCounter(m_wait_timeout));
        }
        m_read_time_counter.StopTimeCounter();

        // read all byte available on buffer
        QByteArray raw = this->m_socket->readAll();
        if (raw.size() < 10) {
            return true;
        }

        // get last correct feedback package
        QString raw_string = QString::fromUtf8(raw);
        QStringList packs = raw_string.split(",;;");
        packs.removeLast();
        int pack_index = packs.size() - 1;
        while (pack_index >= 0) {
            if (parseData(packs[pack_index])) {
                // parse data successful
                m_isDataHasRead = true;
                break;
            }
            pack_index--;
        }
    } catch (const std::exception& e) {
        OLOG_INFO << "try to read from buffer fail:" << e.what();
        m_port_state = TcState::Error;
        return false;
    }

    return true;
}

bool KawasakiStatusPort::retrieveReadStatus() {
    bool status = m_isDataHasRead;
    m_isDataHasRead = false;
    return status;
}

inline bool KawasakiStatusPort::parseData(QString &pack) {
    QStringList elements = pack.split(',');
    // total elements + 1
    if (elements.size() != 21) {
        return false;
    }

    bool is_number =  false;
    // KawasakiSheet
    // one elements represent for 16 bit from robot controller
    // referenec frame: current joint angle (0 - 6), current pose (7 - 12),
    //                  input status (13 - 14), output status (15 - 16), monitor speed(17), state (18)

    // parse current joint angle
    double joint_j1 = elements[0].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double joint_j2 = elements[1].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double joint_j3 = elements[2].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double joint_j4 = elements[3].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double joint_j5 = elements[4].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double joint_j6 = elements[5].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    // reserve elements[6] for additional axis

    // parse current position
    double pose_x = elements[7].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double pose_y = elements[8].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double pose_z = elements[9].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double pose_rx = elements[10].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double pose_ry = elements[11].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;
    double pose_rz = elements[12].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;

    // parse controller external input signal
    int gi_low = elements[13].toInt(&is_number, 10);
    RETURN_IF_NOT_NUMBER;
    int gi_high = elements[14].toInt(&is_number, 10);
    RETURN_IF_NOT_NUMBER;

    // parse controller external output signal
    int go_low = elements[15].toInt(&is_number, 10);
    RETURN_IF_NOT_NUMBER;
    int go_high = elements[16].toInt(&is_number, 10);
    RETURN_IF_NOT_NUMBER;

    // parse controller internal signal
    int ginternal_low= elements[17].toInt(&is_number, 10);
    RETURN_IF_NOT_NUMBER;
    int ginternal_high = elements[18].toInt(&is_number, 10);
    RETURN_IF_NOT_NUMBER;

    int mspeed = elements[19].toInt(&is_number, 10);
    RETURN_IF_NOT_NUMBER;

    // parse current state
    int states = elements[20].toDouble(&is_number);
    RETURN_IF_NOT_NUMBER;

    m_feedbackData.current_pose_oat =
        CartesianPoint(pose_x, pose_y, pose_z, pose_rx, pose_ry, pose_rz);
    m_feedbackData.current_joint =
        JointPoint(joint_j1, joint_j2, joint_j3, joint_j4, joint_j5, joint_j6);

    for (int i = 0; i < 16; ++i) {
        m_feedbackData.external_input[i] = (gi_low >> (15 - i)) & 1;
    }

    for (int i = 0; i < 16; ++i) {
        m_feedbackData.external_input[16 + i] = (gi_high >> (15 - i)) & 1;
    }

    for (int i = 0; i < 16; ++i) {
        m_feedbackData.external_output[i] = (go_low >> (15 - i)) & 1;
    }

    for (int i = 0; i < 16; ++i) {
        m_feedbackData.external_output[16 + i] = (go_high >> (15 - i)) & 1;
    }

    for (int i = 0; i < 16; ++i) {
        m_feedbackData.internal_signal[i] = (ginternal_low >> (15 - i)) & 1;
    }

    for (int i = 0; i < 16; ++i) {
        m_feedbackData.internal_signal[16 + i] = (ginternal_high >> (15 - i)) & 1;
    }

    m_feedbackData.monitor_speed = mspeed;

    /*
     * | RUN/HOLD | POWER | CS | REPEAT | ERROR | reserve | EMERGENCY |
     */
    m_feedbackData.state_run = states & 1;
    m_feedbackData.state_power = (states >> 1) & 1;
    m_feedbackData.state_cycle_start = (states >> 2) & 1;
    m_feedbackData.state_repeat = (states >> 3) & 1;
    m_feedbackData.state_error = (states >> 4) & 1;
    m_feedbackData.state_emergency = (states >> 6) & 1;
    m_feedbackData.start_moving = (states >> 7) & 1;
    if (m_feedbackData.start_moving == 1) {
        OLOG_INFO << "robot start moving";
    }
    return true;
}

}
