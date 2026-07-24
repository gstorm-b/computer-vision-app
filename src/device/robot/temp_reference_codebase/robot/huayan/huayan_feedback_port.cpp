#include "huayan_feedback_port.h"

namespace rb {

HuayanFeedBackPort::HuayanFeedBackPort() : TCPClient() {
    m_read_timeout = false;
    m_read_time_counter.StopTimeCounter();
    m_wait_timeout = 500;
}

inline bool HuayanFeedBackPort::IsDataHasRead() const {
    return m_isDataHasRead;
}

const HuayanSheet& HuayanFeedBackPort::GetFeedbackData() {
    return m_feedbackData;
}

bool HuayanFeedBackPort::polling() {
    if (this->m_socket == nullptr) {
        return false;
    }

    m_read_timeout = this->m_socket->waitForReadyRead(60);
    if (!m_read_timeout) {
        return !(m_read_time_counter.StartTimeCounter(m_wait_timeout));
    }
    m_read_time_counter.StopTimeCounter();

    // read all byte available on buffer
    QByteArray raw = this->m_socket->readAll();
    QByteArray dataSheet;
    if (raw.isEmpty()) {
        return true;
    }

    char *pBuffer = raw.data();
    // get newest data, so check from tail to head
    for (int index = raw.size() - 13; index >= 0; index--) {
        int headerCheck = charToUint(pBuffer + index);
        if (headerCheck == headerValue) {
            int total_Size = charToUint(pBuffer + index + 4);
            if ((index + total_Size) > raw.size()) {
                continue;
            }
            int data_Size = charToUint(pBuffer + index + 8);
            // just for sure received correct format
            if ((total_Size - data_Size) == 12) {
                dataSheet = raw.mid(index + 12, data_Size);
                parseData(dataSheet);
                break;
            }
        }
    }

    return true;
}

bool HuayanFeedBackPort::retrieveReadStatus() {
    bool status = m_isDataHasRead;
    m_isDataHasRead = false;
    return status;
}

QString HuayanFeedBackPort::robotStateToQString() {
    HuayanMachineState::StateWrapper state(m_feedbackData.RobotState);
    return QString::fromStdString(state.GetBrief());
}

inline double HuayanFeedBackPort::jstringToDouble(const  QJsonValue &jvalue, double default_value) {
    bool isOk = false;
    double ret_value = jvalue.toString().toDouble(&isOk);
    return (isOk) ? ret_value : default_value;
}

inline int HuayanFeedBackPort::jstringToInt(const QJsonValue &jvalue, int default_value) {
    bool isOk = false;
    int ret_value = jvalue.toString().toInt(&isOk);
    return (isOk) ? ret_value : default_value;
}

inline int HuayanFeedBackPort::charToUint(char *pBuffer) {
    int value = pBuffer[3] & 0xFF;
    value <<= 8;
    value |= pBuffer[2] & 0xFF;
    value <<= 8;
    value |= pBuffer[1] & 0xFF;
    value <<= 8;
    value |= pBuffer[0] & 0xFF;
    return value;
}

inline void  HuayanFeedBackPort::parseData(QByteArray &rawBytes) {
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(rawBytes);
    const QJsonValue PosAndVel = jsonDoc["PosAndVel"];
    const QJsonValue EndIO = jsonDoc["EndIO"];
    const QJsonValue ElectricBoxIO = jsonDoc["ElectricBoxIO"];
    const QJsonValue StateAndError = jsonDoc["StateAndError"];

    /// Note: consider type of QJsonValue
    /// huayan pass double value inside " mark

    /// parse position and velocity
    // speed ratio
    m_feedbackData.actual_overide = PosAndVel["Actual_Override"][0].toDouble(.0f);
    // actual cartesian position
    m_feedbackData.actual_position_cartesian.setX(
        jstringToDouble(PosAndVel["Actual_Position"][6], .0f));
    m_feedbackData.actual_position_cartesian.setY(
        jstringToDouble(PosAndVel["Actual_Position"][7], .0f));
    m_feedbackData.actual_position_cartesian.setZ(
        jstringToDouble(PosAndVel["Actual_Position"][8], .0f));
    m_feedbackData.actual_position_cartesian.setRx(
        jstringToDouble(PosAndVel["Actual_Position"][9], .0f));
    m_feedbackData.actual_position_cartesian.setRy(
        jstringToDouble(PosAndVel["Actual_Position"][10], .0f));
    m_feedbackData.actual_position_cartesian.setRz(
        jstringToDouble(PosAndVel["Actual_Position"][11], .0f));
    // actual joint position
    m_feedbackData.actual_position_joint.setJ1(
        jstringToDouble(PosAndVel["Actual_Position"][0], .0f));
    m_feedbackData.actual_position_joint.setJ2(
        jstringToDouble(PosAndVel["Actual_Position"][1], .0f));
    m_feedbackData.actual_position_joint.setJ3(
        jstringToDouble(PosAndVel["Actual_Position"][2], .0f));
    m_feedbackData.actual_position_joint.setJ4(
        jstringToDouble(PosAndVel["Actual_Position"][3], .0f));
    m_feedbackData.actual_position_joint.setJ5(
        jstringToDouble(PosAndVel["Actual_Position"][4], .0f));
    m_feedbackData.actual_position_joint.setJ6(
        jstringToDouble(PosAndVel["Actual_Position"][5], .0f));
    // acutal speed override
    m_feedbackData.actual_overide = jstringToDouble(PosAndVel["Actual_Override"], .0f);
    /// State and error
    m_feedbackData.RobotState = StateAndError["robotState"].toInt();
    m_feedbackData.RobotEnabled = StateAndError["robotEnabled"].toInt();
    m_feedbackData.RobotPaused = StateAndError["robotPaused"].toInt();
    m_feedbackData.RobotMoving = StateAndError["robotMoving"].toInt();
    m_feedbackData.RobotBlendingDone = StateAndError["robotBlendingDone"].toInt();
    m_feedbackData.InPos = StateAndError["InPos"].toInt();
    m_feedbackData.ErrorAxisID = StateAndError["Error_AxisID"].toInt();
    m_feedbackData.ErrorCode = StateAndError["Error_Code"].toInt();
    m_feedbackData.IsReduceMode = StateAndError["IsReduceMode"].toInt();
    m_feedbackData.IsFreeDriveMode = StateAndError["IsFreeDriveMode"].toInt();
    // error list

    m_feedbackData.EndDI[0] = EndIO["EndDI"][0].toInt();
    m_feedbackData.EndDI[1] = EndIO["EndDI"][1].toInt();
    m_feedbackData.EndDI[2] = EndIO["EndDI"][2].toInt();
    // m_feedbackData.EndDI[3] = EndIO["EndDI"][3].toInt();
    m_feedbackData.EndDO[0] = EndIO["EndDO"][0].toInt();
    m_feedbackData.EndDO[1] = EndIO["EndDO"][1].toInt();
    m_feedbackData.EndDO[2] = EndIO["EndDO"][2].toInt();
    // m_feedbackData.EndDO[3] = EndIO["EndDO"][3].toInt();
    m_feedbackData.EndButton[0] = EndIO["EndButton"][0].toInt();
    m_feedbackData.EndButton[1] = EndIO["EndButton"][1].toInt();
    m_feedbackData.EndButton[2] = EndIO["EndButton"][2].toInt();
    m_feedbackData.EndButton[3] = EndIO["EndButton"][3].toInt();
    m_feedbackData.EndAI[0] = jstringToDouble(EndIO["EndAI"][0], .0f);
    m_feedbackData.EndAI[1] = jstringToDouble(EndIO["EndAI"][1], .0f);
    m_feedbackData.EnableEndButton = EndIO["EnableEndBTN"].toInt();

    m_feedbackData.BoxDI[0] = ElectricBoxIO["BoxDI"][0].toInt();
    m_feedbackData.BoxDI[1] = ElectricBoxIO["BoxDI"][1].toInt();
    m_feedbackData.BoxDI[2] = ElectricBoxIO["BoxDI"][2].toInt();
    m_feedbackData.BoxDI[3] = ElectricBoxIO["BoxDI"][3].toInt();
    m_feedbackData.BoxDI[4] = ElectricBoxIO["BoxDI"][4].toInt();
    m_feedbackData.BoxDI[5] = ElectricBoxIO["BoxDI"][5].toInt();
    m_feedbackData.BoxDI[6] = ElectricBoxIO["BoxDI"][6].toInt();
    m_feedbackData.BoxDI[7] = ElectricBoxIO["BoxDI"][7].toInt();

    m_feedbackData.BoxDO[0] = ElectricBoxIO["BoxDO"][0].toInt();
    m_feedbackData.BoxDO[1] = ElectricBoxIO["BoxDO"][1].toInt();
    m_feedbackData.BoxDO[2] = ElectricBoxIO["BoxDO"][2].toInt();
    m_feedbackData.BoxDO[3] = ElectricBoxIO["BoxDO"][3].toInt();
    m_feedbackData.BoxDO[4] = ElectricBoxIO["BoxDO"][4].toInt();
    m_feedbackData.BoxDO[5] = ElectricBoxIO["BoxDO"][5].toInt();
    m_feedbackData.BoxDO[6] = ElectricBoxIO["BoxDO"][6].toInt();
    m_feedbackData.BoxDO[7] = ElectricBoxIO["BoxDO"][7].toInt();

    m_feedbackData.BoxCI[0] = ElectricBoxIO["BoxCI"][0].toInt();
    m_feedbackData.BoxCI[1] = ElectricBoxIO["BoxCI"][1].toInt();
    m_feedbackData.BoxCI[2] = ElectricBoxIO["BoxCI"][2].toInt();
    m_feedbackData.BoxCI[3] = ElectricBoxIO["BoxCI"][3].toInt();
    m_feedbackData.BoxCI[4] = ElectricBoxIO["BoxCI"][4].toInt();
    m_feedbackData.BoxCI[5] = ElectricBoxIO["BoxCI"][5].toInt();
    m_feedbackData.BoxCI[6] = ElectricBoxIO["BoxCI"][6].toInt();
    m_feedbackData.BoxCI[7] = ElectricBoxIO["BoxCI"][7].toInt();

    m_feedbackData.BoxCO[0] = ElectricBoxIO["BoxCO"][0].toInt();
    m_feedbackData.BoxCO[1] = ElectricBoxIO["BoxCO"][1].toInt();
    m_feedbackData.BoxCO[2] = ElectricBoxIO["BoxCO"][2].toInt();
    m_feedbackData.BoxCO[3] = ElectricBoxIO["BoxCO"][3].toInt();
    m_feedbackData.BoxCO[4] = ElectricBoxIO["BoxCO"][4].toInt();
    m_feedbackData.BoxCO[5] = ElectricBoxIO["BoxCO"][5].toInt();
    m_feedbackData.BoxCO[6] = ElectricBoxIO["BoxCO"][6].toInt();
    m_feedbackData.BoxCO[7] = ElectricBoxIO["BoxCO"][7].toInt();

    // mark data package updated
    m_isDataHasRead = true;
}

}
