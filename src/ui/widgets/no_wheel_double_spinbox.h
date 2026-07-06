#ifndef NO_WHEEL_DOUBLE_SPINBOX_H
#define NO_WHEEL_DOUBLE_SPINBOX_H

#include <QColor>
#include <QWidget>
#include <QDoubleSpinBox>
#include <QWheelEvent>

class NoWheelDoubleSpinBox : public QDoubleSpinBox {
    Q_OBJECT

public:
    explicit NoWheelDoubleSpinBox(QWidget *parent = nullptr)
        : QDoubleSpinBox(parent) {

    }

protected:
    void wheelEvent(QWheelEvent *event) override {
        event->ignore();
    }
};



#endif // NO_WHEEL_DOUBLE_SPINBOX_H
