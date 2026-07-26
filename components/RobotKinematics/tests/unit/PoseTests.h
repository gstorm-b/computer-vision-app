#pragma once

#include <QObject>

/// QtTest suite covering RobotKinematics::Pose construction, unit conversion, and
/// composition/inversion algebra.
class PoseTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies Pose::identity() has zero translation and an identity rotation.
    void identityHasNoTranslationOrRotation();
    /// Verifies Pose::fromIsometry() preserves the source Eigen::Isometry3d's translation
    /// and rotation exactly.
    void fromIsometryPreservesTransform();
    /// Verifies fromXYZRPY_m_rad() applies roll/pitch/yaw as fixed-axis (extrinsic) rotations
    /// in Z*Y*X composition order.
    void fromXYZRPY_m_radUsesFixedAxisRpyConvention();
    /// Verifies fromXYZRPY_mm_deg() correctly converts millimeter/degree inputs to the
    /// meter/radian internal representation.
    void fromXYZRPY_mm_degConvertsExplicitUnits();
    /// Verifies pose composition (operator*), inversion, and that composed rotations remain
    /// unit quaternions.
    void composesTransforms();
};
