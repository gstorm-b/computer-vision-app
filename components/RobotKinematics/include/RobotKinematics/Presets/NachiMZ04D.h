#pragma once

#include <RobotKinematics/Model/RobotModelConfig.h>

/// Vendor/model-specific robot configuration presets built from reverse-engineered or
/// datasheet kinematic parameters (currently: the Nachi MZ04D).
namespace RobotKinematics::Presets {

/// Nachi MZ04D serial 6DOF preset.
///
/// Kinematics were reverse-engineered from teach-pendant measurements (joint angles
/// vs. tool-flange poses, tool offset 0) recorded in docs/preset_references/nachi-mz04d.md.
/// The canonical joint origins below are the standard-DH table
///
///        theta   d(mm)   alpha   a(mm)
///   J1     0     340.0   +90       0      (340 = 157.5 + 182.5 base/shoulder height)
///   J2     0       0       0     260
///   J3     0       0     +90      25
///   J4     0     280     -90       0
///   J5     0       0     +90       0
///   J6     0      72       0       0
///
/// expressed as canonical link transforms (all joints revolute about local Z). This
/// table reproduces all 21 reference poses to <= 0.04 mm and <= 0.01 deg.
///
/// Joint position limits are from the teach pendant (docs/preset_references/nachi-mz04d.md).
/// Posture labels follow the Nachi manual: shoulder = sign(J1) (J1<0 righty, J1>0 lefty),
/// wrist = sign(J5) (J5<0 flip, J5>0 non-flip), elbow = sign(J3) (J3<0 above, J3>0 below;
/// below-side confirmed by ground-truth measurements, above-side inferred by symmetry).
/// @return a fully populated SerialRobotConfig for the Nachi MZ04D (joints, limits, tools,
/// posture resolver/labels, default solver, and source/metadata references).
SerialRobotConfig nachiMZ04D();

} // namespace RobotKinematics::Presets
