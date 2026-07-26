#pragma once

#include <QObject>

/// Unit test suite for the plain collision-detection data types: CollisionCheckRequest
/// defaults, CollisionCheckResult::ok() semantics, and CollisionProfile/CollisionGeometry
/// field round-trips.
class CollisionApiTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies a default-constructed CollisionCheckRequest has zero safety margin,
    /// returnAllPairs set, and an empty joint vector.
    void requestDefaultsReturnAllPairsWithZeroSafetyMargin();
    /// Verifies CollisionCheckResult::ok() reflects the `status` field only, not the
    /// `hasCollision` flag: a status-Ok result with a collision is still ok(), while an
    /// InvalidRequest status is not, regardless of hasCollision.
    void resultOkDependsOnStatusNotCollisionFlag();
    /// Builds a CollisionProfile with sphere and capsule CollisionGeometry entries, a
    /// disabled pair, a source reference, and metadata, then verifies every field
    /// round-trips through the struct as assigned.
    void profileCanStoreSphereAndCapsuleGeometry();
};
