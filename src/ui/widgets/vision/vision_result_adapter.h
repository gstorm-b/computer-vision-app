#ifndef VISION_RESULT_ADAPTER_H
#define VISION_RESULT_ADAPTER_H

#include <QMap>
#include <QSize>
#include <QVariant>

#include "matching/image_matcher.h"
#include "model/camera_workspace.h"
#include "model/localization_runtime_controller.h"
#include "ui/widgets/vision/vision_overlay_types.h"

/// Stateless adapter that translates matcher/runtime result types (mtc::MatchResult,
/// vc::model::LocalizationRuntimeController::CycleResult) into the UI-facing
/// VisionResultOverlay used by the vision canvas and result viewer widget.
class VisionResultAdapter {
public:
    /// Converts a raw match result into a VisionResultOverlay: splits matched objects
    /// into accepted/rejected sets (rejected = collision, outside condition ROI, or not
    /// pickable), offsets all coordinates by `result.cropOffsetPoint`, assigns 1-based
    /// display indices, and appends workspace/condition ROI overlays when `workspace`
    /// is provided.
    /// @param sourceImageSize size of the image the result was computed against
    /// @param runtimeSignalValues optional signal values copied verbatim into the overlay
    static VisionResultOverlay fromMatchResult(
        const mtc::MatchResult &result,
        const QSize &sourceImageSize,
        const vc::model::CameraWorkspace *workspace = nullptr,
        const QMap<QString, QVariant> *runtimeSignalValues = nullptr);

    /// Builds a VisionResultOverlay from a full runtime cycle result: derives the
    /// image size from the first non-empty of rawImage/matchResult.Image/displayImage,
    /// delegates to fromMatchResult() for the object/ROI overlay data, then marks each
    /// accepted object's sentToOutput flag from the corresponding row's status
    /// (true when the status text starts with "Sent").
    static VisionResultOverlay fromCycleResult(
        const vc::model::LocalizationRuntimeController::CycleResult &result,
        const vc::model::CameraWorkspace *workspace = nullptr,
        const QMap<QString, QVariant> *runtimeSignalValues = nullptr);
};

#endif // VISION_RESULT_ADAPTER_H
