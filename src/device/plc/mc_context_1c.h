#ifndef MC_CONTEXT_1C_H
#define MC_CONTEXT_1C_H

/**
 * @file mc_context_1c.h
 * @brief Concrete McContext for the MC 1C computer-link frame over a serial line
 *        (Context_Mc1C).
 */

#include "mc_context.h"
#include "device/plc/mc_msg_serial_port.h"

#include <QObject>
#include <QMetaType>

namespace vc::device {

/**
 * @class Context_Mc1C
 * @brief Concrete McContext for the MC 1C frame over a serial line: adds the computer-link
 *        addressing fields (station number, PLC number, message wait time), the message format
 *        and the sum-check switch on top of the base M/D device-range settings, and always
 *        carries a McMsgSerialCfg as its message-interface config.
 *
 * Field set taken from the 1C reference implementation
 * (`reference source/1C_frame/fx3communicator.cpp`), whose request frame is
 * `ENQ + station(2) + PLC(2) + command(2) + wait time(1) + area + CR + LF`, and from
 * `mc_frame_1c.py`, which contributes the format and sum-check parameters.
 */
class Context_Mc1C : public McContext {
    Q_GADGET

    G_PROPERTY_ENUM_READWRITE(vc::device::mc::McFrameFormat, frameFormat, "Frame format")   ///< Message format; format 4 adds the CR+LF terminator format 1 omits.
    G_PROPERTY_NUMBER_READWRITE(int, stationNumber, 0, 31, "Station No.")   ///< Computer-link station number, sent as two ASCII hex digits.
    G_PROPERTY_NUMBER_READWRITE(int, plcNumber, 0, 255, "PLC No.")   ///< PLC number; 0xFF addresses the station the cable is plugged into.
    G_PROPERTY_NUMBER_READWRITE(int, messageWaitTime, 0, 15, "Message wait time")   ///< Message wait time in 10 ms units, sent as one ASCII hex digit.
    G_PROPERTY_BOOL_READWRITE(bool, useSumCheck, "Use sum check")   ///< Whether the frame carries a sum check; must match the PLC port's own setting.

    /// Translation markers for the display names above. Not read by any code: lupdate cannot
    /// see Q_CLASSINFO, so without this table these labels never enter the .ts. The context
    /// must be this class's className(). See TaskLocalizeConfig::kDisplayNameSources for the
    /// full reasoning; the contract test asserts this list and the properties agree.
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::device::Context_Mc1C", "Frame format"),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc1C", "Station No."),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc1C", "PLC No."),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc1C", "Message wait time"),
        QT_TRANSLATE_NOOP("vc::device::Context_Mc1C", "Use sum check"),
    };

public:
    /// Constructs with ASCII data code — the computer-link frames are ASCII by definition —
    /// and a default-constructed McMsgSerialCfg message-interface config.
    Context_Mc1C() {
        m_data_code = McDataCode::Ascii;
        m_msg_cfg = std::make_shared<McMsgSerialCfg>();
    }

    /// Constructs with the given data code and a default-constructed McMsgSerialCfg.
    /// @note A 1C frame is ASCII on the wire; a Binary data code here is accepted only so the
    ///       factory signature stays uniform across frame types.
    explicit Context_Mc1C(McDataCode data_code) {
        m_data_code = data_code;
        m_msg_cfg = std::make_shared<McMsgSerialCfg>();
    }

    /// Deep-copies `other`, including its message-interface config.
    ///
    /// The implicit copy would share the `shared_ptr<McMsgItfConfig>` with the source, so two
    /// "independent" contexts would write each other's serial settings. Context_Mc3E has that
    /// behaviour today — see backlog #46; the new contexts do not reproduce it.
    Context_Mc1C(const Context_Mc1C &other)
        : McContext(other),
        m_frameFormat(other.m_frameFormat),
        m_stationNumber(other.m_stationNumber),
        m_plcNumber(other.m_plcNumber),
        m_messageWaitTime(other.m_messageWaitTime),
        m_useSumCheck(other.m_useSumCheck) {

        if (other.m_msg_cfg) {
            m_msg_cfg = std::make_shared<McMsgSerialCfg>(
                *static_cast<McMsgSerialCfg *>(other.m_msg_cfg.get()));
        }
    }

    /// Default destructor (no owned resources beyond base-class members).
    ~Context_Mc1C() override = default;

    /// Returns this class's static QMetaObject, for Q_GADGET-based property introspection.
    const QMetaObject &getMetaObject() const override {
        return vc::device::Context_Mc1C::staticMetaObject;
    }

    /// Returns McFrameType::Frame_1C.
    McFrameType frameType() const override {
        return McFrameType::Frame_1C;
    }

    /// Returns McMsgItfType::SerialPort.
    McMsgItfType msgIntefaceType() const override {
        return McMsgItfType::SerialPort;
    }

    /// Creates a heap-allocated deep copy of this context via the copy constructor above.
    /// Caller takes ownership of the returned pointer.
    McContext* clone() const override {
        return new Context_Mc1C(*this);
    }

    /// Serializes the base McContext settings plus the 1C addressing fields and, if set, the
    /// message-interface config under "MsgConfig". Enums are written as their key names so a
    /// reordered enumerator cannot change a saved project's meaning.
    QJsonObject toJson() const override {
        QJsonObject obj = McContext::toJson();
        obj["frameFormat"] = qenumToString(m_frameFormat);
        obj["stationNumber"] = m_stationNumber;
        obj["plcNumber"] = m_plcNumber;
        obj["messageWaitTime"] = m_messageWaitTime;
        obj["useSumCheck"] = m_useSumCheck;

        if (m_msg_cfg) {
            obj["MsgConfig"] = m_msg_cfg->toJson();
        }
        return obj;
    }

    /// Restores the 1C fields from `obj` (falling back to the current value for each missing or
    /// unrecognized key), lazily creates the McMsgSerialCfg if not already present and restores
    /// it from "MsgConfig", then chains to McContext::fromJson() for the base settings.
    /// @return the base-class result (false if the base data code could not be parsed).
    bool fromJson(const QJsonObject &obj) override {
        m_frameFormat = stringToQEnum(obj["frameFormat"].toString(), m_frameFormat);
        m_stationNumber = obj["stationNumber"].toInt(m_stationNumber);
        m_plcNumber = obj["plcNumber"].toInt(m_plcNumber);
        m_messageWaitTime = obj["messageWaitTime"].toInt(m_messageWaitTime);
        m_useSumCheck = obj["useSumCheck"].toBool(m_useSumCheck);

        if (!m_msg_cfg) {
            m_msg_cfg = std::make_shared<McMsgSerialCfg>();
        }
        m_msg_cfg->fromJson(obj["MsgConfig"].toObject());
        return McContext::fromJson(obj);
    }

public:
    McFrameFormat m_frameFormat{McFrameFormat::Format_4};   ///< Message format; backing store for the frameFormat property.
    int m_stationNumber{0};      ///< Computer-link station number; backing store for the stationNumber property.
    int m_plcNumber{0xFF};       ///< PLC number (0xFF = the directly connected station); backing store for the plcNumber property.
    int m_messageWaitTime{0};    ///< Message wait time in 10 ms units; backing store for the messageWaitTime property.
    bool m_useSumCheck{false};   ///< Sum-check switch; backing store for the useSumCheck property.
};

}

Q_DECLARE_METATYPE(vc::device::Context_Mc1C)

#endif // MC_CONTEXT_1C_H
