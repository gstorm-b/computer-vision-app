#ifndef HUAYAN_MSG_RETURN_H
#define HUAYAN_MSG_RETURN_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QDebug>

namespace rb {

class HuayanMsgReturn {
public:
    HuayanMsgReturn();
    HuayanMsgReturn(QString response);

    bool isValid();
    bool isCommandOK();
    QString GetRawResponse();
    QString GetCommandHeader();
    bool hasParams();
    QStringList GetParams();
    int ErrorCode();
    QString GetErrorExplain();

private:
    void ParseResponse();

private:
    QString m_raw;
    QString m_command_header;

    bool m_valid;
    bool m_return_state;

    int m_error_code;
    QString m_error_explaination;

    bool m_has_params;
    QStringList m_params;
};

}

#endif // HUAYAN_MSG_RETURN_H
