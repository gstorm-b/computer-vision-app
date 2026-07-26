#include "group_frame.h"

/// Constructs the frame with default header styling (dark gray background, "Header" title)
/// and disables the frame border. The child layout / addWidget() setup is commented out,
/// so this class does not manage a body layout for child widgets.
GroupFrame::GroupFrame(QWidget* parent)
    : QFrame(parent),
    m_headerBackground(Qt::darkGray),
    // m_headerFont(QFont("Segoe UI", 20, QFont::Bold)),
    m_title("Header") {

    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // m_layout = new QVBoxLayout(this);
    // m_layout->setContentsMargins(0, 30, 0, 0);
    // setLayout(m_layout);
}

// void GroupFrame::addWidget(QWidget* w) {
//     m_layout->addWidget(w);
// }

/// Returns the header bar's fill color.
QColor GroupFrame::headerBackground() const {
    return m_headerBackground;
}

/// Sets the header bar's fill color, requests a repaint, and emits
/// headerBackGroundColorChanged().
void GroupFrame::setHeaderBackground(const QColor& color) {
    m_headerBackground = color;
    update();
    emit headerBackGroundColorChanged();
}

/// Returns the font used to draw the header title text (a default-constructed QFont until
/// setHeaderFont() is called, since the constructor's custom font init is commented out).
QFont GroupFrame::headerFont() const {
    return m_headerFont;
}

/// Sets the font used to draw the header title text, requests a repaint, and emits
/// headerFontChanged().
void GroupFrame::setHeaderFont(const QFont& font) {
    m_headerFont = font;
    update();
    emit headerFontChanged();
}

/// Returns the color used to draw the header title text.
QColor GroupFrame::headerColor() const {
    return m_headerColor;
}

/// Sets the color used to draw the header title text, requests a repaint, and emits
/// headerColorChanged().
void GroupFrame::setHeaderColor(const QColor& color) {
    m_headerColor = color;
    update();
    emit headerColorChanged();
}

/// Returns the current header title text.
QString GroupFrame::title() const {
    return m_title;
}

/// Sets the header title text, requests a repaint, and emits titleChanged().
void GroupFrame::setTitle(const QString& t) {
    m_title = t;
    update();
    emit titleChanged();
}

/// Paints the default frame background via the current style, then fills a header bar
/// spanning the top 30 pixels of the widget with m_headerBackground and draws m_title
/// left-aligned/vertically centered inside it using m_headerFont and m_headerColor.
/// @param event forwarded to QFrame::paintEvent() for the default frame drawing
void GroupFrame::paintEvent(QPaintEvent* event) {
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
