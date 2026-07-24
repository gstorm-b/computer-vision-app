/*
 * IMPORTANT NOTES
 * Fast port added after version HR6.5.1
 * Fast port not accept all command
 * Fast port only accept read types command
 * Robot version at that time wrote this library was HR6.4.1
 */

#ifndef HUAYAN_H
#define HUAYAN_H

#include <memory>

#include <QMutex>
#include "utils/chronocounter.h"
#include "robot/rbutils.h"
#include "robot/robot_abstract.h"
#include "robot/robot_command.h"
#include "robot/robot_point.h"
#include "robot/huayan/huayan_types.h"
#include "robot/huayan/huayan_msg_return.h"
#include "robot/huayan/huayan_feedback_port.h"
#include "robot/huayan/huayan_command_port.h"
#include "robot/huayan/huayan_command.h"
#include "robot/huayan/huayan_context.h"
#include "plc/fx3communicator.h"

namespace rb {

class HuayanConnectParams : public RbConnectParams {
public:
    HuayanConnectParams() {}

public:
    QString ip_address;
    QThread::Priority thread_priority;
};

class RbHuayan : public RobotAbstract {
    Q_OBJECT
public:
    explicit RbHuayan(QObject *parent = nullptr);
    ~RbHuayan();

    rb::RbType RobotType() const override {
        return RbType::Huayan_E;
    }

    /**
     * @brief RobotConnect: connect to robot, start thread
     */
    bool RobotConnect(RbConnectParams *params) override;

    /**
     * @brief RobotDisconnect: disconnect with robot
     */
    void RobotDisconnect() override;

    const bool RobotReadyForConnect() override;
    const bool isRobotConnected() override;
    const bool isRobotDisconnected() override;
    const bool RobotEnableState() override;
    void RobotEmergencyStop() override;
    CartesianPoint RobotCurrentPosision() override;
    const bool RobotHomeReturn() override;

    void RobotPushCommand(std::shared_ptr<RbCommand> cmd);
    bool RobotFreeJog(RbMotionType _type, RbAxis _axis, RbDirection _direction);
    bool RobotStopFreeJog();
    bool RobotDistanceJog(RbMotionType motion_type, RbAxis axis, RbDirection direction,
                          double distance, bool tool_motion = false);
    bool RobotAlignToZ(QString tcp_name, QString ucs_name);
    void RobotMoveHome();
    void RobotSetHomePos(CartesianPoint position);
    CartesianPoint RobotHomePos();

    void RobotRunContinuous();
    void RobotRunSingleCycle();
    void RobotStopCycle();
    void RobotForceStop();
    bool RobotIsSqRunning();
    bool RobotIsInError();
    void RobotPushMainSequence(QList<std::shared_ptr<RbCommand>> &cmd_sq);

    void SetPlcCommunicator(Fx3Communicator *plc_communicator);

    void VisionReturn(CartesianPoint coor);
    void VisionFail();
    void VisionReset();

protected:

    /**
         * @brief run: robot thread loop
         * @details this thread loop get robot feedback, handle command
         */
    void run() override;

private:
    enum SqRunningStage {
        SqIdle,
        SqPrepareForRun,
        SqStartNewRun,
        SqRunning,
        SqLastCommandExecuted,
        SqWaitStopMoving,
        SqStop,
        SqForceStop
    };

    void robotTerminate();
    void remoteControllerStart();
    void remoteControllerClean();
    void resetStatusBeforeLoop();
    inline void emergencyStopHandle();
    inline void feedbackPortHandle();
    inline void commandPortHandle();
    inline void sqCommandPortHandle();
    inline void resetCommandQueue();
    inline void autoRunHandle();

    inline void setSqStartCommandPointer();
    inline void setSqStopCommandPointer();
    inline void setSqNextCommandPointer();
    inline void setSqCommandPointer(int line_number);

    inline void internalFlagHandle(HyInternalFlag flag);
    inline bool isAtHome();
    inline bool isInRange(double value_1, double value_2, double range);

signals:
    void s_RobotFeedbackUpdate(HuayanSheet feedback);

private:
    QMutex m_data_locker;

    // fast port only exists after added after version HR6.5.1
    // HuayanCommandPort *m_fast_command_port = nullptr;
    std::unique_ptr<HuayanCommandPort> m_command_port{nullptr};
    std::unique_ptr<HuayanCommandPort> m_command_sq_port{nullptr};
    std::unique_ptr<HuayanFeedBackPort> m_feedback_port{nullptr};

    HuayanSheet m_feedback_data;

    QThread::Priority m_thread_priority;
    QString m_tcp_address;

    std::shared_ptr<ChronoCounter> m_feedback_output_counter;
    int m_fb_output_timeout;

    QList<std::shared_ptr<RbCommand>> m_cmd_queue;
    std::shared_ptr<RbCommand> m_current_cmd;
    QList<HuayanMsgReturn> m_cmd_responses;
    std::shared_ptr<HuayanCmdInterface> m_cmd_interface;
    std::shared_ptr<HyMsgTransport> m_ctx_msg_transport = nullptr;
    std::shared_ptr<HyFlagTransport> m_ctx_flag_transport = nullptr;

    bool m_controller_running = false;
    bool m_robot_connected = false;
    bool m_is_ready_connect = true;
    bool m_feedback_lost_connect = false;
    bool m_is_first_feedback = true;
    bool m_emergency_stop_triggered = false;
    bool m_error_stop = false;

    // bool m_is_connected = false;
    bool m_status_enable = false;

    bool m_is_ready_for_operate = false;

    /// Variables for sq control
    // QList<std::shared_ptr<RbCommand>> m_main_sq;
    QList<std::shared_ptr<RbCommand>> m_sq_cmd_queue;
    QMap<int, int> m_sq_line_number_map;
    std::shared_ptr<RbCommand> m_sq_current_cmd;
    int m_sq_executing_idx;
    int m_sq_next_execute_idx;
    int m_sq_previous_execute_idx;
    int m_sq_current_line_number;
    QList<HuayanMsgReturn> m_sq_cmd_responses;
    std::shared_ptr<HuayanCmdInterface> m_sq_cmd_interface;
    std::shared_ptr<HyMsgTransport> m_ctx_sq_msg_transport = nullptr;
    std::shared_ptr<HyFlagTransport> m_ctx_sq_flag_transport = nullptr;

    SqRunningStage m_sq_stage;
    bool m_is_sq_runnning;
    bool m_is_sq_continuous;

    CartesianPoint m_home_position;
    bool m_stand_at_home;
    double m_before_movehome_speed;

    bool m_in_moving;

    bool m_vs_outside_fail;
    bool m_vs_outside_reset;
    bool m_vs_outside_new_coor;
    CartesianPoint m_vs_outside_coordinate;

    bool m_containing_coordinate;
    bool m_wait_for_coordinate;
    int m_coordinate_put_index;
    CartesianPoint m_vs_coordinate;

    bool m_vision_trigger_waypoint_wait;
    bool m_vision_trigger_wait_wp_finish;
    QString m_vision_trigger_waypoint_id;

    QString m_normal_tcp_name{"TCP_PGC_140_50"};
    QString m_normal_ucs_name{"Base"};

    /// variables for plc
    Fx3Communicator *m_plc_communicator;
    Fx3DeviceMap m_plc_device_map;
};

}

#endif
