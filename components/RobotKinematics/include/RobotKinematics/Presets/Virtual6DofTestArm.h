#pragma once

#include <RobotKinematics/Model/RobotModelConfig.h>

/// Built-in preset robot definitions used as fixtures/examples (not loaded from JSON).
namespace RobotKinematics::Presets {

/// Builds the hard-coded 6-DOF virtual test arm fixture: a serial chain of 6 revolute
/// joints (+/-pi limits) with a vision user frame, a default and probe tool, sign-based
/// shoulder/elbow/wrist posture labels, and adaptive damped-least-squares solver defaults.
/// Used as a project fixture/example rather than loaded from a preset file.
SerialRobotConfig virtual6DofTestArm();

} // namespace RobotKinematics::Presets
