#include <RobotKinematics/Collision/CollisionBackend.h>
#include <RobotKinematics/Collision/CollisionProfileJsonLoader.h>
#include <RobotKinematics/Core/Units.h>
#include <RobotKinematics/Core/Ids.h>
#include <RobotKinematics/Model/FrameRegistry.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Presets/NachiMZ04D.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include <Eigen/Geometry>

#include <iomanip>
#include <iostream>
#include <string>

using namespace RobotKinematics;

namespace {

/// Builds the list of directories to search when resolving repo-relative resource paths:
/// the current working directory, the application directory, and up to 8 of its ancestor
/// directories, with duplicates removed.
QStringList searchRoots()
{
    QStringList roots;
    roots << QDir::currentPath() << QCoreApplication::applicationDirPath();

    QDir walker(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8 && walker.cdUp(); ++i) {
        roots << walker.absolutePath();
    }

    roots.removeDuplicates();
    return roots;
}

/// Resolves `relativePath` against the current directory first, then each of searchRoots()
/// in turn, returning the first existing match as an absolute path.
/// @return the resolved absolute path, or an empty string if no candidate exists
QString resolveRepoPath(const QString& relativePath)
{
    const QFileInfo direct(relativePath);
    if (direct.exists()) {
        return direct.absoluteFilePath();
    }

    for (const QString& root : searchRoots()) {
        const QFileInfo candidate(QDir(root).filePath(relativePath));
        if (candidate.exists()) {
            return candidate.absoluteFilePath();
        }
    }

    return {};
}

/// Prints the pose's translation (converted to millimeters) and rotation quaternion to stdout.
void printPose(const Pose& pose)
{
    const Eigen::Vector3d t = pose.translation_m();
    const Eigen::Quaterniond q = pose.rotationQuaternion();

    std::cout << "  translation: "
              << units::toMm(t.x()) << ", "
              << units::toMm(t.y()) << ", "
              << units::toMm(t.z()) << " mm\n";
    std::cout << "  rotation quaternion: "
              << "w=" << q.w() << ", "
              << "x=" << q.x() << ", "
              << "y=" << q.y() << ", "
              << "z=" << q.z() << "\n";
}

/// Prints the joint vector to stdout as a bracketed, comma-separated list of degree values.
void printJointDegrees(const JointVector& joints)
{
    const std::vector<double> values_deg = joints.toDegrees();
    std::cout << std::fixed << std::setprecision(6);
    for (std::size_t i = 0; i < values_deg.size(); ++i) {
        std::cout << (i == 0 ? "  [" : ", ") << values_deg[i];
    }
    std::cout << "] deg\n";
}

/// End-to-end demo for the Nachi MZ04D preset: runs forward kinematics on a sample joint set
/// (also re-expressed in a "ceiling" user frame), solves inverse kinematics back from the
/// resulting flange pose, then loads the robot's collision profile (resolved via
/// resolveRepoPath()) and runs a primitive self-collision check, printing results throughout.
/// @return 0 on success, 1 if IK solving, collision-profile loading, or the collision check fails
int runNachiExample()
{
    const SerialRobotConfig config = Presets::nachiMZ04D();
    const SerialRobotKinematics robot(config);

    UserFrame ceiling_frame;
    ceiling_frame.id = "ceiling_frame";
    ceiling_frame.transform = Pose::fromXYZRPY_mm_deg(0.0, 0.0, 0.0, 180.0, 0.0, 0.0);

    const JointVector sampleJoints = JointVector::fromDegrees({
        28.1579,
        -18.8069,
        163.839,
        -0.710019,
        35.8922,
        152.731,
    });

    std::cout << "Robot: " << config.identity.vendor << " " << config.identity.model << "\n";
    std::cout << "Sample joints:\n";
    printJointDegrees(sampleJoints);

    const Pose flangePose = ForwardKinematics::flangePose(config, sampleJoints);
    std::cout << "\nForward kinematics, base -> flange:\n";
    printPose(flangePose);

    // const FkChain chain = ForwardKinematics::computeChain(config, sampleJoints);
    // const Result<Pose> frameInCeiling = ForwardKinematics::userFrameInBase(chain, ceiling_frame);
    const Pose CeilingPose  = ceiling_frame.transform * flangePose;
    std::cout << "\nForward kinematics, Ceiling -> flange:\n";
    printPose(CeilingPose);

    IKRequest ik;
    ik.targetPose = flangePose;
    ik.seedJoint = sampleJoints;
    ik.options.maxSolutions = 8;

    const IKResult solved = robot.solve(ik);
    if (!solved.ok()) {
        std::cerr << "\nIK failed: " << solved.message << "\n";
        return 1;
    }

    std::cout << "\nInverse kinematics, best solution:\n";
    printJointDegrees(solved.best().joints);
    std::cout << "  position error: " << solved.best().positionError_m << " m\n";
    std::cout << "  orientation error: " << solved.best().orientationError_rad << " rad\n";

    const IKResult allSolutions = robot.solveAll(ik);
    std::cout << "  solutions found by solveAll: " << allSolutions.solutions.size() << "\n";

    auto collisionProfileIt = config.metadata.find("collisionProfile");
    if (collisionProfileIt == config.metadata.end()) {
        std::cerr << "\nCollision profile metadata is missing.\n";
        return 1;
    }

    const QString collisionProfilePath = resolveRepoPath(QString::fromStdString(collisionProfileIt->second));
    if (collisionProfilePath.isEmpty()) {
        std::cerr << "\nCould not find collision profile: " << collisionProfileIt->second << "\n";
        return 1;
    }

    const Result<CollisionProfile> collisionProfile =
        CollisionProfileJsonLoader::loadFile(collisionProfilePath.toStdString());
    if (!collisionProfile.ok()) {
        std::cerr << "\nCould not load collision profile: " << collisionProfile.message << "\n";
        return 1;
    }

    CollisionCheckRequest collisionRequest;
    collisionRequest.joints = sampleJoints;
    collisionRequest.returnAllPairs = true;

    const CollisionCheckResult collision =
        CollisionBackends::checkPrimitive(config, collisionProfile.value, collisionRequest);
    if (!collision.ok()) {
        std::cerr << "\nPrimitive collision check failed: " << collision.message << "\n";
        return 1;
    }

    std::cout << "\nPrimitive self-collision:\n";
    std::cout << "  has collision: " << (collision.hasCollision ? "true" : "false") << "\n";
    std::cout << "  checked pairs: " << collision.pairs.size() << "\n";
    for (const CollisionPairResult& pair : collision.pairs) {
        if (!pair.colliding) {
            continue;
        }
        std::cout << "  colliding pair: " << pair.linkA << "/" << pair.geometryA
                  << " <-> " << pair.linkB << "/" << pair.geometryB << "\n";
    }

    const CollisionBackendInfo meshInfo = CollisionBackends::meshInfo();
    std::cout << "\nMesh backend: " << meshInfo.backendName
              << " available=" << (meshInfo.available ? "true" : "false") << "\n";
    return 0;
}

} // namespace

/// Entry point for the NachiMZ04Cli example: creates a minimal QCoreApplication (needed for
/// Qt types used by the library) and runs runNachiExample().
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);
    return runNachiExample();
}
