#ifndef ROBOT_KINEMATIC_CHECK_WIDGET_H
#define ROBOT_KINEMATIC_CHECK_WIDGET_H

#include <QWidget>

#include "device/output_device/vision_output_config.h"

class QComboBox;

namespace Ui {
class RobotKinematicCheckWidget;
}

/// Reusable editor widget for a RobotKinematicCheckConfig.
///
/// Lets the operator enable/disable the robot kinematic reachability check,
/// optionally enable the mesh self-collision check, pick a built-in robot preset
/// (Nachi MZ04D; no custom-preset authoring yet), and set the TCP (flange -> tool
/// point) offset. Model-free: the owner feeds a config via setConfig() and reacts
/// to edits through configChanged(). Structure lives in
/// robot_kinematic_check_widget.ui; this class only wires behaviour. Embedded by
/// the vision-output device widgets. Backed by the RobotKinematics component.
class RobotKinematicCheckWidget : public QWidget {
    Q_OBJECT

public:
    /// Constructs the widget and loads its .ui layout.
    /// @param parent optional owning widget
    explicit RobotKinematicCheckWidget(QWidget *parent = nullptr);
    /// Destroys the widget and releases its `ui` layout instance.
    ~RobotKinematicCheckWidget() override;

    /// Populates the widget's controls (enable toggle, collision toggle, preset,
    /// TCP fields, pick-path table) from `cfg`, suppressing configChanged() while
    /// loading.
    /// @param cfg the config to display/edit
    void setConfig(const vc::device::RobotKinematicCheckConfig &cfg);
    /// Reads the widget's current controls back into a RobotKinematicCheckConfig
    /// (preset, TCP offset, collision toggle, pick path).
    /// @return the config reflecting the widget's current state
    vc::device::RobotKinematicCheckConfig config() const;

signals:
    /// Emitted whenever an edited control changes the resulting config, unless
    /// suppressed by an in-progress setConfig() load.
    void configChanged();
    /// Emitted when the FK/IK tester sub-widget's visibility is toggled.
    void testerWidgetVisibleChanged();

private slots:
    /// Builds a robot config from the currently selected preset + TCP and runs
    /// forward kinematics on the tester spin boxes, independent of the saved
    /// config (verification only).
    void onComputeForward();
    /// Builds a robot config from the currently selected preset + TCP and runs
    /// inverse kinematics on the tester spin boxes, independent of the saved
    /// config (verification only).
    void onComputeInverse();

    /// Appends a new row to the pick-path table.
    void onAddPathRow();
    /// Removes the currently selected row from the pick-path table.
    void onRemovePathRow();
    /// Confirms before switching preset, since posture labels are preset-specific
    /// and switching clears the pick-path; reverts the combo box selection if the
    /// user declines.
    /// @param preset the newly selected preset name
    void onPresetChanged(const QString &preset);

private:
    /// Emits configChanged(), unless suppressed by an in-progress setConfig()
    /// load (m_loading).
    void notifyConfigChanged();
    /// Appends a pick-path table row prefilled from `point`.
    /// @param point the path point (offset + posture labels) to display in the new row
    void addPathRow(const vc::device::PickPathPoint &point);
    /// Fills `combo` with the posture labels the current preset defines for
    /// `axis`, selecting `current` if it is among them.
    /// @param combo the combo box to populate
    /// @param axis posture axis name (e.g. "shoulder", "elbow", "wrist")
    /// @param current label to preselect, if present among the preset's labels
    void populatePostureCombo(QComboBox *combo, const QString &axis, const QString &current);
    /// Renumbers the pick-path table rows after an add/remove so they stay
    /// sequential.
    void renumberPathRows();

    Ui::RobotKinematicCheckWidget *ui;   ///< Generated .ui layout; owned by this widget.
    bool m_loading{false};       ///< Suppresses configChanged() while setConfig() runs.
    QString m_currentPreset;     ///< Last accepted preset (used to revert the combo on change-confirm decline).
};

#endif // ROBOT_KINEMATIC_CHECK_WIDGET_H
