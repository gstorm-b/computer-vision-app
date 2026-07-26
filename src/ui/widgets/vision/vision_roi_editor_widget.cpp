#include "ui/widgets/vision/vision_roi_editor_widget.h"

#include <QVBoxLayout>

#include "ui/widgets/vision/vision_canvas.h"
#include "ui/widgets/vision/vision_numeric_inspector.h"
#include "ui/widgets/vision/vision_tool_palette.h"

/// Builds the tool palette / canvas / numeric inspector child widgets, lays them out
/// vertically (palette and inspector fixed, canvas stretched to fill remaining space),
/// and connects tool-mode, undo/redo, delete, fit, selection, and ROI-edit signals between
/// them so the three widgets stay in sync.
VisionRoiEditorWidget::VisionRoiEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_toolPalette = new VisionToolPalette(this);
    m_canvas = new VisionCanvas(this);
    m_numericInspector = new VisionNumericInspector(this);

    layout->addWidget(m_toolPalette);
    layout->addWidget(m_canvas, 1);
    layout->addWidget(m_numericInspector);

    connect(m_toolPalette, &VisionToolPalette::toolModeRequested,
            m_canvas, &VisionCanvas::setToolMode);
    connect(m_canvas, &VisionCanvas::toolModeChanged,
            m_toolPalette, &VisionToolPalette::setActiveMode);
    connect(m_toolPalette, &VisionToolPalette::deleteRequested,
            m_canvas, &VisionCanvas::deleteSelectedRoi);
    connect(m_toolPalette, &VisionToolPalette::fitRequested,
            m_canvas, &VisionCanvas::fitImageToView);
    connect(m_toolPalette, &VisionToolPalette::undoRequested,
            m_canvas, &VisionCanvas::undo);
    connect(m_toolPalette, &VisionToolPalette::redoRequested,
            m_canvas, &VisionCanvas::redo);
    connect(m_canvas, &VisionCanvas::selectedRoiChanged,
            this, [this](const VisionRoi &roi) {
                if (roi.id.isEmpty()) {
                    m_numericInspector->clearSelection();
                } else {
                    m_numericInspector->setSelectedRoi(roi);
                }
                emit selectedRoiChanged(roi);
            });
    connect(m_canvas, &VisionCanvas::roisChanged,
            this, &VisionRoiEditorWidget::roisChanged);
    connect(m_canvas, &VisionCanvas::undoAvailabilityChanged,
            this, [this](bool canUndo, bool canRedo) {
                m_toolPalette->setUndoAvailable(canUndo);
                m_toolPalette->setRedoAvailable(canRedo);
            });
    connect(m_numericInspector, &VisionNumericInspector::roiEdited,
            m_canvas, &VisionCanvas::updateSelectedRoi);
}

/// @param image OpenCV matrix to display as the canvas background.
void VisionRoiEditorWidget::setImage(const cv::Mat &image)
{
    m_canvas->setImage(image);
    m_numericInspector->setImageSize(m_canvas->imageSize());
}

/// @param pixmap pre-rendered pixmap to display as the canvas background.
void VisionRoiEditorWidget::setImage(const QPixmap &pixmap)
{
    m_canvas->setImage(pixmap);
    m_numericInspector->setImageSize(m_canvas->imageSize());
}

/// Clears the canvas's displayed image and resets the numeric inspector to an empty image
/// size with no active selection.
void VisionRoiEditorWidget::clearImage()
{
    m_canvas->clearImage();
    m_numericInspector->setImageSize({});
    m_numericInspector->clearSelection();
}

/// Forwards `rois` to the canvas as the editable ROI set, replacing any ROIs previously set.
void VisionRoiEditorWidget::setRois(const QVector<VisionRoi> &rois)
{
    m_canvas->setEditableRois(rois);
}

/// @return the canvas's current editable ROI set.
QVector<VisionRoi> VisionRoiEditorWidget::rois() const
{
    return m_canvas->editableRois();
}

/// Forwards `rois` to the canvas as the read-only auxiliary ROI overlay, replacing any
/// previously set.
void VisionRoiEditorWidget::setAuxiliaryRois(const QVector<VisionRoi> &rois)
{
    m_canvas->setAuxiliaryRois(rois);
}

/// Propagates `readOnly` to both the canvas (disables ROI editing) and the tool palette
/// (disables the draw/delete controls).
void VisionRoiEditorWidget::setReadOnly(bool readOnly)
{
    m_canvas->setReadOnly(readOnly);
    m_toolPalette->setReadOnly(readOnly);
}

/// @return the ROI currently selected on the canvas.
VisionRoi VisionRoiEditorWidget::selectedRoi() const
{
    return m_canvas->selectedRoi();
}
