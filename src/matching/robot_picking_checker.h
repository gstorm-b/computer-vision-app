#ifndef ROBOT_PICKING_CHECKER_H
#define ROBOT_PICKING_CHECKER_H

/**
 * @file robot_picking_checker.h
 * @brief IRobotPickingChecker — dependency-inversion port for robot reachability/collision
 *        queries from the vision matching layer.
 */

namespace mtc {

/**
 * @struct WorldPickPose
 * @brief Neutral data-transfer pose: robot/world coordinates in millimetres, top-down
 *        pick rotation in degrees, plus the pattern's picking offset. Carries no
 *        robot-library or Qt types.
 *
 * The x/y/z/r fields are the bare matched pick point, as produced by
 * IRobotPickingChecker::imageToWorld(). The offset* fields are the owning pattern's
 * picking offset, which the checker composes in the TOOL frame (pose = pick * offset)
 * before testing reachability — the matching layer deliberately does not do that
 * composition itself, so it needs no pose maths and no robot library.
 *
 * @note An all-zero offset makes the composition a no-op, giving the pre-Phase-5
 *       behaviour of testing the bare pick point.
 */
struct WorldPickPose {
    double x_mm{0.0};      ///< World/robot X coordinate, in millimetres.
    double y_mm{0.0};      ///< World/robot Y coordinate, in millimetres.
    double z_mm{0.0};      ///< World/robot Z coordinate, in millimetres.
    double r_deg{0.0};     ///< Top-down pick rotation, in degrees.

    double offsetX_mm{0.0};   ///< Pattern picking offset along the tool X axis, in millimetres.
    double offsetY_mm{0.0};   ///< Pattern picking offset along the tool Y axis, in millimetres.
    double offsetZ_mm{0.0};   ///< Pattern picking offset along the tool Z axis, in millimetres.
    double offsetRx_deg{0.0}; ///< Pattern picking rotation offset about the tool X axis, in degrees.
    double offsetRy_deg{0.0}; ///< Pattern picking rotation offset about the tool Y axis, in degrees.
    double offsetRz_deg{0.0}; ///< Pattern picking rotation offset about the tool Z axis, in degrees.
};

/**
 * @class IRobotPickingChecker
 * @brief Port (dependency-inversion boundary) that lets the pure-vision ImageMatcher
 *        ask "can the robot actually pick this object?" without depending on the
 *        robot kinematics / calibration / device layers.
 *
 * The concrete adapter (vc::model::RobotKinematicPickingChecker) lives in a higher layer.
 */
class IRobotPickingChecker {
public:
    /// Default virtual destructor.
    virtual ~IRobotPickingChecker() = default;

    /**
     * @brief Step 1 — converts an image pixel position + angle (in the matcher's image frame)
     *        to a world/robot pick pose.
     * @param[in]  imgX        pixel X coordinate in the matcher's image frame
     * @param[in]  imgY        pixel Y coordinate in the matcher's image frame
     * @param[in]  imgAngleDeg pick angle, in degrees, in the matcher's image frame
     * @param[out] out         world pick pose, populated only on success
     * @return false if no usable calibration is available
     */
    virtual bool imageToWorld(double imgX, double imgY, double imgAngleDeg,
                              WorldPickPose& out) const = 0;

    /// Whether the operator enabled the (simplified-mesh) self-collision check.
    /// ImageMatcher passes this as the withCollision flag to isPickable().
    virtual bool collisionCheckEnabled() const = 0;

    /**
     * @brief Step 2 (+3) — checks reachability via solveAll IK at the pick pose composed
     *        with the pattern's picking offset; when `withCollision` is set, the same IK
     *        solution is additionally run through the simplified-mesh self-collision check.
     * @param[in] pose          the world/robot pick pose plus its picking offset; the
     *                          implementation composes them in the TOOL frame
     * @param[in] withCollision when true, also require the pose to be collision-free
     * @return true only when the pose is reachable (and collision-free when requested)
     */
    virtual bool isPickable(const WorldPickPose& pose, bool withCollision) const = 0;
};

} // namespace mtc

#endif // ROBOT_PICKING_CHECKER_H
