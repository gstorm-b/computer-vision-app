#ifndef VISION_TCPIP_PROTOCOL_H
#define VISION_TCPIP_PROTOCOL_H

#include <QtGlobal>
#include <QMetaType>
#include <QString>

namespace vc::device {

// =====================================================================
// Vision TCP/IP protocol constants (shared by server + client transports)
// =====================================================================
//
// Both transports speak the same two-channel protocol:
//   - Main channel : "{detected},{x,y,z,r},...;"  (delimiter ';';
//                    each axis is fixed-width "%08.2f", e.g. 1.0 -> "00001.00")
//   - Heartbeat    : probe "connection_check.", reply "ack,{count}."
//                    (delimiter '.', count wraps at 2^16)
//
// The software is always the heartbeat master: it sends the probe and
// expects the ack, regardless of which side opened the TCP connection.
//
// Graceful shutdown: on an intentional deviceDisconnect() the software sends
// a one-shot "disconnect." notice on the heartbeat channel just before tearing
// the link down, so the peer can distinguish a planned close from a fault
// (timeout / bad format). It is NOT sent on the lost-connection path.
//
#define VISION_OUTPUT_HB_MESSAGE        "connection_check."
#define VISION_OUTPUT_HB_ACK_PREFIX     "ack,"
#define VISION_OUTPUT_HB_DISCONNECT     "disconnect."
#define VISION_OUTPUT_HB_TERMINATOR     '.'
#define VISION_OUTPUT_MAIN_TERMINATOR   ';'
#define VISION_OUTPUT_MSG_COUNT_LIMIT   (1 << 16)

// =====================================================================
// VisionTcpipRuntimeState — live link/heartbeat state, read-only for UI.
// =====================================================================
//
// Field names are transport-neutral: "mainClientConnected" /
// "heartbeatClientConnected" mean "the main / heartbeat link is up",
// whether this device accepted the link (server) or dialled it (client).
//
struct VisionTcpipRuntimeState {
    Q_GADGET

    Q_PROPERTY(bool mainClientConnected MEMBER mainClientConnected)
    Q_PROPERTY(bool heartbeatClientConnected MEMBER heartbeatClientConnected)
    Q_PROPERTY(bool awaitingHeartbeatReply MEMBER awaitingHeartbeatReply)
    Q_PROPERTY(quint16 expectedAckCount MEMBER expectedAckCount)
    Q_PROPERTY(quint16 lastAckCount MEMBER lastAckCount)

public:
    bool mainClientConnected{false};
    bool heartbeatClientConnected{false};
    bool awaitingHeartbeatReply{false};
    quint16 expectedAckCount{0};
    quint16 lastAckCount{0};
};

// =====================================================================
// VisionTcpipDiagnostics — cumulative counters + last error, read-only.
// =====================================================================
struct VisionTcpipDiagnostics {
    Q_GADGET

    Q_PROPERTY(QString lastError MEMBER lastError)
    Q_PROPERTY(QString lastHeartbeatLossReason MEMBER lastHeartbeatLossReason)
    Q_PROPERTY(quint64 mainPayloadsReceived MEMBER mainPayloadsReceived)
    Q_PROPERTY(quint64 resultPayloadsSent MEMBER resultPayloadsSent)
    Q_PROPERTY(quint64 heartbeatTimeoutCount MEMBER heartbeatTimeoutCount)
    Q_PROPERTY(quint64 lostConnectionCount MEMBER lostConnectionCount)

public:
    QString lastError;
    QString lastHeartbeatLossReason;
    quint64 mainPayloadsReceived{0};
    quint64 resultPayloadsSent{0};
    quint64 heartbeatTimeoutCount{0};
    quint64 lostConnectionCount{0};
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::VisionTcpipRuntimeState)
Q_DECLARE_METATYPE(vc::device::VisionTcpipDiagnostics)

#endif // VISION_TCPIP_PROTOCOL_H
