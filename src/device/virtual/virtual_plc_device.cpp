#include "device/virtual/virtual_plc_device.h"

#include "core/logger/app_logger.h"

namespace vc::device {

namespace {

/// True when `tag` is `<prefix><decimal address>` — e.g. "M100" for a bit, "D200" for a word.
///
/// Deliberately strict. Accepting anything here would let a signal-mapping mistake pass on a
/// virtual PLC and fail only on real hardware, which is where it is most expensive to find.
bool isMcTag(const QString &tag, QChar expectedPrefix)
{
    const QString trimmed = tag.trimmed();
    if (trimmed.size() < 2 ||
        trimmed.at(0).toUpper() != expectedPrefix.toUpper()) {
        return false;
    }

    bool ok = false;
    const int address = trimmed.mid(1).toInt(&ok, 10);
    return ok && address >= 0;
}

} // namespace

VirtualPlcDevice::VirtualPlcDevice(const QString &id, const QString &name, QObject *parent)
    : PlcDevice(id, name, parent)
{
    // Publish the address of the owned config to the base class, which keeps a NON-owning
    // pointer and serialises through it. Without this the device saves an empty DeviceConfig
    // and loses its sub-type token on reload. Same call, same reason, as
    // camera_basler_gige.cpp:103.
    this->blockSignals(true);
    IDevice::setDeviceConfig(&m_config);
    this->blockSignals(false);
}

/// Reports Connected. There is nothing to reach.
bool VirtualPlcDevice::deviceConnect()
{
    setConnectionStatus(ConnectStatus::Connected);
    return true;
}

/// Reports Disconnected.
bool VirtualPlcDevice::deviceDisconnect()
{
    setConnectionStatus(ConnectStatus::Disconnected);
    return true;
}

/// True while the connection status is Connected.
bool VirtualPlcDevice::isDeviceConnected() const
{
    return connectStatus() == ConnectStatus::Connected;
}

namespace {

/// Builds `<prefix>0 … <prefix><count-1>`.
QStringList tagRange(QChar prefix, int count)
{
    QStringList tags;
    tags.reserve(count);
    for (int address = 0; address < count; ++address) {
        tags << (QString(prefix) + QString::number(address));
    }
    return tags;
}

} // namespace

/// Returns `M0 … M<digitalTagCount-1>`.
QStringList VirtualPlcDevice::availableDigitalIoNames() const
{
    return tagRange(QLatin1Char('M'), m_config.digitalTagCount());
}

/// Returns `D0 … D<wordTagCount-1>`.
QStringList VirtualPlcDevice::availableWordIoNames() const
{
    return tagRange(QLatin1Char('D'), m_config.wordTagCount());
}

/// Takes ownership of `cfg`, copies it into the owned config, and re-publishes that.
void VirtualPlcDevice::setDeviceConfig(IDeviceCfg *cfg)
{
    if (auto *virtualCfg = dynamic_cast<VirtualPlcCfg *>(cfg)) {
        m_config = *virtualCfg;
    } else if (cfg != nullptr) {
        LOG_DEV_ERR << "Virtual PLC was given a config it cannot use; ignored.";
    }
    delete cfg;
    // Re-publish the owned member, not the caller's object: the base holds a non-owning
    // pointer and the one it was given has just been deleted.
    IDevice::setDeviceConfig(&m_config);
}

/// Records `value` under `tag` if it is a valid M-address; rejects it otherwise.
bool VirtualPlcDevice::writeDigitalIoByName(const QString &tag, bool value)
{
    if (!isMcTag(tag, QLatin1Char('M'))) {
        return false;
    }
    digitalWrites.insert(tag, value);
    return true;
}

/// Records `value` under `tag` if it is a valid D-address; rejects it otherwise.
bool VirtualPlcDevice::writeWordIoByName(const QString &tag, qint16 value)
{
    if (!isMcTag(tag, QLatin1Char('D'))) {
        return false;
    }
    wordWrites.insert(tag, value);
    return true;
}

} // namespace vc::device
