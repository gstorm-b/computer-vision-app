#include "no_wheel_combobox.h"

NoWheelComboBox::NoWheelComboBox(QWidget *parent) : QComboBox(parent), isAllowSelection(true) {

}

void NoWheelComboBox::setAllowSelection(bool enable) {
    isAllowSelection = enable;
}

void NoWheelComboBox::setBackGroundColor(QColor color) {
    if (!color.isValid()) {
        return;
    }

    QPalette cbb_pallete = this->palette();
    cbb_pallete.setColor(this->backgroundRole(), color);
    this->setPalette(cbb_pallete);
}

void NoWheelComboBox::wheelEvent(QWheelEvent *event) {
    event->ignore();
}

void NoWheelComboBox::mousePressEvent(QMouseEvent *event) {
    if (!isAllowSelection) {
        return;
    }

    emit AboutToShow();
    QComboBox::mousePressEvent(event);
}

void NoWheelComboBox::mouseReleaseEvent(QMouseEvent *event) {
    if (!isAllowSelection) {
        return;
    }

    QComboBox::mouseReleaseEvent(event);
}

void NoWheelComboBox::keyPressEvent(QKeyEvent *event) {
    if (!isAllowSelection) {
        return;
    }

    QComboBox::keyPressEvent(event);
}

void NoWheelComboBox::keyReleaseEvent(QKeyEvent *event) {
    if (!isAllowSelection) {
        return;
    }

    QComboBox::keyReleaseEvent(event);
}
