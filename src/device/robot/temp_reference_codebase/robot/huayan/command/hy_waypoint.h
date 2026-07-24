#ifndef HY_WAYPOINT_H
#define HY_WAYPOINT_H

#include "robot/robot_point.h"
#include "robot/rbutils.h"
#include "robot/huayan/huayan_command.h"

/**
 * WayPoint: move to end point, allow to move continuosly without deceleration
 * LIST OF PARAMS:
 * nRbtID: robot id
 * dX - dRz: cartesian position
 * dJ1 - dJ6: joint position
 * sTcpName: name of tcp set in tech pandent, can use default "TCP",
 *          it will invalid when nIsUseJoint = 1
 * sUcsName: name of user coordinate system, can use default "Base",
 *          it will invalid when nIsUseJoint = 1
 * dVelocity: maximum move speed,
 *          unit for joint movement [°/s],
 *          unit for XYZ movement [mm/s], unit for Rx, Ry, Rz [°/s]
 * dAcc: maximum move acceleration,
 *          in [°/s²] for joint movement,
 *          [mm/s²] for XYZ movement,
 *          [°/s²] for spatial movement, and [°/s²] for Rx, Ry, and Rz.
 * dRadius: Transition radius, unit [mm]
 * nMoveType: movement type, 1 is Linear movement, 0 is Joint movement
 * nIsUseJoint: Whether to use joint angle as target point.
 *          If nMoveType=0, nIsUseJoint is valid:
 *          0: Do not use joint angle
 *          1: Use joint angle
 * nIsSeek: If nIsSeek is 1, the DI stop detection is enabled.
 *          During the waypoint movement,
 *          if the DI status indexed by the nIOBit bit of the switch box is equal to nIOState,
 *          the robot stops moving.
 *          Otherwise, it moves to the target point and completes the movement.
 * nIOBit: The DI index corresponding to the switch box is invalid when nIsSeek= 0.
 * nIOState: Detected DI status, invalid when nIsSeek= 0
 * strCmdID: The current waypoint ID can be customized or set in sequence to "1", "2", and "3".
 */

namespace rb {

class HyWayPoint : public HuayanCommand {
public:
    HyWayPoint(RbMotionType motion_type,
               double velocity, double accel, double blendRadius,
               CartesianPoint target_point,
               JointPoint target_joint_point,
               QString uuid,
               QString tcp = "TCP", QString ucs = "Base",
               bool isUseJoint = false);

    QString comamndId() override {
        return m_uuid_string;
    }


    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = WayPointExecuteState::UnExecute;
    }

    const bool isMotionCommand() override {
        return true;
    }

    void execute() override;

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyWayPoint>(*this);
    }

private:
    enum WayPointExecuteState {
        UnExecute = 0,
        SendMoveCommand,
        WaitMoveCommandReponse,
        Finish,
        Error
    };

    inline void send_waypoint_command();
    inline void wait_waypoint_command_response();

    // inline QString get_move_command();

private:
    WayPointExecuteState m_cmd_state = WayPointExecuteState::UnExecute;
    QString m_waypoint_cmd;
    const QString m_waypoint_cmd_header{"WayPoint"};

    CartesianPoint m_end_point;
    JointPoint m_end_joint_point;
    QString m_tcp_name{"TCP"};
    QString m_ucs_name{"Base"};
    double m_velocity{1000};
    double m_acceleration{1500};
    double m_blend_radius{0};
    RbMotionType m_move_type;
    int m_is_use_joint{0};
    int m_is_seek{0};
    int m_io_bit{0};
    int m_io_state{0};
    QString m_uuid_string;
};

}

#endif // HY_WAYPOINT_H
