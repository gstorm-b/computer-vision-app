#include "vision_tcpip_device_base.h"

#include <QMutexLocker>
#include <utility>

// Robot kinematics component (Phase 2 reachability + optional mesh collision).
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <optional>

#include <RobotKinematics/Collision/CollisionBackend.h>
#include <RobotKinematics/Collision/MeshCollisionProfile.h>
#include <RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h>
#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Model/RobotModelConfig.h>
#include <RobotKinematics/Presets/NachiMZ04D.h>

namespace vc::device {

// =====================================================================
// Connect / disconnect lifecycle
// =====================================================================
/// Activates the device: if already active, just re-syncs runtime state and
/// returns true; otherwise opens the transport (startTransport()). On transport
/// failure, tears it back down, records the failure in diagnostics/connection
/// status, and returns false.
/// @return true if the device is (now) active and its transport is open
bool VisionTcpipDeviceBase::deviceConnect() {
    QMutexLocker locker(&m_mutex);

    if (m_active) {
        LOG_DEV_INFO << "VisionTcpipDeviceBase already active" << name();
        syncRuntimeState();
        return true;
    }

    m_active = true;
    if (!startTransport()) {
        stopTransport();
        m_active = false;
        m_diagnostics.lastError = m_last_msg;
        setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
        return false;
    }

    m_diagnostics.lastError.clear();
    syncRuntimeState();
    return true;
}

/// Deactivates the device: sends a best-effort disconnect notice on the
/// heartbeat channel before tearing down the transport, then clears the last
/// error and marks the connection status as Disconnected.
/// @return always true
bool VisionTcpipDeviceBase::deviceDisconnect() {
    QMutexLocker locker(&m_mutex);
    m_active = false;
    // Tell the peer this is a planned close before the transport is torn down.
    sendDisconnectNotice();
    stopTransport();
    m_diagnostics.lastError.clear();
    setConnectionStatus(ConnectStatus::Disconnected);
    LOG_DEV_INFO << "VisionTcpipDeviceBase disconnected" << name();
    return true;
}

/// Forces a disconnect if the device is currently connected or active;
/// otherwise a no-op.
void VisionTcpipDeviceBase::deviceTerminate() {
    LOG_DEV_DEBUG << "VisionTcpipDeviceBase terminate" << name() << "id" << id();
    if (isDeviceConnected() || m_active) {
        deviceDisconnect();
    }
}

/// Writes a VisionOutputRequest's payload to the main channel socket. Rejects
/// requests that are not Request_VisionOutput or when no main link is connected.
/// When the request is a Result, first runs the advisory robot-kinematics
/// reachability check (runKinematicCheck()) before building/sending the payload.
/// @param request the request to send; must be a VisionOutputRequest
/// @return true if the full payload was written to the socket; false if the
///         request was rejected or the write was short
bool VisionTcpipDeviceBase::pushRequest(IRequest *request) {
    if (!request || request->type() != RequestType::Request_VisionOutput) {
        return false;
    }
    if (!m_mainSocket || m_mainSocket->state() != QAbstractSocket::ConnectedState) {
        LOG_DEV_INFO << "VisionTcpipDeviceBase: drop request, no main link";
        return false;
    }

    VisionOutputRequest *vreq = static_cast<VisionOutputRequest*>(request);

    // Phase 2: advisory robot-kinematics reachability check on the pick poses.
    // Runs before the write; it only logs / emits, never blocks the send.
    if (vreq->kind() == VisionOutputRequest::Result) {
        runKinematicCheck(vreq->positions());
    }

    QByteArray payload = vreq->buildPayload();

    qint64 written = m_mainSocket->write(payload);
    if (written != payload.size()) {
        LOG_DEV_ERR << "VisionTcpipDeviceBase: short write" << written << "/" << payload.size();
        return false;
    }
    m_mainSocket->flush();

    ++m_diagnostics.resultPayloadsSent;
    emit resultSent(payload);
    return true;
}

/// Internal helpers for the robot-kinematics reachability/collision check, not
/// exposed outside this translation unit.
namespace {

/// Preset name for the only built-in C++ robot preset shipped by the
/// RobotKinematics component; the picking TCP is appended to it as the default
/// tool from the device config (see buildRobotConfig()).
constexpr char kNachiMz04dPreset[] = "Nachi MZ04D";
constexpr char kPickingToolId[]    = "picking_tcp";  ///< Tool id used for the picking-TCP tool appended in buildRobotConfig().

/// Path (relative to the deployed binary) of the Nachi MZ04D *simplified* (voxel)
/// mesh-collision profile, copied next to the binary by the robotkinematics.pri
/// post-link step. The simplified profile is used (same as the widget tester)
/// because it is fast enough for the per-cycle result send path. See
/// docs/backlog/later_todo_list.md #27.
constexpr char kSimplifiedMeshProfileDeployedRel[] =
    "robot_assets/Nachi/MZ04/nachi_mz04d_mesh_collision_simplified.json";
/// Source-tree fallback path for the same profile, used when the deployed copy
/// is not found (dev runs).
constexpr char kSimplifiedMeshProfileSourceRel[] =
    "components/RobotKinematics/presets/Nachi/MZ04/nachi_mz04d_mesh_collision_simplified.json";

/// Resolves a repo-relative asset path by walking up from the application
/// directory and the current working directory (source-tree fallback for dev
/// runs), returning the first candidate that exists on disk.
/// @param relative repo-relative path of the asset to locate
/// @return absolute path to the asset, or an empty string if not found
QString resolveAssetPath(const QString &relative) {
    const QFileInfo direct(relative);
    if (direct.exists()) return direct.absoluteFilePath();

    QStringList roots;
    roots << QDir::currentPath() << QCoreApplication::applicationDirPath();
    QDir walker(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8 && walker.cdUp(); ++i)
        roots << walker.absolutePath();
    roots.removeDuplicates();

    for (const QString &root : roots) {
        const QFileInfo candidate(QDir(root).filePath(relative));
        if (candidate.exists()) return candidate.absoluteFilePath();
    }
    return {};
}

/// Builds the selected robot config (currently only the Nachi MZ04D preset is
/// recognised) with the picking TCP appended as the default tool, using the TCP
/// offset/orientation from `kc`.
/// @param kc kinematic-check config supplying the preset name and TCP pose
/// @param out output robot config; populated on success
/// @return false when `kc.presetName` does not match a known preset (`out` untouched)
bool buildRobotConfig(const RobotKinematicCheckConfig &kc,
                      RobotKinematics::SerialRobotConfig &out) {
    if (kc.presetName != QLatin1String(kNachiMz04dPreset))
        return false;

    out = RobotKinematics::Presets::nachiMZ04D();
    RobotKinematics::Tool tool;
    tool.id          = kPickingToolId;
    tool.name        = kc.tcpName.toStdString();
    tool.flangeToTcp = RobotKinematics::Pose::fromXYZRPY_mm_deg(
        kc.tcpX, kc.tcpY, kc.tcpZ, kc.tcpRoll, kc.tcpPitch, kc.tcpYaw);
    out.tools.push_back(tool);
    out.defaultToolId = kPickingToolId;
    return true;
}

/// Lazily loaded, shared Nachi MZ04D simplified mesh-collision profile (voxel
/// meshes), resolved via the deployed path then the source-tree fallback. The
/// profile is fixed for the preset, so it is loaded once (function-local static)
/// and reused; logs and returns nullptr if the file cannot be found or parsed.
/// @return pointer to the cached profile, or nullptr if loading failed
const RobotKinematics::MeshCollisionProfile *nachiSimplifiedMeshProfile() {
    static const std::optional<RobotKinematics::MeshCollisionProfile> profile =
        []() -> std::optional<RobotKinematics::MeshCollisionProfile> {
            // Prefer the copy deployed next to the binary; fall back to the source tree.
            QString path;
            const QFileInfo deployed(QDir(QCoreApplication::applicationDirPath())
                                         .filePath(QString::fromLatin1(kSimplifiedMeshProfileDeployedRel)));
            if (deployed.exists())
                path = deployed.absoluteFilePath();
            else
                path = resolveAssetPath(QString::fromLatin1(kSimplifiedMeshProfileSourceRel));

            if (path.isEmpty()) {
                LOG_DEV_ERR << "VisionOutput collision check: mesh profile not found"
                            << kSimplifiedMeshProfileDeployedRel << "- collision check skipped";
                return std::nullopt;
            }
            const RobotKinematics::Result<RobotKinematics::MeshCollisionProfile> res =
                RobotKinematics::MeshCollisionProfileJsonLoader::loadFile(path.toStdString());
            if (!res.ok()) {
                LOG_DEV_ERR << "VisionOutput collision check: failed to load mesh profile"
                            << path << "-" << QString::fromStdString(res.message);
                return std::nullopt;
            }
            return res.value;
        }();
    return profile ? &(*profile) : nullptr;
}

} // namespace

/// Advisory robot-kinematics reachability (and, if enabled, self-collision)
/// check over the outgoing pick poses: solves IK per pose against the
/// configured preset/tool, logs unreachable poses and any detected
/// self-collisions, and emits kinematicCheckResult(). No-op if the check is
/// disabled in config, or if the configured preset is not recognised. Never
/// blocks or alters the sent payload — this runs before the write in
/// pushRequest() purely for logging/telemetry.
/// @param positions pick poses (x, y, z, r) to test for reachability
void VisionTcpipDeviceBase::runKinematicCheck(const QVector<VisionOutputPosition> &positions) {
    const RobotKinematicCheckConfig kc = kinematicCheckConfig();
    if (!kc.enabled) return;

    RobotKinematics::SerialRobotConfig robotCfg;
    if (!buildRobotConfig(kc, robotCfg)) {
        LOG_DEV_ERR << "VisionOutput kinematic check: unknown preset"
                    << kc.presetName << "- check skipped";
        return;
    }

    const RobotKinematics::SerialRobotKinematics solver(robotCfg);

    // Mesh self-collision is optional and only attempted when both the operator
    // enabled it and the Nachi mesh profile is available.
    const RobotKinematics::MeshCollisionProfile *meshProfile =
        kc.collisionCheckEnabled ? nachiSimplifiedMeshProfile() : nullptr;

    int reachable = 0;
    int colliding = 0;
    for (const VisionOutputPosition &p : positions) {
        RobotKinematics::IKRequest req;
        req.targetPose = RobotKinematics::Pose::fromXYZRPY_mm_deg(p.x, p.y, p.z, 180.0, 0.0, p.r);
        req.tool = RobotKinematics::ToolId{kPickingToolId};

        const RobotKinematics::IKResult ik = solver.solve(req);
        if (!ik.ok()) {
            LOG_USER_WARN << "VisionOutput kinematic check: pose not reachable"
                          << "(" << p.toString() << ")";
            continue;
        }
        ++reachable;

        if (!meshProfile) continue;

        RobotKinematics::MeshCollisionCheckRequest collisionReq;
        collisionReq.joints = ik.best().joints;
        const RobotKinematics::CollisionCheckResult collision =
            RobotKinematics::CollisionBackends::checkMesh(robotCfg, *meshProfile, collisionReq);
        if (!collision.ok()) {
            LOG_DEV_ERR << "VisionOutput collision check: backend error -"
                        << QString::fromStdString(collision.message);
            meshProfile = nullptr;   // stop retrying for this batch
        } else if (collision.hasCollision) {
            ++colliding;
            LOG_USER_WARN << "VisionOutput collision check: self-collision at pose"
                          << "(" << p.toString() << ")";
        }
    }

    if (kc.collisionCheckEnabled) {
        LOG_DEV_DEBUG << "VisionOutput kinematic check:" << reachable << "/" << positions.size()
                      << "reachable," << colliding << "in self-collision";
    }
    emit kinematicCheckResult(positions.size(), reachable);
}

// =====================================================================
// Socket attach / detach (shared by both transports)
// =====================================================================
/// Adopts `sock` as the main-channel socket: clears the RX buffer, wires
/// disconnected/readyRead handlers, syncs runtime state, and emits
/// mainClientStateChanged(true). No-op if `sock` is null.
/// @param sock connected socket handed off by the concrete transport
void VisionTcpipDeviceBase::attachMainSocket(QTcpSocket *sock) {
    if (!sock) return;
    m_mainSocket = sock;
    m_mainRxBuffer.clear();

    connect(m_mainSocket, &QTcpSocket::disconnected,
            this, &VisionTcpipDeviceBase::onMainSocketDisconnected);
    connect(m_mainSocket, &QTcpSocket::readyRead,
            this, &VisionTcpipDeviceBase::onMainSocketReadyRead);

    LOG_DEV_INFO << "VisionTcpip main link up from"
                 << m_mainSocket->peerAddress().toString();
    syncRuntimeState();
    emit mainClientStateChanged(true);
}

/// Adopts `sock` as the heartbeat-channel socket: clears the RX buffer, resets
/// heartbeat state, wires disconnected/readyRead handlers, then (since the
/// software is always the heartbeat master) immediately sends the first probe
/// and starts the heartbeat timer. No-op if `sock` is null.
/// @param sock connected socket handed off by the concrete transport
void VisionTcpipDeviceBase::attachHeartbeatSocket(QTcpSocket *sock) {
    if (!sock) return;
    m_hbSocket = sock;
    m_hbRxBuffer.clear();
    resetHeartbeatState();

    connect(m_hbSocket, &QTcpSocket::disconnected,
            this, &VisionTcpipDeviceBase::onHeartbeatSocketDisconnected);
    connect(m_hbSocket, &QTcpSocket::readyRead,
            this, &VisionTcpipDeviceBase::onHeartbeatSocketReadyRead);

    LOG_DEV_INFO << "VisionTcpip heartbeat link up from"
                 << m_hbSocket->peerAddress().toString();

    // Software is the heartbeat master: kick the first probe and start ticking.
    sendHeartbeatProbe();
    startHeartbeatTimer();
    syncRuntimeState();
}

/// Disconnects signals from the main socket, aborts it, schedules it for
/// deletion (deleteLater()), clears m_mainSocket and the RX buffer, syncs
/// runtime state, and emits mainClientStateChanged(false). No-op if no main
/// socket is attached.
void VisionTcpipDeviceBase::detachMainSocket() {
    if (!m_mainSocket) return;
    m_mainSocket->disconnect(this);
    m_mainSocket->abort();
    m_mainSocket->deleteLater();
    m_mainSocket = nullptr;
    m_mainRxBuffer.clear();
    syncRuntimeState();
    emit mainClientStateChanged(false);
}

/// Stops the heartbeat timer, then (if attached) disconnects signals from the
/// heartbeat socket, aborts it, schedules it for deletion, and clears
/// m_hbSocket. Always clears the RX buffer and resets heartbeat state.
void VisionTcpipDeviceBase::detachHeartbeatSocket() {
    stopHeartbeatTimer();
    if (m_hbSocket) {
        m_hbSocket->disconnect(this);
        m_hbSocket->abort();
        m_hbSocket->deleteLater();
        m_hbSocket = nullptr;
    }
    m_hbRxBuffer.clear();
    resetHeartbeatState();
}

// =====================================================================
// Main channel framing (';'-terminated)
// =====================================================================
/// Reads all available bytes from the main socket into m_mainRxBuffer, then
/// extracts and dispatches (via handleMainPayload()) every complete,
/// terminator-delimited message currently buffered, leaving any partial
/// trailing message for the next call.
void VisionTcpipDeviceBase::onMainSocketReadyRead() {
    if (!m_mainSocket) return;
    m_mainRxBuffer.append(m_mainSocket->readAll());

    int idx = m_mainRxBuffer.indexOf(VISION_OUTPUT_MAIN_TERMINATOR);
    while (idx >= 0) {
        QByteArray msg = m_mainRxBuffer.left(idx + 1);
        m_mainRxBuffer.remove(0, idx + 1);
        handleMainPayload(msg);
        idx = m_mainRxBuffer.indexOf(VISION_OUTPUT_MAIN_TERMINATOR);
    }
}

/// Handles the main socket's disconnected signal: detaches the socket and, if
/// the device is still active, notifies the subclass via onLinkLost().
void VisionTcpipDeviceBase::onMainSocketDisconnected() {
    if (!m_mainSocket) return;
    LOG_DEV_INFO << "VisionTcpip main link disconnected" << name();
    detachMainSocket();
    if (m_active) {
        onLinkLost();
    }
}

/// Records receipt of a complete main-channel message in diagnostics and
/// forwards it to listeners via mainRequestReceived(); does not parse the payload.
void VisionTcpipDeviceBase::handleMainPayload(const QByteArray &payload) {
    LOG_DEV_DEBUG << "VisionTcpip main RX:" << payload;
    ++m_diagnostics.mainPayloadsReceived;
    emit mainRequestReceived(payload);
}

// =====================================================================
// Heartbeat channel ('.'-terminated)
// =====================================================================
/// Reads all available bytes from the heartbeat socket into m_hbRxBuffer, then
/// extracts and dispatches (via handleHeartbeatPayload()) every complete,
/// terminator-delimited message currently buffered, leaving any partial
/// trailing message for the next call.
void VisionTcpipDeviceBase::onHeartbeatSocketReadyRead() {
    if (!m_hbSocket) return;
    m_hbRxBuffer.append(m_hbSocket->readAll());

    int idx = m_hbRxBuffer.indexOf(VISION_OUTPUT_HB_TERMINATOR);
    while (idx >= 0) {
        QByteArray msg = m_hbRxBuffer.left(idx + 1);
        m_hbRxBuffer.remove(0, idx + 1);
        handleHeartbeatPayload(msg);
        idx = m_hbRxBuffer.indexOf(VISION_OUTPUT_HB_TERMINATOR);
    }
}

/// Handles the heartbeat socket's disconnected signal: detaches the socket,
/// syncs runtime state, and — if the device is still active — notifies the
/// subclass via onLinkLost().
void VisionTcpipDeviceBase::onHeartbeatSocketDisconnected() {
    if (!m_hbSocket) return;
    LOG_DEV_INFO << "VisionTcpip heartbeat link disconnected" << name();
    detachHeartbeatSocket();
    syncRuntimeState();
    if (m_active) {
        onLinkLost();
    }
}

/// Parses one heartbeat reply of the form "ack,{msg_count}." and validates it
/// against the expected sequence: declares a lost connection (via
/// declareLostConnection()) on malformed format, an out-of-range count, or a
/// count mismatch with m_expectedAckCount. On a valid match, records the ack,
/// clears the awaiting-reply flag, restarts the reply timer, and advances
/// m_expectedAckCount (wrapping at VISION_OUTPUT_MSG_COUNT_LIMIT).
/// @param payload raw heartbeat message, including its terminating '.'
void VisionTcpipDeviceBase::handleHeartbeatPayload(const QByteArray &payload) {
    // Valid form: "ack,{msg_count}."
    if (!payload.startsWith(VISION_OUTPUT_HB_ACK_PREFIX) || !payload.endsWith('.')) {
        declareLostConnection(QString("Invalid heartbeat reply format: %1")
                                  .arg(QString::fromUtf8(payload)));
        return;
    }

    QByteArray middle = payload.mid(qstrlen(VISION_OUTPUT_HB_ACK_PREFIX),
                                    payload.size() - qstrlen(VISION_OUTPUT_HB_ACK_PREFIX) - 1);
    bool ok = false;
    int parsed = middle.toInt(&ok);
    if (!ok || parsed < 0 || parsed >= VISION_OUTPUT_MSG_COUNT_LIMIT) {
        declareLostConnection(QString("Invalid heartbeat msg_count: %1")
                                  .arg(QString::fromUtf8(middle)));
        return;
    }

    if (static_cast<quint16>(parsed) != m_expectedAckCount) {
        declareLostConnection(QString("Heartbeat msg_count mismatch, expected %1 got %2")
                                  .arg(m_expectedAckCount).arg(parsed));
        return;
    }

    m_lastAckCount     = static_cast<quint16>(parsed);
    m_hbAwaitingReply  = false;
    m_hbLastReplyTimer.restart();

    // Advance the counter, wrap at 2^16.
    m_expectedAckCount = static_cast<quint16>((m_expectedAckCount + 1) % VISION_OUTPUT_MSG_COUNT_LIMIT);
    syncRuntimeState();
}

/// Periodic heartbeat-timer callback: if a probe reply has been awaited longer
/// than cfgHeartbeatTimeoutMs() since the last valid reply, declares a lost
/// connection (via declareLostConnection()); otherwise sends the next probe.
/// No-op if the heartbeat socket is not connected.
void VisionTcpipDeviceBase::onHeartbeatTick() {
    if (!m_hbSocket || m_hbSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // Silent for too long since the last valid reply -> lost connection.
    if (m_hbAwaitingReply && m_hbLastReplyTimer.isValid()
        && m_hbLastReplyTimer.elapsed() > cfgHeartbeatTimeoutMs()) {
        ++m_diagnostics.heartbeatTimeoutCount;
        declareLostConnection(QString("Heartbeat reply timeout (%1 ms)")
                                  .arg(cfgHeartbeatTimeoutMs()));
        return;
    }

    sendHeartbeatProbe();
}

/// Best-effort graceful goodbye on the heartbeat channel: writes the
/// disconnect-notice message and blocks briefly for it to flush. No-op if the
/// heartbeat socket is not connected, so the caller can always follow with
/// detachHeartbeatSocket()/stopTransport() unconditionally.
void VisionTcpipDeviceBase::sendDisconnectNotice() {
    // Bounded wait so the notice clears the socket buffer before the imminent
    // detachHeartbeatSocket()->abort(), which would otherwise discard it.
    constexpr int kDisconnectFlushMs = 150;

    if (!m_hbSocket || m_hbSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray notice(VISION_OUTPUT_HB_DISCONNECT);
    if (m_hbSocket->write(notice) != notice.size()) {
        LOG_DEV_ERR << "VisionTcpip: short write on disconnect notice" << name();
        return;
    }
    m_hbSocket->flush();
    m_hbSocket->waitForBytesWritten(kDisconnectFlushMs);
    LOG_DEV_INFO << "VisionTcpip sent disconnect notice" << name();
}

/// Writes the heartbeat probe message to the heartbeat socket and marks a
/// reply as awaited. Starts (but does not restart) m_hbLastReplyTimer if it is
/// not already running, since it measures time since the last valid reply, not
/// time since the last probe. No-op if no heartbeat socket is attached.
void VisionTcpipDeviceBase::sendHeartbeatProbe() {
    if (!m_hbSocket) return;

    QByteArray probe(VISION_OUTPUT_HB_MESSAGE);
    m_hbSocket->write(probe);
    m_hbSocket->flush();

    m_hbAwaitingReply = true;
    // The timer measures "time since the last valid reply" (or since the first
    // probe when no reply has arrived yet). It must NOT be restarted on every
    // probe, otherwise elapsed() never exceeds the timeout.
    if (!m_hbLastReplyTimer.isValid()) {
        m_hbLastReplyTimer.start();
    }
    syncRuntimeState();
}

/// Lazily creates the (precise) heartbeat QTimer wired to onHeartbeatTick(),
/// then (re)applies cfgHeartbeatIntervalMs() and starts it.
void VisionTcpipDeviceBase::startHeartbeatTimer() {
    if (!m_hbTimer) {
        m_hbTimer = new QTimer(this);
        m_hbTimer->setTimerType(Qt::PreciseTimer);
        connect(m_hbTimer, &QTimer::timeout,
                this, &VisionTcpipDeviceBase::onHeartbeatTick);
    }
    m_hbTimer->setInterval(cfgHeartbeatIntervalMs());
    m_hbTimer->start();
}

/// Stops the heartbeat timer if it exists and is currently running.
void VisionTcpipDeviceBase::stopHeartbeatTimer() {
    if (m_hbTimer && m_hbTimer->isActive()) {
        m_hbTimer->stop();
    }
}

// =====================================================================
// Lost-connection / state bookkeeping
// =====================================================================
/// Records `reason` in diagnostics (lastError, lastHeartbeatLossReason, and
/// incremented lostConnectionCount), emits heartbeatLost(reason), detaches both
/// sockets, sets ConnectStatus::LostConnected, and — if the device is still
/// active — notifies the subclass via onLinkLost().
/// @param reason human-readable description of why the connection was declared lost
void VisionTcpipDeviceBase::declareLostConnection(const QString &reason) {
    LOG_DEV_ERR << "VisionTcpip lost connection:" << reason;
    m_diagnostics.lastError = reason;
    m_diagnostics.lastHeartbeatLossReason = reason;
    ++m_diagnostics.lostConnectionCount;
    emit heartbeatLost(reason);

    detachHeartbeatSocket();
    detachMainSocket();

    setConnectionStatus(ConnectStatus::LostConnected, reason);
    if (m_active) {
        onLinkLost();
    }
}

/// Resets the ack-sequence counters and awaiting-reply flag to their initial
/// state, invalidates the reply timer, and syncs runtime state.
void VisionTcpipDeviceBase::resetHeartbeatState() {
    m_expectedAckCount = 0;
    m_lastAckCount     = 0;
    m_hbAwaitingReply  = false;
    m_hbLastReplyTimer.invalidate();
    syncRuntimeState();
}

/// Recomputes m_runtimeState (main/heartbeat connected flags, awaiting-reply
/// flag, and ack counters) from the current socket/heartbeat state.
void VisionTcpipDeviceBase::syncRuntimeState() {
    m_runtimeState.mainClientConnected =
        m_mainSocket && m_mainSocket->state() == QAbstractSocket::ConnectedState;
    m_runtimeState.heartbeatClientConnected =
        m_hbSocket && m_hbSocket->state() == QAbstractSocket::ConnectedState;
    m_runtimeState.awaitingHeartbeatReply = m_hbAwaitingReply;
    m_runtimeState.expectedAckCount = m_expectedAckCount;
    m_runtimeState.lastAckCount = m_lastAckCount;
}

} // namespace vc::device
