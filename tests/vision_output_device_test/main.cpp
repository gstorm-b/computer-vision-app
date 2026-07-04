#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QtTest/QtTest>

#include <functional>

#include "device/output_device/vision_output_device.h"
#include "device/output_device/vision_output_request.h"
#include "device/output_device/vision_tcpip_config.h"
#include "device/output_device/vision_tcpip_device.h"

using namespace vc::device;

namespace {

constexpr int  kMainPort   = 25101;
constexpr int  kHbPort     = 25102;
constexpr int  kHbInterval = 150;
constexpr int  kHbTimeout  = 400;
constexpr char kListen[]   = "127.0.0.1";

VisionTcpipDeviceCfg makeCfg() {
    VisionTcpipDeviceCfg cfg;
    cfg.m_listenAddress       = kListen;
    cfg.m_mainPort            = kMainPort;
    cfg.m_heartbeatPort       = kHbPort;
    cfg.m_heartbeatIntervalMs = kHbInterval;
    cfg.m_heartbeatTimeoutMs  = kHbTimeout;
    return cfg;
}

// Pump the event loop until `pred` holds or `ms` elapses.
bool waitFor(std::function<bool()> pred, int ms = 2000) {
    QElapsedTimer t;
    t.start();
    while (!pred() && t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(5);
    }
    return pred();
}

// External client that connects INTO the VisionTcpipDevice server: one socket
// on the main port, one on the heartbeat port. Optionally auto-replies
// "ack,{count}." to each "connection_check." probe (the server is the
// heartbeat master).
class ClientPeer : public QObject {
    Q_OBJECT
public:
    explicit ClientPeer(bool autoAck, QObject *parent = nullptr)
        : QObject(parent), m_autoAck(autoAck) {}

    void connectMain() {
        m_mainSock = new QTcpSocket(this);
        connect(m_mainSock, &QTcpSocket::readyRead, this, [this]() {
            m_mainRx.append(m_mainSock->readAll());
        });
        m_mainSock->connectToHost(kListen, kMainPort);
    }

    void connectHeartbeat() {
        m_hbSock = new QTcpSocket(this);
        connect(m_hbSock, &QTcpSocket::readyRead, this, [this]() {
            m_hbRx.append(m_hbSock->readAll());
            if (!m_autoAck) return;
            int idx = m_hbRx.indexOf('.');
            while (idx >= 0) {
                const QByteArray msg = m_hbRx.left(idx + 1);
                m_hbRx.remove(0, idx + 1);
                if (msg == QByteArray("connection_check.")) {
                    ++m_probesReceived;
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
        m_hbSock->connectToHost(kListen, kHbPort);
    }

    void sendMain(const QByteArray &data) {
        if (!m_mainSock) return;
        m_mainSock->write(data);
        m_mainSock->flush();
    }

    bool mainConnected() const {
        return m_mainSock && m_mainSock->state() == QAbstractSocket::ConnectedState;
    }
    int probesReceived() const { return m_probesReceived; }
    bool gotDisconnectNotice() const { return m_gotDisconnectNotice; }
    QByteArray mainRx()  const { return m_mainRx; }

private:
    bool m_autoAck;
    QTcpSocket *m_mainSock{nullptr};
    QTcpSocket *m_hbSock{nullptr};
    QByteArray m_mainRx;
    QByteArray m_hbRx;
    int m_probesReceived{0};
    bool m_gotDisconnectNotice{false};
    quint16 m_ackCount{0};
};

} // namespace

class VisionOutputDeviceTest : public QObject {
    Q_OBJECT

private slots:

    void test_config_clone_and_json() {
        VisionTcpipDeviceCfg cfg = makeCfg();
        QJsonObject obj = cfg.toJson();

        VisionTcpipDeviceCfg restored;
        QVERIFY(restored.fromJson(obj));
        QCOMPARE(restored.m_listenAddress,       cfg.m_listenAddress);
        QCOMPARE(restored.m_mainPort,            cfg.m_mainPort);
        QCOMPARE(restored.m_heartbeatPort,       cfg.m_heartbeatPort);
        QCOMPARE(restored.m_heartbeatIntervalMs, cfg.m_heartbeatIntervalMs);
        QCOMPARE(restored.m_heartbeatTimeoutMs,  cfg.m_heartbeatTimeoutMs);

        std::unique_ptr<IDeviceCfg> cloned(cfg.clone());
        QVERIFY(cloned != nullptr);
        QCOMPARE(cloned->deviceType(), DeviceType::VisionOutput);
        QCOMPARE(static_cast<VisionOutputDeviceCfg*>(cloned.get())->visionOutputType(),
                 VisionOutputType::VisionTCPIP);
    }

    void test_request_payload_format() {
        QVector<VisionOutputPosition> positions = {
            {10.0, 20.0, 0.0, 90.0},
            {15.0, 25.0, 0.0, 0.0},
        };
        VisionOutputRequest req(positions);
        QCOMPARE(req.buildPayload(),
                 QByteArray("2,00010.00,00020.00,00000.00,00090.00,00015.00,00025.00,00000.00,00000.00;"));

        VisionOutputRequest empty(QVector<VisionOutputPosition>{});
        QCOMPARE(empty.buildPayload(), QByteArray("0;"));

        VisionOutputRequest raw(QByteArray("raw_bytes;"));
        QCOMPARE(raw.buildPayload(), QByteArray("raw_bytes;"));
    }

    // Server reaches Connected (listening) as soon as deviceConnect succeeds,
    // before any client attaches.
    void test_server_listens_on_connect() {
        VisionTcpipDevice device(QStringLiteral("srv"), QStringLiteral("Server"));
        VisionTcpipDeviceCfg cfg = makeCfg();
        device.setVisionTcpipConfig(cfg);

        QVERIFY(device.deviceConnect());
        QCOMPARE(device.connectStatus(), ConnectStatus::Connected);
        QVERIFY(!device.isMainClientConnected());
        QVERIFY(!device.isHeartbeatClientConnected());

        device.deviceDisconnect();
        QCOMPARE(device.connectStatus(), ConnectStatus::Disconnected);
    }

    // Client attaches on both ports; heartbeat probe is acked and the counter
    // advances; result push reaches the client; client requests are received.
    void test_client_attach_heartbeat_and_io() {
        VisionTcpipDevice device(QStringLiteral("srv_io"), QStringLiteral("Server IO"));
        VisionTcpipDeviceCfg cfg = makeCfg();
        device.setVisionTcpipConfig(cfg);
        QVERIFY(device.deviceConnect());

        QSignalSpy mainStateSpy(&device, &VisionTcpipDevice::mainClientStateChanged);
        QSignalSpy reqSpy(&device, &VisionTcpipDevice::mainRequestReceived);

        ClientPeer peer(/*autoAck=*/true);
        peer.connectHeartbeat();
        peer.connectMain();

        QVERIFY(waitFor([&]() { return device.isMainClientConnected()
                                    && device.isHeartbeatClientConnected(); }));
        QVERIFY(!mainStateSpy.isEmpty());
        QCOMPARE(mainStateSpy.last().at(0).toBool(), true);

        // Heartbeat: at least one probe acked -> expected counter advanced.
        QVERIFY(waitFor([&]() { return peer.probesReceived() >= 1
                                    && device.expectedAckCount() >= 1; }));
        QCOMPARE(device.connectStatus(), ConnectStatus::Connected);

        // Result push from the server reaches the client's main socket.
        VisionOutputRequest result(QVector<VisionOutputPosition>{{1.0, 2.0, 3.0, 4.0}});
        QVERIFY(device.pushRequest(&result));
        QVERIFY(waitFor([&]() { return peer.mainRx().contains(';'); }));
        QCOMPARE(peer.mainRx(), QByteArray("1,00001.00,00002.00,00003.00,00004.00;"));

        // A ';'-framed message from the client is surfaced as a request.
        peer.sendMain(QByteArray("trigger;"));
        QVERIFY(waitFor([&]() { return reqSpy.count() >= 1; }));
        QCOMPARE(reqSpy.last().at(0).toByteArray(), QByteArray("trigger;"));

        device.deviceDisconnect();
    }

    // A graceful deviceDisconnect() sends "disconnect." on the heartbeat port
    // before the link is torn down.
    void test_disconnect_notice_on_graceful_close() {
        VisionTcpipDevice device(QStringLiteral("srv_dc"), QStringLiteral("Server DC"));
        VisionTcpipDeviceCfg cfg = makeCfg();
        device.setVisionTcpipConfig(cfg);
        QVERIFY(device.deviceConnect());

        ClientPeer peer(/*autoAck=*/true);
        peer.connectHeartbeat();
        QVERIFY(waitFor([&]() { return device.isHeartbeatClientConnected()
                                    && peer.probesReceived() >= 1; }));
        QVERIFY(!peer.gotDisconnectNotice());

        device.deviceDisconnect();
        QVERIFY(waitFor([&]() { return peer.gotDisconnectNotice(); }));
        QCOMPARE(device.connectStatus(), ConnectStatus::Disconnected);
    }

    // No ack from the heartbeat client -> server declares LostConnected.
    void test_heartbeat_timeout_declares_lost() {
        VisionTcpipDevice device(QStringLiteral("srv_to"), QStringLiteral("Server TO"));
        VisionTcpipDeviceCfg cfg = makeCfg();
        device.setVisionTcpipConfig(cfg);
        QVERIFY(device.deviceConnect());

        QSignalSpy lostSpy(&device, &VisionTcpipDevice::heartbeatLost);

        ClientPeer peer(/*autoAck=*/false);
        peer.connectHeartbeat();
        QVERIFY(waitFor([&]() { return device.isHeartbeatClientConnected(); }));

        QVERIFY(waitFor([&]() {
            return device.connectStatus() == ConnectStatus::LostConnected;
        }, 3000));
        QVERIFY(lostSpy.count() >= 1);
        QVERIFY(device.diagnostics().lostConnectionCount >= 1);

        device.deviceDisconnect();
        // The notice is a graceful-close signal only; the lost path never sends it.
        QVERIFY(!peer.gotDisconnectNotice());
    }

    // A second client on the main port is rejected; the first stays attached.
    void test_reject_duplicate_main_client() {
        VisionTcpipDevice device(QStringLiteral("srv_dup"), QStringLiteral("Server Dup"));
        VisionTcpipDeviceCfg cfg = makeCfg();
        device.setVisionTcpipConfig(cfg);
        QVERIFY(device.deviceConnect());

        ClientPeer first(/*autoAck=*/true);
        first.connectMain();
        QVERIFY(waitFor([&]() { return device.isMainClientConnected(); }));

        ClientPeer second(/*autoAck=*/true);
        second.connectMain();
        // Give the server a chance to accept + reject the duplicate.
        QTest::qWait(200);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        // The server still has exactly one main client and forwards a push to it.
        QVERIFY(device.isMainClientConnected());
        VisionOutputRequest result(QVector<VisionOutputPosition>{{5.0, 6.0, 7.0, 8.0}});
        QVERIFY(device.pushRequest(&result));
        QVERIFY(waitFor([&]() { return first.mainRx().contains(';'); }));
        QCOMPARE(first.mainRx(), QByteArray("1,00005.00,00006.00,00007.00,00008.00;"));

        device.deviceDisconnect();
    }
};

QTEST_MAIN(VisionOutputDeviceTest)
#include "main.moc"
