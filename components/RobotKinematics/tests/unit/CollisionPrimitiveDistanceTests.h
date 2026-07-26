#pragma once

#include <QObject>

/// QtTest suite covering CollisionChecker's primitive-pair distance computation (sphere-sphere,
/// sphere-capsule, capsule-capsule) across separated, touching, overlapping, and safety-margin
/// scenarios.
class CollisionPrimitiveDistanceTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies sphere-sphere distance is correct when separated, drops to zero when touching,
    /// and goes negative (with colliding set) once the spheres overlap.
    void sphereSphereDistancesCoverSeparatedTouchingAndOverlap();
    /// Verifies sphere-capsule distance reports the correct gap when separated, and a negative
    /// penetrating distance with colliding set once the sphere is moved into the capsule.
    void sphereCapsuleDistanceReportsGapAndCollision();
    /// Verifies capsule-capsule gap distance is correct, and that a configured safety margin can
    /// turn an otherwise non-colliding gap into a reported collision.
    void capsuleCapsuleDistanceAndSafetyMarginAreApplied();
};
