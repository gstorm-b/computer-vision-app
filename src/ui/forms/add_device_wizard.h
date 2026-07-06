#ifndef ADD_DEVICE_WIZARD_H
#define ADD_DEVICE_WIZARD_H

#include <QDialog>
#include <QMap>

#include "device/device_manager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AddDeviceWizard; }
class QFrame;
class QLabel;
QT_END_NAMESPACE

class AddDeviceWizard : public QDialog {
    Q_OBJECT

public:
    explicit AddDeviceWizard(std::shared_ptr<vc::device::DeviceManager> mng,
                             const QString &taskName = QString(),
                             QWidget *parent = nullptr);
    ~AddDeviceWizard() override;

    int showWizard();

    QString getDeviceId()   const { return m_pendingDeviceId; }
    QString getDeviceName() const;
    QString getDeviceType() const;

    void reject() override;

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onCardClicked(vc::device::DeviceType type);
    void onAddClicked();
    void onNameChanged(const QString &text);

private:
    struct CardRefs {
        QFrame *card{nullptr};
        QLabel *icon{nullptr};
        QLabel *name{nullptr};
        QLabel *desc{nullptr};
        int     stackPage{-1};      // -1 => hide stack for this type
        QString colorKey;           // matches QSS [deviceColor="..."]
        QString defaultName;
        QString iconPath;
    };

    void initCards();
    void refreshCardIcons();
    void selectCard(vc::device::DeviceType type);
    void reloadStyleSheet();
    void repolish(QWidget *w);
    QJsonObject buildDeviceJson(vc::device::DeviceType type);

    Ui::AddDeviceWizard *ui{nullptr};

    std::shared_ptr<vc::device::DeviceManager> m_manager;
    QString                m_pendingDeviceId;
    vc::device::DeviceType m_selectedType{vc::device::DeviceType::Camera};

    QMap<vc::device::DeviceType, CardRefs> m_cards;
};

#endif // ADD_DEVICE_WIZARD_H
