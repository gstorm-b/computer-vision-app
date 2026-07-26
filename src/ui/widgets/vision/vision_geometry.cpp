#include "ui/widgets/vision/vision_geometry.h"

#include <QImage>
#include <QtMath>

#include <opencv2/imgproc.hpp>

namespace {

/// Rotates `point` about `center` by `angleDeg` degrees (counter-clockwise in Qt's
/// y-down coordinate space, matching qDegreesToRadians conventions).
QPointF rotatePoint(const QPointF &point, const QPointF &center, double angleDeg)
{
    const double radians = qDegreesToRadians(angleDeg);
    const double s = std::sin(radians);
    const double c = std::cos(radians);
    const QPointF translated = point - center;
    return QPointF(translated.x() * c - translated.y() * s,
                   translated.x() * s + translated.y() * c) + center;
}

} // namespace

namespace vision {

QPixmap pixmapFromMat(const cv::Mat &mat)
{
    if (mat.empty()) return {};

    QImage image;
    if (mat.type() == CV_8UC1) {
        image = QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step),
                       QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        image = QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step),
                       QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        image = QImage(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step),
                       QImage::Format_RGBA8888).copy();
    } else {
        return {};
    }

    return QPixmap::fromImage(image);
}

QRectF rectFromRoi(const VisionRoi &roi)
{
    return QRectF(roi.center.x() - roi.size.width() * 0.5,
                  roi.center.y() - roi.size.height() * 0.5,
                  roi.size.width(),
                  roi.size.height());
}

VisionRoi normalizedRoi(const VisionRoi &roi)
{
    VisionRoi normalized = roi;
    normalized.size.setWidth(qAbs(normalized.size.width()));
    normalized.size.setHeight(qAbs(normalized.size.height()));
    if (normalized.shape == VisionRoiShape::AxisAlignedRect) {
        normalized.angleDeg = 0.0;
    }
    return normalized;
}

VisionRoi clampRoiToImage(const VisionRoi &roi, const QSize &imageSize)
{
    VisionRoi clamped = normalizedRoi(roi);
    if (imageSize.isEmpty()) return clamped;

    clamped.center.setX(qBound(0.0, clamped.center.x(),
                               static_cast<double>(imageSize.width())));
    clamped.center.setY(qBound(0.0, clamped.center.y(),
                               static_cast<double>(imageSize.height())));

    clamped.size.setWidth(qBound(1.0, clamped.size.width(),
                                 static_cast<double>(imageSize.width())));
    clamped.size.setHeight(qBound(1.0, clamped.size.height(),
                                  static_cast<double>(imageSize.height())));

    if (clamped.shape == VisionRoiShape::AxisAlignedRect) {
        QRectF rect = rectFromRoi(clamped);
        rect.setLeft(qMax(0.0, rect.left()));
        rect.setTop(qMax(0.0, rect.top()));
        rect.setRight(qMin(rect.right(), static_cast<double>(imageSize.width())));
        rect.setBottom(qMin(rect.bottom(), static_cast<double>(imageSize.height())));
        clamped.center = rect.center();
        clamped.size = rect.size();
    }

    return clamped;
}

QVector<QPointF> roiCorners(const VisionRoi &roi)
{
    const VisionRoi normalized = normalizedRoi(roi);
    const QRectF rect = rectFromRoi(normalized);
    QVector<QPointF> corners{
        rect.topLeft(),
        rect.topRight(),
        rect.bottomRight(),
        rect.bottomLeft(),
    };

    if (normalized.shape == VisionRoiShape::RotatedRect && !qFuzzyIsNull(normalized.angleDeg)) {
        for (QPointF &point : corners) {
            point = rotatePoint(point, normalized.center, normalized.angleDeg);
        }
    }

    return corners;
}

QPolygonF roiPolygon(const VisionRoi &roi)
{
    QPolygonF polygon;
    const QVector<QPointF> corners = roiCorners(roi);
    for (const QPointF &point : corners) {
        polygon << point;
    }
    return polygon;
}

QRectF boundingRectForPoints(const QVector<QPointF> &points)
{
    if (points.isEmpty()) return {};
    QRectF rect(points.first(), QSizeF(0.0, 0.0));
    for (const QPointF &point : points) {
        rect |= QRectF(point, QSizeF(0.0, 0.0));
    }
    return rect;
}

} // namespace vision
