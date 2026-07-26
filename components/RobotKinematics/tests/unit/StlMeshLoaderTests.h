#pragma once

#include <QObject>

/// QtTest suite for StlMeshLoader: covers ASCII/binary STL parsing, millimeter-to-meter
/// normalization, and rejection of degenerate triangles or malformed input.
class StlMeshLoaderTests : public QObject
{
    Q_OBJECT

private slots:
    /// Loads a hand-built ASCII STL cuboid with a 0.001 scale factor and checks that the
    /// resulting mesh statistics (triangle/vertex counts, bounds, scale) are in meters.
    void loadsAsciiStlAndNormalizesMillimetersToMeters();
    /// Loads a hand-built binary STL cuboid already expressed in meters with scale 1.0 and
    /// checks that triangle count and bounds pass through unchanged.
    void loadsBinaryStlAndPreservesMeterScale();
    /// Feeds an ASCII STL containing a single collinear (zero-area) triangle and expects
    /// loadFile to fail with KinematicsStatus::InvalidRequest and a "degenerate triangle" message.
    void rejectsDegenerateTriangles();
    /// Loads an ASCII STL mixing one degenerate and one valid triangle: expects rejection
    /// when skipping is disabled, and a mesh containing only the valid triangle when
    /// StlMeshLoadOptions requests degenerate triangles to be skipped.
    void filtersDegenerateTrianglesWhenSkipModeIsRequested();
    /// Loads an all-degenerate ASCII STL with skip mode enabled and expects failure because
    /// no triangles survive filtering, reported via a "no triangles" message.
    void rejectsAllDegenerateAsciiStlWhenSkipModeLeavesNoTriangles();
    /// Verifies that a non-STL payload is rejected regardless of scale, and that a
    /// non-positive scale factor is also rejected, both with KinematicsStatus::InvalidRequest.
    void rejectsInvalidPayloadAndNonPositiveScale();
};
