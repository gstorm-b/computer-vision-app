#ifndef HY_GET_COORDINATE_H
#define HY_GET_COORDINATE_H

#include "robot/huayan/huayan_command.h"

namespace rb {

class HyGetCoordinate : public HuayanCommand {
public:
    HyGetCoordinate(bool isImmediate, bool isNotWait, int idx) :
        HuayanCommand(),
        m_coor_exe_state(HyCoorState::Init),
        m_reset_index(idx),
        m_immediate(isImmediate),
        m_const_immediate(isImmediate),
        m_is_not_waiting(isNotWait) {

    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_coor_exe_state = HyCoorState::Init;
        m_immediate = m_const_immediate;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HyGetCoordinate m_interface pointer null.";
            return;
        }

        _start_point:
        switch (m_coor_exe_state) {
        case HyCoorState::Init:
            if (m_immediate) {
                m_interface->flag.request_vs_coordinate();
                m_coor_exe_state = HyCoorState::Waiting;
                qInfo() << "HyGetCoordinate request vision matching immediately";
                break;
            }

            m_interface->flag.set_auto_request_coordinate(true, m_reset_index);
            qInfo() << "HyGetCoordinate set auto request after index:" << m_reset_index;

            if (m_interface->flag.is_vs_coordinate_available()) {
                m_coor_exe_state = HyCoorState::Finish;
                qInfo() << "HyGetCoordinate auto request setted, new coordinate holding, finish this command.";
                goto _start_point;
            } else {
                m_interface->flag.request_vs_coordinate();
                m_coor_exe_state = HyCoorState::Waiting;
                qInfo() << "HyGetCoordinate auto request setted, no coordinate holding, trigger and wait.";
            }
            break;

        case HyCoorState::Request:
            // do nothing
            break;

        case HyCoorState::Waiting:
            if (m_interface->flag.is_vs_coordinate_available()) {
                m_coor_exe_state = HyCoorState::Finish;
                goto _start_point;
            }
            break;

        case HyCoorState::Finish:
            m_execute_state = ExecuteState::Executed;
            break;
        }

        // switch (m_coor_exe_state) {
        // case HyCoorState::Init:
        //     if (m_immediate) {
        //         m_coor_exe_state = HyCoorState::Request;
        //         qInfo() << "HyGetCoordinate request vision matching immediately";
        //         goto _start_point;
        //     }

        //     if (m_interface->flag.get_vs_request_count() == 0) {
        //         qInfo() << "Reset vision request counter";
        //         HyGetCoordinate::request_static_count = 1;
        //         m_interface->flag.set_auto_request_coordinate(true, m_reset_index);
        //         m_coor_exe_state = HyCoorState::Request;
        //         m_immediate = true;
        //         m_is_not_waiting = false;
        //         qInfo() << "HyGetCoordinate set auto request after index:" << m_reset_index;
        //         goto _start_point;
        //     }

        //     HyGetCoordinate::request_static_count += 1;
        //     if (HyGetCoordinate::request_static_count <= m_interface->flag.get_vs_request_count()) {
        //         if (m_interface->flag.is_vs_coordinate_available()) {
        //             m_coor_exe_state = HyCoorState::Finish;
        //             qInfo() << "HyGetCoordinate auto request setted, new coordinate holding, finish this command.";
        //             goto _start_point;
        //         } else {
        //             m_coor_exe_state = HyCoorState::Waiting;
        //             qInfo() << "HyGetCoordinate auto request setted, no coordinate holding, trigger and wait.";
        //         }
        //     }

        //     HyGetCoordinate::request_static_count = m_interface->flag.get_vs_request_count() + 1;
        //     qInfo() << "Vision have not contain coordinate, request get coordinate";
        //     m_coor_exe_state = HyCoorState::Request;
        //     m_immediate = true;
        //     m_is_not_waiting = false;
        //     goto _start_point;

        //     break;

        // case HyCoorState::Request:
        //     m_interface->flag.request_vs_coordinate();
        //     if (m_immediate && m_is_not_waiting) {
        //         m_coor_exe_state = HyCoorState::Finish;
        //     } else {
        //         m_coor_exe_state = HyCoorState::Waiting;
        //     }
        //     break;

        // case HyCoorState::Waiting:
        //     if (m_interface->flag.is_vs_coordinate_available()) {
        //         m_coor_exe_state = HyCoorState::Finish;
        //         goto _start_point;
        //     }

        //     // if (m_interface->flag.is_vision_fail() {

        //     // }
        //     break;

        // case HyCoorState::Finish:
        //     m_execute_state = ExecuteState::Executed;
        //     break;
        // }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<HyGetCoordinate>(*this);
    }

private:
    enum HyCoorState {
        Init = 0,
        Request,
        Waiting,
        Finish
    };

    HyCoorState m_coor_exe_state = HyCoorState::Init;
    int m_reset_index;
    bool m_immediate;
    bool m_const_immediate;
    bool m_is_not_waiting;

    static int request_static_count;
};

}

#endif // HY_GET_COORDINATE_H
