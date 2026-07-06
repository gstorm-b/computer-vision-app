#include "ui/widgets/vision/vision_roi_config_adapter.h"

QString VisionRoiConfigAdapter::sessionKey(int groupNumber, int patternNumber)
{
    return QStringLiteral("%1:%2").arg(groupNumber).arg(patternNumber);
}

QVector<VisionRoi> VisionRoiConfigAdapter::loadPatternRois(
    int groupNumber,
    int patternNumber,
    const mtc::MatchPatternConfig &config) const
{
    Q_UNUSED(config);
    return m_sessionPatternRois.value(sessionKey(groupNumber, patternNumber));
}

bool VisionRoiConfigAdapter::savePatternRois(int groupNumber,
                                             int patternNumber,
                                             mtc::MatchPatternConfig *config,
                                             const QVector<VisionRoi> &rois,
                                             QString *warningMessage)
{
    Q_UNUSED(config);
    m_sessionPatternRois.insert(sessionKey(groupNumber, patternNumber), rois);

    if (warningMessage) {
        *warningMessage = rois.isEmpty()
            ? QString()
            : QStringLiteral("Pattern ROI is kept in the UI session only. MatchPatternConfig has no dedicated ROI field yet.");
    }
    return rois.isEmpty();
}
