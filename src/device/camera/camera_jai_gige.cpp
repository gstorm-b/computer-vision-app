/**
 * @file camera_jai_gige.cpp
 * @brief Implementation of JaiGigeCfg (JSON round-trip) and JaiGigECamera (Pleora eBUS SDK).
 */

#include "camera_jai_gige.h"

#include "core/logger/app_logger.h"
#include "device/camera/jai_runtime.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QMetaObject>
#include <QSet>
#include <QThread>

#include <PvAcquisitionStateManager.h>
#include <PvBuffer.h>
#include <PvBufferConverter.h>
#include <PvDevice.h>
#include <PvDeviceEventSink.h>
#include <PvDeviceGEV.h>
#include <PvDeviceInfo.h>
#include <PvDeviceInfoGEV.h>
#include <PvGenBoolean.h>
#include <PvGenCommand.h>
#include <PvGenEnumEntry.h>
#include <PvGenParameter.h>
#include <PvGenString.h>
#include <PvImage.h>
#include <PvInterface.h>
#include <PvPipeline.h>
#include <PvStream.h>
#include <PvStreamGEV.h>
#include <PvSystem.h>

using namespace vc::device::jai;

namespace vc::device {

namespace {

/// Converts a QString to the SDK's string type. eBUS takes `const PvString &` everywhere, and
/// the temporary must outlive the call — always pass this inline, never store the result of
/// `.toUtf8()` separately.
PvString pv(const QString &text)
{
    return PvString(text.toUtf8().constData());
}

/// Converts an SDK string back to QString.
QString qs(const PvString &text)
{
    return QString::fromUtf8(text.GetAscii());
}

/// Renders a PvResult for a log line: the code name plus the SDK's own description.
///
/// eBUS does not throw. Every call returns a PvResult and an ignored one is a silent failure,
/// which on a camera means "the setting did not take" with nothing to show for it.
QString describe(const PvResult &result)
{
    return QStringLiteral("%1 - %2").arg(qs(result.GetCodeString()), qs(result.GetDescription()));
}

/// GenICam symbolics tried, in order, when driving a line as a user-controlled output.
///
/// **UserOutput0 is the commissioned default** and therefore first. The rest are a fallback, not
/// a search: the number of user outputs and their numbering base are model-dependent, so a
/// camera that starts at UserOutput1 still works instead of failing with nothing pointing at why.
/// Not a single hard-coded "UserOutput1", which is what the Basler device assumes.
constexpr const char *const kUserOutputCandidates[] = {
    "UserOutput0", "UserOutput1", "UserOutput2", "UserOutput3", nullptr
};

/// Returns true when `enumeration` has an entry with the given symbolic and it is available.
bool hasEnumEntry(PvGenEnum *enumeration, const char *symbolic)
{
    if (!enumeration) {
        return false;
    }
    const PvGenEnumEntry *entry = nullptr;
    if (!enumeration->GetEntryByName(PvString(symbolic), &entry).IsOK() || entry == nullptr) {
        return false;
    }
    bool available = false;
    return entry->IsAvailable(available).IsOK() && available;
}

} // namespace

/**
 * @class JaiLinkEventSink
 * @brief Turns the eBUS link-down callback into a queued call on the camera's own thread.
 *
 * `OnLinkDisconnected` is invoked from an eBUS-internal heartbeat thread, not from the
 * device's worker thread. Touching the device directly from it would race the grab path and
 * publish a connection status from a foreign thread — the same class of defect the Modbus
 * devices already paid for. `QMetaObject::invokeMethod` with `Qt::QueuedConnection` is the
 * whole point of this class.
 */
class JaiLinkEventSink : public PvDeviceEventSink {
public:
    explicit JaiLinkEventSink(JaiGigECamera *camera) : m_camera(camera) {}

    void OnLinkDisconnected(PvDevice *device) override
    {
        PVUNREFPARAM(device);
        if (!m_camera) {
            return;
        }
        QMetaObject::invokeMethod(m_camera, "onLinkLost", Qt::QueuedConnection);
    }

private:
    JaiGigECamera *m_camera{nullptr};   ///< Not owned; outlives this sink by construction.
};

// ── JaiGigeCfg ───────────────────────────────────────────────────────────────────────────────

QJsonObject JaiGigeCfg::toJson() const
{
    QJsonObject obj = CameraCfg::toJson();
    obj["ModelName"]                  = m_modelName;
    obj["UserDefinedName"]            = m_userDefinedName;
    obj["SerialNumber"]               = m_serialNumber;
    obj["IpAddress"]                  = m_ipAddress;
    obj["PixelFormat"]                = m_pixelFormat;
    obj["IsColor"]                    = m_isColor;

    obj["AutoBacklightControl"]       = m_autoBacklightControl;
    obj["AutoBacklightLine"]          = m_autoBacklightLine;
    obj["AutoBacklightLineInvert"]    = m_autoBacklightInvert;
    obj["AutoBackLightDelay"]         = m_autoBacklightDelay;

    obj["AutoExposureMode"]           = JaiExposureTypeToString(m_autoExposureMode);
    obj["ExposureLimitMax"]           = m_paramsExposureMax;
    obj["ExposureLimitMin"]           = m_paramsExposureMin;
    obj["Exposure"]                   = m_paramsExposureTime;

    obj["GainLimitMax"]               = m_paramsGainMax;
    obj["GainLimitMin"]               = m_paramsGainMin;
    obj["Gain"]                       = m_paramsGain;

    obj["EnableAcquisitionFrameRate"] = m_enableAcquisitionFrameRate;
    obj["AcquisitionFrameRate"]       = m_paramsAcquisitionFrameRate;
    obj["GrabTimeoutMs"]              = m_grabTimeoutMs;

    QJsonArray lines;
    for (const JaiIOLine &line : m_ioCapabilities) {
        lines.append(line.toJson());
    }
    obj["ioCapabilities"] = lines;

    return obj;
}

bool JaiGigeCfg::fromJson(const QJsonObject &obj)
{
    // Camera type is checked first: a config of the wrong family must not half-load.
    if (!CameraCfg::fromJson(obj)) {
        return false;
    }

    m_modelName                  = obj["ModelName"].toString();
    m_userDefinedName            = obj["UserDefinedName"].toString();
    m_serialNumber               = obj["SerialNumber"].toString();
    m_ipAddress                  = obj["IpAddress"].toString();
    m_pixelFormat                = obj["PixelFormat"].toString();
    m_isColor                    = obj["IsColor"].toBool(false);

    m_autoBacklightControl       = obj["AutoBacklightControl"].toBool(false);
    m_autoBacklightLine          = obj["AutoBacklightLine"].toString();
    m_autoBacklightInvert        = obj["AutoBacklightLineInvert"].toBool(false);
    m_autoBacklightDelay         = obj["AutoBackLightDelay"].toInt(0);

    m_autoExposureMode           = JaiExposureTypeFromString(obj["AutoExposureMode"].toString());
    m_paramsExposureMax          = obj["ExposureLimitMax"].toDouble(0.0);
    m_paramsExposureMin          = obj["ExposureLimitMin"].toDouble(0.0);
    m_paramsExposureTime         = obj["Exposure"].toDouble(0.0);

    m_paramsGainMax              = obj["GainLimitMax"].toInt(0);
    m_paramsGainMin              = obj["GainLimitMin"].toInt(0);
    m_paramsGain                 = obj["Gain"].toDouble(0.0);

    m_enableAcquisitionFrameRate = obj["EnableAcquisitionFrameRate"].toBool(false);
    m_paramsAcquisitionFrameRate = obj["AcquisitionFrameRate"].toDouble(0.0);
    // Defaulted rather than zeroed: a project saved before this key existed would otherwise load
    // a zero timeout and every grab would fail instantly.
    m_grabTimeoutMs              = obj["GrabTimeoutMs"].toInt(JaiGigECamera::kDefaultGrabTimeoutMs);

    m_ioCapabilities.clear();
    const QJsonArray lines = obj["ioCapabilities"].toArray();
    for (const QJsonValue &value : lines) {
        const QJsonObject lineObj = value.toObject();
        if (lineObj.isEmpty()) {
            continue;
        }
        JaiIOLine line;
        line.fromJson(lineObj);
        if (!line.name.isEmpty()) {
            m_ioCapabilities.append(line);
        }
    }

    return true;
}

// ── Construction ─────────────────────────────────────────────────────────────────────────────

JaiGigECamera::JaiGigECamera(QString id, QString name, QObject *parent)
    : CameraDevice(id, name, parent)
{
    // IDevice keeps a NON-owning config pointer and writes through it without calling
    // setDeviceConfig(), so the concrete device must publish its own member here or a loaded
    // project would write into nothing. Signals blocked so construction emits no change.
    this->blockSignals(true);
    IDevice::setDeviceConfig(&m_config);
    this->blockSignals(false);
}

JaiGigECamera::~JaiGigECamera()
{
    releaseSdkObjects();
}

bool JaiGigECamera::fromJson(const QJsonObject &obj)
{
    if (!CameraDevice::fromJson(obj)) {
        return false;
    }
    return IDevice::fromJson(obj);
}

void JaiGigECamera::deviceTerminate()
{
    if (isDeviceConnected()) {
        LOG_DEV_DEBUG << "JAI GigE camera disconnect";
        deviceDisconnect();
    }
    LOG_DEV_DEBUG << "JAI GigE camera device terminate, device name" << name() << ", id" << id();
}

// ── Connection lifecycle ─────────────────────────────────────────────────────────────────────

JaiGigECamera::DiscoveredIdentity JaiGigECamera::findCameraByIp(const QString &ipAddress,
                                                                QString *reason)
{
    DiscoveredIdentity match;

    PvSystem system;
    const PvResult found = system.Find();
    if (!found.IsOK()) {
        *reason = tr("Camera discovery failed: %1").arg(describe(found));
        return match;
    }

    QStringList seen;
    // eBUS enumerates per network adapter, so a camera reachable through more than one adapter
    // is reported once per adapter — the same camera, not several. Keyed on MAC, the one
    // identifier that belongs to the camera rather than to the path taken to reach it, so the
    // warning below and the "cameras found" list each name it once. Matching itself is left
    // alone: the first entry with the right IP still wins, whichever adapter surfaced it.
    QSet<QString> seenMac;

    const uint32_t interfaceCount = system.GetInterfaceCount();
    for (uint32_t i = 0; i < interfaceCount; ++i) {
        const PvInterface *iface = system.GetInterface(i);
        if (iface == nullptr) {
            continue;
        }
        const uint32_t deviceCount = iface->GetDeviceCount();
        for (uint32_t d = 0; d < deviceCount; ++d) {
            const PvDeviceInfo *info = iface->GetDeviceInfo(d);
            const auto *gev = dynamic_cast<const PvDeviceInfoGEV *>(info);
            if (gev == nullptr) {
                continue;   // USB3 Vision or another transport; this device is GigE only
            }

            const QString ip  = qs(gev->GetIPAddress());
            const QString mac = qs(gev->GetMACAddress());
            const bool firstSighting = mac.isEmpty() || !seenMac.contains(mac);
            if (!mac.isEmpty()) {
                seenMac.insert(mac);
            }

            if (firstSighting) {
                seen << QStringLiteral("%1 (%2 %3)")
                            .arg(ip, qs(gev->GetModelName()), qs(gev->GetSerialNumber()));

                if (!gev->IsConfigurationValid()) {
                    // Worth its own line: a camera on the wrong subnet is *visible* to discovery
                    // but cannot be opened, and "not found" would send the reader looking at
                    // cabling.
                    LOG_USER_WARN << tr("JAI camera at %1 has an invalid IP configuration "
                                        "(wrong subnet or no address assigned).").arg(ip);
                }
            }

            if (ip == ipAddress && match.connectionId.isEmpty()) {
                match.connectionId    = qs(gev->GetConnectionID());
                match.modelName       = qs(gev->GetModelName());
                match.serialNumber    = qs(gev->GetSerialNumber());
                match.userDefinedName = qs(gev->GetUserDefinedName());
                match.ipAddress       = ip;
            }
        }
    }

    if (!match.connectionId.isEmpty()) {
        return match;
    }

    if (seen.isEmpty()) {
        // The single most common cause on a fresh station, and invisible from the panel.
        *reason = tr("No GigE Vision camera found on any interface. Check cabling and IP "
                     "configuration — and that this application is allowed through Windows "
                     "Defender Firewall, since discovery replies arrive as unsolicited "
                     "inbound UDP.");
    } else {
        *reason = tr("No JAI camera at %1. Cameras found: %2")
                      .arg(ipAddress, seen.join(QStringLiteral(", ")));
    }
    return match;
}

bool JaiGigECamera::openStreamAndPipeline(const QString &connectionId, QString *reason)
{
    PvResult result;
    m_stream = PvStream::CreateAndOpen(pv(connectionId), &result);
    if (m_stream == nullptr || !result.IsOK()) {
        *reason = tr("Could not open a stream from the camera: %1").arg(describe(result));
        return false;
    }

    // GigE-specific setup. Without NegotiatePacketSize() the stream falls back to a packet size
    // the path may not carry, and without SetStreamDestination() the camera has nowhere to send
    // the images: acquisition then "works" and every RetrieveNextBuffer times out.
    auto *deviceGev = dynamic_cast<PvDeviceGEV *>(m_device);
    auto *streamGev = dynamic_cast<PvStreamGEV *>(m_stream);
    if (deviceGev != nullptr && streamGev != nullptr) {
        const PvResult negotiated = deviceGev->NegotiatePacketSize();
        if (!negotiated.IsOK()) {
            // Not fatal: the SDK falls back to a safe size. Worth saying, because it caps
            // throughput and usually means a NIC without jumbo frames enabled.
            LOG_USER_WARN << tr("JAI camera packet-size negotiation failed (%1); "
                                "streaming continues at the default packet size.")
                                 .arg(describe(negotiated));
        }
        const PvResult destination = deviceGev->SetStreamDestination(
            streamGev->GetLocalIPAddress(), streamGev->GetLocalPort());
        if (!destination.IsOK()) {
            *reason = tr("Could not set the camera's stream destination: %1")
                          .arg(describe(destination));
            return false;
        }

        configurePacketDelay(deviceGev);
    }

    m_pipeline = new PvPipeline(m_stream);
    m_pipeline->SetBufferCount(kPipelineBufferCount);
    m_pipeline->SetBufferSize(m_device->GetPayloadSize());
    // Re-allocate rather than reject when a block does not fit. The payload size is read once,
    // here; turning on chunk data or changing the ROI afterwards grows the block, and without
    // this every grab from then on would come back incomplete for a reason no log would name.
    m_pipeline->SetHandleBufferTooSmall(true);

    const PvResult started = m_pipeline->Start();
    if (!started.IsOK()) {
        *reason = tr("Could not start the acquisition pipeline: %1").arg(describe(started));
        return false;
    }

    // NO StreamEnable() here, deliberately. eBUS offers two mutually exclusive designs for
    // driving acquisition and they cannot be mixed:
    //
    //   manual        — StreamEnable() at connect, then the AcquisitionStart/AcquisitionStop
    //                   GenICam commands per grab, StreamDisable() at teardown (PvPipelineSample).
    //   state manager — no StreamEnable() at all; PvAcquisitionStateManager::Start()/Stop() take
    //                   TLParamsLocked per grab and count frames to release it (eBUSPlayerSample).
    //
    // We use the state manager. For a GigE device StreamEnable() does exactly one thing — set
    // TLParamsLocked — and that is precisely what the manager must be free to do itself:
    // PvAcquisitionStateManager.h documents Start() as returning STATE_ERROR "if TLParamsLocked
    // is already set". Calling both left the lock held from connect onward, so EVERY grab failed
    // with STATE_ERROR and, because TLParamsLocked freezes the streaming-related nodes, the
    // camera also refused to leave Continuous acquisition ("AcquisitionMode: Node is not
    // writable") — which silently disabled the SingleFrame fix this stream setup exists to serve.
    return true;
}

void JaiGigECamera::configurePacketDelay(::PvDeviceGEV *deviceGev)
{
    // THE fix for incomplete frames, and the symptom points the wrong way.
    //
    // `RESENDS_FAILURE` reads like a bad cable or a missing jumbo-frame setting. It was neither:
    // jumbo frames were already on (8976-byte packets) and the cable was fine. In SingleFrame the
    // camera pushes a whole 5 MP frame onto the wire as one burst at line rate, the adapter
    // cannot absorb it, and the resend requests then fail against the same saturated link.
    //
    // GevSCPD is the camera-side gap between packets, in device timestamp ticks. Measured on this
    // bench, 10 grabs per row, 4000-byte packets on a 62.5 MHz tick:
    //
    //   | GevSCPD | link use | grabs  | incomplete blocks | mean   |
    //   |---------|----------|--------|-------------------|--------|
    //   | 0       | 100%     | 0/10   | 118 of 118        | 3031ms |
    //   | 800     | 71%      | 10/10  | 15 of 25          | 609ms  |
    //   | 1495    | 57%      | 10/10  | 13 of 23          | 561ms  |
    //   | 3000    | 40%      | 10/10  | **0 of 10**       | 239ms  |
    //   | 6000    | 25%      | 10/10  | 0 of 10           | 245ms  |
    //   | 20000   | 9%       | 10/10  | 0 of 10           | 517ms  |
    //
    // Hence kTargetLinkUtilisation. Note the shape: pacing *reduces* latency until it does not,
    // because a frame that arrives whole beats a frame retried three times. Packet size, held at
    // 40% utilisation, made no difference worth having (8976 → 1 error, 8192 → 0, 4000 → 7, all
    // within run-to-run noise), so the negotiated size is left alone.
    PvGenParameterArray *params = deviceGev->GetParameters();
    if (params == nullptr) {
        return;
    }

    int64_t packetSize = 0;
    int64_t tickFrequency = 0;
    if (!params->GetIntegerValue(PvString("GevSCPSPacketSize"), packetSize).IsOK()
        || !params->GetIntegerValue(PvString("GevTimestampTickFrequency"), tickFrequency).IsOK()
        || packetSize <= 0 || tickFrequency <= 0) {
        LOG_DEV_INFO << "JAI camera did not report packet size and tick frequency; "
                        "leaving the inter-packet delay alone.";
        return;
    }

    int64_t linkSpeedMbps = 0;   // GevLinkSpeed is optional; assume Gigabit when absent.
    if (!params->GetIntegerValue(PvString("GevLinkSpeed"), linkSpeedMbps).IsOK()
        || linkSpeedMbps <= 0) {
        linkSpeedMbps = 1000;
    }

    // Time one packet occupies the wire, then the extra gap that brings the burst down to
    // kTargetLinkUtilisation of line rate.
    const double packetSeconds =
        (static_cast<double>(packetSize) * 8.0) / (static_cast<double>(linkSpeedMbps) * 1.0e6);
    const double gapSeconds = packetSeconds * (1.0 / kTargetLinkUtilisation - 1.0);
    const auto ticks = static_cast<int64_t>(gapSeconds * static_cast<double>(tickFrequency));
    if (ticks <= 0) {
        return;
    }

    // Written unconditionally, even when the camera already reports a non-zero delay. GevSCPD
    // is stored ON THE CAMERA and survives power cycles, so a value left behind by another
    // application — or by an earlier session of this one — silently becomes this station's
    // behaviour. Reading it and deferring to it makes the grab path depend on history no log
    // records. This cost real debugging time: a stale delay from an experiment made a later
    // measurement look like packet size mattered when it did not.
    const PvResult applied = params->SetIntegerValue(PvString("GevSCPD"), ticks);
    if (!applied.IsOK()) {
        LOG_USER_WARN << tr("JAI camera refused an inter-packet delay of %1 (%2). Frames will "
                            "likely arrive incomplete: this camera can saturate a Gigabit link "
                            "with a single frame.")
                             .arg(ticks)
                             .arg(describe(applied));
        return;
    }

    LOG_USER_INFO << tr("JAI camera inter-packet delay set to %1 ticks (%2 us at %3 Hz), pacing "
                        "%4-byte packets to about %5% of the %6 Mbit/s link so a frame is not "
                        "sent as one burst the adapter cannot absorb.")
                         .arg(ticks)
                         .arg(gapSeconds * 1.0e6, 0, 'f', 1)
                         .arg(tickFrequency)
                         .arg(packetSize)
                         .arg(static_cast<int>(kTargetLinkUtilisation * 100.0))
                         .arg(linkSpeedMbps);
}

void JaiGigECamera::configureAcquisitionMode()
{
    m_singleFrameMode = false;
    m_previousAcquisitionMode.clear();
    if (m_device == nullptr) {
        return;
    }

    PvGenParameterArray *params = m_device->GetParameters();
    PvGenEnum *mode = params ? params->GetEnum(PvString("AcquisitionMode")) : nullptr;
    if (mode == nullptr || !mode->IsAvailable()) {
        LOG_USER_WARN << tr("JAI camera exposes no AcquisitionMode; it will free-run during each "
                            "grab. If images arrive incomplete, enable the acquisition frame-rate "
                            "limit and set it low.");
        return;
    }

    PvString current;
    if (mode->GetValue(current).IsOK()) {
        m_previousAcquisitionMode = qs(current);
    }

    if (!mode->IsWritable()) {
        // Distinct from a refused write, and worth its own message: AcquisitionMode is a
        // streaming-related node, so it is read-only for as long as TLParamsLocked is held. If
        // this fires, something took that lock before us — see openStreamAndPipeline().
        LOG_USER_WARN << tr("JAI camera reports AcquisitionMode as read-only (currently %1), which "
                            "means the transport parameters are already locked; it will free-run "
                            "during each grab.")
                             .arg(m_previousAcquisitionMode.isEmpty()
                                      ? tr("unknown")
                                      : m_previousAcquisitionMode);
        return;
    }

    if (!applyAcquisitionMode("SingleFrame")) {
        return;
    }

    LOG_USER_INFO << tr("JAI camera acquisition mode set to SingleFrame (was %1): one frame per "
                        "trigger, so a grab no longer competes with a free-running stream.")
                         .arg(m_previousAcquisitionMode.isEmpty()
                                  ? tr("unknown")
                                  : m_previousAcquisitionMode);
}

bool JaiGigECamera::applyAcquisitionMode(const char *symbolic)
{
    if (m_device == nullptr) {
        return false;
    }
    PvGenParameterArray *params = m_device->GetParameters();
    PvGenEnum *mode = params ? params->GetEnum(PvString("AcquisitionMode")) : nullptr;
    if (mode == nullptr || !mode->IsAvailable() || !mode->IsWritable()) {
        return false;
    }
    if (!hasEnumEntry(mode, symbolic)) {
        LOG_USER_WARN << tr("JAI camera does not offer %1 acquisition.")
                             .arg(QString::fromLatin1(symbolic));
        return false;
    }

    const PvResult set = mode->SetValue(PvString(symbolic));
    if (!set.IsOK()) {
        LOG_USER_WARN << tr("JAI camera refused %1 acquisition (%2).")
                             .arg(QString::fromLatin1(symbolic), describe(set));
        return false;
    }

    // Tracks what the camera is in NOW, not what it was put in at connect. grabSingleShot()'s
    // re-arm path keys on it (one frame per trigger means a lost frame must be re-requested, and
    // in Continuous it must not be), so it has to follow every mode change including the
    // temporary one continuous shot makes. `m_previousAcquisitionMode` is the separate,
    // connect-time record used to put the camera back the way it was found.
    m_singleFrameMode = (qstrcmp(symbolic, "SingleFrame") == 0);
    return true;
}

void JaiGigECamera::restoreAcquisitionMode()
{
    // Keyed on m_previousAcquisitionMode, NOT on m_singleFrameMode. The latter says what the
    // camera is in now, and continuous shot legitimately clears it — gating on it meant a
    // disconnect during live view left the camera in Continuous, which is the one state that
    // makes the next session's first grab flood the link.
    if (m_device == nullptr || m_previousAcquisitionMode.isEmpty()) {
        return;
    }
    PvGenParameterArray *params = m_device->GetParameters();
    if (PvGenEnum *mode = params ? params->GetEnum(PvString("AcquisitionMode")) : nullptr) {
        // Best effort: we changed a setting the operator may inspect with another tool, so we
        // put it back. A failure here is not worth failing a disconnect over.
        mode->SetValue(pv(m_previousAcquisitionMode));
    }
    m_singleFrameMode = false;
    m_previousAcquisitionMode.clear();
}

void JaiGigECamera::releaseSdkObjects()
{
    // Order matters and is not interchangeable: the acquisition manager drives the device's
    // AcquisitionStop and TLParamsLocked, the pipeline holds buffers owned by the stream, and
    // the stream's channel belongs to the device. Freeing the device first leaves the manager
    // and the pipeline working through a dead handle.
    // BEFORE the acquisition manager is destroyed: stopContinuousShot() drives it, and a stream
    // left running past teardown would keep a camera acquiring with nobody reading it.
    stopContinuousShot();

    // Release the manual backlight override here, where both a clean disconnect and a lost link
    // pass through. A camera reconnected while the flag was still set would suppress its own
    // auto-backlight sequence for every grab, with nothing on screen explaining why — and the
    // lamp itself is off after a power cycle, so the flag would not even be true.
    if (m_backlightOverride) {
        m_backlightOverride = false;
        LOG_DEV_INFO << "JAI camera backlight override released by teardown.";
    }
    m_backlightOn = false;
    emit backlightStateChanged(false);

    if (m_acquisition != nullptr) {
        if (m_acquisition->GetState() == PvAcquisitionStateLocked) {
            m_acquisition->Stop();
        }
        delete m_acquisition;
        m_acquisition = nullptr;
    }

    restoreAcquisitionMode();

    if (m_pipeline != nullptr) {
        m_pipeline->Stop();
        delete m_pipeline;
        m_pipeline = nullptr;
    }

    if (m_device != nullptr) {
        // A backstop, not the counterpart of a StreamEnable() — we never call one (see
        // openStreamAndPipeline()). For a GigE device this only clears TLParamsLocked, so it
        // costs nothing when the manager already released it and rescues the case where it
        // could not.
        m_device->StreamDisable();
        if (m_eventSink != nullptr) {
            // Unregister BEFORE the device goes away: the sink is invoked from an eBUS thread,
            // and deleting it while still registered is a callback into freed memory.
            m_device->UnregisterEventSink(m_eventSink);
            delete m_eventSink;
            m_eventSink = nullptr;
        }
        m_device->Disconnect();
        PvDevice::Free(m_device);
        m_device = nullptr;
    }

    if (m_stream != nullptr) {
        m_stream->Close();
        PvStream::Free(m_stream);
        m_stream = nullptr;
    }
}

bool JaiGigECamera::deviceConnect()
{
    if (isDeviceConnected()) {
        setConnectionStatus(ConnectStatus::Connected);
        return true;
    }

    // A previous failed attempt may have left objects behind; start from nothing.
    releaseSdkObjects();

    const QString ipAddress = m_config.m_ipAddress;
    if (ipAddress.isEmpty()) {
        m_last_msg = tr("JAI camera has no IP address configured.");
        LOG_USER_ERR << m_last_msg;
        setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
        return false;
    }

    // BEFORE any call that builds a GenICam node map. PvDevice/PvGenICam delay-load GenApi
    // from a folder the eBUS installer leaves off PATH; without this the process is killed by
    // an SEH delay-load exception the moment we connect, and the log ends mid-sentence. See
    // jai_runtime.h. Discovery above is unaffected, which is what made this look like a bug in
    // connect rather than a missing runtime.
    QString runtimeDetail;
    if (!jai::ensureGenICamRuntime(&runtimeDetail)) {
        m_last_msg = runtimeDetail;
        setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
        return false;
    }

    setConnectionStatus(ConnectStatus::Connecting);

    QString reason;
    const DiscoveredIdentity discovered = findCameraByIp(ipAddress, &reason);
    if (discovered.connectionId.isEmpty()) {
        m_last_msg = reason;
        LOG_USER_ERR << m_last_msg;
        setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
        return false;
    }
    m_connectionId = discovered.connectionId;
    m_discovered = discovered;

    PvResult result;
    m_device = PvDevice::CreateAndConnect(pv(m_connectionId), &result);
    if (m_device == nullptr || !result.IsOK()) {
        m_last_msg = tr("Could not connect to the JAI camera at %1: %2")
                         .arg(ipAddress, describe(result));
        LOG_USER_ERR << m_last_msg;
        releaseSdkObjects();
        setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
        return false;
    }

    // Registered before the stream is opened, so a cable pulled during setup is still reported.
    m_eventSink = new JaiLinkEventSink(this);
    const PvResult sinkResult = m_device->RegisterEventSink(m_eventSink);
    if (!sinkResult.IsOK()) {
        // Not fatal, but it costs the one advantage this camera has over the Basler one: without
        // the sink a pulled cable is only noticed on the next failed grab. Say so out loud.
        LOG_USER_WARN << tr("JAI camera link monitoring could not be registered (%1); "
                            "a disconnection will only be detected on the next grab.")
                             .arg(describe(sinkResult));
        delete m_eventSink;
        m_eventSink = nullptr;
    }

    if (!openStreamAndPipeline(m_connectionId, &reason)) {
        m_last_msg = reason;
        LOG_USER_ERR << m_last_msg;
        releaseSdkObjects();
        setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
        return false;
    }

    initializeIOPort();
    m_config.m_ioCapabilities = m_io_lines;

    // Before readCameraSetting(): the exposure range a GenICam camera reports depends on the
    // acquisition mode and frame rate, so the limits must be read in the mode we will grab in.
    configureAcquisitionMode();

    // Read FIRST, for identity, pixel format and the ranges — but LimitsOnly, so the commissioned
    // values survive to be pushed. See the note in readCameraSetting() on why adopting here
    // would destroy exactly the settings this sequence exists to restore.
    readCameraSetting(ValueSync::LimitsOnly);

    // Then PUSH. Connect used to read and never write, so the camera kept whatever settings it
    // powered up with and the commissioned ones existed only in the UI. Invisible for as long as
    // the camera retained its settings across a reconnect — and the moment it was power-cycled,
    // it came back at defaults and grabbed with them: exposure, gain, frame rate and the
    // backlight routing all silently different from what the panel showed, with nothing logged.
    //
    // applyParametersChange() is the same path the Apply button uses, deliberately: a second
    // write path here would be a second thing to keep in step with the config. It re-reads the
    // ranges afterwards, which is why readCameraSetting() is not repeated below.
    applyParametersChange();

    m_acquisition = new PvAcquisitionStateManager(m_device, m_stream);

    m_str_camera_id = m_config.m_serialNumber;
    LOG_USER_INFO << tr("JAI camera connected: %1 %2 %3 at %4, pixel format %5")
                         .arg(m_config.m_modelName,
                              m_config.m_serialNumber,
                              m_config.m_userDefinedName,
                              m_config.m_ipAddress,
                              m_config.m_pixelFormat);

    setConnectionStatus(ConnectStatus::Connected);
    return true;
}

bool JaiGigECamera::deviceDisconnect()
{
    if (m_device == nullptr) {
        LOG_USER_INFO << tr("JAI camera disconnect ignored: not connected.");
        setConnectionStatus(ConnectStatus::NoConnection);
        return false;
    }

    releaseSdkObjects();
    m_last_msg = tr("JAI camera %1 closed.").arg(m_str_camera_id);
    LOG_USER_INFO << m_last_msg;

    setConnectionStatus(ConnectStatus::Disconnected);
    return true;
}

void JaiGigECamera::onLinkLost()
{
    if (!isDeviceConnected()) {
        return;   // already torn down; the callback and a manual disconnect raced
    }

    LOG_USER_ERR << tr("JAI camera %1 link lost.").arg(m_str_camera_id);
    releaseSdkObjects();

    // LostConnected, not Disconnected: only the former starts the runtime's recovery policy,
    // and a pulled cable is exactly what that policy exists for.
    setConnectionStatus(ConnectStatus::LostConnected);
}

// ── Configuration ────────────────────────────────────────────────────────────────────────────

void JaiGigECamera::setDeviceConfig(IDeviceCfg *cfg)
{
    if (!cfg) {
        return;
    }

    auto *jaiCfg = dynamic_cast<JaiGigeCfg *>(cfg);
    if (!jaiCfg) {
        if (cfg != &m_config) {
            LOG_DEV_ERR << "JaiGigECamera: config is not a JaiGigeCfg." << "deviceId=" << id();
        }
        return;
    }

    QMutexLocker locker(&m_mutex);

    // Field by field, deliberately: the identity and capability fields below are read FROM the
    // camera at connect, and copying the whole object would let a stale UI copy overwrite what
    // the hardware just reported.
    m_config.m_ipAddress                  = jaiCfg->m_ipAddress;

    m_config.m_autoExposureMode           = jaiCfg->m_autoExposureMode;
    m_config.m_paramsExposureTime         = jaiCfg->m_paramsExposureTime;
    m_config.m_paramsGain                 = jaiCfg->m_paramsGain;
    m_config.m_enableAcquisitionFrameRate = jaiCfg->m_enableAcquisitionFrameRate;
    m_config.m_paramsAcquisitionFrameRate = jaiCfg->m_paramsAcquisitionFrameRate;

    m_config.m_autoBacklightControl       = jaiCfg->m_autoBacklightControl;
    m_config.m_autoBacklightLine          = jaiCfg->m_autoBacklightLine;
    m_config.m_autoBacklightInvert        = jaiCfg->m_autoBacklightInvert;
    m_config.m_autoBacklightDelay         = jaiCfg->m_autoBacklightDelay;

    m_config.m_grabTimeoutMs              = jaiCfg->m_grabTimeoutMs;

    m_config.setCalibrator(jaiCfg->calibrator());
    m_config.setCalibBoardPreset(jaiCfg->calibBoardPreset());
    m_config.setCalibThreshold(jaiCfg->calibThreshold());

    // While disconnected there is nothing authoritative to protect, so identity travels too —     // that is what makes a loaded project show its camera before the first connect.
    if (!isDeviceConnected()) {
        m_config.m_modelName       = jaiCfg->m_modelName;
        m_config.m_serialNumber    = jaiCfg->m_serialNumber;
        m_config.m_userDefinedName = jaiCfg->m_userDefinedName;
        m_config.m_pixelFormat     = jaiCfg->m_pixelFormat;
        m_config.m_isColor         = jaiCfg->m_isColor;
        m_config.m_paramsExposureMin = jaiCfg->m_paramsExposureMin;
        m_config.m_paramsExposureMax = jaiCfg->m_paramsExposureMax;
        m_config.m_paramsGainMin     = jaiCfg->m_paramsGainMin;
        m_config.m_paramsGainMax     = jaiCfg->m_paramsGainMax;
        m_config.m_ioCapabilities    = jaiCfg->m_ioCapabilities;
    }

    IDevice::setDeviceConfig(&m_config);
}

void JaiGigECamera::setJaiGigeConfig(JaiGigeCfg &cfg)
{
    QMutexLocker locker(&m_mutex);
    m_config = cfg;
    IDevice::setDeviceConfig(&m_config);
}

JaiGigeCfg JaiGigECamera::jaiGigeConfig() const
{
    return m_config;
}

void JaiGigECamera::setGrabTimeout(int ms)
{
    QMutexLocker locker(&m_mutex);
    // Clamped below the runner's watchdog rather than trusted: a grab timeout above
    // kSingleShotTimeoutMs means the runner always gives up first, reports TimedOut, and then
    // has no in-flight command left to resolve when the real grabFinished arrives. That exact
    // mismatch is recorded on CameraRunner::kSingleShotTimeoutMs.
    constexpr int kCeiling = 4000;
    static_assert(kCeiling < 8000, "grab timeout must stay under CameraRunner::kSingleShotTimeoutMs");
    m_config.m_grabTimeoutMs = qBound(100, ms, kCeiling);
}

bool JaiGigECamera::setExposure(double exposure)
{
    QMutexLocker locker(&m_mutex);
    if ((exposure < m_config.m_paramsExposureMin) || (exposure > m_config.m_paramsExposureMax)) {
        return false;
    }
    m_config.m_paramsExposureTime = exposure;
    emit exposureChanged(exposure);
    return true;
}

bool JaiGigECamera::setGain(double gain)
{
    QMutexLocker locker(&m_mutex);
    if ((gain < m_config.m_paramsGainMin) || (gain > m_config.m_paramsGainMax)) {
        return false;
    }
    m_config.m_paramsGain = gain;
    emit gainChanged(gain);
    return true;
}

void JaiGigECamera::setBacklightControl(bool enable)
{
    m_config.m_autoBacklightControl = enable;
    emit backlightControlChanged(enable);
}

// ── Camera settings ──────────────────────────────────────────────────────────────────────────

void JaiGigECamera::readCameraSetting(ValueSync values)
{
    if (m_device == nullptr) {
        LOG_DEV_ERR << "JAI camera read settings failed: device is null";
        return;
    }

    PvGenParameterArray *params = m_device->GetParameters();
    if (params == nullptr) {
        LOG_DEV_ERR << "JAI camera read settings failed: no parameter array";
        return;
    }

    // Identity: the camera's own GenICam nodes first, what discovery reported as the fallback.
    //
    // `PvDevice` has no device-info accessor — unlike Pylon's CInstantCamera::GetDeviceInfo(),
    // the eBUS record belongs to `PvDeviceInfo` and exists only during enumeration. Reading the
    // SFNC nodes is also the better answer: `DeviceUserID` is what the operator set, and it can
    // have been changed since the discovery pass that found this camera.
    const auto readString = [params](const char *const *candidates, QString *out) {
        const PvString node = resolveNode(params, candidates);
        if (!node.GetLength()) {
            return;
        }
        PvString value;
        if (params->GetStringValue(node, value).IsOK() && value.GetLength() > 0) {
            *out = qs(value);
        }
    };
    static const char *const kModelNode[]  = { "DeviceModelName", nullptr };
    static const char *const kSerialNode[] = { "DeviceSerialNumber", "DeviceID", nullptr };
    static const char *const kUserNode[]   = { "DeviceUserID", "DeviceUserDefinedName", nullptr };

    m_config.m_modelName       = m_discovered.modelName;
    m_config.m_serialNumber    = m_discovered.serialNumber;
    m_config.m_userDefinedName = m_discovered.userDefinedName;
    if (!m_discovered.ipAddress.isEmpty()) {
        m_config.m_ipAddress = m_discovered.ipAddress;
    }
    readString(kModelNode, &m_config.m_modelName);
    readString(kSerialNode, &m_config.m_serialNumber);
    readString(kUserNode, &m_config.m_userDefinedName);

    // Pixel format decides the cv::Mat shape downstream, so it is read, never assumed.
    PvString pixelFormat;
    if (params->GetEnumValue(PvString("PixelFormat"), pixelFormat).IsOK()) {
        m_config.m_pixelFormat = qs(pixelFormat);
    }

    // Exposure. The node NAME differs between SFNC and legacy firmware, and so does its TYPE —     // see the note on NumericFeature in jai_define.h. Reading it as a float and ignoring the
    // failed result is what leaves the UI with a 0..0 range and an unusable exposure field.
    const NumericFeature exposure = readNumericFeature(params, node_names::kExposureTime);
    if (!exposure.exists()) {
        LOG_USER_WARN << tr("JAI camera exposes no exposure-time feature; exposure control is "
                            "unavailable on this model.");
    } else if (!exposure.valid) {
        // Keep whatever limits we had rather than writing 0..0 into the config: a degenerate
        // range reaches the property browser as a field that cannot be moved off zero.
        LOG_USER_WARN << tr("JAI camera reported an unusable exposure range (%1); the previous "
                            "limits are kept. Exposure limits on this camera depend on the "
                            "acquisition frame rate, so check that setting.")
                             .arg(exposure.describe());
    } else {
        m_config.m_paramsExposureMin = exposure.min;
        m_config.m_paramsExposureMax = exposure.max;
        // A saved value outside what THIS camera accepts is replaced by the camera's own — a
        // project moved to a different model would otherwise push a rejected value on every
        // apply and never say why.
        //
        // Skipped on the pre-push read (LimitsOnly), and that is not a detail. The exposure
        // maximum on this family moves with the acquisition frame rate: a camera that powered up
        // at 22 fps reports 10..44730, so a commissioned 199892 us — perfectly legal at the
        // configured 5 fps — looks out of range and would be thrown away here, moments before the
        // push that was about to restore the frame rate and make it legal again. The post-push
        // read runs with the ranges we will actually grab at, which is where clamping belongs.
        if ((values == ValueSync::AdoptFromCamera)
            && ((m_config.m_paramsExposureTime < exposure.min)
                || (m_config.m_paramsExposureTime > exposure.max))) {
            m_config.m_paramsExposureTime = exposure.value;
        }
    }

    // Adopted only after the push. Read unconditionally, this overwrote the commissioned mode with
    // whatever the camera powered up in — so a power-cycled camera came back Off and the panel
    // then agreed with it, erasing the evidence that anything had been configured.
    if (values == ValueSync::AdoptFromCamera) {
        const PvString exposureAutoNode = resolveNode(params, node_names::kExposureAuto);
        if (exposureAutoNode.GetLength()) {
            PvString mode;
            if (params->GetEnumValue(exposureAutoNode, mode).IsOK()) {
                m_config.m_autoExposureMode = JaiExposureModeFromGenICam(qs(mode));
            }
        }
    }

    // Gain. SFNC exposes it as a float in dB, legacy firmware as an integer in device units;
    // one read handles both.
    NumericFeature gain = readNumericFeature(params, node_names::kGainFloat);
    if (!gain.exists()) {
        gain = readNumericFeature(params, node_names::kGainInteger);
    }
    if (!gain.exists()) {
        LOG_USER_WARN << tr("JAI camera exposes no gain feature; gain control is unavailable "
                            "on this model.");
    } else if (!gain.valid) {
        LOG_USER_WARN << tr("JAI camera reported an unusable gain range (%1); the previous "
                            "limits are kept.").arg(gain.describe());
    } else {
        // Rounded outward, not truncated. CameraCfg's gain limits are `int` while an SFNC
        // float gain is often a fraction of a dB: truncating a 0.0..3.98 range to 0..3 would
        // silently forbid the top of the camera's own range, and a 0.0..0.9 range would collapse
        // to 0..0 and make gain unsettable.
        m_config.m_paramsGainMin = static_cast<int>(std::floor(gain.min));
        m_config.m_paramsGainMax = static_cast<int>(std::ceil(gain.max));
        if ((values == ValueSync::AdoptFromCamera)
            && ((m_config.m_paramsGain < gain.min) || (m_config.m_paramsGain > gain.max))) {
            m_config.m_paramsGain = gain.value;
        }
    }

    // Frame rate.
    const NumericFeature frameRate = readNumericFeature(params, node_names::kFrameRate);
    if (frameRate.exists() && (m_config.m_paramsAcquisitionFrameRate <= 0.0)) {
        m_config.m_paramsAcquisitionFrameRate = frameRate.value;
    }
    // Same as the auto-exposure mode: adopted only after the push. A camera at defaults reports
    // rate limiting off, and taking that at connect turned the commissioned limit off in the
    // config too — the setting that keeps this camera inside the paced link budget.
    if (values == ValueSync::AdoptFromCamera) {
        const PvString frameRateEnableNode = resolveNode(params, node_names::kFrameRateEnable);
        if (frameRateEnableNode.GetLength()) {
            bool enabled = false;
            if (params->GetBooleanValue(frameRateEnableNode, enabled).IsOK()) {
                m_config.m_enableAcquisitionFrameRate = enabled;
            }
        }
    }

    // Colour or mono, decided by the SDK from the actual pixel type rather than by matching
    // format-name substrings, which would get "BayerRG8" wrong.
    int64_t pixelTypeValue = 0;
    if (params->GetEnumValue(PvString("PixelFormat"), pixelTypeValue).IsOK()) {
        m_config.m_isColor = PvImage::IsPixelColor(static_cast<PvPixelType>(pixelTypeValue));
    }

    // Logged, always. Without this line neither the operator nor the next reader can tell a
    // camera that reports a narrow range from a range this device failed to read — and those
    // two look identical on the property browser, as an exposure field that will not move.
    LOG_USER_INFO << tr("JAI camera features: %1").arg(cameraFeatureSnapshot());
}

QString JaiGigECamera::cameraFeatureSnapshot() const
{
    if (m_device == nullptr) {
        return QStringLiteral("not connected");
    }
    PvGenParameterArray *params = m_device->GetParameters();
    if (params == nullptr) {
        return QStringLiteral("not connected");
    }

    const NumericFeature exposure = readNumericFeature(params, node_names::kExposureTime);
    NumericFeature gain = readNumericFeature(params, node_names::kGainFloat);
    if (!gain.exists()) {
        gain = readNumericFeature(params, node_names::kGainInteger);
    }
    const NumericFeature frameRate = readNumericFeature(params, node_names::kFrameRate);

    return QStringLiteral("exposure %1 | gain %2 | frame rate %3 | payload %4 bytes")
        .arg(exposure.describe(), gain.describe(), frameRate.describe())
        .arg(m_device->GetPayloadSize());
}

bool JaiGigECamera::applyParametersChange()
{
    if (m_device == nullptr) {
        LOG_DEV_ERR << "JAI camera apply parameters failed: device is null";
        emit parametersApplied(false);
        return false;
    }

    PvGenParameterArray *params = m_device->GetParameters();
    if (params == nullptr) {
        emit parametersApplied(false);
        return false;
    }

    bool allOk = true;
    const auto note = [&allOk](const QString &what, const PvResult &result) {
        if (!result.IsOK()) {
            allOk = false;
            LOG_USER_ERR << tr("JAI camera could not set %1: %2").arg(what, describe(result));
        }
    };

    const PvString exposureAutoNode = resolveNode(params, node_names::kExposureAuto);
    if (exposureAutoNode.GetLength()) {
        note(QStringLiteral("ExposureAuto"),
             params->SetEnumValue(exposureAutoNode,
                                  PvString(JaiExposureModeToGenICam(m_config.m_autoExposureMode))));
    }

    // The frame-rate limit goes FIRST. On this camera family the exposure range depends on it —     // a longer exposure than the frame interval is impossible — so writing exposure before the
    // frame rate means writing it against the old, narrower limit and having it clamped.
    const PvString frameRateEnableNode = resolveNode(params, node_names::kFrameRateEnable);
    if (frameRateEnableNode.GetLength()) {
        note(QStringLiteral("AcquisitionFrameRateEnable"),
             params->SetBooleanValue(frameRateEnableNode, m_config.m_enableAcquisitionFrameRate));
    }
    if (m_config.m_enableAcquisitionFrameRate) {
        const NumericFeature frameRate = readNumericFeature(params, node_names::kFrameRate);
        if (frameRate.exists()) {
            note(QStringLiteral("AcquisitionFrameRate"),
                 writeNumericFeature(params, frameRate,
                                     m_config.m_paramsAcquisitionFrameRate));
        }
    }

    // Exposure time is only writable while auto-exposure is off; writing it under Continuous is
    // rejected by the camera and would report a failure the operator cannot act on.
    if (m_config.m_autoExposureMode == JaiExposureMode::Exposure_Off) {
        const NumericFeature exposure = readNumericFeature(params, node_names::kExposureTime);
        if (exposure.exists()) {
            note(QStringLiteral("ExposureTime"),
                 writeNumericFeature(params, exposure, m_config.m_paramsExposureTime));
        }
    }

    NumericFeature gain = readNumericFeature(params, node_names::kGainFloat);
    if (!gain.exists()) {
        gain = readNumericFeature(params, node_names::kGainInteger);
    }
    if (gain.exists()) {
        note(QString::fromUtf8(gain.node.GetAscii()),
             writeNumericFeature(params, gain, m_config.m_paramsGain));
    }

    // The backlight line and its polarity are edited in the same property browser as exposure and
    // gain, so they have to be re-applied here too. Without this the operator changes the line,
    // presses Apply, and the camera keeps driving the old one until the next connect.
    configureBacklightOutput();

    // Re-read afterwards. GenICam ranges are not constants: changing the frame rate moves the
    // exposure maximum, and a property browser still showing the pre-change limits is the
    // "exposure limit is wrong" report this device already produced once.
    readCameraSetting();

    emit parametersApplied(allOk);
    return allOk;
}

// ── Digital I/O ──────────────────────────────────────────────────────────────────────────────

void JaiGigECamera::initializeIOPort()
{
    m_io_lines.clear();
    if (m_device == nullptr) {
        return;
    }

    PvGenParameterArray *params = m_device->GetParameters();
    if (params == nullptr) {
        return;
    }

    PvGenEnum *lineSelector = params->GetEnum(PvString("LineSelector"));
    if (lineSelector == nullptr || !lineSelector->IsAvailable()) {
        LOG_USER_WARN << tr("JAI camera exposes no LineSelector; digital I/O is unavailable.");
        return;
    }

    int64_t entryCount = 0;
    if (!lineSelector->GetEntriesCount(entryCount).IsOK()) {
        return;
    }

    for (int64_t index = 0; index < entryCount; ++index) {
        const PvGenEnumEntry *entry = nullptr;
        if (!lineSelector->GetEntryByIndex(index, &entry).IsOK() || entry == nullptr) {
            continue;
        }
        bool available = false;
        if (!entry->IsAvailable(available).IsOK() || !available) {
            continue;
        }

        PvString symbolic;
        if (!entry->GetName(symbolic).IsOK()) {
            continue;
        }

        if (!lineSelector->SetValue(symbolic).IsOK()) {
            continue;
        }

        JaiIOLine line;
        line.name = qs(symbolic);

        PvGenEnum *lineMode = params->GetEnum(PvString("LineMode"));
        if (lineMode != nullptr && lineMode->IsAvailable()) {
            line.is_writable = lineMode->IsWritable();
            line.can_be_input = hasEnumEntry(lineMode, "Input");
            line.can_be_output = hasEnumEntry(lineMode, "Output");
        } else {
            // No LineMode means the line's direction is fixed in hardware. Fall back to what
            // the camera does expose: a line with a readable status is an input, a line that
            // can be driven from a user output is an output.
            PvGenBoolean *status = params->GetBoolean(PvString("LineStatus"));
            line.can_be_input = (status != nullptr && status->IsAvailable());
            PvGenEnum *source = params->GetEnum(PvString("LineSource"));
            line.can_be_output = (source != nullptr && source->IsAvailable());
        }

        LOG_DEV_INFO << QStringLiteral("JAI camera line %1: input=%2 output=%3 writable=%4")
                            .arg(line.name)
                            .arg(line.can_be_input)
                            .arg(line.can_be_output)
                            .arg(line.is_writable);

        m_io_lines.append(line);
    }
}

PvGenParameterArray *JaiGigECamera::selectLine(const QString &line)
{
    // Guarded on the SDK handle, NOT on isDeviceConnected(). That reports the app's published
    // status, which deviceConnect() only sets to Connected on its very last line — so every line
    // access made *during* connect returned null and did nothing, silently. That is what stopped
    // configureBacklightOutput() applying at connect: the backlight only ever got set up when the
    // operator later pressed Apply, and a fresh session or a power-cycled camera ran with
    // whatever routing the camera came up in.
    if (m_device == nullptr) {
        LOG_DEV_ERR << "JAI camera line access failed: no device";
        return nullptr;
    }
    if (line.isEmpty()) {
        return nullptr;
    }

    PvGenParameterArray *params = m_device->GetParameters();
    if (params == nullptr) {
        return nullptr;
    }

    PvGenEnum *lineSelector = params->GetEnum(PvString("LineSelector"));
    if (lineSelector == nullptr || !lineSelector->IsAvailable()) {
        LOG_USER_ERR << tr("LineSelector does not exist on this camera.");
        return nullptr;
    }
    if (!lineSelector->SetValue(pv(line)).IsOK()) {
        LOG_USER_ERR << tr("%1 is not available on this camera.").arg(line);
        return nullptr;
    }
    return params;
}

bool JaiGigECamera::readIO(QString name)
{
    PvGenParameterArray *params = selectLine(name);
    if (params == nullptr) {
        return false;
    }

    bool value = false;
    const PvResult result = params->GetBooleanValue(PvString("LineStatus"), value);
    if (!result.IsOK()) {
        LOG_USER_ERR << tr("Could not read line %1: %2").arg(name, describe(result));
        return false;
    }
    return value;
}

QString JaiGigECamera::routeLineToUserOutput(const QString &line, bool invert)
{
    PvGenParameterArray *params = selectLine(line);
    if (params == nullptr) {
        return QString();
    }

    // Direction first: on models whose lines are switchable, LineSource and LineInverter do not
    // apply until the line is an output. Guarded on writability because on this camera family the
    // direction is fixed in hardware (LineMode reads writable=0), where attempting it would log a
    // failure for something that was never wrong.
    const PvString lineModeNode = resolveNode(params, node_names::kLineMode);
    if (lineModeNode.GetLength()) {
        if (PvGenEnum *lineMode = params->GetEnum(lineModeNode)) {
            if (lineMode->IsWritable() && hasEnumEntry(lineMode, "Output")) {
                lineMode->SetValue(PvString("Output"));
            }
        }
    }

    // Polarity in HARDWARE, via LineInverter — not by flipping the value before writing it.
    // The two look equivalent while this code is the only thing driving the pin and are not:
    // a software flip leaves the output at the wrong level whenever the value is written by
    // anything else, including the camera's own power-up default.
    const PvString inverterNode = resolveNode(params, node_names::kLineInverter);
    if (inverterNode.GetLength()) {
        const PvResult inverted = params->SetBooleanValue(inverterNode, invert);
        if (!inverted.IsOK()) {
            LOG_USER_WARN << tr("JAI camera would not set the inverter on line %1 (%2); the "
                                "output's polarity is whatever the camera defaults to.")
                                 .arg(line, describe(inverted));
        }
    } else if (invert) {
        // Only worth saying when inversion was actually asked for.
        LOG_USER_WARN << tr("JAI camera line %1 has no LineInverter, so the requested inverted "
                            "polarity cannot be applied.").arg(line);
    }

    const PvString sourceNode = resolveNode(params, node_names::kLineSource);
    PvGenEnum *lineSource = sourceNode.GetLength() ? params->GetEnum(sourceNode) : nullptr;
    if (lineSource == nullptr || !lineSource->IsAvailable()) {
        LOG_USER_ERR << tr("Line %1 has no LineSource; it cannot be driven as an output.")
                            .arg(line);
        return QString();
    }

    // UserOutput0 by default, per the commissioned convention. Still checked rather than assumed,
    // and still able to fall back: the number of user outputs is model-dependent, and a camera
    // that starts at UserOutput1 would otherwise fail here with nothing pointing at why.
    const char *chosen = nullptr;
    for (const char *const *candidate = kUserOutputCandidates; *candidate != nullptr; ++candidate) {
        if (hasEnumEntry(lineSource, *candidate)) {
            chosen = *candidate;
            break;
        }
    }
    if (chosen == nullptr) {
        LOG_USER_ERR << tr("Line %1 offers no user-controlled output source.").arg(line);
        return QString();
    }

    const PvResult sourceSet = lineSource->SetValue(PvString(chosen));
    if (!sourceSet.IsOK()) {
        LOG_USER_ERR << tr("Could not route line %1 to %2: %3")
                            .arg(line, QString::fromLatin1(chosen), describe(sourceSet));
        return QString();
    }

    return QString::fromLatin1(chosen);
}

bool JaiGigECamera::setUserOutputState(const QString &userOutput, bool value)
{
    // Same reason as selectLine(): the SDK handle, not the published status, or this cannot run
    // during deviceConnect().
    if (userOutput.isEmpty() || m_device == nullptr) {
        return false;
    }
    PvGenParameterArray *params = m_device->GetParameters();
    if (params == nullptr) {
        return false;
    }

    const PvString selectorNode = resolveNode(params, node_names::kUserOutputSelector);
    const PvString valueNode    = resolveNode(params, node_names::kUserOutputValue);
    if (!selectorNode.GetLength() || !valueNode.GetLength()) {
        LOG_USER_ERR << tr("JAI camera exposes no user-output control; outputs cannot be driven.");
        return false;
    }

    const PvResult selected = params->SetEnumValue(selectorNode, pv(userOutput));
    if (!selected.IsOK()) {
        LOG_USER_ERR << tr("Could not select %1: %2").arg(userOutput, describe(selected));
        return false;
    }

    const PvResult written = params->SetBooleanValue(valueNode, value);
    if (!written.IsOK()) {
        LOG_USER_ERR << tr("Could not drive %1: %2").arg(userOutput, describe(written));
        return false;
    }
    return true;
}

void JaiGigECamera::configureBacklightOutput()
{
    m_backlightUserOutput.clear();
    if (m_device == nullptr || m_config.m_autoBacklightLine.isEmpty()) {
        return;
    }

    m_backlightUserOutput =
        routeLineToUserOutput(m_config.m_autoBacklightLine, m_config.m_autoBacklightInvert);
    if (m_backlightUserOutput.isEmpty()) {
        LOG_USER_ERR << tr("JAI camera backlight output could not be set up on line %1; the "
                           "backlight will not switch.").arg(m_config.m_autoBacklightLine);
        return;
    }

    LOG_USER_INFO << tr("JAI camera backlight output ready: line %1 driven by %2, inverted %3.")
                         .arg(m_config.m_autoBacklightLine,
                              m_backlightUserOutput,
                              m_config.m_autoBacklightInvert ? tr("yes") : tr("no"));
}

QString JaiGigECamera::backlightDiagnostics() const
{
    if (m_backlightUserOutput.isEmpty() || m_device == nullptr) {
        return QStringLiteral("not configured");
    }
    PvGenParameterArray *params = m_device->GetParameters();
    if (params == nullptr) {
        return QStringLiteral("not configured");
    }

    // Re-selects the line, because LineInverter and LineSource are per-selected-line registers and
    // anything since could have moved the selector.
    const PvString selectorNode = resolveNode(params, node_names::kLineSelector);
    if (selectorNode.GetLength()) {
        params->SetEnumValue(selectorNode, pv(m_config.m_autoBacklightLine));
    }

    QString out = QStringLiteral("line=%1 userOutput=%2")
                      .arg(m_config.m_autoBacklightLine, m_backlightUserOutput);

    const PvString sourceNode = resolveNode(params, node_names::kLineSource);
    PvString source;
    if (sourceNode.GetLength() && params->GetEnumValue(sourceNode, source).IsOK()) {
        out.append(QStringLiteral(" LineSource=%1").arg(qs(source)));
    }
    const PvString inverterNode = resolveNode(params, node_names::kLineInverter);
    bool inverter = false;
    if (inverterNode.GetLength() && params->GetBooleanValue(inverterNode, inverter).IsOK()) {
        out.append(QStringLiteral(" LineInverter=%1").arg(inverter ? 1 : 0));
    }

    // The level currently held on the user output, selected before reading for the same reason.
    const PvString userSelectorNode = resolveNode(params, node_names::kUserOutputSelector);
    const PvString userValueNode    = resolveNode(params, node_names::kUserOutputValue);
    if (userSelectorNode.GetLength() && userValueNode.GetLength()
        && params->SetEnumValue(userSelectorNode, pv(m_backlightUserOutput)).IsOK()) {
        bool level = false;
        if (params->GetBooleanValue(userValueNode, level).IsOK()) {
            out.append(QStringLiteral(" UserOutputValue=%1").arg(level ? 1 : 0));
        }
    }

    return out;
}

void JaiGigECamera::setBacklightState(bool on)
{
    // Only the user output's level. The routing — LineSelector, LineMode, LineInverter,
    // LineSource — is static configuration done once by configureBacklightOutput(); redoing it on
    // every grab is four extra GenICam register writes per trigger for a wiring decision that
    // cannot change between them, and it is how the polarity flag ended up applied in software.
    if (m_backlightUserOutput.isEmpty()) {
        return;
    }
    if (setUserOutputState(m_backlightUserOutput, on)) {
        m_backlightOn = on;
    }
    emit backlightStateChanged(m_backlightOn);
}

bool JaiGigECamera::setAutoBacklightState(bool on)
{
    if (!m_config.m_autoBacklightControl || m_config.m_autoBacklightLine.isEmpty()) {
        return false;
    }
    if (m_backlightOverride) {
        // The operator owns the lamp until they release it. Switching it here is exactly what
        // would make a manual toggle look like it worked and then silently undo itself at the
        // next trigger — the failure the override exists to prevent.
        return false;
    }
    setBacklightState(on);
    return true;
}

bool JaiGigECamera::setBacklightOverride(bool on)
{
    if (m_device == nullptr || !isDeviceConnected()) {
        m_last_msg = tr("Backlight control failed: the JAI camera is not connected.");
        LOG_USER_ERR << m_last_msg;
        emit backlightStateChanged(m_backlightOn);
        return false;
    }
    if (m_backlightUserOutput.isEmpty()) {
        // Configured by configureBacklightOutput() at connect and on every apply. Empty means the
        // operator has not chosen a backlight line, or the chosen one could not be routed — both
        // are commissioning state, not faults, so say which rather than failing silently.
        m_last_msg = tr("Backlight control failed: no backlight output line is configured for "
                        "this camera.");
        LOG_USER_ERR << m_last_msg;
        emit backlightStateChanged(m_backlightOn);
        return false;
    }

    // Set the flag before driving the lamp: setBacklightState() emits, and a listener that reads
    // isBacklightOverridden() from that emission must not see the old value.
    m_backlightOverride = on;
    setBacklightState(on);

    LOG_DEV_INFO << "JAI camera backlight override" << (on ? "engaged" : "released")
                 << "output=" << m_backlightUserOutput << "level=" << m_backlightOn;
    return m_backlightOn == on;
}

void JaiGigECamera::setOutputLineState(const QString &line, bool value)
{
    // The ad-hoc path, for writeIO() on an arbitrary line: route then drive, every time, because
    // nothing has configured that line in advance.
    //
    // No inversion here. This used to apply m_autoBacklightInvert — the BACKLIGHT's polarity —
    // to whichever line it was handed, so a station with an inverted backlight silently drove
    // every other output upside down too.
    const QString userOutput = routeLineToUserOutput(line, false);
    if (userOutput.isEmpty()) {
        return;
    }
    setUserOutputState(userOutput, value);
}

bool JaiGigECamera::writeIO(QString name, bool value)
{
    if (m_device == nullptr || !isDeviceConnected()) {
        LOG_DEV_ERR << "JAI camera write IO failed: camera is not connected";
        return false;
    }
    setOutputLineState(name, value);
    return true;
}

// ── Acquisition ──────────────────────────────────────────────────────────────────────────────

void JaiGigECamera::jai_buffer_to_mat(PvBuffer *buffer, cv::Mat &mat)
{
    mat = cv::Mat();
    if (buffer == nullptr) {
        return;
    }

    IPvImage *source = buffer->GetImage();
    if (source == nullptr) {
        LOG_DEV_ERR << "JAI camera buffer carries no image payload";
        return;
    }

    const PvPixelType sourceType = source->GetPixelType();
    const bool isColor = PvImage::IsPixelColor(sourceType);
    const PvPixelType targetType = isColor ? PvPixelBGR8 : PvPixelMono8;

    if (!PvBufferConverter::IsConversionSupported(sourceType, targetType)) {
        // Empty mat and a named format, rather than reinterpreting bytes we do not understand:
        // a wrong-but-plausible image is far worse here than no image, because the matcher
        // would happily report positions from it.
        LOG_USER_ERR << tr("JAI camera pixel format %1 cannot be converted to %2; "
                           "no image produced.")
                            .arg(m_config.m_pixelFormat,
                                 isColor ? QStringLiteral("BGR8") : QStringLiteral("Mono8"));
        return;
    }

    PvBuffer destination;
    destination.GetImage()->Alloc(source->GetWidth(), source->GetHeight(), targetType);

    PvBufferConverter converter;
    const PvResult converted = converter.Convert(buffer, &destination, true);
    if (!converted.IsOK()) {
        LOG_USER_ERR << tr("JAI camera image conversion failed: %1").arg(describe(converted));
        return;
    }

    IPvImage *result = destination.GetImage();
    const int rows = static_cast<int>(result->GetHeight());
    const int cols = static_cast<int>(result->GetWidth());

    // Cloned, not wrapped: `destination` is a stack object and its buffer dies with this call.
    const cv::Mat view(rows, cols, isColor ? CV_8UC3 : CV_8UC1, result->GetDataPointer());
    mat = view.clone();
}

GrabResult JaiGigECamera::grabSingleShot()
{
    // ONE result object for every path. The Basler implementation shadows this with a local in
    // its success branch, so the value it *returns* always says the grab failed while the signal
    // it emits says it succeeded; that is documented on grabSingleShot() there. Not repeated.
    GrabResult result;
    result.isGrabSuccess = false;

    // CameraRunner resolves its in-flight CameraSingleShot command from grabFinished, so EVERY
    // exit below must emit exactly once. A silent return leaves the command hanging until the
    // watchdog fires.
    const auto finish = [this, &result](const QString &message, bool success) {
        result.isGrabSuccess = success;
        result.msg = message;
        if (!success) {
            m_last_msg = message;
        }
        emit grabFinished(result);
        return result;
    };

    if (m_device == nullptr || m_pipeline == nullptr || !isDeviceConnected()) {
        return finish(tr("Grab failed: the JAI camera is not connected."), false);
    }

    if (m_acquisition == nullptr) {
        return finish(tr("Grab failed: the acquisition manager is not available."), false);
    }

    // Single shot wins. Rejecting the grab instead would make calibration Detect fail with a
    // confusing error whenever live view happened to be on — and the operator asking for one
    // identified frame plainly wants the stream out of the way, not an error message.
    if (m_continuousActive) {
        LOG_DEV_INFO << "JAI camera single shot pre-empted continuous acquisition.";
        stopContinuousShot();
    }

    // Settle time is owed only when this grab is what switched the lamp on. Under a manual
    // override it has been on since the operator pressed the button, and sleeping again would
    // add the delay to every trigger for nothing.
    if (setAutoBacklightState(true) && m_config.m_autoBacklightDelay > 0) {
        QThread::usleep(static_cast<unsigned long>(m_config.m_autoBacklightDelay));
    }

    // Drain anything the pipeline is still holding from a previous acquisition — it is what stops
    // a single shot returning a frame captured before the backlight came on.
    drainPipeline();

    QString message = tr("Grab failed: no image arrived within %1 ms.")
                          .arg(m_config.m_grabTimeoutMs);
    bool success = false;
    bool linkGone = false;
    bool startFailed = false;
    int discarded = 0;
    int triggers = 0;
    QString lastDiscardReason;

    QElapsedTimer deadline;
    deadline.start();

    // TWO nested loops, and the outer one is the point.
    //
    // In SingleFrame the camera sends exactly ONE frame per AcquisitionStart. If that frame
    // arrives incomplete there is nothing else coming, so discarding it and waiting — which is
    // what a single loop does — burns the whole timeout on a stream that has already finished.
    // Observed exactly that: one RESENDS_FAILURE at ~120 ms, then 2.9 s of waiting, every grab.
    // A damaged frame must instead be answered by re-arming: stop, start, ask for another.
    //
    // In Continuous the camera keeps sending, so waiting IS the recovery and re-arming would
    // throw away frames already in flight. Hence the mode check on the retry path.
    while (!success && !linkGone && deadline.elapsed() < m_config.m_grabTimeoutMs) {
        // Through the state manager, not a bare AcquisitionStart: it also sets and releases
        // `TLParamsLocked`, which this device skipped entirely at first. The SDK's own samples
        // set it, and a camera streaming with transport parameters unlocked is free to disagree
        // with the buffer sizes we allocated.
        //
        // Unwind a lock left over from a previous grab first. Start() fails with STATE_ERROR when
        // TLParamsLocked is already held, and nothing in that failure path clears it — so without
        // this, one interrupted grab makes every later grab fail identically until the operator
        // reconnects the camera. That is not hypothetical: it is how a stray StreamEnable()
        // bricked single-shot entirely, and a recovery that costs one call is worth having.
        if (m_acquisition->GetState() == PvAcquisitionStateLocked) {
            m_acquisition->Stop();
        }

        const PvResult started = m_acquisition->Start();
        if (!started.IsOK()) {
            message   = tr("Grab failed: acquisition could not start: %1").arg(describe(started));
            startFailed = true;
            break;
        }
        ++triggers;

        while (deadline.elapsed() < m_config.m_grabTimeoutMs) {
            const int remaining =
                qMax(1, m_config.m_grabTimeoutMs - static_cast<int>(deadline.elapsed()));

            PvBuffer *buffer = nullptr;
            PvResult operation;
            const PvResult retrieved = m_pipeline->RetrieveNextBuffer(
                &buffer, static_cast<uint32_t>(remaining), &operation);

            if (!retrieved.IsOK()) {
                if (retrieved.GetCode() == PvResult::Code::TIMEOUT) {
                    continue;   // keep waiting until our own deadline, not the SDK's
                }
                message = tr("Grab failed: %1").arg(describe(retrieved));
                // A stream that has gone away reports here as well as through the link sink; the
                // sink is the primary path, this is the backstop for a camera that stopped
                // answering without dropping the link.
                linkGone = (retrieved.GetCode() == PvResult::Code::ABORTED)
                           || (retrieved.GetCode() == PvResult::Code::NOT_CONNECTED);
                break;
            }

            if (!operation.IsOK()) {
                // An incomplete block: the frame was damaged in flight.
                //
                // Counted, not logged per buffer. One line per discarded block put 272 entries in
                // a single commissioning session and buried everything else, while saying the
                // same thing every time; the summary below carries the same information and names
                // a cause the reader can act on.
                ++discarded;
                lastDiscardReason = describe(operation);
                m_pipeline->ReleaseBuffer(buffer);
                if (m_singleFrameMode) {
                    break;   // nothing more is coming — re-arm in the outer loop
                }
                continue;    // Continuous: the next frame is already on its way
            }

            cv::Mat image;
            jai_buffer_to_mat(buffer, image);
            m_pipeline->ReleaseBuffer(buffer);

            if (image.empty()) {
                message = tr("Grab failed: the image could not be converted.");
            } else {
                result.frame = image;
                success      = true;
                message      = tr("Grab successful.");
            }
            break;
        }

        if (!m_singleFrameMode) {
            break;   // one acquisition window is all a free-running camera gets
        }
    }

    // Only when still locked. In SingleFrame the manager counts the one expected frame and
    // releases TLParamsLocked by itself, so an unconditional Stop() here would report a failure
    // on exactly the grabs that went right.
    if (m_acquisition->GetState() == PvAcquisitionStateLocked) {
        const PvResult stopped = m_acquisition->Stop();
        if (!stopped.IsOK()) {
            LOG_DEV_ERR << "JAI camera acquisition stop failed:" << describe(stopped);
        }
    }

    if (!success && !startFailed) {
        // The numbers that separate "the link is dropping packets" from "the camera never
        // triggered". Only on failure: on the happy path they are noise.
        LOG_DEV_INFO << "JAI camera grab diagnostics:" << streamDiagnostics()
                     << "triggers=" << triggers << "discarded=" << discarded;
    }

    setAutoBacklightState(false);

    if (discarded > 0) {
        // Named causes, not just a count. Every one of these has a different fix, and an
        // operator reading "incomplete buffer" has no way to guess which applies.
        // Names the setting that actually fixed this, and points at the log line that carries the
        // numbers. Two earlier versions of this message sent the reader to jumbo frames and then
        // to the MTU; both were already correct on the station where every frame was being lost,
        // and neither is what the measurements blamed. See configurePacketDelay().
        LOG_USER_WARN << tr("JAI camera discarded %1 incomplete frame(s) over %2 trigger(s) "
                            "(last: %3). This camera can fill a Gigabit link with a single frame, "
                            "so the first thing to check is the inter-packet delay (GevSCPD) in "
                            "the grab diagnostics below: a zero there means nothing is pacing the "
                            "transmission and the adapter is dropping what it cannot absorb.")
                             .arg(discarded)
                             .arg(triggers)
                             .arg(lastDiscardReason);
    }

    if (!success) {
        LOG_USER_ERR << message;
    }
    if (linkGone) {
        // Published after the signal below would be too late for this grab, but it is what
        // starts recovery for the next one.
        onLinkLost();
    }

    return finish(message, success);
}

QString JaiGigECamera::streamDiagnostics() const
{
    if (m_device == nullptr || m_stream == nullptr) {
        return QStringLiteral("not connected");
    }

    // Read by name and skip whatever this camera does not expose. The counter names below are
    // eBUS stream statistics rather than SFNC device features, but they are still model-dependent
    // — asking for all of them and reporting what answers beats hard-coding one vendor's set.
    const auto readInt = [](PvGenParameterArray *params, const char *name, QString *out) {
        if (params == nullptr) {
            return;
        }
        int64_t value = 0;
        if (params->GetIntegerValue(PvString(name), value).IsOK()) {
            out->append(QStringLiteral(" %1=%2").arg(QString::fromLatin1(name)).arg(value));
        }
    };
    const auto readFloat = [](PvGenParameterArray *params, const char *name, QString *out) {
        if (params == nullptr) {
            return;
        }
        double value = 0.0;
        if (params->GetFloatValue(PvString(name), value).IsOK()) {
            out->append(QStringLiteral(" %1=%2")
                            .arg(QString::fromLatin1(name))
                            .arg(value, 0, 'f', 1));
        }
    };

    QString out;
    PvGenParameterArray *device = m_device->GetParameters();
    // GevSCPSPacketSize is what NegotiatePacketSize() settled on: ~1500 means jumbo frames are
    // off somewhere in the path. GevSCPD is the inter-packet delay in timestamp ticks; 0 means
    // the camera transmits a burst at full line rate, which is what makes an adapter drop it.
    // GevSCPD is the load-bearing one: a zero here on a camera that is dropping frames is the
    // whole diagnosis. See configurePacketDelay().
    readInt(device, "GevSCPSPacketSize", &out);
    readInt(device, "GevSCPD", &out);
    readInt(device, "GevTimestampTickFrequency", &out);

    PvGenParameterArray *stream = m_stream->GetParameters();
    readInt(stream, "BlockCount", &out);
    readInt(stream, "BlockErrorCount", &out);
    readInt(stream, "ImagesDropped", &out);
    readInt(stream, "LostPacketCount", &out);
    readInt(stream, "ResendRequestCount", &out);
    readInt(stream, "ResendPacketCount", &out);
    readInt(stream, "ResendGroupRequestedCount", &out);
    readInt(stream, "ErrorCount", &out);
    readFloat(stream, "Bandwidth", &out);
    readFloat(stream, "AcquisitionRate", &out);

    return out.isEmpty() ? QStringLiteral("no counters available") : out.trimmed();
}

bool JaiGigECamera::startAutoContinuousShot()
{
    return false;
}

void JaiGigECamera::stopAutoContinousShot()
{
}

double JaiGigECamera::pacedFrameRateCeiling() const
{
    // What the link can actually carry once configurePacketDelay() has paced it, in frames per
    // second. Free-running faster than this puts the camera straight back into the failure that
    // pacing exists to prevent: the sensor's own 22 fps at 5.24 MB a frame is ~115 MB/s, and the
    // paced budget is 40% of a Gigabit link — about 50 MB/s.
    if (m_device == nullptr) {
        return 0.0;
    }
    const uint32_t payload = m_device->GetPayloadSize();
    if (payload == 0) {
        return 0.0;
    }

    int64_t linkSpeedMbps = 0;
    if (PvGenParameterArray *params = m_device->GetParameters()) {
        if (!params->GetIntegerValue(PvString("GevLinkSpeed"), linkSpeedMbps).IsOK()
            || linkSpeedMbps <= 0) {
            linkSpeedMbps = 1000;
        }
    } else {
        linkSpeedMbps = 1000;
    }

    const double bytesPerSecond =
        (static_cast<double>(linkSpeedMbps) * 1.0e6 / 8.0) * kTargetLinkUtilisation;
    // Margin on top: the ceiling is derived from nominal link speed, and running right at a
    // computed limit is how a "safe" setting still drops frames.
    return (bytesPerSecond / static_cast<double>(payload)) * kContinuousRateMargin;
}

void JaiGigECamera::capFrameRateForContinuous()
{
    m_frameRateCapped = false;
    if (m_device == nullptr) {
        return;
    }
    PvGenParameterArray *params = m_device->GetParameters();
    if (params == nullptr) {
        return;
    }

    const double ceiling = pacedFrameRateCeiling();
    const NumericFeature frameRate = readNumericFeature(params, node_names::kFrameRate);
    if ((ceiling <= 0.0) || !frameRate.exists() || !frameRate.valid) {
        LOG_USER_WARN << tr("JAI camera cannot have its frame rate limited for continuous "
                            "acquisition; it may free-run faster than the link carries and "
                            "deliver incomplete frames.");
        return;
    }

    if (frameRate.value <= ceiling) {
        return;   // already slower than the budget; leave the operator's setting alone
    }

    // Frame-rate limiting has to be switched ON as well as set — on this family the rate node is
    // ignored while its enable is false, which would make the cap look applied and do nothing.
    const PvString enableNode = resolveNode(params, node_names::kFrameRateEnable);
    if (enableNode.GetLength()) {
        m_frameRateEnableWasOn = false;
        params->GetBooleanValue(enableNode, m_frameRateEnableWasOn);
        params->SetBooleanValue(enableNode, true);
    }

    const double target = qMax(frameRate.min, ceiling);
    if (!writeNumericFeature(params, frameRate, target).IsOK()) {
        LOG_USER_WARN << tr("JAI camera refused a continuous frame rate of %1 fps; incomplete "
                            "frames are likely.").arg(target, 0, 'f', 1);
        return;
    }

    m_frameRateBeforeContinuous = frameRate.value;
    m_frameRateCapped = true;
    LOG_USER_INFO << tr("JAI camera frame rate limited to %1 fps for continuous acquisition (was "
                        "%2 fps). A %3-byte frame at more than that exceeds what the paced link "
                        "carries, and the frames arrive incomplete.")
                         .arg(target, 0, 'f', 1)
                         .arg(frameRate.value, 0, 'f', 1)
                         .arg(m_device->GetPayloadSize());
}

void JaiGigECamera::restoreFrameRateAfterContinuous()
{
    if (!m_frameRateCapped || m_device == nullptr) {
        return;
    }
    m_frameRateCapped = false;

    PvGenParameterArray *params = m_device->GetParameters();
    if (params == nullptr) {
        return;
    }
    const NumericFeature frameRate = readNumericFeature(params, node_names::kFrameRate);
    if (frameRate.exists() && frameRate.valid) {
        writeNumericFeature(params, frameRate, m_frameRateBeforeContinuous);
    }
    const PvString enableNode = resolveNode(params, node_names::kFrameRateEnable);
    if (enableNode.GetLength()) {
        params->SetBooleanValue(enableNode, m_frameRateEnableWasOn);
    }
}

bool JaiGigECamera::startContinuousShot()
{
    if (!isDeviceConnected() || m_device == nullptr || m_pipeline == nullptr
        || m_acquisition == nullptr) {
        m_last_msg = tr("Continuous acquisition failed: the JAI camera is not connected.");
        LOG_USER_ERR << m_last_msg;
        return false;
    }
    if (m_continuousActive) {
        // Not an error: a widget cannot always know, and refusing loudly would make a double
        // click look like a fault. Still reports the state — see the note on the emit below.
        LOG_DEV_INFO << "JAI camera continuous acquisition already running; start ignored.";
        emit continuousStateChanged(true);
        return true;
    }

    if (!applyAcquisitionMode("Continuous")) {
        m_last_msg = tr("Continuous acquisition failed: the camera would not leave single-frame "
                        "acquisition.");
        LOG_USER_ERR << m_last_msg;
        emit continuousStateChanged(false);
        return false;
    }
    capFrameRateForContinuous();

    // Once for the whole stream, not per frame. Toggling a backlight at the frame rate would
    // strobe the scene and wear the output.
    setAutoBacklightState(true);

    drainPipeline();
    if (m_acquisition->GetState() == PvAcquisitionStateLocked) {
        m_acquisition->Stop();
    }

    const PvResult started = m_acquisition->Start();
    if (!started.IsOK()) {
        m_last_msg = tr("Continuous acquisition failed to start: %1").arg(describe(started));
        LOG_USER_ERR << m_last_msg;
        setAutoBacklightState(false);
        restoreFrameRateAfterContinuous();
        applyAcquisitionMode("SingleFrame");
        emit continuousStateChanged(false);
        return false;
    }

    m_continuousActive    = true;
    m_continuousFrames    = 0;
    m_continuousDiscarded = 0;
    m_continuousClock.start();

    LOG_USER_INFO << tr("JAI camera continuous acquisition started.");
    // Emitted after EVERY start/stop request, including the ones that changed nothing, and not
    // only on an edge. CameraRunner resolves its continuous commands from this signal, so an
    // idempotent stop that stayed silent would leave that command unresolved until its watchdog
    // fired — which is exactly the case a toggle button hits when it and the device disagree.
    emit continuousStateChanged(true);

    // A QUEUED self-post, not a loop. The device lives on the runner's worker thread and is
    // driven entirely by queued signals, so a `while (streaming)` loop here would starve that
    // thread's event loop and the stop command could never be delivered — the only way out would
    // be killing the task. One frame per trip through the event loop keeps stop deliverable, at a
    // worst-case latency of kContinuousRetrieveMs (the time RetrieveNextBuffer may block).
    QMetaObject::invokeMethod(this, "pumpContinuousFrame", Qt::QueuedConnection);
    return true;
}

void JaiGigECamera::pumpContinuousFrame()
{
    if (!m_continuousActive || m_pipeline == nullptr) {
        return;   // stopped between the post and its delivery
    }

    PvBuffer *buffer = nullptr;
    PvResult operation;
    const PvResult retrieved =
        m_pipeline->RetrieveNextBuffer(&buffer, kContinuousRetrieveMs, &operation);

    if (retrieved.IsOK()) {
        if (operation.IsOK()) {
            cv::Mat image;
            jai_buffer_to_mat(buffer, image);
            m_pipeline->ReleaseBuffer(buffer);
            if (!image.empty()) {
                ++m_continuousFrames;
                GrabResult frame;
                frame.isGrabSuccess = true;
                frame.frame         = image;
                frame.msg           = tr("Continuous frame.");
                // NOT grabFinished. CameraRunner resolves its in-flight command from that
                // signal and applies the single-shot retry budget to it; a stream of frames
                // arriving there would resolve commands that are not running.
                emit continuousFrameReady(frame);
            }
        } else {
            // Counted, never logged per frame: at ~9 fps a per-frame line buries the log within
            // seconds, which is the mistake the single-shot path already made once.
            ++m_continuousDiscarded;
            m_pipeline->ReleaseBuffer(buffer);
        }
    } else if (retrieved.GetCode() != PvResult::Code::TIMEOUT) {
        const bool linkGone = (retrieved.GetCode() == PvResult::Code::ABORTED)
                              || (retrieved.GetCode() == PvResult::Code::NOT_CONNECTED);
        LOG_USER_ERR << tr("JAI camera continuous acquisition stopped: %1")
                            .arg(describe(retrieved));
        stopContinuousShot();
        if (linkGone) {
            onLinkLost();
        }
        return;
    }

    QMetaObject::invokeMethod(this, "pumpContinuousFrame", Qt::QueuedConnection);
}

void JaiGigECamera::drainPipeline()
{
    if (m_pipeline == nullptr) {
        return;
    }
    for (;;) {
        PvBuffer *stale = nullptr;
        PvResult staleOperation;
        if (!m_pipeline->RetrieveNextBuffer(&stale, 0, &staleOperation).IsOK()) {
            break;
        }
        m_pipeline->ReleaseBuffer(stale);
    }
}

void JaiGigECamera::stopContinuousShot()
{
    if (!m_continuousActive) {
        // Idempotent: callers cannot always know, and stop-when-stopped is not a fault. Still
        // reports the state, so a CameraContinuousStop command resolves instead of timing out.
        emit continuousStateChanged(false);
        return;
    }

    // Cleared FIRST. Everything below can re-enter the event loop, and the pump checks this flag
    // on every trip — leaving it set until the end would let one more frame be emitted after the
    // caller was told streaming had stopped.
    m_continuousActive = false;

    if ((m_acquisition != nullptr) && (m_acquisition->GetState() == PvAcquisitionStateLocked)) {
        const PvResult stopped = m_acquisition->Stop();
        if (!stopped.IsOK()) {
            LOG_DEV_ERR << "JAI camera continuous stop failed:" << describe(stopped);
        }
    }

    setAutoBacklightState(false);

    restoreFrameRateAfterContinuous();
    // Back to SingleFrame, so the very next single shot behaves as it did before live view was
    // ever used. A camera left in Continuous is the state that floods the link.
    applyAcquisitionMode("SingleFrame");
    drainPipeline();

    const qint64 elapsed = qMax(qint64(1), m_continuousClock.elapsed());
    LOG_USER_INFO << tr("JAI camera continuous acquisition stopped: %1 frame(s) in %2 s (%3 fps), "
                        "%4 incomplete frame(s) discarded.")
                         .arg(m_continuousFrames)
                         .arg(elapsed / 1000.0, 0, 'f', 1)
                         .arg(m_continuousFrames * 1000.0 / static_cast<double>(elapsed), 0, 'f', 1)
                         .arg(m_continuousDiscarded);

    emit continuousStateChanged(false);
}

GrabResult JaiGigECamera::softwareTriggerShot()
{
    return GrabResult();
}

} // namespace vc::device
