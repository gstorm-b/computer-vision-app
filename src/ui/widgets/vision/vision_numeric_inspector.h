#ifndef VISION_NUMERIC_INSPECTOR_H
#define VISION_NUMERIC_INSPECTOR_H

#include <QSize>
#include <QWidget>

#include "ui/widgets/vision/vision_overlay_types.h"

class QLabel;
class NoWheelDoubleSpinBox;

/// Form widget showing/editing a single VisionRoi's numeric fields (center X/Y, width,
/// height, angle) via spin boxes; edits are clamped to the current image bounds and
/// re-emitted through roiEdited().
class VisionNumericInspector : public QWidget {
    Q_OBJECT

public:
    explicit VisionNumericInspector(QWidget *parent = nullptr);

    /// Sets the image bounds used to clamp ROI edits and constrain the spin box ranges.
    void setImageSize(const QSize &imageSize);

    /// Populates the fields from `roi` and enables editing; the angle field is enabled
    /// only when `roi.shape` is VisionRoiShape::RotatedRect. Selection is considered
    /// valid only if `roi.id` is non-empty and `roi.isValid()`.
    void setSelectedRoi(const VisionRoi &roi);

    /// Resets to the "No ROI selected" state and disables all fields.
    void clearSelection();

signals:
    /// Emitted with the edited, image-clamped ROI whenever the user changes a field
    /// while a valid ROI is selected.
    void roiEdited(VisionRoi roi);

private slots:
    /// Builds the current ROI from the field values, clamps it to the image size, and
    /// emits roiEdited(); does nothing if no ROI is currently selected.
    void emitEditedRoi();

private:
    /// Enables/disables all spin boxes; the angle spin box additionally requires the
    /// current ROI shape to be RotatedRect.
    void setFieldsEnabled(bool enabled);

    /// Builds a VisionRoi from m_currentRoi plus the current spin box values (angle is
    /// taken from the field only for RotatedRect, otherwise forced to 0).
    VisionRoi currentRoiFromFields() const;

    /// Recomputes the center/width/height spin box ranges from m_imageSize (falling
    /// back to a minimum of 1 for zero-sized images).
    void updateRanges();

private:
    QSize m_imageSize;  ///< Current image bounds used to clamp edits and size the spin box ranges.
    VisionRoi m_currentRoi;  ///< Last ROI set via setSelectedRoi(), used as the base for field edits.
    bool m_hasSelection{false};  ///< Whether a valid ROI is currently selected (controls field enablement and edit emission).
    QLabel *m_titleLabel{nullptr};  ///< Label showing the selected ROI's name or "No ROI selected".
    NoWheelDoubleSpinBox *m_centerXSpin{nullptr};  ///< Editor for the ROI center X coordinate.
    NoWheelDoubleSpinBox *m_centerYSpin{nullptr};  ///< Editor for the ROI center Y coordinate.
    NoWheelDoubleSpinBox *m_widthSpin{nullptr};  ///< Editor for the ROI width.
    NoWheelDoubleSpinBox *m_heightSpin{nullptr};  ///< Editor for the ROI height.
    NoWheelDoubleSpinBox *m_angleSpin{nullptr};  ///< Editor for the ROI rotation angle; enabled only for RotatedRect ROIs.
};

#endif // VISION_NUMERIC_INSPECTOR_H
