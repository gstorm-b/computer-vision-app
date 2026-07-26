#ifndef LAMP_BUTTON_H
#define LAMP_BUTTON_H

#include <QPushButton>
#include <QStyle>

/// Push button that acts as a status "lamp": exposes a Status enum and reflects it as a
/// dynamic "status" property so a stylesheet can color/decorate the button per state.
class LampButton : public QPushButton {
    Q_OBJECT
public:
    /// Connection/operation states the lamp can display.
    enum Status {
        Disconnected,  ///< No connection established.
        Connected,     ///< Successfully connected.
        ConnectFail,   ///< The last connection attempt failed.
        Operation,     ///< Actively performing an operation.
        Error          ///< An error condition.
    };

    /// Constructs the button in the Disconnected state and applies the matching style.
    explicit LampButton(QWidget* parent = nullptr) :
        QPushButton(parent),
        m_status(Disconnected) {
        updateStyle();
    }

    /// Updates the lamp state and re-applies the style if `status` actually changed.
    void setStatus(Status status) {
        if (m_status != status) {
            m_status = status;
            updateStyle();
        }
    }

    /// Returns the current lamp status.
    Status status() const { return m_status; }

private:
    Status m_status;  ///< Current lamp status, mirrored into the "status" dynamic property.

    /// Maps m_status to the "status" dynamic property (used by the stylesheet), then
    /// forces the style to unpolish/re-polish and repaints the button.
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
