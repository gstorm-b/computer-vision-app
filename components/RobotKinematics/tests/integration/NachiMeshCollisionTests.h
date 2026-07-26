#pragma once

#include <QObject>

/// QtTest suite validating the Nachi MZ04D STL mesh-collision profile: loads and validates it
/// against the Nachi preset, checks every referenced STL loads at millimeter scale, verifies
/// mesh-to-link placement reproduces the 3D visualizer's home pose, and (when the Coal mesh
/// backend is compiled in) checks self-collision detection at the home pose and at a known
/// folded self-collision pose.
class NachiMeshCollisionTests : public QObject
{
    Q_OBJECT

private slots:
    /// Loads the on-disk Nachi mesh-collision profile JSON and checks its id, robot model, mesh
    /// count, disabled-pair count, and backend preference (Coal first), then validates it
    /// against Presets::nachiMZ04D() with MeshCollisionProfileValidator.
    void profileLoadsAndValidatesAgainstNachiPreset();
    /// For every mesh in the profile, checks its STL file exists on disk, loads via
    /// StlMeshLoader with degenerate-triangle skipping enabled, has at least one triangle, is
    /// scaled to meters (scaleToMeters == 0.001), and has a bounding-box extent under 1 m on
    /// each axis (sanity check for an industrial-arm part authored in mm).
    void everyStlMeshLoadsWithMillimeterScale();
    /// Computes the FK chain at the home joint state, composes each mesh's link pose with its
    /// meshToLink transform, and checks the resulting base-frame pose matches the fixed
    /// reference poses used by Robot3DVizualize::visualHomeCorrectionForPartKey (within 1e-9).
    void meshToLinkTransformsReproduceVisualizerHomePlacement();
    /// Skipped unless ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND is defined. Runs
    /// CollisionBackends::checkMesh() at the home joint state and checks it reports no
    /// self-collision, guarding against wrong mesh-to-link transforms or disabled pairs.
    void meshBackendDetectsHomePoseHasNoCollisionWhenCompiled();
    /// Skipped unless ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND is defined. Runs
    /// CollisionBackends::checkMesh() at an aggressively folded joint state (zero safety margin)
    /// and checks it reports a self-collision between the base mesh and the folded J2/J3 meshes.
    void meshBackendDetectsKnownSelfCollisionPoseWhenCompiled();
};

/// QtTest entry point for NachiMeshCollisionTests: constructs the suite and runs it via
/// QTest::qExec.
/// @return the number of failing test functions (0 = all passed)
int runNachiMeshCollisionTests(int argc, char** argv);
