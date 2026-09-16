#ifndef JAI_DEFINE_H
#define JAI_DEFINE_H

/**
 * @file jai_define.h
 * @brief JAI-camera-specific enums, GenICam node-name resolution helpers, and the JaiIOLine
 *        capability struct shared by the JAI GigE camera device and its config.
 *
 * The JAI camera is driven through the **Pleora eBUS SDK** (`PvDevice`/`PvStream`/`PvBuffer`),
 * not the JAI SDK C API — both are installed on this machine and only the eBUS one is used.
 * `reference source/JaiCamTest` is the sample this was settled from.
 */

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "core/utils/meta_utils.h"

#include <PvGenParameterArray.h>
#include <PvGenEnum.h>
#include <PvGenFloat.h>
#include <PvGenInteger.h>

namespace vc::device::jai {
Q_NAMESPACE

/**
 * @enum JaiExposureMode
 * @brief Auto-exposure mode, mirroring the GenICam `ExposureAuto` enumeration.
 *
 * Deliberately a separate enum from `BaslerExposureMode` rather than a shared one: the two
 * happen to carry the same three values today, and merging them would make the first camera
 * whose SDK offers a fourth mode a breaking change for the other. The display strings are also
 * translated per class, and the contract test asserts the context matches the owning class.
 */
enum class JaiExposureMode {
    Exposure_Off,         ///< Exposure time is fixed and set manually.
    Exposure_Once,        ///< Camera auto-adjusts exposure once, then holds it.
    Exposure_Continuous   ///< Camera continuously auto-adjusts exposure.
};
Q_ENUM_NS(JaiExposureMode)

/**
 * @enum JaiLineType
 * @brief GenICam I/O line role, as reported by the camera's LineSelector/LineMode nodes.
 */
enum class JaiLineType {
    Line_Input,   ///< Line can be configured as a digital input.
    Line_Output,  ///< Line can be configured as a digital output.
    Line_GPIO     ///< Line can be configured as either input or output.
};
Q_ENUM_NS(JaiLineType)

/// Enum key strings registered only so `lupdate`/`linguist` picks up translatable enum labels;
/// not read by any code at runtime. Same mechanism as `enum_keys_basler_defines`.
static inline const char *enum_keys_jai_defines[] = {
    // JaiExposureMode
    QT_TR_NOOP("Exposure_Off"),
    QT_TR_NOOP("Exposure_Once"),
    QT_TR_NOOP("Exposure_Continuous"),

    // JaiLineType
    QT_TR_NOOP("Line_Input"),
    QT_TR_NOOP("Line_Output"),
    QT_TR_NOOP("Line_GPIO"),
};

/// Converts a JaiExposureMode to its Qt-enum-registered string name (e.g. "Exposure_Off").
[[maybe_unused]] static QString JaiExposureTypeToString(JaiExposureMode t) {
    return qenumToString(t);
}

/// Parses a JaiExposureMode from its registered enum name string.
/// @return the matching enum value, or Exposure_Off if `t` matches none.
[[maybe_unused]] static JaiExposureMode JaiExposureTypeFromString(QString t) {
    return stringToQEnum(t, JaiExposureMode::Exposure_Off);
}

/// Converts a JaiExposureMode to the plain GenICam `ExposureAuto` symbolic ("Off"/"Once"/
/// "Continuous"). These three strings are fixed by the GenICam SFNC, not by JAI.
[[maybe_unused]] static const char *JaiExposureModeToGenICam(JaiExposureMode mode) {
    switch (mode) {
    case JaiExposureMode::Exposure_Once:       return "Once";
    case JaiExposureMode::Exposure_Continuous: return "Continuous";
    case JaiExposureMode::Exposure_Off:        break;
    }
    return "Off";
}

/// Parses a JaiExposureMode from the GenICam `ExposureAuto` symbolic.
[[maybe_unused]] static JaiExposureMode JaiExposureModeFromGenICam(const QString &symbolic) {
    if (symbolic == QLatin1String("Once")) {
        return JaiExposureMode::Exposure_Once;
    }
    if (symbolic == QLatin1String("Continuous")) {
        return JaiExposureMode::Exposure_Continuous;
    }
    return JaiExposureMode::Exposure_Off;
}

// ── GenICam node-name resolution ─────────────────────────────────────────────────────────────
//
// Do not hard-code a single node name for exposure, gain or frame rate.
//
// The GenICam SFNC renamed these features, and which spelling a camera carries depends on the
// model and its firmware, not on the vendor: SFNC-compliant firmware exposes `ExposureTime`
// (float, microseconds) and `Gain` (float, dB), while older firmware exposes `ExposureTimeAbs`
// and `GainRaw` (integer). The Basler device in this repository hard-codes the legacy pair
// because that is what its one camera model answers to — and that is exactly the assumption
// that would make this device work on the camera it was written against and silently fail on
// the next one, with "node not present" and no exposure control.
//
// The model is not known until connect (Q4 in the phase plan: "pixel-format and camera model
// read from camera after connected"), so the node set cannot be decided at compile time.
// resolveNode() picks whichever spelling the connected camera actually has.

/// Candidate GenICam node names for a feature, most-preferred first.
namespace node_names {
/// Exposure time in microseconds. SFNC name first, legacy second.
static inline const char *const kExposureTime[] = { "ExposureTime", "ExposureTimeAbs", nullptr };
/// Auto-exposure mode. One spelling across both generations.
static inline const char *const kExposureAuto[] = { "ExposureAuto", nullptr };
/// Gain. SFNC `Gain` is a float in dB; legacy `GainRaw` is an integer in device units.
static inline const char *const kGainFloat[] = { "Gain", nullptr };
static inline const char *const kGainInteger[] = { "GainRaw", "GainRawChannelA", nullptr };
/// Acquisition frame rate and its enable switch.
static inline const char *const kFrameRate[] = {
    "AcquisitionFrameRate", "AcquisitionFrameRateAbs", nullptr };
static inline const char *const kFrameRateEnable[] = {
    "AcquisitionFrameRateEnable", "AcquisitionFrameRateEnabled", nullptr };

/// Digital line configuration. `LineInverter` carries the output's polarity in HARDWARE, which is
/// where it belongs: inverting the value in software before writing looks equivalent but leaves
/// the pin at the wrong level whenever anything other than this code drives the output — at power
/// -up, and between a disconnect and the next configure.
static inline const char *const kLineSelector[] = { "LineSelector", nullptr };
static inline const char *const kLineMode[]     = { "LineMode", nullptr };
static inline const char *const kLineSource[]   = { "LineSource", nullptr };
static inline const char *const kLineInverter[] = { "LineInverter", "LineInvert", nullptr };
static inline const char *const kLineStatus[]   = { "LineStatus", nullptr };
/// The user-controlled output that a line is routed to, and its level.
static inline const char *const kUserOutputSelector[] = { "UserOutputSelector", nullptr };
static inline const char *const kUserOutputValue[]    = { "UserOutputValue", nullptr };
} // namespace node_names

/// Returns the first name in @p candidates that @p params actually exposes, or an empty
/// PvString when the camera has none of them.
///
/// @param params     the device's GenICam parameter array
/// @param candidates null-terminated array of candidate node names, most-preferred first
[[maybe_unused]] static PvString resolveNode(PvGenParameterArray *params,
                                             const char *const *candidates) {
    if (!params || !candidates) {
        return PvString();
    }
    for (const char *const *name = candidates; *name != nullptr; ++name) {
        PvGenParameter *parameter = params->Get(PvString(*name));
        if (parameter != nullptr && parameter->IsAvailable()) {
            return PvString(*name);
        }
    }
    return PvString();
}

/**
 * @struct NumericFeature
 * @brief A resolved GenICam numeric feature: its node name, its actual type, and its range.
 *
 * @warning Resolving the *name* generically is only half the job — the **type** varies too. The SFNC
 * says `ExposureTime` is a Float, but firmware that predates it (or only partly follows it)
 * exposes the same concept as an Integer, and `GetFloatRange()` on an Integer node simply fails.
 * Reading exposure as a float and ignoring the failed result is how a camera ends up reporting a
 * 0..0 range and an exposure field the operator cannot move, with nothing in the log to say why.
 */
struct NumericFeature {
    PvString node;         ///< Resolved node name; empty when the camera has none of the candidates.
    bool isFloat{false};   ///< True for a Float node, false for an Integer one.
    double min{0.0};
    double max{0.0};
    double value{0.0};
    bool valid{false};     ///< True when the range was read AND is usable (min < max).

    /// True when the camera exposes this feature at all.
    bool exists() const { return node.GetLength() > 0; }
    /// One-line rendering for the connect log.
    QString describe() const {
        if (!exists()) {
            return QStringLiteral("(absent)");
        }
        return QStringLiteral("%1[%2] %3..%4 = %5")
            .arg(QString::fromUtf8(node.GetAscii()),
                 isFloat ? QStringLiteral("float") : QStringLiteral("int"))
            .arg(min).arg(max).arg(value)
            + (valid ? QString() : QStringLiteral(" (RANGE UNUSABLE)"));
    }
};

/// Resolves the first available candidate node and reads its range and current value, using
/// whichever accessor matches the node's real type.
[[maybe_unused]] static NumericFeature readNumericFeature(PvGenParameterArray *params,
                                                          const char *const *candidates) {
    NumericFeature feature;
    feature.node = resolveNode(params, candidates);
    if (!feature.exists() || !params) {
        return feature;
    }

    PvGenParameter *parameter = params->Get(feature.node);
    PvGenType type = PvGenTypeInteger;
    if (!parameter || !parameter->GetType(type).IsOK()) {
        return feature;
    }

    if (type == PvGenTypeFloat) {
        feature.isFloat = true;
        const bool haveRange = params->GetFloatRange(feature.node, feature.min, feature.max).IsOK();
        params->GetFloatValue(feature.node, feature.value);
        feature.valid = haveRange && (feature.min < feature.max);
    } else if (type == PvGenTypeInteger) {
        int64_t min = 0;
        int64_t max = 0;
        int64_t value = 0;
        const bool haveRange = params->GetIntegerRange(feature.node, min, max).IsOK();
        params->GetIntegerValue(feature.node, value);
        feature.min = double(min);
        feature.max = double(max);
        feature.value = double(value);
        feature.valid = haveRange && (min < max);
    }
    return feature;
}

/// Writes @p value to a feature resolved by readNumericFeature(), using the matching accessor.
/// @return the SDK result; an invalid/absent feature yields NOT_IMPLEMENTED rather than a crash.
[[maybe_unused]] static PvResult writeNumericFeature(PvGenParameterArray *params,
                                                     const NumericFeature &feature,
                                                     double value) {
    if (!params || !feature.exists()) {
        return PvResult(PvResult::Code::NOT_IMPLEMENTED);
    }
    if (feature.isFloat) {
        return params->SetFloatValue(feature.node, value);
    }
    return params->SetIntegerValue(feature.node, static_cast<int64_t>(value));
}

/**
 * @struct JaiIOLine
 * @brief Reported I/O capability of a single GenICam camera line, discovered by querying the
 *        camera's LineSelector/LineMode nodes (see JaiGigECamera::initializeIOPort).
 *
 * Field-for-field the same shape as `BaslerIOLine`, and deliberately not shared with it: this
 * struct is persisted inside each camera's own config JSON, so making one type serve both would
 * couple two independently-versioned on-disk formats to a single declaration.
 */
struct JaiIOLine {
    bool can_be_input = false;   ///< True if the line's LineMode entries include "Input".
    bool can_be_output = false;  ///< True if the line's LineMode entries include "Output".
    bool is_writable = false;    ///< True if the camera's LineMode node is writable for this line.
    QString name = "";           ///< GenICam line name (LineSelector symbolic), e.g. "Line1".

    /// Serializes this line's capability flags and name to a QJsonObject.
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["can_be_input"] = can_be_input;
        obj["can_be_output"] = can_be_output;
        obj["is_writable"] = is_writable;
        obj["name"] = name;
        return obj;
    }

    /// Populates this line's fields from a previously-serialized JSON object.
    /// @param obj the JSON object to read; missing keys default to false/empty
    void fromJson(const QJsonObject &obj) {
        can_be_input = obj["can_be_input"].toBool(false);
        can_be_output = obj["can_be_output"].toBool(false);
        is_writable = obj["is_writable"].toBool(false);
        name = obj["name"].toString("");
    }
};

} // namespace vc::device::jai

Q_DECLARE_METATYPE(vc::device::jai::JaiExposureMode)
Q_DECLARE_METATYPE(vc::device::jai::JaiLineType)

#endif // JAI_DEFINE_H
