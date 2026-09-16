#ifndef CAMERA_JAI_GIGE_H
#define CAMERA_JAI_GIGE_H

/**
 * @file camera_jai_gige.h
 * @brief JAI GigE camera config (JaiGigeCfg) and CameraDevice implementation (JaiGigECamera)
 *        built on the Pleora eBUS SDK for GigE Vision cameras.
 */

#include "device/idevice.h"
#include "device/camera/camera_device.h"
#include "device/camera/jai_define.h"

#include "core/qgadget_macro.h"

#include <QElapsedTimer>
#include <QList>

// Every SDK type this header names must be forward-declared HERE, at global scope. Writing
// `class PvBuffer *` inside a member declaration below declares `vc::device::PvBuffer` instead
// and fails to match the SDK's type — the error points at the member, not at the declaration.
// Add new SDK types to this list rather than elaborating them inline; that shortcut has already
// cost two build failures in this file.
class PvAcquisitionStateManager;
class PvBuffer;
class PvDevice;
class PvStream;
class PvPipeline;
class PvGenParameterArray;
class PvDeviceGEV;

namespace vc::device {

class JaiLinkEventSink;   ///< Defined in the .cpp; forwards eBUS link events onto this thread.

/**
 * @class JaiGigeCfg
 * @brief JAI GigE camera configuration: exposure/gain/frame-rate/backlight parameters plus
 *        camera identity (model/serial/IP), exposed as Q_GADGET properties for the
 *        property-browser UI.
 */
class JaiGigeCfg : public CameraCfg {
    Q_GADGET

    G_PROPERTY_STRING_READ(QString, modelName, "Model name")                                       ///< Camera model name (read-only, from device info).
    G_PROPERTY_STRING_READ(QString, userDefinedName, "User-defined name")                          ///< User-defined camera name (read-only, from device info).
    G_PROPERTY_STRING_READ(QString, serialNumber, "Serial number")                                 ///< Camera serial number (read-only, from device info).
    G_PROPERTY_STRING_READWRITE(QString, ipAddress, "IP Address")                                  ///< IP address used to locate the camera on connect.
    G_PROPERTY_STRING_READ(QString, pixelFormat, "Pixel format")                                   ///< GenICam PixelFormat symbolic read from the camera on connect.

    G_PROPERTY_ENUM_READWRITE(vc::device::jai::JaiExposureMode, autoExposureMode, "Exposure Mode") ///< Auto-exposure mode (Off/Once/Continuous).
    G_PROPERTY_NUMBER_READWRITE(double, paramsExposureTime, 0.0, 10000000.0, "Exposure time")       ///< Configured exposure time, microseconds.

    G_PROPERTY_NUMBER_READWRITE(double, paramsGain, 0.0, 10000.0, "Gain")                          ///< Configured gain.

    G_PROPERTY_BOOL_READWRITE(bool, enableAcquisitionFrameRate, "Enable acquisition rate")         ///< Enables acquisition frame-rate limiting.
    G_PROPERTY_NUMBER_READWRITE(double, paramsAcquisitionFrameRate, 1.0, 1000.0, "Acquisition rate") ///< Configured acquisition frame rate.

    G_PROPERTY_BOOL_READWRITE(bool, autoBacklightControl, "Enable auto backlight")                 ///< Enables automatic backlight output around a grab.
    G_PROPERTY_STRING_READWRITE(QString, autoBacklightLine, "Backlight line")                      ///< Digital output line driven for backlight control.
    G_PROPERTY_NUMBER_READWRITE(int, autoBacklightDelay, 0, 10000000, "Back light delay (us)")      ///< Delay (microseconds) after enabling backlight before grabbing.
    G_PROPERTY_BOOL_READWRITE(bool, autoBacklightInvert, "Line invert")                            ///< Inverts the backlight output line's logic level.

    G_PROPERTY_NUMBER_READWRITE(int, grabTimeoutMs, 100, 4000, "Grab timeout (ms)")                 ///< Blocking-grab timeout; see JaiGigECamera::kDefaultGrabTimeoutMs.

    /// Translation markers for the display names above. Not read by any code: lupdate cannot see
    /// Q_CLASSINFO, so without this table these labels never enter the .ts. The context must be
    /// this class's className(); the contract test asserts this list and the properties agree.
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Model name"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "User-defined name"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Serial number"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "IP Address"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Pixel format"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Exposure Mode"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Exposure time"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Gain"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Enable acquisition rate"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Acquisition rate"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Enable auto backlight"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Backlight line"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Back light delay (us)"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Line invert"),
        QT_TRANSLATE_NOOP("vc::device::JaiGigeCfg", "Grab timeout (ms)"),
    };

public:
    explicit JaiGigeCfg() : CameraCfg() {}

    /// Returns this class's Qt meta-object, used by the gadget property-browser machinery.
    const QMetaObject &getMetaObject() const override {
        return vc::device::JaiGigeCfg::staticMetaObject;
    }

    /// Always DeviceType::Camera.
    DeviceType deviceType() const override { return DeviceType::Camera; }
    /// Always CameraType::JaiGigE.
    CameraType cameraType() const override { return CameraType::JaiGigE; }

    /// Enables/disables the auto-backlight control feature.
    void enableBlackLightControl(bool ena) override { m_autoBacklightControl = ena; }

    /// Returns the configured exposure-time range via the output parameters.
    void exposureTimeLimit(double &min, double &max) override {
        min = m_paramsExposureMin;
        max = m_paramsExposureMax;
    }
    /// Sets the allowed exposure-time range.
    void setExposureTimeLimit(double min, double max) override {
        m_paramsExposureMin = min;
        m_paramsExposureMax = max;
    }
    /// Sets the exposure time; silently ignored if `value` is outside [min, max].
    void setExposureTime(double value) override {
        if ((value >= m_paramsExposureMin) && (value <= m_paramsExposureMax)) {
            m_paramsExposureTime = value;
        }
    }
    /// Returns the currently configured exposure time, microseconds.
    double exposureTime() override { return m_paramsExposureTime; }

    /// Sets the auto-exposure mode directly from the enum value.
    void setAutoExposure(vc::device::jai::JaiExposureMode mode) { m_autoExposureMode = mode; }
    /// Sets the auto-exposure mode by parsing its registered enum name.
    void setAutoExposure(const QString &mode) {
        m_autoExposureMode = vc::device::jai::JaiExposureTypeFromString(mode);
    }
    /// Returns the current auto-exposure mode as its registered enum name.
    QString autoExposure() const {
        return vc::device::jai::JaiExposureTypeToString(m_autoExposureMode);
    }

    /// Returns the configured gain range via the output parameters.
    void gainLimit(int &min, int &max) override {
        min = m_paramsGainMin;
        max = m_paramsGainMax;
    }
    /// Sets the allowed gain range.
    void setGainLimit(int min, int max) override {
        m_paramsGainMin = min;
        m_paramsGainMax = max;
    }
    /// Sets the gain; silently ignored if `value` is outside [min, max].
    void setGain(int value) override {
        if ((value >= m_paramsGainMin) && (value <= m_paramsGainMax)) {
            m_paramsGain = value;
        }
    }
    /// Returns the currently configured gain.
    /// @note The CameraCfg contract returns int. The JAI SFNC `Gain` node is a float in dB, so
    ///       `m_paramsGain` is stored as a double and this accessor truncates. Use
    ///       `m_paramsGain` directly where the fractional part matters — the truncation is the
    ///       base-class contract's, not this camera's.
    int gain() override { return static_cast<int>(m_paramsGain); }

    /// Enables/disables acquisition frame-rate limiting and sets its value.
    void setAcquisitionFrameRate(bool enable, double rate) override {
        m_enableAcquisitionFrameRate = enable;
        m_paramsAcquisitionFrameRate = rate;
    }
    /// Returns whether acquisition frame-rate limiting is enabled and its current value.
    void acquisitionFrameRate(bool &enable, double &rate) override {
        enable = m_enableAcquisitionFrameRate;
        rate = m_paramsAcquisitionFrameRate;
    }

    /// Serializes all JAI-specific fields plus the CameraCfg base fields. In the .cpp.
    QJsonObject toJson() const override;
    /// Restores all JAI-specific fields from JSON produced by toJson(). In the .cpp.
    bool fromJson(const QJsonObject &obj) override;

    /// Returns a heap-allocated copy of this config; caller takes ownership.
    IDeviceCfg *clone() override { return new JaiGigeCfg(*this); }

public:
    QString m_modelName{"Unknown"};        ///< Camera model name, from device info on connect.
    QString m_userDefinedName;             ///< User-defined camera name, from device info on connect.
    QString m_serialNumber;                ///< Camera serial number, from device info on connect.
    QString m_ipAddress;                   ///< IP address used to locate the camera on connect.
    QString m_pixelFormat;                 ///< GenICam PixelFormat symbolic, read on connect.

    double m_paramsExposureMin{0.0};       ///< Minimum allowed exposure time, from the camera.
    double m_paramsExposureMax{0.0};       ///< Maximum allowed exposure time, from the camera.
    double m_paramsExposureTime{0.0};      ///< Configured exposure time, microseconds.
    vc::device::jai::JaiExposureMode m_autoExposureMode{
        vc::device::jai::JaiExposureMode::Exposure_Off};  ///< Auto-exposure mode.

    int m_paramsGainMin{0};                ///< Minimum allowed gain, from the camera.
    int m_paramsGainMax{0};                ///< Maximum allowed gain, from the camera.
    double m_paramsGain{0.0};              ///< Configured gain; double because SFNC `Gain` is a float.

    bool m_enableAcquisitionFrameRate{false};  ///< Enables acquisition frame-rate limiting.
    double m_paramsAcquisitionFrameRate{0.0};  ///< Configured acquisition frame rate.

    bool m_autoBacklightControl{false};    ///< Enables automatic backlight output around a grab.
    QString m_autoBacklightLine;           ///< Digital output line driven for backlight control.
    bool m_autoBacklightInvert{false};     ///< Inverts the backlight output line's logic level.
    int m_autoBacklightDelay{0};           ///< Delay (microseconds) after enabling backlight.

    int m_grabTimeoutMs{1500};             ///< Blocking-grab timeout; see kDefaultGrabTimeoutMs.

    bool m_isColor{false};                 ///< True when the camera's PixelFormat is a colour one.

    QList<vc::device::jai::JaiIOLine> m_ioCapabilities;  ///< I/O lines and capabilities, read from the camera.
};

/**
 * @class JaiGigECamera
 * @brief JAI GigE Vision camera device on the Pleora eBUS SDK. Handles connect/disconnect,
 *        exposure/gain/frame-rate through the GenICam node map, digital I/O lines, and a
 *        blocking single-shot grab converted to `cv::Mat`.
 *
 * **How this differs from `BaslerGigECamera`, and why.**
 *
 * | Concern | Basler (Pylon) | JAI (eBUS) |
 * |---|---|---|
 * | Cable pull | Noticed only on a failed grab — no callback exists | `PvDeviceEventSink::OnLinkDisconnected` fires on its own |
 * | Acquisition | `GrabOne()` wraps start/stop | Explicit `AcquisitionStart` / `RetrieveNextBuffer` / `AcquisitionStop` |
 * | Feature names | Legacy `ExposureTimeAbs` / `GainRaw`, hard-coded | Resolved at connect; see `jai_define.h` node_names |
 * | Errors | C++ exceptions (`GenericException`) | `PvResult` return codes; the SDK does not throw |
 *
 * The link callback is the important one: an idle station with a pulled cable is reported here
 * within the eBUS heartbeat, where the Basler device stays "connected" until the next trigger.
 */
class JaiGigECamera : public vc::device::CameraDevice {
    Q_OBJECT

public:
    /// Constructs the camera with the given id/name and registers m_config as its IDeviceCfg.
    explicit JaiGigECamera(QString id, QString name, QObject *parent = nullptr);
    ~JaiGigECamera() override;

    /**
     * @brief Finds the camera by the configured IP, connects, and brings it to the commissioned
     *        settings.
     *
     * Enumerates GigE devices, opens a stream and pipeline, reads the I/O-line capabilities and
     * the feature ranges, then pushes m_config onto the camera.
     *
     * @return true if the camera was found and opened successfully.
     * @post On success the camera holds the configured exposure, gain, frame rate and backlight
     *       routing — it is not left at whatever it powered up with.
     */
    bool deviceConnect() override;
    /// Stops any acquisition, closes the stream and disconnects the device.
    bool deviceDisconnect() override;
    /// Returns true if connectStatus() is ConnectStatus::Connected.
    bool isDeviceConnected() const override {
        return connectStatus() == device::ConnectStatus::Connected;
    }

    /// Always CameraType::JaiGigE.
    CameraType cameraType() const override { return CameraType::JaiGigE; }

    /// Always true: answer Q5 of the phase-8 plan — JAI cameras expose usable I/O lines.
    bool hasIOPort() const override { return true; }

    /**
     * @brief Whether the connected camera delivers colour frames.
     * @return true for a colour PixelFormat; false before the first successful connect.
     * @note Read from the camera's PixelFormat at connect, not declared up front: model and pixel
     *       format are only known after connecting. The pre-connect false is also what an
     *       uncalibrated mono pipeline should assume.
     */
    bool isRgbCamera() const override { return m_config.m_isColor; }

    /// Copies the JAI-specific fields from `cfg` into m_config, under m_mutex.
    void setDeviceConfig(IDeviceCfg *cfg) override;
    /// Replaces m_config wholesale with `cfg`, under m_mutex.
    void setJaiGigeConfig(JaiGigeCfg &cfg);
    /// Returns a copy of the current JAI configuration.
    JaiGigeCfg jaiGigeConfig() const;

    /// Sets the blocking-grab timeout, clamped below kSingleShotTimeoutMs. Under m_mutex.
    void setGrabTimeout(int ms) override;
    /// Sets the exposure time if within the configured limits and emits exposureChanged().
    bool setExposure(double exposure) override;
    /// Sets the gain if within the configured limits and emits gainChanged().
    bool setGain(double gain) override;
    /// Enables/disables auto-backlight control and emits backlightControlChanged().
    void setBacklightControl(bool enable) override;

    /**
     * @brief Forces the backlight on or releases it, suppressing the auto sequence while held.
     * @param[in] on true to take the lamp and drive it on; false to release and drive it off.
     * @return false when the backlight output is unconfigured or the camera is not connected.
     * @post backlightStateChanged() is emitted on every path, including refusals.
     * @see setAutoBacklightState(), which is what the override suppresses.
     */
    bool setBacklightOverride(bool on) override;
    /// Whether the manual override currently owns the backlight.
    bool isBacklightOverridden() const override { return m_backlightOverride; }
    /**
     * @brief Reads the current level of the named digital line via LineSelector/LineStatus.
     * @param[in] name LineSelector symbolic, e.g. "Line2".
     * @return the line level; false if the camera is not connected or the line is unknown.
     */
    bool readIO(QString name) override;

    /**
     * @brief Drives the named digital output line to @p value.
     * @param[in] name  LineSelector symbolic, e.g. "Line2".
     * @param[in] value level to drive.
     * @return false when the camera is not connected; true once the write was attempted.
     */
    bool writeIO(QString name, bool value) override;
    /// Pushes exposure mode/time, gain and frame-rate from m_config to the camera and emits
    /// parametersApplied().
    bool applyParametersChange() override;

    /// Default blocking-grab timeout in milliseconds, overridable per device.
    ///
    /// Must stay **below** `CameraRunner::kSingleShotTimeoutMs`, or the runner's watchdog always
    /// fires before the camera can possibly answer. setGrabTimeout() clamps to it.
    static constexpr int kDefaultGrabTimeoutMs = 1500;

    /// Number of stream buffers in the acquisition pipeline.
    ///
    /// It was 4, on the reasoning that a single-shot device gains nothing from a deep queue.
    /// That reasoning was wrong while the camera free-ran during the grab window: a 5 MP mono
    /// sensor at ~22 fps is ~115 MB/s, over what GigE carries, and four buffers were recycled
    /// while still filling. The acquisition mode is the real fix (see configureAcquisitionMode)
    /// but the SDK samples use 16 and the memory is trivial next to one 5 MP frame.
    static constexpr int kPipelineBufferCount = 16;

    /// Fraction of the GigE link the camera is paced to, via `GevSCPD`. See
    /// configurePacketDelay() for the measurements behind the number.
    ///
    /// Below ~40% every frame arrived whole; above it, incomplete frames appeared and grew until
    /// at 100% (no pacing at all) *nothing* got through. Counter-intuitively this also made grabs
    /// **faster** — 3031 ms unpaced against 239 ms at 40% — because a frame that arrives once
    /// beats a frame retried until the timeout.
    static constexpr double kTargetLinkUtilisation = 0.40;

    /// How long the continuous pump may block in `RetrieveNextBuffer`, in milliseconds.
    ///
    /// This is also the **worst-case stop latency**: the pump only sees a stop between trips
    /// through the event loop, so a longer block makes the Stop button feel dead. Short enough to
    /// feel immediate, long enough that a frame at the paced rate (~110 ms apart) usually arrives
    /// within one wait rather than costing an extra trip.
    static constexpr uint32_t kContinuousRetrieveMs = 250;

    /// Safety margin applied to the computed continuous frame-rate ceiling.
    ///
    /// The ceiling comes from *nominal* link speed and a nominal payload; running exactly at a
    /// computed limit is how a setting that looks safe still drops frames.
    static constexpr double kContinuousRateMargin = 0.90;

    /**
     * @brief Performs a blocking single grab, optionally switching the backlight around it, and
     *        converts the result to a cv::Mat.
     * @return the grab outcome; the same value carried by grabFinished().
     * @post grabFinished() is emitted on EVERY path, success or failure — CameraRunner resolves
     *       its in-flight command from it, so a silent return hangs the runner.
     */
    GrabResult grabSingleShot() override;

    /**
     * @brief Not implemented.
     * @return always false.
     * @note Deliberately left alone while startContinuousShot() was implemented: nothing calls
     *       it, and it has no meaning distinct from that pair. Inventing one would be a second
     *       abstraction before the first has a user. Recorded in
     *       `docs/backlog/later_todo_list.md`.
     */
    bool startAutoContinuousShot() override;

    /// Not implemented: no-op. See startAutoContinuousShot().
    void stopAutoContinousShot() override;

    /**
     * @brief Switches the camera to `Continuous` acquisition and streams frames on
     *        continuousFrameReady() until stopContinuousShot().
     *
     * @return true if streaming started or was already running; false if not connected or the
     *         camera refused to leave single-frame acquisition.
     * @warning Caps `AcquisitionFrameRate` to what the paced link carries, for the duration. Left
     *          free-running this sensor does 22 fps x 5.24 MB, about 115 MB/s, against a paced
     *          budget near 50 MB/s — which reproduces exactly the every-frame-incomplete defect
     *          configurePacketDelay() exists to prevent.
     * @see stopContinuousShot(), isContinuousActive()
     */
    bool startContinuousShot() override;

    /**
     * @brief Stops streaming and restores `SingleFrame` and the operator's frame rate.
     *
     * Idempotent and safe from any state; logs a frame/discard summary for the stream that ran.
     * @post The camera can grabSingleShot() immediately afterwards.
     */
    void stopContinuousShot() override;
    /// Whether the continuous pump is running.
    bool isContinuousActive() const override { return m_continuousActive; }

    /// Not implemented: always returns a default (failed) GrabResult.
    GrabResult softwareTriggerShot() override;

    /// Validates and applies device-level JSON fields.
    bool fromJson(const QJsonObject &obj) override;

    /**
     * @brief One line of GigE transport and stream counters, for diagnosing lost frames.
     *
     * Reports the negotiated packet size and inter-packet delay from the device, and the block,
     * lost-packet and resend counters from the stream. These are the numbers that separate the
     * three causes an incomplete frame can have — a path that drops large packets, a camera
     * out-running the adapter, and a camera that never triggered — none of which
     * `RESENDS_FAILURE` on its own distinguishes.
     *
     * @return a human-readable, log-ready summary; never empty.
     * @note Safe to call while disconnected; returns a short "not connected" string.
     * @see backlightDiagnostics(), cameraFeatureSnapshot()
     */
    QString streamDiagnostics() const;

    /**
     * @brief What the camera actually has configured for the backlight line, read back from it.
     *
     * The counterpart to streamDiagnostics(), and it exists for the same reason: "the light does
     * not switch" has four causes that look identical from outside — the wrong line selected, the
     * line not routed to a user output, the polarity inverted the wrong way, or the value simply
     * never written. This reports all four.
     *
     * @return a log-ready summary; "not configured" when configureBacklightOutput() has not run
     *         or failed.
     * @see configureBacklightOutput()
     */
    QString backlightDiagnostics() const;

    /**
     * @brief The exposure, gain and frame-rate features as the CAMERA currently reports them.
     *
     * Node name, type, range and live value, read from the device and independent of what the
     * config holds — the one line that separates "the panel and the camera agree" from "the panel
     * is showing settings the camera never received", which is otherwise invisible until an image
     * comes out wrong.
     *
     * @return a log-ready summary; "not connected" when there is no device.
     * @note Also what readCameraSetting() logs.
     */
    QString cameraFeatureSnapshot() const;

public slots:
    /// Disconnects the camera if still connected and logs device shutdown.
    void deviceTerminate() override;

    /// Publishes LostConnected after the eBUS SDK reported the link went down.
    /// @note Public because the event sink invokes it across threads by name; not for callers.
    void onLinkLost();

private slots:
    /**
     * @brief Retrieves at most one frame and re-posts itself while continuous acquisition runs.
     *
     * @post While streaming, re-posts itself with `QMetaObject::invokeMethod(..., QueuedConnection)`
     *       and emits continuousFrameReady() for each complete frame.
     * @warning A slot, and invoked by NAME, because the queued self-post is the whole design: the
     *          device is driven by queued signals on the runner's worker thread, so a
     *          `while (streaming)` loop would starve that thread's event loop and the stop command
     *          could never be delivered. One frame per trip keeps stop deliverable.
     */
    void pumpContinuousFrame();

private:
    /// Identity a GigE discovery pass reports for one camera, before it is connected.
    ///
    /// Carried out of discovery rather than read back from the connected device because the SDK's
    /// `PvDevice` has no device-info accessor — the record lives on `PvDeviceInfo`, which only
    /// exists during enumeration. readCameraSetting() prefers the camera's own GenICam nodes and
    /// falls back to this.
    struct DiscoveredIdentity {
        QString connectionId;   ///< eBUS connection ID; empty when no camera matched.
        QString modelName;
        QString serialNumber;
        QString userDefinedName;
        QString ipAddress;
    };

    /**
     * @brief Enumerates GigE devices and returns the identity of the one whose IP matches.
     * @param[in]  ipAddress the address to look for.
     * @param[out] reason    on no match, what was found instead; must not be null.
     * @return the matching identity, or one with an empty `connectionId` when none matched.
     */
    DiscoveredIdentity findCameraByIp(const QString &ipAddress, QString *reason);

    /**
     * @brief Opens the stream for @p connectionId and builds the acquisition pipeline.
     * @param[in]  connectionId eBUS connection ID from findCameraByIp().
     * @param[out] reason       on failure, what went wrong; must not be null.
     * @return false on failure, in which case the device is left fully torn down.
     */
    bool openStreamAndPipeline(const QString &connectionId, QString *reason);

    /// Releases pipeline, stream and device in the order eBUS requires. Safe to call twice.
    void releaseSdkObjects();

    /**
     * @brief Paces the camera's transmission via `GevSCPD` so one frame is not sent as a single
     *        wire-rate burst.
     *
     * Always writes, rather than deferring to a non-zero value it finds, because `GevSCPD` is
     * stored on the camera and survives power cycles — a value left by another application would
     * otherwise silently become this station's behaviour.
     *
     * @param[in] deviceGev the connected device, already past packet-size negotiation.
     * @warning Without this, **every** frame arrives incomplete on this camera: measured 0 of 10
     *          grabs and 118 of 118 blocks failed with `RESENDS_FAILURE` /
     *          `TOO_MANY_CONSECUTIVE_RESENDS`, against 10 of 10 grabs and zero failed blocks once
     *          paced. The symptom names packets, so jumbo frames and cabling are the natural
     *          suspects; both were already fine.
     * @see kTargetLinkUtilisation
     */
    void configurePacketDelay(::PvDeviceGEV *deviceGev);

    /**
     * @brief Writes `AcquisitionMode` and updates m_singleFrameMode to match.
     * @param[in] symbolic the GenICam enum entry, e.g. "SingleFrame" or "Continuous".
     * @return true if the camera accepted the mode.
     * @note Success is not logged — callers say what they were doing and why.
     */
    bool applyAcquisitionMode(const char *symbolic);

    /// Frames per second the paced link can carry for this camera's payload size.
    /// @return the ceiling in fps, or 0.0 when it cannot be computed.
    double pacedFrameRateCeiling() const;
    /// Limits `AcquisitionFrameRate` to pacedFrameRateCeiling() for the duration of continuous
    /// acquisition, remembering the operator's value. No-op when already slower than the ceiling.
    void capFrameRateForContinuous();
    /// Puts `AcquisitionFrameRate` and its enable back the way capFrameRateForContinuous() found
    /// them. No-op when nothing was capped.
    void restoreFrameRateAfterContinuous();

    /// Releases every buffer the pipeline is still holding. Zero timeout, so it costs nothing on
    /// an empty queue.
    void drainPipeline();

    /**
     * @brief Puts the camera into single-frame acquisition, remembering what it was on.
     *
     * @post m_previousAcquisitionMode holds the mode found at connect, for
     *       restoreAcquisitionMode() to put back.
     * @warning This is what stops a grab flooding the link. Left in its default `Continuous` mode,
     *          `AcquisitionStart` makes the camera free-run at full rate for the whole grab
     *          window — 5 MP mono at ~22 fps is ~115 MB/s, more than Gigabit Ethernet carries — so
     *          most blocks arrive with packets missing, the resends fail against an
     *          already-saturated link, and the grab succeeds only on whichever frame happens to
     *          survive. It *worked*, and it cost up to 1.3 s per trigger against a 1.5 s timeout.
     */
    void configureAcquisitionMode();

    /// Puts `AcquisitionMode` back to whatever the camera had before we changed it.
    void restoreAcquisitionMode();

    /**
     * @enum ValueSync
     * @brief Whether a read may replace the configured values with the camera's own.
     */
    enum class ValueSync {
        /// Identity, pixel format and ranges only — the operator's values are left alone.
        ///
        /// Used for the read that happens BEFORE connect pushes the config onto the camera. A
        /// camera at its power-up defaults reports narrower ranges and different modes, and
        /// adopting those would discard the very settings the push is about to restore.
        LimitsOnly,
        /// Also adopt the camera's values where the configured one does not fit its range, and
        /// take the auto-exposure mode and frame-rate-limit switch from the camera.
        ///
        /// Correct only AFTER a push, when the camera holds what was asked of it and its ranges
        /// are the ones grabs will actually run at.
        AdoptFromCamera,
    };

    /// Enumerates LineSelector entries and records each line's Input/Output capability.
    void initializeIOPort();

    /**
     * @brief Reads identity, pixel format and the exposure/gain/frame-rate ranges from the camera
     *        into m_config.
     * @param[in] values whether the camera's own values may replace the configured ones. Defaults
     *                   to AdoptFromCamera, which is right everywhere except the pre-push read in
     *                   deviceConnect().
     * @see ValueSync
     */
    void readCameraSetting(ValueSync values = ValueSync::AdoptFromCamera);

    /**
     * @brief Configures @p line as a user-driven output.
     *
     * Sets `LineMode` (where the model allows it), `LineInverter` from @p invert, and routes
     * `LineSource` to a user output.
     *
     * @param[in] line   LineSelector symbolic, e.g. "Line2".
     * @param[in] invert true to drive the line active-low.
     * @return the user output the line was routed to — `UserOutput0` unless the model starts
     *         elsewhere — or an empty string on failure, which is logged.
     * @warning Polarity goes into `LineInverter`, in the camera, and is not applied to the value
     *          before writing it. A software flip looks equivalent and is not: it leaves the pin
     *          at the wrong level whenever anything else determines the output, the camera's own
     *          power-up default included.
     */
    QString routeLineToUserOutput(const QString &line, bool invert);

    /**
     * @brief Sets one user output's level via UserOutputSelector/UserOutputValue.
     * @param[in] userOutput the user output to drive, e.g. "UserOutput0".
     * @param[in] value      level to write.
     * @return false on failure, which is logged.
     */
    bool setUserOutputState(const QString &userOutput, bool value);

    /**
     * @brief Sets up the backlight line once, from m_config's line and invert flag.
     *
     * Called at connect and again on every applyParametersChange(), because both settings are
     * edited in the same property browser as exposure and gain.
     *
     * @post m_backlightUserOutput names the output to drive, or is empty if setup failed.
     * @see setBacklightState(), backlightDiagnostics()
     */
    void configureBacklightOutput();

    /**
     * @brief Switches the backlight by writing only m_backlightUserOutput's level.
     * @param[in] on level to drive the backlight to.
     * @warning Nothing else is written. The routing is static configuration
     *          (configureBacklightOutput()); redoing it per grab is four extra register writes per
     *          trigger for a wiring decision that cannot change between them.
     */
    void setBacklightState(bool on);

    /**
     * @brief Drives the backlight for the automatic grab/stream sequence, honouring a manual
     *        override.
     * @param[in] on level the auto sequence wants.
     * @return true if the lamp was actually driven; false when auto-backlight is off, no line is
     *         configured, or a manual override holds the lamp. Callers use this for the settle
     *         delay, which is only owed when this grab is what switched the lamp on.
     *
     * Every automatic backlight write goes through here rather than calling setBacklightState()
     * directly. There are five such sites — around a single shot, and around the start, failed
     * start and stop of a stream — and each one used to repeat the same
     * `m_autoBacklightControl && !m_autoBacklightLine.isEmpty()` guard. Routing them through one
     * function is what makes the override total: a new grab path cannot forget to respect it,
     * because it cannot reach the lamp any other way.
     */
    bool setAutoBacklightState(bool on);

    /**
     * @brief Drives an arbitrary line to @p value for writeIO(): routes it, then writes the level.
     * @param[in] line  LineSelector symbolic.
     * @param[in] value level to drive.
     * @note Does NOT apply the backlight's invert flag. It used to, so a station with an inverted
     *       backlight drove every other output upside down as well.
     */
    void setOutputLineState(const QString &line, bool value);

    /**
     * @brief Converts an acquired eBUS buffer to @p mat as BGR8 (colour) or Mono8 (mono).
     * @param[in]  buffer the retrieved eBUS buffer.
     * @param[out] mat    the converted image; left empty for a pixel format the SDK cannot
     *                    convert, which is logged.
     */
    void jai_buffer_to_mat(::PvBuffer *buffer, cv::Mat &mat);

    /**
     * @brief Selects @p line in LineSelector so the per-line nodes address it.
     * @param[in] line LineSelector symbolic.
     * @return the device parameter array, or null when there is no device or the line is not one
     *         of the camera's symbolics.
     */
    PvGenParameterArray *selectLine(const QString &line);

private:
    JaiGigeCfg m_config;                 ///< Current configuration; registered as this device's IDeviceCfg.
    QString m_connectionId;              ///< eBUS connection ID of the connected camera.
    QString m_str_camera_id;             ///< Serial number, cached after connect, for messages.
    DiscoveredIdentity m_discovered;     ///< What enumeration reported; the fallback identity.

    PvDevice *m_device{nullptr};         ///< Owned via PvDevice::Free; null when disconnected.
    PvStream *m_stream{nullptr};         ///< Owned via PvStream::Free; null when disconnected.
    PvPipeline *m_pipeline{nullptr};     ///< Owned with delete; null when disconnected.
    /// Owned. Handles `TLParamsLocked` around AcquisitionStart/Stop, which this device used to
    /// skip entirely — the SDK's own samples set it and some cameras need it to stream cleanly.
    /// Must be destroyed BEFORE the device and stream it wraps.
    ::PvAcquisitionStateManager *m_acquisition{nullptr};
    QString m_previousAcquisitionMode;   ///< AcquisitionMode before we forced SingleFrame; empty if unchanged.
    bool m_singleFrameMode{false};       ///< True when the camera is in SingleFrame RIGHT NOW; continuous shot clears it.

    bool m_continuousActive{false};      ///< True while the continuous pump is running.
    int m_continuousFrames{0};           ///< Frames emitted during the current stream.
    int m_continuousDiscarded{0};        ///< Incomplete frames dropped during the current stream.
    QElapsedTimer m_continuousClock;     ///< Started with the stream; only used for the summary line.

    bool m_frameRateCapped{false};       ///< True when continuous lowered AcquisitionFrameRate.
    double m_frameRateBeforeContinuous{0.0};  ///< Operator's frame rate, restored on stop.
    bool m_frameRateEnableWasOn{false};  ///< AcquisitionFrameRateEnable before continuous forced it on.
    JaiLinkEventSink *m_eventSink{nullptr};  ///< Owned; unregistered before deletion. See the .cpp.

    QList<vc::device::jai::JaiIOLine> m_io_lines;  ///< Lines and capabilities, from initializeIOPort().
    /// User output the backlight line is routed to; empty when the backlight is unconfigured.
    /// Resolved by configureBacklightOutput() and the only thing setBacklightState() needs.
    QString m_backlightUserOutput;
    /// True while the operator's manual override owns the lamp; suppresses the auto sequence.
    bool m_backlightOverride{false};
    /// Level the backlight was last driven to, so a refused request can report the truth.
    bool m_backlightOn{false};
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::JaiGigeCfg)

#endif // CAMERA_JAI_GIGE_H
