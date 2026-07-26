#ifndef VISION_RESULT_VIEWER_WIDGET_H
#define VISION_RESULT_VIEWER_WIDGET_H

#include <QWidget>
#include <opencv2/core/mat.hpp>

#include "ui/widgets/vision/vision_overlay_types.h"

class QAction;
class QToolButton;
class VisionCanvas;

/// Composite widget presenting a VisionCanvas with a toolbar of view/overlay controls
/// (fit-to-view button and an overlay-visibility menu with per-element toggles); wraps
/// the canvas's image/overlay/selection API and forwards its selection signal.
class VisionResultViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit VisionResultViewerWidget(QWidget *parent = nullptr);

    /// Displays `image` on the canvas (read-only, pan tool mode).
    void setImage(const cv::Mat &image);

    /// Displays `pixmap` on the canvas (read-only, pan tool mode).
    void setImage(const QPixmap &pixmap);

    /// Replaces the current overlay with `overlay`, overriding its visibility with the
    /// toolbar's current toggle state (so a freshly produced result does not reset the
    /// user's overlay selections), clears any selected result object, and pushes the
    /// overlay to the canvas.
    void setOverlay(const VisionResultOverlay &overlay);

    /// Applies `visibility` to the stored overlay, syncs the toolbar toggle actions to
    /// match, and forwards the visibility to the canvas.
    void setOverlayVisibility(const VisionOverlayVisibility &visibility);

    /// Selects the result object at `objectIndex` on the canvas.
    void setSelectedResultObject(int objectIndex);

    /// Clears the canvas's current result-object selection.
    void clearSelectedResultObject();

    /// Clears the selection, clears the canvas image, and resets the stored overlay to
    /// a default-constructed VisionResultOverlay.
    void clearResult();

signals:
    /// Forwarded from VisionCanvas::resultObjectSelectionChanged when the selected
    /// result object changes.
    void resultObjectSelectionChanged(int objectIndex);

private:
    /// Sets each overlay toggle action's checked state from m_overlay.visibility.
    void syncActions();

    /// Reads the current checked state of every overlay toggle action into a
    /// VisionOverlayVisibility.
    VisionOverlayVisibility visibilityFromActions() const;

private:
    VisionCanvas *m_canvas{nullptr};  ///< The underlying read-only, pan-mode vision canvas.
    VisionResultOverlay m_overlay;  ///< Last overlay set via setOverlay(), with visibility kept in sync with the toolbar.
    QToolButton *m_fitButton{nullptr};  ///< Toolbar button that triggers VisionCanvas::fitImageToView.
    QToolButton *m_overlayButton{nullptr};  ///< Toolbar button that pops up the overlay-visibility menu.
    QAction *m_scoreAction{nullptr};  ///< Toggle for VisionOverlayVisibility::showScore.
    QAction *m_angleAction{nullptr};  ///< Toggle for VisionOverlayVisibility::showAngle.
    QAction *m_patternAction{nullptr};  ///< Toggle for VisionOverlayVisibility::showPatternId.
    QAction *m_rejectedAction{nullptr};  ///< Toggle for VisionOverlayVisibility::showRejectedCandidates.
    QAction *m_faultAction{nullptr};  ///< Toggle for VisionOverlayVisibility::showFaultMarkers.
    QAction *m_outputAction{nullptr};  ///< Toggle for VisionOverlayVisibility::showSentToOutputMarkers.
    QAction *m_pickingBoxAction{nullptr};  ///< Toggle for VisionOverlayVisibility::showPickingBoxes.
    QAction *m_signalsAction{nullptr};  ///< Toggle for VisionOverlayVisibility::showRuntimeSignalValues.
};

#endif // VISION_RESULT_VIEWER_WIDGET_H
