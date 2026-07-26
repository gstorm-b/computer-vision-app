#ifndef EDIT_PATTERN_WIZARD_H
#define EDIT_PATTERN_WIZARD_H

#include <QDialog>
#include <QStringList>
#include <QString>
#include <QPoint>
#include <QRect>
#include <opencv2/core.hpp>

class QStackedWidget;
class QLabel;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;

class AddPatternImageCanvas;

/// 4-step modal dialog reimplementation of `EditPatternWizard.jsx` for editing an
/// already-learned pattern. The learned image is LOCKED (cannot be re-captured);
/// the user can only edit identity, pick point, and picking-box config.
///
/// Steps:
///   1. Identity     - locked image (purple scrim) + editable name/number
///   2. Pick Point   - same canvas; pre-filled from saved pickX/pickY
///   3. Picking Box  - same as Add wizard; pre-filled from saved box config
///   4. Finish       - diff view: changed fields show "old -> new"
///
/// Lock color: #6a5acd (purple), per design tokens.
class EditPatternWizard : public QDialog {
    Q_OBJECT
public:
    /// Editable snapshot of a single pattern: identity (name/number), pick point,
    /// picking-box geometry, and the already-learned template image.
    struct Pattern {
        QString name;             ///< Pattern display name.
        int     number{0};        ///< Pattern number; must be unique within its group.
        int     pickX{0}, pickY{0};              ///< Pick point, in locked-image pixel coordinates.
        double  pickBoxW{120}, pickBoxH{80};      ///< Picking-box width/height, in image pixels.
        double  pickBoxDist{90}, pickBoxAngle{0}; ///< Picking-box offset distance (px) and rotation (degrees), relative to the pick point.
        cv::Mat image;          ///< Already-learned template image; locked (cannot be re-captured by this wizard).
    };

    /// Constructs the wizard pre-filled with `pattern`'s current values; step 1
    /// (Identity) is shown first via goToStep(0) in the .cpp constructor body.
    /// @param groupName name of the pattern's group, shown in the header subtitle
    /// @param pattern current (pre-edit) values, copied into both m_old and m_new
    /// @param usedNames names already used by other patterns in the group, for uniqueness validation
    /// @param usedNumbers numbers already used by other patterns in the group, for uniqueness validation
    explicit EditPatternWizard(const QString &groupName,
                               const Pattern &pattern,
                               const QStringList &usedNames,
                               const QList<int>  &usedNumbers,
                               QWidget *parent = nullptr);
    ~EditPatternWizard() override = default;

    /// Returns the edited pattern values (m_new). Only meaningful after the dialog
    /// has been accepted, i.e. after "Apply Changes" was clicked on the Finish step.
    Pattern result() const { return m_new; }

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

private:
    /// Assembles the dialog layout: header, step rail, stacked step pages, and footer.
    void buildUi();
    /// Builds the title/subtitle header bar with its close button.
    QWidget *buildHeader();
    /// Builds the horizontal step rail showing all 4 steps with numbered bubbles and labels.
    QWidget *buildStepRail();
    /// Builds the footer bar containing the status label and Cancel/Back/Next buttons.
    QWidget *buildFooter();
    /// Builds the Identity step page: locked image canvas plus editable name/number fields.
    QWidget *buildStepIdentity();
    /// Builds the Pick Point step page: locked image canvas in pick mode plus X/Y spin boxes and a Center button.
    QWidget *buildStepPick();
    /// Builds the Picking Box step page: locked image canvas in box mode plus size/offset spin boxes and Reset/Rotate buttons.
    QWidget *buildStepBox();
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
    /// Switches the wizard to `step` (0-3): validates the current step before advancing, syncs step-specific canvases/fields, updates the step rail, footer, and Next/Back buttons.
    void goToStep(int step);
    /// Returns whether the current step's inputs are valid enough to advance; only step 0 (Identity) enforces non-empty/unique name and number, other steps always return true.
    bool currentStepValid() const;

private:
    QString     m_groupName;   ///< Name of the group the edited pattern belongs to.
    QStringList m_usedNames;   ///< Names already used by other patterns in the group (uniqueness check).
    QList<int>  m_usedNumbers; ///< Numbers already used by other patterns in the group (uniqueness check).

    Pattern m_old;          ///< Original pattern values, read-only; used for "was:" labels and the diff view.
    Pattern m_new;          ///< Edited copy of the pattern values, mutated by the wizard steps and returned by result().

    int                  m_currentStep{0};     ///< Index (0-3) of the currently displayed step.
    QStackedWidget      *m_stack{nullptr};      ///< Stacked widget holding the 4 step pages.
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

    // Step 3
    AddPatternImageCanvas *m_boxCanvas{nullptr};     ///< Image canvas in Box mode; lets the user drag the picking box and its rotation handle.
    QDoubleSpinBox        *m_boxWSpin{nullptr};      ///< Picking-box width spin box, kept in sync with the canvas.
    QDoubleSpinBox        *m_boxHSpin{nullptr};      ///< Picking-box height spin box, kept in sync with the canvas.
    QDoubleSpinBox        *m_boxDistSpin{nullptr};   ///< Picking-box offset-distance spin box, kept in sync with the canvas.
    QDoubleSpinBox        *m_boxAngleSpin{nullptr};  ///< Picking-box rotation-angle spin box, kept in sync with the canvas.

    // Step 4 (diff)
    QLabel                *m_diffSummary{nullptr};  ///< Rich-text summary listing each changed field as "old -> new".
    AddPatternImageCanvas *m_finishCanvas{nullptr}; ///< Image canvas in Finish mode, showing the final pick point and picking box.
};

#endif // EDIT_PATTERN_WIZARD_H
