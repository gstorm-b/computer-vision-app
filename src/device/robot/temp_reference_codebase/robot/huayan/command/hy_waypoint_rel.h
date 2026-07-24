#ifndef HY_WAYPOINT_REL_H
#define HY_WAYPOINT_REL_H

#include "robot/robot_point.h"
#include "robot/rbutils.h"
#include "robot/huayan/huayan_command.h"

/**
 * WayPointRel: move with relative position from current point,
 * allow to move continuosly without deceleration
 *
 * LIST OF PARAMS:
 * nRbtID: robot id
 * nType: motion type, 0 is relative joint move, 1 is linear joint move
 * nPointList: whether to use point list points, 0 is not use, 1 is use
 * dPosX - dPosRz: nPointList = 1, use partial location, nPointList = 0 all zero
 * dPosJ1 - dPosJ6: nPointList = 1, use partial location, nPointList = 0 all zero
 * nrelMoveType: relative motion type,
 *               0 is absolute value, (translate only mask axis to asbolute position)
 *               1 is additive value, (translate only mask axis to relative position)
 *               2 is tool mode, (translate relative with tool axis coordinate)
 * nAxisMask_1 - nAxisMask_6: Whether to move, whether each axis/direction is moving
 *              0: Not moving
 *              1: Moving
 * dTarget_1- dTarget_6: movement distance
 *              nType = 0 and nAxisMask = 1, absolute or superimposed distance in this direction
 *              nType = 1 and nAxisMask = 1, absolute or superimposed distance in this axis
 *              nAxisMask = 0 this params Invalid
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
 * nIsUseJoint: Whether to use joint coordinates.
 *          0: Do not use joint angle, default
 *          1: Use joint angle
 * nIsSeek: If nIsSeek is 1, the DI stop detection is enabled.
 *          During the waypoint movement,
 *          if the DI status indexed by the nIOBit bit of the switch box is equal to nIOState,
 *          the robot stops moving.
 *          Otherwise, it moves to the target point and completes the movement.
 * nIOBit: The DI index corresponding to the switch box is invalid when nIsSeek= 0.
 * nIOState: Detected DI status, invalid when nIsSeek= 0
 * strCmdID: The current waypoint ID can be customized or set in sequence to "1", "2", and "3".
 *
 * example move z axis relavtive from a point already know
 * WayPointRel,0,1,1,394,180,291,180,0,33,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0,-50,0,0,0,TCP_PGC_140_50,Base,50.0,360.0,50.0,0,0,0,0,ID1,;
 * example move z axis relavtive from current point
 * WayPointRel,0,1,0,394,180,291,180,0,33,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0,-50,0,0,0,TCP_PGC_140_50,Base,50.0,360.0,50.0,0,0,0,0,ID1,;
 *
 */

namespace rb {

class HyWayPointRel : public HuayanCommand {
public:
    HyWayPointRel(RbMotionType motion_type,
                  double velocity, double accel, double blendRadius,
                  CartesianPoint target_distance,
                  CartesianPoint ref_point, JointPoint ref_joint_point,
                  bool use_ref_point, bool ref_is_absolute,
                  QString uuid,
                  QString tcp = "TCP", QString ucs = "Base",
                  bool use_vs_point = false, bool isUseJoint = false);

    QString comamndId() override {
        return m_uuid_string;
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_state = WayPointRelExecuteState::UnExecute;
    }

    const bool isMotionCommand() override {
        return true;
    }

    void execute() override;

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyWayPointRel>(*this);
    }

private:
    enum WayPointRelExecuteState {
        UnExecute = 0,
        SendMoveCommand,
        WaitMoveCommandReponse,
        Finish,
        Error
    };

    inline void send_waypointrel_command();
    inline void wait_waypointrel_command_response();

private:
    WayPointRelExecuteState m_cmd_state{WayPointRelExecuteState::UnExecute};
    QString m_wprel_cmd;
    const QString m_wprel_cmd_header{"WayPointRel"};

    CartesianPoint m_target_distance;
    CartesianPoint m_ref_point;
    JointPoint m_ref_joint_point;
    bool m_use_ref_point{false};
    bool m_ref_point_is_absolute{false};
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
    QString m_uuid_string{""};

    bool m_use_vs_point;

    bool m_had_trigger_vision_coordinate;
};

}
#endif // HY_WAYPOINT_REL_H
