#ifndef NO_WHEEL_SPINBOX_H
#define NO_WHEEL_SPINBOX_H

#include <QColor>
#include <QWidget>
#include <QSpinBox>
#include <QWheelEvent>

class NoWheelSpinBox : public QSpinBox {
    Q_OBJECT

public:
    explicit NoWheelSpinBox(QWidget *parent = nullptr)
        : QSpinBox(parent) {

    }

protected:
    void wheelEvent(QWheelEvent *event) override {
        event->ignore();
    }
};


#endif // NO_WHEEL_SPINBOX_H
