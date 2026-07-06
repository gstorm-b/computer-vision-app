#ifndef VISION_NUMERIC_INSPECTOR_H
#define VISION_NUMERIC_INSPECTOR_H

#include <QSize>
#include <QWidget>

#include "ui/widgets/vision/vision_overlay_types.h"

class QLabel;
class NoWheelDoubleSpinBox;

class VisionNumericInspector : public QWidget {
    Q_OBJECT

public:
    explicit VisionNumericInspector(QWidget *parent = nullptr);

    void setImageSize(const QSize &imageSize);
    void setSelectedRoi(const VisionRoi &roi);
    void clearSelection();

signals:
    void roiEdited(VisionRoi roi);

private slots:
    void emitEditedRoi();

private:
    void setFieldsEnabled(bool enabled);
    VisionRoi currentRoiFromFields() const;
    void updateRanges();

private:
    QSize m_imageSize;
    VisionRoi m_currentRoi;
    bool m_hasSelection{false};
    QLabel *m_titleLabel{nullptr};
    NoWheelDoubleSpinBox *m_centerXSpin{nullptr};
    NoWheelDoubleSpinBox *m_centerYSpin{nullptr};
    NoWheelDoubleSpinBox *m_widthSpin{nullptr};
    NoWheelDoubleSpinBox *m_heightSpin{nullptr};
    NoWheelDoubleSpinBox *m_angleSpin{nullptr};
};

#endif // VISION_NUMERIC_INSPECTOR_H
