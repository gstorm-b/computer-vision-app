#ifndef PATTERN_SETTING_PANEL_H
#define PATTERN_SETTING_PANEL_H

#include <QWidget>
#include <QPixmap>
#include <QString>

class QLabel;
class QStackedWidget;
class QPushButton;
class QToolButton;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QVBoxLayout;
class QGridLayout;

namespace mtc {
class MatchPattern;
class MatchGroup;
}

// ─────────────────────────────────────────────────────────────────────────────
//  PatternSettingPanel
//
//  Mirrors `PatternSetting` from
//  `ui_scratch/design_handoff_full_project/components/PatternManager.jsx`.
//
//  Sits below the Pattern Library tree in Pane 2. It has 3 internal pages,
//  swapped via a QStackedWidget based on selection:
//
//   ┌─────────── No selection ──────────┐
//   │ "Select a group or pattern"      │
//   └───────────────────────────────────┘
//
//   ┌─────────── Group only ────────────┐
//   │ ┌─ GROUP CONFIG ─────────────────┐│
//   │ │ Group Name        : <name>     ││
//   │ │ Group Number      : <n> (warn) ││
//   │ ├─ PICKING BOX ──────────────────┤│
//   │ │ Low Workpiece Ratio  [____]    ││
//   │ │ Box Width            [____]    ││
//   │ │ Box Height           [____]    ││
//   │ │ Distance             [____]    ││
//   │ │ Angle                [____]    ││
//   │ └────────────────────────────────┘│
//   └───────────────────────────────────┘
//
//   ┌─────────── Pattern selected ──────┐
//   │ #N name              ●LEARNED     │
//   │ ┌────────────────────┐            │
//   │ │   thumbnail        │            │
//   │ └────────────────────┘            │
//   │ [▶ Trigger & Learn] [📁 Open]    │
//   │ ─ Match Settings ────────────────  │
//   │ Pattern Number   #N (output)      │
//   │ Min Score        [0.85]           │
//   │ Angle            [0°]             │
//   │ Tolerance Angle  [180°]           │
//   │ Max Overlap      [0.10]           │
//   │ Pick X           [123 px]         │
//   │ Pick Y           [456 px]         │
//   │ ─ Edge Match Config ─────────────  │
//   │ ...                               │
//   └───────────────────────────────────┘
//
//  Signals are coarse — host (LocalizationPatternsWidget) decides what to do
//  with them (push into MatchGroup config, trigger learn, open image dialog).
// ─────────────────────────────────────────────────────────────────────────────

class PatternSettingPanel : public QWidget {
    Q_OBJECT
public:
    explicit PatternSettingPanel(QWidget *parent = nullptr);
    ~PatternSettingPanel() override = default;

    // Selection — only one is non-null (or both null for "no selection")
    void setSelection(mtc::MatchGroup *group, mtc::MatchPattern *pattern);
    void clearSelection();

    // Update the pattern thumbnail (host calls this when image loaded)
    void setPatternThumbnail(const QPixmap &pix);

    /// Toggle the visibility of the inline thumbnail strip on the pattern
    /// page.  Use when the host already shows a dedicated thumbnail panel
    /// elsewhere (e.g. below the library tree) — the strip in this panel
    /// would otherwise duplicate it.  Hides the thumbnail QLabel together
    /// with the Trigger / Open buttons since those actions are already
    /// available from the workspace toolbar.
    void setShowInlineThumbnail(bool show);

signals:
    // Pattern actions
    void learnRequested();
    void openImageRequested();

    // Pattern field changes (key matches MatchPatternConfig members in spirit)
    void patternFieldChanged(const QString &key, const QVariant &value);

    // Group field changes (e.g. "lowWorkpieceRatio", "pickBoxW", "pickBoxAngle")
    void groupFieldChanged(const QString &key, const QVariant &value);

private:
    // Build helpers
    void buildUi();
    QWidget *buildEmptyPage();
    QWidget *buildGroupPage();
    QWidget *buildPatternPage();

    QWidget *buildSectionHeader(const QString &label);
    QWidget *buildPropRow(const QString &label, QWidget *editor);

    QSpinBox       *makeIntSpin   (int min, int max, int initial = 0);
    QDoubleSpinBox *makeDoubleSpin(double min, double max, double step,
                                    int decimals, double initial = 0.0);
    QComboBox      *makeKernelCombo();
    QCheckBox      *makeToggle();

    void refreshFromGroup();
    void refreshFromPattern();
    void blockEditorSignals(bool block);

private:
    enum Page { kEmpty = 0, kGroup = 1, kPattern = 2 };

    QStackedWidget *m_stack{nullptr};

    // Pattern page widgets
    QLabel         *m_lblPatternTitle  {nullptr};   // "#N name"
    QLabel         *m_lblLearnedBadge  {nullptr};
    QWidget        *m_thumbWrap        {nullptr};   // wrapper holding thumb + buttons
    QLabel         *m_lblThumb         {nullptr};   // thumbnail QLabel
    QPushButton    *m_btnLearn         {nullptr};
    QPushButton    *m_btnOpenImage     {nullptr};

    QLabel         *m_lblPatternNumber {nullptr};   // "#N"
    QDoubleSpinBox *m_spinMinScore     {nullptr};
    QDoubleSpinBox *m_spinAngle        {nullptr};
    QDoubleSpinBox *m_spinTolAngle     {nullptr};
    QDoubleSpinBox *m_spinMaxOverlap   {nullptr};
    QSpinBox       *m_spinPickX        {nullptr};
    QSpinBox       *m_spinPickY        {nullptr};

    // Edge config
    QSpinBox       *m_spinThreshLower  {nullptr};
    QSpinBox       *m_spinThreshUpper  {nullptr};
    QComboBox      *m_cmbKernelSize    {nullptr};
    QSpinBox       *m_spinBlurW        {nullptr};
    QSpinBox       *m_spinBlurH        {nullptr};
    QDoubleSpinBox *m_spinGreediness   {nullptr};
    QSpinBox       *m_spinMinReduceLen {nullptr};
    QSpinBox       *m_spinTSamples     {nullptr};
    QCheckBox      *m_chkInvertBinary  {nullptr};
    QCheckBox      *m_chkSubPixel      {nullptr};
    QCheckBox      *m_chkStopAtL1      {nullptr};

    // Group page widgets
    QLabel         *m_lblGroupName     {nullptr};
    QLabel         *m_lblGroupNumber   {nullptr};
    QDoubleSpinBox *m_spinLowRatio     {nullptr};
    QDoubleSpinBox *m_spinPickBoxW     {nullptr};
    QDoubleSpinBox *m_spinPickBoxH     {nullptr};
    QDoubleSpinBox *m_spinPickBoxDist  {nullptr};
    QDoubleSpinBox *m_spinPickBoxAngle {nullptr};

    // State
    mtc::MatchGroup   *m_group  {nullptr};
    mtc::MatchPattern *m_pattern{nullptr};
    bool               m_blockSignals{false};
};

#endif // PATTERN_SETTING_PANEL_H
