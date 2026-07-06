#ifndef ROBOT_KINEMATIC_CHECK_WIDGET_H
#define ROBOT_KINEMATIC_CHECK_WIDGET_H

#include <QWidget>

#include "device/output_device/vision_output_config.h"

class QComboBox;

namespace Ui {
class RobotKinematicCheckWidget;
}

// ---------------------------------------------------------------------------
// RobotKinematicCheckWidget — reusable editor for a RobotKinematicCheckConfig.
//
// Lets the operator enable/disable the robot kinematic reachability check,
// optionally enable the mesh self-collision check, pick a built-in robot preset
// (Nachi MZ04D; no custom-preset authoring yet), and set the TCP (flange -> tool
// point) offset. Model-free: the owner feeds a config via setConfig() and reacts
// to edits through configChanged(). Structure lives in
// robot_kinematic_check_widget.ui; this class only wires behaviour. Embedded by
// the vision-output device widgets. Backed by the RobotKinematics component.
// ---------------------------------------------------------------------------
class RobotKinematicCheckWidget : public QWidget {
    Q_OBJECT

public:
    explicit RobotKinematicCheckWidget(QWidget *parent = nullptr);
    ~RobotKinematicCheckWidget() override;

    void setConfig(const vc::device::RobotKinematicCheckConfig &cfg);
    vc::device::RobotKinematicCheckConfig config() const;

signals:
    void configChanged();
    void testerWidgetVisibleChanged();

private slots:
    // FK / IK tester (independent of the saved config): build a robot config from
    // the currently selected preset + TCP, then run forward / inverse kinematics
    // on the tester spin boxes for verification.
    void onComputeForward();
    void onComputeInverse();

    // Pick-path table editing.
    void onAddPathRow();
    void onRemovePathRow();
    // Confirms before switching preset (clears the pick-path; posture labels are
    // preset-specific). Reverts the combo if the user declines.
    void onPresetChanged(const QString &preset);

private:
    void notifyConfigChanged();   // emit configChanged() unless m_loading
    void addPathRow(const vc::device::PickPathPoint &point);
    void populatePostureCombo(QComboBox *combo, const QString &axis, const QString &current);
    void renumberPathRows();

    Ui::RobotKinematicCheckWidget *ui;
    bool m_loading{false};       // suppress configChanged while setConfig() runs
    QString m_currentPreset;     // last accepted preset (for change-confirm revert)
};

#endif // ROBOT_KINEMATIC_CHECK_WIDGET_H
