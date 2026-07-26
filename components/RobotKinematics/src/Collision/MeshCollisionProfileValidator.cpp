#include <RobotKinematics/Collision/MeshCollisionProfileValidator.h>

#include <set>

namespace RobotKinematics {

namespace {
/// Appends a MeshCollisionProfileValidationIssue built from `status`, `field`, and `message`
/// to `result.issues`.
void addIssue(MeshCollisionProfileValidationResult& result,
              const std::string& field,
              const std::string& message,
              KinematicsStatus status = KinematicsStatus::InvalidRobotConfig)
{
    result.issues.push_back(MeshCollisionProfileValidationIssue{status, field, message});
}

/// Returns true if `linkId` is non-empty and present in `linkIds`.
bool hasLinkId(const std::set<std::string>& linkIds, const std::string& linkId)
{
    return !linkId.empty() && linkIds.find(linkId) != linkIds.end();
}
}

/// Returns true when no validation issues were recorded.
bool MeshCollisionProfileValidationResult::ok() const
{
    return issues.empty();
}

/// Returns KinematicsStatus::Ok when there are no issues, otherwise the status of the first
/// recorded issue.
KinematicsStatus MeshCollisionProfileValidationResult::status() const
{
    return ok() ? KinematicsStatus::Ok : issues.front().status;
}

/// Validates `profile` against `config`: checks that profile.id and profile.robotModel are
/// non-empty; that each mesh entry has a unique non-empty id, references a link id declared
/// in `config`, has a non-empty path, a positive scaleToMeters, a non-negative margin_m, a
/// positive quality.triangleCount when provided, a non-negative quality.maxSimplificationError_m
/// when provided, and (when quality.mode is Simplified) a non-empty quality.simplifiedFrom;
/// and that each disabledPairs entry refers to mesh ids that exist in the profile. All
/// violations are collected rather than stopping at the first one found.
MeshCollisionProfileValidationResult MeshCollisionProfileValidator::validate(
    const SerialRobotConfig& config,
    const MeshCollisionProfile& profile)
{
    MeshCollisionProfileValidationResult result;

    if (profile.id.empty()) {
        addIssue(result, "profile.id", "mesh collision profile id must not be empty");
    }
    if (profile.robotModel.empty()) {
        addIssue(result, "profile.robotModel", "mesh collision profile robot model must not be empty");
    }

    std::set<std::string> linkIds;
    for (const Link& link : config.links) {
        if (!link.id.empty()) {
            linkIds.insert(link.id);
        }
    }

    std::set<std::string> meshIds;
    for (std::size_t i = 0; i < profile.meshes.size(); ++i) {
        const MeshCollisionGeometry& mesh = profile.meshes[i];
        const std::string prefix = "meshes[" + std::to_string(i) + "]";

        if (mesh.id.empty()) {
            addIssue(result, prefix + ".id", "mesh collision geometry id must not be empty");
        } else if (!meshIds.insert(mesh.id).second) {
            addIssue(result, prefix + ".id", "mesh collision geometry id must be unique");
        }

        if (!hasLinkId(linkIds, mesh.linkId)) {
            addIssue(result, prefix + ".linkId", "mesh collision geometry link id must refer to a declared link");
        }
        if (mesh.path.empty()) {
            addIssue(result, prefix + ".path", "mesh collision geometry path must not be empty");
        }
        if (mesh.scaleToMeters <= 0.0) {
            addIssue(result, prefix + ".scaleToMeters", "mesh collision geometry scaleToMeters must be positive");
        }
        if (mesh.margin_m < 0.0) {
            addIssue(result, prefix + ".margin_m", "mesh collision geometry margin must be non-negative");
        }
        if (mesh.quality.triangleCount.has_value() && *mesh.quality.triangleCount == 0) {
            addIssue(result, prefix + ".quality.triangleCount", "mesh collision geometry triangleCount must be positive when provided");
        }
        if (mesh.quality.maxSimplificationError_m.has_value() &&
            *mesh.quality.maxSimplificationError_m < 0.0) {
            addIssue(result,
                     prefix + ".quality.maxSimplificationError_m",
                     "mesh collision geometry maxSimplificationError_m must be non-negative when provided");
        }
        if (mesh.quality.mode == MeshQualityMode::Simplified &&
            (!mesh.quality.simplifiedFrom.has_value() || mesh.quality.simplifiedFrom->empty())) {
            addIssue(result,
                     prefix + ".quality.simplifiedFrom",
                     "simplified mesh collision geometry must record simplifiedFrom");
        }
    }

    for (std::size_t i = 0; i < profile.disabledPairs.size(); ++i) {
        const DisabledCollisionPair& pair = profile.disabledPairs[i];
        const std::string prefix = "disabledPairs[" + std::to_string(i) + "]";

        if (pair.geometryA.empty() || meshIds.find(pair.geometryA) == meshIds.end()) {
            addIssue(result, prefix + ".geometryA",
                     "disabled mesh collision pair geometryA must refer to an existing mesh id");
        }
        if (pair.geometryB.empty() || meshIds.find(pair.geometryB) == meshIds.end()) {
            addIssue(result, prefix + ".geometryB",
                     "disabled mesh collision pair geometryB must refer to an existing mesh id");
        }
    }

    return result;
}

} // namespace RobotKinematics
