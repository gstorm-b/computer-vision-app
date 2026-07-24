#ifndef KAWASAKI_STATUS_H
#define KAWASAKI_STATUS_H

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValueConstRef>
#include <QJsonValue>

#include "utils/chronocounter.h"
#include "robot/rbutils.h"
#include "robot/robot_point.h"
#include "robot/robot_tcp_client.h"

#define KAWASAKI_CONTROLLER_INPUT_NUM       32
#define KAWASAKI_CONTROLLER_OUTPUT_NUM      32
#define KAWASAKI_CONTROLLER_INTERNAL_NUM    32
#define KAWASAKI_CONTROLLER_START_INTERNAL  2001

namespace rb {

class KawasakiSheet : public rb::RbAbstractFeedBack {
public:
    // current robot pose base OAT
    CartesianPoint current_pose_oat;
    // current robot pose base roll pick yaw
    CartesianPoint current_pose_rpy;
    // current robot joint
    JointPoint current_joint;
    // external input of robot controller
    int external_input[KAWASAKI_CONTROLLER_INPUT_NUM]{false};
    // external output of robot controller
    int external_output[KAWASAKI_CONTROLLER_OUTPUT_NUM]{false};
    // internal signal of robot controller
    int internal_signal[KAWASAKI_CONTROLLER_INTERNAL_NUM]{false};
    // monitor speed
    int monitor_speed{0};
    // controller run/hold state
    bool state_run{false};
    // motor power state
    bool state_power{false};
    // repeat mode state
    bool state_repeat{false};
    // cycle start state
    bool state_cycle_start{false};
    // emergency state
    bool state_emergency{false};
    // error state
    bool state_error{false};
    // robot start moving
    bool start_moving{false};
};

class KawasakiStatusPort : public TCPClient {
public:
    KawasakiStatusPort();

    inline bool IsDataHasRead() const;
    const KawasakiSheet& GetFeedbackData();
    bool polling();
    bool retrieveReadStatus();

private:
    inline bool parseData(QString &pack);

private:
    bool m_isDataHasRead{false};
    KawasakiSheet m_feedbackData;
    bool m_read_timeout{false};
    ChronoCounter m_read_time_counter;
    int m_wait_timeout{500};
};

}

#endif // KAWASAKI_STATUS_H
