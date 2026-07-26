#include "nachi_robot_device.h"

/// NachiRobotDevice implementation: constructs the device with its config, and stubs
/// out connect/disconnect/request handling until the Nachi vendor integration lands.
namespace vc::device {

/// Constructs the device with the given id/name and registers m_cfg as its device
/// configuration via setDeviceConfig().
NachiRobotDevice::NachiRobotDevice(QString id, QString name, QObject* parent)
    : RobotDevice(id, name, parent) {
    setDeviceConfig(&m_cfg);
}

/// Stub: vendor integration is pending; always returns false without connecting.
bool NachiRobotDevice::deviceConnect() {
    // Not implemented yet — vendor integration pending.
    return false;
}

/// Stub: always returns true without doing anything.
bool NachiRobotDevice::deviceDisconnect() {
    return true;
}

/// Returns the last-known connection state (m_connected); currently never set true
/// since deviceConnect() is a stub.
bool NachiRobotDevice::isDeviceConnected() const {
    return m_connected;
}

/// Stub: ignores `request` and always returns false.
bool NachiRobotDevice::pushRequest(IRequest *request) {
    Q_UNUSED(request);
    return false;
}

/// Terminates the device by calling deviceDisconnect().
void NachiRobotDevice::deviceTerminate() {
    deviceDisconnect();
}

} // namespace vc::device
