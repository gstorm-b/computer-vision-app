#pragma once

#include <QObject>

/// QtTest suite validating CollisionProfileJsonLoader: rejects malformed/non-canonical JSON
/// profiles and, for the virtual and Nachi MZ04D presets, checks that a profile loaded from JSON
/// matches its C++ built-in fallback and produces the expected collision/no-collision result at
/// representative joint states.
class CollisionProfileJsonTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies loadJson() fails with KinematicsStatus::InvalidRobotConfig both for a wrong
    /// "schema" value and for non-canonical length/angle units (mm/deg instead of m/rad).
    void loaderRejectsInvalidSchemaAndUnits();
    /// Loads the virtual 6-DOF test arm's collision profile JSON, checks it matches
    /// CollisionProfiles::virtual6DofTestArm() field-for-field, validates it against the robot
    /// config, and confirms a home-pose joint state is collision-free while a folded-arm state
    /// (J2 rotated by pi) reports a collision.
    void virtualProfileJsonMatchesCppFallbackAndRepresentativeStates();
    /// Same as virtualProfileJsonMatchesCppFallbackAndRepresentativeStates() but for the Nachi
    /// MZ04D preset, using its own safe and colliding joint-angle fixtures (in degrees).
    void nachiProfileJsonMatchesCppFallbackAndRepresentativeStates();
};
