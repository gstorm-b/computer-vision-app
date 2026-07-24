#ifndef HUAYAN_TYPES_H
#define HUAYAN_TYPES_H

#include <string>
#include <unordered_map>
#include "robot/robot_point.h"
#include "robot/rbutils.h"

namespace rb {

inline constexpr int HUAYAN_FAST_COMMAND_PORT = 10001;
inline constexpr int HUAYAN_COMMAND_PORT = 10003;
inline constexpr int HUAYAN_JDATA_SHEET_PORT_50 = 10004;
inline constexpr int HUAYAN_JDATA_SHEET_PORT_100 = 10005;
inline constexpr int HUAYAN_JDATA_SHEET_PORT_200 = 10006;
inline constexpr int HUAYAN_SDATA_SHEET_PORT_50 = 10014;
inline constexpr int HUAYAN_SDATA_SHEET_PORT_100 = 10015;
inline constexpr int HUAYAN_SDATA_SHEET_PORT_200 = 10016;
inline constexpr int HUAYAN_MODBUSTCP_PORT = 10502;
inline constexpr int HUAYAN_MQTT_PORT = 1883;

// inline constexpr double HUAYAN_EO5_MAX_REACH = 590.0;

class HuayanMachineState {
public:
    enum mcState {
        UnInitialize = 0,
        Initialze,
        ElectricBoxDisconnect,
        ElectricBoxConnecting,
        EmergencyStopHandling,
        EmergencyStop,
        Blackouting48V,
        Blackout48V,
        Electrifying48V,
        SaftyGuardErrorHandling,
        SaftyGuardError,
        SafetyGuardHandling,
        SaftyGuard,
        ControllerDisconnecting,
        ControllerDisconnect,
        ControllerConnecting,
        ControllerVersionError,
        EtherCATError,
        ControllerChecking,
        Reseting,
        RobotOutOfSafeSpace,
        RobotCollisionStop,
        Error,
        RobotEnabling,
        Disable,
        Moving,
        LongJogMoving,
        RobotStopping,
        RobotDisabling,
        RobotOpeningFreeDriver,
        RobotClosingFreeDriver,
        FreeDriver,
        RobotHolding,
        Standby,
        ScriptRunning,
        ScriptHoldHandling,
        ScriptHolding,
        ScriptStopping,
        ScriptStopped,
        HRAppDisconnected,
        HRAppError,
        RobotLoadIdentify,
        Braking
    };

    class StateWrapper {
    public:
        mcState state;


        StateWrapper() : state(mcState::UnInitialize) {

        }

        StateWrapper(mcState s) : state(s) {

        }

        StateWrapper(int value) {
            if (value >= UnInitialize && value <= Braking) {
                state = static_cast<mcState>(value);
            } else {
                state = UnInitialize;
            }
        }

        std::string GetBrief() const {
            static const std::unordered_map<mcState, std::string> stateToString = {
                {UnInitialize, "Uninitialize"},
                {Initialze, "Initialze"},
                {ElectricBoxDisconnect, "Electric Box Disconnect"},
                {ElectricBoxConnecting, "Electric Box Connecting"},
                {EmergencyStopHandling, "Emergency Stop Handling"},
                {EmergencyStop, "Emergency Stop"},
                {Blackouting48V, "Blackouting 48V"},
                {Blackout48V, "Blackout 48V"},
                {Electrifying48V, "Electrifying 48V"},
                {SaftyGuardErrorHandling, "Safty Guard Error Handling"},
                {SaftyGuardError, "Safty Guard Error"},
                {SafetyGuardHandling, "Safety Guard Handling"},
                {SaftyGuard, "Safty Guard"},
                {ControllerDisconnecting, "Controller Disconnecting"},
                {ControllerDisconnect, "Controller Disconnect"},
                {ControllerConnecting, "Controller Connecting"},
                {ControllerVersionError, "Controller Version Error"},
                {EtherCATError, "EtherCAT Error"},
                {ControllerChecking, "Controller Checking"},
                {Reseting, "Reseting"},
                {RobotOutOfSafeSpace, "Robot Out Of Safespace"},
                {RobotCollisionStop, "Robot Collision Stop"},
                {Error, "Error"},
                {RobotEnabling, "Robot Enabling"},
                {Disable, "Disable"},
                {Moving, "Moving"},
                {LongJogMoving, "Long Jog Moving"},
                {RobotStopping, "Robot Stopping"},
                {RobotDisabling, "Robot Disabling"},
                {RobotOpeningFreeDriver, "Robot Opening Free Driver"},
                {RobotClosingFreeDriver, "Robot Closing Free Driver"},
                {FreeDriver, "Free Driver"},
                {RobotHolding, "Robot Holding"},
                {Standby, "Standby"},
                {ScriptRunning, "Script Running"},
                {ScriptHoldHandling, "Script Hold Handling"},
                {ScriptHolding, "Script Holding"},
                {ScriptStopping, "Script Stopping"},
                {ScriptStopped, "Script Stopped"},
                {HRAppDisconnected, "HRApp Disconnected"},
                {HRAppError, "HRApp Error"},
                {RobotLoadIdentify, "Robot Load Identify"},
                {Braking, "Braking"}
            };

            auto it = stateToString.find(state);
            return it != stateToString.end() ? it->second : "Unknown";
        }

        StateWrapper& operator=(int value) {
            if (value >= UnInitialize && value <= Braking) {
                state = static_cast<mcState>(value);
            } else {
                state = UnInitialize;
            }
            return *this;
        }

        operator int() const {
            return static_cast<int>(state);
        }

        operator std::string() const {
            static const std::unordered_map<mcState, std::string> stateToString = {
                {UnInitialize, "UnInitialize"},
                {Initialze, "Initialze"},
                {ElectricBoxDisconnect, "ElectricBoxDisconnect"},
                {ElectricBoxConnecting, "ElectricBoxConnecting"},
                {EmergencyStopHandling, "EmergencyStopHandling"},
                {EmergencyStop, "EmergencyStop"},
                {Blackouting48V, "Blackouting48V"},
                {Blackout48V, "Blackout48V"},
                {Electrifying48V, "Electrifying48V"},
                {SaftyGuardErrorHandling, "SaftyGuardErrorHandling"},
                {SaftyGuardError, "SaftyGuardError"},
                {SafetyGuardHandling, "SafetyGuardHandling"},
                {SaftyGuard, "SaftyGuard"},
                {ControllerDisconnecting, "ControllerDisconnecting"},
                {ControllerDisconnect, "ControllerDisconnect"},
                {ControllerConnecting, "ControllerConnecting"},
                {ControllerVersionError, "ControllerVersionError"},
                {EtherCATError, "EtherCATError"},
                {ControllerChecking, "ControllerChecking"},
                {Reseting, "Reseting"},
                {RobotOutOfSafeSpace, "RobotOutOfSafeSpace"},
                {RobotCollisionStop, "RobotCollisionStop"},
                {Error, "Error"},
                {RobotEnabling, "RobotEnabling"},
                {Disable, "Disable"},
                {Moving, "Moving"},
                {LongJogMoving, "LongJogMoving"},
                {RobotStopping, "RobotStopping"},
                {RobotDisabling, "RobotDisabling"},
                {RobotOpeningFreeDriver, "RobotOpeningFreeDriver"},
                {RobotClosingFreeDriver, "RobotClosingFreeDriver"},
                {FreeDriver, "FreeDriver"},
                {RobotHolding, "RobotHolding"},
                {Standby, "Standby"},
                {ScriptRunning, "ScriptRunning"},
                {ScriptHoldHandling, "ScriptHoldHandling"},
                {ScriptHolding, "ScriptHolding"},
                {ScriptStopping, "ScriptStopping"},
                {ScriptStopped, "ScriptStopped"},
                {HRAppDisconnected, "HRAppDisconnected"},
                {HRAppError, "HRAppError"},
                {RobotLoadIdentify, "RobotLoadIdentify"},
                {Braking, "Braking"}
            };

            auto it = stateToString.find(state);
            return it != stateToString.end() ? it->second : "Unknown";
        }
    };
};

class HuayanSheet : public RbAbstractFeedBack {
public:
    /// PosAndVel
    // // cartesian position in current user coordinates and tool coordinates
    // double actual_position_cartesian[6];
    // // joint position in current user coordinates and tool coordinates
    // double actual_position_joint[6];
    // // cartesian position in current tool coordinates
    // double acutal_pcs_tcp[6];
    // // cartesian position base on base coordinate system
    // double actual_pcs_base[6];
    // // cartesian coordinate position in the tool coordinate system, no longer provided in the new version
    // double actual_pcs_tool[6];
    // // actual joint speed [°/s]
    // double actual_joint_velocity[6];
    // // acutal joint acceleration [°/s2]
    // double actual_joint_acceleration[6];
    // // joint current [A]
    // double actual_joint_current[6];

    /// PosAndVel
    // cartesian position in current user coordinates and tool coordinates
    CartesianPoint actual_position_cartesian;
    // joint position in current user coordinates and tool coordinates
    JointPoint actual_position_joint;
    // cartesian position in current tool coordinates
    CartesianPoint  acutal_pcs_tcp;
    // cartesian position base on base coordinate system
    CartesianPoint  actual_pcs_base;
    // cartesian coordinate position in the tool coordinate system, no longer provided in the new version
    CartesianPoint  actual_pcs_tool;
    // actual joint speed [°/s]
    double actual_joint_velocity[6];
    // acutal joint acceleration [°/s2]
    double actual_joint_acceleration[6];
    // joint current [A]
    double actual_joint_current[6];
    // speed ratio [0~1]
    double actual_overide;

    /// EndIO
    // end effector digital input
    int EndDI[4];
    // end effector digital output
    int EndDO[4];
    // end effector button
    int EndButton[4];
    // end effector end input analog
    double EndAI[2];
    // end effector enable button
    int EnableEndButton;

    /// ElectricBoxIO
    // Electric box configuration input
    int BoxCI[8];
    // Electric box configuration output
    int BoxCO[8];
    // Electric box digital input
    int BoxDI[8];
    // Electric box digital output
    int BoxDO[8];
    // Conveyor belt position
    double Conveyor;
    // Encoder value
    double Encoder;

    /// ElectricBoxAnalogIO
    // Mode of analog output channel 0, 1 = voltage, 2 = current, 0 = close
    int BoxAnalogOutMode_1;
    // Mode of analog output channel 1, 1 = voltage, 2 = current, 0 = close
    int BoxAnalogOutMode_2;
    // Analog output value of channel 0
    double BoxAnalogOut_1;
    // Analog output value of channel 1
    double BoxAnalogOut_2;
    // Analog intput value of channel 0
    double BoxAnalogIn_1;
    // Analog input value of channel 1
    double BoxAnalogIn_2;

    /// StateAndError
    // robot machine state value from 0 to 42
    int RobotState;
    // HuayanMachineState::mcState RobotState;
    // robot enable state value is 0 or 1
    int RobotEnabled;
    // robot pause state value is 0 or 1
    int RobotPaused;
    // robot moving state value is 0 or 1
    int RobotMoving;
    // robot blending state value is 0 or 1
    int RobotBlendingDone;
    // robot in position state value is 0 or 1
    int InPos;
    // number of error axis
    int ErrorAxisID;
    // error code of axis
    int ErrorCode;
    // is in reduce mode
    int IsReduceMode;
    // is in zero-force teaching mode
    int IsFreeDriveMode;
    // is in auto mode
    int AutoMode;
    // brake status of joint axis, 0 is brake, 1 is release
    int BrakeState[6];
    // axis status
    int nAxisStatus[6];
    // Error code for each axis
    int nAxisErrorCode[6];
    // int nResetSafeSpace[6];
    // int nAxisGroupStatus[6];
    // int nAxisGroupErrorCode[]

    /// MsgTitle
    // json package time stamp
    long long timestamp;
    // json package time
    std::string UpdateTime;
};

}
#endif // HUAYAN_TYPES_H
