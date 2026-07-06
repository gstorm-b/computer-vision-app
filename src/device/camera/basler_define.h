#ifndef BASLER_DEFINE_H
#define BASLER_DEFINE_H

#include <QJsonObject>
#include "core/utils/meta_utils.h"

#include <pylon/PylonIncludes.h>
#include <pylon/_BaslerUniversalCameraParams.h>

namespace vc::device::basler {
Q_NAMESPACE

enum class BaslerLineType {
    Line_Input,
    Line_Output,
    Line_GPIO
};
Q_ENUM_NS(BaslerLineType)

enum class BaslerExposureMode {
    Exposure_Off,
    Exposure_Once,
    Exposure_Continuous
};
Q_ENUM_NS(BaslerExposureMode)

// only use for lingust
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

[[maybe_unused]] static QString BaslerExposureTypeToString(BaslerExposureMode t) {
    return qenumToString(t);
};

[[maybe_unused]] static BaslerExposureMode BaslerExposureTypeFromString(QString t) {
    return stringToQEnum(t, BaslerExposureMode::Exposure_Off);
};

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

struct BaslerIOLine {
    bool can_be_input = false;
    bool can_be_output = false;
    bool is_writable = false;
    QString name = "";

    QJsonObject toJson() {
        QJsonObject obj;
        obj["can_be_input"] = can_be_input;
        obj["can_be_output"] = can_be_output;
        obj["is_writable"] = is_writable;
        obj["name"] = name;
        return obj;
    }

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
