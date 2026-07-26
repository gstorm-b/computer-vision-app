#include "no_wheel_combobox.h"

/// Constructs the combo box with selection interaction enabled by default; wheel events
/// are always ignored regardless of this setting.
NoWheelComboBox::NoWheelComboBox(QWidget *parent) : QComboBox(parent), isAllowSelection(true) {

}

/// Enables or disables mouse/keyboard interaction (press, release, key press/release);
/// wheel events remain ignored either way.
/// @param enable true to allow interaction, false to swallow it
void NoWheelComboBox::setAllowSelection(bool enable) {
    isAllowSelection = enable;
}

/// Applies `color` to the widget's background palette role.
/// @param color the color to apply; the call is a no-op if it is invalid
void NoWheelComboBox::setBackGroundColor(QColor color) {
    if (!color.isValid()) {
        return;
    }

    QPalette cbb_pallete = this->palette();
    cbb_pallete.setColor(this->backgroundRole(), color);
    this->setPalette(cbb_pallete);
}

/// Ignores the event so mouse-wheel scrolling never changes the current selection.
void NoWheelComboBox::wheelEvent(QWheelEvent *event) {
    event->ignore();
}

/// Emits AboutToShow() and forwards to QComboBox::mousePressEvent() when selection is
/// allowed; otherwise swallows the event.
void NoWheelComboBox::mousePressEvent(QMouseEvent *event) {
    if (!isAllowSelection) {
        return;
    }

    emit AboutToShow();
    QComboBox::mousePressEvent(event);
}

/// Forwards to QComboBox::mouseReleaseEvent() when selection is allowed; otherwise
/// swallows the event.
void NoWheelComboBox::mouseReleaseEvent(QMouseEvent *event) {
    if (!isAllowSelection) {
        return;
    }

    QComboBox::mouseReleaseEvent(event);
}

/// Forwards to QComboBox::keyPressEvent() when selection is allowed; otherwise swallows
/// the event.
void NoWheelComboBox::keyPressEvent(QKeyEvent *event) {
    if (!isAllowSelection) {
        return;
    }

    QComboBox::keyPressEvent(event);
}

/// Forwards to QComboBox::keyReleaseEvent() when selection is allowed; otherwise
/// swallows the event.
void NoWheelComboBox::keyReleaseEvent(QKeyEvent *event) {
    if (!isAllowSelection) {
        return;
    }

    QComboBox::keyReleaseEvent(event);
}
