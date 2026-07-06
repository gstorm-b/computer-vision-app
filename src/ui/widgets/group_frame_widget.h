#ifndef GROUP_FRAME_WIDGET_H
#define GROUP_FRAME_WIDGET_H

#include <QFrame>
#include <QPainter>
#include <QVBoxLayout>
#include <QStyleOption>

class GroupFrameWidget : public QFrame {
    Q_OBJECT
    Q_PROPERTY(QColor headerBackground READ headerBackground WRITE setHeaderBackground NOTIFY headerBackGroundColorChanged)
    Q_PROPERTY(QFont headerFont READ headerFont WRITE setHeaderFont NOTIFY headerFontChanged)
    Q_PROPERTY(QColor headerColor READ headerColor WRITE setHeaderColor NOTIFY headerColorChanged FINAL)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)

public:
    explicit GroupFrameWidget(QWidget* parent = nullptr);

    void addWidget(QWidget* w);

    QColor headerBackground() const;
    void setHeaderBackground(const QColor& color);

    QFont headerFont() const;
    void setHeaderFont(const QFont& font);

    QColor headerColor() const;
    void setHeaderColor(const QColor& color);

    QString title() const;
    void setTitle(const QString& t);

protected:
    void paintEvent(QPaintEvent* event) override;

signals:
    void headerBackGroundColorChanged();
    void headerFontChanged();
    void headerColorChanged();
    void titleChanged();

private:
    QVBoxLayout* m_layout;
    QColor m_headerBackground;
    QFont m_headerFont;
    QColor m_headerColor;
    QString m_title;
};

#endif // GROUP_FRAME_WIDGET_H
