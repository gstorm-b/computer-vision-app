#pragma once

#include <QFrame>
#include <QIcon>
#include <QSize>
#include <QString>

#include <memory>

#include "device/idevice.h"

class QLabel;
class QMouseEvent;
class DeviceNavDot;

class DeviceNavItemWidget : public QFrame
{
    Q_OBJECT
public:
    explicit DeviceNavItemWidget(QWidget *parent = nullptr);

    void setDeviceId(const QString &deviceId);
    QString deviceId() const;

    void setDeviceName(const QString &name);
    QString deviceName() const;

    void setDeviceType(vc::device::DeviceType type);
    vc::device::DeviceType deviceType() const;

    void setTypeLabelText(const QString &text);
    QString typeLabelText() const;

    void setIcon(const QIcon &icon, const QSize &size = QSize(14, 14));
    void setIconPath(const QString &iconPath, const QSize &size = QSize(14, 14));

    void setSelected(bool selected);
    bool isSelected() const;

    void setIndicatorState(const QString &state);
    QString indicatorState() const;
    void setConnectStatus(vc::device::ConnectStatus status);

    void setDevice(const std::shared_ptr<vc::device::IDevice> &device);

    DeviceNavDot *indicatorDot() const;

    void refreshStyle();

    static QString roleForDeviceType(vc::device::DeviceType type);
    static QString defaultTypeLabelFor(vc::device::DeviceType type);
    static QString defaultIconPathFor(vc::device::DeviceType type);
    static QString indicatorStateFor(vc::device::ConnectStatus status);

signals:
    void activated(const QString &deviceId);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void initUi();

private:
    QString m_deviceId;
    QString m_iconPath;
    QSize m_iconSize{14, 14};
    vc::device::DeviceType m_deviceType{vc::device::DeviceType::UserType};
    bool m_selected{false};
    QLabel *m_iconLabel{nullptr};
    QLabel *m_nameLabel{nullptr};
    QLabel *m_typeLabel{nullptr};
    DeviceNavDot *m_indicatorDot{nullptr};
};
