#ifndef VISION_TCPIP_PROTOCOL_H
#define VISION_TCPIP_PROTOCOL_H

/**
 * @file vision_tcpip_protocol.h
 * @brief Shared wire-format constants and Q_GADGET state/diagnostics structs for the
 *        vision-output TCP/IP protocol (server + client transports).
 *
 *        Both transports speak the same two-channel protocol:
 *          - Main channel: "{detected},{x,y,z,r},...;" (delimiter ';'; each axis "%08.2f")
 *          - Heartbeat:    probe "connection_check.", reply "ack,{count}." (delimiter '.')
 *
 *        The software is always the heartbeat master. On intentional disconnect a one-shot
 *        "disconnect." notice is written on the heartbeat channel before teardown.
 */

#include <QtGlobal>
#include <QMetaType>
#include <QString>

namespace vc::device {

/// Heartbeat probe message sent by the software (always the heartbeat master).
#define VISION_OUTPUT_HB_MESSAGE        "connection_check."
/// Prefix of a valid heartbeat ack reply, of the form "ack,{count}.".
#define VISION_OUTPUT_HB_ACK_PREFIX     "ack,"
/// One-shot notice written to the heartbeat channel just before an intentional disconnect.
#define VISION_OUTPUT_HB_DISCONNECT     "disconnect."
/// Delimiter terminating each heartbeat-channel message.
#define VISION_OUTPUT_HB_TERMINATOR     '.'
/// Delimiter terminating each main-channel message.
#define VISION_OUTPUT_MAIN_TERMINATOR   ';'
/// Exclusive upper bound the heartbeat msg_count wraps at.
#define VISION_OUTPUT_MSG_COUNT_LIMIT   (1 << 16)

/**
 * @struct VisionTcpipRuntimeState
 * @brief Live link/heartbeat state snapshot exposed to the UI (read-only).
 *        Field names are transport-neutral: "mainClientConnected" /
 *        "heartbeatClientConnected" mean "the main / heartbeat link is up",
 *        whether this device accepted the link (server) or dialled it (client).
 */
struct VisionTcpipRuntimeState {
    Q_GADGET

    Q_PROPERTY(bool mainClientConnected MEMBER mainClientConnected)
    Q_PROPERTY(bool heartbeatClientConnected MEMBER heartbeatClientConnected)
    Q_PROPERTY(bool awaitingHeartbeatReply MEMBER awaitingHeartbeatReply)
    Q_PROPERTY(quint16 expectedAckCount MEMBER expectedAckCount)
    Q_PROPERTY(quint16 lastAckCount MEMBER lastAckCount)

public:
    bool mainClientConnected{false};             ///< True when the main-channel link is up.
    bool heartbeatClientConnected{false};        ///< True when the heartbeat-channel link is up.
    bool awaitingHeartbeatReply{false};          ///< True while a heartbeat probe is outstanding.
    quint16 expectedAckCount{0};                 ///< msg_count expected in the next heartbeat ack.
    quint16 lastAckCount{0};                     ///< msg_count of the last accepted heartbeat ack.
};

/**
 * @struct VisionTcpipDiagnostics
 * @brief Cumulative diagnostic counters and last error string exposed to the UI (read-only).
 */
struct VisionTcpipDiagnostics {
    Q_GADGET

    Q_PROPERTY(QString lastError MEMBER lastError)
    Q_PROPERTY(QString lastHeartbeatLossReason MEMBER lastHeartbeatLossReason)
    Q_PROPERTY(quint64 mainPayloadsReceived MEMBER mainPayloadsReceived)
    Q_PROPERTY(quint64 resultPayloadsSent MEMBER resultPayloadsSent)
    Q_PROPERTY(quint64 heartbeatTimeoutCount MEMBER heartbeatTimeoutCount)
    Q_PROPERTY(quint64 lostConnectionCount MEMBER lostConnectionCount)

public:
    QString lastError;                       ///< Description of the most recent error (any source).
    QString lastHeartbeatLossReason;         ///< Description of the most recent heartbeat-loss reason.
    quint64 mainPayloadsReceived{0};         ///< Count of ';'-terminated main-channel messages received.
    quint64 resultPayloadsSent{0};           ///< Count of result payloads written via pushRequest().
    quint64 heartbeatTimeoutCount{0};        ///< Count of heartbeat reply timeouts observed.
    quint64 lostConnectionCount{0};          ///< Count of times declareLostConnection() has fired.
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::VisionTcpipRuntimeState)
Q_DECLARE_METATYPE(vc::device::VisionTcpipDiagnostics)

#endif // VISION_TCPIP_PROTOCOL_H
