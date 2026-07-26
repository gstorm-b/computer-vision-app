#ifndef NO_WHEEL_DOUBLE_SPINBOX_H
#define NO_WHEEL_DOUBLE_SPINBOX_H

#include <QColor>
#include <QWidget>
#include <QDoubleSpinBox>
#include <QWheelEvent>

/// QDoubleSpinBox specialization that always ignores mouse-wheel events, so scrolling a
/// form does not accidentally change the spin box's value.
class NoWheelDoubleSpinBox : public QDoubleSpinBox {
    Q_OBJECT

public:
    /// Constructs the spin box with default QDoubleSpinBox behavior; wheel events are
    /// ignored regardless.
    explicit NoWheelDoubleSpinBox(QWidget *parent = nullptr)
        : QDoubleSpinBox(parent) {

    }

protected:
    /// Ignores the event so mouse-wheel scrolling never changes the current value.
    void wheelEvent(QWheelEvent *event) override {
        event->ignore();
    }
};



#endif // NO_WHEEL_DOUBLE_SPINBOX_H
