#include "huayan_msg_return.h"

/**
 * Huayan ROBOT command return format
 *
 * return message end  by [,;]
 *
 * Success return
 * MessageName,OK,Param1,Param2,Param3...ParamN,;
 *
 * Failure return
 * MessageName,Fail,ErrorCode,ErrorExplanation,;
 *
 */


namespace rb {

HuayanMsgReturn::HuayanMsgReturn() :
    m_raw(""),
    m_command_header(""),
    m_valid(false),
    m_return_state(false),
    m_error_code(0),
    m_error_explaination(""),
    m_has_params(false) {

}

HuayanMsgReturn::HuayanMsgReturn(QString response) :
    m_raw(""),
    m_command_header(""),
    m_valid(false),
    m_return_state(false),
    m_error_code(0),
    m_error_explaination(""),
    m_has_params(false) {

    if (response.isEmpty()) {
        return;
    }
    m_raw = response;
    this->ParseResponse();
}

bool HuayanMsgReturn::isValid() {
    return m_valid;
}

bool HuayanMsgReturn::isCommandOK() {
    return m_return_state;
}

QString HuayanMsgReturn::GetRawResponse() {
    return m_raw;
}

QString HuayanMsgReturn::GetCommandHeader() {
    return m_command_header;
}

bool HuayanMsgReturn::hasParams() {
    return m_has_params;
}

QStringList HuayanMsgReturn::GetParams() {
    return m_params;
}

int HuayanMsgReturn::ErrorCode() {
    return m_error_code;
}

QString HuayanMsgReturn::GetErrorExplain() {
    return m_error_explaination;
}

void HuayanMsgReturn::ParseResponse() {

    // find end mark
    // if there's no end mark or there are more than 1 end mark
    // end mark only valid at the end of msg
    // => stop parse, set response invalid
    int end_index = m_raw.indexOf(",;");
    if (end_index != m_raw.size() - 2) {
        m_valid = false;
        return;
    }

    QString parse_temp = m_raw;
    parse_temp.remove(end_index, 2);
    m_params = parse_temp.split(',');

    // find command header and return status
    // if after split (comma ',') less than 2 element
    // => reponse msg invalid
    if (m_params.size() < 2) {
        m_valid = false;
        return;
    }

    m_command_header = m_params[0];
    QString command_state = m_params[1];
    m_params.remove(0, 2);

    // IF after split command header and command return state
    // still have elements => have return parameters
    if (command_state == "OK") {
        m_return_state = true;
        if (m_params.size() > 0) {
            // return msg contain params
            m_has_params = true;
        }
        m_valid = true;
    } else if (command_state == "Fail") {
        // MessageName,Fail,ErrorCode,ErrorExplanation,;
        // if there no error code and error explanation
        // or more than 2 pharse
        // => return msg invalid
        if (m_params.size() != 2) {
            m_valid = false;
            return;
        }

        m_return_state = false;
        m_error_code = m_params[0].toInt(0);
        m_error_explaination = m_params[1];
        m_valid = true;
    } else {
        m_valid = false;
        return;
    }
}

}
