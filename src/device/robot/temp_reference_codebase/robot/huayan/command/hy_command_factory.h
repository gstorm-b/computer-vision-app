#ifndef HY_COMMAND_FACTORY_H
#define HY_COMMAND_FACTORY_H

#include <memory>
#include <QList>
#include <QVariant>
#include "robot/robot_command.h"
#include "robot/robot_point.h"
#include "robot/huayan/command/hy_setter_command.h"
#include "robot/huayan/command/hy_jog_command.h"
#include "robot/huayan/command/hy_move_rel.h"
#include "robot/huayan/command/hy_aligntoz.h"
#include "robot/huayan/command/hy_movel.h"
#include "robot/huayan/command/hy_waypoint.h"
#include "robot/huayan/command/hy_waypoint_rel.h"
#include "robot/huayan/command/hy_set_output.h"
#include "robot/huayan/command/hy_set_if_flag.h"
#include "robot/huayan/command/hy_dhgripper.h"
#include "robot/huayan/command/hy_read_waypoint_id.h"

namespace rb {

class HyCmdFactory {
public:
    static std::shared_ptr<RbCommand> OSCmd() {
        return std::make_shared<HySetterCommand>("OSCmd");
    }

    static std::shared_ptr<RbCommand> ConnectToBox() {
        return std::make_shared<HySetterCommand>("ConnectToBox");
    }

    static std::shared_ptr<RbCommand> Electrify() {
        return std::make_shared<HySetterCommand>("Electrify");
    }

    static std::shared_ptr<RbCommand> BlackOut() {
        return std::make_shared<HySetterCommand>("BlackOut");
    }

    static std::shared_ptr<RbCommand> StartMaster() {
        return std::make_shared<HySetterCommand>("StartMaster");
    }

    static std::shared_ptr<RbCommand> CloseMaster() {
        return std::make_shared<HySetterCommand>("CloseMaster");
    }

    static std::shared_ptr<RbCommand> IsSimulation() {
        return std::make_shared<HySetterCommand>("IsSimulation");
    }

    static std::shared_ptr<RbCommand> GrpEnable() {
        return std::make_shared<HySetterCommand>("GrpEnable");
    }

    static std::shared_ptr<RbCommand> GrpDisable() {
        return std::make_shared<HySetterCommand>("GrpDisable");
    }

    static std::shared_ptr<RbCommand> GrpReset() {
        return std::make_shared<HySetterCommand>("GrpReset");
    }

    static std::shared_ptr<RbCommand> GrpStop() {
        return std::make_shared<HySetterCommand>("GrpStop");
    }

    static std::shared_ptr<RbCommand> GrpContinue() {
        return std::make_shared<HySetterCommand>("GrpContinue");
    }

    static std::shared_ptr<RbCommand> GrpCloseFreeDriver() {
        return std::make_shared<HySetterCommand>("GrpCloseFreeDriver");
    }

    static std::shared_ptr<RbCommand> GrpOpenFreeDriver() {
        return std::make_shared<HySetterCommand>("GrpOpenFreeDriver");
    }

    // not use
    static std::shared_ptr<RbCommand> ReadFastPort() {
        return std::make_shared<HySetterCommand>("ReadFastCmdPort");
    }

    static std::shared_ptr<RbCommand> SetSpeedOverride(double value) {
        QList<QVariant> params;
        params.push_back(value);
        return std::make_shared<HySetterCommand>("SetOverride", params);
    }

    static std::shared_ptr<RbCommand> FreeJog(rb::RbMotionType type, rb::RbAxis axis, rb::RbDirection direction) {
        return std::make_shared<HyJogCommand>(type, axis, direction);
    }

    static std::shared_ptr<RbCommand> DistanceJog(rb::RbMotionType type, rb::RbAxis axis, rb::RbDirection direction,
                                                 double distance, bool motion_tool = false) {
        return std::make_shared<HyMoveRel>(type, axis, direction, distance, motion_tool);
    }

    static std::shared_ptr<RbCommand> AlignToZ(QString &tcp_name, QString &ucs_name) {
        return std::make_shared<HyAlignToZ>(tcp_name, ucs_name);
    }

    static std::shared_ptr<RbCommand> MoveLTo(CartesianPoint point, QString tcp_name, QString ucs_name) {
        return std::make_shared<HyMoveL>(point, tcp_name, ucs_name);
    }

    static std::shared_ptr<RbCommand> WayPointMove(RbMotionType motion_type,
                                                   double velocity, double accel, double blendRadius,
                                                   rb::CartesianPoint target_point, rb::JointPoint target_joint_point,
                                                   QString unique_id,
                                                   QString tcp, QString ucs, bool isUseJoint = false) {
        return std::make_shared<HyWayPoint>(motion_type, velocity, accel, blendRadius,
                                            target_point, target_joint_point, unique_id,
                                            tcp, ucs, isUseJoint);
    }

    static std::shared_ptr<RbCommand> WayPointMoveL(double velocity, double accel, double blendRadius,
                                                   rb::CartesianPoint target_point, QString unique_id,
                                                   QString tcp, QString ucs, bool isUseJoint = false) {
        return std::make_shared<HyWayPoint>(RbMotionType::mtLinear, velocity, accel, blendRadius,
                                            target_point, rb::JointPoint(), unique_id,
                                            tcp, ucs, isUseJoint);
    }

    static std::shared_ptr<RbCommand> WayPointMoveJ(double velocity, double accel, double blendRadius,
                                                   rb::JointPoint target_joint_point, QString unique_id,
                                                   QString tcp, QString ucs, bool isUseJoint = false) {
        return std::make_shared<HyWayPoint>(RbMotionType::mtJoint, velocity, accel, blendRadius,
                                            rb::CartesianPoint(), target_joint_point, unique_id,
                                            tcp, ucs, isUseJoint);
    }

    static std::shared_ptr<RbCommand> WayPointMoveRelL(RbMotionType motion_type,
                                            double velocity, double accel, double blendRadius,
                                            CartesianPoint target_distance,
                                            CartesianPoint ref_point, JointPoint ref_joint_point,
                                            bool use_ref_point, bool ref_is_absolute,
                                            QString unique_id,
                                            QString tcp, QString ucs, bool isUseJoint = false) {

        return std::make_shared<HyWayPointRel>(motion_type, velocity, accel, blendRadius,
                                            target_distance, ref_point, ref_joint_point,
                                            use_ref_point, ref_is_absolute,
                                            unique_id,
                                            tcp, ucs, false, isUseJoint);
    }

    static std::shared_ptr<RbCommand> SetOutput(QString device, QString name, bool value, bool wait_stop = true) {
        return std::make_shared<HySetOutput>(device, name, value, wait_stop);
    }

    static std::shared_ptr<RbCommand> SetIFComamnd(HyInternalFlag flag) {
        return std::make_shared<HySetIFCommand>(flag);
    }

    static std::shared_ptr<RbCommand> DHGripperControl(bool is_close) {
        return std::make_shared<HyDhGripperControl>(is_close);
    }

    static std::shared_ptr<RbCommand> ReadWayPointID() {
        return std::make_shared<HyReadWayPointID>();
    }
};

}

#endif // HY_COMMAND_FACTORY_H
