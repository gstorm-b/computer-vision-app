#ifndef MATCH_PATTERN_CONFIG_H
#define MATCH_PATTERN_CONFIG_H

#include <string>
#include <opencv2/core.hpp>

#include "matching/gripper_boxes.h"

/**
 * @file match_pattern_config.h
 * @brief MatchPatternConfig — per-pattern identity, search-parameter, and picking/collision-box
 *        configuration owned by MatchPattern.
 */

namespace mtc {

/**
 * @struct MatchPatternConfig
 * @brief Per-pattern configuration carried by MatchPattern: identity, type-agnostic
 *        search parameters, and the per-pattern picking/collision-box geometry.
 *
 * The matching-algorithm parameters (Canny thresholds, greediness, binarization, ...)
 * are NOT here; they live on the owning group's MatchGroupConfig::typeConfig and are
 * shared by every pattern in the group.
 *
 * @note m_gripperBoxes is only meaningful when the owning group's matching type is
 *       EdgeBased (collision detection is Edge-Based only); Correlation ignores it.
 * @note Copy semantics deep-copy the training image (m_rawImage) so each pattern owns an
 *       independent pixel buffer.
 */
struct MatchPatternConfig {
    /// Default-constructs an empty pattern configuration (no training image, default search parameters).
    MatchPatternConfig() = default;
    /**
     * @brief Deep-copies `other`, cloning m_rawImage so this instance owns an independent pixel buffer.
     * @param[in] other source config to copy from
     */
    MatchPatternConfig(const MatchPatternConfig& other);
    /**
     * @brief Deep-copies `other`, cloning m_rawImage so this instance owns an independent pixel buffer.
     * @param[in] other source config to copy from
     * @return reference to this config after assignment
     */
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
    /// Picking angle (degrees): a constant added to the reported angle of every match from
    /// this pattern (`point_angle = matched_angle + m_angle`), expressing the orientation
    /// at which the part is picked. It is NOT a search parameter — it does not steer or
    /// narrow matching; see m_toleranceAngle.
    double m_angle          = 0.0;
    /// Half-width of the angular search sweep (degrees), centred on ZERO — not on m_angle;
    /// 180 covers the full range.
    double m_toleranceAngle = 180.0;
    double m_maxOverlap     = 0.1;         ///< Maximum allowed overlap ratio (0..1) between accepted matches.
    /// TODO: change toleranceAngle parameters
    // double m_toleranceAngleMin = -180.0;
    // double m_toleranceAngleMax = 180.0;

    // ── Pick position ─────────────────────────────────────────────────────
    cv::Point2f m_pickPosition;            ///< Pick point, in pattern-local image coordinates.

    // ── Picking / collision-box geometry (Edge-Based only) ────────────────
    // Used by the collision check in ImageMatcher; ignored by Correlation.
    GripperBoxes m_gripperBoxes;                   ///< Jaw size and centre-to-centre distance used for the collision check; shareable as a preset.
    /// Jaw-pair orientation relative to the pick angle, in degrees. Deliberately NOT part
    /// of m_gripperBoxes: each pattern is approached at its own angle, while the jaw
    /// size/distance describe the physical gripper and are shared through presets.
    /// Unrelated to m_angle above.
    double       m_pickingBoxAngle = 0.0;
    bool         m_usePickingBox{true};            ///< When false, the gripper collision check is skipped for this pattern and it is never rejected for collision.

    // ── Picking offset applied at picking time ────────────────────────────
    cv::Point3f m_pickingOffset{0.0f, 0.0f, 0.0f};         ///< XYZ offset (mm) applied to the pick position at picking time.
    cv::Point3f m_pickingRotationOffset{0.0f, 0.0f, 0.0f}; ///< RX/RY/RZ rotation offset (deg) applied in the TOOL frame at picking time.
};

} // namespace mtc
#endif // MATCH_PATTERN_CONFIG_H
