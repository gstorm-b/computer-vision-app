#ifndef MITSUBISHI_MC_DEVICE_WIDGET_H
#define MITSUBISHI_MC_DEVICE_WIDGET_H

#include <QWidget>
#include "DockWidget.h"

#include "device/idevice.h"
#include "device/device_manager.h"
#include "device/plc/mc_protocol_device.h"

#include "runtime/plc_runner.h"

#include "ui/forms/device_widget.h"
#include "ui/widgets/plc_widget/devices_monitor_widget.h"

namespace Ui {
class MitsubishiMcDeviceWidget;
}

class MitsubishiMcDeviceWidget : public IDeviceWidget {
    Q_OBJECT

public:
    // Runner is owned by TaskRunner (not by this widget).  See header
    // comment in BaslerCameraWidget for the same rationale.
    explicit MitsubishiMcDeviceWidget(std::shared_ptr<vc::device::IDevice> dv,
                                      vc::runtime::PlcRunner *runner,
                                      ads::CDockWidget *dock = nullptr,
                                      QWidget *parent = nullptr);
    ~MitsubishiMcDeviceWidget();

    QString deviceId() override;
    void loadConfigToDevice() override;
    void loadConfigToWidget() override;

private slots:
    void onPropertyValueChanged(QtProperty *property, const QVariant &variant);

    void onBtnConnect();
    void saveConfig();
    void refreshConfig();

    // Per-row write requests forwarded from the monitor widgets.
    void onBitWriteRequested(int address, quint8 value);
    void onWordWriteRequested(int address, qint16 value);

    // Inline edits on the connection card.
    void onIpEditFinished();
    void onPortEditFinished();
    void onConnectTimeoutEditFinished();
    void onResponseTimeoutEditFinished();

    void onConnectionStateChanged(vc::device::ConnectStatus state);
    void onPollingUpdateValue(std::shared_ptr<vc::device::PlcValueMap> device_map);

private:
    void initWidget();
    void rebuildMonitorRanges();
    void refreshMetaSummary();
    void populateConnectionFields();
    void populateBrowser();
    void updateConnectionVisual(vc::device::ConnectStatus status);

private:
    Ui::MitsubishiMcDeviceWidget *ui;
    std::shared_ptr<vc::device::IDevice> m_device;
    vc::device::McProtocolDevice* m_mc_device;
    ads::CDockWidget *m_dock{nullptr};

    vc::device::McProtocolConfig m_config;
    vc::device::McDeviceMap m_device_map;

    vc::runtime::PlcRunner *m_runner{nullptr};

    // Two stacked monitors for M (bit) and D (word) registers.
    vc::widgets::DevicesMonitorWidget *m_monitor_m{nullptr};
    vc::widgets::DevicesMonitorWidget *m_monitor_d{nullptr};

    bool m_populating_browser{false};
    bool m_loading_connection_fields{false};
};

#endif // MITSUBISHI_MC_DEVICE_WIDGET_H
