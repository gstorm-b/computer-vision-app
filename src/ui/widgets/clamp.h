#ifndef CLAMP_H
#define CLAMP_H

#include <QWidget>
#include <QPaintEvent>
#include <QPainter>
#include <QColor>
#include <QRectF>

class CLamp : public QWidget {
        Q_OBJECT
    public:
        explicit CLamp(QWidget *parent = nullptr);

        void setLampMaxSize(int max_size);
        void setEnableState(bool state);
        void setColorEnable(QColor color);
        void setColorDisable(QColor color);
        void setOuterWidth(int outer_width);

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        int getAdjustedSize();
        QPointF center();

    signals:

    private:
        int m_max_size;
        bool m_enable_state;
        QColor m_color_enable;
        QColor m_color_disable;
        int m_pen_width;
};

#endif // CLAMP_H
