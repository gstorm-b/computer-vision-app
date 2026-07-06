#ifndef VISION_TCPIP_DEVICE_WIDGET_H
#define VISION_TCPIP_DEVICE_WIDGET_H

#include <QWidget>

#include "DockWidget.h"

#include "device/idevice.h"
#include "device/device_manager.h"
#include "device/output_device/vision_output_device.h"
#include "device/output_device/vision_tcpip_device.h"
#include "device/output_device/vision_tcpip_config.h"
#include "device/plc/mc_device_map.h"
#include "runtime/vision_output_runner.h"

#include "ui/forms/device_widget.h"
#include "ui/forms/vision_output/robot_kinematic_check_widget.h"

namespace Ui {
class VisionTcpipDeviceWidget;
}

class VisionTcpipDeviceWidget : public IDeviceWidget {
    Q_OBJECT

public:
    explicit VisionTcpipDeviceWidget(std::shared_ptr<vc::device::IDevice> dv,
                                     vc::runtime::VisionOutputRunner *runner,
                                     ads::CDockWidget *dock = nullptr,
                                     QWidget *parent = nullptr);
    ~VisionTcpipDeviceWidget();

    QString deviceId() override;
    void loadConfigToDevice() override;
    void loadConfigToWidget() override;

private slots:
    void onPropertyValueChanged(QtProperty *property, const QVariant &variant);
    void saveConfig();
    void refreshConfig();

    void onConnectClicked();
    void onFieldConfigChanged();
    void onConnectionStateChanged(vc::device::ConnectStatus state);
    void onMainClientStateChanged(bool connected);

    void onAddRow();
    void onRemoveRow();
    void onSendResult();

private:
    void initWidget();
    void populateBrowser();
    void updateConnectionVisual(vc::device::ConnectStatus status);
    void updateSendSection(bool mainClientConnected);
    void addPositionRow(double x = 0.0, double y = 0.0,
                        double z = 0.0, double r = 0.0);

private:
    Ui::VisionTcpipDeviceWidget *ui;
    std::shared_ptr<vc::device::IDevice> m_device;
    // This widget is the TCP-transport view of the VisionOutput family; the
    // sibling VisionSerial transport will need its own widget when it lands.
    vc::device::VisionTcpipDevice* m_output_device{nullptr};
    ads::CDockWidget *m_dock{nullptr};

    vc::device::VisionTcpipDeviceCfg m_config;

    vc::runtime::VisionOutputRunner *m_runner{nullptr};

    RobotKinematicCheckWidget *m_kcheckWidget{nullptr};

    bool m_populating_browser{false};
};

#endif // VISION_TCPIP_DEVICE_WIDGET_H
