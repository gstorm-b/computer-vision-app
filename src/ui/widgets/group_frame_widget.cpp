#include "group_frame_widget.h"

GroupFrameWidget::GroupFrameWidget(QWidget* parent)
    : QFrame(parent),
    m_headerBackground(Qt::darkGray),
    m_headerFont(QFont("Segoe UI", 10, QFont::Bold)),
    m_title("Header") {

    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 30, 0, 0);
    setLayout(m_layout);
}

void GroupFrameWidget::addWidget(QWidget* w) {
    m_layout->addWidget(w);
}

QColor GroupFrameWidget::headerBackground() const {
    return m_headerBackground;
}

void GroupFrameWidget::setHeaderBackground(const QColor& color) {
    m_headerBackground = color;
    update();
    emit headerBackGroundColorChanged();
}

QFont GroupFrameWidget::headerFont() const {
    return m_headerFont;
}

void GroupFrameWidget::setHeaderFont(const QFont& font) {
    m_headerFont = font;
    update();
    emit headerFontChanged();
}

QColor GroupFrameWidget::headerColor() const {
    return m_headerColor;
}

void GroupFrameWidget::setHeaderColor(const QColor& color) {
    m_headerColor = color;
    update();
    emit headerColorChanged();
}

QString GroupFrameWidget::title() const {
    return m_title;
}

void GroupFrameWidget::setTitle(const QString& t) {
    m_title = t;
    update();
    emit titleChanged();
}

void GroupFrameWidget::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);

    QPainter p(this);
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    QRect headerRect(0, 0, width(), 30);
    p.fillRect(headerRect, m_headerBackground);

    p.setFont(m_headerFont);
    p.setPen(m_headerColor);
    p.drawText(headerRect.adjusted(10, 0, -10, 0),
               Qt::AlignVCenter | Qt::AlignLeft,
               m_title);
}
