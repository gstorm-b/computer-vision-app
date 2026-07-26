#pragma once

#include <QObject>

/// Unit test suite for CollisionChecker: verifies that primitive collision checks
/// transform geometry into the base frame with deterministic pair ordering, respect
/// disabled geometries/pairs, validate joint-vector dimension, and support early exit
/// once the first collision is found.
class CollisionCheckerTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies CollisionChecker::check() transforms each geometry into the base frame,
    /// returns pairs in a fixed index order with the correct link ids, and computes the
    /// analytic sphere-to-sphere separation distance.
    void transformsGeometryIntoBaseAndKeepsDeterministicPairOrder();
    /// Verifies CollisionChecker::check() omits pairs involving a geometry with
    /// `enabled = false` and pairs matching an explicit disabledPairs entry.
    void skipsDisabledGeometryAndDisabledPairs();
    /// Verifies CollisionChecker::check() reports JointDimensionMismatch, with a message
    /// mentioning "joint", when the request's joint vector does not match the robot's DOF.
    void invalidJointDimensionReturnsStructuredFailure();
    /// Verifies that with returnAllPairs set to false, CollisionChecker::check() stops at
    /// the first colliding pair it finds and returns only that single pair.
    void stopsAfterFirstCollisionWhenReturnAllPairsIsFalse();
};
