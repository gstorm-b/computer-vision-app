#include <RobotKinematics/Collision/CollisionProfileJsonLoader.h>

#include <RobotKinematics/Core/Pose.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <set>

namespace RobotKinematics {

namespace {
/// Builds a failure Result<CollisionProfile> carrying `message` with status
/// KinematicsStatus::InvalidRobotConfig; used as the terminal early-return for every
/// validation failure in loadJson.
Result<CollisionProfile> invalid(const std::string& message)
{
    return Result<CollisionProfile>::failure(KinematicsStatus::InvalidRobotConfig, message);
}

/// Returns true if every key present in `object` is one of the recognized top-level
/// collision-profile fields (schema, profile, geometries, disabledPairs, sources, metadata).
bool hasOnlyKnownTopLevelFields(const QJsonObject& object)
{
    const std::set<QString> known = {
        "schema", "profile", "geometries", "disabledPairs", "sources", "metadata",
    };
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (known.find(it.key()) == known.end()) {
            return false;
        }
    }
    return true;
}

/// Reads `key` from `object` as a string, returning an empty std::string when the key is
/// absent or its value is not a string.
std::string stringField(const QJsonObject& object, const char* key)
{
    return object.value(key).toString().toStdString();
}

/// Builds a Pose from `object`'s "xyz_m" and "rpy_rad" fields, each expected to be a
/// 3-element JSON array (x/y/z in meters, roll/pitch/yaw in radians).
/// @note does not validate array size or element type; callers must ensure both arrays have
/// at least 3 numeric entries.
Pose poseFromObject(const QJsonObject& object)
{
    const QJsonArray xyz = object.value("xyz_m").toArray();
    const QJsonArray rpy = object.value("rpy_rad").toArray();
    return Pose::fromXYZRPY_m_rad(xyz.at(0).toDouble(), xyz.at(1).toDouble(), xyz.at(2).toDouble(),
                                  rpy.at(0).toDouble(), rpy.at(1).toDouble(), rpy.at(2).toDouble());
}
}

/// Loads a collision profile from the JSON file at `path` by reading it fully into memory and
/// delegating to loadJson.
Result<CollisionProfile> CollisionProfileJsonLoader::loadFile(const std::string& path)
{
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        return Result<CollisionProfile>::failure(KinematicsStatus::InvalidRequest,
                                                 "collision profile file could not be opened");
    }
    return loadJson(QString::fromUtf8(file.readAll()).toStdString());
}

/// Parses and validates a collision profile encoded as JSON text against the
/// "robot-kinematics-collision/v1" schema (m/rad units only), populating id, robotModel,
/// per-geometry sphere/capsule shapes, disabled pairs, sources, and string-valued metadata.
Result<CollisionProfile> CollisionProfileJsonLoader::loadJson(const std::string& json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(json), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return invalid("collision profile JSON is not a valid object");
    }

    const QJsonObject root = document.object();
    if (!hasOnlyKnownTopLevelFields(root)) {
        return invalid("collision profile contains unknown top-level fields");
    }
    if (root.value("schema").toString() != "robot-kinematics-collision/v1") {
        return invalid("unsupported collision profile schema");
    }

    const QJsonObject profileObject = root.value("profile").toObject();
    const QJsonObject units = profileObject.value("units").toObject();
    if (units.value("length").toString() != "m" || units.value("angle").toString() != "rad") {
        return invalid("collision profile units must be m/rad");
    }

    CollisionProfile profile;
    profile.id = stringField(profileObject, "id");
    profile.robotModel = stringField(profileObject, "robotModel");

    for (const QJsonValue& geometryValue : root.value("geometries").toArray()) {
        const QJsonObject geometryObject = geometryValue.toObject();
        CollisionGeometry geometry;
        geometry.id = stringField(geometryObject, "id");
        geometry.linkId = stringField(geometryObject, "link");
        geometry.geometryToLink = poseFromObject(geometryObject.value("geometryToLink").toObject());
        geometry.margin_m = geometryObject.value("margin_m").toDouble();
        geometry.enabled = !geometryObject.contains("enabled") || geometryObject.value("enabled").toBool();

        const QString shape = geometryObject.value("shape").toString();
        if (shape == "sphere") {
            geometry.shape.type = CollisionShapeType::Sphere;
            geometry.shape.sphere.radius_m =
                geometryObject.value("sphere").toObject().value("radius_m").toDouble();
        } else if (shape == "capsule") {
            geometry.shape.type = CollisionShapeType::Capsule;
            const QJsonObject capsule = geometryObject.value("capsule").toObject();
            geometry.shape.capsule.radius_m = capsule.value("radius_m").toDouble();
            geometry.shape.capsule.length_m = capsule.value("length_m").toDouble();
        } else {
            return invalid("collision profile shape must be sphere or capsule");
        }

        profile.geometries.push_back(geometry);
    }

    for (const QJsonValue& pairValue : root.value("disabledPairs").toArray()) {
        const QJsonObject pairObject = pairValue.toObject();
        profile.disabledPairs.push_back(DisabledCollisionPair{
            stringField(pairObject, "a"),
            stringField(pairObject, "b"),
            stringField(pairObject, "reason"),
        });
    }

    for (const QJsonValue& sourceValue : root.value("sources").toArray()) {
        const QJsonObject sourceObject = sourceValue.toObject();
        SourceReference source;
        source.type = stringField(sourceObject, "type");
        source.title = stringField(sourceObject, "title");
        source.reference = stringField(sourceObject, "reference");
        for (const QJsonValue& appliesTo : sourceObject.value("appliesTo").toArray()) {
            source.appliesTo.push_back(appliesTo.toString().toStdString());
        }
        profile.sources.push_back(source);
    }

    const QJsonObject metadata = root.value("metadata").toObject();
    for (auto it = metadata.begin(); it != metadata.end(); ++it) {
        if (it.value().isString()) {
            profile.metadata[it.key().toStdString()] = it.value().toString().toStdString();
        }
    }

    return Result<CollisionProfile>::success(profile);
}

} // namespace RobotKinematics
