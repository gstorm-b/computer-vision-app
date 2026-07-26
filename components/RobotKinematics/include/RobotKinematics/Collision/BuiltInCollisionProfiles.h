#pragma once

#include <RobotKinematics/Collision/CollisionProfile.h>

namespace RobotKinematics::CollisionProfiles {

/// Builds the conservative-primitive collision profile for the "Virtual6DofTestArm" project
/// fixture: one sphere/capsule per link, sized to conservatively enclose the visual model, with
/// adjacent-joint pairs disabled and metadata marking it as an unrated, project-owned
/// approximation.
/// @return the fully populated CollisionProfile for the virtual 6-DOF test arm
CollisionProfile virtual6DofTestArm();
/// Builds the conservative-primitive collision profile for the Nachi MZ04D robot, estimated from
/// the vendor's visual CAD assets: one sphere/capsule per link with adjacent-joint pairs disabled
/// and metadata marking it as an unrated, CAD-based estimate.
/// @return the fully populated CollisionProfile for the Nachi MZ04D
CollisionProfile nachiMZ04D();

} // namespace RobotKinematics::CollisionProfiles
