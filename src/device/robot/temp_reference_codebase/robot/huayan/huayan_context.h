#ifndef HUAYAN_CONTEXT_H
#define HUAYAN_CONTEXT_H

#include <memory>
#include <QString>
#include "robot/robot_point.h"
#include "robot/huayan/huayan_msg_return.h"
#include "plc/fx3communicator.h"

namespace rb {

struct HyMsgSender {
    virtual ~HyMsgSender() = default;
    virtual const bool send_msg(QString& msg) = 0;
    virtual const bool is_response_received() = 0;
    virtual HuayanMsgReturn* retrieve_response() = 0;
};

struct HyMsgReceiver {
    virtual ~HyMsgReceiver() = default;
    virtual const bool is_sending_marked() = 0;
    virtual QString retrieve_sending_msg() = 0;
    virtual void mark_response(HuayanMsgReturn& response) = 0;
};

class HyMsgTransport final : public HyMsgSender, HyMsgReceiver {
public:
    HyMsgTransport();

    /**
     * @brief send_msg: set sending msg and set send flag to true
     * @param mgs
     * @return return false if currently message forbidden to send, otherwise return true
     */
    const bool send_msg(QString &msg) override;

    /**
     * @brief mark_response: set reponse data for receiver and set receive flag to true
     * if input response valid
     * @param response
     */
    void mark_response(HuayanMsgReturn& response) override;

    /**
     * @brief is_sending_marked
     * @return return true if transporter already hold msg
     */
    const bool is_sending_marked() override;

    /**
     * @brief is_response_received
     * @return return true if transpoter already hold response msg
     */
    const bool is_response_received() override;

    /**
     * @brief retrieve_msg: retrieve sending msg, and reset send flag
     * @return return msg
     */
    QString retrieve_sending_msg() override;

    /**
     * @brief retrieve_response: only retrive response if method is_response_received return true
     * @return return response
     */
    HuayanMsgReturn* retrieve_response() override;

    /**
     * @brief resetContext: reset all data inside context
     */
    void resetContext();

    HyMsgSender& asSender() {
        return *this;
    }

    HyMsgReceiver& asReceiver() {
        return *this;
    }

private:
    bool m_flag_send;
    bool m_flag_recv;

    QString m_send_msg;
    HuayanMsgReturn m_response_msg;
};

enum HyInternalFlag {
    HyfNone = 0,
    HyfHomingStart,
    HyfHomingDone,
    HyfStartSqCycle,
    HyfEndSqCycle,
    HyfTriggerVision,
    HyfGetVisionCoordinate,
    HyfJumpToLine
};

struct HyFlagSender {
    virtual ~HyFlagSender() = default;
    virtual const void setInternalFlag(HyInternalFlag flag) = 0;
    virtual const bool is_first_cycle() = 0;
    virtual const int get_robot_id() = 0;
    virtual const QString get_robot_id_str() = 0;
    virtual const QString get_gobal_tcp_name() = 0;
    virtual const QString get_gobal_ucs_name() = 0;
    virtual const void set_flag_start_jog() = 0;
    virtual const bool has_jog_cmd_stop_raised() = 0;
    virtual const bool is_robot_moving() = 0;
    virtual const bool get_enddo_state(int number) = 0;
    virtual const bool get_enddi_state(int number) = 0;
    virtual const bool get_boxdo_state(int number) = 0;
    virtual const bool get_boxdi_state(int number) = 0;
    virtual Fx3Communicator* plc_comunicator() = 0;
    virtual const bool is_vs_coordinate_available() = 0;
    virtual void request_vs_coordinate() = 0;
    virtual void set_auto_request_coordinate(bool enb, int after_index) = 0;
    virtual const CartesianPoint get_vs_coordinate() = 0;
    virtual const bool is_vision_fail() = 0;
    virtual const int get_vs_request_count() = 0;
    virtual const void set_new_current_waypoint(QString id) = 0;
    virtual void setJumpToLine(int number) = 0;
};

struct HyFlagReceiver {
    virtual ~HyFlagReceiver() = default;
    virtual const bool is_internal_flag_raised() = 0;
    virtual const void set_first_cycle_flag(bool state) = 0;
    virtual HyInternalFlag retrieve_internal_flag() = 0;
    virtual void set_robot_id(int id) = 0;
    virtual void set_global_tcp_name(QString name) = 0;
    virtual void set_global_ucs_name(QString name) = 0;
    virtual const bool set_jog_cmd_stop_flag() = 0;
    virtual void set_robot_moving_flag(bool state) = 0;
    virtual void set_enddo_ptr(int *enddo) = 0;
    virtual void set_enddi_ptr(int *enddi) = 0;
    virtual void set_boxdo_ptr(int *boxdo) = 0;
    virtual void set_boxdi_ptr(int *boxdi) = 0;
    virtual void set_plc_communicator_ptr(Fx3Communicator* ptr) = 0;
    virtual const bool is_request_coordinate() = 0;
    virtual const bool is_auto_request_setted(int &index) = 0;
    virtual void set_vs_coordinate(CartesianPoint point) = 0;
    virtual void clear_vs_coordinate() = 0;
    virtual void raise_vision_fail() = 0;
    virtual const bool is_waypointID_changed() = 0;
    virtual const QString retrive_waypointId() = 0;
    virtual const int getJumpToLine() = 0;
};

class HyFlagTransport final : public HyFlagSender, HyFlagReceiver {
public:
    HyFlagTransport();
    HyFlagTransport(int rb_id);

    const void setInternalFlag(HyInternalFlag flag) override;
    const bool is_internal_flag_raised() override;
    HyInternalFlag retrieve_internal_flag() override;

    virtual const bool is_first_cycle() override {
        return m_is_first_cycle;
    }

    virtual const void set_first_cycle_flag(bool state) override {
        m_is_first_cycle = state;
    }

    const int get_robot_id() override {
        return m_robot_id;
    }

    const QString get_robot_id_str() override {
        return m_robot_id_str;
    }

    const QString get_gobal_tcp_name() override {
        return m_global_tcp_name;
    }

    const QString get_gobal_ucs_name() override {
        return m_global_ucs_name;
    }

    /**
     * @brief set_robot_id: set robot id
     * @param id
     */
    void set_robot_id(int id) override;

    void set_global_tcp_name(QString name) override {
        m_global_tcp_name = name;
    }

    void set_global_ucs_name(QString name) override {
        m_global_ucs_name = name;
    }

    /**
     * @brief set_flag_start_jog: reset stop jog flag,
     * to make sure, this method should be called when start jog
     */
    const void set_flag_start_jog() override {
        m_jog_stop_flag = false;
    }

    /**
     * @brief has_jog_cmd_stop_raised: get stop jog command flag
     * when this flag has set, method will return true and reset this flag (work same as retrieve)
     * @return
     */
    const bool has_jog_cmd_stop_raised() override;

    /**
     * @brief set_jog_cmd_stop_flag: set stop jog command flag
     * when this flag already set, method will return false and not change flag state
     * if this flag is reset, method will set this flag and return true
     * @return
     */
    const bool set_jog_cmd_stop_flag() override;

    const bool is_robot_moving() override {
        return m_is_robot_moving;
    }

    void set_robot_moving_flag(bool state) override {
        m_is_robot_moving = state;
    }

    const bool get_enddo_state(int number) override;
    const bool get_enddi_state(int number) override;
    const bool get_boxdo_state(int number) override;
    const bool get_boxdi_state(int number) override;

    void set_enddo_ptr(int *enddo) override;
    void set_enddi_ptr(int *enddi) override;
    void set_boxdo_ptr(int *boxdo) override;
    void set_boxdi_ptr(int *boxdi) override;

    Fx3Communicator* plc_comunicator() override;
    void set_plc_communicator_ptr(Fx3Communicator* ptr) override;

    const bool is_vs_coordinate_available() override;
    void request_vs_coordinate() override;
    const bool is_request_coordinate() override;
    void set_auto_request_coordinate(bool enb, int after_index) override;
    const CartesianPoint get_vs_coordinate() override;
    const bool is_auto_request_setted(int &index) override;
    void set_vs_coordinate(CartesianPoint point) override;
    void clear_vs_coordinate() override;
    const bool is_vision_fail() override;
    const int get_vs_request_count() override;
    void raise_vision_fail() override;

    const void set_new_current_waypoint(QString id) override;
    const bool is_waypointID_changed() override;
    const QString retrive_waypointId() override;

    virtual void setJumpToLine(int number) override;
    virtual const int getJumpToLine() override;

    /**
     * @brief resetContext: reset all data inside context
     */
    void resetContext();

    /**
     * @brief asSender: return a reference instance as sender
     * @return
     */
    HyFlagSender& asSender() {
        return *this;
    }

    /**
     * @brief asReceiver: return instance as receiver
     * @return
     */
    HyFlagReceiver& asReceiver() {
        return *this;
    }

private:
    bool m_is_internal_flag_raised{false};
    bool m_is_first_cycle{false};
    HyInternalFlag m_interal_flag{HyInternalFlag::HyfNone};

    int m_robot_id = 0;
    QString m_robot_id_str = "0";
    QString m_global_tcp_name = "TCP";
    QString m_global_ucs_name = "Base";
    bool m_jog_stop_flag = false;
    bool m_is_robot_moving = false;

    QString m_current_waypoint_id;
    bool m_is_new_waypoint;

    int *m_enddo_ptr{nullptr};
    int *m_enddi_ptr{nullptr};
    int *m_boxdo_ptr{nullptr};
    int *m_boxdi_ptr{nullptr};

    int m_jump_line_number = 0;

    Fx3Communicator *m_plc{nullptr};
    bool m_is_contain_coordinate = false;
    bool m_is_request_coordinate = false;
    bool m_is_auto_request = false;
    int m_auto_request_index = 0;
    bool m_is_vision_fail = false;
    int m_vs_request_count = 0;
    CartesianPoint m_matched_point;
};

}

#endif // HUAYAN_CONTEXT_H
