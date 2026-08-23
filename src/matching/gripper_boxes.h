#ifndef GRIPPER_BOXES_H
#define GRIPPER_BOXES_H

#include <opencv2/core.hpp>

/**
 * @file gripper_boxes.h
 * @brief GripperBoxes — the configured jaw-pair geometry of a gripper, carried per pattern
 *        and reusable as a named preset.
 */

namespace mtc {

/**
 * @struct GripperBoxes
 * @brief Configured geometry of a gripper's two jaw boxes: the size of each jaw and the
 *        centre-to-centre distance between them.
 *
 * This is the CONFIGURED template, authored once per pattern (or saved as a named
 * preset). The per-match geometry derived from it during matching — the two oriented
 * rects and their corner points — is a different type, MatchedObject::CollisionGeometry.
 * Keep the distinction: this describes the gripper, that describes one matched object.
 *
 * @note The jaw-pair ORIENTATION is deliberately NOT here. It varies per pattern (each
 *       part is approached at its own angle) while size and distance describe the
 *       physical gripper and are therefore shareable as a preset. The orientation lives
 *       on MatchPatternConfig::m_pickingBoxAngle.
 * @note Only meaningful when the owning group's matching type is EdgeBased; collision
 *       detection is Edge-Based only and Correlation ignores these fields.
 * @see MatchedObject::CollisionGeometry, MatchPatternConfig
 */
struct GripperBoxes {
    cv::Size2f size{0.0f, 0.0f};  ///< Size of each jaw box, in image pixels; {0,0} means unset.
    double     distance{0.0};     ///< Centre-to-centre distance between the two jaws, in image pixels.
};

} // namespace mtc
#endif // GRIPPER_BOXES_H
