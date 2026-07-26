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

/// Device widget for a Mitsubishi MC-protocol PLC: hosts the connection card
/// (IP/port/timeouts), a property browser for the MC context and message-interface
/// config, and two DevicesMonitorWidget instances (bit "M" registers and word "D"
/// registers) driven by polling updates forwarded from the PlcRunner.
class MitsubishiMcDeviceWidget : public IDeviceWidget {
    Q_OBJECT

public:
    /// Builds the widget for `dv` and wires it to `runner` for connect/disconnect
    /// requests and polling updates.
    /// @param dv the MC-protocol device (expected to wrap a vc::device::McProtocolDevice)
    /// @param runner the PLC runner used to (dis)connect and receive polling updates;
    ///        the runner is owned by TaskRunner, not by this widget (see the header
    ///        comment in BaslerCameraWidget for the same rationale)
    /// @param dock optional dock widget hosting this widget
    /// @param parent optional parent widget
    explicit MitsubishiMcDeviceWidget(std::shared_ptr<vc::device::IDevice> dv,
                                      vc::runtime::PlcRunner *runner,
                                      ads::CDockWidget *dock = nullptr,
                                      QWidget *parent = nullptr);
    /// Destroys the widget and its generated `ui` form.
    ~MitsubishiMcDeviceWidget();

    /// Returns the id of the wrapped device.
    /// @return `m_device->id()`
    QString deviceId() override;
    /// Currently a no-op: config changes are written back to the device directly from
    /// the individual field-change slots (see saveConfig()) rather than through this hook.
    void loadConfigToDevice() override;
    /// Currently a no-op: widget fields are populated from initWidget()/populateConnectionFields()
    /// rather than through this hook.
    void loadConfigToWidget() override;

private slots:
    /// Applies an edited property-browser value to whichever backing object owns that
    /// property name (the device itself, the MC context, or its message-interface
    /// config), then persists and refreshes the dependent UI; ignored while
    /// m_populating_browser is true to avoid feedback loops from populateBrowser().
    /// @param property the edited property (looked up by name on each candidate object)
    /// @param variant the new value
    void onPropertyValueChanged(QtProperty *property, const QVariant &variant);

    /// Requests connect or disconnect via m_runner depending on the device's current
    /// connection state.
    void onBtnConnect();
    /// Rebuilds the monitor address ranges and pushes m_config into the underlying
    /// McProtocolDevice.
    void saveConfig();
    /// Currently a no-op beyond the null-device guard.
    void refreshConfig();

    // Per-row write requests forwarded from the monitor widgets.
    /// Bit ("M") write request forwarded from m_monitor_m; builds and pushes a WriteBit
    /// MCRequest for `address`, ignored while the device is not connected.
    /// @param value non-zero writes bit 0x01, zero writes 0x00
    void onBitWriteRequested(int address, quint8 value);
    /// Word ("D") write request forwarded from m_monitor_d; builds and pushes a
    /// WriteWord MCRequest for `address`, ignored while the device is not connected.
    void onWordWriteRequested(int address, qint16 value);

    // Inline edits on the connection card.
    /// Applies a trimmed, changed IP address to the Ethernet TCP/IP interface config and
    /// saves; no-op if the current interface is not Ethernet TCP/IP.
    void onIpEditFinished();
    /// Applies a changed port number to the Ethernet TCP/IP interface config and saves;
    /// no-op if the current interface is not Ethernet TCP/IP.
    void onPortEditFinished();
    /// Applies a changed connect timeout to the interface config and saves.
    void onConnectTimeoutEditFinished();
    /// Applies a changed response timeout to the interface config and saves.
    void onResponseTimeoutEditFinished();

    /// Updates the connection indicator/button and, on a lost/closed/failed link, clears
    /// the cached values shown in both monitor widgets.
    /// @param state the new connection state reported by the runner
    void onConnectionStateChanged(vc::device::ConnectStatus state);
    /// Applies a freshly polled device map to the bit and word monitor widgets.
    /// @param device_map the latest PlcValueMap, downcast to McDeviceMap
    void onPollingUpdateValue(std::shared_ptr<vc::device::PlcValueMap> device_map);

private:
    /// One-time setup: initializes the property browser, applies the per-form QSS, wires
    /// runner signals to this widget's slots (never connects to the device directly, so
    /// updates stay on the GUI thread), creates the two DevicesMonitorWidget instances
    /// and slots them into the .ui splitter containers, then populates the connection
    /// card, meta summary, and property browser.
    void initWidget();
    /// Applies the context's configured M/D start address and count to m_monitor_m and
    /// m_monitor_d.
    void rebuildMonitorRanges();
    /// Updates the meta-summary label with the current frame type, data code, and
    /// refresh interval.
    void refreshMetaSummary();
    /// Populates the IP/port/timeout fields from the interface config while blocking
    /// their signals; disables the IP/port fields when the interface is not Ethernet
    /// TCP/IP.
    void populateConnectionFields();
    /// Rebuilds the property browser from the device, MC context, and message-interface
    /// config, guarded by m_populating_browser so the resulting valueChanged signals are
    /// not treated as user edits.
    void populateBrowser();
    /// Updates the connection dot/label/button text and "connectionState" dynamic
    /// property (repolishing each widget) to reflect the given connection state.
    /// @param status only Connected is treated as "connected"; all other values render
    ///        as disconnected
    void updateConnectionVisual(vc::device::ConnectStatus status);

private:
    Ui::MitsubishiMcDeviceWidget *ui;  ///< Generated .ui form; owned by this widget.
    std::shared_ptr<vc::device::IDevice> m_device;  ///< The wrapped device, shared with the device manager.
    vc::device::McProtocolDevice* m_mc_device;  ///< `m_device` downcast to the MC-protocol device; not owned.
    ads::CDockWidget *m_dock{nullptr};  ///< Optional dock hosting this widget; not owned.

    vc::device::McProtocolConfig m_config;  ///< Working copy of the device's MC-protocol config, edited via the property browser.
    vc::device::McDeviceMap m_device_map;  ///< Latest polled bit/word register values, refreshed by onPollingUpdateValue().

    vc::runtime::PlcRunner *m_runner{nullptr};  ///< Runner used for connect/disconnect and polling updates; owned by TaskRunner, not this widget.

    // Two stacked monitors for M (bit) and D (word) registers.
    vc::widgets::DevicesMonitorWidget *m_monitor_m{nullptr};  ///< Bit ("M" register) monitor; child widget, slotted into ui->container_m.
    vc::widgets::DevicesMonitorWidget *m_monitor_d{nullptr};  ///< Word ("D" register) monitor; child widget, slotted into ui->container_d.

    bool m_populating_browser{false};  ///< True while populateBrowser() is rebuilding the browser, to suppress onPropertyValueChanged().
    bool m_loading_connection_fields{false};  ///< True while populateConnectionFields() is applying values, to suppress the inline-edit slots.
};

#endif // MITSUBISHI_MC_DEVICE_WIDGET_H
