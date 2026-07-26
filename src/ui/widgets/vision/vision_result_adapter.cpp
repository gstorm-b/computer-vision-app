#include "ui/widgets/vision/vision_result_adapter.h"

#include "core/utils/theme_manager.h"
#include "ui/widgets/vision/vision_geometry.h"

namespace {

/// Builds a non-editable, axis-aligned VisionRoi representing a workspace/condition
/// ROI rectangle: `roi.visible` is true only when `rect` has positive width and height.
VisionRoi makeWorkspaceRoi(const QString &id,
                           const QString &label,
                           const cv::Rect2f &rect,
                           const QColor &color)
{
    VisionRoi roi;
    roi.id = id;
    roi.label = label;
    roi.shape = VisionRoiShape::AxisAlignedRect;
    roi.center = QPointF(rect.x + rect.width * 0.5, rect.y + rect.height * 0.5);
    roi.size = QSizeF(rect.width, rect.height);
    roi.editable = false;
    roi.visible = rect.width > 0.0f && rect.height > 0.0f;
    roi.color = color;
    return roi;
}

/// Converts a single mtc::MatchedObject into a VisionResultObject: copies pattern
/// name/index, score, center, and the four corner points (all offset by `offset`),
/// collects picking-box polygons (preferring the gripper box's left/right point sets,
/// falling back to the rotated pickingBox rect when neither gripper polygon is
/// present), and derives rejected/faulted from the object's collision, condition-ROI,
/// and pick-eligibility state.
VisionResultObject makeResultObject(const mtc::MatchedObject &object,
                                    const QPointF &offset)
{
    VisionResultObject resultObject;
    resultObject.patternName = QString::fromStdWString(object.pattern_name);
    resultObject.patternIndex = object.pattern_index;
    resultObject.score = object.matched_Score;
    resultObject.center = QPointF(object.point_Center.x + offset.x(),
                                  object.point_Center.y + offset.y());
    resultObject.corners = {
        QPointF(object.point_LT.x + offset.x(), object.point_LT.y + offset.y()),
        QPointF(object.point_RT.x + offset.x(), object.point_RT.y + offset.y()),
        QPointF(object.point_RB.x + offset.x(), object.point_RB.y + offset.y()),
        QPointF(object.point_LB.x + offset.x(), object.point_LB.y + offset.y()),
    };
    const auto appendPickingPolygon = [&resultObject, &offset](const std::vector<cv::Point2f> &points) {
        if (points.size() != 4) return;
        QVector<QPointF> polygon;
        polygon.reserve(4);
        for (const cv::Point2f &point : points) {
            polygon.append(QPointF(point.x + offset.x(), point.y + offset.y()));
        }
        resultObject.pickingBoxPolygons.append(polygon);
    };

    const mtc::MatchedObject::GripperBox &gripperBox = object.gripperBox();
    appendPickingPolygon(gripperBox.box_left_pts);
    appendPickingPolygon(gripperBox.box_right_pts);

    if (resultObject.pickingBoxPolygons.isEmpty()
        && object.pickingBox.size.width > 0.0f
        && object.pickingBox.size.height > 0.0f) {
        cv::Point2f boxPoints[4];
        object.pickingBox.points(boxPoints);
        QVector<QPointF> polygon;
        polygon.reserve(4);
        for (const cv::Point2f &point : boxPoints) {
            polygon.append(QPointF(point.x + offset.x(), point.y + offset.y()));
        }
        resultObject.pickingBoxPolygons.append(polygon);
    }
    resultObject.matchedAngleDeg = object.matched_Angle;
    resultObject.pointAngleDeg = object.point_angle;
    resultObject.possibleToPick = object.isPossibleToPick();
    resultObject.hasCollision = object.hasCollision();
    resultObject.outsideConditionRoi = object.isOutsideConditionRoi();
    resultObject.rejected = object.hasCollision()
                         || object.isOutsideConditionRoi()
                         || !object.isPossibleToPick();
    resultObject.faulted = object.hasCollision() || object.isOutsideConditionRoi();
    return resultObject;
}

/// Appends the workspace ROI and/or condition ROI (when present on `workspace`) to
/// `overlay->roiOverlays`, colored via the active theme's accent/info token when the
/// corresponding "use" flag is enabled, or the muted text token otherwise. Does
/// nothing if `overlay` or `workspace` is null.
void appendWorkspaceOverlays(VisionResultOverlay *overlay,
                             const vc::model::CameraWorkspace *workspace)
{
    if (!overlay || !workspace) return;

    if (workspace->hasRoi()) {
        overlay->roiOverlays.append(
            makeWorkspaceRoi(QStringLiteral("workspace"),
                             QStringLiteral("Workspace ROI"),
                             workspace->roi,
                             workspace->useWorkspace
                                 ? QColor(ThemeManager::tokenValue(QStringLiteral("accent.primary"), ThemeManager::instance()->isDark()))
                                 : QColor(ThemeManager::tokenValue(QStringLiteral("text.muted"), ThemeManager::instance()->isDark()))));
    }

    if (workspace->hasConditionRoi()) {
        overlay->roiOverlays.append(
            makeWorkspaceRoi(QStringLiteral("condition"),
                             QStringLiteral("Condition ROI"),
                             workspace->conditionRoi,
                             workspace->useConditionWorkspace
                                 ? QColor(ThemeManager::tokenValue(QStringLiteral("state.info"), ThemeManager::instance()->isDark()))
                                 : QColor(ThemeManager::tokenValue(QStringLiteral("text.muted"), ThemeManager::instance()->isDark()))));
    }
}

} // namespace

VisionResultOverlay VisionResultAdapter::fromMatchResult(
    const mtc::MatchResult &result,
    const QSize &sourceImageSize,
    const vc::model::CameraWorkspace *workspace,
    const QMap<QString, QVariant> *runtimeSignalValues)
{
    VisionResultOverlay overlay;
    overlay.sourceImageSize = sourceImageSize;
    if (runtimeSignalValues) {
        overlay.runtimeSignalValues = *runtimeSignalValues;
    }

    const QPointF offset(result.cropOffsetPoint.x, result.cropOffsetPoint.y);
    int nextIndex = 1;
    for (const mtc::MatchedObject &object : result.Objects) {
        VisionResultObject resultObject = makeResultObject(object, offset);
        resultObject.index = nextIndex++;
        if (resultObject.rejected) {
            overlay.rejectedObjects.append(resultObject);
        } else {
            overlay.acceptedObjects.append(resultObject);
            overlay.pickingPoints.append(resultObject.center);
        }
    }

    appendWorkspaceOverlays(&overlay, workspace);
    return overlay;
}

VisionResultOverlay VisionResultAdapter::fromCycleResult(
    const vc::model::LocalizationRuntimeController::CycleResult &result,
    const vc::model::CameraWorkspace *workspace,
    const QMap<QString, QVariant> *runtimeSignalValues)
{
    QSize imageSize;
    if (!result.rawImage.empty()) {
        imageSize = QSize(result.rawImage.cols, result.rawImage.rows);
    } else if (!result.matchResult.Image.empty()) {
        imageSize = QSize(result.matchResult.Image.cols, result.matchResult.Image.rows);
    } else {
        imageSize = result.displayImage.empty()
            ? QSize()
            : QSize(result.displayImage.cols, result.displayImage.rows);
    }

    VisionResultOverlay overlay = fromMatchResult(result.matchResult, imageSize,
                                                  workspace, runtimeSignalValues);

    for (int index = 0; index < result.rows.size(); ++index) {
        const auto &row = result.rows.at(index);
        const bool sent = row.status.startsWith(QStringLiteral("Sent"));
        if (index < overlay.acceptedObjects.size()) {
            overlay.acceptedObjects[index].sentToOutput = sent;
        }
    }

    return overlay;
}
