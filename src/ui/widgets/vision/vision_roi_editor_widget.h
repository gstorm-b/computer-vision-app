#ifndef VISION_ROI_EDITOR_WIDGET_H
#define VISION_ROI_EDITOR_WIDGET_H

#include <QWidget>
#include <opencv2/core/mat.hpp>

#include "ui/widgets/vision/vision_overlay_types.h"

class VisionCanvas;
class VisionNumericInspector;
class VisionToolPalette;

class VisionRoiEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit VisionRoiEditorWidget(QWidget *parent = nullptr);

    void setImage(const cv::Mat &image);
    void setImage(const QPixmap &pixmap);
    void clearImage();

    void setRois(const QVector<VisionRoi> &rois);
    QVector<VisionRoi> rois() const;

    void setAuxiliaryRois(const QVector<VisionRoi> &rois);
    void setReadOnly(bool readOnly);

    VisionRoi selectedRoi() const;

signals:
    void roisChanged(QVector<VisionRoi> rois);
    void selectedRoiChanged(VisionRoi roi);

private:
    VisionToolPalette *m_toolPalette{nullptr};
    VisionCanvas *m_canvas{nullptr};
    VisionNumericInspector *m_numericInspector{nullptr};
};

#endif // VISION_ROI_EDITOR_WIDGET_H
