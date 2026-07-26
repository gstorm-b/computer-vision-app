#ifndef MATCH_PATTERN_CONFIG_H
#define MATCH_PATTERN_CONFIG_H

#include <string>
#include <opencv2/core.hpp>

/// Vision/matching module: MatchPatternConfig, the per-pattern identity,
/// search-parameter, and picking/collision-box configuration owned by MatchPattern.
namespace mtc {

/// Per-pattern configuration carried by MatchPattern: identity, type-agnostic
/// search parameters, and the per-pattern picking/collision-box geometry.
/// The matching-algorithm parameters (Canny thresholds, greediness,
/// binarization, ...) are NOT here; they live on the owning group's
/// MatchGroupConfig::typeConfig and are shared by every pattern in the group.
/// @note m_pickingBox* fields are only meaningful when the owning group's
/// matching type is EdgeBased (collision detection is Edge-Based only);
/// Correlation ignores them.
/// @note Copy semantics deep-copy the training image (m_rawImage) so each
/// pattern owns an independent pixel buffer.
struct MatchPatternConfig {
    /// Default-constructs an empty pattern configuration (no training image, default search parameters).
    MatchPatternConfig() = default;
    /// Deep-copies `other`, cloning m_rawImage so this instance owns an independent pixel buffer.
    MatchPatternConfig(const MatchPatternConfig& other);
    /// Deep-copies `other`, cloning m_rawImage so this instance owns an independent pixel buffer.
    MatchPatternConfig& operator=(const MatchPatternConfig& other);
    MatchPatternConfig(MatchPatternConfig&&)            = default;
    MatchPatternConfig& operator=(MatchPatternConfig&&) = default;

    // ── Identity ─────────────────────────────────────────────────────────
    std::wstring m_patternName;            ///< Display name of the pattern.
    int          m_patternIndex = 0;       ///< Index of this pattern within its owning group.

    // ── Training image ────────────────────────────────────────────────────
    cv::Mat m_rawImage;                    ///< Training image used to learn this pattern; deep-copied on copy so each pattern owns an independent buffer.

    // ── Type-agnostic search parameters ───────────────────────────────────
    double m_minScore       = 0.9;         ///< Minimum match score (0..1) accepted as a valid match.
    double m_angle          = 0.0;         ///< Base/reference rotation angle (degrees) of the trained pattern.
    double m_toleranceAngle = 180.0;       ///< Allowed angular search tolerance (degrees) around m_angle; 180 covers the full range.
    double m_maxOverlap     = 0.1;         ///< Maximum allowed overlap ratio (0..1) between accepted matches.
    /// TODO: change toleranceAngle parameters
    // double m_toleranceAngleMin = -180.0;
    // double m_toleranceAngleMax = 180.0;

    // ── Pick position ─────────────────────────────────────────────────────
    cv::Point2f m_pickPosition;            ///< Pick point, in pattern-local image coordinates.

    // ── Picking / collision-box geometry (Edge-Based only) ────────────────
    // Used by the collision check in ImageMatcher; ignored by Correlation.
    cv::Size2f  m_pickingBoxSize{0.0f, 0.0f};      ///< Size of the picking/collision box (Edge-Based only); {0,0} means unset.
    double      m_pickingBoxDistance{0.0};         ///< Distance offset of the picking box from the pick position (Edge-Based only).
    double      m_pickingBoxAngle{0.0};            ///< Rotation angle of the picking box, in degrees (Edge-Based only).

    // ── Picking offset applied at picking time ────────────────────────────
    cv::Point3f m_pickingOffset{0.0f, 0.0f, 0.0f}; ///< XYZ offset applied to the pick position at picking time.
};

} // namespace mtc
#endif // MATCH_PATTERN_CONFIG_H
