#ifndef DEVICE_CAPABILITIES_H
#define DEVICE_CAPABILITIES_H

/**
 * @file device_capabilities.h
 * @brief Device-capability mix-in interfaces: optional facets a concrete device can implement
 *        in addition to its base IDevice/IDeviceCfg contract, queryable via dynamic_cast.
 */

#include "device/output_device/vision_output_request.h"
#include "device/robot_kinematic_check_config.h"

#include <QMap>
#include <QStringList>
#include <QVariant>

namespace vc::device {

/**
 * @class IDigitalIoProvider
 * @brief Capability mix-in for devices that expose named digital (bit) I/O tags/lines.
 */
class IDigitalIoProvider {
public:
    virtual ~IDigitalIoProvider() = default;

    /// Returns the names of all digital I/O tags/lines currently available on the device.
    virtual QStringList availableDigitalIoNames() const = 0;
};

/**
 * @class IWordIoProvider
 * @brief Capability mix-in for devices that expose named word (register-sized) I/O tags.
 */
class IWordIoProvider {
public:
    virtual ~IWordIoProvider() = default;

    /// Returns the names of all word I/O tags currently available on the device.
    virtual QStringList availableWordIoNames() const = 0;
};

/**
 * @class IPlcTagProvider
 * @brief Combined capability for PLC-like devices that expose both digital and word tags by name.
 */
class IPlcTagProvider : public IDigitalIoProvider, public IWordIoProvider {
public:
    ~IPlcTagProvider() override = default;
};

/**
 * @class IPlcIoWriter
 * @brief Capability mix-in for devices that accept writes to named PLC I/O tags.
 */
class IPlcIoWriter {
public:
    virtual ~IPlcIoWriter() = default;

    /// Writes `value` to the digital I/O tag identified by `tag`.
    /// @return true if the write succeeded.
    virtual bool writeDigitalIoByName(const QString &tag, bool value) = 0;

    /// Writes `value` to the word I/O tag identified by `tag`.
    /// @return true if the write succeeded.
    virtual bool writeWordIoByName(const QString &tag, qint16 value) = 0;
};

/**
 * @class IPlcInputSimulator
 * @brief Capability for a PLC-like device whose **input** values can be driven by this software
 *        instead of by a machine on the other end.
 *
 * Real hardware never implements this: a real PLC's inputs are driven by the PLC program, and a
 * device that let the vision software forge them would be lying about the plant. It exists for the
 * hardware-free devices, so a project with no PLC on the network can still be taken past Ready —
 * every runtime state after Ready begins with the PLC changing an input (`bExecuteTrigger`,
 * `bErrorReset`, `nActiveCamera`, `nActivePatternGroup`), and without a way to produce those, every
 * transition the task state machine has is unreachable, including the fault paths that matter most.
 *
 * @warning **Injected inputs and recorded writes are separate stores, deliberately.** A device
 *          implementing this must NOT let a value written through IPlcIoWriter read back as an
 *          input. Echoing our own writes would make the handshake self-completing — the runtime
 *          publishes `bMatchingFinished`, reads it back as an input, and appears to work — and it
 *          would hide the mapping mistake where two logical signals are bound to the same tag.
 *          Real hardware keeps them separate because the plant, not the vision system, owns the
 *          inputs; the simulation has to keep the same shape or it stops proving anything.
 */
class IPlcInputSimulator {
public:
    virtual ~IPlcInputSimulator() = default;

    /**
     * @brief Drives the input tag `tag` to `value` and publishes it as if the PLC had changed it.
     * @param[in]  tag   the tag to drive, in the device's own tag vocabulary.
     * @param[out] error human-readable reason on refusal; may be null.
     * @return true if the tag was accepted and published.
     * @note Called on the device's own worker thread by the runner, never directly from the GUI
     *       thread — the device owns thread-affine state and the publish is a signal emission.
     * @post On success the device emits PlcDevice::valueChanged() carrying `tag`.
     */
    virtual bool injectInputValue(const QString &tag, const QVariant &value,
                                  QString *error = nullptr) = 0;

    /// Returns every input tag currently being driven, and its value. The device is the source of
    /// truth: a UI panel rebuilt from scratch has to be able to show what is already held.
    virtual QMap<QString, QVariant> simulatedInputs() const = 0;
};

/**
 * @class IImageSourceDevice
 * @brief Marker capability for devices that can act as a source of images (e.g. cameras).
 */
class IImageSourceDevice {
public:
    virtual ~IImageSourceDevice() = default;
};

/**
 * @class IResultOutputDevice
 * @brief Capability for devices that can carry the localization task's `vision_output` role:
 *        send a cycle's result positions out, and supply the robot pick-check settings that
 *        gate which positions are sent at all.
 *
 * This is what makes the role a capability rather than a device family. `LocalizationRuntimeController`
 * and `TaskLocalization::buildRuntimeContext()` reach the role through this interface only, so a
 * device from any family — a vision-output transport, a PLC writing into registers — can serve it
 * as long as it implements both methods.
 *
 * @warning The pairing of the two methods is **historical**, and the history is worth keeping.
 *          robotKinematicCheckConfig() used to be read by casting the bound device's config to
 *          `VisionOutputDeviceCfg`; for any other family that cast fails and the commissioned pick
 *          check silently reverted to disabled. Putting both methods on one interface fixed *which*
 *          families could answer, but not the deeper problem — that the answer came from a device
 *          at all. Phase 9 / F1 moved the setting to the task
 *          (`TaskLocalizeConfig::robotCheckConfig()`), so `buildRuntimeContext()` now reaches this
 *          role through sendVisionResult() only. See robotKinematicCheckConfig() below for who
 *          still calls it.
 */
class IResultOutputDevice {
public:
    virtual ~IResultOutputDevice() = default;

    /**
     * @brief Sends one localization cycle's result positions to whatever this device outputs to.
     * @param[in]  positions the cycle's result positions, in send order.
     * @param[out] message   human-readable success/failure detail; may be null.
     * @return true if the device accepted and sent the result.
     * @note Called on the device's own worker thread by the runner, never directly from the GUI
     *       or runtime thread.
     */
    virtual bool sendVisionResult(const QVector<VisionOutputPosition> &positions,
                                  QString *message) = 0;

    /**
     * @brief Returns the robot pick-check settings commissioned on this device.
     *
     * @warning **The localization task no longer reads this.** Since Phase 9 / F1 the pick check
     *          belongs to `TaskLocalizeConfig::robotCheckConfig()` — see backlog item 57.
     *          Routing it through the device made the setting a property of the transport: a
     *          device with nothing commissioned answers "disabled", and that answer is
     *          indistinguishable from "this cell does not want the check", so binding a PLC to
     *          the `vision_output` role silently un-commissioned a gate the cell had been signed
     *          off with. Moving the setting to the task removes the device from the question
     *          entirely, which no answer given here could have done.
     *
     * What still uses it is the **device-side advisory check** in
     * `VisionTcpipDeviceBase::runKinematicCheck()`, which reads its own device config. This
     * method is kept because both Modbus devices and the vision-output family implement it;
     * removing a method with live implementors is a separate decision, filed in Z3.
     *
     * @return the settings as commissioned on this device.
     * @note Returned by value, not by reference: the settings may live in a config the device
     *       clones on access.
     */
    virtual RobotKinematicCheckConfig robotKinematicCheckConfig() const = 0;
};

} // namespace vc::device

#endif // DEVICE_CAPABILITIES_H
