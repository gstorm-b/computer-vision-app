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

/// A single clickable device entry (icon + name + short type label + connect-status
/// dot) in a device navigation list. Style properties ("deviceType", "navActive")
/// and the indicator dot's "lampState" drive QSS; setDevice() keeps it in sync
/// with an vc::device::IDevice snapshot.
class DeviceNavItemWidget : public QFrame
{
    Q_OBJECT
public:
    /// Builds the icon/name/type layout, applies default device type, unselected
    /// state, and "off" indicator, and re-applies the current icon on theme changes.
    explicit DeviceNavItemWidget(QWidget *parent = nullptr);

    /// Sets the device id (mirrored into the "deviceId" property); no-op if unchanged.
    void setDeviceId(const QString &deviceId);
    /// Returns the device id set via setDeviceId()/setDevice().
    QString deviceId() const;

    /// Sets the visible device-name label text; no-op if unchanged or before initUi().
    void setDeviceName(const QString &name);
    /// Returns the current device-name label text, or an empty string if the label doesn't exist yet.
    QString deviceName() const;

    /// Sets the device type: updates the "deviceType" style property and resets
    /// the type label and icon to their defaults for `type`, then repolishes.
    void setDeviceType(vc::device::DeviceType type);
    /// Returns the device type set via setDeviceType()/setDevice().
    vc::device::DeviceType deviceType() const;

    /// Sets the short type-label text, falling back to defaultTypeLabelFor(m_deviceType)
    /// when `text` is empty; no-op if unchanged.
    void setTypeLabelText(const QString &text);
    /// Returns the current type-label text, or an empty string if the label doesn't exist.
    QString typeLabelText() const;

    /// Sets the device icon rendered at `size` (falls back to 14x14 when invalid);
    /// clears the icon label when `icon` is null.
    void setIcon(const QIcon &icon, const QSize &size = QSize(14, 14));
    /// Loads an SVG icon from `iconPath` and applies it via setIcon(); remembers
    /// `iconPath`/`size` so the icon can be re-rendered on theme changes. Clears
    /// the icon when `iconPath` is empty.
    void setIconPath(const QString &iconPath, const QSize &size = QSize(14, 14));

    /// Sets the "navActive" style property used by QSS to highlight the
    /// selected item, then repolishes; no-op if already in that state.
    void setSelected(bool selected);
    /// Returns whether this item is currently marked selected.
    bool isSelected() const;

    /// Sets the indicator dot's "lampState" property (falls back to "off" when
    /// `state` is empty) and repolishes it; no-op if unchanged.
    void setIndicatorState(const QString &state);
    /// Returns the indicator dot's current "lampState" property value, or an
    /// empty string if the dot doesn't exist.
    QString indicatorState() const;
    /// Maps `status` to an indicator state via indicatorStateFor() and applies it.
    void setConnectStatus(vc::device::ConnectStatus status);

    /// Copies id, name, type, and connect status from `device` into this widget;
    /// no-op if `device` is null.
    void setDevice(const std::shared_ptr<vc::device::IDevice> &device);

    /// Returns the indicator dot widget.
    DeviceNavDot *indicatorDot() const;

    /// Repolishes this widget and its name/type labels so QSS reflects the
    /// latest style properties.
    void refreshStyle();

    /// Maps a device type to its QSS "deviceType" role string.
    static QString roleForDeviceType(vc::device::DeviceType type);
    /// Maps a device type to its default short type-label text (e.g. "CAM", "PLC").
    static QString defaultTypeLabelFor(vc::device::DeviceType type);
    /// Maps a device type to its default SVG icon resource path.
    static QString defaultIconPathFor(vc::device::DeviceType type);
    /// Maps a connect status to an indicator lamp state: "on" for Connected,
    /// "warn" for Connecting, "off" for every other status.
    static QString indicatorStateFor(vc::device::ConnectStatus status);

signals:
    /// Emitted with this item's device id when the item is left-clicked.
    void activated(const QString &deviceId);

protected:
    /// Emits activated() on a left-click (when a device id is set) before
    /// forwarding the event to QFrame::mousePressEvent().
    void mousePressEvent(QMouseEvent *event) override;

private:
    /// Builds the icon box (icon label + indicator dot), the name/type label
    /// column, and the row layout; sets the pointing-hand cursor and initial
    /// style properties.
    void initUi();

private:
    QString m_deviceId;    ///< Device id set via setDeviceId()/setDevice(); emitted by activated().
    QString m_iconPath;    ///< Resource path of the last icon set via setIconPath(); re-applied on theme change.
    QSize m_iconSize{14, 14};   ///< Size the icon is rendered at; set alongside m_iconPath.
    vc::device::DeviceType m_deviceType{vc::device::DeviceType::UserType};  ///< Device type set via setDeviceType()/setDevice().
    bool m_selected{false}; ///< Whether setSelected(true) was last called.
    QLabel *m_iconLabel{nullptr};   ///< Displays the device icon.
    QLabel *m_nameLabel{nullptr};   ///< Displays the device name.
    QLabel *m_typeLabel{nullptr};   ///< Displays the short device-type text.
    DeviceNavDot *m_indicatorDot{nullptr}; ///< Small connect-status dot overlaid on the icon.
};
