#ifndef VISION_GEOMETRY_H
#define VISION_GEOMETRY_H

#include <QPixmap>
#include <QPolygonF>
#include <QRectF>
#include <QSize>
#include <QVector>

#include <opencv2/core/mat.hpp>

#include "ui/widgets/vision/vision_overlay_types.h"

/// Free functions for converting between OpenCV/ROI geometry and Qt geometry types
/// (pixmap conversion, ROI-to-rect/polygon conversion, normalization and clamping).
namespace vision {

/// Converts an 8-bit grayscale, BGR, or BGRA cv::Mat into a QPixmap, copying the pixel
/// data and converting BGR/BGRA channel order to RGB/RGBA.
/// @return a null QPixmap if `mat` is empty or its type is not one of the supported ones
QPixmap pixmapFromMat(const cv::Mat &mat);

/// Returns the axis-aligned rectangle centered at `roi.center` with size `roi.size`;
/// does not account for `roi.angleDeg` (use roiCorners()/roiPolygon() for rotated ROIs).
QRectF rectFromRoi(const VisionRoi &roi);

/// Returns a copy of `roi` with width/height made non-negative and, for
/// VisionRoiShape::AxisAlignedRect, angleDeg forced to 0.
VisionRoi normalizedRoi(const VisionRoi &roi);

/// Normalizes `roi` and clamps its center/size to fit within `imageSize`; for an
/// AxisAlignedRect the resulting rectangle's edges are additionally clipped to the
/// image bounds.
/// @return the normalized (but unclamped) ROI unchanged if `imageSize` is empty
VisionRoi clampRoiToImage(const VisionRoi &roi, const QSize &imageSize);

/// Computes the four corner points of `roi` (top-left, top-right, bottom-right,
/// bottom-left order), rotating them about `roi.center` when the shape is
/// VisionRoiShape::RotatedRect with a non-zero angle.
QVector<QPointF> roiCorners(const VisionRoi &roi);

/// Builds a closed polygon from roiCorners(roi).
QPolygonF roiPolygon(const VisionRoi &roi);

/// Computes the axis-aligned bounding rectangle enclosing all of `points`.
/// @return an empty QRectF when `points` is empty
QRectF boundingRectForPoints(const QVector<QPointF> &points);

} // namespace vision

#endif // VISION_GEOMETRY_H
