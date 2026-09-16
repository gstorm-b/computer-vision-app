#ifndef ROBOT_KINEMATIC_PICKING_CHECKER_H
#define ROBOT_KINEMATIC_PICKING_CHECKER_H

#include "matching/robot_picking_checker.h"
#include "calibration/calibrator.h"
#include "device/output_device/vision_output_config.h"   // RobotKinematicCheckConfig

#include <RobotKinematics/Model/RobotModelConfig.h>       // SerialRobotConfig

#include <vector>

/**
 * @file robot_kinematic_picking_checker.h
 * @brief RobotKinematicPickingChecker — concrete mtc::IRobotPickingChecker bridging vision
 *        calibration and robot kinematics/collision checks.
 */

namespace vc::model {

/**
 * @class RobotKinematicPickingChecker
 * @brief Concrete mtc::IRobotPickingChecker: the single bridge that pulls together the
 *        three families the pure-vision matcher must not know about: the calib module
 *        (image -> world), the RobotKinematics component (solveAll IK + simplified-mesh
 *        collision), and the vc::device robot-check config (preset, TCP, collision
 *        toggle).
 *
 * Built by the layer that owns both the calibration and the robot config (the
 * localization runtime), then injected into an ImageMatcher via
 * setRobotPickingChecker(). The matcher only sees the mtc::IRobotPickingChecker
 * interface, so it stays free of RobotKinematics / calib / vc dependencies.
 */
class RobotKinematicPickingChecker : public mtc::IRobotPickingChecker {
public:
    /**
     * @brief Constructs the checker: copies `calibrator` for lifetime safety, builds the
     *        preset's robot config from `config` (see buildRobotConfig()), and prepares
     *        the pick-path waypoints (posture labels resolved to branch signs).
     * @param[in] calibrator calibration used by imageToWorld() (copied, not referenced)
     * @param[in] config     preset name, TCP offset, collision toggle, and pick path
     */
    RobotKinematicPickingChecker(const calib::Calibrator& calibrator,
                                 vc::device::RobotKinematicCheckConfig config);

    /**
     * @brief Converts an image-space pick pose to a world/robot-frame pose via the
     *        calibrator, negating the calibrated rotation to match the robot's angle
     *        convention.
     * @param[in]  imgX        pick point X, in image pixels
     * @param[in]  imgY        pick point Y, in image pixels
     * @param[in]  imgAngleDeg pick orientation, in image-space degrees
     * @param[out] out         output world pick pose; populated only on success
     * @return false if the calibrator is not calibrated, true otherwise
     */
    bool imageToWorld(double imgX, double imgY, double imgAngleDeg,
                      mtc::WorldPickPose& out) const override;
    /// Returns whether the mesh self-collision check is enabled in the config.
    bool collisionCheckEnabled() const override;
    /**
     * @brief Checks whether every pick-path waypoint has a reachable IK solution on its
     *        required posture branch, starting from `pose` and composing each
     *        waypoint's offset in the tool frame; collision-free when `withCollision`
     *        is true and the mesh profile is available.
     * @param[in] pose          base pick pose in the robot/world frame
     * @param[in] withCollision if true, reject solutions that collide per the
     *            simplified mesh-collision profile (when it can be loaded)
     * @return false if the preset is invalid or any waypoint has no valid
     *         (reachable, correctly-postured, collision-free) IK solution
     */
    /**
     * @brief Runs the whole picking path and reports whether every waypoint is reachable.
     *
     * Composition order per waypoint:
     *   1. `basePose = pick * patternOffset` — the pattern's 6-axis picking offset from
     *      `pose`, applied in the TOOL frame (matches what the runtime actually emits);
     *   2. `target = basePose * waypointOffset` — the waypoint's tool-frame offset;
     *   3. any waypoint axis flagged absolute replaces its component of `target` with the
     *      configured base-frame value (decompose → patch → rebuild).
     *
     * @param[in] pose          world pick pose plus the pattern's picking offset
     * @param[in] withCollision when true, reachable solutions must also be collision-free
     * @return true only when EVERY waypoint has a reachable solution on its required
     *         posture branch; false if the robot preset is invalid
     */
    bool isPickable(const mtc::WorldPickPose& pose, bool withCollision) const override;

    /**
     * @brief Reports whether the configured preset actually resolved to a robot model.
     *
     * When it did not — a misspelled or unregistered `presetName` — this checker still
     * answers, but it fails **closed**: isPickable() returns false for every pose. On the
     * dashboard that reads as *"nothing is pickable today"*, which sends a commissioning
     * engineer to the robot, the calibration or the fixture for what is a one-word typo in
     * the settings. The runtime asks this at setup so it can refuse to start and name the
     * preset instead (Phase 9 / F1).
     *
     * @return true when the preset resolved and the checker can give meaningful answers.
     */
    bool isReady() const { return m_robotValid; }

private:
    /**
     * @struct Waypoint
     * @brief One picking-path waypoint, prepared from a config PickPathPoint: a 6-axis
     *        offset (mm/deg) plus per-axis absolute flags and the required posture branch
     *        signs (-1/+1, or 0 for "any") resolved from the preset's posture labels at
     *        construction.
     *
     * An axis with its abs flag clear is a TOOL-frame offset from the pick pose; an axis
     * with it set is an absolute value in the robot BASE frame. See isPickable() for how
     * the two are combined.
     */
    struct Waypoint {
        double dx, dy, dz, dRoll, dPitch, dYaw;  ///< Offset from the pick pose (tool frame), or absolute base-frame value where the matching abs flag is set: translation in mm, rotation in degrees.
        bool absX, absY, absZ, absRoll, absPitch, absYaw;  ///< Per-axis "value is absolute in the base frame" flags.
        int shoulder, elbow, wrist;  ///< Required posture branch sign per axis (-1/+1), or 0 for "any branch".

        /// @return true if no axis is absolute, i.e. the waypoint is a pure tool-frame offset.
        bool allRelative() const {
            return !absX && !absY && !absZ && !absRoll && !absPitch && !absYaw;
        }
    };

    calib::Calibrator m_calibrator;                       ///< Copy of the calibrator used by imageToWorld() (lifetime-safe).
    vc::device::RobotKinematicCheckConfig m_config;       ///< Preset name, TCP offset, collision toggle, and pick path as configured.
    RobotKinematics::SerialRobotConfig m_robot;           ///< Resolved preset + picking TCP; valid only when m_robotValid.
    bool m_robotValid{false};                             ///< False when m_config.presetName did not match a known preset.
    std::vector<Waypoint> m_waypoints;                    ///< Prepared pick path (always >=1 entry).
};

} // namespace vc::model

#endif // ROBOT_KINEMATIC_PICKING_CHECKER_H
