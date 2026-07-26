#include "ForwardKinematicsTests.h"

#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <Eigen/Geometry>

#include <QtTest/QtTest>

#include <cmath>
#include <optional>
#include <string>

using namespace RobotKinematics;

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

/// Returns true if the Euclidean distance between `a` and `b` is within `tol`.
bool vecNear(const Eigen::Vector3d& a, const Eigen::Vector3d& b, double tol = 1e-12)
{
    return (a - b).norm() <= tol;
}

/// Builds a Z-axis revolute joint with the given id, parent/child link ids, and origin pose;
/// limits are set to +/-2*pi with no velocity/effort caps.
Joint revoluteZ(const std::string& id, const std::string& parent, const std::string& child, const Pose& origin)
{
    Joint joint;
    joint.id = id;
    joint.type = JointType::Revolute;
    joint.parentLinkId = parent;
    joint.childLinkId = child;
    joint.axis = Eigen::Vector3d::UnitZ();
    joint.origin = origin;
    joint.limits = JointLimits{-2.0 * kPi, 2.0 * kPi, std::nullopt, std::nullopt};
    return joint;
}

/// Builds a 1-DOF serial robot (base -> flange) whose sole Z-axis revolute joint origin is
/// offset `offsetX` meters along X.
SerialRobotConfig singleJoint(double offsetX)
{
    SerialRobotConfig config;
    config.topology = RobotTopologyType::Serial;
    config.dof = 1;
    config.frames.baseLinkId = "base";
    config.frames.flangeLinkId = "flange";
    config.links = {{"base"}, {"flange"}};
    config.joints = {revoluteZ("J1", "base", "flange", Pose::fromXYZRPY_m_rad(offsetX, 0.0, 0.0, 0.0, 0.0, 0.0))};
    return config;
}

/// Builds a 2-DOF planar serial robot (base -> link1 -> flange) with two Z-axis revolute
/// joints, the second offset `l1` meters along X from the first.
SerialRobotConfig twoJointPlanar(double l1)
{
    SerialRobotConfig config;
    config.topology = RobotTopologyType::Serial;
    config.dof = 2;
    config.frames.baseLinkId = "base";
    config.frames.flangeLinkId = "flange";
    config.links = {{"base"}, {"link1"}, {"flange"}};
    config.joints = {
        revoluteZ("J1", "base", "link1", Pose::identity()),
        revoluteZ("J2", "link1", "flange", Pose::fromXYZRPY_m_rad(l1, 0.0, 0.0, 0.0, 0.0, 0.0)),
    };
    return config;
}

/// Builds a 6-DOF serial robot stacking six Z-axis revolute joints, each `step` meters above
/// the previous link along Z (a straight vertical tower).
SerialRobotConfig sixJointTower(double step)
{
    SerialRobotConfig config;
    config.topology = RobotTopologyType::Serial;
    config.dof = 6;
    config.frames.baseLinkId = "base";
    config.frames.flangeLinkId = "flange";
    config.links = {{"base"}, {"l1"}, {"l2"}, {"l3"}, {"l4"}, {"l5"}, {"flange"}};
    const char* parents[] = {"base", "l1", "l2", "l3", "l4", "l5"};
    const char* children[] = {"l1", "l2", "l3", "l4", "l5", "flange"};
    for (int i = 0; i < 6; ++i) {
        config.joints.push_back(revoluteZ("J" + std::to_string(i + 1), parents[i], children[i],
                                          Pose::fromXYZRPY_m_rad(0.0, 0.0, step, 0.0, 0.0, 0.0)));
    }
    return config;
}
}

void ForwardKinematicsTests::singleRevoluteJointWithOffset()
{
    const SerialRobotConfig config = singleJoint(0.4);

    // The joint origin places the rotation axis at (0.4,0,0); the flange sits on that axis,
    // so it rotates in place: position invariant, orientation follows the joint.
    const Pose atZero = ForwardKinematics::flangePose(config, JointVector::fromRadians({0.0}));
    QVERIFY(vecNear(atZero.translation_m(), Eigen::Vector3d(0.4, 0.0, 0.0)));

    const Pose atNinety = ForwardKinematics::flangePose(config, JointVector::fromRadians({kPi / 2.0}));
    QVERIFY(vecNear(atNinety.translation_m(), Eigen::Vector3d(0.4, 0.0, 0.0)));
    QVERIFY(vecNear(atNinety.isometry().linear() * Eigen::Vector3d::UnitX(), Eigen::Vector3d::UnitY()));

    // A tool offset along flange X makes the TCP swing about the joint axis.
    const Pose tcp = ForwardKinematics::toolPose(config, JointVector::fromRadians({kPi / 2.0}),
                                                 Pose::fromXYZRPY_m_rad(0.2, 0.0, 0.0, 0.0, 0.0, 0.0));
    QVERIFY(vecNear(tcp.translation_m(), Eigen::Vector3d(0.4, 0.2, 0.0)));
}

void ForwardKinematicsTests::twoJointPlanarArmWithTool()
{
    const double l1 = 0.5;
    const double l2 = 0.3;
    const SerialRobotConfig config = twoJointPlanar(l1);
    const Pose tool = Pose::fromXYZRPY_m_rad(l2, 0.0, 0.0, 0.0, 0.0, 0.0);

    struct Case {
        double q1;
        double q2;
    };
    const Case cases[] = {{0.0, 0.0}, {kPi / 2.0, 0.0}, {0.0, kPi / 2.0}, {kPi / 6.0, kPi / 3.0}};
    for (const Case& c : cases) {
        const Pose tcp = ForwardKinematics::toolPose(config, JointVector::fromRadians({c.q1, c.q2}), tool);
        const Eigen::Vector3d expected(l1 * std::cos(c.q1) + l2 * std::cos(c.q1 + c.q2),
                                       l1 * std::sin(c.q1) + l2 * std::sin(c.q1 + c.q2),
                                       0.0);
        QVERIFY(vecNear(tcp.translation_m(), expected, 1e-12));
    }
}

void ForwardKinematicsTests::sixJointTowerComposition()
{
    const double step = 0.1;
    const SerialRobotConfig config = sixJointTower(step);

    const Pose home = ForwardKinematics::flangePose(config, JointVector::fromRadians({0, 0, 0, 0, 0, 0}));
    QVERIFY(vecNear(home.translation_m(), Eigen::Vector3d(0.0, 0.0, 6.0 * step)));

    const Pose pose = ForwardKinematics::flangePose(config, JointVector::fromDegrees({10, 20, 30, 40, 50, 60}));
    // All offsets share the Z rotation axis, so the position is invariant and orientation = Rz(sum).
    QVERIFY(vecNear(pose.translation_m(), Eigen::Vector3d(0.0, 0.0, 6.0 * step), 1e-9));
    const Eigen::Matrix3d expectedR =
        Eigen::AngleAxisd(210.0 * kPi / 180.0, Eigen::Vector3d::UnitZ()).toRotationMatrix();
    QVERIFY((pose.isometry().linear() - expectedR).norm() < 1e-9);
}

/// Entry point that instantiates ForwardKinematicsTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runForwardKinematicsTests(int argc, char** argv)
{
    ForwardKinematicsTests tests;
    return QTest::qExec(&tests, argc, argv);
}
