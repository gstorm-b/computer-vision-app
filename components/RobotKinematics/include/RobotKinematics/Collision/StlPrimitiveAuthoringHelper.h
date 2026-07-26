#pragma once

#include <RobotKinematics/Collision/CollisionProfile.h>
#include <RobotKinematics/Collision/StlMeshLoader.h>
#include <RobotKinematics/Core/Result.h>

#include <array>
#include <cstddef>
#include <string>

/// Root namespace for the RobotKinematics library (poses, joint vectors, kinematics
/// solvers, and collision geometry).
namespace RobotKinematics {

/// Bounding-box, centroid, and per-axis extent statistics computed from a loaded STL
/// mesh; used to size and place the sphere/capsule proposals.
struct StlMeshStatistics {
    std::size_t triangleCount = 0;   ///< Number of triangles in the source mesh.
    std::array<double, 3> minimumBounds_m = {0.0, 0.0, 0.0};  ///< Per-axis minimum vertex coordinate, in meters.
    std::array<double, 3> maximumBounds_m = {0.0, 0.0, 0.0};  ///< Per-axis maximum vertex coordinate, in meters.
    std::array<double, 3> centroid_m = {0.0, 0.0, 0.0};  ///< Arithmetic mean of all mesh vertices, in meters.
    std::array<double, 3> axisLengths_m = {0.0, 0.0, 0.0};  ///< Per-axis bounding-box extent (maximumBounds_m - minimumBounds_m), in meters.
};

/// Identifiers and defaults used to name and configure a draft collision-geometry
/// proposal generated from an STL file.
struct StlPrimitiveProposalRequest {
    std::string profileId = "draft_collision_profile";  ///< Collision profile id written into the draft JSON.
    std::string robotModel = "draft_robot";  ///< Robot model name written into the draft JSON.
    std::string geometryId = "draft_geometry";  ///< Base geometry id; "_sphere"/"_capsule" is appended per proposal.
    std::string linkId = "draft_link";  ///< Link id the proposed geometry is attached to.
    double margin_m = 0.0;  ///< Safety margin applied to the proposed geometry, in meters; must be non-negative.
    bool enabled = true;  ///< Whether the proposed geometry is marked enabled in the draft JSON.
};

/// Result of StlPrimitiveAuthoringHelper::proposeFromFile: computed mesh statistics
/// plus a sphere and a capsule primitive proposal (as both CollisionGeometry values and
/// draft JSON snippets) that a human must review before use.
struct StlPrimitiveProposal {
    StlFileFormat format = StlFileFormat::Ascii;  ///< Detected source STL format (ASCII or binary).
    StlMeshStatistics statistics;  ///< Bounds/centroid/axis-length statistics computed from the mesh.
    CollisionGeometry sphere;  ///< Proposed bounding-sphere collision geometry.
    CollisionGeometry capsule;  ///< Proposed bounding-capsule collision geometry, aligned to the mesh's longest axis.
    std::string sphereDraftJson;  ///< Draft collision-profile JSON snippet embedding the sphere proposal.
    std::string capsuleDraftJson;  ///< Draft collision-profile JSON snippet embedding the capsule proposal.
    std::string reviewNote;  ///< Fixed marker ("draft_manual_review_required") flagging the proposal as unreviewed.
};

/// Authoring helper only: parses simple ASCII/binary STL, computes conservative
/// primitive proposals, and emits draft JSON snippets that must be reviewed by a
/// human before becoming runtime collision profiles.
class StlPrimitiveAuthoringHelper
{
public:
    /// Loads `path` as an STL mesh and derives conservative bounding-sphere and
    /// bounding-capsule collision-geometry proposals from its bounds/centroid, along
    /// with ready-to-review draft JSON for each.
    /// @param path filesystem path to the .stl file to read
    /// @param request naming/margin/enabled defaults to bake into the proposals
    /// @return success with the StlPrimitiveProposal, or failure with InvalidRequest if
    /// the margin is negative or the underlying STL load fails
    static Result<StlPrimitiveProposal> proposeFromFile(const std::string& path,
                                                        const StlPrimitiveProposalRequest& request = {});
};

} // namespace RobotKinematics
