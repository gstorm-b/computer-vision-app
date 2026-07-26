#include "device_nav_item_widget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>
#include <QVBoxLayout>

#include "ui/widgets/controls/device_nav_dot.h"
#include "core/utils/theme_manager.h"
#include "core/utils/windows_helper.h"

namespace {

/// Returns the font used for the device-name label: JetBrains Mono at 9pt,
/// falling back to Consolas when JetBrains Mono is not installed.
QFont deviceNavNameFont()
{
    QFont font(QStringLiteral("JetBrains Mono"));
    if (!font.exactMatch())
        font.setFamily(QStringLiteral("Consolas"));
    font.setPointSizeF(9);
    return font;
}

/// Returns the font used for the short device-type label: bold 7.5pt with
/// slightly widened letter spacing.
QFont deviceNavTypeFont()
{
    QFont font;
    font.setPointSizeF(7.5);
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.4);
    return font;
}

/// Forces `widget` to re-evaluate its QSS (unpolish + polish) and repaint; no-op if null.
void repolish(QWidget *widget)
{
    if (!widget)
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

/// Builds the icon/name/type layout, applies default device type, unselected
/// state, and "off" indicator, and re-applies the current icon on theme changes.
DeviceNavItemWidget::DeviceNavItemWidget(QWidget *parent)
    : QFrame(parent)
{
    initUi();
    setDeviceType(vc::device::DeviceType::UserType);
    setSelected(false);
    setIndicatorState(QStringLiteral("off"));

    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &, bool) {
        if (!m_iconPath.isEmpty())
            setIconPath(m_iconPath, m_iconSize);
    });
}

/// Sets the device id (mirrored into the "deviceId" property); no-op if unchanged.
void DeviceNavItemWidget::setDeviceId(const QString &deviceId)
{
    if (m_deviceId == deviceId)
        return;
    m_deviceId = deviceId;
    setProperty("deviceId", deviceId);
}

/// Returns the device id set via setDeviceId()/setDevice().
QString DeviceNavItemWidget::deviceId() const
{
    return m_deviceId;
}

/// Sets the visible device-name label text; no-op if unchanged or before initUi().
void DeviceNavItemWidget::setDeviceName(const QString &name)
{
    if (m_nameLabel && m_nameLabel->text() == name)
        return;
    if (m_nameLabel)
        m_nameLabel->setText(name);
}

/// Returns the current device-name label text, or an empty string if the label doesn't exist yet.
QString DeviceNavItemWidget::deviceName() const
{
    return m_nameLabel ? m_nameLabel->text() : QString();
}

/// Sets the device type: updates the "deviceType" style property and resets
/// the type label and icon to their defaults for `type`, then repolishes.
void DeviceNavItemWidget::setDeviceType(vc::device::DeviceType type)
{
    m_deviceType = type;
    setProperty("deviceType", roleForDeviceType(type));
    setTypeLabelText(defaultTypeLabelFor(type));
    setIconPath(defaultIconPathFor(type));
    refreshStyle();
}

/// Returns the device type set via setDeviceType()/setDevice().
vc::device::DeviceType DeviceNavItemWidget::deviceType() const
{
    return m_deviceType;
}

/// Sets the short type-label text, falling back to defaultTypeLabelFor(m_deviceType)
/// when `text` is empty; no-op if unchanged.
void DeviceNavItemWidget::setTypeLabelText(const QString &text)
{
    if (!m_typeLabel)
        return;

    const QString resolvedText = text.isEmpty() ? defaultTypeLabelFor(m_deviceType) : text;
    if (m_typeLabel->text() == resolvedText)
        return;
    m_typeLabel->setText(resolvedText);
}

/// Returns the current type-label text, or an empty string if the label doesn't exist.
QString DeviceNavItemWidget::typeLabelText() const
{
    return m_typeLabel ? m_typeLabel->text() : QString();
}

/// Sets the device icon rendered at `size` (falls back to 14x14 when invalid);
/// clears the icon label when `icon` is null.
void DeviceNavItemWidget::setIcon(const QIcon &icon, const QSize &size)
{
    if (!m_iconLabel)
        return;

    m_iconSize = size.isValid() ? size : QSize(14, 14);
    if (icon.isNull()) {
        m_iconLabel->clear();
        return;
    }
    m_iconLabel->setPixmap(icon.pixmap(m_iconSize));
}

/// Loads an SVG icon from `iconPath` and applies it via setIcon(); remembers
/// `iconPath`/`size` so the icon can be re-rendered on theme changes. Clears
/// the icon when `iconPath` is empty.
void DeviceNavItemWidget::setIconPath(const QString &iconPath, const QSize &size)
{
    m_iconPath = iconPath;
    if (iconPath.isEmpty()) {
        setIcon(QIcon(), size);
        return;
    }
    setIcon(svgIcon(iconPath), size);
}

/// Sets the "navActive" style property used by QSS to highlight the selected
/// item, then repolishes; no-op if already in that state.
void DeviceNavItemWidget::setSelected(bool selected)
{
    if (m_selected == selected && property("navActive").toBool() == selected)
        return;

    m_selected = selected;
    setProperty("navActive", selected);
    refreshStyle();
}

/// Returns whether this item is currently marked selected.
bool DeviceNavItemWidget::isSelected() const
{
    return m_selected;
}

/// Sets the indicator dot's "lampState" property (falls back to "off" when
/// `state` is empty) and repolishes it; no-op if unchanged.
void DeviceNavItemWidget::setIndicatorState(const QString &state)
{
    if (!m_indicatorDot)
        return;

    const QString resolvedState = state.isEmpty() ? QStringLiteral("off") : state;
    if (m_indicatorDot->property("lampState").toString() == resolvedState)
        return;

    m_indicatorDot->setProperty("lampState", resolvedState);
    repolish(m_indicatorDot);
}

/// Returns the indicator dot's current "lampState" property value, or an
/// empty string if the dot doesn't exist.
QString DeviceNavItemWidget::indicatorState() const
{
    return m_indicatorDot ? m_indicatorDot->property("lampState").toString()
                          : QString();
}

/// Maps `status` to an indicator state via indicatorStateFor() and applies it.
void DeviceNavItemWidget::setConnectStatus(vc::device::ConnectStatus status)
{
    setIndicatorState(indicatorStateFor(status));
}

/// Copies id, name, type, and connect status from `device` into this widget;
/// no-op if `device` is null.
void DeviceNavItemWidget::setDevice(const std::shared_ptr<vc::device::IDevice> &device)
{
    if (!device)
        return;

    setDeviceId(device->id());
    setDeviceName(device->name());
    setDeviceType(device->deviceType());
    setConnectStatus(device->connectStatus());
}

/// Returns the indicator dot widget.
DeviceNavDot *DeviceNavItemWidget::indicatorDot() const
{
    return m_indicatorDot;
}

/// Repolishes this widget and its name/type labels so QSS reflects the
/// latest style properties.
void DeviceNavItemWidget::refreshStyle()
{
    repolish(this);
    repolish(m_nameLabel);
    repolish(m_typeLabel);
}

/// Maps a device type to its QSS "deviceType" role string.
QString DeviceNavItemWidget::roleForDeviceType(vc::device::DeviceType type)
{
    switch (type) {
    case vc::device::DeviceType::Camera:
        return QStringLiteral("camera");
    case vc::device::DeviceType::PLC:
        return QStringLiteral("plc");
    case vc::device::DeviceType::VisionOutput:
        return QStringLiteral("output");
    case vc::device::DeviceType::Robot:
        return QStringLiteral("robot");
    case vc::device::DeviceType::UserType:
    default:
        return QStringLiteral("default");
    }
}

/// Maps a device type to its default short type-label text (e.g. "CAM", "PLC").
QString DeviceNavItemWidget::defaultTypeLabelFor(vc::device::DeviceType type)
{
    switch (type) {
    case vc::device::DeviceType::Camera:
        return QStringLiteral("CAM");
    case vc::device::DeviceType::PLC:
        return QStringLiteral("PLC");
    case vc::device::DeviceType::VisionOutput:
        return QStringLiteral("OUT");
    case vc::device::DeviceType::Robot:
        return QStringLiteral("BOT");
    case vc::device::DeviceType::UserType:
    default:
        return QStringLiteral("DEV");
    }
}

/// Maps a device type to its default SVG icon resource path.
QString DeviceNavItemWidget::defaultIconPathFor(vc::device::DeviceType type)
{
    switch (type) {
    case vc::device::DeviceType::Camera:
        return QStringLiteral(":/resrc/icon/camera.svg");
    case vc::device::DeviceType::PLC:
        return QStringLiteral(":/resrc/icon/plc_icon.svg");
    case vc::device::DeviceType::VisionOutput:
        return QStringLiteral(":/resrc/icon/plug_connected.svg");
    case vc::device::DeviceType::Robot:
        return QStringLiteral(":/resrc/icon/robot_movement.svg");
    case vc::device::DeviceType::UserType:
    default:
        return QStringLiteral(":/resrc/icon/setting.svg");
    }
}

/// Maps a connect status to an indicator lamp state: "on" for Connected,
/// "warn" for Connecting, "off" for every other status.
QString DeviceNavItemWidget::indicatorStateFor(vc::device::ConnectStatus status)
{
    switch (status) {
    case vc::device::ConnectStatus::Connected:
        return QStringLiteral("on");
    case vc::device::ConnectStatus::Connecting:
        return QStringLiteral("warn");
    case vc::device::ConnectStatus::LostConnected:
    case vc::device::ConnectStatus::ConnectFailed:
    case vc::device::ConnectStatus::Disconnected:
    case vc::device::ConnectStatus::NoConnection:
        return QStringLiteral("off");
    }
    return QStringLiteral("off");
}

/// Emits activated() on a left-click (when a device id is set) before
/// forwarding the event to QFrame::mousePressEvent().
void DeviceNavItemWidget::mousePressEvent(QMouseEvent *event)
{
    if (event && event->button() == Qt::LeftButton && !m_deviceId.isEmpty())
        emit activated(m_deviceId);
    QFrame::mousePressEvent(event);
}

/// Builds the icon box (icon label + indicator dot), the name/type label
/// column, and the row layout; sets the pointing-hand cursor and initial
/// style properties.
void DeviceNavItemWidget::initUi()
{
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setProperty("navItem", true);
    setProperty("navActive", false);

    auto *itemLayout = new QHBoxLayout(this);
    itemLayout->setContentsMargins(7, 7, 7, 7);
    itemLayout->setSpacing(7);

    auto *iconBox = new QWidget(this);
    iconBox->setObjectName(QStringLiteral("devNavIconBox"));
    iconBox->setFixedSize(20, 20);
    iconBox->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_iconLabel = new QLabel(iconBox);
    m_iconLabel->setObjectName(QStringLiteral("devNavIcon"));
    m_iconLabel->move(0, 3);
    m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_indicatorDot = new DeviceNavDot(iconBox);
    m_indicatorDot->setObjectName(QStringLiteral("devNavDot"));
    m_indicatorDot->setFixedSize(5, 5);
    m_indicatorDot->move(13, 13);
    m_indicatorDot->setAttribute(Qt::WA_TransparentForMouseEvents);

    itemLayout->addWidget(iconBox);

    auto *textCol = new QWidget(this);
    textCol->setObjectName(QStringLiteral("devNavTextCol"));
    textCol->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *textLay = new QVBoxLayout(textCol);
    textLay->setContentsMargins(0, 0, 0, 0);
    textLay->setSpacing(1);

    m_nameLabel = new QLabel(textCol);
    m_nameLabel->setObjectName(QStringLiteral("devNavName"));
    m_nameLabel->setFont(deviceNavNameFont());
    m_nameLabel->setProperty("navPart", QStringLiteral("name"));
    m_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_typeLabel = new QLabel(textCol);
    m_typeLabel->setObjectName(QStringLiteral("devNavType"));
    m_typeLabel->setFont(deviceNavTypeFont());
    m_typeLabel->setProperty("navPart", QStringLiteral("type"));
    m_typeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    textLay->addWidget(m_nameLabel);
    textLay->addWidget(m_typeLabel);
    itemLayout->addWidget(textCol, 1);
}
