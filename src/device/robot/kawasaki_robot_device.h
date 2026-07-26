#ifndef KAWASAKI_ROBOT_DEVICE_H
#define KAWASAKI_ROBOT_DEVICE_H

#include "device/robot/robot_device.h"
#include "device/robot/kawasaki_robot_config.h"

namespace vc::device {

/// Minimum concrete robot device for the Kawasaki vendor. Implements only what IDevice
/// requires; all calls are stubs returning sensible defaults until the vendor
/// integration starts.
class KawasakiRobotDevice : public RobotDevice {
    Q_OBJECT

public:
    /// Constructs the device with the given id/name and registers m_cfg as its device
    /// configuration via setDeviceConfig().
    explicit KawasakiRobotDevice(QString id, QString name, QObject* parent = nullptr);
    /// Default destructor.
    ~KawasakiRobotDevice() override = default;

    /// Returns RobotType::Kawasaki.
    RobotType robotType() const override { return RobotType::Kawasaki; }

    /// Stub: vendor integration is pending; always returns false without connecting.
    bool deviceConnect() override;
    /// Stub: always returns true without doing anything.
    bool deviceDisconnect() override;
    /// Returns the last-known connection state (m_connected); currently never set true
    /// since deviceConnect() is a stub.
    bool isDeviceConnected() const override;

    /// Stub: ignores `request` and always returns false.
    bool pushRequest(IRequest *request) override;

public slots:
    /// Terminates the device by calling deviceDisconnect().
    void deviceTerminate() override;

private:
    KawasakiRobotCfg m_cfg;    ///< Device configuration registered via setDeviceConfig().
    bool m_connected{false};   ///< Connection state; never set true by the current stub implementation.
};

} // namespace vc::device

#endif // KAWASAKI_ROBOT_DEVICE_H
