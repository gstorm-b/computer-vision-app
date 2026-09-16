#ifndef LOCALIZATION_FAULT_CODE_H
#define LOCALIZATION_FAULT_CODE_H

#include <QString>

/**
 * @file localization_fault_code.h
 * @brief LocalizationFaultCode — fault codes reported by the localization task's
 *        runtime/recovery logic, and their name/value conversion helpers.
 */

namespace vc::model {

/**
 * @enum LocalizationFaultCode
 * @brief Fault codes reported by the localization task's runtime/recovery logic (see
 *        LocalizationRuntimeController).
 *
 * Grouped by subsystem: 100s camera, 200s vision output, 300s PLC, 400s pattern/calibration,
 * 500s internal.
 */
enum class LocalizationFaultCode : int {
    None = 0,                     ///< No fault.
    CameraLost = 100,             ///< Camera connection was lost after being connected.
    CameraConnectFailed = 101,    ///< Camera failed to (re)connect.
    CameraGrabTimeout = 102,      ///< Camera did not deliver a frame within the grab timeout.
    CameraNotRegistered = 103,    ///< Active camera number names no usable camera: it is outside
                                  ///< the legal range, or no camera is registered for it. A
                                  ///< binding/selection problem, NOT a connection one — a camera
                                  ///< that never existed cannot have been lost (CameraLost).
    VisionOutputLost = 200,       ///< Vision-output device connection was lost, or failed to connect.
    VisionOutputSendFailed = 201, ///< Vision-output device failed to send the matching result.
    PlcLost = 300,                ///< Primary PLC connection was lost, or failed to connect.
    PlcWriteFailed = 301,         ///< A handshake output could not be written to the PLC and the
                                  ///< retry budget was exhausted. The link is up — writes are
                                  ///< being refused or lost — which is why this is not PlcLost:
                                  ///< a cable check would find nothing.
    PatternNotRegistered = 400,   ///< Active pattern/match group number names no usable group: out
                                  ///< of range, missing, or holding no usable train image.
    CalibrationInvalid = 401,     ///< Active camera calibration is missing or invalid.
    InternalError = 500,          ///< Unexpected internal error not covered by another code.
};

/**
 * @brief Returns the stable, human-readable identifier for `code` (e.g. "CameraLost"), used for
 *        logging/diagnostics; falls back to "Unknown" for any value not in the switch.
 * @param[in] code fault code to name
 * @return the identifier string for `code`
 */
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
    case LocalizationFaultCode::CameraNotRegistered:
        return QStringLiteral("CameraNotRegistered");
    case LocalizationFaultCode::VisionOutputLost:
        return QStringLiteral("VisionOutputLost");
    case LocalizationFaultCode::VisionOutputSendFailed:
        return QStringLiteral("VisionOutputSendFailed");
    case LocalizationFaultCode::PlcLost:
        return QStringLiteral("PlcLost");
    case LocalizationFaultCode::PlcWriteFailed:
        return QStringLiteral("PlcWriteFailed");
    case LocalizationFaultCode::PatternNotRegistered:
        return QStringLiteral("PatternNotRegistered");
    case LocalizationFaultCode::CalibrationInvalid:
        return QStringLiteral("CalibrationInvalid");
    case LocalizationFaultCode::InternalError:
        return QStringLiteral("InternalError");
    }

    return QStringLiteral("Unknown");
}

/**
 * @brief Returns the numeric fault-code value (e.g. for publishing over "nFaultCode").
 * @param[in] code fault code to convert
 * @return the underlying int value of `code`
 */
inline int localizationFaultCodeValue(LocalizationFaultCode code)
{
    return static_cast<int>(code);
}

} // namespace vc::model

Q_DECLARE_METATYPE(vc::model::LocalizationFaultCode)

#endif // LOCALIZATION_FAULT_CODE_H
