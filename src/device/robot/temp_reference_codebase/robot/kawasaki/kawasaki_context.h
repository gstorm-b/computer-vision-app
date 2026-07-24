#ifndef KAWASAKI_CONTEXT_H
#define KAWASAKI_CONTEXT_H

#include <memory>
#include <QString>
#include "robot/robot_point.h"
#include "robot/kawasaki/kawasaki_motion.h"
#include "plc/fx3communicator.h"

namespace rb {

struct KwMsgSender {
    virtual ~KwMsgSender() = default;
    virtual const bool send_msg(QString& msg) = 0;
    virtual const bool is_response_received() = 0;
    virtual KawasakiMsgReturn* retrieve_response() = 0;
};

struct KwMsgReceiver {
    virtual ~KwMsgReceiver() = default;
    virtual const bool is_sending_marked() = 0;
    virtual QString retrieve_sending_msg() = 0;
    virtual void mark_response(KawasakiMsgReturn& response) = 0;
};

class KwMsgTransport final : public KwMsgSender, KwMsgReceiver {
public:
    KwMsgTransport() :
        m_flag_send(false),
        m_flag_recv(false),
        m_send_msg(""),
        m_response_msg() {

    }

    /**
     * @brief send_msg: set sending msg and set send flag to true
     * @param mgs
     * @return return false if currently message forbidden to send, otherwise return true
     */
    const bool send_msg(QString &msg) override {
        if (m_flag_send == true) {
            return false;
        }

        m_send_msg = msg;
        m_flag_send = true;
        return true;
    }

    /**
     * @brief mark_response: set reponse data for receiver and set receive flag to true
     * if input response valid
     * @param response
     */
    void mark_response(KawasakiMsgReturn& response) override {
        // TODO: considering overwrite reponse even have not retrieve
        if (response.isValid()) {
            m_flag_recv = true;
            m_response_msg = response;
        }
    }

    /**
     * @brief is_sending_marked
     * @return return true if transporter already hold msg
     */
    const bool is_sending_marked() override {
        return m_flag_send;
    }

    /**
     * @brief is_response_received
     * @return return true if transpoter already hold response msg
     */
    const bool is_response_received() override {
        return m_flag_recv;
    }

    /**
     * @brief retrieve_msg: retrieve sending msg, and reset send flag
     * @return return msg
     */
    QString retrieve_sending_msg() override {
        m_flag_send = false;
        return m_send_msg;
    }

    /**
     * @brief retrieve_response: only retrive response if method is_response_received return true
     * @return return response
     */
    KawasakiMsgReturn* retrieve_response() override {
        if (m_flag_recv) {
            m_flag_recv = false;
            return &m_response_msg;
        }
        return nullptr;
    }

    /**
     * @brief resetContext: reset all data inside context
     */
    void resetContext() {
        m_flag_recv = false;
        m_flag_send = false;
    }

    KwMsgSender& asSender() {
        return *this;
    }

    KwMsgReceiver& asReceiver() {
        return *this;
    }

private:
    bool m_flag_send;
    bool m_flag_recv;

    QString m_send_msg;
    KawasakiMsgReturn m_response_msg;
};

enum KwInternalFlag {
    KwfNone = 0,
    KwfHomingStart,
    KwfHomingDone,
    KwfStartSqCycle,
    KwfEndSqCycle,
    KwfTriggerVision,
    KwfGetVisionCoordinate
};


struct KwFlagSender {
    virtual ~KwFlagSender() = default;
    virtual const void setInternalFlag(KwInternalFlag flag) = 0;
    virtual const bool is_first_cycle() = 0;

    virtual const bool get_external_input_state(int number) = 0;
    virtual const bool get_external_output_state(int number) = 0;
    virtual const bool get_internal_signal_state(int number) = 0;

    virtual Fx3Communicator* plc_comunicator() = 0;

    virtual const bool is_vs_coordinate_available() = 0;
    virtual void request_vs_coordinate() = 0;
    virtual void set_auto_request_coordinate(bool enb, int after_index) = 0;
    virtual const CartesianPoint get_vs_coordinate() = 0;
    virtual const bool is_vision_fail() = 0;
    virtual const int get_vs_request_count() = 0;
};

struct KwFlagReceiver {
    virtual ~KwFlagReceiver() = default;
    virtual const bool is_internal_flag_raised() = 0;
    virtual const void set_first_cycle_flag(bool state) = 0;
    virtual KwInternalFlag retrieve_internal_flag() = 0;

    virtual void set_external_input_ptr(int *ptr) = 0;
    virtual void set_external_output_ptr(int *ptr) = 0;
    virtual void set_internal_signal_ptr(int *ptr) = 0;

    virtual void set_plc_communicator_ptr(Fx3Communicator* ptr) = 0;

    virtual const bool is_request_coordinate() = 0;
    virtual const bool is_auto_request_setted(int &index) = 0;
    virtual void set_vs_coordinate(CartesianPoint point) = 0;
    virtual void clear_vs_coordinate() = 0;
    virtual void raise_vision_fail() = 0;
};

class KwFlagTransport final : public KwFlagSender, KwFlagReceiver {
public:
    KwFlagTransport() {

    }

    const void setInternalFlag(KwInternalFlag flag) override {
        m_is_internal_flag_raised = true;
        m_interal_flag = flag;
    }

    const bool is_internal_flag_raised() override {
        return m_is_internal_flag_raised;
    }

    KwInternalFlag retrieve_internal_flag() override {
        m_is_internal_flag_raised = false;
        return m_interal_flag;
    }

    virtual const bool is_first_cycle() override {
        return m_is_first_cycle;
    }

    virtual const void set_first_cycle_flag(bool state) override {
        m_is_first_cycle = state;
    }

    const bool get_external_input_state(int number) override {
        if ((number>=0) && (number<32)) {
            return (m_external_input_ptr[number]==0) ? false : true;
        }
        return false;
    }

    const bool get_external_output_state(int number) override {
        if ((number>=0) && (number<32)) {
            return (m_external_output_ptr[number]==0) ? false : true;
        }
        return false;
    }

    const bool get_internal_signal_state(int number) override {
        if ((number>=0) && (number<32)) {
            return (m_internal_signal_ptr[number]==0) ? false : true;
        }
        return false;
    }

    void set_external_input_ptr(int *ptr) override {
        if (ptr!=nullptr) {
            m_external_input_ptr = ptr;
        }
    }

    void set_external_output_ptr(int *ptr) override {
        if (ptr!=nullptr) {
            m_external_output_ptr = ptr;
        }
    }

    void set_internal_signal_ptr(int *ptr) override {
        if (ptr!=nullptr) {
            m_internal_signal_ptr = ptr;
        }
    }

    Fx3Communicator* plc_comunicator() override {
        return m_plc;
    }

    void set_plc_communicator_ptr(Fx3Communicator* ptr) override {
        if (ptr!=nullptr) {
            m_plc = ptr;
        }
    }

    const bool is_vs_coordinate_available() override {
        return m_is_contain_coordinate;
    }

    void request_vs_coordinate() override {
        m_is_request_coordinate = true;
        m_is_contain_coordinate = false;
        m_vs_request_count += 1;
    }

    const bool is_request_coordinate() override {
        if (m_is_request_coordinate) {
            m_is_request_coordinate = false;
            return true;
        }
        return false;
    }

    void set_auto_request_coordinate(bool enb, int after_index) override {
        m_is_auto_request = enb;
        m_auto_request_index = after_index;
    }

    const CartesianPoint get_vs_coordinate() override {
        return m_matched_point;
    }

    const bool is_auto_request_setted(int &index) override {
        index = m_auto_request_index;
        return m_is_auto_request;
    }

    void set_vs_coordinate(CartesianPoint point) override  {
        m_is_contain_coordinate = true;
        m_matched_point = point;
    }

    void clear_vs_coordinate() override {
        m_is_contain_coordinate = false;
    }

    const bool is_vision_fail() override {
        if (m_is_vision_fail) {
            m_is_vision_fail = false;
            return true;
        }
        return false;
    }

    const int get_vs_request_count() override {
        return m_vs_request_count;
    }

    void raise_vision_fail() override {
        m_is_vision_fail = true;
    }

    /**
         * @brief resetContext: reset all data inside context
         */
    void resetContext() {
        m_is_internal_flag_raised = false;
        m_is_first_cycle = false;
        m_interal_flag = KwInternalFlag::KwfNone;

        m_is_contain_coordinate = false;
        m_is_request_coordinate = false;
        m_is_auto_request = false;
        m_auto_request_index = 0;
        m_matched_point = CartesianPoint();
        m_is_vision_fail = false;
        m_vs_request_count = 0;
    }

    /**
         * @brief asSender: return a reference instance as sender
         * @return
         */
    KwFlagSender& asSender() {
        return *this;
    }

    /**
         * @brief asReceiver: return instance as receiver
         * @return
         */
    KwFlagReceiver& asReceiver() {
        return *this;
    }

private:
    bool m_is_internal_flag_raised{false};
    bool m_is_first_cycle{false};
    KwInternalFlag m_interal_flag{KwInternalFlag::KwfNone};

    int *m_external_input_ptr{nullptr};
    int *m_external_output_ptr{nullptr};
    int *m_internal_signal_ptr{nullptr};

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

#endif // KAWASAKI_CONTEXT_H
