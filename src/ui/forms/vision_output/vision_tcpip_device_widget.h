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
/// Qt Designer-generated form class for VisionTcpipDeviceWidget (declared by the .ui compiler).
class VisionTcpipDeviceWidget;
}

/// Device configuration/monitoring widget for the TCP-transport Vision Output device: edits the
/// listen address/ports, shows connection and runtime-state/diagnostics properties in the
/// embedded property browser, hosts the robot kinematic-check sub-widget, and lets the operator
/// build a position table and send it to the connected vision client.
class VisionTcpipDeviceWidget : public IDeviceWidget {
    Q_OBJECT

public:
    /// Builds the widget for device `dv`, wiring it to `runner` for connect/disconnect and
    /// connection-state signals; `dock` is the owning dock widget (if any).
    explicit VisionTcpipDeviceWidget(std::shared_ptr<vc::device::IDevice> dv,
                                     vc::runtime::VisionOutputRunner *runner,
                                     ads::CDockWidget *dock = nullptr,
                                     QWidget *parent = nullptr);
    /// Destroys the widget and deletes the generated `ui` form.
    ~VisionTcpipDeviceWidget();

    /// @return the id of the underlying device.
    QString deviceId() override;
    /// Pushes the currently edited `m_config` to the underlying VisionTcpipDevice.
    void loadConfigToDevice() override;
    /// Reloads `m_config` from the underlying VisionTcpipDevice and refreshes the address/port
    /// fields and kinematic-check sub-widget from it (property-browser change notifications are
    /// suppressed while this runs).
    void loadConfigToWidget() override;

private slots:
    /// Handles an edit made in the property browser: dispatches "name" changes to the device
    /// manager (rejecting duplicates) and all other output-config properties through
    /// vc::gadget_meta::writeProperty, then persists via saveConfig(). Ignored while
    /// m_populating_browser is true to avoid feedback loops.
    void onPropertyValueChanged(QtProperty *property, const QVariant &variant);
    /// Writes `m_config` back to the device, warning the user if the save is rejected (e.g. the
    /// device is currently connected).
    void saveConfig();
    /// Placeholder refresh hook; currently a no-op beyond the null-device guard.
    void refreshConfig();

    /// Requests connect or disconnect via the runner depending on the device's current connection
    /// state.
    void onConnectClicked();
    /// Copies the IP/port fields into `m_config` and saves + repopulates the property browser.
    void onFieldConfigChanged();
    /// Updates the connection visuals, repopulates the property browser, and refreshes the send
    /// section's enabled state in response to a runner-reported connection-state change.
    void onConnectionStateChanged(vc::device::ConnectStatus state);
    /// Updates the send-result section's enabled state when the main client connects/disconnects.
    void onMainClientStateChanged(bool connected);

    /// Appends a blank position row to the send table, capped at 20 rows.
    void onAddRow();
    /// Removes the currently selected position row, or the last row if none is selected.
    void onRemoveRow();
    /// Reads every row of the position table into a VisionOutputPosition list and asks the runner
    /// to send it, provided a runner, output device, and connected main client all exist.
    void onSendResult();

private:
    /// Initializes the property browser and theme stylesheets, resolves `m_device` to a
    /// VisionTcpipDevice (disabling the widget on type mismatch), wires runner/device signals,
    /// loads the config, and sets up the send-result table and kinematic-check sub-widget.
    void initWidget();
    /// Clears and rebuilds the property browser with the device, output-config, runtime-state, and
    /// diagnostics property groups (signals blocked for the duration).
    void populateBrowser();
    /// Updates the connect button/label/dot text and "connectionState" style property for
    /// `status`, and disables the address/port fields while connected.
    void updateConnectionVisual(vc::device::ConnectStatus status);
    /// Enables/disables the send-result button and updates its hint label and "sendState" style
    /// property based on whether the main client is connected.
    void updateSendSection(bool mainClientConnected);
    /// Appends a row of four QDoubleSpinBox cell widgets (x, y, z, r) to the position table,
    /// pre-filled with the given values.
    void addPositionRow(double x = 0.0, double y = 0.0,
                        double z = 0.0, double r = 0.0);

private:
    Ui::VisionTcpipDeviceWidget *ui;  ///< Generated UI form; owned by this widget.
    std::shared_ptr<vc::device::IDevice> m_device;  ///< The abstract device backing this widget.
    // This widget is the TCP-transport view of the VisionOutput family; the
    // sibling VisionSerial transport will need its own widget when it lands.
    vc::device::VisionTcpipDevice* m_output_device{nullptr};  ///< m_device cast to its concrete TCP-transport type; null if the cast fails.
    ads::CDockWidget *m_dock{nullptr};  ///< Owning dock widget, if this widget is docked.

    vc::device::VisionTcpipDeviceCfg m_config;  ///< Editable copy of the device's output configuration.

    vc::runtime::VisionOutputRunner *m_runner{nullptr};  ///< Runner used to marshal connect/disconnect/send requests onto the device's thread.

    RobotKinematicCheckWidget *m_kcheckWidget{nullptr};  ///< Embedded kinematic-check sub-widget (owned by the generated `ui` form).

    bool m_populating_browser{false};  ///< True while populateBrowser()/loadConfigToWidget() are updating fields, to suppress change feedback loops.
};

#endif // VISION_TCPIP_DEVICE_WIDGET_H
