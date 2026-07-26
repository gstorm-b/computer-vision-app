#ifndef LOCALIZATION_PATTERNS_WIDGET_H
#define LOCALIZATION_PATTERNS_WIDGET_H

#include <QWidget>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QSet>

#include <opencv2/core.hpp>

#include "DockWidget.h"

#include "model/task_localization.h"
#include "ui/forms/task_widget.h"
#include "ui/widgets/pattern_tree_widget.h"
#include "ui/widgets/vision/vision_overlay_types.h"

#include "ui/forms/pattern/add_pattern_image_dialog.h"
#include "ui/widgets/property_browser/match_config_property_adapter.h"
#include "matching/image_matcher.h"

namespace Ui {
class LocalizationPatternsWidget;
}

class VisionCanvas;
class VisionResultViewerWidget;

/// Task widget for managing localization pattern groups/patterns, editing
/// their match configuration, and running matching tests against a test
/// image. Three responsibilities:
///  1. Pattern management — add/remove/rename groups + patterns via the
///     PatternGroupManager held by the task.
///  2. Match configuration — bind the selected pattern's MatchPatternConfig
///     and the selected group's MatchGroupConfig to a property browser.
///  3. Matching test — run a matching pass on a test image and visualise
///     the result. Image source is provided externally: connect
///     `requestCameraImage()` on the host side and feed back via
///     `setCameraImage()`.
class LocalizationPatternsWidget : public ITaskWidget {
    Q_OBJECT

public:
    /// Constructs the widget for `task`, runs the generated UI setup, and
    /// wires toolbar/tree/property-browser/vision-canvas state via
    /// initWidget().
    /// @param task the localization task this widget edits (cast internally
    ///        to TaskLocalization)
    /// @param dock optional dock-widget host, forwarded to ITaskWidget
    /// @param parent optional parent widget
    explicit LocalizationPatternsWidget(std::shared_ptr<vc::model::ITask> task,
                                        ads::CDockWidget *dock = nullptr,
                                        QWidget *parent = nullptr);
    /// Tears down property-browser state (unbinds the config adapter,
    /// releases group/pattern property groups) and deletes the generated UI.
    ~LocalizationPatternsWidget();

    /// ITaskWidget hook for pushing widget state back into the task config;
    /// currently a no-op because pattern/group/workspace edits are already
    /// committed live through PatternGroupManager and TaskLocalizeConfig as
    /// they happen.
    void loadConfigToTask() override;
    /// ITaskWidget hook for pulling task config into the widget UI; currently
    /// a no-op because initWidget() already seeds UI state (camera combo,
    /// group combo, KPIs) from the task at construction time.
    void loadConfigToWidget() override;

signals:
    /// Emitted when the user clicks "Trigger Camera".  Host should capture
    /// an image from the active camera and feed it back via setCameraImage().
    void requestCameraImage(QString id);

public slots:
    /// External entry point for image input (camera, file, network …).
    /// @note Routes by priority: forwarded to the active Add Pattern wizard
    ///       if one is open, else to the legacy AddPatternImageDialog if
    ///       visible; otherwise becomes the new current test image
    ///       (m_currentImage).
    void setCameraImage(const cv::Mat &image);

    /// Open the Edit Pattern Wizard for a specific (groupNumber, patternNumber).
    /// Returns true if the user accepted and the pattern was updated.
    /// Invoke from a host-supplied UI control (e.g. a "Edit" toolbar button
    /// or a keyboard shortcut), or from the pattern tree's edit-icon click.
    /// @return true on accept and a successful commit through the manager;
    ///         false if the group/pattern wasn't found or the wizard/commit
    ///         was rejected
    /// @note Only pattern-level fields are updated (name, number, pick
    ///       position, picking-box geometry); the raw pattern image is left
    ///       untouched.
    bool editPattern(int groupNumber, int patternNumber);

    /// Open the Edit Pattern Wizard for the currently-selected pattern,
    /// or do nothing if no pattern is selected.
    void editSelectedPattern();

private slots:
    // ── Pattern tree handlers ────────────────────────────────────────────
    /// Selects the tree-clicked group and syncs the active-group combo to
    /// match (blocking its signal to avoid re-entrant selection).
    void onTreeGroupClicked   (int groupIndex, const MatchGroupConfig &cfg);
    /// Selects the tree-clicked pattern (and its owning group) and syncs the
    /// active-group combo to match.
    void onTreePatternClicked (int groupIndex, int patternIndex,
                               const MatchPatternConfig &cfg);

    /// Prompts for a new group's name/number via AddGroupDialog, rejects
    /// duplicates, then adds it through the PatternGroupManager.
    void onTreeAddGroupRequested();
    /// Runs the 5-step AddPatternWizard for `groupIndex`, adds the resulting
    /// pattern through the manager, then pushes the wizard's pick position
    /// and picking-box geometry back into the newly created pattern.
    void onTreeAddPatternRequested  (int groupIndex);
    /// Confirms with the user, then removes the group (and all its patterns)
    /// through the manager and mirrors the removal in the tree widget.
    void onTreeDeleteGroupRequested (int groupIndex);
    /// Confirms with the user, then removes the pattern through the manager,
    /// mirrors the removal in the tree widget, and clears the selection if
    /// the removed pattern was the one currently bound.
    void onTreeDeletePatternRequested(int groupIndex, int patternIndex);

    /// Placeholder for bulk tree-state changes; currently unused (no-op).
    void onTreeGroupsChanged (const QList<MatchGroupConfig> &groups);
    /// Placeholder for live group rename/renumber from the tree's inline
    /// editors; currently a no-op — mirroring it into the manager would need
    /// the group's previous name, which the tree only exposes as new state.
    void onTreeGroupChanged  (int groupIndex, const MatchGroupConfig &cfg);
    /// Placeholder for live pattern rename/renumber from the tree's inline
    /// editors; currently a no-op, same caveat as onTreeGroupChanged().
    void onTreePatternChanged(int groupIndex, int patternIndex,
                              const MatchPatternConfig &cfg);

    /// Forwards a successful camera grab's frame into setCameraImage();
    /// ignored when the grab failed.
    void onCameraGrabFinished(vc::device::GrabResult result);

    // ── Toolbar actions ──────────────────────────────────────────────────
    /// Requests a single-shot grab from the active camera's runner, after
    /// validating a camera is selected and the task is in Commission state;
    /// the resulting frame arrives via onCameraGrabFinished().
    void onTriggerCameraClicked();
    /// Opens a file dialog and loads the chosen image (via OpenCV) as the
    /// current test image.
    void onChooseImageClicked();
    /// Opens WorkspaceSettingDialog to define/edit the active camera's
    /// workspace ROI, condition ROI, and reference image, then persists the
    /// result into the task's TaskLocalizeConfig.
    void onSetWorkspaceClicked();
    /// Validates that an image and an active group are present, then starts
    /// an async matching test via runMatchingTest().
    void onRunMatchingClicked();
    /// Selects the group corresponding to the newly-active combo entry.
    void onActiveGroupChanged(int comboIndex);

    // ── View tab toggle (Raw / Result / Binary) ──────────────────────────
    /// Switches the monitor stack to the Raw page and refreshes the
    /// workspace ROI overlay.
    void onShowRawView();
    /// Re-renders the last match result's overlay (if any) and switches the
    /// monitor stack to the Result page.
    void onShowResultView();
    /// Switches the monitor stack to the Binary page and refreshes the
    /// binarized preview.
    void onShowBinaryView();
    /// Applies the auto/manual threshold mode to the slider/spin controls,
    /// persists threshold + maxValue onto the bound group's shared
    /// EdgeMatchConfig, and refreshes the binary preview.
    void onBinaryThresholdChanged();

    // ── Property browser ────────────────────────────────────────────────
    /// Commits an Edge-Based property edit already written into
    /// m_workingGroupConfig by the adapter, then resyncs the binary-view
    /// controls (and repaints the binary preview if it's the active page).
    void onGroupTypeConfigModified();
    /// Dispatches a property-browser value change to the bound group's
    /// working config or the bound pattern's "Common" working config
    /// (whichever key map contains `property`), then commits it through the
    /// manager.
    void onPropertyValueChanged(QtProperty *property, const QVariant &value);

    // ── Matching commission ─────────────────────────────────────────────
    /// Handles completion of an async matching test: stores the result,
    /// updates KPIs/result table/result overlay, switches to the Result page,
    /// and sets the status pill based on match/collision/area outcome.
    void onMatchingCommissionDone(mtc::MatchResult result);

private:
    // ── Build / wire-up ─────────────────────────────────────────────────
    /// Binds the task, seeds the thumbnail scene, wires the toolbar/tree/
    /// property browser/vision canvases, builds the result table, and seeds
    /// initial UI state (camera/group combos, KPIs, monitor page).
    void initWidget();

    /// Connects toolbar buttons, view-toggle buttons, binary-threshold
    /// controls, and the camera/group combos to their handlers, then seeds
    /// the camera combo and "Use ROI" checkbox.
    void wireToolbar();
    /// Connects PatternTreeWidget signals to their handlers; defers the
    /// edit-pattern flow via QTimer::singleShot so the modal wizard doesn't
    /// delete the row's Edit button while its click is still on the stack.
    void wireTree();
    /// Connects PatternGroupManager's group/pattern added/removed/changed
    /// signals to keep the tree, combo, and cached selection in sync, and
    /// performs the initial tree population from the manager's current
    /// state.
    void wireManagerSignals();
    /// Connects the shared QtVariantPropertyManager's valueChanged to
    /// onPropertyValueChanged(), then builds the initial property browser.
    void wirePropertyBrowser();
    /// Pulls the full library state from the manager and rebuilds the tree
    /// from scratch. Used at init and after bulk re-orderings.
    void rebuildTreeFromManager();
    /// Builds and inserts the result-table widget below the KPI strip.
    void buildResultTable();
    /// Repopulates the result table from `result.Objects`, colour-coding the
    /// score and OK/NG columns by collision/outside-ROI state.
    void populateResultTable(const mtc::MatchResult &result);
    /// Clears the result-viewer selection and empties the result table.
    void clearResultTable();
    /// Unbinds the config adapter and releases the group/pattern property
    /// groups so a fresh property tree can be rebuilt.
    void clearPropertyBrowserState();
    /// Replaces the designer-placed raw/result image views with the vision
    /// canvas/result-viewer widgets (idempotent — no-op if already installed).
    void installVisionWidgets();
    /// Refreshes the raw-view image: the current test image if one is loaded,
    /// else the bound pattern's raw image, else clears the canvas.
    void syncEditorImageForSelection();
    /// Builds the workspace/condition ROI overlays for the active camera,
    /// honoring the "Show ROI" checkbox.
    /// @return empty vector when hidden, no image, or no active camera
    QVector<VisionRoi> buildWorkspaceOverlayRois() const;
    /// Mirrors the result table's current row selection into the result
    /// viewer's highlighted object.
    void syncResultSelectionFromTable();
    /// Mirrors a result-viewer object selection back into the result table's
    /// row selection.
    void syncResultSelectionFromViewer(int objectIndex);
    /// Clears both the result table's row selection and the result viewer's
    /// highlighted object.
    void clearResultSelection();
    /// Looks up the match-result object index stored in a result-table row's
    /// Qt::UserRole data.
    /// @return the object index, or -1 if `row` is out of range
    int resultOverlayIndexForRow(int row) const;

    // ── Property browser construction ───────────────────────────────────
    /// Builds the "Group Settings" property group (picking box etc.) from
    /// kMatchGroupSpecs, bound to m_workingGroupConfig.
    void buildGroupProperties();
    /// Builds the "Pattern" (Common) property group — per-pattern identity +
    /// search params — from kCommonSpecs, bound to m_workingPatternCfg.
    void buildCommonProperties();
    /// Tears down and rebuilds the whole property tree (Group Settings →
    /// Edge-Based adapter → Pattern/Common), then reseeds the binary-view
    /// controls from the (possibly new) bound group.
    void rebuildPropertyBrowser();
    /// Binds `pattern` (may be null) as the working pattern and rebuilds the
    /// property browser to reflect it.
    void bindPatternToBrowser(mtc::MatchPattern *pattern);
    /// Clears the bound pattern and its working config, then rebuilds the
    /// property browser.
    void unbindPattern();

    /// Commit m_workingPatternCfg back to the manager (shared by the property
    /// browser Common group).
    /// @return false and rolls back the working copy (refreshing the Common
    ///         editors) on failure
    bool commitWorkingPatternConfig();

    /// Commit m_workingGroupConfig back to the manager (shared by the Group
    /// Settings + Edge-Based property groups and the binary-view controls).
    /// @return false and rolls back the working copy (refreshing the Group
    ///         Settings editors, the adapter, and the binary controls) on
    ///         failure
    bool commitWorkingGroupConfig();

    /// Seed the binary-view controls (auto/threshold/maxValue) from the bound
    /// group's shared EdgeMatchConfig; disables them when no group is
    /// selected.
    void seedBinaryControlsFromConfig();

    // ── Selection state ─────────────────────────────────────────────────
    /// Selects `groupIndex` as the active group, clears any pattern
    /// selection, re-resolves the bound MatchGroup from the manager, and
    /// refreshes the property browser + raw-view image.
    void selectGroup(int groupIndex);
    /// Selects the pattern `patternIndex` within group `groupIndex`, binding
    /// both the group and the pattern to the property browser and updating
    /// the thumbnail + raw-view image.
    void selectPattern(int groupIndex, int patternIndex);
    /// Clears the group/pattern selection, unbinds the property browser and
    /// thumbnail, and refreshes the raw-view image.
    void clearSelection();
    /// Updates the pattern-thumbnail scene from `pattern` (its pick-position
    /// overlay, or raw image if that's empty); shows a placeholder caption
    /// when `pattern` is null or has no image.
    void updatePatternThumb(mtc::MatchPattern *pattern);

    // ── Camera combo ────────────────────────────────────────────────────
    /// Rebuilds the camera combo from the task's assigned Camera-type
    /// devices, restoring the previous selection by device id when still
    /// present, and enables the Trigger button only if a camera is assigned.
    void rebuildCameraCombo();
    /// Active camera device id from the camera combo ("" when none).
    QString activeCameraId() const;

    // ── Camera workspace (ROI) ──────────────────────────────────────────
    /// Draw the active camera's workspace ROI on the raw image view
    /// (read-only overlay), gated by the "Show ROI" checkbox. Orange when
    /// the workspace is in use, muted when only defined.
    void updateWorkspaceRoiOverlay();
    /// "Use ROI" checkbox → toggle the active camera workspace's
    /// useWorkspace, persisting the change into the task's config.
    void onUseRoiToggled(bool enabled);
    /// Reflect the active camera's useWorkspace into the "Use ROI" checkbox.
    void syncUseRoiCheckbox();

    // ── Group combo ─────────────────────────────────────────────────────
    /// Rebuilds the group combo from the pattern manager's current groups,
    /// preserving the previous selection by group number when possible, and
    /// updates the Run Match button's enabled state.
    void rebuildGroupCombo();

    // ── Status / KPI / state pill ───────────────────────────────────────
    /// Status-pill states shown in the toolbar; drives the pill's label text
    /// and stylesheet role.
    enum class State { Idle, Busy, Success, Warning, Error };
    /// Updates the state pill's text/style and the status-text label.
    /// @param message shown verbatim, or "Ready" when empty
    void  setState(State state, const QString &message = {});
    /// Refreshes the "Groups: N · Patterns: N" status label from the pattern
    /// manager's current group/pattern counts.
    void  updateGroupsCount();
    /// Resets the KPI labels to placeholders and clears the result table.
    void  resetKpis();
    /// Populates the KPI labels (object count, picking count, execution time,
    /// area-below-limit flag) from a matching result.
    void  applyKpis(const mtc::MatchResult &result);

    // ── Image display ───────────────────────────────────────────────────
    /// Displays `image` in the raw preview (vision canvas if installed, else
    /// the legacy image view) along with the workspace ROI overlay.
    void displayRawImage(const cv::Mat &image);
    /// Renders `result`'s overlay (detected objects, ROIs) onto `image` (or
    /// the result's own annotated image as a fallback) in the result viewer.
    void displayResultOverlay(const cv::Mat &image, const mtc::MatchResult &result);
    /// Binarizes m_currentImage at the current threshold (auto/Otsu or
    /// manual) and max value, shows it in the binary view, and updates the
    /// threshold-info label.
    void displayBinaryImage();
    /// Switches the monitor stack widget to `page` (raw, result, or binary)
    /// and syncs the corresponding view-toggle button's checked state.
    void setMonitorPage(int page);

    // ── Matching test runner ────────────────────────────────────────────
    /// Starts an async matching pass on m_currentImage against the selected
    /// group (clamping/disabling the workspace ROI if it falls outside the
    /// image); result arrives via onMatchingCommissionDone().
    /// @return false if there is no manager/selected group/image, or the
    ///         selected group has no patterns
    bool runMatchingTest(mtc::MatchResult &outResult);

    // ── Helpers ─────────────────────────────────────────────────────────
    /// Converts an OpenCV Mat (8-bit gray/BGR/BGRA) into a QPixmap for
    /// display.
    /// @return a null QPixmap for an empty or unsupported Mat type
    static QPixmap matToPixmap(const cv::Mat &mat);

private:
    Ui::LocalizationPatternsWidget *ui;   ///< Generated UI holding the designer-created child widgets.

    vc::model::TaskLocalization     *m_localizeTask{nullptr};   ///< m_task cast to TaskLocalization, set in initWidget(); not owned.
    vc::model::TaskLocalizeConfig    m_config;   ///< Local snapshot of the task's config; refreshed manually after workspace edits, not kept live-bound.

    mtc::PatternGroupManager        *m_patternManager{nullptr};   ///< The task's pattern-group manager (owns all groups/patterns); not owned by this widget.
    mtc::MatchConfigPropertyAdapter *m_configAdapter {nullptr};   ///< Bridges the bound group's Edge-Based algorithm config to the shared property browser; owned via Qt parentage.

    AddPatternImageDialog           *m_addPatternImageDialog{nullptr};   ///< Legacy camera-capture dialog used by the older add-pattern flow.

    /// Active Add Pattern wizard (non-null only while wizard is open).
    /// Lets setCameraImage() forward captures into the wizard preview.
    class AddPatternWizard           *m_activeAddWizard{nullptr};

    /// Result table — sits below the KPI strip in Pane 1, inside a vertical
    /// splitter so the user can resize image vs. table.  Populated after each
    /// matching run from `mtc::MatchResult`.  Mirrors the Result Table from
    /// `PatternManager.jsx → MonitorPane`.
    class QTableWidget               *m_resultTable{nullptr};

    // ── Selection state (-1 = none) ─────────────────────────────────────
    int m_selectedGroupIndex  {-1};   ///< group *number*, not list index
    int m_selectedPatternIndex{-1};   ///< pattern number within the selected group

    // ── Working pattern config — adapter writes through this object,
    //    and we commit it back to the manager on edits. ─────────────────
    mtc::MatchGroupConfig           m_workingGroupConfig;   ///< Working copy of the bound group's config; edited in place by the property browser (Group Settings + Edge-Based adapter) before committing via commitWorkingGroupConfig().
    mtc::MatchGroup                 *m_boundMatchGroup{nullptr};   ///< Live MatchGroup currently bound to the property browser/binary controls (nullptr when none selected); not owned.
    mtc::MatchPatternConfig          m_workingPatternCfg;   ///< Working copy of the bound pattern's config; edited in place by the property browser's "Common" group before committing via commitWorkingPatternConfig().
    mtc::MatchPattern               *m_boundPattern{nullptr};   ///< Live MatchPattern currently bound to the property browser/thumbnail (nullptr when none selected); not owned.

    // ── Group-level properties on the same property browser ─────────────
    QtVariantProperty *m_groupVariant{nullptr};   ///< Root "Group Settings" property-group node.
    QMap<QtProperty *, QString> m_groupPropKeys;   ///< Maps each group QtProperty back to its PropSpec key, for dispatch in onPropertyValueChanged().
    QMap<QString, QtVariantProperty *> m_groupProps;   ///< Maps each PropSpec key to its QtVariantProperty, for refresh/rollback.

    // ── Pattern-level "Common" properties on the same property browser ──
    QtVariantProperty *m_commonVariant{nullptr};   ///< Root "Pattern" (Common) property-group node.
    QMap<QtProperty *, QString> m_commonPropKeys;   ///< Maps each Common QtProperty back to its PropSpec key, for dispatch in onPropertyValueChanged().
    QMap<QString, QtVariantProperty *> m_commonProps;   ///< Maps each PropSpec key to its QtVariantProperty, for refresh/rollback.


    // ── Test image ──────────────────────────────────────────────────────
    cv::Mat m_currentImage;   ///< Current test image (camera grab or file), used for raw/binary preview and matching runs.
    mtc::MatchResult m_lastMatchResult;   ///< Result of the most recent matching run; valid only when m_hasLastMatchResult is true.
    bool m_hasLastMatchResult{false};   ///< True once a matching run has completed at least once.
    VisionCanvas *m_rawPreview{nullptr};   ///< Vision canvas installed in place of the raw-image view (owned via Qt parentage).
    VisionResultViewerWidget *m_resultViewer{nullptr};   ///< Result viewer installed in place of the result-image view (owned via Qt parentage).

    // ── Pattern thumbnail scene ────────────────────────────────────────
    QGraphicsScene      *m_thumbScene  {nullptr};   ///< Graphics scene backing the pattern-thumbnail preview.
    QGraphicsPixmapItem *m_thumbPixmap {nullptr};   ///< Pixmap item within m_thumbScene showing the selected pattern's thumbnail.
};

#endif // LOCALIZATION_PATTERNS_WIDGET_H
