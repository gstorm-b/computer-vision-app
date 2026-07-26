#ifndef NO_WHEEL_COMBOBOX_H
#define NO_WHEEL_COMBOBOX_H

#include <QColor>
#include <QWidget>
#include <QComboBox>
#include <QWheelEvent>

/// QComboBox specialization that always ignores mouse-wheel events (to avoid accidental value
/// changes while scrolling a form) and can additionally be switched into a fully non-interactive
/// state where mouse and keyboard events are swallowed instead of forwarded to QComboBox.
class NoWheelComboBox : public QComboBox {
    Q_OBJECT

    public:
        /// Constructs the combo box with selection interaction enabled by default; wheel events
        /// are always ignored regardless of this setting.
        explicit NoWheelComboBox(QWidget *parent = nullptr);

        /// Enables or disables mouse/keyboard interaction (press, release, key press/release);
        /// wheel events remain ignored either way.
        /// @param enable true to allow interaction, false to swallow it
        void setAllowSelection(bool enable);

        /// Applies `color` to the widget's background palette role.
        /// @param color the color to apply; the call is a no-op if it is invalid
        void setBackGroundColor(QColor color);

    protected:
        /// Ignores the event so mouse-wheel scrolling never changes the current selection.
        void wheelEvent(QWheelEvent *event) override;

        /// Emits AboutToShow() and forwards to QComboBox::mousePressEvent() when selection is
        /// allowed; otherwise swallows the event.
        void mousePressEvent(QMouseEvent *event) override;

        /// Forwards to QComboBox::mouseReleaseEvent() when selection is allowed; otherwise
        /// swallows the event.
        void mouseReleaseEvent(QMouseEvent *event) override;

        /// Forwards to QComboBox::keyPressEvent() when selection is allowed; otherwise swallows
        /// the event.
        void keyPressEvent(QKeyEvent *event) override;

        /// Forwards to QComboBox::keyReleaseEvent() when selection is allowed; otherwise
        /// swallows the event.
        void keyReleaseEvent(QKeyEvent *event) override;


    signals:
        /// Emitted from mousePressEvent(), just before the press is forwarded to QComboBox
        /// (and thus before its popup opens), when selection is allowed.
        void AboutToShow();

    private:
        bool isAllowSelection; ///< When false, mouse/key events are swallowed instead of forwarded to QComboBox.
};

#endif // NO_WHEEL_COMBOBOX_H
