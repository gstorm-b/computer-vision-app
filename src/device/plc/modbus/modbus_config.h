#ifndef MODBUS_CONFIG_H
#define MODBUS_CONFIG_H

/**
 * @file modbus_config.h
 * @brief Configuration for the Modbus PLC devices: the register map and result layout shared by
 *        both sub-types, plus the connection settings that differ between them.
 *
 * Three classes across three headers, following the family pattern
 * (`vision_tcpip_config.h` / `vision_tcpip_client_config.h`):
 *   - `ModbusConfig`       (here) — everything about the register space and the result block.
 *   - `ModbusTcpClientCfg`        — plus host/port/timing, because we dial out and poll.
 *   - `ModbusTcpServerCfg`        — plus listen address/port, because we are dialled into.
 *
 * The split between client and server is deliberate rather than one config with a mode flag: a
 * poll interval means nothing to a server that publishes on write, and a listen address means
 * nothing to a client. One class carrying both would put fields on a property browser that do
 * nothing, and a field that appears configurable but is ignored is worse than an absent one.
 *
 * The split across three *headers* has a second reason: the architecture contract test reads each
 * header and requires every QT_TRANSLATE_NOOP context in it to name that header's class, so two
 * marker tables in one file cannot both be right.
 */

#include "core/qgadget_macro.h"
#include "device/plc/plc_device.h"
#include "device/plc/modbus/modbus_register_map.h"
#include "device/plc/modbus/modbus_result_layout.h"

#include <QString>

namespace vc::device {

/**
 * @class ModbusConfig
 * @brief Register-space and result-block settings shared by both Modbus sub-types.
 *
 * @note The result block defaults to address 1000, deliberately clear of the default holding
 *       range (0..63). Overlapping them is allowed but rarely wanted: the task would poll back
 *       the words the vision result just wrote, filling the signal map with encoded pose halves.
 *       Use overlapsPolledHoldingRange() to warn rather than to forbid — a customer PLC may
 *       genuinely want the result inside a block it already reads.
 */
class ModbusConfig : public PlcCfg {
    Q_GADGET

    G_PROPERTY_NUMBER_READWRITE(int, unitId, 1, 247, "Unit / Slave Id")

    // Each label carries its TAG PREFIX, because the prefix is what the operator has to type into
    // the signal map and the two vocabularies used to share no word at all: "Discrete Input" is
    // DI, "Holding Register" is HR, "Input Register" is IR. Worse, "Input Register" and "Discrete
    // Input" both contain "Input", which is how someone reaches for a DI tag when they wanted IR.
    G_PROPERTY_NUMBER_READWRITE(int, coilStart, 0, 65535, "Coil (COIL) Start")
    G_PROPERTY_NUMBER_READWRITE(int, coilCount, 0, 2000, "Coil (COIL) Count")
    G_PROPERTY_NUMBER_READWRITE(int, discreteInputStart, 0, 65535, "Discrete Input (DI) Start")
    G_PROPERTY_NUMBER_READWRITE(int, discreteInputCount, 0, 2000, "Discrete Input (DI) Count")
    G_PROPERTY_NUMBER_READWRITE(int, holdingStart, 0, 65535, "Holding Register (HR) Start")
    G_PROPERTY_NUMBER_READWRITE(int, holdingCount, 0, 1000, "Holding Register (HR) Count")
    G_PROPERTY_NUMBER_READWRITE(int, inputRegisterStart, 0, 65535, "Input Register (IR) Start")
    G_PROPERTY_NUMBER_READWRITE(int, inputRegisterCount, 0, 1000, "Input Register (IR) Count")

    G_PROPERTY_NUMBER_READWRITE(int, resultStartAddress, 0, 65535, "Result Start Address")
    G_PROPERTY_NUMBER_READWRITE(int, resultMaxPositions, 0, 64, "Result Max Positions")

    /// Logs every Modbus request and reply, not only the failures. Off by default because a
    /// 100 ms poll over four areas is forty log lines a second, forever. Turning it on is the
    /// commissioning tool for "why is this peer answering that?"; **failures are logged in full
    /// either way**, so a fault does not need to be reproduced with tracing enabled.
    G_PROPERTY_BOOL_READWRITE(bool, traceProtocol, "Protocol Trace")

    /// Translation markers for the "_name" labels the macros above emit as Q_CLASSINFO.
    /// lupdate cannot see Q_CLASSINFO, so without this table these labels reach the UI in
    /// English and can never be translated, with no build error. The context must be spelled
    /// exactly as this class's className(); the architecture contract test asserts both.
public:
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Unit / Slave Id"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Coil (COIL) Start"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Coil (COIL) Count"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Discrete Input (DI) Start"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Discrete Input (DI) Count"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Holding Register (HR) Start"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Holding Register (HR) Count"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Input Register (IR) Start"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Input Register (IR) Count"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Result Start Address"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Result Max Positions"),
        QT_TRANSLATE_NOOP("vc::device::ModbusConfig", "Protocol Trace"),
    };

public:
    ~ModbusConfig() override = default;

    /// Returns the configured span for `area`, as the register map wants it.
    ModbusRange range(ModbusArea area) const {
        switch (area) {
        case ModbusArea::Coils:            return { m_coilStart, m_coilCount };
        case ModbusArea::DiscreteInputs:   return { m_discreteInputStart, m_discreteInputCount };
        case ModbusArea::HoldingRegisters: return { m_holdingStart, m_holdingCount };
        case ModbusArea::InputRegisters:   return { m_inputRegisterStart, m_inputRegisterCount };
        }
        return {};
    }

    /// Applies all four configured spans to `map`, discarding whatever it held.
    void configureRegisterMap(ModbusRegisterMap &map) const {
        map.configure(range(ModbusArea::Coils),
                      range(ModbusArea::DiscreteInputs),
                      range(ModbusArea::HoldingRegisters),
                      range(ModbusArea::InputRegisters));
    }

    /**
     * @brief Returns the word area the vision result block lives in.
     * @return HoldingRegisters by default.
     *
     * A **client** has no choice: it publishes by writing to the slave, and the protocol gives a
     * master no way to write input registers. A **server** owns its space and may publish into
     * input registers instead — see ModbusTcpServerCfg, which overrides this.
     */
    virtual ModbusArea resultArea() const { return ModbusArea::HoldingRegisters; }

    /// Returns the result block as a span within resultArea().
    ModbusRange resultRange() const {
        return { m_resultStartAddress,
                 ModbusResultLayout::registerCount(m_resultMaxPositions) };
    }

    /// Returns whether the result block shares any address with the mapped span of the area it
    /// lives in. Overlap is allowed but rarely wanted; see the class note.
    bool overlapsPolledResultRange() const {
        return resultRange().overlaps(range(resultArea()));
    }

    /// Returns whether a full result publish fits in a single Modbus write request. Above the
    /// limit the publish spans two requests, which is why the count word is written last.
    bool resultFitsSingleRequest() const {
        return m_resultMaxPositions <= ModbusResultLayout::kMaxPositionsInSingleRequest;
    }

    /// Serializes the family PlcType key plus every shared field.
    QJsonObject toJson() const override {
        QJsonObject obj = PlcCfg::toJson();
        obj[DEVICE_JSK_MB_UNIT_ID]         = m_unitId;
        obj[DEVICE_JSK_MB_COIL_START]      = m_coilStart;
        obj[DEVICE_JSK_MB_COIL_COUNT]      = m_coilCount;
        obj[DEVICE_JSK_MB_DISCRETE_START]  = m_discreteInputStart;
        obj[DEVICE_JSK_MB_DISCRETE_COUNT]  = m_discreteInputCount;
        obj[DEVICE_JSK_MB_HOLDING_START]   = m_holdingStart;
        obj[DEVICE_JSK_MB_HOLDING_COUNT]   = m_holdingCount;
        obj[DEVICE_JSK_MB_INPUT_REG_START] = m_inputRegisterStart;
        obj[DEVICE_JSK_MB_INPUT_REG_COUNT] = m_inputRegisterCount;
        obj[DEVICE_JSK_MB_RESULT_START]    = m_resultStartAddress;
        obj[DEVICE_JSK_MB_RESULT_MAX_POS]  = m_resultMaxPositions;
        obj[DEVICE_JSK_MB_TRACE_PROTOCOL]  = m_traceProtocol;
        return obj;
    }

    /// Restores the shared fields; any missing key keeps the current (default) value.
    /// @return false if the PlcCfg type check fails
    bool fromJson(const QJsonObject &obj) override {
        if (!PlcCfg::fromJson(obj)) {
            return false;
        }
        m_unitId             = obj[DEVICE_JSK_MB_UNIT_ID].toInt(m_unitId);
        m_coilStart          = obj[DEVICE_JSK_MB_COIL_START].toInt(m_coilStart);
        m_coilCount          = obj[DEVICE_JSK_MB_COIL_COUNT].toInt(m_coilCount);
        m_discreteInputStart = obj[DEVICE_JSK_MB_DISCRETE_START].toInt(m_discreteInputStart);
        m_discreteInputCount = obj[DEVICE_JSK_MB_DISCRETE_COUNT].toInt(m_discreteInputCount);
        m_holdingStart       = obj[DEVICE_JSK_MB_HOLDING_START].toInt(m_holdingStart);
        m_holdingCount       = obj[DEVICE_JSK_MB_HOLDING_COUNT].toInt(m_holdingCount);
        m_inputRegisterStart = obj[DEVICE_JSK_MB_INPUT_REG_START].toInt(m_inputRegisterStart);
        m_inputRegisterCount = obj[DEVICE_JSK_MB_INPUT_REG_COUNT].toInt(m_inputRegisterCount);
        m_resultStartAddress = obj[DEVICE_JSK_MB_RESULT_START].toInt(m_resultStartAddress);
        m_resultMaxPositions = obj[DEVICE_JSK_MB_RESULT_MAX_POS].toInt(m_resultMaxPositions);
        m_traceProtocol      = obj[DEVICE_JSK_MB_TRACE_PROTOCOL].toBool(m_traceProtocol);
        return true;
    }

public:
    int m_unitId{1};                 ///< Modbus unit / slave id.
    int m_coilStart{0};              ///< First coil address polled/served.
    int m_coilCount{64};             ///< Number of coils; 0 disables the area.
    int m_discreteInputStart{0};     ///< First discrete-input address.
    int m_discreteInputCount{64};    ///< Number of discrete inputs; 0 disables the area.
    int m_holdingStart{0};           ///< First holding-register address.
    int m_holdingCount{64};          ///< Number of holding registers; 0 disables the area.
    int m_inputRegisterStart{0};     ///< First input-register address.
    int m_inputRegisterCount{64};    ///< Number of input registers; 0 disables the area.

    /// First holding register of the vision-result block. Clear of the default polled holding
    /// range on purpose; see the class note.
    int m_resultStartAddress{1000};
    /// Capacity of the result block, in positions. Defaults to 8 so the whole block (4 + 8×12 =
    /// 100 registers) fits one Modbus write request; see
    /// ModbusResultLayout::kMaxPositionsInSingleRequest.
    int m_resultMaxPositions{8};

    /// Log every request and reply, not only failures. See the property comment above.
    bool m_traceProtocol{false};
};

} // namespace vc::device

#endif // MODBUS_CONFIG_H
