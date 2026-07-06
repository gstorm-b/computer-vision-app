#include "clamp.h"

CLamp::CLamp(QWidget *parent)
    : QWidget{parent} {
    m_max_size = 0;
    m_enable_state = false;
    m_pen_width = 1;
    m_color_enable = QColor(Qt::green);
    m_color_disable = QColor(Qt::gray);
}

void CLamp::setLampMaxSize(int max_size) {
    if (max_size < 5) {
        return;
    }

    m_max_size = max_size;
}

void CLamp::setEnableState(bool state) {
    if (m_enable_state == state) {
        return;
    }

    m_enable_state = state;
    this->update();
}

void CLamp::setColorEnable(QColor color) {
    if (color.isValid()) {
        m_color_enable = color;
    }
}

void CLamp::setColorDisable(QColor color) {
    if (color.isValid()) {
        m_color_disable = color;
    }
}

void CLamp::setOuterWidth(int outer_width) {
    if (outer_width < 0) {
        return;
    }

    m_pen_width = outer_width;
}

void CLamp::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    int lamp_size;
    if (m_max_size < 5) {
       lamp_size = (getAdjustedSize() / 2) - m_pen_width;
    } else {
        lamp_size = (m_max_size / 2) - m_pen_width;
    }
    QColor paint_color;
    if (m_enable_state) {
        paint_color = m_color_enable;
    } else {
        paint_color = m_color_disable;
    }

    QRectF lamp_rect(-lamp_size, -lamp_size, lamp_size*2, lamp_size*2);
    lamp_rect.translate(this->center());

    QPen pen(Qt::black);
    pen.setWidth(m_pen_width);
    painter.setPen(pen);
    painter.setBrush(paint_color);
    painter.drawEllipse(lamp_rect);
}

int CLamp::getAdjustedSize() {
    if (this->width() > this->height()) {
        return this->height();
    }
    return this->width();
}

QPointF CLamp::center() {
    return QPointF(this->width()/2, this->height()/2);
}
