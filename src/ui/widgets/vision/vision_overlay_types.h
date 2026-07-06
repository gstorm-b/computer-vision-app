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

enum class VisionRoiShape {
    AxisAlignedRect,
    RotatedRect,
};

struct VisionRoi {
    QString id;
    QString label;
    VisionRoiShape shape{VisionRoiShape::AxisAlignedRect};
    QPointF center;
    QSizeF size;
    double angleDeg{0.0};
    bool editable{true};
    bool visible{true};
    bool selected{false};
    bool warning{false};
    QColor color;

    bool isValid() const {
        return size.width() > 0.0 && size.height() > 0.0;
    }
};

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

struct VisionResultObject {
    int index{0};
    QString patternName;
    int patternIndex{-1};
    double score{0.0};
    QPointF center;
    QVector<QPointF> corners;
    QVector<QVector<QPointF>> pickingBoxPolygons;
    double matchedAngleDeg{0.0};
    double pointAngleDeg{0.0};
    bool rejected{false};
    bool faulted{false};
    bool sentToOutput{false};
    bool possibleToPick{false};
    bool hasCollision{false};
    bool outsideConditionRoi{false};
};

struct VisionOverlayVisibility {
    bool showScore{true};
    bool showAngle{true};
    bool showPatternId{true};
    bool showRejectedCandidates{true};
    bool showFaultMarkers{true};
    bool showSentToOutputMarkers{true};
    bool showPickingBoxes{false};
    bool showRuntimeSignalValues{false};
};

struct VisionResultOverlay {
    QSize sourceImageSize;
    QVector<VisionResultObject> acceptedObjects;
    QVector<VisionResultObject> rejectedObjects;
    QVector<QPointF> pickingPoints;
    QVector<VisionRoi> roiOverlays;
    QMap<QString, QVariant> runtimeSignalValues;
    VisionOverlayVisibility visibility;

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
