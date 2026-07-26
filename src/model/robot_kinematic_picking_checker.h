#ifndef ROBOT_KINEMATIC_PICKING_CHECKER_H
#define ROBOT_KINEMATIC_PICKING_CHECKER_H

#include "matching/robot_picking_checker.h"
#include "calibration/calibrator.h"
#include "device/output_device/vision_output_config.h"   // RobotKinematicCheckConfig

#include <RobotKinematics/Model/RobotModelConfig.h>       // SerialRobotConfig

#include <vector>

namespace vc::model {

/// Concrete mtc::IRobotPickingChecker: the single bridge that pulls together the
/// three families the pure-vision matcher must not know about: the calib module
/// (image -> world), the RobotKinematics component (solveAll IK + simplified-mesh
/// collision), and the vc::device robot-check config (preset, TCP, collision
/// toggle).
///
/// Built by the layer that owns both the calibration and the robot config (the
/// localization runtime), then injected into an ImageMatcher via
/// setRobotPickingChecker(). The matcher only sees the mtc::IRobotPickingChecker
/// interface, so it stays free of RobotKinematics / calib / vc dependencies.
class RobotKinematicPickingChecker : public mtc::IRobotPickingChecker {
public:
    /// Constructs the checker: copies `calibrator` for lifetime safety, builds the
    /// preset's robot config from `config` (see buildRobotConfig()), and prepares
    /// the pick-path waypoints (posture labels resolved to branch signs).
    /// @param calibrator calibration used by imageToWorld() (copied, not referenced)
    /// @param config preset name, TCP offset, collision toggle, and pick path
    RobotKinematicPickingChecker(const calib::Calibrator& calibrator,
                                 vc::device::RobotKinematicCheckConfig config);

    /// Converts an image-space pick pose to a world/robot-frame pose via the
    /// calibrator, negating the calibrated rotation to match the robot's angle
    /// convention.
    /// @param imgX pick point X, in image pixels
    /// @param imgY pick point Y, in image pixels
    /// @param imgAngleDeg pick orientation, in image-space degrees
    /// @param out output world pick pose; populated only on success
    /// @return false if the calibrator is not calibrated, true otherwise
    bool imageToWorld(double imgX, double imgY, double imgAngleDeg,
                      mtc::WorldPickPose& out) const override;
    /// Returns whether the mesh self-collision check is enabled in the config.
    bool collisionCheckEnabled() const override;
    /// Checks whether every pick-path waypoint has a reachable IK solution on its
    /// required posture branch, starting from `pose` and composing each
    /// waypoint's offset in the tool frame; collision-free when `withCollision`
    /// is true and the mesh profile is available.
    /// @param pose base pick pose in the robot/world frame
    /// @param withCollision if true, reject solutions that collide per the
    ///        simplified mesh-collision profile (when it can be loaded)
    /// @return false if the preset is invalid or any waypoint has no valid
    ///         (reachable, correctly-postured, collision-free) IK solution
    bool isPickable(const mtc::WorldPickPose& pose, bool withCollision) const override;

private:
    /// One picking-path waypoint, prepared from a config PickPathPoint: a 6-axis
    /// tool-frame offset (mm/deg) plus required posture branch signs (-1/+1, or 0
    /// for "any") resolved from the preset's posture labels at construction.
    struct Waypoint {
        double dx, dy, dz, dRoll, dPitch, dYaw;  ///< Tool-frame offset from the pick pose: translation in mm, rotation in degrees.
        int shoulder, elbow, wrist;  ///< Required posture branch sign per axis (-1/+1), or 0 for "any branch".
    };

    calib::Calibrator m_calibrator;                       ///< Copy of the calibrator used by imageToWorld() (lifetime-safe).
    vc::device::RobotKinematicCheckConfig m_config;       ///< Preset name, TCP offset, collision toggle, and pick path as configured.
    RobotKinematics::SerialRobotConfig m_robot;           ///< Resolved preset + picking TCP; valid only when m_robotValid.
    bool m_robotValid{false};                             ///< False when m_config.presetName did not match a known preset.
    std::vector<Waypoint> m_waypoints;                    ///< Prepared pick path (always >=1 entry).
};

} // namespace vc::model

#endif // ROBOT_KINEMATIC_PICKING_CHECKER_H
