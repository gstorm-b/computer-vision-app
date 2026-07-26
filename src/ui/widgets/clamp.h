#ifndef CLAMP_H
#define CLAMP_H

#include <QWidget>
#include <QPaintEvent>
#include <QPainter>
#include <QColor>
#include <QRectF>

/// Circular status lamp widget: paints a filled, outlined circle centered in the
/// widget, switching between an "enabled" and "disabled" fill color.
class CLamp : public QWidget {
        Q_OBJECT
    public:
        /// Constructs the lamp with default state (disabled), gray/green colors,
        /// and a 1px outline; the drawn size auto-fits the widget until
        /// setLampMaxSize() is called.
        explicit CLamp(QWidget *parent = nullptr);

        /// Sets a fixed lamp diameter (in pixels, before subtracting the outline
        /// width) used instead of auto-sizing to the widget's bounds.
        /// @param max_size ignored (no-op) if less than 5
        void setLampMaxSize(int max_size);

        /// Sets whether the lamp is drawn in its enabled or disabled color;
        /// triggers a repaint only when the state actually changes.
        void setEnableState(bool state);

        /// Sets the fill color used when the lamp is enabled; ignored if `color`
        /// is not a valid QColor.
        void setColorEnable(QColor color);

        /// Sets the fill color used when the lamp is disabled; ignored if `color`
        /// is not a valid QColor.
        void setColorDisable(QColor color);

        /// Sets the outline (pen) width used when drawing the lamp circle.
        /// @param outer_width ignored (no-op) if negative
        void setOuterWidth(int outer_width);

    protected:
        /// Paints the lamp as an anti-aliased filled circle (black outline, fill
        /// color chosen from the current enable state) centered in the widget.
        void paintEvent(QPaintEvent *event) override;

    private:
        /// Returns the widget's shorter side (width or height), used to derive the
        /// lamp diameter when no explicit max size has been set.
        int getAdjustedSize();

        /// Returns the widget's center point in local (widget) coordinates.
        QPointF center();

    signals:

    private:
        int m_max_size;          ///< Fixed lamp diameter override; 0/unset triggers auto-sizing via getAdjustedSize().
        bool m_enable_state;     ///< Current enable/disable state driving which fill color is painted.
        QColor m_color_enable;   ///< Fill color used when enabled (defaults to green).
        QColor m_color_disable;  ///< Fill color used when disabled (defaults to gray).
        int m_pen_width;         ///< Outline width used when drawing the lamp circle.
};

#endif // CLAMP_H
