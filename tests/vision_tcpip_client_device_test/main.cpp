#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest/QtTest>

#include <functional>

#include "device/output_device/vision_output_device.h"
#include "device/output_device/vision_output_request.h"
#include "device/output_device/vision_tcpip_client_config.h"
#include "device/output_device/vision_tcpip_client_device.h"

using namespace vc::device;

namespace {

constexpr int  kMainPort   = 25201;
constexpr int  kHbPort     = 25202;
constexpr int  kHbInterval = 150;
constexpr int  kHbTimeout  = 400;
constexpr char kHost[]     = "127.0.0.1";

VisionTcpipClientDeviceCfg makeCfg(int reconnectMs = 200) {
    VisionTcpipClientDeviceCfg cfg;
    cfg.m_serverAddress       = kHost;
    cfg.m_mainPort            = kMainPort;
    cfg.m_heartbeatPort       = kHbPort;
    cfg.m_heartbeatIntervalMs = kHbInterval;
    cfg.m_heartbeatTimeoutMs  = kHbTimeout;
    cfg.m_reconnectIntervalMs = reconnectMs;
    return cfg;
}

bool waitFor(std::function<bool()> pred, int ms = 2000) {
    QElapsedTimer t;
    t.start();
    while (!pred() && t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(5);
    }
    return pred();
}

// Remote peer the client device dials into. Listens on the main and/or
// heartbeat ports. The heartbeat side optionally auto-replies "ack,{count}."
// to each "connection_check." probe (the software is the heartbeat master).
class RemotePeer : public QObject {
    Q_OBJECT
public:
    explicit RemotePeer(bool autoAck, QObject *parent = nullptr)
        : QObject(parent), m_autoAck(autoAck) {}

    bool start(bool enableMain = true, bool enableHb = true) {
        if (enableMain) {
            if (!m_mainSrv.listen(QHostAddress(kHost), kMainPort)) return false;
            connect(&m_mainSrv, &QTcpServer::newConnection, this, [this]() {
                m_mainSock = m_mainSrv.nextPendingConnection();
                connect(m_mainSock, &QTcpSocket::readyRead, this, [this]() {
                    m_mainRx.append(m_mainSock->readAll());
                });
            });
        }
        if (enableHb) {
            if (!m_hbSrv.listen(QHostAddress(kHost), kHbPort)) return false;
            connect(&m_hbSrv, &QTcpServer::newConnection, this, [this]() {
                m_hbSock = m_hbSrv.nextPendingConnection();
                connect(m_hbSock, &QTcpSocket::readyRead, this, [this]() {
                    m_hbRx.append(m_hbSock->readAll());
                    if (!m_autoAck) return;
                    int idx = m_hbRx.indexOf('.');
                    while (idx >= 0) {
                        const QByteArray msg = m_hbRx.left(idx + 1);
                        m_hbRx.remove(0, idx + 1);
                        if (msg == QByteArray("connection_check.")) {
                            ++m_probeCount;
                            const QByteArray ack =
                                QByteArray("ack,") + QByteArray::number(m_ackCount) + ".";
                            m_hbSock->write(ack);
                            m_hbSock->flush();
                            m_ackCount = (m_ackCount + 1) % (1 << 16);
                        } else if (msg == QByteArray("disconnect.")) {
                            m_gotDisconnectNotice = true;
                        }
                        idx = m_hbRx.indexOf('.');
                    }
                });
            });
        }
        return true;
    }

    void dropMain() {
        if (m_mainSock) {
            m_mainSock->abort();
            m_mainSock->deleteLater();
            m_mainSock = nullptr;
        }
    }

    bool mainConnected() const {
        return m_mainSock && m_mainSock->state() == QAbstractSocket::ConnectedState;
    }
    bool hbConnected() const {
        return m_hbSock && m_hbSock->state() == QAbstractSocket::ConnectedState;
    }
    int  probeCount()   const { return m_probeCount; }
    bool gotDisconnectNotice() const { return m_gotDisconnectNotice; }
    QByteArray mainRx() const { return m_mainRx; }

private:
    bool m_autoAck;
    QTcpServer m_mainSrv;
    QTcpServer m_hbSrv;
    QTcpSocket *m_mainSock{nullptr};
    QTcpSocket *m_hbSock{nullptr};
    QByteArray m_mainRx;
    QByteArray m_hbRx;
    int m_probeCount{0};
    bool m_gotDisconnectNotice{false};
    quint16 m_ackCount{0};
};

} // namespace

class VisionTcpipClientDeviceTest : public QObject {
    Q_OBJECT

private slots:

    void test_config_clone_and_json() {
        VisionTcpipClientDeviceCfg cfg = makeCfg(1234);
        QJsonObject obj = cfg.toJson();

        VisionTcpipClientDeviceCfg restored;
        QVERIFY(restored.fromJson(obj));
        QCOMPARE(restored.m_serverAddress,       cfg.m_serverAddress);
        QCOMPARE(restored.m_mainPort,            cfg.m_mainPort);
        QCOMPARE(restored.m_heartbeatPort,       cfg.m_heartbeatPort);
        QCOMPARE(restored.m_heartbeatIntervalMs, cfg.m_heartbeatIntervalMs);
        QCOMPARE(restored.m_heartbeatTimeoutMs,  cfg.m_heartbeatTimeoutMs);
        QCOMPARE(restored.m_reconnectIntervalMs, cfg.m_reconnectIntervalMs);

        std::unique_ptr<IDeviceCfg> cloned(cfg.clone());
        QVERIFY(cloned != nullptr);
        QCOMPARE(cloned->deviceType(), DeviceType::VisionOutput);
        QCOMPARE(static_cast<VisionOutputDeviceCfg*>(cloned.get())->visionOutputType(),
                 VisionOutputType::VisionTcpipClient);
    }

    void test_request_payload_format() {
        VisionOutputRequest req(QVector<VisionOutputPosition>{{1.0, 2.0, 3.0, 4.0}});
        QCOMPARE(req.buildPayload(), QByteArray("1,00001.00,00002.00,00003.00,00004.00;"));
    }

    // deviceConnect immediately reports Connecting (dialing), not Connected.
    void test_connect_starts_in_connecting_state() {
        VisionTcpipClientDevice device(QStringLiteral("vc_state"),
                                       QStringLiteral("Vision Client State"));
        VisionTcpipClientDeviceCfg cfg = makeCfg();
        device.setVisionTcpipClientConfig(cfg);

        // No remote peer at all -> the dial cannot complete.
        QVERIFY(device.deviceConnect());
        QCOMPARE(device.connectStatus(), ConnectStatus::Connecting);
        QVERIFY(!device.isMainClientConnected());
        QVERIFY(!device.isHeartbeatClientConnected());

        device.deviceDisconnect();
        QCOMPARE(device.connectStatus(), ConnectStatus::Disconnected);
    }

    // Only the heartbeat link comes up -> stays Connecting, never Connected.
    void test_stays_connecting_until_both_links_up() {
        RemotePeer peer(/*autoAck=*/true);
        QVERIFY(peer.start(/*enableMain=*/false, /*enableHb=*/true));

        VisionTcpipClientDevice device(QStringLiteral("vc_one"),
                                       QStringLiteral("Vision Client One"));
        VisionTcpipClientDeviceCfg cfg = makeCfg();
        device.setVisionTcpipClientConfig(cfg);
        QVERIFY(device.deviceConnect());

        QVERIFY(waitFor([&]() { return device.isHeartbeatClientConnected(); }));

        // With the main port absent, the device must NOT report Connected.
        const bool reachedConnected =
            waitFor([&]() { return device.connectStatus() == ConnectStatus::Connected; }, 600);
        QVERIFY(!reachedConnected);
        QCOMPARE(device.connectStatus(), ConnectStatus::Connecting);
        QVERIFY(!device.isMainClientConnected());

        device.deviceDisconnect();
    }

    // Both links up -> Connected, heartbeat acked, result pushed to the peer.
    void test_connected_heartbeat_and_result() {
        RemotePeer peer(/*autoAck=*/true);
        QVERIFY(peer.start());

        VisionTcpipClientDevice device(QStringLiteral("vc_ok"),
                                       QStringLiteral("Vision Client OK"));
        VisionTcpipClientDeviceCfg cfg = makeCfg();
        device.setVisionTcpipClientConfig(cfg);
        QVERIFY(device.deviceConnect());

        QVERIFY(waitFor([&]() { return device.connectStatus() == ConnectStatus::Connected; }));
        QVERIFY(device.isMainClientConnected());
        QVERIFY(device.isHeartbeatClientConnected());
        QVERIFY(peer.mainConnected() && peer.hbConnected());

        QVERIFY(waitFor([&]() { return peer.probeCount() >= 1
                                    && device.expectedAckCount() >= 1; }));

        VisionOutputRequest req(QVector<VisionOutputPosition>{{1.0, 2.0, 3.0, 4.0}});
        QVERIFY(device.pushRequest(&req));
        QVERIFY(waitFor([&]() { return peer.mainRx().contains(';'); }));
        QCOMPARE(peer.mainRx(), QByteArray("1,00001.00,00002.00,00003.00,00004.00;"));

        QVERIFY(device.deviceDisconnect());
        QCOMPARE(device.connectStatus(), ConnectStatus::Disconnected);
    }

    // A graceful deviceDisconnect() sends "disconnect." to the remote heartbeat
    // port before the dialed links are torn down.
    void test_disconnect_notice_on_graceful_close() {
        RemotePeer peer(/*autoAck=*/true);
        QVERIFY(peer.start());

        VisionTcpipClientDevice device(QStringLiteral("vc_dc"),
                                       QStringLiteral("Vision Client DC"));
        VisionTcpipClientDeviceCfg cfg = makeCfg();
        device.setVisionTcpipClientConfig(cfg);
        QVERIFY(device.deviceConnect());

        QVERIFY(waitFor([&]() { return device.connectStatus() == ConnectStatus::Connected
                                    && peer.probeCount() >= 1; }));
        QVERIFY(!peer.gotDisconnectNotice());

        QVERIFY(device.deviceDisconnect());
        QVERIFY(waitFor([&]() { return peer.gotDisconnectNotice(); }));
        QCOMPARE(device.connectStatus(), ConnectStatus::Disconnected);
    }

    // No ack from the remote -> heartbeat times out -> LostConnected.
    void test_heartbeat_timeout_declares_lost() {
        RemotePeer peer(/*autoAck=*/false);
        QVERIFY(peer.start());

        VisionTcpipClientDevice device(QStringLiteral("vc_to"),
                                       QStringLiteral("Vision Client TO"));
        // Large reconnect delay so it does not thrash within the window.
        VisionTcpipClientDeviceCfg cfg = makeCfg(/*reconnectMs=*/5000);
        device.setVisionTcpipClientConfig(cfg);
        QVERIFY(device.deviceConnect());

        QVERIFY(waitFor([&]() { return device.isHeartbeatClientConnected(); }));
        QVERIFY(waitFor([&]() {
            return device.connectStatus() == ConnectStatus::LostConnected;
        }, 3000));
        QVERIFY(device.diagnostics().lostConnectionCount >= 1);

        device.deviceDisconnect();
    }

    // A dropped main link triggers reconnect and returns to Connected.
    void test_reconnect_after_link_drop() {
        RemotePeer peer(/*autoAck=*/true);
        QVERIFY(peer.start());

        VisionTcpipClientDevice device(QStringLiteral("vc_rc"),
                                       QStringLiteral("Vision Client RC"));
        VisionTcpipClientDeviceCfg cfg = makeCfg(/*reconnectMs=*/200);
        device.setVisionTcpipClientConfig(cfg);
        QVERIFY(device.deviceConnect());

        QVERIFY(waitFor([&]() { return device.connectStatus() == ConnectStatus::Connected; }));

        peer.dropMain();
        QVERIFY(waitFor([&]() { return !device.isMainClientConnected(); }));

        // The client redials the main link and recovers to Connected.
        QVERIFY(waitFor([&]() { return device.connectStatus() == ConnectStatus::Connected
                                    && device.isMainClientConnected(); }, 4000));

        device.deviceDisconnect();
    }
};

QTEST_MAIN(VisionTcpipClientDeviceTest)
#include "main.moc"
