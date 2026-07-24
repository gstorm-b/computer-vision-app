#ifndef KW_WAIT_INPUT_H
#define KW_WAIT_INPUT_H

#include "robot/kawasaki/command/kawasaki_command.h"
#include "utils/chronocounter.h"

namespace rb {

class KwWaitInput : public KawasakiCommand {
public:
    KwWaitInput(QString device, QString address, bool state) :
        KawasakiCommand(),
        m_cmd_stage(WaitInputStage::UnExecute),
        m_input_device(device),
        m_input_address(address),
        m_input_state(state) {

    }

    QString commandId() override {
        return "";
    }

    void resetCommand() override {
        m_execute_state = ExecuteState::UnExecuted;
        m_cmd_stage = WaitInputStage::UnExecute;
    }

    void execute() override {
        if (this->m_interface == nullptr) {
            qCritical() << "HyWaitInput m_interface pointer null.";
            m_execute_state = ExecuteState::ExecuteError;
            return;
        }

    _start_point:
        switch (m_cmd_stage) {
        case WaitInputStage::UnExecute:
            m_execute_state = ExecuteState::Executing;
            m_cmd_stage = WaitInputStage::GetWait;
            goto _start_point;
            break;

        case WaitInputStage::GetWait:
            m_address_number = m_input_address.toInt(&is_ok);
            if (!is_ok) {
                m_cmd_stage = WaitInputStage::Error;
                goto _start_point;
            }

            if (m_input_device == "ExternalInput") {
                m_cmd_stage = WaitInputStage::WaitingExternalInput;
            } else if (m_input_device == "ExternalOutput") {
                m_cmd_stage = WaitInputStage::WaitingExternalOutput;
            } else if (m_input_device == "InternalSignal") {
                m_cmd_stage = WaitInputStage::WaitingInternalSignal;
            } else if (m_input_device == "PLC") {
                m_cmd_stage = WaitInputStage::WaitingPLC;
            } else {
                m_cmd_stage = WaitInputStage::Error;
            }
            goto _start_point;
            break;

        case WaitInputStage::WaitingExternalInput:
            if (m_input_state == m_interface->flag.get_external_input_state(m_address_number)) {
                qDebug() << "Kawasaki command WaitInput command done, External Input" << m_address_number << m_input_state;
                m_execute_state = ExecuteState::Executed;
                return;
            }
            break;

        case WaitInputStage::WaitingExternalOutput:
            if (m_input_state == m_interface->flag.get_external_output_state(m_address_number)) {
                qDebug() << "Kawasaki command WaitInput command done, External Output" << m_address_number << m_input_state;
                m_execute_state = ExecuteState::Executed;
                return;
            }
            break;

        case WaitInputStage::WaitingInternalSignal:
            if (m_input_state == m_interface->flag.get_internal_signal_state(m_address_number)) {
                qDebug() << "Kawasaki command WaitInput command done, Internal Signal" << m_address_number << m_input_state;
                m_execute_state = ExecuteState::Executed;
                return;
            }
            break;

        case WaitInputStage::WaitingPLC:
        {
            bool plc_value = false;
            m_interface->flag.plc_comunicator()->Fx3GetMDeviceValue(m_address_number, plc_value);
            if (plc_value == m_input_state) {
                m_execute_state = ExecuteState::Executed;
            }
        }
        break;

        case WaitInputStage::Finish:
            m_execute_state = ExecuteState::Executed;
            break;

        case WaitInputStage::Error:
            m_execute_state = ExecuteState::ExecuteError;
            break;
        }
    }

    std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KwWaitInput>(*this);
    }

private:
    enum WaitInputStage {
        UnExecute = 0,
        GetWait,
        WaitingExternalInput,
        WaitingExternalOutput,
        WaitingInternalSignal,
        WaitingPLC,
        Finish,
        Error
    };

private:
    WaitInputStage m_cmd_stage = WaitInputStage::UnExecute;

    bool is_ok{false};

    QString m_input_device;
    QString m_input_address;
    bool m_input_state;
    int m_address_number;
};

}

#endif // KW_WAIT_INPUT_H
