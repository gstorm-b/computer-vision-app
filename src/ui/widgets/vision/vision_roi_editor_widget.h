#ifndef VISION_ROI_EDITOR_WIDGET_H
#define VISION_ROI_EDITOR_WIDGET_H

#include <QWidget>
#include <opencv2/core/mat.hpp>

#include "ui/widgets/vision/vision_overlay_types.h"

class VisionCanvas;
class VisionNumericInspector;
class VisionToolPalette;

/// Composite ROI editing widget combining a VisionToolPalette (mode/undo/redo/delete
/// controls), a VisionCanvas (image + interactive ROI overlay), and a VisionNumericInspector
/// (numeric edit fields for the selected ROI) in a single vertical layout, and wiring the
/// signal/slot connections between them.
class VisionRoiEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit VisionRoiEditorWidget(QWidget *parent = nullptr);

    /// Sets the background image to display from an OpenCV matrix and updates the numeric
    /// inspector's known image size accordingly.
    void setImage(const cv::Mat &image);
    /// Sets the background image to display from a QPixmap and updates the numeric
    /// inspector's known image size accordingly.
    void setImage(const QPixmap &pixmap);
    /// Clears the displayed image, resets the numeric inspector's image size to empty, and
    /// clears its current selection.
    void clearImage();

    /// Replaces the set of user-editable ROIs shown/manipulable on the canvas.
    void setRois(const QVector<VisionRoi> &rois);
    /// @return the current editable ROIs from the canvas.
    QVector<VisionRoi> rois() const;

    /// Replaces the set of read-only auxiliary ROIs drawn for reference alongside the
    /// editable ones.
    void setAuxiliaryRois(const QVector<VisionRoi> &rois);
    /// Enables/disables editing on the canvas and disables the corresponding tool palette
    /// controls (draw/delete) when read-only.
    void setReadOnly(bool readOnly);

    /// @return the currently selected ROI on the canvas (an invalid/empty ROI if none is selected).
    VisionRoi selectedRoi() const;

signals:
    void roisChanged(QVector<VisionRoi> rois);   ///< Emitted when the canvas's editable ROI set changes.
    void selectedRoiChanged(VisionRoi roi);      ///< Emitted when the canvas selection changes; an empty-id ROI means no selection.

private:
    VisionToolPalette *m_toolPalette{nullptr};             ///< Mode/undo/redo/delete/fit toolbar above the canvas.
    VisionCanvas *m_canvas{nullptr};                        ///< Image display and interactive ROI editing surface.
    VisionNumericInspector *m_numericInspector{nullptr};    ///< Numeric field editor for the selected ROI, shown below the canvas.
};

#endif // VISION_ROI_EDITOR_WIDGET_H
