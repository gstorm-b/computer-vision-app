#ifndef EDIT_PATTERN_WIZARD_H
#define EDIT_PATTERN_WIZARD_H

#include <QDialog>
#include <QStringList>
#include <QString>
#include <QPoint>
#include <QRect>
#include <opencv2/core.hpp>

#include "model/gripper_preset_store.h"

class QStackedWidget;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;

class AddPatternImageCanvas;

/**
 * @file edit_pattern_wizard.h
 * @brief EditPatternWizard — 5-step modal dialog for editing an already-learned pattern.
 */

/**
 * @class EditPatternWizard
 * @brief 5-step modal dialog reimplementation of `EditPatternWizard.jsx` for editing an
 *        already-learned pattern.
 *
 * The learned image is LOCKED (cannot be re-captured); the user can only edit identity,
 * pick point and angle, picking-box config, and the 6-axis pick offset.
 *
 * Steps:
 *   1. Identity     - locked image (purple scrim) + editable name/number
 *   2. Pick Point   - same canvas; pre-filled from saved pickX/pickY + picking angle
 *   3. Picking Box  - same as Add wizard; pre-filled from saved box config, preset picker
 *   4. Offset       - 6-axis pick offset (X/Y/Z mm, RX/RY/RZ deg, TOOL frame)
 *   5. Finish       - diff view: changed fields show "old -> new"
 *
 * Lock color: #6a5acd (purple), per design tokens.
 *
 * @note Enter and Escape are deliberately inert, matching AddPatternWizard: a stray
 *       keystroke must not discard or prematurely commit an edit in progress.
 */
class EditPatternWizard : public QDialog {
    Q_OBJECT
public:
    /**
     * @struct Pattern
     * @brief Editable snapshot of a single pattern: identity (name/number), pick point,
     *        picking-box geometry, and the already-learned template image.
     */
    struct Pattern {
        QString name;             ///< Pattern display name.
        int     number{0};        ///< Pattern number; must be unique within its group.
        int     pickX{0}, pickY{0};              ///< Pick point, in locked-image pixel coordinates.
        double  pickAngle{0};     ///< Picking angle (degrees) — MatchPatternConfig::m_angle, added to every reported match angle.
        double  pickBoxW{120}, pickBoxH{80};      ///< Picking-box width/height, in image pixels.
        double  pickBoxDist{90}, pickBoxAngle{0}; ///< Picking-box offset distance (px) and jaw rotation (degrees), relative to the pick point. Unrelated to pickAngle.
        double  offX{0}, offY{0}, offZ{0};        ///< Pick offset XYZ, in millimetres.
        double  offRX{0}, offRY{0}, offRZ{0};     ///< Pick rotation offset RX/RY/RZ, in degrees, applied in the TOOL frame.
        cv::Mat image;          ///< Already-learned template image; locked (cannot be re-captured by this wizard).
    };

    /**
     * @brief Constructs the wizard pre-filled with @p pattern's current values; step 1
     *        (Identity) is shown first via goToStep(0) in the .cpp constructor body.
     * @param[in] groupName name of the pattern's group, shown in the header subtitle
     * @param[in] pattern current (pre-edit) values, copied into both m_old and m_new
     * @param[in] usedNames names already used by other patterns in the group, for uniqueness validation
     * @param[in] usedNumbers numbers already used by other patterns in the group, for uniqueness validation
     * @param[in] parent parent widget
     */
    explicit EditPatternWizard(const QString &groupName,
                               const Pattern &pattern,
                               const QStringList &usedNames,
                               const QList<int>  &usedNumbers,
                               QWidget *parent = nullptr);
    ~EditPatternWizard() override = default;

    /// Returns the edited pattern values (m_new). Only meaningful after the dialog
    /// has been accepted, i.e. after "Apply Changes" was clicked on the Finish step.
    Pattern result() const { return m_new; }

    /**
     * @brief Offers `presets` as ready-made picking-box geometries on the box step.
     *
     * Call before exec(). Mirrors AddPatternWizard::setGripperPresets(): choosing a preset
     * copies its jaw size and distance into the spin boxes, editing any of them switches
     * the selector back to "Custom", and the wizard always returns whatever the spin boxes
     * hold. The selector starts on "Custom" because an existing pattern holds resolved
     * values with no record of which preset (if any) they came from.
     * @param[in] presets the task's registered gripper geometries
     */
    void setGripperPresets(const vc::model::GripperPresetStore &presets);

private slots:
    /// Advances to the next step, unless already on the last step.
    void onNext();
    /// Returns to the previous step.
    void onBack();
    /// Rejects the dialog, discarding all edits.
    void onCancel();
    /// Accepts the dialog, committing m_new as the final result().
    void onApply();

    /// Updates m_new.name from the name field, refreshes the duplicate-name error label, and updates the footer status.
    void onNameChanged(const QString &v);
    /// Updates m_new.number from the number field, refreshes the duplicate-number error label, and updates the footer status.
    void onNumberChanged(int v);
    /// Updates m_new pick coordinates from `p`, syncs the pick spin boxes and pick canvas (signals blocked to avoid feedback loops), and updates the footer status.
    void onPickChanged(const QPoint &p);
    /// Sets the pick point to the center of the locked image (or of the CW x CH default canvas size if there is no image).
    void onPickCenter();
    /// Reads the current values from the box/offset spin boxes into m_new, pushes them to the box canvas, and updates the footer status.
    void onBoxChanged();
    /// Resets the picking-box geometry to the default jaw dimensions (120x80, d=90, angle=0), mirroring the Add wizard, then applies via onBoxChanged().
    void onBoxReset();
    /// Rotates the picking-box angle by +90 degrees, wrapping into the [-180, 180] range, then applies via onBoxChanged().
    void onBoxRotate90();
    /// Reads the six offset spin boxes into m_new and updates the footer status.
    void onOffsetChanged();
    /// Zeroes all six offset axes and applies via onOffsetChanged().
    void onOffsetReset();

protected:
    /// Swallows Return/Enter and Escape so neither closes the dialog; every other key is
    /// forwarded to QDialog. See the class note — a stray keystroke must not discard an
    /// edit in progress.
    void keyPressEvent(QKeyEvent *event) override;

private:
    /// Assembles the dialog layout: header, step rail, stacked step pages, and footer.
    void buildUi();
    /// Builds the title/subtitle header bar with its close button.
    QWidget *buildHeader();
    /// Builds the horizontal step rail showing every step with numbered bubbles and labels.
    QWidget *buildStepRail();
    /// Returns the gripper-preset selector to "Custom" after a manual geometry edit.
    void markGripperPresetCustom();
    /// Builds the footer bar containing the status label and Cancel/Back/Next buttons.
    QWidget *buildFooter();
    /// Builds the Identity step page: locked image canvas plus editable name/number fields.
    QWidget *buildStepIdentity();
    /// Builds the Pick Point step page: locked image canvas in pick mode plus X/Y and picking-angle spin boxes and a Center button.
    QWidget *buildStepPick();
    /// Builds the Picking Box step page: locked image canvas in box mode plus the preset selector, size/offset spin boxes and Reset/Rotate buttons.
    QWidget *buildStepBox();
    /// Builds the Offset step page: the six pick-offset axes (X/Y/Z in mm, RX/RY/RZ in degrees, TOOL frame).
    QWidget *buildStepOffset();
    /// Builds the Finish step page: locked image canvas in finish mode plus the old-vs-new diff summary.
    QWidget *buildStepFinish();

    /// Creates one numbered step-rail bubble label (1-based display) styled as active if `idx == 0`.
    QLabel *makeStepBubble(int idx);
    /// Refreshes all step-rail bubble/label styles (done/current/pending) to match m_currentStep.
    void updateStepRail();
    /// Refreshes the footer status text to reflect validation state or current values of the active step.
    void updateFooterStatus();
    /// Rebuilds the Finish-step diff HTML by comparing every m_old/m_new field and highlighting changed ones.
    void refreshDiff();
    /// Switches the wizard to `step`: validates the current step before advancing, syncs step-specific canvases/fields, updates the step rail, footer, and Next/Back buttons.
    void goToStep(int step);
    /// Returns whether the current step's inputs are valid enough to advance; only step 0 (Identity) enforces non-empty/unique name and number, other steps always return true.
    bool currentStepValid() const;

private:
    QString     m_groupName;   ///< Name of the group the edited pattern belongs to.
    QStringList m_usedNames;   ///< Names already used by other patterns in the group (uniqueness check).
    QList<int>  m_usedNumbers; ///< Numbers already used by other patterns in the group (uniqueness check).

    Pattern m_old;          ///< Original pattern values, read-only; used for "was:" labels and the diff view.
    Pattern m_new;          ///< Edited copy of the pattern values, mutated by the wizard steps and returned by result().

    int                  m_currentStep{0};     ///< Index of the currently displayed step.
    QStackedWidget      *m_stack{nullptr};      ///< Stacked widget holding the step pages.
    QList<QLabel*>       m_stepBubbles;         ///< Numbered/checkmark bubble labels in the step rail, one per step.
    QList<QLabel*>       m_stepLabels;          ///< Step-name labels in the step rail, one per step.
    QLabel              *m_subtitleLabel{nullptr}; ///< Header subtitle showing group name and current step.
    QLabel              *m_footerStatus{nullptr};  ///< Footer label showing validation/status text for the current step.
    QPushButton         *m_btnBack{nullptr};    ///< Footer "Back" button; disabled on step 0.
    QPushButton         *m_btnNext{nullptr};    ///< Footer "Next"/"Apply Changes" button; label and slot connection change on the Finish step.
    QPushButton         *m_btnCancel{nullptr};  ///< Footer "Cancel" button; rejects the dialog.

    // Step 1
    AddPatternImageCanvas *m_lockedCanvas{nullptr}; ///< Locked (non-editable) image canvas shown on the Identity step.
    QLineEdit             *m_inputName{nullptr};    ///< Editable pattern-name field on the Identity step.
    QSpinBox              *m_inputNumber{nullptr};  ///< Editable pattern-number field on the Identity step.
    QLabel                *m_lblNameError{nullptr};   ///< Shows a duplicate-name validation message, empty when the name is valid.
    QLabel                *m_lblNumberError{nullptr}; ///< Shows a duplicate-number validation message, empty when the number is valid.

    // Step 2
    AddPatternImageCanvas *m_pickCanvas{nullptr}; ///< Image canvas in Pick mode; lets the user click to set the pick point.
    QSpinBox              *m_pickXSpin{nullptr};  ///< Pick-point X spin box, kept in sync with the canvas.
    QSpinBox              *m_pickYSpin{nullptr};  ///< Pick-point Y spin box, kept in sync with the canvas.
    QDoubleSpinBox        *m_pickAngleSpin{nullptr}; ///< Picking-angle spin box (Pattern::pickAngle).

    // Step 3
    AddPatternImageCanvas *m_boxCanvas{nullptr};     ///< Image canvas in Box mode; lets the user drag the picking box and its rotation handle.
    QComboBox             *m_presetCombo{nullptr};   ///< Gripper-preset selector; index 0 is "Custom".
    vc::model::GripperPresetStore m_gripperPresets;  ///< Presets offered by the selector (see setGripperPresets()).
    QDoubleSpinBox        *m_boxWSpin{nullptr};      ///< Picking-box width spin box, kept in sync with the canvas.
    QDoubleSpinBox        *m_boxHSpin{nullptr};      ///< Picking-box height spin box, kept in sync with the canvas.
    QDoubleSpinBox        *m_boxDistSpin{nullptr};   ///< Picking-box offset-distance spin box, kept in sync with the canvas.
    QDoubleSpinBox        *m_boxAngleSpin{nullptr};  ///< Picking-box rotation-angle spin box, kept in sync with the canvas.

    // Step 4 (offset)
    QDoubleSpinBox        *m_offXSpin{nullptr};   ///< Offset-X spin box (mm).
    QDoubleSpinBox        *m_offYSpin{nullptr};   ///< Offset-Y spin box (mm).
    QDoubleSpinBox        *m_offZSpin{nullptr};   ///< Offset-Z spin box (mm).
    QDoubleSpinBox        *m_offRXSpin{nullptr};  ///< Rotation-offset RX spin box (deg).
    QDoubleSpinBox        *m_offRYSpin{nullptr};  ///< Rotation-offset RY spin box (deg).
    QDoubleSpinBox        *m_offRZSpin{nullptr};  ///< Rotation-offset RZ spin box (deg).

    // Step 5 (diff)
    QLabel                *m_diffSummary{nullptr};  ///< Rich-text summary listing each changed field as "old -> new".
    AddPatternImageCanvas *m_finishCanvas{nullptr}; ///< Image canvas in Finish mode, showing the final pick point and picking box.
};

#endif // EDIT_PATTERN_WIZARD_H
