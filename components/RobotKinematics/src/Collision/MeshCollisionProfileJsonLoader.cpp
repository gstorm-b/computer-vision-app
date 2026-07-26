#include <RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>
#include <limits>
#include <set>

namespace RobotKinematics {

namespace {
/// Builds a failure Result<MeshCollisionProfile> carrying `message` with status
/// KinematicsStatus::InvalidRobotConfig; used as the terminal early-return for every
/// validation failure in loadJson.
Result<MeshCollisionProfile> invalid(const std::string& message)
{
    return Result<MeshCollisionProfile>::failure(KinematicsStatus::InvalidRobotConfig, message);
}

/// Returns true if every key present in `object` is one of the recognized top-level
/// mesh-collision-profile fields (schema, profile, meshes, disabledPairs, sources, metadata).
bool hasOnlyKnownTopLevelFields(const QJsonObject& object)
{
    const std::set<QString> known = {
        "schema", "profile", "meshes", "disabledPairs", "sources", "metadata",
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

/// Returns true if `value` is a JSON number and its double representation is finite
/// (excludes NaN/infinity as well as non-numeric JSON values).
bool isFiniteNumber(const QJsonValue& value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

/// Returns true if `object` contains `key` and its value is a finite JSON number.
bool hasFiniteNumericField(const QJsonObject& object, const char* key)
{
    return object.contains(key) && isFiniteNumber(object.value(key));
}

/// Returns true if `object`'s `key` field is a JSON array of exactly 3 finite numbers.
bool hasFiniteNumericArray(const QJsonObject& object, const char* key)
{
    const QJsonArray array = object.value(key).toArray();
    if (array.size() != 3) {
        return false;
    }
    for (const QJsonValue& value : array) {
        if (!isFiniteNumber(value)) {
            return false;
        }
    }
    return true;
}

/// Builds a Pose from `object`'s "xyz_m" and "rpy_rad" 3-element JSON arrays (x/y/z in
/// meters, roll/pitch/yaw in radians).
/// @note callers in this file validate both arrays with hasFiniteNumericArray before invoking
/// this; it does not itself check array size or numeric type.
Pose poseFromObject(const QJsonObject& object)
{
    const QJsonArray xyz = object.value("xyz_m").toArray();
    const QJsonArray rpy = object.value("rpy_rad").toArray();
    return Pose::fromXYZRPY_m_rad(xyz.at(0).toDouble(), xyz.at(1).toDouble(), xyz.at(2).toDouble(),
                                  rpy.at(0).toDouble(), rpy.at(1).toDouble(), rpy.at(2).toDouble());
}

/// Parses a mesh source-units string ("m" or "mm") into its MeshSourceUnits enumerator.
/// @return the matching MeshSourceUnits on success; a failure Result with
/// KinematicsStatus::InvalidRobotConfig otherwise
Result<MeshSourceUnits> parseSourceUnits(const QString& value)
{
    if (value == "m") {
        return Result<MeshSourceUnits>::success(MeshSourceUnits::Meters);
    }
    if (value == "mm") {
        return Result<MeshSourceUnits>::success(MeshSourceUnits::Millimeters);
    }
    return Result<MeshSourceUnits>::failure(KinematicsStatus::InvalidRobotConfig,
                                            "mesh sourceUnits must be 'm' or 'mm'");
}

/// Parses a mesh file-format string (currently only "stl") into its MeshFileFormat enumerator.
/// @return the matching MeshFileFormat on success; a failure Result with
/// KinematicsStatus::InvalidRobotConfig otherwise
Result<MeshFileFormat> parseFormat(const QString& value)
{
    if (value == "stl") {
        return Result<MeshFileFormat>::success(MeshFileFormat::Stl);
    }
    return Result<MeshFileFormat>::failure(KinematicsStatus::InvalidRobotConfig,
                                           "mesh format must be 'stl'");
}

/// Parses a mesh quality-mode string ("original", "simplified", or "convex") into its
/// MeshQualityMode enumerator.
/// @return the matching MeshQualityMode on success; a failure Result with
/// KinematicsStatus::InvalidRobotConfig otherwise
Result<MeshQualityMode> parseQualityMode(const QString& value)
{
    if (value == "original") {
        return Result<MeshQualityMode>::success(MeshQualityMode::Original);
    }
    if (value == "simplified") {
        return Result<MeshQualityMode>::success(MeshQualityMode::Simplified);
    }
    if (value == "convex") {
        return Result<MeshQualityMode>::success(MeshQualityMode::Convex);
    }
    return Result<MeshQualityMode>::failure(KinematicsStatus::InvalidRobotConfig,
                                            "mesh quality mode must be original, simplified, or convex");
}
}

/// Loads a mesh collision profile from the JSON file at `path`, then rewrites every mesh's
/// `path` field that is not already absolute into an absolute path resolved against the
/// profile file's own directory.
Result<MeshCollisionProfile> MeshCollisionProfileJsonLoader::loadFile(const std::string& path)
{
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) {
        return Result<MeshCollisionProfile>::failure(KinematicsStatus::InvalidRequest,
                                                     "mesh collision profile file could not be opened");
    }

    Result<MeshCollisionProfile> loaded = loadJson(QString::fromUtf8(file.readAll()).toStdString());
    if (!loaded.ok()) {
        return loaded;
    }

    const QDir baseDir = QFileInfo(QString::fromStdString(path)).absoluteDir();
    for (MeshCollisionGeometry& mesh : loaded.value.meshes) {
        QFileInfo meshPath(QString::fromStdString(mesh.path));
        if (!meshPath.isAbsolute()) {
            mesh.path = QDir::cleanPath(baseDir.absoluteFilePath(meshPath.filePath())).toStdString();
        }
    }

    return loaded;
}

/// Parses and validates a mesh collision profile encoded as JSON text against the
/// "robot-kinematics-collision-mesh/v1" schema (m/rad units only): populates id, robotModel,
/// backendPreference, and per-mesh geometry (meshToLink pose, scaleToMeters, margin_m,
/// enabled, format, sourceUnits, and quality mode/triangleCount/simplifiedFrom/
/// maxSimplificationError_m), enforcing unique mesh ids; also populates disabledPairs
/// (validated against the parsed mesh ids), sources, and string-valued metadata.
Result<MeshCollisionProfile> MeshCollisionProfileJsonLoader::loadJson(const std::string& json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(json), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return invalid("mesh collision profile JSON is not a valid object");
    }

    const QJsonObject root = document.object();
    if (!hasOnlyKnownTopLevelFields(root)) {
        return invalid("mesh collision profile contains unknown top-level fields");
    }
    if (root.value("schema").toString() != "robot-kinematics-collision-mesh/v1") {
        return invalid("unsupported mesh collision profile schema");
    }

    const QJsonObject profileObject = root.value("profile").toObject();
    const QJsonObject units = profileObject.value("units").toObject();
    if (units.value("length").toString() != "m" || units.value("angle").toString() != "rad") {
        return invalid("mesh collision profile units must be m/rad");
    }

    MeshCollisionProfile profile;
    profile.id = stringField(profileObject, "id");
    profile.robotModel = stringField(profileObject, "robotModel");

    for (const QJsonValue& backendValue : profileObject.value("backendPreference").toArray()) {
        const Result<MeshCollisionBackendKind> backend =
            meshCollisionBackendKindFromString(backendValue.toString().toStdString());
        if (!backend.ok()) {
            return invalid(backend.message);
        }
        profile.backendPreference.push_back(backend.value);
    }

    std::set<std::string> meshIds;
    for (const QJsonValue& meshValue : root.value("meshes").toArray()) {
        const QJsonObject meshObject = meshValue.toObject();

        if (!meshObject.contains("meshToLink")) {
            return invalid("mesh collision geometry must include meshToLink");
        }
        const QJsonObject meshToLinkObject = meshObject.value("meshToLink").toObject();
        if (!hasFiniteNumericArray(meshToLinkObject, "xyz_m") ||
            !hasFiniteNumericArray(meshToLinkObject, "rpy_rad")) {
            return invalid("mesh collision geometry meshToLink must include xyz_m and rpy_rad arrays");
        }
        if (!hasFiniteNumericField(meshObject, "scaleToMeters")) {
            return invalid("mesh collision geometry scaleToMeters must be a finite number");
        }
        if (meshObject.contains("margin_m") && !isFiniteNumber(meshObject.value("margin_m"))) {
            return invalid("mesh collision geometry margin_m must be a finite number");
        }
        if (meshObject.contains("enabled") && !meshObject.value("enabled").isBool()) {
            return invalid("mesh collision geometry enabled must be a boolean");
        }

        MeshCollisionGeometry mesh;
        mesh.id = stringField(meshObject, "id");
        mesh.linkId = stringField(meshObject, "link");
        mesh.path = stringField(meshObject, "path");
        mesh.scaleToMeters = meshObject.value("scaleToMeters").toDouble();
        mesh.meshToLink = poseFromObject(meshToLinkObject);
        mesh.margin_m = meshObject.value("margin_m").toDouble();
        mesh.enabled = !meshObject.contains("enabled") || meshObject.value("enabled").toBool();

        const Result<MeshFileFormat> format =
            parseFormat(meshObject.value("format").toString());
        if (!format.ok()) {
            return invalid(format.message);
        }
        mesh.format = format.value;

        const Result<MeshSourceUnits> sourceUnits =
            parseSourceUnits(meshObject.value("sourceUnits").toString());
        if (!sourceUnits.ok()) {
            return invalid(sourceUnits.message);
        }
        mesh.sourceUnits = sourceUnits.value;

        const QJsonObject qualityObject = meshObject.value("quality").toObject();
        const Result<MeshQualityMode> qualityMode =
            parseQualityMode(qualityObject.value("mode").toString());
        if (!qualityMode.ok()) {
            return invalid(qualityMode.message);
        }
        mesh.quality.mode = qualityMode.value;
        if (qualityObject.contains("triangleCount") && !qualityObject.value("triangleCount").isNull()) {
            const QJsonValue triangleCount = qualityObject.value("triangleCount");
            const double rawCount = triangleCount.toDouble(-1.0);
            if (!triangleCount.isDouble() || !std::isfinite(rawCount) || rawCount < 1.0 ||
                std::floor(rawCount) != rawCount ||
                rawCount > static_cast<double>((std::numeric_limits<std::size_t>::max)())) {
                return invalid("mesh quality triangleCount must be a positive integer when provided");
            }
            mesh.quality.triangleCount = static_cast<std::size_t>(rawCount);
        }
        if (qualityObject.contains("simplifiedFrom") && !qualityObject.value("simplifiedFrom").isNull()) {
            if (!qualityObject.value("simplifiedFrom").isString()) {
                return invalid("mesh quality simplifiedFrom must be a string when provided");
            }
            mesh.quality.simplifiedFrom = qualityObject.value("simplifiedFrom").toString().toStdString();
        }
        if (qualityObject.contains("maxSimplificationError_m") &&
            !qualityObject.value("maxSimplificationError_m").isNull()) {
            if (!isFiniteNumber(qualityObject.value("maxSimplificationError_m"))) {
                return invalid("mesh quality maxSimplificationError_m must be a finite number when provided");
            }
            mesh.quality.maxSimplificationError_m =
                qualityObject.value("maxSimplificationError_m").toDouble();
        }

        if (!meshIds.insert(mesh.id).second) {
            return invalid("mesh collision geometry ids must be unique");
        }

        profile.meshes.push_back(mesh);
    }

    for (const QJsonValue& pairValue : root.value("disabledPairs").toArray()) {
        const QJsonObject pairObject = pairValue.toObject();
        const std::string geometryA = stringField(pairObject, "a");
        const std::string geometryB = stringField(pairObject, "b");

        if (meshIds.find(geometryA) == meshIds.end() || meshIds.find(geometryB) == meshIds.end()) {
            return invalid("mesh collision disabledPairs must refer to existing mesh ids");
        }

        profile.disabledPairs.push_back(DisabledCollisionPair{
            geometryA,
            geometryB,
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

    return Result<MeshCollisionProfile>::success(profile);
}

} // namespace RobotKinematics
