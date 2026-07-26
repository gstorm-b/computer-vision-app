#ifndef BASLER_DEFINE_H
#define BASLER_DEFINE_H

#include <QJsonObject>
#include "core/utils/meta_utils.h"

#include <pylon/PylonIncludes.h>
#include <pylon/_BaslerUniversalCameraParams.h>

/// Basler-camera-specific enums, GenICam/Pylon parameter conversion helpers, and the
/// BaslerIOLine capability struct shared by the Basler GigE camera device and its config.
namespace vc::device::basler {
Q_NAMESPACE

/// GenICam I/O line role, as reported by the camera's LineSelector/LineMode nodes.
enum class BaslerLineType {
    Line_Input,   ///< Line can be configured as a digital input.
    Line_Output,  ///< Line can be configured as a digital output.
    Line_GPIO     ///< Line can be configured as either input or output.
};
Q_ENUM_NS(BaslerLineType)

/// Auto-exposure mode, mirroring the Basler GenICam ExposureAuto enumeration.
enum class BaslerExposureMode {
    Exposure_Off,         ///< Exposure time is fixed and set manually.
    Exposure_Once,        ///< Camera auto-adjusts exposure once, then holds it.
    Exposure_Continuous   ///< Camera continuously auto-adjusts exposure.
};
Q_ENUM_NS(BaslerExposureMode)

/// Enum key strings registered only so `lupdate`/`linguist` picks up translatable
/// enum labels; not read by any code at runtime.
static inline const char* enum_keys_basler_defines[] = {
    // BaslerLineType
    QT_TR_NOOP("Line_Input"),
    QT_TR_NOOP("Line_Output"),
    QT_TR_NOOP("Line_GPIO"),

    // Basler SDK Exposure
    QT_TR_NOOP("Exposure_Off"),
    QT_TR_NOOP("Exposure_Once"),
    QT_TR_NOOP("Exposure_Continuous"),
};

/// Converts a BaslerExposureMode to its Qt-enum-registered string name (e.g. "Exposure_Off").
/// @param t the exposure mode to convert
/// @return the enum's registered name string
[[maybe_unused]] static QString BaslerExposureTypeToString(BaslerExposureMode t) {
    return qenumToString(t);
};

/// Parses a BaslerExposureMode from its registered enum name string.
/// @param t the enum name string (as produced by BaslerExposureTypeToString)
/// @return the matching enum value, or Exposure_Off if `t` does not match any name
[[maybe_unused]] static BaslerExposureMode BaslerExposureTypeFromString(QString t) {
    return stringToQEnum(t, BaslerExposureMode::Exposure_Off);
};

/// Converts a Pylon/GenICam ExposureAutoEnums value to the plain "Off"/"Once"/"Continuous"
/// string used by the camera's GenApi ExposureAuto parameter.
/// @param mode the Pylon SDK exposure-auto enum value
/// @return "Off", "Once", or "Continuous"; defaults to "Off" for unrecognized values
[[maybe_unused]] static QString autoExposureToQString(Basler_UniversalCameraParams::ExposureAutoEnums mode) {
    if (mode == Basler_UniversalCameraParams::ExposureAutoEnums::ExposureAuto_Off) {
        return "Off";
    } else if (mode == Basler_UniversalCameraParams::ExposureAutoEnums::ExposureAuto_Once) {
        return "Once";
    } else if (mode == Basler_UniversalCameraParams::ExposureAutoEnums::ExposureAuto_Continuous) {
        return "Continuous";
    }
    return "Off";
}

/// Parses a Pylon/GenICam ExposureAutoEnums value from its "Off"/"Once"/"Continuous" string.
/// @param mode the exposure-auto mode string
/// @return the matching Pylon SDK enum value, or ExposureAuto_Off if `mode` is unrecognized
[[maybe_unused]] static Basler_UniversalCameraParams::ExposureAutoEnums autoExposureToEnum(QString mode) {
    if (mode == "Off") {
        return Basler_UniversalCameraParams::ExposureAutoEnums::ExposureAuto_Off;
    } else if (mode == "Once") {
        return Basler_UniversalCameraParams::ExposureAutoEnums::ExposureAuto_Once;
    } else if (mode == "Continuous") {
        return Basler_UniversalCameraParams::ExposureAutoEnums::ExposureAuto_Continuous;
    }
    return Basler_UniversalCameraParams::ExposureAutoEnums::ExposureAuto_Off;
}

/// Reported I/O capability of a single GenICam camera line, as discovered by querying the
/// camera's LineSelector/LineMode nodes (see BaslerGigECamera::initializeIOPort).
struct BaslerIOLine {
    bool can_be_input = false;   ///< True if the line's LineMode entries include "Input".
    bool can_be_output = false;  ///< True if the line's LineMode entries include "Output".
    bool is_writable = false;    ///< True if the camera's LineMode node is writable for this line.
    QString name = "";           ///< GenICam line name (LineSelector symbolic), e.g. "Line1".

    /// Serializes this line's capability flags and name to a QJsonObject.
    /// @return the populated JSON object
    QJsonObject toJson() {
        QJsonObject obj;
        obj["can_be_input"] = can_be_input;
        obj["can_be_output"] = can_be_output;
        obj["is_writable"] = is_writable;
        obj["name"] = name;
        return obj;
    }

    /// Populates this line's fields from a previously-serialized JSON object.
    /// @param obj the JSON object to read; missing keys default to false/empty
    void fromJson(QJsonObject &obj) {
        can_be_input = obj["can_be_input"].toBool(false);
        can_be_output = obj["can_be_output"].toBool(false);
        is_writable = obj["is_writable"].toBool(false);
        name = obj["name"].toString("");
    }
};

} // namespace vc::device

Q_DECLARE_METATYPE(vc::device::basler::BaslerLineType)
Q_DECLARE_METATYPE(vc::device::basler::BaslerExposureMode)

#endif // BASLER_DEFINE_H
