#ifndef VISION_ROI_CONFIG_ADAPTER_H
#define VISION_ROI_CONFIG_ADAPTER_H

#include <QMap>
#include <QString>
#include <QVector>

#include "matching/match_pattern_config.h"
#include "ui/widgets/vision/vision_overlay_types.h"

/// Adapter that holds per-pattern ROI overlays for the UI session, keyed by
/// group/pattern number. `mtc::MatchPatternConfig` currently has no dedicated ROI
/// field, so ROIs are kept in-memory only (`config` is accepted for future use but
/// otherwise unused) and are lost when the session ends.
class VisionRoiConfigAdapter {
public:
    /// Returns the ROIs previously saved for (`groupNumber`, `patternNumber`) in this
    /// session, or an empty vector if none were saved. `config` is currently unused.
    QVector<VisionRoi> loadPatternRois(int groupNumber,
                                       int patternNumber,
                                       const mtc::MatchPatternConfig &config) const;

    /// Stores `rois` in the in-session cache for (`groupNumber`, `patternNumber`).
    /// `config` is currently unused (no persistent ROI field exists on
    /// MatchPatternConfig yet). When `warningMessage` is non-null and `rois` is
    /// non-empty, it is set to a note that the ROI is session-only and not persisted.
    /// @return true if `rois` is empty, false otherwise
    bool savePatternRois(int groupNumber,
                         int patternNumber,
                         mtc::MatchPatternConfig *config,
                         const QVector<VisionRoi> &rois,
                         QString *warningMessage = nullptr);

private:
    /// Builds the "group:pattern" string key used to index m_sessionPatternRois.
    static QString sessionKey(int groupNumber, int patternNumber);

private:
    QMap<QString, QVector<VisionRoi>> m_sessionPatternRois;  ///< In-session ROI cache, keyed by sessionKey(groupNumber, patternNumber).
};

#endif // VISION_ROI_CONFIG_ADAPTER_H
