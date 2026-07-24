/**
 * SETTING PROCESS CONTROL PROGRAM (PC Program) on KAWASAKI robot controller is needed
 * This is 2 TCP Port
 * Monitor port: 31400
 * Motion port: 31401
 * When disconnect need send disconnect command to motion port
 *  a variable named disconnect_now, which is gobal variable defined on AS Controller
 *  will be set (assigned 1) to break recieve data loop on 2 PC Program
 *
 * Interface command number
 * 1 : MoveJ
 * 2 : MoveL
 * 4 : Stop
 * 5 : Set DO
 * 6 : Set TCP
 * 9 : Get Tool
 * 11 : Get Flange Pose
 * 13 : Control( 1 - viz, 2 - robot)
 * 82 : Disconnect
 * 83 : Version
 *
 * Considering align z, JAPPRO
 *
 * Considering dedicated output signal, normal:
 *      1 : use for monitor motor on, off state
 *      2 : use for monitor Error state
 *      3 : use for monitor Automatic state
 *      check at Aux. Dedicated output signal
 *
 * SPEED in AS Program is percent speed of maximum performance speed of robot
 *
 * MoveL command format:
 *      cm2,x,y,z,rx,ry,rz,speed,accel,accuracy
 *      cm2,0,400,200,-180,180,90,50,50,200,
 *      cm2,0,-400,200,-180,180,90,50,50,200,
 *
 */

#ifndef KAWASAKI_H
#define KAWASAKI_H

#include <memory>

#include <QMutex>
// #include "utils/chronocounter.h"
// #include "robot/rbutils.h"
// #include "robot/kawasaki/kawasaki_context.h"
#include "robot/kawasaki/command/kawasaki_command.h"
#include "robot/robot_abstract.h"
#include "robot/robot_command.h"
#include "robot/robot_command.h"
#include "robot/robot_point.h"
#include "kawasaki_motion.h"
#include "kawasaki_status.h"

namespace rb {

class KawasakiConnectParams : public RbConnectParams {
public:
    KawasakiConnectParams() {}

public:
    QString ip_address;
    QThread::Priority thread_priority;
};

class RbKawasaki : public RobotAbstract {
    Q_OBJECT
public:
    explicit RbKawasaki (QObject *parent = nullptr);
    ~RbKawasaki ();

    rb::RbType RobotType() const override {
        return RbType::Kawasaki_EF;
    }

    bool RobotConnect(RbConnectParams *params) override;
    void RobotDisconnect() override;

    const bool RobotReadyForConnect() override;
    const bool isRobotConnected() override;
    const bool isRobotDisconnected() override;
    const bool RobotEnableState() override;
    void RobotEmergencyStop() override;
    CartesianPoint RobotCurrentPosision() override;
    const bool RobotHomeReturn() override;

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
        SqRunning,
        SqLastCommandExecuted,
        SqWaitFinish,
        SqForceStop
    };

    void robotTerminate();
    void remoteControllerStart();
    void remoteControllerClean();
    void resetStatusBeforeLoop();
    inline void emergencyStopHandle();
    inline void resetCommandQueue();
    inline void feedbackPortHandle();
    inline void subCommandHandle();
    inline void sequenceCommandHandle();
    inline void autoRunHandle();

    inline void setSqStartCommandPointer();
    inline void setSqStopCommandPointer();
    inline void setSqNextCommandPointer();

    inline void internalFlagHandle(KwInternalFlag flag);
    inline bool isAtHome();
    inline bool isInRange(double value_1, double value_2, double range);

signals:
    void s_RobotFeedbackUpdate(KawasakiSheet feedback);

private:
    // mutex locker to prevent thread memory access conflict
    QMutex m_data_locker;

    // polling thread priority
    QThread::Priority m_thread_priority;

    // connect tcp address
    QString m_tcp_address;
    // tcp port for motion
    std::unique_ptr<KawasakiMotionPort> m_motion_port{nullptr};
    // tcp port for status
    std::unique_ptr<KawasakiStatusPort> m_status_port{nullptr};
    // feedback data struct
    KawasakiSheet m_feedback_data;

    // time counter for emit send feedback data to UI
    std::shared_ptr<ChronoCounter> m_feedback_output_timer;
    // refresh UI interval time
    int m_fb_output_timeout;

    // single execute command queue
    QList<std::shared_ptr<RbCommand>> m_sub_cmd_queue;
    // current single command execute
    std::shared_ptr<RbCommand> m_sub_current_cmd;

    // command sequene queue
    QList<std::shared_ptr<RbCommand>> m_sq_cmd_queue;
    // current sequence command in queue
    std::shared_ptr<RbCommand> m_current_cmd;
    // current executing command index in sequence run mode
    int m_sq_executing_idx;
    // next executing command index
    int m_sq_next_execute_idx;
    // previous executing command index
    int m_sq_previous_execute_idx;
    // response list received from robot
    QList<KawasakiMsgReturn> m_cmd_sq_responses;
    // command interface context
    std::shared_ptr<KawasakiCmdInterface> m_cmd_interface;
    // context message transport service
    std::shared_ptr<KwMsgTransport> m_ctx_msg_transport;
    // context flag transport service
    std::shared_ptr<KwFlagTransport> m_ctx_flag_transport;

    // current state of sequence run mode
    SqRunningStage m_sq_stage;
    // is sequence mode running
    bool m_is_sq_runnning;
    // is sequence mode in continous run
    bool m_is_sq_continuous;

    // keep thread polling
    bool m_controller_running = false;
    // is connected with robot
    bool m_robot_connected = false;
    // is ready for connection, to prevent start again thread while connecting
    bool m_is_ready_connect = true;
    // is lost connect if receiving timeout
    bool m_feedback_lost_connect = false;
    // first received feedback when connec with robot
    bool m_is_first_feedback = true;
    // user emergency button trigger
    bool m_emergency_stop_triggered = false;
    // error state read from feedback package
    bool m_error_stop = false;
    // robot enable status
    bool m_status_enable = false;
    // kawasaki robot condition for repeat mode operation
    bool m_is_ready_for_repeat_run= false;

    // robot home position
    CartesianPoint m_home_position;
    // is robot stand at home position, with position tolerrance around 0.15mm
    bool m_stand_at_home;

    // robot tcp offset
    CartesianPoint m_tcp_offset;

    // vision coordinate put index
    int m_coordinate_put_index;
    // coordinate received from vision module
    CartesianPoint m_vs_coordinate;

    // int m_wait_stop_moving;

    /// variables for plc
    Fx3Communicator *m_plc_communicator;
    Fx3DeviceMap m_plc_device_map;
};

}
#endif // KAWASAKI_H
