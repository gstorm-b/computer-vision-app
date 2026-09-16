#ifndef MODBUS_TCP_SERVER_DEVICE_H
#define MODBUS_TCP_SERVER_DEVICE_H

/**
 * @file modbus_tcp_server_device.h
 * @brief Modbus TCP server PLC device: we are the slave, we own the register space and a PLC
 *        master reads and writes it.
 */

#include "device/device_capabilities.h"
#include "device/plc/modbus/modbus_register_map.h"
#include "device/plc/modbus/modbus_tcp_server_config.h"
#include "device/plc/plc_device.h"

#include <QModbusDataUnit>

#include <memory>

QT_BEGIN_NAMESPACE
class QModbusTcpConnectionObserver;
class QModbusTcpServer;
QT_END_NAMESPACE

namespace vc::device {

/**
 * @class ModbusTcpServerDevice
 * @brief PLC-family device hosting a `QModbusTcpServer`: a master polls us, and the values it
 *        writes become task signal changes exactly as polled values do on the client device.
 *
 * The register map, the tag names and the result layout are the same code the client uses, so a
 * PLC program written against one sub-type works against the other. What differs is only where
 * the values come from: the client asks, the server is told.
 *
 * @note **No poll timer and no blocking transactions.** Reads and writes are local memory
 *       operations on our own register space, so the whole bounded-wait machinery the client
 *       needs has no counterpart here. Change detection runs when a master writes, and when we
 *       publish a result.
 */
class ModbusTcpServerDevice : public PlcDevice,
                              public IPlcTagProvider,
                              public IPlcIoWriter,
                              public IResultOutputDevice {
    Q_OBJECT

public:
    explicit ModbusTcpServerDevice(QString id, QString name, QObject *parent = nullptr);
    ~ModbusTcpServerDevice() override;

    // ── IDevice ───────────────────────────────────────────────────────────────────────────────
    /// Binds the configured listen address/port and starts accepting masters.
    /// @return false if the port could not be bound — a port already in use fails here, visibly,
    ///         rather than leaving a device that looks connected and never answers.
    bool deviceConnect() override;
    /// Stops listening and publishes ConnectStatus::Disconnected.
    bool deviceDisconnect() override;
    bool isDeviceConnected() const override {
        return connectStatus() == ConnectStatus::Connected;
    }

    PlcType plcType() const override { return PlcType::ModbusTcpServer; }

    /// Applies `cfg` (must be a ModbusTcpServerCfg) and rebuilds the register space.
    /// No-op while listening: the register space is already published to connected masters.
    void setDeviceConfig(IDeviceCfg *cfg) override;

    /// Returns a copy of the current configuration.
    ModbusTcpServerCfg modbusConfig() const { return m_config; }

    bool fromJson(const QJsonObject &obj) override;

    /// Not used: results arrive through IResultOutputDevice, IO through IPlcIoWriter.
    bool pushRequest(IRequest *request) override;

    // ── IPlcTagProvider / IPlcIoWriter ────────────────────────────────────────────────────────
    QStringList availableDigitalIoNames() const override;
    QStringList availableWordIoNames() const override;
    /// Writes a coil in our own register space, where a connected master can read it.
    bool writeDigitalIoByName(const QString &tag, bool value) override;
    /// Writes a holding register in our own register space.
    bool writeWordIoByName(const QString &tag, qint16 value) override;

    // ── IResultOutputDevice ───────────────────────────────────────────────────────────────────
    /// Publishes `positions` into our holding registers, following the same count-last handshake
    /// the client device uses so one PLC program serves both.
    bool sendVisionResult(const QVector<VisionOutputPosition> &positions,
                          QString *message) override;
    RobotKinematicCheckConfig robotKinematicCheckConfig() const override {
        return m_kinematicCheck;
    }
    /// Sets the pick-check settings commissioned on this device.
    void setRobotKinematicCheckConfig(const RobotKinematicCheckConfig &check) {
        m_kinematicCheck = check;
    }
    /// Flags the next result publish as "matcher reported low area".
    void setResultLowArea(bool lowArea) { m_lowArea = lowArea; }

public slots:
    void deviceTerminate() override;

private slots:
    /// A master wrote to our register space: mirror the values into the register map and publish
    /// the change, so a master write and a client poll reach the task through the same path.
    void onDataWritten(QModbusDataUnit::RegisterType table, int address, int size);

private:
    /// Rebuilds the register map from the current config.
    void rebuildRegisterMap();
    /// Builds the register space handed to QModbusTcpServer: the configured spans, with the
    /// result block's own area widened to cover it wherever the operator put it.
    void applyRegisterSpace();
    /// Returns the span the server must own for `area`: the configured span, unioned with the
    /// result block when the result lives in that area.
    ModbusRange servedRange(ModbusArea area) const;
    /// Writes registers of `area` into our own space and mirrors them into the register map.
    /// @note `area` must be a word area; the caller decides which, because the result block may
    ///       live in holding or in input registers.
    bool writeServedRegisters(ModbusArea area, int address, const QList<quint16> &values,
                              QString *error);
    /// Returns true when the caller is on this device's own thread, logging and refusing if not.
    /// @note QModbusTcpServer owns thread-affine sockets; setData() from another thread neither
    ///       fails nor takes effect reliably. See the note in the .cpp.
    bool assertOnDeviceThread(const QString &what) const;
    /// Publishes changed tags and a map snapshot.
    void publishChanges();

    ModbusTcpServerCfg m_config;   ///< Current configuration.
    ModbusRegisterMap m_map;       ///< Served values, tag names and change detection.
    RobotKinematicCheckConfig m_kinematicCheck;  ///< Pick-check settings for the vision-output role.

    QModbusTcpServer *m_server{nullptr};  ///< Owned; created on the worker thread by deviceConnect().
    /// @note The connection observer is deliberately NOT a member. `installConnectionObserver()`
    ///       **takes ownership** — Qt holds it in a QScopedPointer and deletes it with the server
    ///       — so keeping our own owning handle to it is a double free. It cost a heap-corruption
    ///       crash to find that out; a fresh observer is handed over on every connect instead.
    quint16 m_resultSequence{0};          ///< Last published result sequence number.
    bool m_lowArea{false};                ///< Low-area flag for the next result publish.
};

} // namespace vc::device

#endif // MODBUS_TCP_SERVER_DEVICE_H
