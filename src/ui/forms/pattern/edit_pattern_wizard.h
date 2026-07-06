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

// ─────────────────────────────────────────────────────────────────────────────
//  EditPatternWizard
//
//  4-step modal dialog reimplementation of `EditPatternWizard.jsx`.
//  Image is LOCKED (cannot be re-captured) — user can only edit identity,
//  pick point, and picking-box config.
//
//      1. Identity     — locked image (purple scrim) + editable name/number
//      2. Pick Point   — same canvas; pre-filled from saved pickX/pickY
//      3. Picking Box  — same as Add wizard; pre-filled from saved box config
//      4. Finish       — diff view: changed fields show "old → new"
//
//  Lock color: #6a5acd (purple), per design tokens.
// ─────────────────────────────────────────────────────────────────────────────

class EditPatternWizard : public QDialog {
    Q_OBJECT
public:
    struct Pattern {
        QString name;
        int     number{0};
        int     pickX{0}, pickY{0};
        double  pickBoxW{120}, pickBoxH{80};
        double  pickBoxDist{90}, pickBoxAngle{0};
        cv::Mat image;          // already learned image — locked
    };

    explicit EditPatternWizard(const QString &groupName,
                               const Pattern &pattern,
                               const QStringList &usedNames,
                               const QList<int>  &usedNumbers,
                               QWidget *parent = nullptr);
    ~EditPatternWizard() override = default;

    // Result accessors (valid only after Accepted)
    Pattern result() const { return m_new; }

private slots:
    void onNext();
    void onBack();
    void onCancel();
    void onApply();

    void onNameChanged(const QString &v);
    void onNumberChanged(int v);
    void onPickChanged(const QPoint &p);
    void onPickCenter();
    void onBoxChanged();
    void onBoxReset();
    void onBoxRotate90();

private:
    void buildUi();
    QWidget *buildHeader();
    QWidget *buildStepRail();
    QWidget *buildFooter();
    QWidget *buildStepIdentity();
    QWidget *buildStepPick();
    QWidget *buildStepBox();
    QWidget *buildStepFinish();

    QLabel *makeStepBubble(int idx);
    void updateStepRail();
    void updateFooterStatus();
    void refreshDiff();
    void goToStep(int step);
    bool currentStepValid() const;

private:
    QString     m_groupName;
    QStringList m_usedNames;
    QList<int>  m_usedNumbers;

    Pattern m_old;          // original (read-only)
    Pattern m_new;          // edited copy

    int                  m_currentStep{0};
    QStackedWidget      *m_stack{nullptr};
    QList<QLabel*>       m_stepBubbles;
    QList<QLabel*>       m_stepLabels;
    QLabel              *m_subtitleLabel{nullptr};
    QLabel              *m_footerStatus{nullptr};
    QPushButton         *m_btnBack{nullptr};
    QPushButton         *m_btnNext{nullptr};
    QPushButton         *m_btnCancel{nullptr};

    // Step 1
    AddPatternImageCanvas *m_lockedCanvas{nullptr};
    QLineEdit             *m_inputName{nullptr};
    QSpinBox              *m_inputNumber{nullptr};
    QLabel                *m_lblNameError{nullptr};
    QLabel                *m_lblNumberError{nullptr};

    // Step 2
    AddPatternImageCanvas *m_pickCanvas{nullptr};
    QSpinBox              *m_pickXSpin{nullptr};
    QSpinBox              *m_pickYSpin{nullptr};

    // Step 3
    AddPatternImageCanvas *m_boxCanvas{nullptr};
    QDoubleSpinBox        *m_boxWSpin{nullptr};
    QDoubleSpinBox        *m_boxHSpin{nullptr};
    QDoubleSpinBox        *m_boxDistSpin{nullptr};
    QDoubleSpinBox        *m_boxAngleSpin{nullptr};

    // Step 4 (diff)
    QLabel                *m_diffSummary{nullptr};
    AddPatternImageCanvas *m_finishCanvas{nullptr};
};

#endif // EDIT_PATTERN_WIZARD_H
