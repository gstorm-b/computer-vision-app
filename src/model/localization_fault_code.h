#ifndef LOCALIZATION_FAULT_CODE_H
#define LOCALIZATION_FAULT_CODE_H

#include <QString>

namespace vc::model {

/// Fault codes reported by the localization task's runtime/recovery logic (see
/// LocalizationRuntimeController), grouped by subsystem: 100s camera, 200s vision output,
/// 300s PLC, 400s pattern/calibration, 500s internal.
enum class LocalizationFaultCode : int {
    None = 0,                     ///< No fault.
    CameraLost = 100,             ///< Camera connection was lost after being connected.
    CameraConnectFailed = 101,    ///< Camera failed to (re)connect.
    CameraGrabTimeout = 102,      ///< Camera did not deliver a frame within the grab timeout.
    VisionOutputLost = 200,       ///< Vision-output device connection was lost, or failed to connect.
    VisionOutputSendFailed = 201, ///< Vision-output device failed to send the matching result.
    PlcLost = 300,                ///< Primary PLC connection was lost, or failed to connect.
    PatternInvalid = 400,         ///< Active pattern/match group is missing or invalid.
    CalibrationInvalid = 401,     ///< Active camera calibration is missing or invalid.
    InternalError = 500,          ///< Unexpected internal error not covered by another code.
};

/// Returns the stable, human-readable identifier for `code` (e.g. "CameraLost"), used for
/// logging/diagnostics; falls back to "Unknown" for any value not in the switch.
/// @param code fault code to name
/// @return the identifier string for `code`
inline QString localizationFaultCodeName(LocalizationFaultCode code)
{
    switch (code) {
    case LocalizationFaultCode::None:
        return QStringLiteral("None");
    case LocalizationFaultCode::CameraLost:
        return QStringLiteral("CameraLost");
    case LocalizationFaultCode::CameraConnectFailed:
        return QStringLiteral("CameraConnectFailed");
    case LocalizationFaultCode::CameraGrabTimeout:
        return QStringLiteral("CameraGrabTimeout");
    case LocalizationFaultCode::VisionOutputLost:
        return QStringLiteral("VisionOutputLost");
    case LocalizationFaultCode::VisionOutputSendFailed:
        return QStringLiteral("VisionOutputSendFailed");
    case LocalizationFaultCode::PlcLost:
        return QStringLiteral("PlcLost");
    case LocalizationFaultCode::PatternInvalid:
        return QStringLiteral("PatternInvalid");
    case LocalizationFaultCode::CalibrationInvalid:
        return QStringLiteral("CalibrationInvalid");
    case LocalizationFaultCode::InternalError:
        return QStringLiteral("InternalError");
    }

    return QStringLiteral("Unknown");
}

/// Returns the numeric fault-code value (e.g. for publishing over "nFaultCode").
/// @param code fault code to convert
/// @return the underlying int value of `code`
inline int localizationFaultCodeValue(LocalizationFaultCode code)
{
    return static_cast<int>(code);
}

} // namespace vc::model

Q_DECLARE_METATYPE(vc::model::LocalizationFaultCode)

#endif // LOCALIZATION_FAULT_CODE_H
