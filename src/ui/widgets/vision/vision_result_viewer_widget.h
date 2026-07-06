#ifndef VISION_RESULT_VIEWER_WIDGET_H
#define VISION_RESULT_VIEWER_WIDGET_H

#include <QWidget>
#include <opencv2/core/mat.hpp>

#include "ui/widgets/vision/vision_overlay_types.h"

class QAction;
class QToolButton;
class VisionCanvas;

class VisionResultViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit VisionResultViewerWidget(QWidget *parent = nullptr);

    void setImage(const cv::Mat &image);
    void setImage(const QPixmap &pixmap);
    void setOverlay(const VisionResultOverlay &overlay);
    void setOverlayVisibility(const VisionOverlayVisibility &visibility);
    void setSelectedResultObject(int objectIndex);
    void clearSelectedResultObject();
    void clearResult();

signals:
    void resultObjectSelectionChanged(int objectIndex);

private:
    void syncActions();
    VisionOverlayVisibility visibilityFromActions() const;

private:
    VisionCanvas *m_canvas{nullptr};
    VisionResultOverlay m_overlay;
    QToolButton *m_fitButton{nullptr};
    QToolButton *m_overlayButton{nullptr};
    QAction *m_scoreAction{nullptr};
    QAction *m_angleAction{nullptr};
    QAction *m_patternAction{nullptr};
    QAction *m_rejectedAction{nullptr};
    QAction *m_faultAction{nullptr};
    QAction *m_outputAction{nullptr};
    QAction *m_pickingBoxAction{nullptr};
    QAction *m_signalsAction{nullptr};
};

#endif // VISION_RESULT_VIEWER_WIDGET_H
