#ifndef KW_COMMAND_FACTORY_H
#define KW_COMMAND_FACTORY_H

#include <memory>
#include <QList>
#include <QVariant>
#include "robot/robot_command.h"
#include "robot/robot_point.h"
#include "robot/kawasaki/command/kw_move_joint.h"
#include "robot/kawasaki/command/kw_move_linear.h"
#include "robot/kawasaki/command/kw_move_rel_linear.h"
#include "robot/kawasaki/command/kw_set_if_flag.h"

namespace rb {

class KwCmdFactory {
public:
    static std::shared_ptr<RbCommand> MoveJ(JointPoint target, double velocity, double accel, double accuracy) {
        return std::make_shared<KwMoveJ>(target, velocity, accel, accuracy);
    }

    static std::shared_ptr<RbCommand> MoveL(CartesianPoint point, double velocity, double accel, double accuracy, bool is_use_rpy, bool is_vs_coordinate) {
        return std::make_shared<KwMoveL>(point, velocity, accel, accuracy, is_use_rpy, is_vs_coordinate);
    }


    static std::shared_ptr<RbCommand> MoveRelativeL(CartesianPoint target_distance,
                                            CartesianPoint ref_point,
                                            double velocity, double accel, double accuracy,
                                            bool use_ref_point, bool use_vs_point = false) {
        return std::make_shared<KwMoveRelL>(target_distance, ref_point,
                                            velocity, accel, accuracy,
                                            use_ref_point, use_vs_point);
    }

    static std::shared_ptr<RbCommand> SetIFComamnd(KwInternalFlag flag) {
        return std::make_shared<KwSetIFCommand>(flag);
    }
};

}


#endif // KW_COMMAND_FACTORY_H
