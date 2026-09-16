#ifndef MC_CONTEXT_3C_H
#define MC_CONTEXT_3C_H

/**
 * @file mc_context_3c.h
 * @brief Concrete McContext for the MC 3C computer-link frame over a serial line
 *        (Context_Mc3C).
 */

#include "mc_context.h"
#include "device/plc/mc_msg_serial_port.h"

#include <QObject>
#include <QMetaType>

namespace vc::device {

/**
 * @class Context_Mc3C
 * @brief Concrete McContext for the MC 3C frame over a serial line: adds the four access-route
 *        numbers, the PLC series, the message format and the sum-check switch on top of the
 *        base M/D device-range settings, and always carries a McMsgSerialCfg as its
 *        message-interface config.
 *
 * Field set taken from `reference source/3C_frame/mc_frame_3c.py`, whose request frame is
 * `control code + frame ID ("F9") + access route + request data + [sum check]`, with the access
 * route being `station No.(2) + network No.(2) + PC No.(2) + self-station No.(2)`, each as ASCII
 * hex.
 *
 * @note The reference hard-codes the access route to 00/00/FF/00 and notes it "could be
 *       modified on next version". Here it is configurable, which is what a context is for —
 *       the same four numbers are what a multi-drop or networked station has to change.
 */
class Context_Mc3C : public McContext {
    Q_GADGET

    G_PROPERTY_ENUM_READWRITE(vc::device::mc::McFrameFormat, frameFormat, "Frame format")   ///< Message format; format 4 adds the CR+LF terminator format 1 omits.
    G_PROPERTY_ENUM_READWRITE(vc::device::mc::McPlcSeries, plcSeries, "PLC series")   ///< Target PLC series; decides device-header width and sub-command value.
    G_PROPERTY_NUMBER_READWRITE(int, stationNumber, 0, 31, "Station No.")   ///< Access route: station number, sent as two ASCII hex digits.
    G_PROPERTY_NUMBER_READWRITE(int, networkNumber, 0, 255, "Network No.")   ///< Access route: network number; 0 when connected directly.
    G_PROPERTY_NUMBER_READWRITE(int, pcNumber, 0, 255, "PC No.")   ///< Access route: PC number; 0xFF when connected directly.
    G_PROPERTY_NUMBER_READWRITE(int, selfStationNumber, 0, 255, "Self-station No.")   ///< Access route: self-station number; normally 0.
    G_PROPERTY_BOOL_READWRITE(bool, useSumCheck, "Use sum check")   ///< Whether the frame carries a sum check; must match the PLC port's own setting.

    /// Translation markers for the display names above. Not read by any code: lupdate cannot
    /// see Q_CLASSINFO, so without this table these labels never enter the .ts. The context
    /// must be this class's className(). See TaskLocalizeConfig::kDisplayNameSources for the
    /// full reasoning; the contract test asserts this list and the properties agree.
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::device::Context_Mc3C", "Frame format"),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc3C", "PLC series"),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc3C", "Station No."),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc3C", "Network No."),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc3C", "PC No."),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc3C", "Self-station No."),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc3C", "Use sum check"),
    };

public:
    /// Constructs with ASCII data code — the computer-link frames are ASCII by definition —
    /// and a default-constructed McMsgSerialCfg message-interface config.
    Context_Mc3C() {
        m_data_code = McDataCode::Ascii;
        m_msg_cfg = std::make_shared<McMsgSerialCfg>();
    }

    /// Constructs with the given data code and a default-constructed McMsgSerialCfg.
    /// @note A 3C frame is ASCII on the wire; a Binary data code here is accepted only so the
    ///       factory signature stays uniform across frame types.
    explicit Context_Mc3C(McDataCode data_code) {
        m_data_code = data_code;
        m_msg_cfg = std::make_shared<McMsgSerialCfg>();
    }

    /// Deep-copies `other`, including its message-interface config.
    ///
    /// The implicit copy would share the `shared_ptr<McMsgItfConfig>` with the source, so two
    /// "independent" contexts would write each other's serial settings. Context_Mc3E has that
    /// behaviour today — see backlog #46; the new contexts do not reproduce it.
    Context_Mc3C(const Context_Mc3C &other)
        : McContext(other),
        m_frameFormat(other.m_frameFormat),
        m_plcSeries(other.m_plcSeries),
        m_stationNumber(other.m_stationNumber),
        m_networkNumber(other.m_networkNumber),
        m_pcNumber(other.m_pcNumber),
        m_selfStationNumber(other.m_selfStationNumber),
        m_useSumCheck(other.m_useSumCheck) {

        if (other.m_msg_cfg) {
            m_msg_cfg = std::make_shared<McMsgSerialCfg>(
                *static_cast<McMsgSerialCfg *>(other.m_msg_cfg.get()));
        }
    }

    /// Default destructor (no owned resources beyond base-class members).
    ~Context_Mc3C() override = default;

    /// Returns this class's static QMetaObject, for Q_GADGET-based property introspection.
    const QMetaObject &getMetaObject() const override {
        return vc::device::Context_Mc3C::staticMetaObject;
    }

    /// Returns McFrameType::Frame_3C.
    McFrameType frameType() const override {
        return McFrameType::Frame_3C;
    }

    /// Returns McMsgItfType::SerialPort.
    McMsgItfType msgIntefaceType() const override {
        return McMsgItfType::SerialPort;
    }

    /// Creates a heap-allocated deep copy of this context via the copy constructor above.
    /// Caller takes ownership of the returned pointer.
    McContext* clone() const override {
        return new Context_Mc3C(*this);
    }

    /// Serializes the base McContext settings plus the 3C access route, PLC series, format and
    /// sum-check switch and, if set, the message-interface config under "MsgConfig". Enums are
    /// written as their key names so a reordered enumerator cannot change a saved project's
    /// meaning.
    QJsonObject toJson() const override {
        QJsonObject obj = McContext::toJson();
        obj["frameFormat"] = qenumToString(m_frameFormat);
        obj["plcSeries"] = qenumToString(m_plcSeries);
        obj["stationNumber"] = m_stationNumber;
        obj["networkNumber"] = m_networkNumber;
        obj["pcNumber"] = m_pcNumber;
        obj["selfStationNumber"] = m_selfStationNumber;
        obj["useSumCheck"] = m_useSumCheck;

        if (m_msg_cfg) {
            obj["MsgConfig"] = m_msg_cfg->toJson();
        }
        return obj;
    }

    /// Restores the 3C fields from `obj` (falling back to the current value for each missing or
    /// unrecognized key), lazily creates the McMsgSerialCfg if not already present and restores
    /// it from "MsgConfig", then chains to McContext::fromJson() for the base settings.
    /// @return the base-class result (false if the base data code could not be parsed).
    bool fromJson(const QJsonObject &obj) override {
        m_frameFormat = stringToQEnum(obj["frameFormat"].toString(), m_frameFormat);
        m_plcSeries = stringToQEnum(obj["plcSeries"].toString(), m_plcSeries);
        m_stationNumber = obj["stationNumber"].toInt(m_stationNumber);
        m_networkNumber = obj["networkNumber"].toInt(m_networkNumber);
        m_pcNumber = obj["pcNumber"].toInt(m_pcNumber);
        m_selfStationNumber = obj["selfStationNumber"].toInt(m_selfStationNumber);
        m_useSumCheck = obj["useSumCheck"].toBool(m_useSumCheck);

        if (!m_msg_cfg) {
            m_msg_cfg = std::make_shared<McMsgSerialCfg>();
        }
        m_msg_cfg->fromJson(obj["MsgConfig"].toObject());
        return McContext::fromJson(obj);
    }

public:
    McFrameFormat m_frameFormat{McFrameFormat::Format_4};   ///< Message format; backing store for the frameFormat property.
    McPlcSeries m_plcSeries{McPlcSeries::PlcSeries_Q};      ///< Target PLC series; backing store for the plcSeries property.
    int m_stationNumber{0};        ///< Access route station number; backing store for the stationNumber property.
    int m_networkNumber{0};        ///< Access route network number; backing store for the networkNumber property.
    int m_pcNumber{0xFF};          ///< Access route PC number (0xFF = directly connected); backing store for the pcNumber property.
    int m_selfStationNumber{0};    ///< Access route self-station number; backing store for the selfStationNumber property.
    bool m_useSumCheck{false};     ///< Sum-check switch; backing store for the useSumCheck property.
};

}

Q_DECLARE_METATYPE(vc::device::Context_Mc3C)

#endif // MC_CONTEXT_3C_H
