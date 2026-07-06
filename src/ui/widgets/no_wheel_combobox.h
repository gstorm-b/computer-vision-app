#ifndef NO_WHEEL_COMBOBOX_H
#define NO_WHEEL_COMBOBOX_H

#include <QColor>
#include <QWidget>
#include <QComboBox>
#include <QWheelEvent>

class NoWheelComboBox : public QComboBox {
    Q_OBJECT

    public:
        explicit NoWheelComboBox(QWidget *parent = nullptr);

        void setAllowSelection(bool enable);
        void setBackGroundColor(QColor color);

    protected:
        void wheelEvent(QWheelEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;
        void keyReleaseEvent(QKeyEvent *event) override;


    signals:
        void AboutToShow();

    private:
        bool isAllowSelection;
};

#endif // NO_WHEEL_COMBOBOX_H
