#ifndef NO_WHEEL_SPINBOX_H
#define NO_WHEEL_SPINBOX_H

#include <QColor>
#include <QWidget>
#include <QSpinBox>
#include <QWheelEvent>

/// QSpinBox specialization that always ignores mouse-wheel events, so scrolling a form
/// does not accidentally change the spin box's value.
class NoWheelSpinBox : public QSpinBox {
    Q_OBJECT

public:
    /// Constructs the spin box with default QSpinBox behavior; wheel events are ignored
    /// regardless.
    explicit NoWheelSpinBox(QWidget *parent = nullptr)
        : QSpinBox(parent) {

    }

protected:
    /// Ignores the event so mouse-wheel scrolling never changes the current value.
    void wheelEvent(QWheelEvent *event) override {
        event->ignore();
    }
};


#endif // NO_WHEEL_SPINBOX_H
