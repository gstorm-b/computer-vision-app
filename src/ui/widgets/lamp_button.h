#ifndef LAMP_BUTTON_H
#define LAMP_BUTTON_H

#include <QPushButton>
#include <QStyle>

class LampButton : public QPushButton {
    Q_OBJECT
public:
    enum Status {
        Disconnected,
        Connected,
        ConnectFail,
        Operation,
        Error
    };

    explicit LampButton(QWidget* parent = nullptr) :
        QPushButton(parent),
        m_status(Disconnected) {
        updateStyle();
    }

    void setStatus(Status status) {
        if (m_status != status) {
            m_status = status;
            updateStyle();
        }
    }

    Status status() const { return m_status; }

private:
    Status m_status;

    void updateStyle() {
        switch (m_status) {
        case Disconnected:
            setProperty("status", "disconnected");
            break;
        case Connected:
            setProperty("status", "connected");
            break;
        case Operation:
            setProperty("status", "operation");
            break;
        case Error:
            setProperty("status", "error");
            break;
        case ConnectFail:
            setProperty("status", "connect_fail");
            break;
        }
        style()->unpolish(this);
        style()->polish(this);
        update();
    }
};

#endif // LAMP_BUTTON_H
