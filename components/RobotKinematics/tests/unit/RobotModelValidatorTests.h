#pragma once

#include <QObject>

/// QtTest suite covering RobotModelValidator::validateSerialRobotConfig()'s acceptance of
/// well-formed configs and its detection of missing frames, invalid axes/limits, malformed
/// chains, and unresolvable default tools.
class RobotModelValidatorTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies a fully-populated, well-formed 6-DOF serial config validates as Ok with no
    /// issues.
    void acceptsValidSerialRobotConfig();
    /// Verifies clearing frames.baseLinkId/flangeLinkId is reported as issues on the
    /// "frames.base" and "frames.flange" fields.
    void reportsMissingBaseAndFlange();
    /// Verifies a zero-length joint axis is reported as an issue on the corresponding
    /// "joints[i].axis" field.
    void reportsInvalidJointAxis();
    /// Verifies inverted lower/upper limits, a negative velocity/acceleration limit, and an
    /// out-of-range home position are reported as issues on their respective
    /// "joints[i].limits*" fields.
    void reportsInvalidLimitsAndHome();
    /// Verifies a joint whose parent link breaks the expected serial parent/child chain is
    /// reported as an issue on "joints[i].parent".
    void reportsMalformedSerialChain();
    /// Verifies an unresolvable defaultToolId is reported with KinematicsStatus::ToolNotFound
    /// on the "defaultTool" field.
    void reportsMissingDefaultTool();
    /// Verifies a Fixed joint appended past the configured dof (e.g. a fixed TCP mount beyond
    /// the flange) does not trigger a validation failure.
    void acceptsFixedJointBeyondConfiguredDof();
};
