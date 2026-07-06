#ifndef VISION_RESULT_ADAPTER_H
#define VISION_RESULT_ADAPTER_H

#include <QMap>
#include <QSize>
#include <QVariant>

#include "matching/image_matcher.h"
#include "model/camera_workspace.h"
#include "model/localization_runtime_controller.h"
#include "ui/widgets/vision/vision_overlay_types.h"

class VisionResultAdapter {
public:
    static VisionResultOverlay fromMatchResult(
        const mtc::MatchResult &result,
        const QSize &sourceImageSize,
        const vc::model::CameraWorkspace *workspace = nullptr,
        const QMap<QString, QVariant> *runtimeSignalValues = nullptr);

    static VisionResultOverlay fromCycleResult(
        const vc::model::LocalizationRuntimeController::CycleResult &result,
        const vc::model::CameraWorkspace *workspace = nullptr,
        const QMap<QString, QVariant> *runtimeSignalValues = nullptr);
};

#endif // VISION_RESULT_ADAPTER_H
