#ifndef HUAYAN_FEEDBACK_PORT_H
#define HUAYAN_FEEDBACK_PORT_H

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValueConstRef>
#include <QJsonValue>

#include "utils/chronocounter.h"
// #include <chrono>
#include "robot/robot_tcp_client.h"
#include "robot/huayan/huayan_types.h"

#define HUAYAN_MAX_END_DI 8
#define HUAYAN_MAX_END_DO 8

namespace rb {

class HuayanFeedBackPort : public TCPClient {
public:
    HuayanFeedBackPort();

    inline bool IsDataHasRead() const;
    const HuayanSheet& GetFeedbackData();
    bool polling();
    bool retrieveReadStatus();
    QString robotStateToQString();

private:
    inline double jstringToDouble(const QJsonValue &jvalue, double default_value);
    inline int jstringToInt(const QJsonValue &jvalue, int default_value);
    inline int charToUint(char *pBuffer);
    inline void parseData(QByteArray &rawBytes);

private:
    const char header[4] = {'L', 'T', 'B', 'R'};
    const int headerValue = charToUint((char *)header);

    bool m_isDataHasRead = false;
    HuayanSheet m_feedbackData;
    bool m_read_timeout;
    ChronoCounter m_read_time_counter;
    int m_wait_timeout{500};
};

}
#endif // HUAYAN_FEEDBACK_PORT_H
