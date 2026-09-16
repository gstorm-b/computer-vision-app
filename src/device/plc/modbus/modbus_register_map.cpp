/**
 * @file modbus_register_map.cpp
 * @brief Implementation of Modbus tag naming, register storage and change detection.
 */

#include "device/plc/modbus/modbus_register_map.h"

#include "core/logger/app_logger.h"

namespace vc::device {

namespace {

/// Largest address the Modbus protocol can express.
constexpr int kMaxAddress = 65535;

} // namespace

namespace modbus_tags {

QString prefix(ModbusArea area)
{
    switch (area) {
    case ModbusArea::Coils:            return QStringLiteral("COIL");
    case ModbusArea::DiscreteInputs:   return QStringLiteral("DI");
    case ModbusArea::HoldingRegisters: return QStringLiteral("HR");
    case ModbusArea::InputRegisters:   return QStringLiteral("IR");
    }
    return QString();
}

QString format(ModbusArea area, int address)
{
    return prefix(area) + QStringLiteral("%1").arg(address, kAddressDigits, 10, QChar('0'));
}

bool parse(const QString &tag, ModbusArea *area, int *address)
{
    // Longest prefix first: "DI" is not a prefix of any other, but checking in a fixed order
    // keeps this immune to a future prefix that does overlap.
    for (const ModbusArea candidate : kModbusAreas) {
        const QString candidatePrefix = prefix(candidate);
        if (!tag.startsWith(candidatePrefix)) {
            continue;
        }
        const QString digits = tag.mid(candidatePrefix.size());
        if (digits.isEmpty()) {
            return false;
        }
        bool ok = false;
        const int parsed = digits.toInt(&ok);
        if (!ok || parsed < 0 || parsed > kMaxAddress) {
            return false;
        }
        if (area) {
            *area = candidate;
        }
        if (address) {
            *address = parsed;
        }
        return true;
    }
    return false;
}

bool isBitArea(ModbusArea area)
{
    return area == ModbusArea::Coils || area == ModbusArea::DiscreteInputs;
}

bool isMasterWritable(ModbusArea area)
{
    return area == ModbusArea::Coils || area == ModbusArea::HoldingRegisters;
}

bool isServerWritable(ModbusArea area)
{
    // All four. The server owns the space; see the header for why this is a separate question
    // from what a master may write.
    Q_UNUSED(area);
    return true;
}

} // namespace modbus_tags

// ── ModbusRegisterMap ────────────────────────────────────────────────────────────────────────

ModbusRegisterMap::AreaState &ModbusRegisterMap::state(ModbusArea area)
{
    switch (area) {
    case ModbusArea::Coils:            return m_coils;
    case ModbusArea::DiscreteInputs:   return m_discreteInputs;
    case ModbusArea::HoldingRegisters: return m_holdingRegisters;
    case ModbusArea::InputRegisters:   return m_inputRegisters;
    }
    return m_coils;
}

const ModbusRegisterMap::AreaState &ModbusRegisterMap::state(ModbusArea area) const
{
    return const_cast<ModbusRegisterMap *>(this)->state(area);
}

void ModbusRegisterMap::configure(const ModbusRange &coils,
                                  const ModbusRange &discreteInputs,
                                  const ModbusRange &holdingRegisters,
                                  const ModbusRange &inputRegisters)
{
    const ModbusRange ranges[] = { coils, discreteInputs, holdingRegisters, inputRegisters };

    int index = 0;
    for (const ModbusArea area : kModbusAreas) {
        AreaState &st = state(area);
        st.range = ranges[index++];
        st.values.clear();
        st.shadow.clear();
        // No baseline yet: the first takeChangedValues() after a (re)configure must report
        // nothing, or every task signal currently true would fire a rising edge at connect.
        st.hasBaseline = false;
        for (int offset = 0; offset < st.range.count; ++offset) {
            st.values[st.range.start + offset] = 0;
        }
    }

    rebuildTagNames();
}

void ModbusRegisterMap::rebuildTagNames()
{
    m_digitalTagNames.clear();
    m_wordTagNames.clear();

    for (const ModbusArea area : kModbusAreas) {
        const AreaState &st = state(area);
        QStringList &target =
            modbus_tags::isBitArea(area) ? m_digitalTagNames : m_wordTagNames;
        for (int offset = 0; offset < st.range.count; ++offset) {
            target.append(modbus_tags::format(area, st.range.start + offset));
        }
    }
}

const ModbusRange &ModbusRegisterMap::range(ModbusArea area) const
{
    return state(area).range;
}

bool ModbusRegisterMap::setBit(ModbusArea area, int address, bool value)
{
    if (!modbus_tags::isBitArea(area) || !isConfigured(area, address)) {
        return false;
    }
    state(area).values[address] = value ? 1 : 0;
    return true;
}

/// Reads a stored value by tag name. See PlcValueMap::valueForTag() for why an unconfigured or
/// never-polled address must answer false rather than 0.
bool ModbusRegisterMap::valueForTag(const QString &tag, QVariant *value) const
{
    ModbusArea area{};
    int address = 0;
    if (!modbus_tags::parse(tag.trimmed(), &area, &address)) {
        return false;
    }

    bool ok = false;
    if (modbus_tags::isBitArea(area)) {
        const bool stored = bit(area, address, &ok);
        if (!ok) {
            return false;
        }
        if (value) {
            *value = QVariant(stored);
        }
        return true;
    }

    const quint16 stored = word(area, address, &ok);
    if (!ok) {
        return false;
    }
    if (value) {
        // Signed, matching what the device publishes through valueChanged(): a task's number
        // signals are qint16 on the wire and a register past 32767 is a negative index, not a
        // large positive one.
        *value = QVariant(static_cast<int>(static_cast<qint16>(stored)));
    }
    return true;
}

bool ModbusRegisterMap::bit(ModbusArea area, int address, bool *ok) const
{
    const AreaState &st = state(area);
    const auto it = st.values.find(address);
    if (!modbus_tags::isBitArea(area) || it == st.values.end()) {
        if (ok) {
            *ok = false;
        }
        return false;
    }
    if (ok) {
        *ok = true;
    }
    return it->second != 0;
}

bool ModbusRegisterMap::setWord(ModbusArea area, int address, quint16 value)
{
    if (modbus_tags::isBitArea(area) || !isConfigured(area, address)) {
        return false;
    }
    state(area).values[address] = value;
    return true;
}

quint16 ModbusRegisterMap::word(ModbusArea area, int address, bool *ok) const
{
    const AreaState &st = state(area);
    const auto it = st.values.find(address);
    if (modbus_tags::isBitArea(area) || it == st.values.end()) {
        if (ok) {
            *ok = false;
        }
        return 0;
    }
    if (ok) {
        *ok = true;
    }
    return it->second;
}

int ModbusRegisterMap::setBits(ModbusArea area, int address, const QList<bool> &values)
{
    int stored = 0;
    for (int i = 0; i < values.size(); ++i) {
        if (setBit(area, address + i, values.at(i))) {
            ++stored;
        }
    }
    return stored;
}

int ModbusRegisterMap::setWords(ModbusArea area, int address, const QList<quint16> &values)
{
    int stored = 0;
    for (int i = 0; i < values.size(); ++i) {
        if (setWord(area, address + i, values.at(i))) {
            ++stored;
        }
    }
    return stored;
}

QMap<QString, QVariant> ModbusRegisterMap::takeChangedValues()
{
    QMap<QString, QVariant> changed;

    for (const ModbusArea area : kModbusAreas) {
        AreaState &st = state(area);
        const bool report = st.hasBaseline;
        const bool isBit = modbus_tags::isBitArea(area);

        for (const auto &entry : st.values) {
            const auto shadowIt = st.shadow.find(entry.first);
            const bool differs =
                shadowIt == st.shadow.end() || shadowIt->second != entry.second;
            if (!differs) {
                continue;
            }
            if (report) {
                // Words go out as qint16: a task number signal is signed 16-bit throughout the
                // application, so this is the same value the Mitsubishi family would publish for
                // the same register content.
                changed.insert(modbus_tags::format(area, entry.first),
                               isBit ? QVariant(entry.second != 0)
                                     : QVariant(static_cast<qint16>(entry.second)));
            }
            st.shadow[entry.first] = entry.second;
        }

        // Addresses that disappeared from the configured span leave the baseline behind them.
        for (auto it = st.shadow.begin(); it != st.shadow.end();) {
            it = st.values.count(it->first) ? std::next(it) : st.shadow.erase(it);
        }

        st.hasBaseline = true;
    }

    return changed;
}

void ModbusRegisterMap::resetChangeBaseline()
{
    for (const ModbusArea area : kModbusAreas) {
        AreaState &st = state(area);
        st.shadow.clear();
        st.hasBaseline = false;
    }
}

void ModbusRegisterMap::adoptCurrentAsBaseline()
{
    for (const ModbusArea area : kModbusAreas) {
        AreaState &st = state(area);
        st.shadow = st.values;
        st.hasBaseline = true;
    }
}

} // namespace vc::device
