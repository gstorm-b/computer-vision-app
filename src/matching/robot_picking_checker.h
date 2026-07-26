#ifndef ROBOT_PICKING_CHECKER_H
#define ROBOT_PICKING_CHECKER_H

/// Vision/matching module: IRobotPickingChecker, the dependency-inversion
/// port used by ImageMatcher to query robot reachability/collision without
/// depending on the robot-kinematics/calibration/device layers.
namespace mtc {

/// Neutral data-transfer pose: robot/world coordinates in millimetres, top-down
/// pick rotation in degrees. Carries no robot-library or Qt types.
struct WorldPickPose {
    double x_mm{0.0};      ///< World/robot X coordinate, in millimetres.
    double y_mm{0.0};      ///< World/robot Y coordinate, in millimetres.
    double z_mm{0.0};      ///< World/robot Z coordinate, in millimetres.
    double r_deg{0.0};     ///< Top-down pick rotation, in degrees.
};

/// Port (dependency-inversion boundary) that lets the pure-vision ImageMatcher
/// ask "can the robot actually pick this object?" without depending on the
/// robot kinematics / calibration / device layers. The concrete adapter
/// (vc::model::RobotKinematicPickingChecker) lives in a higher layer.
class IRobotPickingChecker {
public:
    /// Default virtual destructor.
    virtual ~IRobotPickingChecker() = default;

    /// Step 1 — converts an image pixel position + angle (in the matcher's
    /// image frame) to a world/robot pick pose.
    /// @param imgX pixel X coordinate in the matcher's image frame
    /// @param imgY pixel Y coordinate in the matcher's image frame
    /// @param imgAngleDeg pick angle, in degrees, in the matcher's image frame
    /// @param out output world pick pose, populated only on success
    /// @return false if no usable calibration is available
    virtual bool imageToWorld(double imgX, double imgY, double imgAngleDeg,
                              WorldPickPose& out) const = 0;

    /// Whether the operator enabled the (simplified-mesh) self-collision check.
    /// ImageMatcher passes this as the withCollision flag to isPickable().
    virtual bool collisionCheckEnabled() const = 0;

    /// Step 2 (+3) — checks reachability via solveAll IK at the pick pose;
    /// when `withCollision` is set, the same IK solution is additionally run
    /// through the simplified-mesh self-collision check.
    /// @param pose the world/robot pick pose to test
    /// @param withCollision when true, also require the pose to be collision-free
    /// @return true only when the pose is reachable (and collision-free when requested)
    virtual bool isPickable(const WorldPickPose& pose, bool withCollision) const = 0;
};

} // namespace mtc

#endif // ROBOT_PICKING_CHECKER_H
