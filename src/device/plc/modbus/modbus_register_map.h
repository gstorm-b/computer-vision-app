#ifndef MODBUS_REGISTER_MAP_H
#define MODBUS_REGISTER_MAP_H

/**
 * @file modbus_register_map.h
 * @brief Modbus register storage, tag naming and change detection, shared by the client
 *        (we are master) and the server (we are slave) devices.
 *
 * One implementation for both sides on purpose: the register space, the tag names the task
 * signal map is written against, and the change detection that feeds `valueChanged()` are the
 * same question whether the values arrived because we polled a slave or because a master wrote
 * them to us. Only the Qt object underneath and the connect/listen lifecycle differ.
 */

#include "device/plc/plc_device.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <map>

namespace vc::device {

/**
 * @enum ModbusArea
 * @brief The four Modbus data areas. Two are bit-addressed and two are word-addressed, and
 *        two of the four are read-only *by the protocol* — a master cannot write a discrete
 *        input or an input register at all.
 */
enum class ModbusArea {
    Coils,             ///< Bit, read/write. Modbus "0x" references.
    DiscreteInputs,    ///< Bit, read-only. Modbus "1x" references.
    HoldingRegisters,  ///< 16-bit word, read/write. Modbus "4x" references.
    InputRegisters,    ///< 16-bit word, read-only. Modbus "3x" references.
};

/// The four areas in the fixed order used everywhere a listing is produced (tag lists, UI
/// monitors, JSON). Iteration order is part of what makes tag lists stable across sessions.
inline constexpr ModbusArea kModbusAreas[] = {
    ModbusArea::Coils,
    ModbusArea::DiscreteInputs,
    ModbusArea::HoldingRegisters,
    ModbusArea::InputRegisters,
};

/**
 * @namespace vc::device::modbus_tags
 * @brief Tag naming for Modbus addresses.
 *
 * @warning **These names are persisted in task signal maps.** A task's `bExecuteTrigger` is
 *          stored as the literal string `"COIL00100"`, so changing the prefix or the padding
 *          silently unbinds every commissioned signal in every saved project. The format is
 *          `<prefix><5-digit zero-padded decimal address>`, matching the Mitsubishi family's
 *          `M0100`/`D0100` convention but at five digits because Modbus addresses run to 65535.
 *
 * @note The prefixes deliberately do **not** collide with the Mitsubishi family's `M`/`D`.
 *       If they did, a project switching PLC vendors would find some tags by accident and map
 *       them to completely unrelated registers — half-working silently, which is far worse than
 *       failing to resolve. With disjoint prefixes a vendor switch fails loudly on every tag.
 */
namespace modbus_tags {

/// Width of the zero-padded decimal address in a tag name, e.g. the `01000` of `HR01000`.
///
/// Exposed so the UI can spell an address exactly as format() does. A panel that pads to its own
/// width prints a string the operator cannot type: `COIL0100` is not the tag `COIL00100`, and the
/// signal map binds nothing.
constexpr int kAddressDigits = 5;

/// Tag prefix for `area` (`COIL`, `DI`, `HR`, `IR`).
QString prefix(ModbusArea area);

/// Formats `address` in `area` as a tag name, e.g. `HR01000`.
QString format(ModbusArea area, int address);

/// Parses a tag name produced by format().
/// @param[in]  tag     the tag name to parse
/// @param[out] area    the parsed area; may be null
/// @param[out] address the parsed address; may be null
/// @return false if `tag` does not match any area prefix or carries a non-numeric address
bool parse(const QString &tag, ModbusArea *area, int *address);

/// Returns whether `area` holds bits (Coils, DiscreteInputs) rather than 16-bit words.
bool isBitArea(ModbusArea area);

/**
 * @brief Returns whether a Modbus **master** can write `area`.
 *
 * False for discrete inputs and input registers: the protocol defines no write function code for
 * them at all (01/05/15 for coils, 03/06/16 for holding registers — nothing for 1x or 3x). This
 * is the specification, not a policy of ours.
 */
bool isMasterWritable(ModbusArea area);

/**
 * @brief Returns whether a Modbus **server** can write `area` — true for all four.
 *
 * The server owns the register space. "Input" in *discrete input* and *input register* is named
 * from the server's point of view: data entering the Modbus data model from the server's own
 * process, which the master may only read.
 *
 * @warning Do not use isMasterWritable() to decide what the server may write. That mistake made
 *          our server refuse to publish into the two areas it is the *only* legitimate writer of,
 *          and cost the safest server layout there is — vision results in input registers, where a
 *          master physically cannot overwrite them. The two predicates exist separately so the
 *          question "who is asking?" has to be answered at every call site.
 */
bool isServerWritable(ModbusArea area);

} // namespace modbus_tags

/**
 * @struct ModbusRange
 * @brief A contiguous span of addresses within one area: `count` addresses from `start`.
 *        An empty range (count 0) means the area is not used at all.
 */
struct ModbusRange {
    int start{0};  ///< First address in the span.
    int count{0};  ///< Number of addresses in the span; 0 disables the area.

    /// Returns the last address in the span, or `start - 1` when the range is empty.
    int last() const { return start + count - 1; }
    /// Returns whether `address` falls inside this span.
    bool contains(int address) const {
        return count > 0 && address >= start && address <= last();
    }
    /// Returns whether this span shares at least one address with `other`.
    bool overlaps(const ModbusRange &other) const {
        return count > 0 && other.count > 0 && start <= other.last() && other.start <= last();
    }
    bool operator==(const ModbusRange &other) const {
        return start == other.start && count == other.count;
    }
};

/**
 * @class ModbusRegisterMap
 * @brief The polled/served Modbus register space plus the change detection that drives
 *        `PlcDevice::valueChanged()`.
 *
 * Holds one value per configured address in each of the four areas, the tag names for those
 * addresses, and a shadow copy used to report only what actually changed. `PlcValueMap` so a
 * snapshot can ride `pollingUpdate()` across threads exactly as `McDeviceMap` does.
 */
class ModbusRegisterMap : public PlcValueMap {
public:
    /// Implements PlcValueMap::clone() via the copy constructor.
    std::shared_ptr<PlcValueMap> clone() const override {
        return std::make_shared<ModbusRegisterMap>(*this);
    }

    /// Reads a stored value by tag name (`COIL00000`, `DI00002`, `HR00101`, `IR00001`); see
    /// PlcValueMap::valueForTag(). An address outside the configured span answers false, which
    /// is the same answer as "never polled" and the correct one: nothing is known about it.
    bool valueForTag(const QString &tag, QVariant *value) const override;

    /**
     * @brief (Re)declares which addresses exist, discarding all stored values.
     *
     * Every configured address is created with a zero value, so `digitalTagNames()` and
     * `wordTagNames()` answer completely before the first poll — the signal-map editor needs
     * the full tag list at commissioning time, when nothing has been read yet.
     *
     * @post The first takeChangedValues() after configure() reports nothing: there is no
     *       previous state to have changed from, and reporting the whole map as "changed"
     *       would fire every task signal's rising edge at connect.
     */
    void configure(const ModbusRange &coils,
                   const ModbusRange &discreteInputs,
                   const ModbusRange &holdingRegisters,
                   const ModbusRange &inputRegisters);

    /// Returns the configured span for `area`.
    const ModbusRange &range(ModbusArea area) const;

    /// Returns whether `address` is inside the configured span for `area`.
    bool isConfigured(ModbusArea area, int address) const {
        return range(area).contains(address);
    }

    /// Stores a bit value. Ignored (returns false) for a word area or an unconfigured address.
    bool setBit(ModbusArea area, int address, bool value);
    /// Reads a stored bit; returns false and sets `*ok` to false for an unknown address.
    bool bit(ModbusArea area, int address, bool *ok = nullptr) const;

    /// Stores a raw register value. Ignored (returns false) for a bit area or an unconfigured
    /// address.
    bool setWord(ModbusArea area, int address, quint16 value);
    /// Reads a stored register; returns 0 and sets `*ok` to false for an unknown address.
    quint16 word(ModbusArea area, int address, bool *ok = nullptr) const;

    /// Bulk store starting at `address`, one entry per element. Values landing outside the
    /// configured span are dropped.
    /// @return the number of values actually stored
    int setBits(ModbusArea area, int address, const QList<bool> &values);
    /// Bulk store starting at `address`, one entry per element.
    /// @return the number of values actually stored
    int setWords(ModbusArea area, int address, const QList<quint16> &values);

    /// Returns the tag names of every configured bit address (coils, then discrete inputs).
    QStringList digitalTagNames() const { return m_digitalTagNames; }
    /// Returns the tag names of every configured word address (holding, then input registers).
    QStringList wordTagNames() const { return m_wordTagNames; }

    /**
     * @brief Returns the values that changed since the previous call, keyed by tag name, and
     *        adopts the current values as the new baseline.
     *
     * @return bit values as `bool`; **word values as `qint16`, not `quint16`**.
     * @note The signed reinterpretation is deliberate and matches the Mitsubishi family: a task
     *       number signal is a signed 16-bit quantity throughout this application, so a holding
     *       register carrying 40000 reaches the task signal map as -25536. A PLC programmer
     *       writing a raw count above 32767 into a register the task reads will see it that way;
     *       see the result-contract doc.
     */
    QMap<QString, QVariant> takeChangedValues();

    /// Drops the change baseline so the next takeChangedValues() reports nothing. For the
    /// **client**, where the first poll after connect brings the slave's existing state and
    /// reporting it as changes would fire a rising edge on every task signal that happens to be
    /// true at connect.
    void resetChangeBaseline();

    /// Adopts the current values as the baseline immediately, so the very next change *is*
    /// reported. For the **server**, whose starting state is not unknown: it owns the register
    /// space and that space starts at zero. Deferring the baseline to the first event would
    /// swallow the first write a master makes after connecting — which, for a handshake bit, is
    /// the one that matters most.
    void adoptCurrentAsBaseline();

private:
    struct AreaState {
        ModbusRange range;
        std::map<int, quint16> values;  ///< Bit areas store 0/1 here; word areas store the raw register.
        std::map<int, quint16> shadow;  ///< Baseline for change detection.
        bool hasBaseline{false};
    };

    AreaState &state(ModbusArea area);
    const AreaState &state(ModbusArea area) const;
    void rebuildTagNames();

    AreaState m_coils;
    AreaState m_discreteInputs;
    AreaState m_holdingRegisters;
    AreaState m_inputRegisters;

    QStringList m_digitalTagNames;  ///< Cached; rebuilt only by configure().
    QStringList m_wordTagNames;     ///< Cached; rebuilt only by configure().
};

} // namespace vc::device

#endif // MODBUS_REGISTER_MAP_H
