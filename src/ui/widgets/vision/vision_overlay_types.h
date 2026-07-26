#ifndef VISION_OVERLAY_TYPES_H
#define VISION_OVERLAY_TYPES_H

#include <QColor>
#include <QMap>
#include <QMetaType>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QtGlobal>
#include <QVariant>
#include <QVector>

/// Shape kind for a VisionRoi: whether it is an unrotated axis-aligned rectangle or a
/// rectangle that may carry a rotation angle.
enum class VisionRoiShape {
    AxisAlignedRect,
    RotatedRect,
};

/// A single region of interest overlaid on a vision image: geometry (center/size/angle),
/// display metadata (label, color, visibility/selection/warning flags), and whether the
/// user can edit it.
struct VisionRoi {
    QString id;  ///< Stable identifier used to look up/update this ROI (e.g. session key, "workspace", "condition").
    QString label;  ///< Human-readable name shown in the UI (e.g. numeric inspector title).
    VisionRoiShape shape{VisionRoiShape::AxisAlignedRect};  ///< Whether the ROI is an unrotated rectangle or may be rotated.
    QPointF center;  ///< Center point of the ROI, in source-image pixel coordinates.
    QSizeF size;  ///< Width/height of the ROI, in source-image pixel units.
    double angleDeg{0.0};  ///< Rotation angle in degrees, meaningful only when shape is RotatedRect.
    bool editable{true};  ///< Whether the user is allowed to move/resize/rotate this ROI in the UI.
    bool visible{true};  ///< Whether this ROI should be drawn.
    bool selected{false};  ///< Whether this ROI is the currently selected one in the UI.
    bool warning{false};  ///< Whether this ROI should be rendered with a warning indication.
    QColor color;  ///< Display color for this ROI's outline.

    /// True when both dimensions of `size` are strictly positive.
    bool isValid() const {
        return size.width() > 0.0 && size.height() > 0.0;
    }
};

/// Field-wise equality of two VisionRoi values (angleDeg compared with qFuzzyCompare
/// tolerance).
inline bool operator==(const VisionRoi &lhs, const VisionRoi &rhs)
{
    return lhs.id == rhs.id
        && lhs.label == rhs.label
        && lhs.shape == rhs.shape
        && lhs.center == rhs.center
        && lhs.size == rhs.size
        && qFuzzyCompare(lhs.angleDeg + 1.0, rhs.angleDeg + 1.0)
        && lhs.editable == rhs.editable
        && lhs.visible == rhs.visible
        && lhs.selected == rhs.selected
        && lhs.warning == rhs.warning
        && lhs.color == rhs.color;
}

/// A single matched/candidate object produced by a vision matching cycle, translated
/// into UI-friendly fields for overlay drawing (position, corners, picking-box
/// polygons, and the accept/reject/fault/pick-eligibility flags derived from the match).
struct VisionResultObject {
    int index{0};  ///< 1-based display index assigned when building the overlay (see VisionResultAdapter).
    QString patternName;  ///< Name of the matched pattern, as reported by the matcher.
    int patternIndex{-1};  ///< Index of the matched pattern, or -1 if not applicable.
    double score{0.0};  ///< Match score reported by the matcher.
    QPointF center;  ///< Matched object center, in source-image pixel coordinates.
    QVector<QPointF> corners;  ///< The object's four corner points (LT, RT, RB, LB order), in source-image coordinates.
    QVector<QVector<QPointF>> pickingBoxPolygons;  ///< Gripper/picking-box outlines associated with this object, each a closed polygon.
    double matchedAngleDeg{0.0};  ///< Matched orientation angle, in degrees.
    double pointAngleDeg{0.0};  ///< Reported picking-point angle, in degrees.
    bool rejected{false};  ///< True when this object was excluded from the accepted set (collision, outside condition ROI, or not pickable).
    bool faulted{false};  ///< True when this object triggered a collision or fell outside the condition ROI.
    bool sentToOutput{false};  ///< True when the runtime reported this object as sent to output.
    bool possibleToPick{false};  ///< Whether the matcher judged this object pickable.
    bool hasCollision{false};  ///< Whether the matcher detected a collision for this object.
    bool outsideConditionRoi{false};  ///< Whether this object falls outside the configured condition ROI.
};

/// Toggle flags controlling which overlay elements the vision canvas/result viewer draws.
struct VisionOverlayVisibility {
    bool showScore{true};  ///< Whether to draw each object's match score.
    bool showAngle{true};  ///< Whether to draw each object's matched/point angle.
    bool showPatternId{true};  ///< Whether to draw each object's pattern name/index.
    bool showRejectedCandidates{true};  ///< Whether to draw objects in VisionResultOverlay::rejectedObjects.
    bool showFaultMarkers{true};  ///< Whether to mark objects with VisionResultObject::faulted.
    bool showSentToOutputMarkers{true};  ///< Whether to mark objects with VisionResultObject::sentToOutput.
    bool showPickingBoxes{false};  ///< Whether to draw VisionResultObject::pickingBoxPolygons.
    bool showRuntimeSignalValues{false};  ///< Whether to draw VisionResultOverlay::runtimeSignalValues on the canvas.
};

/// Complete set of overlay data drawn on top of a source vision image: matched
/// objects (accepted/rejected), picking points, configured ROIs, runtime signal
/// values, and the current visibility toggles.
struct VisionResultOverlay {
    QSize sourceImageSize;  ///< Size, in pixels, of the image this overlay was computed against.
    QVector<VisionResultObject> acceptedObjects;  ///< Matched objects eligible for picking/output.
    QVector<VisionResultObject> rejectedObjects;  ///< Matched objects excluded due to collision, condition ROI, or pick-eligibility.
    QVector<QPointF> pickingPoints;  ///< Picking points, one per accepted object, in source-image coordinates.
    QVector<VisionRoi> roiOverlays;  ///< ROIs to draw alongside the result (workspace ROI, condition ROI, pattern ROIs).
    QMap<QString, QVariant> runtimeSignalValues;  ///< Named runtime signal values to optionally display on the canvas.
    VisionOverlayVisibility visibility;  ///< Which of the above elements are currently shown.

    /// True when there is no image size, no accepted/rejected objects, no ROI
    /// overlays, and no runtime signal values.
    bool isEmpty() const {
        return sourceImageSize.isEmpty()
            && acceptedObjects.isEmpty()
            && rejectedObjects.isEmpty()
            && roiOverlays.isEmpty()
            && runtimeSignalValues.isEmpty();
    }
};

Q_DECLARE_METATYPE(VisionRoi)
Q_DECLARE_METATYPE(VisionResultObject)
Q_DECLARE_METATYPE(VisionResultOverlay)

#endif // VISION_OVERLAY_TYPES_H
