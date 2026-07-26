#ifndef GROUP_FRAME_H
#define GROUP_FRAME_H

#include <QFrame>
#include <QFrame>
#include <QPainter>
#include <QVBoxLayout>
#include <QStyleOption>

/// QFrame subclass that paints a colored header bar with a title across the top of the
/// widget. The addWidget()/child-layout support present in the sibling GroupFrameWidget
/// class is currently disabled here (commented out), so this widget only renders the
/// header decoration and does not manage a body layout.
class GroupFrame : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QColor headerBackground READ headerBackground WRITE setHeaderBackground NOTIFY headerBackGroundColorChanged)
    Q_PROPERTY(QFont headerFont READ headerFont WRITE setHeaderFont NOTIFY headerFontChanged)
    Q_PROPERTY(QColor headerColor READ headerColor WRITE setHeaderColor NOTIFY headerColorChanged FINAL)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)

public:
    /// Constructs the frame with default header styling (dark gray background, "Header"
    /// title) and disables the frame border; does not create a child layout, since
    /// addWidget()/layout support is currently disabled in this class.
    explicit GroupFrame(QWidget* parent = nullptr);

    // void addWidget(QWidget* w);

    /// Returns the header bar's fill color.
    QColor headerBackground() const;
    /// Sets the header bar's fill color and repaints.
    /// @note emits headerBackGroundColorChanged()
    void setHeaderBackground(const QColor& color);

    /// Returns the font used to draw the header title text.
    /// @note the constructor's custom font initialization is currently commented out, so
    /// this returns a default-constructed QFont until setHeaderFont() is called.
    QFont headerFont() const;
    /// Sets the font used to draw the header title text and repaints.
    /// @note emits headerFontChanged()
    void setHeaderFont(const QFont& font);

    /// Returns the color used to draw the header title text.
    QColor headerColor() const;
    /// Sets the color used to draw the header title text and repaints.
    /// @note emits headerColorChanged()
    void setHeaderColor(const QColor& color);

    /// Returns the current header title text.
    QString title() const;
    /// Sets the header title text and repaints.
    /// @note emits titleChanged()
    void setTitle(const QString& t);

protected:
    /// Paints the default frame background via the current style, then draws the header
    /// bar (m_headerBackground) across the top 30 pixels of the widget with m_title drawn
    /// left-aligned/vertically centered using m_headerFont and m_headerColor.
    void paintEvent(QPaintEvent* event) override;

signals:
    /// Emitted after the header background color changes via setHeaderBackground().
    void headerBackGroundColorChanged();
    /// Emitted after the header title font changes via setHeaderFont().
    void headerFontChanged();
    /// Emitted after the header title color changes via setHeaderColor().
    void headerColorChanged();
    /// Emitted after the header title text changes via setTitle().
    void titleChanged();

private:
    // QVBoxLayout* m_layout;
    QColor m_headerBackground;  ///< Fill color of the header bar (default: Qt::darkGray).
    QFont m_headerFont;         ///< Font for the header title text (default-constructed QFont; the intended custom default is currently commented out in the constructor).
    QColor m_headerColor;       ///< Color for the header title text (default-constructed QColor until set).
    QString m_title;            ///< Header title text displayed in the header bar (default: "Header").
};

#endif // GROUP_FRAME_H
