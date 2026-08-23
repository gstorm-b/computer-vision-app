#include "match_pattern_config.h"

namespace mtc {

/// Copy-constructs, deep-copying the training image (m_rawImage.clone()) so this config
/// owns an independent pixel buffer; all other fields are value-copied.
MatchPatternConfig::MatchPatternConfig(const MatchPatternConfig& other) {
    m_patternName        = other.m_patternName;
    m_patternIndex       = other.m_patternIndex;
    m_rawImage           = other.m_rawImage.clone();
    m_minScore           = other.m_minScore;
    m_angle              = other.m_angle;
    m_toleranceAngle     = other.m_toleranceAngle;
    m_maxOverlap         = other.m_maxOverlap;
    m_pickPosition          = other.m_pickPosition;
    m_gripperBoxes          = other.m_gripperBoxes;
    m_pickingBoxAngle       = other.m_pickingBoxAngle;
    m_usePickingBox         = other.m_usePickingBox;
    m_pickingOffset         = other.m_pickingOffset;
    m_pickingRotationOffset = other.m_pickingRotationOffset;
}

/// Copy-assigns, deep-copying the training image (m_rawImage.clone()) so this config
/// owns an independent pixel buffer; all other fields are value-copied. Self-assignment
/// safe (no-op when `&other == this`).
MatchPatternConfig& MatchPatternConfig::operator=(const MatchPatternConfig& other) {
    if (this != &other) {
        m_patternName        = other.m_patternName;
        m_patternIndex       = other.m_patternIndex;
        m_rawImage           = other.m_rawImage.clone();
        m_minScore           = other.m_minScore;
        m_angle              = other.m_angle;
        m_toleranceAngle     = other.m_toleranceAngle;
        m_maxOverlap         = other.m_maxOverlap;
        m_pickPosition          = other.m_pickPosition;
        m_gripperBoxes          = other.m_gripperBoxes;
        m_pickingBoxAngle       = other.m_pickingBoxAngle;
        m_usePickingBox         = other.m_usePickingBox;
        m_pickingOffset         = other.m_pickingOffset;
        m_pickingRotationOffset = other.m_pickingRotationOffset;
    }
    return *this;
}

} // namespace mtc
