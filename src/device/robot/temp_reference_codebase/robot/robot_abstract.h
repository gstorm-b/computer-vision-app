#ifndef ROBOT_ABSTRACT_H
#define ROBOT_ABSTRACT_H

#include <memory>
#include <QObject>
#include <QThread>
#include "robot/robot_point.h"
#include "robot/rbutils.h"

namespace rb {

enum RbType {
    Robot_unknown = 0,
    DOBOT_MG400,
    Huayan_E,
    Kawasaki_EF
};

class RbConnectParams {
public:
    virtual ~RbConnectParams() = default;
};

class RobotAbstract : public QThread {
    Q_OBJECT
public:
    // using QThread::QThread;
    explicit RobotAbstract(QObject *parent = nullptr) :
        QThread(parent) {

    }

    ~RobotAbstract() {}

    virtual rb::RbType RobotType() const = 0;

    virtual bool RobotConnect(RbConnectParams *params) = 0;
    virtual void RobotDisconnect() = 0;

    virtual const bool RobotReadyForConnect() = 0;
    virtual const bool isRobotConnected() = 0;
    virtual const bool isRobotDisconnected() = 0;

    virtual const bool RobotEnableState() = 0;

    virtual void RobotEmergencyStop() = 0;

    virtual CartesianPoint RobotCurrentPosision() = 0;

    virtual const bool RobotHomeReturn() = 0;

signals:
    /**
     * @brief s_RobotConnected: emit when robot connected
     */
    void s_RobotConnected();

    /**
     * @brief s_RobotDisconnected: emit when robot disconnected
     */
    void s_RobotDisconnected();

    /**
     * @brief s_RobotConnectFailed: emit when robot connect fail or lost connect
     */
    void s_RobotConnectFailed();

    /**
     * @brief s_RobotFeedbackUpdate: emit when polling feedback timer timeout
     * @param feedback: parsed feedback data from robot
     */
    // void s_RobotFeedbackUpdate(std::shared_ptr<rb::RbAbstractFeedBack> feedback);

    /**
     * @brief s_RobotModeChanged: emit when robot mode changed
     * @param last_mode: previous mode
     * @param current_mode: current mode
     */
    void s_RobotModeChanged(int last_mode, int current_mode);

    /**
     * @brief s_RobotAtHomePosition: emit when robot at home position or leave home positio
     * @param state: true if robot enter home position, otherwise false
     */
    void s_RobotAtHomePosition(bool state);

    /**
         * @brief s_RobotSqQueueEmpty: emit if sequence table empty when start running sequence mode
         */
    void s_RobotSqQueueEmpty();

    /**
         * @brief s_RobotSqRunStartFail: emit if PLC not connected when start running sequence mode
         */
    void s_RobotSqRunStartFail();

    /**
         * @brief s_RobotSqRunStarted: emit when sequence mode started
         * @param is_continuous: true if running at continuous mode
         */
    void s_RobotSqRunStarted(bool is_continuous);

    /**
         * @brief s_RobotSqRunFinished: emit when sequence mode running completed
         */
    void s_RobotSqRunFinished();

    /**
         * @brief s_RobotSingleCycleStarted: emit before execute first command in the sequence table
         * @param is_continuous: true if running at continuous mode
         */
    void s_RobotSingleCycleStarted(bool is_continuous);

    /**
         * @brief s_RobotSingleCycleFinished: emit when executed the last command in the sequence table
         */
    void s_RobotSingleCycleFinished();

    /**
         * @brief s_RobotForceStopped: emit when sequence mode force stopped
         */
    void s_RobotForceStopped();

    /**
         * @brief s_RobotVisionTrigger: emit for trigger camera get image and process image
         */
    void s_RobotVisionTrigger();

    /**
     * @brief s_RobotStartHoming: emit when robot started homing
     */
    void s_RobotStartHoming();

    /**
     * @brief s_RobotHomingFinished: emit when robot finished homing
     */
    void s_RobotHomingFinished();

    /**
     * @brief s_RobotErrorStop: emit when robot in error mode
     */
    void s_RobotErrorStop();

    /**
     * @brief s_RobotErrorHadReset: emit when robot error had reset
     */
    void s_RobotErrorHadReset();

    /**
         * @brief s_FinishedCommand: emit when robot finished a single command
         * @param index: the index indicates completed command in the sequence table
         */
    void s_FinishedCommand(int index);

    /**
         * @brief s_FinishedCommand: emit when robot finished a single command
         * @param index: the index indicates completed command in the sequence table
         */
    void s_StartExecutingCommand(int index);
};

} // namespace rb

#endif
