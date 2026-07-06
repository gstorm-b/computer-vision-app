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

QFont deviceNavNameFont()
{
    QFont font(QStringLiteral("JetBrains Mono"));
    if (!font.exactMatch())
        font.setFamily(QStringLiteral("Consolas"));
    font.setPointSizeF(9);
    return font;
}

QFont deviceNavTypeFont()
{
    QFont font;
    font.setPointSizeF(7.5);
    font.setBold(true);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.4);
    return font;
}

void repolish(QWidget *widget)
{
    if (!widget)
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

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

void DeviceNavItemWidget::setDeviceId(const QString &deviceId)
{
    if (m_deviceId == deviceId)
        return;
    m_deviceId = deviceId;
    setProperty("deviceId", deviceId);
}

QString DeviceNavItemWidget::deviceId() const
{
    return m_deviceId;
}

void DeviceNavItemWidget::setDeviceName(const QString &name)
{
    if (m_nameLabel && m_nameLabel->text() == name)
        return;
    if (m_nameLabel)
        m_nameLabel->setText(name);
}

QString DeviceNavItemWidget::deviceName() const
{
    return m_nameLabel ? m_nameLabel->text() : QString();
}

void DeviceNavItemWidget::setDeviceType(vc::device::DeviceType type)
{
    m_deviceType = type;
    setProperty("deviceType", roleForDeviceType(type));
    setTypeLabelText(defaultTypeLabelFor(type));
    setIconPath(defaultIconPathFor(type));
    refreshStyle();
}

vc::device::DeviceType DeviceNavItemWidget::deviceType() const
{
    return m_deviceType;
}

void DeviceNavItemWidget::setTypeLabelText(const QString &text)
{
    if (!m_typeLabel)
        return;

    const QString resolvedText = text.isEmpty() ? defaultTypeLabelFor(m_deviceType) : text;
    if (m_typeLabel->text() == resolvedText)
        return;
    m_typeLabel->setText(resolvedText);
}

QString DeviceNavItemWidget::typeLabelText() const
{
    return m_typeLabel ? m_typeLabel->text() : QString();
}

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

void DeviceNavItemWidget::setIconPath(const QString &iconPath, const QSize &size)
{
    m_iconPath = iconPath;
    if (iconPath.isEmpty()) {
        setIcon(QIcon(), size);
        return;
    }
    setIcon(svgIcon(iconPath), size);
}

void DeviceNavItemWidget::setSelected(bool selected)
{
    if (m_selected == selected && property("navActive").toBool() == selected)
        return;

    m_selected = selected;
    setProperty("navActive", selected);
    refreshStyle();
}

bool DeviceNavItemWidget::isSelected() const
{
    return m_selected;
}

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

QString DeviceNavItemWidget::indicatorState() const
{
    return m_indicatorDot ? m_indicatorDot->property("lampState").toString()
                          : QString();
}

void DeviceNavItemWidget::setConnectStatus(vc::device::ConnectStatus status)
{
    setIndicatorState(indicatorStateFor(status));
}

void DeviceNavItemWidget::setDevice(const std::shared_ptr<vc::device::IDevice> &device)
{
    if (!device)
        return;

    setDeviceId(device->id());
    setDeviceName(device->name());
    setDeviceType(device->deviceType());
    setConnectStatus(device->connectStatus());
}

DeviceNavDot *DeviceNavItemWidget::indicatorDot() const
{
    return m_indicatorDot;
}

void DeviceNavItemWidget::refreshStyle()
{
    repolish(this);
    repolish(m_nameLabel);
    repolish(m_typeLabel);
}

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

void DeviceNavItemWidget::mousePressEvent(QMouseEvent *event)
{
    if (event && event->button() == Qt::LeftButton && !m_deviceId.isEmpty())
        emit activated(m_deviceId);
    QFrame::mousePressEvent(event);
}

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
