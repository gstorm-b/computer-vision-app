#ifndef MODBUS_TCP_CLIENT_DEVICE_H
#define MODBUS_TCP_CLIENT_DEVICE_H

/**
 * @file modbus_tcp_client_device.h
 * @brief Modbus TCP client PLC device: we are the master, we dial a slave and poll it.
 */

#include "device/device_capabilities.h"
#include "device/plc/modbus/modbus_register_map.h"
#include "device/plc/modbus/modbus_tcp_client_config.h"
#include "device/plc/plc_device.h"

#include <QList>
#include <QPointer>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QModbusTcpClient;
class QModbusReply;
class QModbusDataUnit;
QT_END_NAMESPACE

namespace vc::device {

/**
 * @class ModbusTcpClientDevice
 * @brief PLC-family device driving a `QModbusTcpClient`: polls the configured coil / discrete
 *        input / holding / input-register spans, serves tag reads and writes for IO mapping, and
 *        publishes localization results into the slave's holding registers.
 *
 * Meets the same three contracts `McProtocolDevice` does, because the runtime cannot tell the two
 * apart: `valueChanged()` carries the changed tags, `pollingUpdate()` carries a snapshot, and a
 * lost link publishes `ConnectStatus::LostConnected` so the recovery policy starts reconnecting.
 *
 * @note **Every Modbus transaction on this device is a bounded blocking wait on the device's own
 *       worker thread.** Qt's Modbus API is asynchronous only, and the alternatives to waiting are
 *       both worse: an async result publish cannot honour `IResultOutputDevice::sendVisionResult()`'s
 *       synchronous contract, and interleaving a poll with the publish sequence would let a master
 *       observe a half-written result block. Waiting here costs nothing the GUI can feel — the
 *       runner already marshals every call onto this thread — and it makes the publish a strict,
 *       readable sequence. Each wait is bounded by the configured response timeout.
 */
class ModbusTcpClientDevice : public PlcDevice,
                              public IPlcTagProvider,
                              public IPlcIoWriter,
                              public IResultOutputDevice {
    Q_OBJECT

public:
    /// Constructs the device; the QModbusTcpClient is created on the worker thread by
    /// deviceConnect(), not here, because it must live on the thread that drives it.
    explicit ModbusTcpClientDevice(QString id, QString name, QObject *parent = nullptr);
    ~ModbusTcpClientDevice() override;

    // ── IDevice ───────────────────────────────────────────────────────────────────────────────
    /// Creates the Modbus client, connects to the configured slave and starts polling.
    /// @return false if the connection could not be started
    bool deviceConnect() override;
    /// Stops polling and tears the client down, publishing ConnectStatus::Disconnected.
    bool deviceDisconnect() override;
    bool isDeviceConnected() const override {
        return connectStatus() == ConnectStatus::Connected;
    }

    PlcType plcType() const override { return PlcType::ModbusTcpClient; }

    /// Applies `cfg` (must be a ModbusTcpClientCfg) and rebuilds the register map.
    /// No-op while connected: the register spans define the poll plan currently in flight.
    void setDeviceConfig(IDeviceCfg *cfg) override;

    /// Returns a copy of the current configuration.
    ModbusTcpClientCfg modbusConfig() const { return m_config; }

    /// Restores from JSON, then rebuilds the register map to match.
    bool fromJson(const QJsonObject &obj) override;

    /// Not used by this device: results arrive through IResultOutputDevice, IO through
    /// IPlcIoWriter. Always returns false rather than silently accepting a request nothing
    /// will ever send.
    bool pushRequest(IRequest *request) override;

    // ── IPlcTagProvider / IPlcIoWriter ────────────────────────────────────────────────────────
    QStringList availableDigitalIoNames() const override;
    QStringList availableWordIoNames() const override;
    /**
     * @brief Writes a coil.
     *
     * Fails, with a logged reason, for a discrete-input tag, an address outside the configured
     * span, a caller on the wrong thread, or while disconnected. **A failed write is never
     * retried and never queued**: replaying it when the link returns would fire at an
     * unpredictable moment against a machine that has moved on.
     *
     * @warning **One case IS deferred, and it is not a failure.** A well-formed write that arrives
     * while a transaction already holds the wire is parked and replayed as soon as that
     * transaction unwinds. This is not the queue the rule above forbids — the write has not been
     * attempted, nothing has failed, and the wire is healthy. It exists because transact() waits
     * in a nested event loop that still dispatches the queued meta-calls PlcRunner posts for every
     * other write, so a burst of publishes lands inside the first one's wait. These used to be
     * refused one layer down in transact(), which silently dropped nine writes out of ten in a
     * ten-signal burst *and* charged each refusal to the link retry budget until the client
     * disconnected itself.
     *
     * @return true when the write reached the wire, or was accepted for replay.
     */
    bool writeDigitalIoByName(const QString &tag, bool value) override;
    /// Writes a holding register. Same failure and deferral rules as writeDigitalIoByName().
    bool writeWordIoByName(const QString &tag, qint16 value) override;

    // ── IResultOutputDevice ───────────────────────────────────────────────────────────────────
    /// Publishes `positions` into the slave's holding registers per the result contract:
    /// count cleared, payload written, sequence/flags written, count written last.
    bool sendVisionResult(const QVector<VisionOutputPosition> &positions,
                          QString *message) override;
    /// Returns the pick-check settings commissioned on this device.
    RobotKinematicCheckConfig robotKinematicCheckConfig() const override {
        return m_kinematicCheck;
    }

    /// Sets the pick-check settings. Held on the device rather than only in the config so the
    /// capability can answer without a config clone on every cycle.
    void setRobotKinematicCheckConfig(const RobotKinematicCheckConfig &check) {
        m_kinematicCheck = check;
    }

    /// Flags the next result publish as "matcher reported low area".
    void setResultLowArea(bool lowArea) { m_lowArea = lowArea; }

public slots:
    void deviceTerminate() override;

private slots:
    /// One polling pass over every configured span; skipped while a transaction is in flight.
    void onPollTimeout();

private:
    /// A single read request in the round-robin poll plan.
    struct ReadChunk {
        ModbusArea area{ModbusArea::Coils};
        int start{0};
        int count{0};
    };

    /// Rebuilds the register map and the poll plan from the current config.
    void rebuildRegisterMap();
    /// Splits the configured spans into requests that respect the Modbus per-request ceilings.
    void rebuildPollPlan();

    /// Sends one request and waits, bounded by the configured response timeout.
    /// @param[in]  unit  the data unit to read or write
    /// @param[in]  write true to send a write request, false for a read
    /// @param[out] reply the completed unit for a read; may be null
    /// @param[out] error human-readable failure reason; may be null
    /// @return false on send failure, timeout, or a protocol/exception error
    bool transact(const QModbusDataUnit &unit, bool write,
                  QModbusDataUnit *reply, QString *error);

    /// Returns true when the caller is on this device's own thread, logging and refusing if not.
    /// @note QModbusTcpClient owns a thread-affine QTcpSocket; a request sent from another thread
    ///       is not rejected by Qt, it simply never reaches the wire. See the note in the .cpp.
    bool assertOnDeviceThread(const QString &what) const;

    /// Counts a failed transaction; publishes LostConnected once the retry budget is spent.
    void noteTransactionFailure(const QString &reason);
    /// Clears the failure counter after a successful transaction.
    void noteTransactionSuccess();

    /**
     * @brief Replays writes that were deferred while a transaction held the wire.
     *
     * Called at the end of every transaction. Re-entrant calls are no-ops: the loop here re-reads
     * the queue, so a write that arrives inside a replayed write's own nested wait is picked up by
     * this same loop rather than by a nested drain.
     *
     * @warning Does nothing while sendVisionResult() is running. Its four register writes must
     *          reach the wire consecutively — a master reading the result block between them would
     *          see a half-written result with a stale sequence word, which is the interleaving the
     *          poll timer is already stopped to prevent.
     */
    void drainPendingIoWrites();

public:
    /// Tracked digital write. Overrides PlcDevice's synchronous default because this family
    /// **defers**: a write that arrives inside another transaction's nested wait is parked, and
    /// resolving it at submission would report an outcome for a write not yet attempted.
    void writeDigitalIoTracked(quint64 id, const QString &tag, bool value) override;
    /// Word counterpart of writeDigitalIoTracked(); same deferred contract.
    void writeWordIoTracked(quint64 id, const QString &tag, qint16 value) override;

private:
    /// Fails every parked write that will never be replayed, with `reason`.
    void failPendingIoWrites(const QString &reason);

    /// Tears down the client and publishes `finalStatus`. Deferred if a transaction is in
    /// flight, so a nested wait can never delete the reply it is waiting on.
    void teardown(ConnectStatus finalStatus);

    /// Publishes changed tags and a map snapshot, exactly as McProtocolDevice does.
    void publishPollResults();

    ModbusTcpClientCfg m_config;        ///< Current configuration.
    ModbusRegisterMap m_map;            ///< Polled values, tag names and change detection.
    RobotKinematicCheckConfig m_kinematicCheck;  ///< Pick-check settings for the vision-output role.

    QModbusTcpClient *m_client{nullptr};  ///< Owned; created on the worker thread by deviceConnect().
    QTimer *m_pollTimer{nullptr};         ///< Drives onPollTimeout(); created once, reused.
    QList<ReadChunk> m_pollPlan;          ///< Read requests covering the configured spans.

    bool m_inTransaction{false};      ///< True while a bounded wait is running.
    /// One-shot: the last transact() failure was our own scheduler colliding with itself, not the
    /// link failing. Set only by the in-flight rejection, and consumed by the very next
    /// noteTransactionFailure() — which is safe because every transact() call site calls that
    /// immediately on failure, with no exceptions.
    bool m_lastFailureWasLocalCollision{false};

    /// One tag write deferred because a transaction was already in flight when it arrived.
    struct PendingIoWrite {
        bool isBit;        ///< True for a coil write, false for a holding-register write.
        QString tag;       ///< The tag as the caller gave it; re-validated on replay.
        bool bitValue;     ///< Value for a coil write.
        qint16 wordValue;  ///< Value for a register write.
        /// Completion id, or 0 for an untracked write. Carried through the park so the outcome
        /// reported is the one the REPLAY produced, not the "queued" the caller saw — which is
        /// exactly the distinction "true from writeDigitalIoByName" could never make.
        quint64 trackedId{0};
    };
    /// Writes that arrived inside another transaction's nested wait, in arrival order.
    QList<PendingIoWrite> m_pendingIoWrites;
    /// Completion id for the write about to enter writeDigitalIoByName()/writeWordIoByName(),
    /// set by the tracked wrappers and consumed at the top of those methods. A member rather
    /// than a parameter so the two IPlcIoWriter signatures stay untouched for their existing
    /// callers; E3 replaces this with the interface change.
    quint64 m_incomingTrackedId{0};
    /// Reads and clears m_incomingTrackedId. Called once, first thing, by each write method.
    quint64 takeIncomingTrackedId() {
        const quint64 id = m_incomingTrackedId;
        m_incomingTrackedId = 0;
        return id;
    }
    bool m_draining{false};           ///< True while drainPendingIoWrites() is replaying.
    /// True while sendVisionResult() holds the wire. Its four writes must not be interleaved.
    bool m_suppressPendingDrain{false};
    bool m_teardownPending{false};    ///< A disconnect arrived during a transaction.
    ConnectStatus m_pendingStatus{ConnectStatus::Disconnected};  ///< Status the deferred teardown will publish.
    int m_consecutiveFailures{0};     ///< Failed transactions since the last success.
    quint16 m_resultSequence{0};      ///< Last published result sequence number.
    bool m_lowArea{false};            ///< Low-area flag for the next result publish.
};

} // namespace vc::device

#endif // MODBUS_TCP_CLIENT_DEVICE_H
