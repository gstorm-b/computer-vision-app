#include <RobotKinematics/Adapters/UrdfAdapter.h>

#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Model/RobotModelValidator.h>

#include <QXmlStreamReader>

#include <Eigen/Geometry>

#include <QStringList>

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace RobotKinematics {

namespace {
/// Maps a JointType to its URDF `type` attribute string; anything other than
/// Prismatic/Fixed is reported as "revolute".
std::string jointTypeName(JointType type)
{
    if (type == JointType::Prismatic) {
        return "prismatic";
    }
    if (type == JointType::Fixed) {
        return "fixed";
    }
    return "revolute";
}

/// Parses a URDF joint `type` attribute string into a JointType; any value other than
/// "prismatic"/"fixed" (including unrecognized strings) resolves to Revolute.
JointType jointTypeFromName(const QString& type)
{
    if (type == "prismatic") {
        return JointType::Prismatic;
    }
    if (type == "fixed") {
        return JointType::Fixed;
    }
    return JointType::Revolute;
}

/// Extracts URDF-convention roll/pitch/yaw Euler angles from a Pose's rotation matrix.
/// @param pose the pose whose orientation is decomposed
/// @return vector of (roll, pitch, yaw) in radians; the pitch asin input is clamped to
/// [-1, 1] to avoid NaN from floating-point rounding at/near gimbal lock
Eigen::Vector3d rpyFromPose(const Pose& pose)
{
    const Eigen::Matrix3d r = pose.isometry().linear();
    const double pitch = std::asin(std::max(-1.0, std::min(1.0, -r(2, 0))));
    const double roll = std::atan2(r(2, 1), r(2, 2));
    const double yaw = std::atan2(r(1, 0), r(0, 0));
    return Eigen::Vector3d(roll, pitch, yaw);
}

/// Formats a 3-vector as a space-separated "x y z" string for URDF xyz/rpy attributes.
std::string vec3(const Eigen::Vector3d& v)
{
    std::ostringstream out;
    out << v.x() << " " << v.y() << " " << v.z();
    return out.str();
}

/// Parses a space-separated "x y z" URDF attribute string into a Vector3d.
/// @param value the attribute text to parse
/// @param fallback value returned unchanged if `value` does not split into exactly 3 parts
Eigen::Vector3d parseVec3(const QString& value, const Eigen::Vector3d& fallback)
{
    const QStringList parts = value.split(' ', Qt::SkipEmptyParts);
    if (parts.size() != 3) {
        return fallback;
    }
    return Eigen::Vector3d(parts[0].toDouble(), parts[1].toDouble(), parts[2].toDouble());
}
}

/// Serializes a serial robot configuration to a minimal URDF XML document: emits a
/// `<link>` per link and a `<joint>` per joint (with origin xyz/rpy, and axis/limit for
/// non-fixed joints), plus a trailing XML comment when posture/tools/sources metadata
/// exists that URDF cannot represent. Validates `config` first via RobotModelValidator.
Result<std::string> UrdfAdapter::exportSerialRobot(const SerialRobotConfig& config)
{
    const ModelValidationResult validation = RobotModelValidator::validateSerialRobotConfig(config);
    if (!validation.ok()) {
        return Result<std::string>::failure(validation.status(), validation.issues.front().message);
    }

    std::ostringstream xml;
    xml << "<robot name=\"" << config.identity.model << "\">\n";
    for (const Link& link : config.links) {
        xml << "  <link name=\"" << link.id << "\"/>\n";
    }
    for (const Joint& joint : config.joints) {
        const Eigen::Vector3d rpy = rpyFromPose(joint.origin);
        xml << "  <joint name=\"" << joint.id << "\" type=\"" << jointTypeName(joint.type) << "\">\n";
        xml << "    <parent link=\"" << joint.parentLinkId << "\"/>\n";
        xml << "    <child link=\"" << joint.childLinkId << "\"/>\n";
        xml << "    <origin xyz=\"" << vec3(joint.origin.translation_m()) << "\" rpy=\"" << vec3(rpy) << "\"/>\n";
        if (joint.type != JointType::Fixed) {
            xml << "    <axis xyz=\"" << vec3(joint.axis) << "\"/>\n";
            if (joint.limits.has_value()) {
                xml << "    <limit lower=\"" << joint.limits->lower << "\" upper=\"" << joint.limits->upper << "\"/>\n";
            }
        }
        xml << "  </joint>\n";
    }
    if (!config.posture.resolver.empty() || !config.tools.empty() || !config.sources.empty()) {
        xml << "  <!-- RobotKinematics metadata not represented in URDF: tools, posture, solver, sources. -->\n";
    }
    xml << "</robot>\n";
    return Result<std::string>::success(xml.str());
}

/// Parses a URDF XML document into a serial robot configuration: reads all `<link>` and
/// `<joint>` elements, then walks the joint graph from `baseLinkId` to `flangeLinkId`
/// (following each joint's parent/child link chain) to build the ordered joint list.
/// Non-fixed joints without an explicit `<limit>` default to +/-pi radians. The dof count
/// is the number of Revolute/Prismatic joints in the resulting chain. The assembled
/// config is validated via RobotModelValidator before being returned.
Result<SerialRobotConfig> UrdfAdapter::importSerialRobot(const std::string& urdf,
                                                         const std::string& baseLinkId,
                                                         const std::string& flangeLinkId)
{
    SerialRobotConfig config;
    config.topology = RobotTopologyType::Serial;
    config.frames.baseLinkId = baseLinkId;
    config.frames.flangeLinkId = flangeLinkId;

    std::vector<Joint> parsedJoints;
    QXmlStreamReader reader(QString::fromStdString(urdf));
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }
        if (reader.name() == QStringLiteral("robot")) {
            config.identity.model = reader.attributes().value("name").toString().toStdString();
        } else if (reader.name() == QStringLiteral("link")) {
            config.links.push_back(Link{reader.attributes().value("name").toString().toStdString()});
        } else if (reader.name() == QStringLiteral("joint")) {
            Joint joint;
            joint.id = reader.attributes().value("name").toString().toStdString();
            joint.type = jointTypeFromName(reader.attributes().value("type").toString());
            while (!(reader.isEndElement() && reader.name() == QStringLiteral("joint")) && !reader.atEnd()) {
                reader.readNext();
                if (!reader.isStartElement()) {
                    continue;
                }
                if (reader.name() == QStringLiteral("parent")) {
                    joint.parentLinkId = reader.attributes().value("link").toString().toStdString();
                } else if (reader.name() == QStringLiteral("child")) {
                    joint.childLinkId = reader.attributes().value("link").toString().toStdString();
                } else if (reader.name() == QStringLiteral("origin")) {
                    const Eigen::Vector3d xyz = parseVec3(reader.attributes().value("xyz").toString(), Eigen::Vector3d::Zero());
                    const Eigen::Vector3d rpy = parseVec3(reader.attributes().value("rpy").toString(), Eigen::Vector3d::Zero());
                    joint.origin = Pose::fromXYZRPY_m_rad(xyz.x(), xyz.y(), xyz.z(), rpy.x(), rpy.y(), rpy.z());
                } else if (reader.name() == QStringLiteral("axis")) {
                    joint.axis = parseVec3(reader.attributes().value("xyz").toString(), Eigen::Vector3d::UnitZ());
                } else if (reader.name() == QStringLiteral("limit")) {
                    JointLimits limits;
                    limits.lower = reader.attributes().value("lower").toDouble();
                    limits.upper = reader.attributes().value("upper").toDouble();
                    joint.limits = limits;
                }
            }
            if (joint.type != JointType::Fixed && !joint.limits.has_value()) {
                joint.limits = JointLimits{-3.141592653589793, 3.141592653589793, std::nullopt, std::nullopt};
            }
            parsedJoints.push_back(joint);
        }
    }
    if (reader.hasError()) {
        return Result<SerialRobotConfig>::failure(KinematicsStatus::InvalidRobotConfig, "URDF XML parse error");
    }

    std::string current = baseLinkId;
    while (current != flangeLinkId) {
        auto next = std::find_if(parsedJoints.begin(), parsedJoints.end(), [&](const Joint& joint) {
            return joint.parentLinkId == current;
        });
        if (next == parsedJoints.end()) {
            return Result<SerialRobotConfig>::failure(KinematicsStatus::InvalidRobotConfig,
                                                      "URDF serial chain is incomplete");
        }
        config.joints.push_back(*next);
        current = next->childLinkId;
    }

    int movable = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type == JointType::Revolute || joint.type == JointType::Prismatic) {
            ++movable;
        }
    }
    config.dof = movable;

    const ModelValidationResult validation = RobotModelValidator::validateSerialRobotConfig(config);
    if (!validation.ok()) {
        return Result<SerialRobotConfig>::failure(validation.status(), validation.issues.front().message);
    }
    return Result<SerialRobotConfig>::success(config);
}

} // namespace RobotKinematics
