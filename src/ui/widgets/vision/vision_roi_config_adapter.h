#ifndef VISION_ROI_CONFIG_ADAPTER_H
#define VISION_ROI_CONFIG_ADAPTER_H

#include <QMap>
#include <QString>
#include <QVector>

#include "matching/match_pattern_config.h"
#include "ui/widgets/vision/vision_overlay_types.h"

class VisionRoiConfigAdapter {
public:
    QVector<VisionRoi> loadPatternRois(int groupNumber,
                                       int patternNumber,
                                       const mtc::MatchPatternConfig &config) const;

    bool savePatternRois(int groupNumber,
                         int patternNumber,
                         mtc::MatchPatternConfig *config,
                         const QVector<VisionRoi> &rois,
                         QString *warningMessage = nullptr);

private:
    static QString sessionKey(int groupNumber, int patternNumber);

private:
    QMap<QString, QVector<VisionRoi>> m_sessionPatternRois;
};

#endif // VISION_ROI_CONFIG_ADAPTER_H
