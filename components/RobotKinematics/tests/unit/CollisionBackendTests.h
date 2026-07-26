#pragma once

#include <QObject>

/// Unit test suite for CollisionBackends: verifies reported backend capabilities
/// (primitive vs. mesh, with/without a compiled mesh adapter), that primitive checks
/// flow through the backend facade, and — when the Coal mesh backend is compiled in —
/// that mesh checks detect synthetic overlap/separation and honor a safety margin.
class CollisionBackendTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies CollisionBackends::primitiveInfo() reports the Primitive kind, the
    /// "primitive_sphere_capsule" name, availability, distance support, and no mesh or
    /// contact support.
    void primitiveBackendReportsBuiltInCapabilities();
    /// Verifies CollisionBackends::meshInfo() reports Coal-backed capabilities when
    /// ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND is compiled in, or an unavailable/
    /// no-mesh-support info with an explanatory detail message otherwise.
    void meshBackendReportsUnavailableWithoutCompiledAdapter();
    /// Verifies CollisionBackends::checkPrimitive() returns an Ok, collision-free result
    /// for the Virtual6DofTestArm preset and its built-in collision profile at the home
    /// pose.
    void primitiveChecksFlowThroughBackendFacade();
    /// Verifies CollisionBackends::checkMesh() reports UnsupportedSolver with a message
    /// naming the unavailable backend (fcl/coal) when no mesh backend is compiled in, or
    /// selects the compiled Coal backend and rejects an Fcl preference when it is.
    void meshChecksReturnStructuredUnsupportedStatus();
    /// (Coal-only, QSKIPs otherwise) Writes a synthetic cuboid STL to a temp file, then
    /// verifies checkMesh() detects overlap at the home pose, no collision after
    /// rotating clear, and zero pairs once the pair is added to disabledPairs.
    void meshChecksDetectSyntheticOverlapAndSeparationWhenCompiled();
    /// (Coal-only, QSKIPs otherwise) Verifies that raising CollisionCheckRequest's
    /// safetyMargin_m past a known clearance distance turns a non-colliding mesh check
    /// into a colliding one.
    void meshChecksApplySafetyMarginWhenCompiled();
};
