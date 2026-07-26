#include "ui/widgets/vision/vision_numeric_inspector.h"

#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>

#include "ui/widgets/no_wheel_double_spinbox.h"
#include "ui/widgets/vision/vision_geometry.h"

namespace {

/// Constructs a NoWheelDoubleSpinBox configured with the given range, step, and
/// decimal precision.
NoWheelDoubleSpinBox *makeSpinBox(double min, double max, double step, int decimals)
{
    auto *spin = new NoWheelDoubleSpinBox;
    spin->setRange(min, max);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    return spin;
}

} // namespace

/// Builds the form layout (title label plus center X/Y, width, height, and angle spin
/// boxes), wires every spin box's valueChanged to emitEditedRoi(), and starts in the
/// cleared/no-selection state.
VisionNumericInspector::VisionNumericInspector(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QFormLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_titleLabel = new QLabel(tr("No ROI selected"), this);
    m_centerXSpin = makeSpinBox(0.0, 999999.0, 1.0, 1);
    m_centerYSpin = makeSpinBox(0.0, 999999.0, 1.0, 1);
    m_widthSpin = makeSpinBox(1.0, 999999.0, 1.0, 1);
    m_heightSpin = makeSpinBox(1.0, 999999.0, 1.0, 1);
    m_angleSpin = makeSpinBox(-180.0, 180.0, 1.0, 1);

    layout->addRow(m_titleLabel);
    layout->addRow(tr("Center X"), m_centerXSpin);
    layout->addRow(tr("Center Y"), m_centerYSpin);
    layout->addRow(tr("Width"), m_widthSpin);
    layout->addRow(tr("Height"), m_heightSpin);
    layout->addRow(tr("Angle"), m_angleSpin);

    for (NoWheelDoubleSpinBox *spin : {m_centerXSpin, m_centerYSpin, m_widthSpin,
                                       m_heightSpin, m_angleSpin}) {
        connect(spin, qOverload<double>(&NoWheelDoubleSpinBox::valueChanged),
                this, [this](double) { emitEditedRoi(); });
    }

    clearSelection();
}

void VisionNumericInspector::setImageSize(const QSize &imageSize)
{
    m_imageSize = imageSize;
    updateRanges();
}

void VisionNumericInspector::setSelectedRoi(const VisionRoi &roi)
{
    m_hasSelection = !roi.id.isEmpty() && roi.isValid();
    m_currentRoi = roi;
    updateRanges();

    const QSignalBlocker blockX(m_centerXSpin);
    const QSignalBlocker blockY(m_centerYSpin);
    const QSignalBlocker blockW(m_widthSpin);
    const QSignalBlocker blockH(m_heightSpin);
    const QSignalBlocker blockA(m_angleSpin);

    m_centerXSpin->setValue(roi.center.x());
    m_centerYSpin->setValue(roi.center.y());
    m_widthSpin->setValue(roi.size.width());
    m_heightSpin->setValue(roi.size.height());
    m_angleSpin->setValue(roi.shape == VisionRoiShape::RotatedRect ? roi.angleDeg : 0.0);
    m_angleSpin->setEnabled(roi.shape == VisionRoiShape::RotatedRect);

    m_titleLabel->setText(roi.label.isEmpty()
                              ? tr("Selected ROI")
                              : tr("Selected ROI: %1").arg(roi.label));
    setFieldsEnabled(m_hasSelection);
}

void VisionNumericInspector::clearSelection()
{
    m_hasSelection = false;
    m_currentRoi = VisionRoi();
    m_titleLabel->setText(tr("No ROI selected"));
    setFieldsEnabled(false);
}

void VisionNumericInspector::emitEditedRoi()
{
    if (!m_hasSelection) return;
    VisionRoi roi = vision::clampRoiToImage(currentRoiFromFields(), m_imageSize);
    m_currentRoi = roi;
    emit roiEdited(roi);
}

void VisionNumericInspector::setFieldsEnabled(bool enabled)
{
    m_centerXSpin->setEnabled(enabled);
    m_centerYSpin->setEnabled(enabled);
    m_widthSpin->setEnabled(enabled);
    m_heightSpin->setEnabled(enabled);
    m_angleSpin->setEnabled(enabled && m_currentRoi.shape == VisionRoiShape::RotatedRect);
}

VisionRoi VisionNumericInspector::currentRoiFromFields() const
{
    VisionRoi roi = m_currentRoi;
    roi.center.setX(m_centerXSpin->value());
    roi.center.setY(m_centerYSpin->value());
    roi.size.setWidth(m_widthSpin->value());
    roi.size.setHeight(m_heightSpin->value());
    roi.angleDeg = (roi.shape == VisionRoiShape::RotatedRect) ? m_angleSpin->value() : 0.0;
    return roi;
}

void VisionNumericInspector::updateRanges()
{
    const double maxWidth = qMax(1, m_imageSize.width());
    const double maxHeight = qMax(1, m_imageSize.height());
    m_centerXSpin->setRange(0.0, maxWidth);
    m_centerYSpin->setRange(0.0, maxHeight);
    m_widthSpin->setRange(1.0, maxWidth);
    m_heightSpin->setRange(1.0, maxHeight);
}
