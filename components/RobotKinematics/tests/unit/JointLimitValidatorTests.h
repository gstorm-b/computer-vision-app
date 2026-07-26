#pragma once

#include <QObject>

/// QtTest suite covering JointVector unit conversion and JointLimitValidator's acceptance/
/// rejection of joint configurations against a synthetic three-joint revolute chain.
class JointLimitValidatorTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies JointVector::fromRadians/fromDegrees produce equivalent radian values.
    void jointVectorConvertsRadiansAndDegrees();
    /// Verifies a joint vector within all configured limits validates as Ok with no violations.
    void acceptsJointsInsideLimits();
    /// Verifies a joint below its configured minimum is reported as a JointLimitViolation
    /// identifying the correct joint index/id.
    void rejectsJointBelowMinimum();
    /// Verifies a joint above its configured maximum is reported as a JointLimitViolation
    /// identifying the correct joint index.
    void rejectsJointAboveMaximum();
    /// Verifies a joint vector with too few elements is reported as JointDimensionMismatch.
    void rejectsWrongDimension();
};
