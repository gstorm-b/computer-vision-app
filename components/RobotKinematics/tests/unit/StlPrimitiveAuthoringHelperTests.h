#pragma once

#include <QObject>

/// QtTest suite for StlPrimitiveAuthoringHelper: verifies that proposeFromFile derives
/// conservative bounding-sphere/capsule collision primitives (with correct ids, link id,
/// and margin) from ASCII and binary STL meshes, and rejects invalid payloads.
class StlPrimitiveAuthoringHelperTests : public QObject
{
    Q_OBJECT

private slots:
    /// Proposes primitives from an ASCII STL cuboid using an explicit request (profile,
    /// robot, geometry, and link ids plus a margin) and verifies the sphere/capsule
    /// proposals, ids, link id, and margin on the result.
    void proposesConservativePrimitivesFromAsciiStl();
    /// Proposes primitives from a binary STL cuboid using default request options and
    /// verifies the same conservative sphere/capsule proposal statistics.
    void proposesConservativePrimitivesFromBinaryStl();
    /// Feeds a non-STL payload to proposeFromFile and expects failure with
    /// KinematicsStatus::InvalidRequest.
    void rejectsInvalidStlPayload();
};

/// Entry point invoked by TestMain to run the StlPrimitiveAuthoringHelperTests suite under QtTest.
/// @return the number of failing test functions (0 on success)
int runStlPrimitiveAuthoringHelperTests(int argc, char** argv);
