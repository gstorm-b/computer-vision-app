/**
 * @file main.cpp
 * @brief Modbus device tests: the shipped client and server devices driven against real Qt
 *        Modbus peers over loopback. No PLC hardware.
 *
 * **What these tests prove.** That the two devices speak Modbus TCP correctly enough for a Qt
 * peer on the other end to read and write what the product intends: the poll reaches the task
 * signal path, writes land where they are addressed, a lost link is reported the way the runtime
 * recovery policy needs, and a result publish leaves the register block a PLC program can read
 * per `docs/domains/task_localization/modbus_result_contract.md`.
 *
 * **What they do not prove.** That a *particular customer PLC* agrees. Real controllers differ on
 * unit-id handling, on whether they accept a multi-register write spanning their own block
 * boundaries, and on how they present addresses to the ladder programmer (1-based "40001" style
 * references versus the 0-based protocol addresses used here). Loopback cannot surface any of
 * that. Only a real slave/master settles it, which is why Checkpoint B is owner-run.
 */

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QModbusTcpClient>
#include <QModbusTcpServer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QThread>

#include <functional>

#include "device/plc/modbus/modbus_register_map.h"
#include "device/plc/modbus/modbus_result_layout.h"
#include "device/plc/modbus/modbus_tcp_client_device.h"
#include "device/plc/modbus/modbus_tcp_server_device.h"

using namespace vc::device;

namespace {

/// Picks a port nothing is listening on, by binding and immediately releasing one. Racy in
/// principle, reliable in practice on a test machine, and far better than a hard-coded port that
/// collides with whatever else the developer is running.
quint16 freePort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

/// Runs a blocking read against `client` and returns the values, or an empty list on failure.
QList<quint16> readRegisters(QModbusTcpClient *client, int serverAddress,
                             QModbusDataUnit::RegisterType type, int start, int count)
{
    QModbusDataUnit request(type, start, quint16(count));
    QModbusReply *reply = client->sendReadRequest(request, serverAddress);
    if (!reply) {
        return {};
    }
    if (!reply->isFinished()) {
        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        guard.start(3000);
        loop.exec();
    }
    QList<quint16> values;
    if (reply->isFinished() && reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        for (uint i = 0; i < unit.valueCount(); ++i) {
            values.append(unit.value(i));
        }
    }
    reply->deleteLater();
    return values;
}

/// Writes a contiguous block through `client` in ONE request and reports whether the slave
/// accepted it. Distinct from writeRegister() because a multi-register write is a different
/// function code (FC16) covering a CONTIGUOUS span — which is the whole point of the test that
/// uses it.
bool writeRegisterBlock(QModbusTcpClient *client, int serverAddress,
                        QModbusDataUnit::RegisterType type, int address,
                        const QList<quint16> &values)
{
    QModbusDataUnit request(type, address, quint16(values.size()));
    for (int i = 0; i < values.size(); ++i) {
        request.setValue(i, values.at(i));
    }
    QModbusReply *reply = client->sendWriteRequest(request, serverAddress);
    if (!reply) {
        return false;
    }
    if (!reply->isFinished()) {
        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        guard.start(3000);
        loop.exec();
    }
    const bool ok = reply->isFinished() && reply->error() == QModbusDevice::NoError;
    reply->deleteLater();
    return ok;
}

/// Writes one register through `client` and reports whether the slave accepted it.
bool writeRegister(QModbusTcpClient *client, int serverAddress,
                   QModbusDataUnit::RegisterType type, int address, quint16 value)
{
    QModbusDataUnit request(type, address, 1);
    request.setValue(0, value);
    QModbusReply *reply = client->sendWriteRequest(request, serverAddress);
    if (!reply) {
        return false;
    }
    if (!reply->isFinished()) {
        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(reply, &QModbusReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        guard.start(3000);
        loop.exec();
    }
    const bool ok = reply->isFinished() && reply->error() == QModbusDevice::NoError;
    reply->deleteLater();
    return ok;
}

/// Brings a QModbusDevice to the connected state, bounded.
bool connectPeer(QModbusDevice *device, int timeoutMs = 3000)
{
    if (!device->connectDevice()) {
        return false;
    }
    if (device->state() == QModbusDevice::ConnectedState) {
        return true;
    }
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(device, &QModbusDevice::stateChanged, &loop,
                     [&loop](QModbusDevice::State state) {
        if (state == QModbusDevice::ConnectedState
            || state == QModbusDevice::UnconnectedState) {
            loop.quit();
        }
    });
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    guard.start(timeoutMs);
    loop.exec();
    return device->state() == QModbusDevice::ConnectedState;
}

/// A plain Qt Modbus slave standing in for a customer PLC: the register space the client device
/// is configured against, plus room for the result block.
QModbusTcpServer *makeSlaveFixture(QObject *parent, quint16 port, int unitId)
{
    auto *slave = new QModbusTcpServer(parent);
    QModbusDataUnitMap space;
    space.insert(QModbusDataUnit::Coils, { QModbusDataUnit::Coils, 0, 64 });
    space.insert(QModbusDataUnit::DiscreteInputs, { QModbusDataUnit::DiscreteInputs, 0, 64 });
    // Wide enough to cover both the polled span (0..63) and the result block at 1000.
    space.insert(QModbusDataUnit::HoldingRegisters,
                 { QModbusDataUnit::HoldingRegisters, 0, 1200 });
    space.insert(QModbusDataUnit::InputRegisters, { QModbusDataUnit::InputRegisters, 0, 64 });
    slave->setMap(space);
    slave->setServerAddress(unitId);
    slave->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                  QStringLiteral("127.0.0.1"));
    slave->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
    return slave;
}

/// Runs `job` to completion on a thread that is not the caller's, and returns what it returned.
///
/// Used to reproduce the exact mistake the device panel shipped: a write issued from a thread the
/// device does not live on. Nothing here is asynchronous — the caller waits — so the assertion
/// that follows is about thread *affinity*, not about timing.
bool runOnAnotherThread(std::function<bool()> job)
{
    class Runner : public QThread {
    public:
        explicit Runner(std::function<bool()> job) : m_job(std::move(job)) {}
        bool result{false};
    protected:
        void run() override { result = m_job(); }
    private:
        std::function<bool()> m_job;
    };

    Runner runner(std::move(job));
    runner.start();
    runner.wait();
    return runner.result;
}

} // namespace

class ModbusDeviceTest : public QObject {
    Q_OBJECT

private slots:

    // ── Client device ────────────────────────────────────────────────────────────────────────

    // The poll must reach the task through valueChanged(), keyed by the tag names saved projects
    // contain, and the first pass after connect must report nothing: values read from a
    // freshly-connected slave are a starting state, and reporting them as changes would fire a
    // rising edge on every task signal that happens to be true at connect.
    void test_client_poll_publishes_changes_but_not_the_connect_state()
    {
        const quint16 port = freePort();
        QModbusTcpServer *slave = makeSlaveFixture(this, port, 1);
        QVERIFY2(connectPeer(slave), qPrintable(slave->errorString()));
        QVERIFY(slave->setData(QModbusDataUnit::Coils, 5, 1));
        QVERIFY(slave->setData(QModbusDataUnit::HoldingRegisters, 10, 1234));

        ModbusTcpClientDevice device(QStringLiteral("mb_client"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_refreshInterval = 20;
        cfg.m_responseTimeout = 1000;
        device.setDeviceConfig(&cfg);

        QSignalSpy valueSpy(&device, &PlcDevice::valueChanged);
        QSignalSpy pollSpy(&device, &PlcDevice::pollingUpdate);
        QVERIFY(valueSpy.isValid());
        QVERIFY(pollSpy.isValid());

        QVERIFY2(device.deviceConnect(), "client could not connect to the loopback slave");
        QVERIFY(device.isDeviceConnected());

        // Let a few passes run. The starting state must not be reported as changes.
        QTRY_VERIFY_WITH_TIMEOUT(pollSpy.count() >= 2, 3000);
        QCOMPARE(valueSpy.count(), 0);

        // Now move something on the slave: that is a change, and it must arrive.
        QVERIFY(slave->setData(QModbusDataUnit::Coils, 5, 0));
        QVERIFY(slave->setData(QModbusDataUnit::HoldingRegisters, 10, 4321));

        QTRY_VERIFY_WITH_TIMEOUT(valueSpy.count() >= 1, 3000);
        QMap<QString, QVariant> changed;
        for (const QList<QVariant> &args : valueSpy) {
            const auto batch = args.at(0).value<QMap<QString, QVariant>>();
            for (auto it = batch.cbegin(); it != batch.cend(); ++it) {
                changed.insert(it.key(), it.value());
            }
        }
        QCOMPARE(changed.value(QStringLiteral("COIL00005")).toBool(), false);
        QCOMPARE(changed.value(QStringLiteral("HR00010")).toInt(), 4321);

        device.deviceDisconnect();
        slave->disconnectDevice();
    }

    // A write must land at the addressed register on the slave, and a write to an area the
    // Modbus specification makes read-only must be refused locally rather than sent and rejected.
    void test_client_writes_reach_the_slave_and_respect_read_only_areas()
    {
        const quint16 port = freePort();
        QModbusTcpServer *slave = makeSlaveFixture(this, port, 1);
        QVERIFY2(connectPeer(slave), qPrintable(slave->errorString()));

        ModbusTcpClientDevice device(QStringLiteral("mb_client_w"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_refreshInterval = 1000;   // keep polling out of the way of the assertions
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        QVERIFY(device.writeDigitalIoByName(QStringLiteral("COIL00003"), true));
        quint16 coil = 0;
        QVERIFY(slave->data(QModbusDataUnit::Coils, 3, &coil));
        // Non-zero, not literally 1: "write single coil" (function 5) puts 0xFF00 on the wire for
        // ON, and Qt's server stores what arrived rather than normalising it to a bit. A read
        // (function 1) turns it back into a bit, so nothing downstream sees 0xFF00 — but a test
        // that reached into the fixture's storage and expected 1 would be asserting Qt's
        // internals, not our behaviour.
        QVERIFY(coil != 0);

        QVERIFY(device.writeWordIoByName(QStringLiteral("HR00007"), qint16(-2)));
        quint16 reg = 0;
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters, 7, &reg));
        QCOMPARE(reg, quint16(0xFFFE));

        // Read-only by protocol, refused before anything goes on the wire.
        QVERIFY(!device.writeDigitalIoByName(QStringLiteral("DI00003"), true));
        QVERIFY(!device.writeWordIoByName(QStringLiteral("IR00003"), 1));
        // Outside the configured span.
        QVERIFY(!device.writeWordIoByName(QStringLiteral("HR09999"), 1));
        // Not a tag of the right kind.
        QVERIFY(!device.writeDigitalIoByName(QStringLiteral("HR00007"), true));
        QVERIFY(!device.writeWordIoByName(QStringLiteral("COIL00003"), 1));

        device.deviceDisconnect();
        slave->disconnectDevice();
    }

    // A write while disconnected must fail and say so immediately. Queueing it would fire at an
    // unpredictable moment later, against a machine that has moved on.
    void test_client_write_while_disconnected_fails_rather_than_queues()
    {
        ModbusTcpClientDevice device(QStringLiteral("mb_client_off"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = freePort();   // nothing listening
        device.setDeviceConfig(&cfg);

        QVERIFY(!device.isDeviceConnected());
        QVERIFY(!device.writeDigitalIoByName(QStringLiteral("COIL00000"), true));
        QVERIFY(!device.writeWordIoByName(QStringLiteral("HR00000"), 1));

        QString message;
        QVERIFY(!device.sendVisionResult({}, &message));
        QVERIFY2(!message.isEmpty(), "a refused result publish must carry a reason");
    }

    // The defect this guard exists for, reproduced. The device panel called the writer straight
    // from the GUI thread while the device lived on PlcRunner's worker thread. Qt reported nothing:
    // the frame never reached the socket, the peer logged nothing because nothing arrived, and the
    // caller waited out the full response timeout. Intermittently — sometimes the frame did go —
    // which is the worst kind of wrong. The write must be refused, immediately, and the register on
    // the slave must be untouched.
    void test_a_client_write_from_the_wrong_thread_is_refused_not_silently_lost()
    {
        const quint16 port = freePort();
        QModbusTcpServer *slave = makeSlaveFixture(this, port, 1);
        QVERIFY2(connectPeer(slave), qPrintable(slave->errorString()));

        ModbusTcpClientDevice device(QStringLiteral("mb_client_thread"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_refreshInterval = 1000;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        // From this thread — the device's own — the same write succeeds, so the refusal below is
        // about the thread and nothing else.
        QVERIFY(device.writeWordIoByName(QStringLiteral("HR00009"), 111));
        quint16 reg = 0;
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters, 9, &reg));
        QCOMPARE(reg, quint16(111));

        const bool offThread = runOnAnotherThread([&device]() {
            return device.writeWordIoByName(QStringLiteral("HR00009"), 222);
        });
        QVERIFY2(!offThread, "a write from another thread must be refused, not attempted");

        // Refused *before* anything went out: the slave still holds the value the good write left.
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters, 9, &reg));
        QCOMPARE(reg, quint16(111));

        const bool offThreadBit = runOnAnotherThread([&device]() {
            return device.writeDigitalIoByName(QStringLiteral("COIL00009"), true);
        });
        QVERIFY(!offThreadBit);
        quint16 coil = 1;
        QVERIFY(slave->data(QModbusDataUnit::Coils, 9, &coil));
        QCOMPARE(coil, quint16(0));

        // A wiring mistake must not spend the retry budget. The link is healthy and must still be
        // usable: were the refusals counted as transaction failures, enough of them would declare
        // a perfectly good link lost.
        QVERIFY(device.isDeviceConnected());
        QVERIFY(device.writeWordIoByName(QStringLiteral("HR00009"), 333));
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters, 9, &reg));
        QCOMPARE(reg, quint16(333));

        device.deviceDisconnect();
        slave->disconnectDevice();
    }

    // The server's half of the same guard: it owns thread-affine sockets, and setData() from
    // another thread neither fails nor reliably takes effect.
    // The field defect (owner, 2026-09-07): "trong runtime không thể ghi coil value".
    //
    // The runtime publishes in bursts — publishInitialReadyOutputs() alone is eight bools and two
    // words — and PlcRunner posts each as its own queued meta-call. transact() waits for its reply
    // in a nested QEventLoop, and exec(ExcludeUserInputEvents) defers only USER input, so the rest
    // of the burst is dispatched INSIDE the first write's wait.
    //
    // That used to do two bad things at once: every write after the first was refused outright
    // ("A Modbus transaction is already in flight"), so nine of ten never reached the PLC; and each
    // refusal was charged to the link retry budget, so past retryCount (default 3) the client
    // declared a perfectly healthy link lost and disconnected itself.
    //
    // This test posts the burst the same way the runner does, so the collisions really happen.
    void test_a_queued_burst_of_writes_all_reach_the_slave_and_keep_the_link()
    {
        const quint16 port = freePort();
        QModbusTcpServer *slave = makeSlaveFixture(this, port, 1);
        QVERIFY2(connectPeer(slave), qPrintable(slave->errorString()));

        ModbusTcpClientDevice device(QStringLiteral("mb_client_burst"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_refreshInterval = 1000;   // keep polling out of the way, like the sibling tests
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        constexpr int kBurst = 10;   // more than m_retryCount, which is what used to tear the link
        for (int i = 0; i < kBurst; ++i) {
            QMetaObject::invokeMethod(&device, [&device, i]() {
                device.writeDigitalIoByName(QStringLiteral("COIL%1").arg(i, 5, 10, QChar('0')),
                                            (i % 2) == 0);
            }, Qt::QueuedConnection);
        }

        // Wait on the LAST coil the burst sets HIGH (i = 8, the ninth write). Waiting on a coil
        // whose expected value is 0 would pass before anything happened at all, since every coil
        // starts at 0 — the wait has to observe a change, not a coincidence.
        //
        // Compared as a truth value, not against 1: QModbusServer stores a coil set by FC5 as the
        // protocol's own ON word, 0xFF00, so `== 1` fails on a coil that is genuinely on.
        QTRY_VERIFY_WITH_TIMEOUT(([&]() {
            quint16 lastHigh = 0;
            return slave->data(QModbusDataUnit::Coils, 8, &lastHigh) && lastHigh != 0;
        })(), 5000);

        for (int i = 0; i < kBurst; ++i) {
            quint16 coil = 0xFFFF;
            QVERIFY2(slave->data(QModbusDataUnit::Coils, quint16(i), &coil),
                     qPrintable(QStringLiteral("coil %1 was never written").arg(i)));
            QCOMPARE(coil != 0, (i % 2) == 0);
        }

        // And the link must have survived its own burst. This is the half that used to fail
        // loudest: the client logged "Modbus link declared lost after repeated failures" and
        // dropped a connection the peer never had a problem with.
        QVERIFY2(device.isDeviceConnected(),
                 "the client tore its own link down on a burst of writes");

        // Still usable afterwards, so the retry budget really was left alone.
        QVERIFY(device.writeWordIoByName(QStringLiteral("HR00009"), 444));
        quint16 reg = 0;
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters, 9, &reg));
        QCOMPARE(reg, quint16(444));

        device.deviceDisconnect();
        slave->disconnectDevice();
    }

    // ── Phase 9 / E2: tracked writes resolve on every family ─────────────────────────────
    //
    // writeDigitalIoByName() returning true has never meant "written": this client PARKS a write
    // that collides with an in-flight transaction and returns true for it, which is correct (the
    // alternative dropped nine writes out of ten in a publish burst) but leaves the caller unable
    // to tell a write that reached the slave from one that was dropped. The tracked form reports
    // the outcome of the write that actually happened.

    void test_a_deferred_client_write_resolves_with_the_outcome_of_the_replay()
    {
        const quint16 port = freePort();
        QModbusTcpServer *slave = makeSlaveFixture(this, port, 1);
        QVERIFY2(connectPeer(slave), qPrintable(slave->errorString()));

        ModbusTcpClientDevice device(QStringLiteral("mb_track_defer"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_refreshInterval = 1000;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        QSignalSpy finished(&device, &vc::device::PlcDevice::ioWriteFinished);
        QVERIFY(finished.isValid());

        // A burst: the first write holds the wire and the rest are parked inside its nested wait,
        // which is the path this test exists for.
        // Ids are allocated by the caller — the same contract PlcRunner uses, so the test drives
        // the device exactly as the runner will.
        constexpr int kBurst = 6;
        QVector<quint64> ids(kBurst, 0);
        for (int i = 0; i < kBurst; ++i) {
            ids[i] = quint64(i + 1);
            QMetaObject::invokeMethod(&device, [&device, &ids, i]() {
                device.writeDigitalIoTracked(
                    ids[i], QStringLiteral("COIL%1").arg(i, 5, 10, QChar('0')), true);
            }, Qt::QueuedConnection);
        }

        // Killed LAST, so it is dispatched inside the first write's nested wait — after the rest
        // have parked and before the drain replays them.
        //
        // This is what makes the case discriminate. Asserting only that the burst resolves once
        // each with ok=true passes whether the completion comes from the replay or is handed out
        // optimistically at submission — measured: resolving at submission left that version of
        // this test fully green. The outcome has to be one that ONLY the replay can produce, so
        // the link is taken away between the two.
        QMetaObject::invokeMethod(slave, [slave]() { slave->disconnectDevice(); },
                                  Qt::QueuedConnection);

        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), kBurst, 8000);

        QHash<quint64, bool> okById;
        for (const QList<QVariant> &emission : finished) {
            const quint64 id = emission.at(0).toULongLong();
            QVERIFY2(id != 0, "a tracked write must never resolve with id 0");
            QVERIFY2(!okById.contains(id), "no id may resolve twice");
            okById.insert(id, emission.at(1).toBool());
            if (!emission.at(1).toBool()) {
                QVERIFY2(!emission.at(2).toString().isEmpty(), "a failure must say why");
            }
        }
        for (const quint64 id : ids) {
            QVERIFY2(id != 0, "every submission must hand back an id");
            QVERIFY2(okById.contains(id), "every submitted write must resolve exactly once");
        }

        // ids[0] takes the wire; ids[1..] are the ones parked inside its nested wait. The
        // assertion has to be about THOSE, and only those: ids[0] fails on its own when the slave
        // dies mid-transaction, and counting it satisfied a weaker version of this check even with
        // the submission-time bug installed. Measured, not assumed.
        int parkedFailures = 0;
        for (int i = 1; i < kBurst; ++i) {
            if (!okById.value(ids[i], true)) {
                ++parkedFailures;
            }
        }
        QVERIFY2(parkedFailures > 0,
                 "a write parked when the link died must report the failure of its REPLAY, not "
                 "the optimistic 'true' the bool return has always given for a queued write");

        device.deviceDisconnect();
    }

    /// A write parked when the link dies is unreplayable. Failing it is the point: silence would
    /// leave the caller waiting for a completion that can never come.
    void test_a_deferred_write_abandoned_by_teardown_resolves_as_failed()
    {
        const quint16 port = freePort();
        QModbusTcpServer *slave = makeSlaveFixture(this, port, 1);
        QVERIFY2(connectPeer(slave), qPrintable(slave->errorString()));

        ModbusTcpClientDevice device(QStringLiteral("mb_track_drop"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_refreshInterval = 1000;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        QSignalSpy finished(&device, &vc::device::PlcDevice::ioWriteFinished);

        // Disconnected first, so the write is refused rather than parked — the same "it will
        // never happen" outcome, reached by the path a test can drive deterministically.
        device.deviceDisconnect();
        slave->disconnectDevice();

        const quint64 id = 77;
        QMetaObject::invokeMethod(&device, [&device]() {
            device.writeDigitalIoTracked(77, QStringLiteral("COIL00000"), true);
        }, Qt::QueuedConnection);

        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 4000);
        QCOMPARE(finished.at(0).at(0).toULongLong(), id);
        QCOMPARE(finished.at(0).at(1).toBool(), false);
        QVERIFY2(!finished.at(0).at(2).toString().isEmpty(), "a failure must say why");
    }

    /// The synchronous family inherits PlcDevice's default and must not be the one that lies:
    /// its bool return already carries the outcome, so the completion is immediate.
    void test_a_server_tracked_write_resolves_immediately_with_the_real_outcome()
    {
        ModbusTcpServerDevice device(QStringLiteral("mb_srv_track"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = freePort();
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        QSignalSpy finished(&device, &vc::device::PlcDevice::ioWriteFinished);
        QVERIFY(finished.isValid());

        constexpr quint64 kOkId = 11;
        constexpr quint64 kBadId = 12;

        device.writeWordIoTracked(kOkId, QStringLiteral("HR00003"), 1234);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(0).toULongLong(), kOkId);
        QCOMPARE(finished.at(0).at(1).toBool(), true);

        // And a refusal is reported as one, not swallowed.
        device.writeWordIoTracked(kBadId, QStringLiteral("NOTATAG"), 1);
        QCOMPARE(finished.count(), 2);
        QCOMPARE(finished.at(1).at(0).toULongLong(), kBadId);
        QCOMPARE(finished.at(1).at(1).toBool(), false);

        device.deviceDisconnect();
    }

    void test_a_server_write_from_the_wrong_thread_is_refused()
    {
        ModbusTcpServerDevice device(QStringLiteral("mb_server_thread"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = freePort();
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        QVERIFY(device.writeDigitalIoByName(QStringLiteral("COIL00002"), true));
        QVERIFY(device.writeWordIoByName(QStringLiteral("HR00002"), 77));

        QVERIFY(!runOnAnotherThread([&device]() {
            return device.writeDigitalIoByName(QStringLiteral("COIL00003"), true);
        }));
        QVERIFY(!runOnAnotherThread([&device]() {
            return device.writeWordIoByName(QStringLiteral("HR00003"), 88);
        }));

        // The refused writes left no trace in the served space.
        const QStringList names = device.availableDigitalIoNames();
        QVERIFY(names.contains(QStringLiteral("COIL00003")));

        device.deviceDisconnect();
    }

    // Connecting to a port nothing is listening on must fail visibly, not leave a device that
    // looks connected.
    void test_client_connect_to_nothing_fails_visibly()
    {
        ModbusTcpClientDevice device(QStringLiteral("mb_client_dead"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = freePort();
        cfg.m_responseTimeout = 300;
        device.setDeviceConfig(&cfg);

        QSignalSpy failedSpy(&device, &IDevice::connectionFailed);
        QVERIFY(failedSpy.isValid());

        QVERIFY(!device.deviceConnect());
        QVERIFY(!device.isDeviceConnected());
        QCOMPARE(device.connectStatus(), ConnectStatus::ConnectFailed);
        QCOMPARE(failedSpy.count(), 1);
    }

    // The result block a PLC program reads. Asserted through a real slave, at the addresses and
    // in the encoding the contract document publishes.
    void test_client_result_publish_lands_in_the_contract_layout()
    {
        const quint16 port = freePort();
        QModbusTcpServer *slave = makeSlaveFixture(this, port, 1);
        QVERIFY2(connectPeer(slave), qPrintable(slave->errorString()));

        ModbusTcpClientDevice device(QStringLiteral("mb_client_r"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_refreshInterval = 1000;
        cfg.m_resultStartAddress = 1000;
        cfg.m_resultMaxPositions = 4;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        VisionOutputPosition position;
        position.x = 125.50;
        position.y = -40.25;
        position.rz = 37.10;

        QString message;
        QVERIFY2(device.sendVisionResult({ position }, &message), qPrintable(message));

        quint16 raw = 0;
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters,
                            1000 + ModbusResultLayout::kOffsetCount, &raw));
        QCOMPARE(raw, quint16(1));
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters,
                            1000 + ModbusResultLayout::kOffsetSequence, &raw));
        QCOMPARE(raw, quint16(1));   // first publish since construction
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters,
                            1000 + ModbusResultLayout::kOffsetFlags, &raw));
        QCOMPARE(raw, quint16(0));

        // The pose, read back the way a PLC program would.
        QList<quint16> payload;
        for (int i = 0; i < ModbusResultLayout::kRegistersPerPosition; ++i) {
            QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters,
                                quint16(1000 + ModbusResultLayout::kHeaderRegisters + i), &raw));
            payload.append(raw);
        }
        const QVector<VisionOutputPosition> decoded =
            ModbusResultLayout::decodePayload(payload, 1);
        QCOMPARE(decoded.size(), 1);
        QCOMPARE(decoded.at(0).x, 125.50);
        QCOMPARE(decoded.at(0).y, -40.25);
        QCOMPARE(decoded.at(0).rz, 37.10);

        // Second publish advances the sequence, so a master can tell two identical results apart.
        QVERIFY(device.sendVisionResult({ position }, &message));
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters,
                            1000 + ModbusResultLayout::kOffsetSequence, &raw));
        QCOMPARE(raw, quint16(2));

        // A publish that overflows the block reports truncation and never writes more than the
        // block holds.
        QVector<VisionOutputPosition> many;
        for (int i = 0; i < 9; ++i) {
            many.append(position);
        }
        QVERIFY(device.sendVisionResult(many, &message));
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters,
                            1000 + ModbusResultLayout::kOffsetCount, &raw));
        QCOMPARE(raw, quint16(4));
        QVERIFY(slave->data(QModbusDataUnit::HoldingRegisters,
                            1000 + ModbusResultLayout::kOffsetFlags, &raw));
        QCOMPARE(raw, ModbusResultLayout::kFlagTruncated);

        device.deviceDisconnect();
        slave->disconnectDevice();
    }

    // ── Server device ────────────────────────────────────────────────────────────────────────

    // A master's write must reach the task through the same signal path a client poll uses. If
    // these two diverged, a task commissioned against one sub-type would behave differently
    // against the other, which is exactly what sharing the register map exists to prevent.
    void test_server_master_writes_surface_as_value_changes()
    {
        const quint16 port = freePort();

        ModbusTcpServerDevice device(QStringLiteral("mb_server"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        device.setDeviceConfig(&cfg);

        QSignalSpy valueSpy(&device, &PlcDevice::valueChanged);
        QVERIFY(valueSpy.isValid());
        QVERIFY2(device.deviceConnect(), "server could not bind the loopback port");

        auto *master = new QModbusTcpClient(this);
        master->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                       QStringLiteral("127.0.0.1"));
        master->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
        master->setTimeout(2000);
        QVERIFY2(connectPeer(master), qPrintable(master->errorString()));

        QVERIFY(writeRegister(master, cfg.m_unitId, QModbusDataUnit::Coils, 4, 1));
        QVERIFY(writeRegister(master, cfg.m_unitId, QModbusDataUnit::HoldingRegisters, 9, 77));

        QTRY_VERIFY_WITH_TIMEOUT(valueSpy.count() >= 1, 3000);
        QMap<QString, QVariant> changed;
        for (const QList<QVariant> &args : valueSpy) {
            const auto batch = args.at(0).value<QMap<QString, QVariant>>();
            for (auto it = batch.cbegin(); it != batch.cend(); ++it) {
                changed.insert(it.key(), it.value());
            }
        }
        QCOMPARE(changed.value(QStringLiteral("COIL00004")).toBool(), true);
        QCOMPARE(changed.value(QStringLiteral("HR00009")).toInt(), 77);

        master->disconnectDevice();
        device.deviceDisconnect();
    }

    // A master batching two signals into one FC16 request gets an ACK for the whole block, but
    // only the part inside the mapped span reaches the task signal map. The rest is dropped.
    //
    // This is the shape of a real field defect (2026-09-04): a robot wrote nActiveCamera and
    // nActivePatternGroup in one request and only the camera took effect, while writing the same
    // two values as two separate requests worked. FC16 covers a CONTIGUOUS block, so batching
    // signals that are not adjacent puts the second value on whatever register follows the first.
    // The device cannot refuse the write — Qt has already served it from the register space it
    // owns — so all it can do is say so, which is why the "outside the mapped span" log is at
    // user level rather than dev level.
    void test_a_master_write_spanning_past_the_mapped_span_delivers_only_the_mapped_part()
    {
        const quint16 port = freePort();

        ModbusTcpServerDevice device(QStringLiteral("mb_server_span"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_holdingStart = 0;
        cfg.m_holdingCount = 8;      // mapped span is HR0..HR7; the server still serves further
        device.setDeviceConfig(&cfg);

        QSignalSpy valueSpy(&device, &PlcDevice::valueChanged);
        QVERIFY(valueSpy.isValid());
        QVERIFY2(device.deviceConnect(), "server could not bind the loopback port");

        auto *master = new QModbusTcpClient(this);
        master->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                       QStringLiteral("127.0.0.1"));
        master->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
        master->setTimeout(2000);
        QVERIFY2(connectPeer(master), qPrintable(master->errorString()));

        // One request, four registers, starting inside the mapped span and running two past its
        // end. The master is told this succeeded.
        QVERIFY2(writeRegisterBlock(master, cfg.m_unitId, QModbusDataUnit::HoldingRegisters,
                                    6, { 11, 22, 33, 44 }),
                 "the server refused a write that straddles the end of the mapped span");

        QTRY_VERIFY_WITH_TIMEOUT(valueSpy.count() >= 1, 3000);
        QMap<QString, QVariant> changed;
        for (const QList<QVariant> &args : valueSpy) {
            const auto batch = args.at(0).value<QMap<QString, QVariant>>();
            for (auto it = batch.cbegin(); it != batch.cend(); ++it) {
                changed.insert(it.key(), it.value());
            }
        }

        // HR6 and HR7 are mapped and arrive.
        QCOMPARE(changed.value(QStringLiteral("HR00006")).toInt(), 11);
        QCOMPARE(changed.value(QStringLiteral("HR00007")).toInt(), 22);
        // HR8 and HR9 are past the mapped span: accepted on the wire, never delivered. A signal
        // bound to either would sit at its old value with nothing failing anywhere.
        QVERIFY2(!changed.contains(QStringLiteral("HR00008")),
                 "an address outside the mapped span reached the signal map");
        QVERIFY2(!changed.contains(QStringLiteral("HR00009")),
                 "an address outside the mapped span reached the signal map");

        master->disconnectDevice();
        device.deviceDisconnect();
    }

    // The server's result block must read exactly as the client's does, at the same addresses,
    // so one PLC program serves both sub-types. This is the interoperability claim the contract
    // document makes, asserted rather than assumed.
    void test_server_result_block_reads_the_same_as_the_clients()
    {
        const quint16 port = freePort();

        ModbusTcpServerDevice device(QStringLiteral("mb_server_r"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_resultStartAddress = 1000;
        cfg.m_resultMaxPositions = 4;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        auto *master = new QModbusTcpClient(this);
        master->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                       QStringLiteral("127.0.0.1"));
        master->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
        master->setTimeout(2000);
        QVERIFY2(connectPeer(master), qPrintable(master->errorString()));

        VisionOutputPosition position;
        position.x = 125.50;
        position.y = -40.25;
        position.rz = 37.10;

        QString message;
        QVERIFY2(device.sendVisionResult({ position }, &message), qPrintable(message));

        const QList<quint16> header = readRegisters(
            master, cfg.m_unitId, QModbusDataUnit::HoldingRegisters,
            1000, ModbusResultLayout::kHeaderRegisters);
        QCOMPARE(header.size(), ModbusResultLayout::kHeaderRegisters);
        QCOMPARE(header.at(ModbusResultLayout::kOffsetCount), quint16(1));
        QCOMPARE(header.at(ModbusResultLayout::kOffsetSequence), quint16(1));
        QCOMPARE(header.at(ModbusResultLayout::kOffsetFlags), quint16(0));

        const QList<quint16> payload = readRegisters(
            master, cfg.m_unitId, QModbusDataUnit::HoldingRegisters,
            1000 + ModbusResultLayout::kHeaderRegisters,
            ModbusResultLayout::kRegistersPerPosition);
        const QVector<VisionOutputPosition> decoded =
            ModbusResultLayout::decodePayload(payload, 1);
        QCOMPARE(decoded.size(), 1);
        QCOMPARE(decoded.at(0).x, 125.50);
        QCOMPARE(decoded.at(0).y, -40.25);
        QCOMPARE(decoded.at(0).rz, 37.10);

        master->disconnectDevice();
        device.deviceDisconnect();
    }

    // The result block sits outside the mapped holding span by default. The server must still
    // own those registers, or a master's read of the result fails with an illegal-data-address
    // exception — the block would be published to nobody.
    void test_server_serves_the_result_block_outside_the_mapped_span()
    {
        const quint16 port = freePort();

        ModbusTcpServerDevice device(QStringLiteral("mb_server_span"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_holdingStart = 0;
        cfg.m_holdingCount = 8;         // far short of the result block
        cfg.m_resultStartAddress = 1000;
        cfg.m_resultMaxPositions = 2;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        auto *master = new QModbusTcpClient(this);
        master->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                       QStringLiteral("127.0.0.1"));
        master->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
        master->setTimeout(2000);
        QVERIFY(connectPeer(master));

        QString message;
        QVERIFY(device.sendVisionResult({ VisionOutputPosition() }, &message));

        const QList<quint16> header = readRegisters(
            master, cfg.m_unitId, QModbusDataUnit::HoldingRegisters, 1000, 4);
        QCOMPARE(header.size(), 4);
        QCOMPARE(header.at(ModbusResultLayout::kOffsetCount), quint16(1));

        master->disconnectDevice();
        device.deviceDisconnect();
    }

    // The server is the ONLY side that can write discrete inputs and input registers — "input" is
    // named from the server's point of view, and the protocol defines no function code by which a
    // master writes them. This originally failed: the server asked isWritable(), a
    // master-perspective predicate, and refused to write the two areas it owns.
    //
    // Both halves are asserted here: the server writes them and a master reads back what it wrote,
    // and the master's attempt to write the same area does not succeed.
    void test_server_writes_the_input_areas_the_master_can_only_read()
    {
        const quint16 port = freePort();

        ModbusTcpServerDevice device(QStringLiteral("mb_server_in"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        auto *master = new QModbusTcpClient(this);
        master->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                       QStringLiteral("127.0.0.1"));
        master->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
        master->setTimeout(2000);
        QVERIFY2(connectPeer(master), qPrintable(master->errorString()));

        // The server writes a discrete input and an input register.
        QVERIFY2(device.writeDigitalIoByName(QStringLiteral("DI00006"), true),
                 "the server owns the space and must be able to write a discrete input");
        QVERIFY2(device.writeWordIoByName(QStringLiteral("IR00011"), qint16(-3)),
                 "the server owns the space and must be able to write an input register");

        // The master reads them back — this is the whole point of the two areas.
        //
        // A bit read comes back byte-padded: asking for one discrete input yields a reply
        // carrying the whole byte, so Qt reports eight values. The first is the one addressed.
        // Assert the value, not the count, or this reads as a failure when the protocol is
        // behaving exactly as specified.
        const QList<quint16> bits =
            readRegisters(master, cfg.m_unitId, QModbusDataUnit::DiscreteInputs, 6, 1);
        QVERIFY(!bits.isEmpty());
        QCOMPARE(bits.at(0), quint16(1));

        const QList<quint16> words =
            readRegisters(master, cfg.m_unitId, QModbusDataUnit::InputRegisters, 11, 1);
        QCOMPARE(words.size(), 1);
        QCOMPARE(words.at(0), quint16(0xFFFD));

        // And the master cannot write them back. There is no function code for it, so the
        // request either never forms or comes back an error — never a success.
        QVERIFY2(!writeRegister(master, cfg.m_unitId, QModbusDataUnit::DiscreteInputs, 6, 0),
                 "a master must not be able to write a discrete input");
        QVERIFY2(!writeRegister(master, cfg.m_unitId, QModbusDataUnit::InputRegisters, 11, 0),
                 "a master must not be able to write an input register");

        // Unchanged by the refused writes.
        const QList<quint16> afterRefusal =
            readRegisters(master, cfg.m_unitId, QModbusDataUnit::DiscreteInputs, 6, 1);
        QVERIFY(!afterRefusal.isEmpty());
        QCOMPARE(afterRefusal.at(0), quint16(1));

        master->disconnectDevice();
        device.deviceDisconnect();
    }

    // The client is a master, so the same two areas stay refused for it — and that refusal is
    // local, before anything reaches the wire.
    void test_client_still_cannot_write_the_input_areas()
    {
        const quint16 port = freePort();
        QModbusTcpServer *slave = makeSlaveFixture(this, port, 1);
        QVERIFY2(connectPeer(slave), qPrintable(slave->errorString()));

        ModbusTcpClientDevice device(QStringLiteral("mb_client_in"), QStringLiteral("Client"));
        ModbusTcpClientCfg cfg;
        cfg.m_hostAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_refreshInterval = 1000;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        QVERIFY(!device.writeDigitalIoByName(QStringLiteral("DI00006"), true));
        QVERIFY(!device.writeWordIoByName(QStringLiteral("IR00011"), 1));

        device.deviceDisconnect();
        slave->disconnectDevice();
    }

    // The server may publish the vision result into input registers instead of holding registers.
    // This is the safer server layout: identical offsets, identical encoding, identical handshake
    // — but a master has no function code that can overwrite the block.
    void test_server_publishes_the_result_into_input_registers_when_configured()
    {
        const quint16 port = freePort();

        ModbusTcpServerDevice device(QStringLiteral("mb_server_ir"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_resultInInputRegisters = true;
        cfg.m_resultStartAddress = 1000;
        cfg.m_resultMaxPositions = 2;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        auto *master = new QModbusTcpClient(this);
        master->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                       QStringLiteral("127.0.0.1"));
        master->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
        master->setTimeout(2000);
        QVERIFY(connectPeer(master));

        VisionOutputPosition position;
        position.x = 125.50;
        position.y = -40.25;
        position.rz = 37.10;

        QString message;
        QVERIFY2(device.sendVisionResult({ position }, &message), qPrintable(message));

        // Read as INPUT registers (function 04), at the same offsets the contract publishes.
        const QList<quint16> header = readRegisters(
            master, cfg.m_unitId, QModbusDataUnit::InputRegisters,
            1000, ModbusResultLayout::kHeaderRegisters);
        QCOMPARE(header.size(), ModbusResultLayout::kHeaderRegisters);
        QCOMPARE(header.at(ModbusResultLayout::kOffsetCount), quint16(1));
        QCOMPARE(header.at(ModbusResultLayout::kOffsetSequence), quint16(1));

        const QList<quint16> payload = readRegisters(
            master, cfg.m_unitId, QModbusDataUnit::InputRegisters,
            1000 + ModbusResultLayout::kHeaderRegisters,
            ModbusResultLayout::kRegistersPerPosition);
        const QVector<VisionOutputPosition> decoded =
            ModbusResultLayout::decodePayload(payload, 1);
        QCOMPARE(decoded.size(), 1);
        QCOMPARE(decoded.at(0).x, 125.50);
        QCOMPARE(decoded.at(0).rz, 37.10);

        // The master cannot corrupt it — that is the reason to choose this layout.
        QVERIFY(!writeRegister(master, cfg.m_unitId, QModbusDataUnit::InputRegisters, 1000, 0));
        const QList<quint16> after =
            readRegisters(master, cfg.m_unitId, QModbusDataUnit::InputRegisters, 1000, 1);
        QCOMPARE(after.size(), 1);
        QCOMPARE(after.at(0), quint16(1));

        // And nothing was published into holding registers, where the default layout would be.
        // A PLC program pointed at 4x will not find a block that moved to 3x, and this asserts it
        // rather than leaving it as a claim in the doc.
        quint16 holding = 0xFFFF;
        QVERIFY(device.modbusConfig().resultArea() == ModbusArea::InputRegisters);
        if (readRegisters(master, cfg.m_unitId, QModbusDataUnit::HoldingRegisters, 1000, 1)
                .size() == 1) {
            holding = readRegisters(master, cfg.m_unitId,
                                    QModbusDataUnit::HoldingRegisters, 1000, 1).at(0);
            QCOMPARE(holding, quint16(0));
        }

        master->disconnectDevice();
        device.deviceDisconnect();
    }

    // A port already in use must fail at deviceConnect(), visibly. A server that silently did not
    // bind would look connected on the panel and never answer a master.
    void test_server_port_already_in_use_fails_visibly()
    {
        const quint16 port = freePort();

        QTcpServer squatter;
        QVERIFY2(squatter.listen(QHostAddress::LocalHost, port),
                 "could not occupy the port for the test");

        ModbusTcpServerDevice device(QStringLiteral("mb_server_busy"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        device.setDeviceConfig(&cfg);

        QSignalSpy failedSpy(&device, &IDevice::connectionFailed);
        QVERIFY(failedSpy.isValid());

        QVERIFY(!device.deviceConnect());
        QVERIFY(!device.isDeviceConnected());
        QCOMPARE(device.connectStatus(), ConnectStatus::ConnectFailed);
        QCOMPARE(failedSpy.count(), 1);
    }

    // B4 asks for multiple master connections to be *verified*, not assumed. This is what Qt
    // actually does: QModbusTcpServer accepts concurrent connections and serves each of them
    // against the one shared register space. Both masters below read the same published result.
    //
    // The consequence worth knowing, and now recorded in the contract doc: there is no per-master
    // isolation and no per-master handshake. Two masters consuming the same result block will
    // both see the same count and sequence, and neither can tell the other has already taken it.
    // A cell with two consumers needs its own arbitration; this layer will not provide one.
    void test_server_accepts_multiple_masters_against_one_register_space()
    {
        const quint16 port = freePort();

        ModbusTcpServerDevice device(QStringLiteral("mb_server_multi"), QStringLiteral("Server"));
        ModbusTcpServerCfg cfg;
        cfg.m_listenAddress = QStringLiteral("127.0.0.1");
        cfg.m_port = port;
        cfg.m_resultStartAddress = 1000;
        cfg.m_resultMaxPositions = 2;
        device.setDeviceConfig(&cfg);
        QVERIFY(device.deviceConnect());

        auto makeMaster = [this, port]() {
            auto *master = new QModbusTcpClient(this);
            master->setConnectionParameter(QModbusDevice::NetworkAddressParameter,
                                           QStringLiteral("127.0.0.1"));
            master->setConnectionParameter(QModbusDevice::NetworkPortParameter, port);
            master->setTimeout(2000);
            return master;
        };

        QModbusTcpClient *first = makeMaster();
        QModbusTcpClient *second = makeMaster();
        QVERIFY2(connectPeer(first), qPrintable(first->errorString()));
        QVERIFY2(connectPeer(second), qPrintable(second->errorString()));

        QString message;
        QVERIFY(device.sendVisionResult({ VisionOutputPosition() }, &message));

        const QList<quint16> firstRead =
            readRegisters(first, cfg.m_unitId, QModbusDataUnit::HoldingRegisters, 1000, 4);
        const QList<quint16> secondRead =
            readRegisters(second, cfg.m_unitId, QModbusDataUnit::HoldingRegisters, 1000, 4);

        QCOMPARE(firstRead.size(), 4);
        QCOMPARE(secondRead.size(), 4);
        QCOMPARE(firstRead, secondRead);
        QCOMPARE(firstRead.at(ModbusResultLayout::kOffsetCount), quint16(1));

        // A write from one master is visible to the other: one shared space, no isolation.
        QVERIFY(writeRegister(first, cfg.m_unitId, QModbusDataUnit::HoldingRegisters, 5, 42));
        const QList<quint16> crossRead =
            readRegisters(second, cfg.m_unitId, QModbusDataUnit::HoldingRegisters, 5, 1);
        QCOMPARE(crossRead.size(), 1);
        QCOMPARE(crossRead.at(0), quint16(42));

        first->disconnectDevice();
        second->disconnectDevice();
        device.deviceDisconnect();
    }

    // The pairing an integrator actually builds first: our own client device polling our own
    // server device, both left on their default register settings. If these two cannot talk to
    // each other out of the box, nothing else about the pair is trustworthy.
    void test_our_client_polls_our_server_on_default_settings()
    {
        const quint16 port = freePort();

        ModbusTcpServerDevice server(QStringLiteral("pair_server"), QStringLiteral("Server"));
        ModbusTcpServerCfg serverCfg;                    // defaults, except where to listen
        serverCfg.m_listenAddress = QStringLiteral("127.0.0.1");
        serverCfg.m_port = port;
        server.setDeviceConfig(&serverCfg);
        QVERIFY2(server.deviceConnect(), "server could not listen");

        ModbusTcpClientDevice client(QStringLiteral("pair_client"), QStringLiteral("Client"));
        ModbusTcpClientCfg clientCfg;                    // defaults, except where to dial
        clientCfg.m_hostAddress = QStringLiteral("127.0.0.1");
        clientCfg.m_port = port;
        clientCfg.m_refreshInterval = 20;
        client.setDeviceConfig(&clientCfg);

        QSignalSpy pollSpy(&client, &PlcDevice::pollingUpdate);
        QSignalSpy errorSpy(&client, &IDevice::connectionFailed);
        QVERIFY(pollSpy.isValid());
        QVERIFY(errorSpy.isValid());

        QVERIFY2(client.deviceConnect(), "client could not connect to our own server");

        // A completed poll pass means every configured area was read without an exception.
        QTRY_VERIFY_WITH_TIMEOUT(pollSpy.count() >= 2, 3000);
        QCOMPARE(client.connectStatus(), ConnectStatus::Connected);
        QCOMPARE(errorSpy.count(), 0);

        // And a value the server publishes reaches the client's map through a real poll.
        QVERIFY(server.writeWordIoByName(QStringLiteral("HR00012"), qint16(4242)));
        QSignalSpy valueSpy(&client, &PlcDevice::valueChanged);
        QVERIFY(valueSpy.isValid());
        QTRY_VERIFY_WITH_TIMEOUT(valueSpy.count() >= 1, 3000);

        QMap<QString, QVariant> changed;
        for (const QList<QVariant> &args : valueSpy) {
            const auto batch = args.at(0).value<QMap<QString, QVariant>>();
            for (auto it = batch.cbegin(); it != batch.cend(); ++it) {
                changed.insert(it.key(), it.value());
            }
        }
        QCOMPARE(changed.value(QStringLiteral("HR00012")).toInt(), 4242);

        client.deviceDisconnect();
        server.deviceDisconnect();
    }

    // The failure the logging exists for, reproduced on purpose: the client asks for a range the
    // server does not map, and the server answers IllegalDataAddress. This is what "connect
    // fails with a Modbus exception response" looks like when two devices are configured with
    // spans that do not agree — and the pass is abandoned at the first such range, so every
    // later area goes unread and the link eventually drops.
    void test_mismatched_spans_produce_an_illegal_data_address_exception()
    {
        const quint16 port = freePort();

        ModbusTcpServerDevice server(QStringLiteral("span_server"), QStringLiteral("Server"));
        ModbusTcpServerCfg serverCfg;
        serverCfg.m_listenAddress = QStringLiteral("127.0.0.1");
        serverCfg.m_port = port;
        serverCfg.m_coilCount = 8;              // the server maps only 8 coils …
        server.setDeviceConfig(&serverCfg);
        QVERIFY(server.deviceConnect());

        ModbusTcpClientDevice client(QStringLiteral("span_client"), QStringLiteral("Client"));
        ModbusTcpClientCfg clientCfg;
        clientCfg.m_hostAddress = QStringLiteral("127.0.0.1");
        clientCfg.m_port = port;
        clientCfg.m_coilCount = 64;             // … while the client polls 64
        clientCfg.m_refreshInterval = 20;
        clientCfg.m_responseTimeout = 500;
        clientCfg.m_retryCount = 1;
        client.setDeviceConfig(&clientCfg);
        QVERIFY(client.deviceConnect());

        // The link is declared lost once the retry budget is spent — the poll never completes.
        QTRY_COMPARE_WITH_TIMEOUT(client.connectStatus(), ConnectStatus::LostConnected, 5000);

        // The server refuses the read outright, so nothing was ever mirrored.
        QSignalSpy valueSpy(&client, &PlcDevice::valueChanged);
        QVERIFY(valueSpy.isValid());
        QCOMPARE(valueSpy.count(), 0);

        server.deviceDisconnect();
    }

    // Both devices must answer for the robot pick-check settings, because both can carry the
    // localization task's vision_output role (Phase 8/B1). A device that could send results but
    // not supply these settings would silently disable a commissioned safety gate.
    void test_both_modbus_devices_carry_the_result_output_capability()
    {
        ModbusTcpClientDevice client(QStringLiteral("cap_c"), QStringLiteral("Client"));
        ModbusTcpServerDevice server(QStringLiteral("cap_s"), QStringLiteral("Server"));

        QVERIFY(dynamic_cast<IResultOutputDevice *>(&client) != nullptr);
        QVERIFY(dynamic_cast<IResultOutputDevice *>(&server) != nullptr);
        QVERIFY(dynamic_cast<IPlcTagProvider *>(&client) != nullptr);
        QVERIFY(dynamic_cast<IPlcIoWriter *>(&server) != nullptr);

        RobotKinematicCheckConfig check;
        check.enabled = true;
        check.presetName = QStringLiteral("Nachi MZ04D");
        check.tcpZ = 120.0;

        client.setRobotKinematicCheckConfig(check);
        server.setRobotKinematicCheckConfig(check);

        QCOMPARE(client.robotKinematicCheckConfig().enabled, true);
        QCOMPARE(client.robotKinematicCheckConfig().tcpZ, 120.0);
        QCOMPARE(server.robotKinematicCheckConfig().presetName,
                 QStringLiteral("Nachi MZ04D"));
    }
};

QTEST_MAIN(ModbusDeviceTest)
#include <main.moc>
