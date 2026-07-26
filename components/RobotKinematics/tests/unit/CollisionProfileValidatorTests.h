#pragma once

#include <QObject>

/// QtTest suite covering CollisionProfileValidator's structural checks: unique geometry ids,
/// geometries referencing known links, positive primitive dimensions, and well-formed
/// disabled-pair references.
class CollisionProfileValidatorTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies a well-formed CollisionProfile (unique ids, valid links, positive dimensions,
    /// valid disabled pair) validates as ok() with no issues.
    void validProfilePasses();
    /// Verifies a profile with two geometries sharing the same id is rejected with an
    /// InvalidRobotConfig status and a message mentioning "unique".
    void duplicateGeometryIdsAreRejected();
    /// Verifies a geometry referencing a link id absent from the robot model is rejected with
    /// an InvalidRobotConfig status and an issue field mentioning ".linkId".
    void missingLinkIdsAreRejected();
    /// Verifies zero/negative sphere radius and zero/negative capsule radius or length are all
    /// rejected with a message mentioning "positive".
    void nonPositivePrimitiveDimensionsAreRejected();
    /// Verifies a disabled pair referencing an unknown geometry id is rejected with an
    /// InvalidRobotConfig status and an issue field mentioning "disabledPairs".
    void invalidDisabledPairsAreRejected();
};
