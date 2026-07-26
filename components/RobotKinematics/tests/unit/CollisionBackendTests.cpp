#include "CollisionBackendTests.h"

#include <RobotKinematics/Collision/CollisionBackend.h>
#include <RobotKinematics/Collision/BuiltInCollisionProfiles.h>
#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Model/RobotModelConfig.h>
#include <RobotKinematics/Presets/Virtual6DofTestArm.h>

#include <QDir>
#include <QFile>
#include <QtTest/QtTest>

#include <Eigen/Core>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

using namespace RobotKinematics;

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;

/// A plain 3D point used to build synthetic STL triangle data for mesh-backend tests.
struct Vec3
{
    double x;
    double y;
    double z;
};

/// A single triangle (three vertices) used when writing a synthetic ASCII STL mesh.
struct Triangle
{
    Vec3 a;
    Vec3 b;
    Vec3 c;
};

/// Builds a single-DOF revolute joint rotating about the local Z axis, spanning
/// +/-pi, with zero home position.
Joint revoluteZ(const std::string& id,
                const std::string& parent,
                const std::string& child,
                const Pose& origin)
{
    Joint joint;
    joint.id = id;
    joint.type = JointType::Revolute;
    joint.parentLinkId = parent;
    joint.childLinkId = child;
    joint.axis = Eigen::Vector3d::UnitZ();
    joint.origin = origin;
    joint.limits = JointLimits{-kPi, kPi, std::nullopt, std::nullopt};
    joint.home = 0.0;
    return joint;
}

/// Builds a minimal 3-DOF serial robot (three Z-axis revolute joints, each link 1m
/// apart along X) used as a lightweight fixture for mesh-collision backend tests.
SerialRobotConfig lineRobot()
{
    SerialRobotConfig config;
    config.identity = RobotIdentity{"RobotKinematics", "CollisionLineRobot", "Collision Line Robot", "1.0.0"};
    config.topology = RobotTopologyType::Serial;
    config.dof = 3;
    config.links = {{"base_link"}, {"link_1"}, {"link_2"}, {"flange"}};
    config.joints = {
        revoluteZ("J1", "base_link", "link_1", Pose::identity()),
        revoluteZ("J2", "link_1", "link_2", Pose::fromXYZRPY_m_rad(1.0, 0.0, 0.0, 0.0, 0.0, 0.0)),
        revoluteZ("J3", "link_2", "flange", Pose::fromXYZRPY_m_rad(1.0, 0.0, 0.0, 0.0, 0.0, 0.0)),
    };
    config.frames.baseLinkId = "base_link";
    config.frames.flangeLinkId = "flange";
    config.tools = {Tool{"default", "Default Tool", Pose::identity()}};
    config.defaultToolId = "default";
    return config;
}

/// Returns the 12 triangles (two per face) forming a closed 0.2 x 0.1 x 0.1 meter
/// axis-aligned cuboid with one corner at the origin.
std::vector<Triangle> cuboidTrianglesMeters()
{
    const Vec3 p000{0.0, 0.0, 0.0};
    const Vec3 p100{0.2, 0.0, 0.0};
    const Vec3 p010{0.0, 0.1, 0.0};
    const Vec3 p110{0.2, 0.1, 0.0};
    const Vec3 p001{0.0, 0.0, 0.1};
    const Vec3 p101{0.2, 0.0, 0.1};
    const Vec3 p011{0.0, 0.1, 0.1};
    const Vec3 p111{0.2, 0.1, 0.1};

    return {
        {p000, p110, p100}, {p000, p010, p110},
        {p001, p101, p111}, {p001, p111, p011},
        {p000, p100, p101}, {p000, p101, p001},
        {p010, p011, p111}, {p010, p111, p110},
        {p000, p001, p011}, {p000, p011, p010},
        {p100, p110, p111}, {p100, p111, p101},
    };
}

/// Serializes cuboidTrianglesMeters() into an ASCII STL file body (facet/outer
/// loop/vertex blocks with a zero normal, since the loader only needs vertex geometry).
QByteArray asciiStlBytesMeters()
{
    QByteArray bytes;
    bytes += "solid cuboid\n";
    for (const Triangle& triangle : cuboidTrianglesMeters()) {
        bytes += "  facet normal 0 0 0\n";
        bytes += "    outer loop\n";
        bytes += QByteArray("      vertex ") + QByteArray::number(triangle.a.x, 'f', 6) + " " +
                 QByteArray::number(triangle.a.y, 'f', 6) + " " +
                 QByteArray::number(triangle.a.z, 'f', 6) + "\n";
        bytes += QByteArray("      vertex ") + QByteArray::number(triangle.b.x, 'f', 6) + " " +
                 QByteArray::number(triangle.b.y, 'f', 6) + " " +
                 QByteArray::number(triangle.b.z, 'f', 6) + "\n";
        bytes += QByteArray("      vertex ") + QByteArray::number(triangle.c.x, 'f', 6) + " " +
                 QByteArray::number(triangle.c.y, 'f', 6) + " " +
                 QByteArray::number(triangle.c.z, 'f', 6) + "\n";
        bytes += "    endloop\n";
        bytes += "  endfacet\n";
    }
    bytes += "endsolid cuboid\n";
    return bytes;
}

/// Writes `bytes` to a file named `fileName` in the system temp directory (removing
/// any existing file at that path first).
/// @return the written file's path, or an empty string if opening/writing it failed
QString writeTempFile(const QString& fileName, const QByteArray& bytes)
{
    const QString path = QDir::temp().filePath(fileName);
    QFile::remove(path);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    if (file.write(bytes) != bytes.size()) {
        return QString();
    }
    file.close();
    return path;
}

/// Builds a MeshCollisionProfile preferring the Coal backend, with two STL meshes
/// sharing `path`: a "base_mesh" on base_link offset along X by `baseMeshOffsetX_m`,
/// and an unoffset "link_2_mesh" on link_2.
MeshCollisionProfile syntheticMeshProfile(const std::string& path, double baseMeshOffsetX_m = 0.85)
{
    MeshCollisionProfile profile;
    profile.id = "synthetic_coal_profile";
    profile.robotModel = "CollisionLineRobot";
    profile.backendPreference = {MeshCollisionBackendKind::Coal};

    MeshCollisionGeometry baseMesh;
    baseMesh.id = "base_mesh";
    baseMesh.linkId = "base_link";
    baseMesh.path = path;
    baseMesh.format = MeshFileFormat::Stl;
    baseMesh.sourceUnits = MeshSourceUnits::Meters;
    baseMesh.scaleToMeters = 1.0;
    baseMesh.meshToLink = Pose::fromXYZRPY_m_rad(baseMeshOffsetX_m, 0.0, 0.0, 0.0, 0.0, 0.0);

    MeshCollisionGeometry linkMesh;
    linkMesh.id = "link_2_mesh";
    linkMesh.linkId = "link_2";
    linkMesh.path = path;
    linkMesh.format = MeshFileFormat::Stl;
    linkMesh.sourceUnits = MeshSourceUnits::Meters;
    linkMesh.scaleToMeters = 1.0;

    profile.meshes = {baseMesh, linkMesh};
    return profile;
}
}

void CollisionBackendTests::primitiveBackendReportsBuiltInCapabilities()
{
    const CollisionBackendInfo info = CollisionBackends::primitiveInfo();

    QCOMPARE(static_cast<int>(info.kind), static_cast<int>(CollisionBackendKind::Primitive));
    QCOMPARE(info.backendName, std::string("primitive_sphere_capsule"));
    QVERIFY(info.available);
    QVERIFY(!info.supportsMesh);
    QVERIFY(info.supportsDistance);
    QVERIFY(!info.supportsContacts);
}

void CollisionBackendTests::meshBackendReportsUnavailableWithoutCompiledAdapter()
{
    const CollisionBackendInfo info = CollisionBackends::meshInfo();

    QCOMPARE(static_cast<int>(info.kind), static_cast<int>(CollisionBackendKind::Mesh));
#ifdef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    QCOMPARE(static_cast<int>(info.meshBackend), static_cast<int>(MeshCollisionBackendKind::Coal));
    QVERIFY(info.available);
    QVERIFY(info.supportsMesh);
    QVERIFY(info.supportsDistance);
    QVERIFY(info.supportsContacts);
    QVERIFY(info.detail.find("Coal") != std::string::npos);
#else
    QCOMPARE(static_cast<int>(info.meshBackend), static_cast<int>(MeshCollisionBackendKind::None));
    QVERIFY(!info.available);
    QVERIFY(!info.supportsMesh);
    QVERIFY(info.detail.find("No mesh collision backend") != std::string::npos);
#endif
}

void CollisionBackendTests::primitiveChecksFlowThroughBackendFacade()
{
    const SerialRobotConfig config = Presets::virtual6DofTestArm();
    const CollisionProfile profile = CollisionProfiles::virtual6DofTestArm();

    CollisionCheckRequest request;
    request.joints = JointVector::fromRadians({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});

    const CollisionCheckResult result = CollisionBackends::checkPrimitive(config, profile, request);

    QCOMPARE(result.status, KinematicsStatus::Ok);
    QVERIFY(!result.hasCollision);
}

void CollisionBackendTests::meshChecksReturnStructuredUnsupportedStatus()
{
    const SerialRobotConfig config = Presets::virtual6DofTestArm();
    MeshCollisionProfile profile;
    profile.id = "draft_mesh_profile";
    profile.robotModel = "Virtual6DofTestArm";

#ifdef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    profile.backendPreference = {MeshCollisionBackendKind::Fcl};

    MeshCollisionCheckRequest request;
    request.joints = JointVector::fromRadians({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    request.preferredBackend = MeshCollisionBackendKind::Fcl;

    const CollisionCheckResult result = CollisionBackends::checkMesh(config, profile, request);

    QCOMPARE(result.status, KinematicsStatus::UnsupportedSolver);
    QVERIFY(!result.hasCollision);
    QVERIFY(result.message.find("fcl") != std::string::npos);
#else
    profile.backendPreference = {MeshCollisionBackendKind::Coal};

    MeshCollisionCheckRequest request;
    request.joints = JointVector::fromRadians({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});

    const CollisionCheckResult result = CollisionBackends::checkMesh(config, profile, request);

    QCOMPARE(result.status, KinematicsStatus::UnsupportedSolver);
    QVERIFY(!result.hasCollision);
    QVERIFY(result.message.find("coal") != std::string::npos);
#endif
}

void CollisionBackendTests::meshChecksDetectSyntheticOverlapAndSeparationWhenCompiled()
{
#ifndef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    QSKIP("Coal mesh backend is not compiled in this build");
#else
    const QString path =
        writeTempFile(QStringLiteral("rk_mesh_backend_synthetic.stl"), asciiStlBytesMeters());
    QVERIFY(!path.isEmpty());

    MeshCollisionProfile profile = syntheticMeshProfile(path.toStdString());

    MeshCollisionCheckRequest overlapRequest;
    overlapRequest.joints = JointVector::fromRadians({0.0, 0.0, 0.0});

    const CollisionCheckResult overlapResult =
        CollisionBackends::checkMesh(lineRobot(), profile, overlapRequest);

    QCOMPARE(overlapResult.status, KinematicsStatus::Ok);
    QVERIFY(overlapResult.hasCollision);
    QCOMPARE(overlapResult.pairs.size(), std::size_t(1));
    QVERIFY(overlapResult.pairs.front().colliding);
    QVERIFY(overlapResult.pairs.front().distance_m <= 0.0);
    QVERIFY(overlapResult.pairs.front().contactCount > 0);

    MeshCollisionCheckRequest separatedRequest;
    separatedRequest.joints = JointVector::fromRadians({kPi * 0.5, 0.0, 0.0});

    const CollisionCheckResult separatedResult =
        CollisionBackends::checkMesh(lineRobot(), profile, separatedRequest);

    QCOMPARE(separatedResult.status, KinematicsStatus::Ok);
    QVERIFY(!separatedResult.hasCollision);
    QCOMPARE(separatedResult.pairs.size(), std::size_t(1));
    QVERIFY(!separatedResult.pairs.front().colliding);
    QVERIFY(separatedResult.pairs.front().distance_m > 0.5);
    QCOMPARE(separatedResult.pairs.front().contactCount, std::size_t(0));

    profile.disabledPairs = {DisabledCollisionPair{"base_mesh", "link_2_mesh", "test_pair_filter"}};
    const CollisionCheckResult filteredResult =
        CollisionBackends::checkMesh(lineRobot(), profile, overlapRequest);

    QCOMPARE(filteredResult.status, KinematicsStatus::Ok);
    QVERIFY(!filteredResult.hasCollision);
    QCOMPARE(filteredResult.pairs.size(), std::size_t(0));
#endif
}

void CollisionBackendTests::meshChecksApplySafetyMarginWhenCompiled()
{
#ifndef ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
    QSKIP("Coal mesh backend is not compiled in this build");
#else
    const QString path =
        writeTempFile(QStringLiteral("rk_mesh_backend_margin.stl"), asciiStlBytesMeters());
    QVERIFY(!path.isEmpty());

    MeshCollisionProfile profile = syntheticMeshProfile(path.toStdString(), 0.75);

    MeshCollisionCheckRequest noMarginRequest;
    noMarginRequest.joints = JointVector::fromRadians({0.0, 0.0, 0.0});

    const CollisionCheckResult noMarginResult =
        CollisionBackends::checkMesh(lineRobot(), profile, noMarginRequest);

    QCOMPARE(noMarginResult.status, KinematicsStatus::Ok);
    QVERIFY(!noMarginResult.hasCollision);
    QCOMPARE(noMarginResult.pairs.size(), std::size_t(1));
    QVERIFY(noMarginResult.pairs.front().distance_m > 0.0);

    MeshCollisionCheckRequest marginRequest = noMarginRequest;
    marginRequest.safetyMargin_m = noMarginResult.pairs.front().distance_m + 0.001;

    const CollisionCheckResult marginResult =
        CollisionBackends::checkMesh(lineRobot(), profile, marginRequest);

    QCOMPARE(marginResult.status, KinematicsStatus::Ok);
    QVERIFY(marginResult.hasCollision);
    QCOMPARE(marginResult.pairs.size(), std::size_t(1));
    QVERIFY(marginResult.pairs.front().colliding);
    QVERIFY(marginResult.pairs.front().distance_m > 0.0);
    QVERIFY(marginResult.pairs.front().distance_m <= marginRequest.safetyMargin_m);
#endif
}

/// Entry point invoked by TestMain to run the CollisionBackendTests suite under QtTest.
/// @return the number of failing test functions (0 on success)
int runCollisionBackendTests(int argc, char** argv)
{
    CollisionBackendTests tests;
    return QTest::qExec(&tests, argc, argv);
}
