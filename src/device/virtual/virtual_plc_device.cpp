#include "device/virtual/virtual_plc_device.h"

#include <memory>

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
    // A real PLC starts polling on connect and publishes its whole register space; this one has
    // to do the same or it is the only PLC family that can never answer "what do you hold right
    // now?" — which is the question setup() asks to resolve the active selection (Phase 9 / C6).
    // Publishing whatever has been driven so far, including nothing, is the honest answer.
    emit pollingUpdate(std::make_shared<VirtualPlcValueMap>(m_inputValues));
    return true;
}

/// Reports Disconnected.
bool VirtualPlcDevice::deviceDisconnect()
{
    setConnectionStatus(ConnectStatus::Disconnected);
    return true;
}

/// Queues a connection-status change through the event loop. See the header for why it is
/// queued rather than applied in place.
void VirtualPlcDevice::forceConnectionStatus(ConnectStatus status)
{
    QMetaObject::invokeMethod(this, [this, status]() {
        setConnectionStatus(status);
    }, Qt::QueuedConnection);
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
namespace {
/// Consumes one refusal for `tag` from `budget`, if one is owed.
/// @return true if this write should be refused
bool consumeWriteRefusal(QMap<QString, int> &budget, const QString &tag)
{
    const auto it = budget.find(tag);
    if (it == budget.end() || it.value() == 0) {
        return false;
    }
    if (it.value() > 0) {
        it.value() -= 1;
    }
    return true;   // a negative budget refuses forever
}
} // namespace

bool VirtualPlcDevice::writeDigitalIoByName(const QString &tag, bool value)
{
    if (!isMcTag(tag, QLatin1Char('M'))) {
        return false;
    }
    // Checked AFTER the tag is validated, so a forced refusal is a refusal of a write that would
    // otherwise have succeeded — which is the failure the handshake write policy is about.
    if (consumeWriteRefusal(failWritesForTag, tag)) {
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
    if (consumeWriteRefusal(failWritesForTag, tag)) {
        return false;
    }
    wordWrites.insert(tag, value);
    return true;
}

/// Drives an input tag and publishes it through valueChanged(), as if the PLC had changed it.
bool VirtualPlcDevice::injectInputValue(const QString &tag, const QVariant &value, QString *error)
{
    const QString trimmed = tag.trimmed();

    // Refused while disconnected, because a real PLC cannot deliver a value over a link that is
    // down. Reported here rather than silently dropped: the runtime would refuse to act on it
    // anyway (markRuntimeReady needs every role healthy), and a poke that vanishes with no
    // explanation reads as a broken panel rather than a disconnected device.
    if (!isDeviceConnected()) {
        if (error) {
            *error = QStringLiteral("Virtual PLC is not connected; input %1 was not delivered.")
                         .arg(trimmed);
        }
        return false;
    }

    // Coerced by prefix, not by the QVariant's own type. A bit area yields a bool and a register
    // area a number on real hardware, and LocalizationRuntimeController relies on exactly that to
    // tell "bad index" apart from "this number signal is mapped to a coil".
    QVariant published;
    if (isMcTag(trimmed, QLatin1Char('M'))) {
        published = value.toBool();
    } else if (isMcTag(trimmed, QLatin1Char('D'))) {
        bool ok = false;
        const int number = value.toInt(&ok);
        if (!ok) {
            if (error) {
                *error = QStringLiteral("Input %1 is a word tag and needs a number.").arg(trimmed);
            }
            return false;
        }
        published = static_cast<int>(static_cast<qint16>(number));
    } else {
        // As strict as the write path, and for the same reason: a virtual PLC that accepted any
        // tag would let a signal-mapping mistake pass here and fail only on real hardware.
        if (error) {
            *error = QStringLiteral("%1 is not a valid tag. Use M<address> or D<address>.")
                         .arg(trimmed);
        }
        return false;
    }

    m_inputValues.insert(trimmed, published);
    emit valueChanged({{trimmed, published}});
    // The snapshot too, not only the change. valueChanged() answers "what just moved"; a
    // consumer that arrives later — setup(), resolving the active selection — needs "what does
    // the PLC hold right now", and on a real device that is what pollingUpdate() carries.
    emit pollingUpdate(std::make_shared<VirtualPlcValueMap>(m_inputValues));
    return true;
}

} // namespace vc::device
