#include "kawasaki_robot_device.h"

namespace vc::device {

/// Constructs the device and registers its embedded KawasakiRobotCfg as the
/// device's config object via setDeviceConfig().
KawasakiRobotDevice::KawasakiRobotDevice(QString id, QString name, QObject* parent)
    : RobotDevice(id, name, parent) {
    setDeviceConfig(&m_cfg);
}

/// Stub for the vendor connection sequence; always fails since the Kawasaki
/// integration is not implemented yet.
/// @return false unconditionally
bool KawasakiRobotDevice::deviceConnect() {
    // Not implemented yet — vendor integration pending.
    return false;
}

/// Stub disconnect; there is no live connection to tear down yet.
/// @return true unconditionally
bool KawasakiRobotDevice::deviceDisconnect() {
    return true;
}

/// Returns the last-known connection state tracked in m_connected.
bool KawasakiRobotDevice::isDeviceConnected() const {
    return m_connected;
}

/// Stub request handler; the vendor protocol is not implemented yet so every
/// request is rejected without being queued or sent.
/// @param request the request to push (ignored)
/// @return false unconditionally
bool KawasakiRobotDevice::pushRequest(IRequest *request) {
    Q_UNUSED(request);
    return false;
}

/// Terminates the device by disconnecting it.
void KawasakiRobotDevice::deviceTerminate() {
    deviceDisconnect();
}

} // namespace vc::device
