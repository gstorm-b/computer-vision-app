#include <RobotKinematics/Collision/CollisionBackend.h>
#include <RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h>
#include <RobotKinematics/Presets/PresetJsonLoader.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QStringList>

#include <Eigen/Core>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

using namespace RobotKinematics;

/// Local helper types and functions for the mesh collision benchmark CLI: building
/// synthetic/file-driven scenarios, timing repeated CollisionBackends::checkMesh calls,
/// and printing the results.
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;  ///< Pi, used to build revolute joint limits and benchmark joint angles.

/// A single 3D point in meters, used to describe synthetic STL triangle vertices.
struct Vec3
{
    double x;  ///< X coordinate in meters.
    double y;  ///< Y coordinate in meters.
    double z;  ///< Z coordinate in meters.
};

/// A single synthetic mesh triangle, defined by its three vertices in meters.
struct Triangle
{
    Vec3 a;  ///< First vertex.
    Vec3 b;  ///< Second vertex.
    Vec3 c;  ///< Third vertex.
};

/// One benchmark case: a robot/mesh-profile pair plus the joint configuration to evaluate it at.
struct ScenarioDefinition
{
    std::string label;  ///< Human-readable name printed alongside the scenario's measurement.
    SerialRobotConfig robot;  ///< Robot configuration used for forward kinematics during the check.
    MeshCollisionProfile profile;  ///< Mesh collision profile (link meshes + backend preference) checked against.
    JointVector joints;  ///< Joint configuration the scenario is evaluated at.
};

/// Timing and collision-result summary produced by running a scenario through measureScenario().
struct ScenarioMeasurement
{
    std::string label;  ///< Scenario label this measurement corresponds to.
    KinematicsStatus status = KinematicsStatus::Ok;  ///< Status of the last (or failing warm-up) collision check.
    bool hasCollision = false;  ///< Whether the last collision check reported any colliding pair.
    std::size_t pairCount = 0;  ///< Number of collision pairs reported by the last check.
    std::size_t contactCount = 0;  ///< Contact count of the first reported pair (0 if no pairs).
    double distance_m = 0.0;  ///< Distance in meters of the first reported pair (0 if no pairs).
    qint64 totalNs = 0;  ///< Total elapsed nanoseconds across the measured iterations.
    int iterations = 0;  ///< Number of measured (non-warm-up) iterations actually run.
    int warmupIterations = 0;  ///< Number of warm-up iterations configured for this measurement.
};

/// Builds a revolute joint rotating about its local Z axis, with limits set to [-pi, pi]
/// and home position 0.
/// @param id joint identifier
/// @param parent parent link id
/// @param child child link id
/// @param origin joint origin pose relative to the parent link
/// @return the constructed Joint
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

/// Builds a fixed 3-DOF synthetic serial robot ("CollisionLineRobot") made of three
/// 1 m revolute-Z links in a straight line, used as the fixture robot for the synthetic
/// benchmark scenarios.
/// @return the constructed robot configuration
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

/// Builds the 12 triangles (2 per face) of a 0.2 x 0.1 x 0.1 m axis-aligned cuboid with
/// one corner at the origin, used as the synthetic collision mesh geometry.
/// @return the cuboid's triangles, in meters
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

/// Serializes cuboidTrianglesMeters() as an ASCII STL "solid cuboid" document (facet
/// normals are written as 0 0 0 since only geometry, not normals, is needed by the check).
/// @return the ASCII STL file content, in meters
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

/// Writes `bytes` to `fileName` under the system temp directory, first removing any
/// existing file at that path.
/// @param fileName file name (not full path) to create under QDir::temp()
/// @param bytes content to write
/// @return the full temp-file path on success, or an empty string if the file could not
///         be opened or the write was short
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

/// Builds a Coal-backed mesh collision profile for the synthetic scenarios: the same
/// mesh file at `path` is attached both to base_link (offset 0.85 m along X) and to
/// link_2 (no offset), so the two instances can be moved apart/together by the joint angles.
/// @param path filesystem path to the mesh file (STL) shared by both geometries
/// @return the constructed mesh collision profile
MeshCollisionProfile syntheticMeshProfile(const std::string& path)
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
    baseMesh.meshToLink = Pose::fromXYZRPY_m_rad(0.85, 0.0, 0.0, 0.0, 0.0, 0.0);

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

/// Parses a comma-separated list of joint values (radians) from a --joints-rad argument.
/// @param csv comma-separated joint values; empty tokens are skipped
/// @return the parsed values on success, or InvalidRequest with a message naming the
///         offending token if any token fails to parse as a finite double
Result<std::vector<double>> parseJointCsv(const QString& csv)
{
    const QStringList tokens = csv.split(',', Qt::SkipEmptyParts);
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(tokens.size()));

    for (const QString& token : tokens) {
        bool ok = false;
        const double value = token.trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return Result<std::vector<double>>::failure(
                KinematicsStatus::InvalidRequest,
                "failed to parse joints-rad value: " + token.toStdString());
        }
        values.push_back(value);
    }

    return Result<std::vector<double>>::success(values);
}

/// Builds a synthetic benchmark scenario: writes the synthetic cuboid STL fixture to a
/// temp file, then assembles lineRobot() + syntheticMeshProfile() at the given joint angles.
/// @param label human-readable scenario name
/// @param joints joint angles in radians (converted via JointVector::fromRadians)
/// @return the constructed scenario on success, or InvalidRequest if the STL fixture
///         could not be written
Result<ScenarioDefinition> buildSyntheticScenario(const std::string& label, const std::vector<double>& joints)
{
    const QString meshPath =
        writeTempFile(QStringLiteral("rk_mesh_collision_benchmark_synthetic.stl"), asciiStlBytesMeters());
    if (meshPath.isEmpty()) {
        return Result<ScenarioDefinition>::failure(KinematicsStatus::InvalidRequest,
                                                   "failed to write synthetic STL fixture");
    }

    ScenarioDefinition scenario;
    scenario.label = label;
    scenario.robot = lineRobot();
    scenario.profile = syntheticMeshProfile(meshPath.toStdString());
    scenario.joints = JointVector::fromRadians(joints);
    return Result<ScenarioDefinition>::success(std::move(scenario));
}

/// Builds a file-driven benchmark scenario by loading a robot preset JSON, a mesh
/// collision profile JSON, and a joints-rad CSV string, failing on the first error found.
/// @param label human-readable scenario name
/// @param presetPath path to the robot preset JSON file
/// @param meshProfilePath path to the mesh collision profile JSON file
/// @param jointsCsv comma-separated joint values in radians
/// @return the constructed scenario on success, or the status/message of whichever
///         load/parse step failed first
Result<ScenarioDefinition> loadJsonScenario(const QString& label,
                                            const QString& presetPath,
                                            const QString& meshProfilePath,
                                            const QString& jointsCsv)
{
    const Result<SerialRobotConfig> robot = PresetJsonLoader::loadFile(presetPath.toStdString());
    if (!robot.ok()) {
        return Result<ScenarioDefinition>::failure(
            robot.status,
            "failed to load preset JSON: " + robot.message);
    }

    const Result<MeshCollisionProfile> profile =
        MeshCollisionProfileJsonLoader::loadFile(meshProfilePath.toStdString());
    if (!profile.ok()) {
        return Result<ScenarioDefinition>::failure(
            profile.status,
            "failed to load mesh profile JSON: " + profile.message);
    }

    const Result<std::vector<double>> joints = parseJointCsv(jointsCsv);
    if (!joints.ok()) {
        return Result<ScenarioDefinition>::failure(joints.status, joints.message);
    }

    ScenarioDefinition scenario;
    scenario.label = label.toStdString();
    scenario.robot = robot.value;
    scenario.profile = profile.value;
    scenario.joints = JointVector::fromRadians(joints.value);
    return Result<ScenarioDefinition>::success(std::move(scenario));
}

/// Runs `warmupIterations` untimed CollisionBackends::checkMesh calls followed by
/// `iterations` timed calls for `scenario`, using a QElapsedTimer around the timed loop.
/// If a warm-up call fails, returns immediately with that failure's status/result and
/// totalNs left at 0; if a timed call fails, the timed loop stops early and the
/// already-elapsed time is still reported.
/// @param scenario scenario (robot + mesh profile + joints) to check
/// @param iterations number of timed iterations to run
/// @param warmupIterations number of untimed iterations to run before timing starts
/// @return summary of the last executed check plus elapsed timing
ScenarioMeasurement measureScenario(const ScenarioDefinition& scenario,
                                    const int iterations,
                                    const int warmupIterations)
{
    MeshCollisionCheckRequest request;
    request.joints = scenario.joints;

    for (int index = 0; index < warmupIterations; ++index) {
        const CollisionCheckResult warmup = CollisionBackends::checkMesh(scenario.robot, scenario.profile, request);
        if (!warmup.ok()) {
            return ScenarioMeasurement{
                scenario.label,
                warmup.status,
                warmup.hasCollision,
                warmup.pairs.size(),
                warmup.pairs.empty() ? std::size_t(0) : warmup.pairs.front().contactCount,
                warmup.pairs.empty() ? 0.0 : warmup.pairs.front().distance_m,
                0,
                0,
                warmupIterations,
            };
        }
    }

    CollisionCheckResult lastResult;
    QElapsedTimer timer;
    timer.start();
    for (int index = 0; index < iterations; ++index) {
        lastResult = CollisionBackends::checkMesh(scenario.robot, scenario.profile, request);
        if (!lastResult.ok()) {
            break;
        }
    }

    return ScenarioMeasurement{
        scenario.label,
        lastResult.status,
        lastResult.hasCollision,
        lastResult.pairs.size(),
        lastResult.pairs.empty() ? std::size_t(0) : lastResult.pairs.front().contactCount,
        lastResult.pairs.empty() ? 0.0 : lastResult.pairs.front().distance_m,
        timer.nsecsElapsed(),
        iterations,
        warmupIterations,
    };
}

/// Prints a single-line "key=value" summary of `measurement` to stdout, including
/// total elapsed time in milliseconds and average time per iteration in microseconds.
/// @param measurement measurement to print
void printMeasurement(const ScenarioMeasurement& measurement)
{
    const double totalMs = static_cast<double>(measurement.totalNs) / 1.0e6;
    const double avgUs = measurement.iterations > 0
                             ? (static_cast<double>(measurement.totalNs) / static_cast<double>(measurement.iterations)) / 1.0e3
                             : 0.0;

    std::cout << "scenario=" << measurement.label
              << " status=" << static_cast<int>(measurement.status)
              << " iterations=" << measurement.iterations
              << " warmup=" << measurement.warmupIterations
              << " hasCollision=" << measurement.hasCollision
              << " pairCount=" << measurement.pairCount
              << " contactCount=" << measurement.contactCount
              << " distance_m=" << measurement.distance_m
              << " total_ms=" << totalMs
              << " avg_us=" << avgUs
              << std::endl;
}
}

/// Entry point for the RobotKinematics mesh collision benchmark CLI. Parses --iterations,
/// --warmup, and either the synthetic scenarios (default) or a single file-driven scenario
/// (when --preset-json/--mesh-profile-json/--joints-rad are all supplied), runs
/// measureScenario() for each and prints its result, and optionally dumps per-pair
/// distance/contact info via --dump-pairs.
/// @param argc argument count, forwarded to QCoreApplication
/// @param argv argument vector, forwarded to QCoreApplication
/// @return 0 on success; 1 on invalid arguments, scenario build/load failure, or a
///         benchmark scenario failing before measurement completed
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("RobotKinematics mesh collision benchmark");
    parser.addHelpOption();

    const QCommandLineOption iterationsOption(
        QStringList{"i", "iterations"},
        "Measured iterations per scenario.",
        "count",
        "1000");
    const QCommandLineOption warmupOption(
        QStringList{"w", "warmup"},
        "Warm-up iterations per scenario.",
        "count",
        "100");
    const QCommandLineOption presetJsonOption(
        QStringList{"preset-json"},
        "Path to a robot preset JSON file for file-driven benchmarking.",
        "path");
    const QCommandLineOption meshProfileJsonOption(
        QStringList{"mesh-profile-json"},
        "Path to a mesh collision profile JSON file for file-driven benchmarking.",
        "path");
    const QCommandLineOption jointsRadOption(
        QStringList{"joints-rad"},
        "Comma-separated joint vector in radians for file-driven benchmarking.",
        "csv");
    const QCommandLineOption dumpPairsOption(
        QStringList{"dump-pairs"},
        "After running, dump per-pair distance/colliding info for the final iteration.");

    parser.addOption(iterationsOption);
    parser.addOption(warmupOption);
    parser.addOption(presetJsonOption);
    parser.addOption(meshProfileJsonOption);
    parser.addOption(jointsRadOption);
    parser.addOption(dumpPairsOption);
    parser.process(app);

    bool iterationsOk = false;
    const int iterations = parser.value(iterationsOption).toInt(&iterationsOk);
    bool warmupOk = false;
    const int warmupIterations = parser.value(warmupOption).toInt(&warmupOk);
    if (!iterationsOk || iterations <= 0 || !warmupOk || warmupIterations < 0) {
        std::cerr << "[ERROR] iterations must be > 0 and warmup must be >= 0." << std::endl;
        return 1;
    }

    std::vector<ScenarioDefinition> scenarios;

    const bool hasFileScenarioOptions =
        parser.isSet(presetJsonOption) || parser.isSet(meshProfileJsonOption) || parser.isSet(jointsRadOption);
    if (hasFileScenarioOptions) {
        if (!parser.isSet(presetJsonOption) || !parser.isSet(meshProfileJsonOption) || !parser.isSet(jointsRadOption)) {
            std::cerr << "[ERROR] preset-json, mesh-profile-json, and joints-rad must be provided together." << std::endl;
            return 1;
        }

        const Result<ScenarioDefinition> scenario =
            loadJsonScenario("file_json_case",
                             parser.value(presetJsonOption),
                             parser.value(meshProfileJsonOption),
                             parser.value(jointsRadOption));
        if (!scenario.ok()) {
            std::cerr << "[ERROR] " << scenario.message << std::endl;
            return 1;
        }
        scenarios.push_back(scenario.value);
    } else {
        const Result<ScenarioDefinition> overlapScenario =
            buildSyntheticScenario("synthetic_overlap", {0.0, 0.0, 0.0});
        if (!overlapScenario.ok()) {
            std::cerr << "[ERROR] " << overlapScenario.message << std::endl;
            return 1;
        }

        const Result<ScenarioDefinition> separatedScenario =
            buildSyntheticScenario("synthetic_separated", {kPi * 0.5, 0.0, 0.0});
        if (!separatedScenario.ok()) {
            std::cerr << "[ERROR] " << separatedScenario.message << std::endl;
            return 1;
        }

        scenarios.push_back(overlapScenario.value);
        scenarios.push_back(separatedScenario.value);
    }

    for (const ScenarioDefinition& scenario : scenarios) {
        const ScenarioMeasurement measurement = measureScenario(scenario, iterations, warmupIterations);
        printMeasurement(measurement);
        if (measurement.status != KinematicsStatus::Ok) {
            std::cerr << "[ERROR] benchmark scenario failed before measurement completed: "
                      << measurement.label << std::endl;
            return 1;
        }
        if (parser.isSet(dumpPairsOption)) {
            MeshCollisionCheckRequest request;
            request.joints = scenario.joints;
            request.returnAllPairs = true;
            const CollisionCheckResult result =
                CollisionBackends::checkMesh(scenario.robot, scenario.profile, request);
            for (const CollisionPairResult& pair : result.pairs) {
                std::cout << "pair scenario=" << scenario.label
                          << " a=" << pair.geometryA
                          << " b=" << pair.geometryB
                          << " colliding=" << pair.colliding
                          << " distance_m=" << pair.distance_m
                          << " contactCount=" << pair.contactCount
                          << std::endl;
            }
        }
    }

    return 0;
}
