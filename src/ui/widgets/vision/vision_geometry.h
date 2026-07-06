#ifndef VISION_GEOMETRY_H
#define VISION_GEOMETRY_H

#include <QPixmap>
#include <QPolygonF>
#include <QRectF>
#include <QSize>
#include <QVector>

#include <opencv2/core/mat.hpp>

#include "ui/widgets/vision/vision_overlay_types.h"

namespace vision {

QPixmap pixmapFromMat(const cv::Mat &mat);
QRectF rectFromRoi(const VisionRoi &roi);
VisionRoi normalizedRoi(const VisionRoi &roi);
VisionRoi clampRoiToImage(const VisionRoi &roi, const QSize &imageSize);
QVector<QPointF> roiCorners(const VisionRoi &roi);
QPolygonF roiPolygon(const VisionRoi &roi);
QRectF boundingRectForPoints(const QVector<QPointF> &points);

} // namespace vision

#endif // VISION_GEOMETRY_H
