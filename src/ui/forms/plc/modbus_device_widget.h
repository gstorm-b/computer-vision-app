#ifndef MODBUS_DEVICE_WIDGET_H
#define MODBUS_DEVICE_WIDGET_H

#include <QWidget>
#include "DockWidget.h"

#include "device/idevice.h"
#include "device/plc/modbus/modbus_register_map.h"
#include "device/plc/modbus/modbus_tcp_client_device.h"
#include "device/plc/modbus/modbus_tcp_server_device.h"

#include "runtime/plc_runner.h"

#include "ui/forms/device_widget.h"
#include "ui/widgets/plc_widget/devices_monitor_widget.h"

namespace Ui {
class ModbusDeviceWidget;
}

/**
 * @file modbus_device_widget.h
 * @brief ModbusDeviceWidget — device widget for both Modbus PLC sub-types.
 */

/**
 * @class ModbusDeviceWidget
 * @brief Device widget for a Modbus TCP client **or** server: connection card, property browser
 *        over the config, and two DevicesMonitorWidget instances (bit areas and word areas)
 *        driven by polling updates forwarded from the PlcRunner.
 *
 * **One widget, two sub-types.** The plan called for two widgets, but the client and the server
 * differ in exactly one thing — the connection card — and share the whole register-map surface:
 * the same tag names, the same monitors, the same result block, the same property browser. Two
 * classes would be two copies of a 600-line panel kept in sync by hand, and the panel that
 * mattered would be the one someone forgot to update. `MitsubishiMcDeviceWidget` already carries
 * two connection cards (TCP and serial) in one widget for the same reason; this follows it.
 *
 * The monitors are labelled by area rather than hard-coded to "M"/"D": a Modbus device has four
 * areas, two of which the protocol makes read-only, so the bit monitor covers coils and discrete
 * inputs and the word monitor covers holding and input registers. Write requests from a row in a
 * read-only area are refused by the device, with a reason.
 */
class ModbusDeviceWidget : public IDeviceWidget {
    Q_OBJECT

public:
    /**
     * @brief Builds the widget for @p dv and wires it to @p runner for connect/disconnect
     *        requests and polling updates.
     * @param[in] dv the Modbus device (a ModbusTcpClientDevice or a ModbusTcpServerDevice)
     * @param[in] runner the PLC runner used to (dis)connect and receive polling updates;
     *        owned by TaskRunner, not by this widget
     * @param[in] dock optional dock widget hosting this widget
     * @param[in] parent optional parent widget
     */
    explicit ModbusDeviceWidget(std::shared_ptr<vc::device::IDevice> dv,
                                vc::runtime::PlcRunner *runner,
                                ads::CDockWidget *dock = nullptr,
                                QWidget *parent = nullptr);
    ~ModbusDeviceWidget() override;

    /// Returns the id of the wrapped device.
    QString deviceId() override;
    /// No-op: edits are pushed to the device as they are made, from the field slots and the
    /// property browser, rather than through this hook.
    void loadConfigToDevice() override;
    /// No-op: fields are populated by initWidget()/populateConnectionFields().
    void loadConfigToWidget() override;

private slots:
    /// Applies an edited property-browser value to the device's config, then persists it and
    /// refreshes the dependent UI. Ignored while populateBrowser() is rebuilding.
    void onPropertyValueChanged(QtProperty *property, const QVariant &variant);

    /// Requests connect or disconnect via m_runner depending on the current connection state.
    void onBtnConnect();

    /// Inline connection-card edits.
    void onHostEditFinished();
    void onListenEditFinished();
    void onPortChanged();
    void onUnitIdChanged();
    void onRefreshIntervalChanged();

    /// Switches which bit area (coils or discrete inputs) the bit monitor shows.
    void onBitAreaChanged();
    /// Switches which word area (holding or input registers) the word monitor shows.
    void onWordAreaChanged();

    /// Bit write request forwarded from the bit monitor.
    void onBitWriteRequested(int address, quint8 value);
    /// Word write request forwarded from the word monitor.
    void onWordWriteRequested(int address, qint16 value);

    /// Updates the connection indicator and clears the monitors on a lost/closed link.
    void onConnectionStateChanged(vc::device::ConnectStatus state);
    /// Applies a freshly polled register map to both monitors.
    void onPollingUpdateValue(std::shared_ptr<vc::device::PlcValueMap> device_map);

private:
    /// One-time setup: property browser, per-form QSS, runner wiring, monitor creation.
    void initWidget();
    /// Applies the configured spans to the two monitors.
    void rebuildMonitorRanges();
    /// Updates the meta summary and the result-block label.
    void refreshMetaSummary();
    /// Populates the connection card and shows the card matching this device's sub-type.
    void populateConnectionFields();
    /// Rebuilds the property browser from the device and its config.
    void populateBrowser();
    /// Updates the connection dot/label/button for `status`.
    void updateConnectionVisual(vc::device::ConnectStatus status);
    /// Pushes the working config copy back onto the device.
    void saveConfig();

    /// Returns the shared Modbus config of whichever sub-type this widget wraps, or null.
    vc::device::ModbusConfig *config() const;
    /// Returns true when the wrapped device is the TCP client sub-type.
    bool isClient() const { return m_client != nullptr; }

    /// Returns the area currently selected in `combo`, or `fallback` if nothing is selected.
    vc::device::ModbusArea selectedArea(const class QComboBox *combo,
                                        vc::device::ModbusArea fallback) const;

    /// Returns whether *this* device may write `area`.
    /// @note Sub-type dependent, and that is the whole point: a client is a master and can never
    ///       write discrete inputs or input registers, while a server owns the space and writes
    ///       all four. Asking one question for both is what made the server refuse the two areas
    ///       it is the only legitimate writer of.
    bool canWriteArea(vc::device::ModbusArea area) const;

    Ui::ModbusDeviceWidget *ui;  ///< Generated .ui form; owned by this widget.
    std::shared_ptr<vc::device::IDevice> m_device;  ///< The wrapped device, shared with the device manager.
    vc::device::ModbusTcpClientDevice *m_client{nullptr};  ///< Set when the sub-type is the client; not owned.
    vc::device::ModbusTcpServerDevice *m_server{nullptr};  ///< Set when the sub-type is the server; not owned.
    ads::CDockWidget *m_dock{nullptr};  ///< Optional dock hosting this widget; not owned.

    vc::device::ModbusTcpClientCfg m_clientConfig;  ///< Working copy, used when the sub-type is the client.
    vc::device::ModbusTcpServerCfg m_serverConfig;  ///< Working copy, used when the sub-type is the server.
    vc::device::ModbusRegisterMap m_registerMap;    ///< Latest polled/served values.

    vc::runtime::PlcRunner *m_runner{nullptr};  ///< Runner for connect/disconnect and polling; owned by TaskRunner.

    vc::widgets::DevicesMonitorWidget *m_monitor_bits{nullptr};   ///< Coils + discrete inputs.
    vc::widgets::DevicesMonitorWidget *m_monitor_words{nullptr};  ///< Holding + input registers.

    bool m_populating_browser{false};       ///< Suppresses onPropertyValueChanged() during populateBrowser().
    bool m_loading_connection_fields{false};///< Suppresses the inline-edit slots during populateConnectionFields().
};

#endif // MODBUS_DEVICE_WIDGET_H
