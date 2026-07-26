#include "localization_patterns_widget.h"
#include "ui_localization_patterns_widget.h"

#include "ui/forms/pattern/pattern_manager_dialog.h"
#include "ui/forms/pattern/pattern_theme.h"
#include "core/utils/theme_manager.h"
#include "ui/forms/pattern/add_pattern_wizard.h"
#include "ui/forms/pattern/edit_pattern_wizard.h"
#include "ui/forms/task/workspace_setting_dialog.h"
#include "qtpropertybrowser/qtvariantproperty.h"
#include "ui/widgets/vision/vision_canvas.h"
#include "ui/widgets/vision/vision_geometry.h"
#include "ui/widgets/vision/vision_result_adapter.h"
#include "ui/widgets/vision/vision_result_viewer_widget.h"
#include "matching/vision_utils.h"

#include <QTableWidget>
#include <QHeaderView>
#include <QSlider>
#include <QSpinBox>

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QShortcut>
#include <QTimer>
#include <QFileInfo>
#include <QDateTime>
#include <QPainter>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QLayout>
#include <QStackedWidget>
#include <QFrame>
#include <QSplitter>
#include <QHash>
#include <QPair>
#include <QMetaType>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/types.hpp>

#include <QImage>

#include <initializer_list>

namespace {

/// Converts a QPixmap back to a cv::Mat (inverse of matToPixmap). Used to persist the
/// workspace reference image chosen in the workspace dialog.
/// @param pixmap source pixmap; converted via QImage with format-specific handling
///        (Grayscale8, RGB888, or RGBA8888 fallback with BGR/BGRA channel swap)
/// @return the converted, deep-copied cv::Mat, or an empty Mat if the pixmap is null
cv::Mat pixmapToMat(const QPixmap &pixmap) {
    const QImage qimg = pixmap.toImage();
    if (qimg.isNull()) return cv::Mat();

    switch (qimg.format()) {
    case QImage::Format_Grayscale8: {
        cv::Mat mat(qimg.height(), qimg.width(), CV_8UC1,
                    const_cast<uchar*>(qimg.bits()),
                    static_cast<size_t>(qimg.bytesPerLine()));
        return mat.clone();
    }
    case QImage::Format_RGB888: {
        cv::Mat mat(qimg.height(), qimg.width(), CV_8UC3,
                    const_cast<uchar*>(qimg.bits()),
                    static_cast<size_t>(qimg.bytesPerLine()));
        cv::Mat bgr;
        cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    }
    default: {
        const QImage converted = qimg.convertToFormat(QImage::Format_RGBA8888);
        cv::Mat mat(converted.height(), converted.width(), CV_8UC4,
                    const_cast<uchar*>(converted.bits()),
                    static_cast<size_t>(converted.bytesPerLine()));
        cv::Mat bgra;
        cv::cvtColor(mat, bgra, cv::COLOR_RGBA2BGRA);
        return bgra;
    }
    }
}

/// Swaps `oldWidget` for `newWidget` inside `layout`, hiding the old widget and showing the
/// new one. No-op if any argument is null. Used to swap the plain designer image-view widgets
/// for the VisionCanvas / VisionResultViewerWidget instances installed by installVisionWidgets().
/// @param layout the layout holding oldWidget
/// @param oldWidget the widget being replaced; hidden after the swap
/// @param newWidget the replacement widget; shown after the swap
void replacePageWidget(QLayout *layout, QWidget *oldWidget, QWidget *newWidget)
{
    if (!layout || !oldWidget || !newWidget) return;
    layout->replaceWidget(oldWidget, newWidget);
    oldWidget->hide();
    newWidget->show();
}

/// Resolves a theme token name to a QColor for the current light/dark theme via ThemeManager.
/// @param token theme token key (e.g. "accent.primary", "text.muted")
/// @return the token's color for the currently active theme
QColor themeTokenColor(const QString &token)
{
    return QColor(ThemeManager::tokenValue(token, ThemeManager::instance()->isDark()));
}

/// Returns the first candidate in `candidates` that is non-null, non-empty, and converts to a
/// valid QPixmap (via vision::pixmapFromMat), or nullptr if none qualify.
/// @param candidates images to test, in priority order
/// @return pointer to the first renderable candidate, or nullptr
const cv::Mat *firstRenderableImage(std::initializer_list<const cv::Mat *> candidates)
{
    for (const cv::Mat *candidate : candidates) {
        if (candidate && !candidate->empty()
            && !vision::pixmapFromMat(*candidate).isNull()) {
            return candidate;
        }
    }
    return nullptr;
}

/// Page indices into `stackedWidget_ImageView`: raw camera/file image, match-result overlay,
/// and binarized preview, respectively.
constexpr int kMonitorPageRaw = 0;
constexpr int kMonitorPageResult = 1;
constexpr int kMonitorPageBinary = 2;

} // namespace

namespace {

// State pill stylesheet templates ─ identical layout, different accent.
/// State-pill stylesheet template — identical layout for every state; only the foreground
/// (%1) and background (%2) accent colors differ.
const char* kPillBase =
    "QLabel { color: %1; background-color: %2; border-radius: 13px; "
    "padding: 3px 10px; font: 600 9pt \"Segoe UI\"; }";

/// Fills kPillBase with the given foreground/background colors for the status-pill QLabel.
/// @param fg foreground (text) color, as a CSS color string
/// @param bg background color, as a CSS color string
/// @return the resulting Qt stylesheet fragment
QString pillStyle(const QString &fg, const QString &bg) {
    return QString(kPillBase).arg(fg, bg);
}

/// Extracts the human-readable error message from a PatternGroupManager result.
/// @return `r.error` converted from std::wstring to QString
inline QString resultMessage(const mtc::ManagerResult &r) {
    return QString::fromStdWString(r.error);
}

} // anonymous

// ── Image conversion helper ──────────────────────────────────────────────────

/// Converts an OpenCV cv::Mat (8-bit grayscale, BGR, or BGRA) to a QPixmap for display,
/// deep-copying pixel data and swapping channel order (BGR/BGRA to RGB/RGBA) as needed.
/// @param mat source image; must be CV_8UC1, CV_8UC3, or CV_8UC4
QPixmap LocalizationPatternsWidget::matToPixmap(const cv::Mat &mat) {
    if (mat.empty()) return {};

    QImage img;
    if (mat.type() == CV_8UC1) {
        img = QImage(mat.data, mat.cols, mat.rows,
                     static_cast<int>(mat.step),
                     QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        img = QImage(rgb.data, rgb.cols, rgb.rows,
                     static_cast<int>(rgb.step),
                     QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        img = QImage(rgba.data, rgba.cols, rgba.rows,
                     static_cast<int>(rgba.step),
                     QImage::Format_RGBA8888).copy();
    } else {
        return {};
    }
    return QPixmap::fromImage(img);
}

/// Declarative table of the group-level "Group Settings" properties shown in the property
/// browser (name, number, sort-by-angle, sort angle) — each entry pairs a property-browser
/// spec with a getter/setter into mtc::MatchGroupConfig, consumed by PropSpecHelper.
static const QList<PropSpec<mtc::MatchGroupConfig>> kMatchGroupSpecs = {
    { "groupName",       "Group name",           "Group defined name.",
     QMetaType::QString, "",   "",   "",  -1, false,
     [](const mtc::MatchGroupConfig &c){ return QVariant(QString::fromStdWString(c.m_groupName)); },
     [](mtc::MatchGroupConfig &c, const QVariant &v){ c.m_groupName = v.toString().toStdWString(); } },

    { "groupNumber",
     "Group Number",
     "Group defined number.",
     QMetaType::Int, 1, 32, 1,  1, false,
     [](const mtc::MatchGroupConfig &c){ return QVariant(c.m_groupIndex); },
     [](mtc::MatchGroupConfig &c, const QVariant &v){ c.m_groupIndex = v.toInt(); } },

    { "sortByAngle",
     "Sort by Angle",
     "Enable sort result by angle.",
     QMetaType::Bool, 0, 0, 0,  0, false,
     [](const mtc::MatchGroupConfig &c){ return QVariant(c.m_sortByAngle); },
     [](mtc::MatchGroupConfig &c, const QVariant &v){ c.m_sortByAngle = v.toBool(); } },

    { "sortConditionAngle",
     "Sort Angle (degree)",
     "Sort angle (degree).",
     QMetaType::Double, -180.0, 180.0,   1.0,  3, false,
     [](const mtc::MatchGroupConfig &c){ return QVariant(c.m_sortConditionAngle); },
     [](mtc::MatchGroupConfig &c, const QVariant &v){ c.m_sortConditionAngle = v.toDouble(); } },
};

// ── Pattern-level "Common" parameters (per-pattern identity + search) ─────────
// Moved here from MatchConfigPropertyAdapter: these are per-pattern, whereas the
// adapter now owns only the group-level algorithm (Edge-Based) config.
/// Declarative table of per-pattern "Common" properties (identity + search params) shown in
/// the property browser; each entry pairs a property-browser spec with a getter/setter into
/// mtc::MatchPatternConfig, consumed by PropSpecHelper.
static const QList<PropSpec<mtc::MatchPatternConfig>> kCommonSpecs = {
    { "patternName",       "Pattern Name",           "Name of pattern.",
     QMetaType::QString, 0,   -1,   0,  0, false,
     [](const mtc::MatchPatternConfig &c){ return QVariant(QString::fromStdWString(c.m_patternName)); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){ c.m_patternName = v.toString().toStdWString(); } },

    { "patternIndex",       "Pattern Number",        "Number of pattern (1 – 16).",
     QMetaType::Int, 1,   16,   1,  1, false,
     [](const mtc::MatchPatternConfig &c){ return QVariant(c.m_patternIndex); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){ c.m_patternIndex = v.toInt(); } },

    { "minScore",       "Min Score",           "Minimum acceptable match score (0 – 1).",
      QMetaType::Double, 0.0,   1.0,   0.01,  3, false,
      [](const mtc::MatchPatternConfig &c){ return QVariant(c.m_minScore); },
      [](mtc::MatchPatternConfig &c, const QVariant &v){ c.m_minScore = v.toDouble(); } },

    { "angle",          "Angle (°)",           "Search center angle in degrees.",
      QMetaType::Double, -360.0, 360.0, 1.0,  1, false,
      [](const mtc::MatchPatternConfig &c){ return QVariant(c.m_angle); },
      [](mtc::MatchPatternConfig &c, const QVariant &v){ c.m_angle = v.toDouble(); } },

    { "toleranceAngle", "Tolerance Angle (°)", "Allowed angle deviation around the center angle.",
      QMetaType::Double, 0.0,  360.0,  1.0,   1, false,
      [](const mtc::MatchPatternConfig &c){ return QVariant(c.m_toleranceAngle); },
      [](mtc::MatchPatternConfig &c, const QVariant &v){ c.m_toleranceAngle = v.toDouble(); } },

    { "maxOverlap",     "Max Overlap",         "Maximum allowed overlap ratio between detections (0 – 1).",
      QMetaType::Double, 0.0,  1.0,    0.01,  3, false,
      [](const mtc::MatchPatternConfig &c){ return QVariant(c.m_maxOverlap); },
      [](mtc::MatchPatternConfig &c, const QVariant &v){ c.m_maxOverlap = v.toDouble(); } },

    { "pickingPosX",     "Picking position (X)", "Picking position offset X on pattern.",
     QMetaType::Double, -1000000.0,  1000000.0,    0.01,  3, false,
     [](const mtc::MatchPatternConfig &c){ return QVariant(c.m_pickPosition.x); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){ c.m_pickPosition.x = v.toDouble(); } },

    { "pickingPosY",     "Picking position (Y)", "Picking position offset Y on pattern.",
     QMetaType::Double, -1000000.0,  1000000.0,    0.01,  3, false,
     [](const mtc::MatchPatternConfig &c){ return QVariant(c.m_pickPosition.y); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){ c.m_pickPosition.y = v.toDouble(); } },

    { "patternPickingBoxSizeWidth",
     "Picking Box Size (Width)",
     "Picking-box width. Edge-Based mode only; ignored in Correlation.",
     QMetaType::Double, 0.0, 100000.0, 1.0,  2, false,
     [](const mtc::MatchPatternConfig &c){
         return QVariant(c.m_pickingBoxSize.width); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){
         c.m_pickingBoxSize.width = v.toDouble(); } },

    { "patternPickingBoxSizeHeight",
     "Picking Box Size (Height)",
     "Picking-box height. Edge-Based mode only; ignored in Correlation.",
     QMetaType::Double, 0.0, 100000.0, 1.0,  2, false,
     [](const mtc::MatchPatternConfig &c){
         return QVariant(c.m_pickingBoxSize.height); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){
         c.m_pickingBoxSize.height = v.toDouble(); } },

    { "patternPickingBoxDistance",
     "Picking Box Distance",
     "Minimum distance between two picking boxes. Edge-Based mode only; ignored in Correlation.",
     QMetaType::Double, 0.0, 100000.0, 1.0,  2, false,
     [](const mtc::MatchPatternConfig &c){
         return QVariant(c.m_pickingBoxDistance); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){
         c.m_pickingBoxDistance = v.toDouble(); } },

    { "patternPickingBoxAngle",
     "Picking Box Angle",
     "Orientation of the picking box (degrees). Edge-Based mode only; ignored in Correlation.",
     QMetaType::Double, -360.0, 360.0, 0.1,  2, false,
     [](const mtc::MatchPatternConfig &c){
         return QVariant(c.m_pickingBoxAngle); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){
         c.m_pickingBoxAngle = v.toDouble(); } },

    { "patternPickingOffsetX",
     "Picking Offset (X)",
     "X-axis offset applied at picking time.",
     QMetaType::Double, -100000.0, 100000.0, 0.1,  3, false,
     [](const mtc::MatchPatternConfig &c){
         return QVariant(c.m_pickingOffset.x); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){
         c.m_pickingOffset.x = v.toFloat(); } },

    { "patternPickingOffsetY",
     "Picking Offset (Y)",
     "Y-axis offset applied at picking time.",
     QMetaType::Double, -100000.0, 100000.0, 0.1,  3, false,
     [](const mtc::MatchPatternConfig &c){
         return QVariant(c.m_pickingOffset.y); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){
         c.m_pickingOffset.y = v.toFloat(); } },

    { "patternPickingOffsetZ",
     "Picking Offset (Z)",
     "Z-axis offset applied at picking time.",
     QMetaType::Double, -100000.0, 100000.0, 0.1,  3, false,
     [](const mtc::MatchPatternConfig &c){
         return QVariant(c.m_pickingOffset.z); },
     [](mtc::MatchPatternConfig &c, const QVariant &v){
         c.m_pickingOffset.z = v.toFloat(); } },
};

// ── Construction ─────────────────────────────────────────────────────────────

/// Constructs the widget for `task`, sets up the generated UI, and runs initWidget() to wire
/// toolbar/tree/property-browser signals and seed initial state.
LocalizationPatternsWidget::LocalizationPatternsWidget(
        std::shared_ptr<vc::model::ITask> task,
        ads::CDockWidget *dock, QWidget *parent)
    : ITaskWidget(task, dock, parent),
      ui(new Ui::LocalizationPatternsWidget) {

    ui->setupUi(this);
    initWidget();
}

/// Releases property-browser state (unbinds the adapter, deletes owned Qt properties) before
/// destroying the generated UI.
LocalizationPatternsWidget::~LocalizationPatternsWidget() {
    clearPropertyBrowserState();
    delete ui;
}

/// No-op: this widget has no config to push back into the task (all edits commit directly
/// through PatternGroupManager as they happen).
void LocalizationPatternsWidget::loadConfigToTask()   {}
/// No-op: this widget has no widget-side config to (re)load from the task at open time (the
/// pattern tree is populated separately via wireManagerSignals()/rebuildTreeFromManager()).
void LocalizationPatternsWidget::loadConfigToWidget() {}

// ── initWidget ───────────────────────────────────────────────────────────────

/// Builds and wires all widget-owned state after ui->setupUi(): stylesheet reload hooks,
/// splitter ratios, the pattern-thumbnail scene, the task/pattern-manager bindings, the
/// property browser plus its Edge-Based config adapter, the vision canvas/result-viewer
/// widgets, toolbar/tree/manager signal wiring, and the result table — then seeds the group
/// combo, groups/patterns KPI, status pill, and monitor page (Raw) with their initial values.
void LocalizationPatternsWidget::initWidget() {
    setupThemeReload(QStringLiteral(":/styles/localization_patterns_widget_dark.qss"),
                     QStringLiteral(":/styles/localization_patterns_widget_light.qss"));

    // Refresh the right-pane section title so it matches what the panel shows.
    if (ui->label_property_title) {
        ui->label_property_title->setText(tr("  PATTERN CONFIGURATION"));
    }

    // splitter ratios: monitor | tree+thumb (library pane) | inspector
    ui->splitter_main->setStretchFactor(0, 6);
    ui->splitter_main->setStretchFactor(1, 3);
    ui->splitter_main->setStretchFactor(2, 3);

    // splitter ratios:  library tree | thumb widget
    ui->splitter_thumb->setStretchFactor(0, 6);
    ui->splitter_thumb->setStretchFactor(1, 4);

    // ── Pattern thumbnail scene — same QGraphicsView (now under the tree) ──
    m_thumbScene  = new QGraphicsScene(this);
    m_thumbPixmap = m_thumbScene->addPixmap(QPixmap{});
    if (ui->imageView_PatternThumb) {
        ui->imageView_PatternThumb->setScene(m_thumbScene);
        ui->imageView_PatternThumb->setRenderHint(QPainter::SmoothPixmapTransform);
    }

    // Bind to the task
    if (m_task) {
        m_localizeTask   = static_cast<vc::model::TaskLocalization*>(m_task.get());
        m_config         = m_localizeTask->taskLocalizeConfig();
        m_patternManager = m_localizeTask->patternManager();
    }

    // Camera-image dialog (legacy capture flow used by the new wizard)
    m_addPatternImageDialog = new AddPatternImageDialog(this);
    connect(m_addPatternImageDialog, &AddPatternImageDialog::requestImage,
            this, [this]() {
        this->onTriggerCameraClicked();
    });

    // init property widget
    if (ui->wg_property_browser) {
        initPropertyBrowser(ui->wg_property_browser);
    }

    // Property adapter for the group-level algorithm (Edge-Based) config
    m_configAdapter = new mtc::MatchConfigPropertyAdapter(m_variantManager, this);
    connect(m_configAdapter, &mtc::MatchConfigPropertyAdapter::configModified,
            this, &LocalizationPatternsWidget::onGroupTypeConfigModified);

    installVisionWidgets();

    wireToolbar();
    wireTree();
    wireManagerSignals();
    // Wire the property-browser's manager-level valueChanged → group-property
    // commit slot, and seed the browser with the group-settings group.  Must
    // run after m_configAdapter has been constructed (above) so both handlers
    // observe the same QtVariantPropertyManager instance.
    wirePropertyBrowser();

    // Build & install the Result Table below the KPI strip (Pane 1 bottom).
    // Mirrors `MonitorPane > Result Table` from PatternManager.jsx.
    buildResultTable();

    rebuildGroupCombo();
    updateGroupsCount();
    setState(State::Idle);

    // Binary-view threshold/maxValue controls are seeded from the selected
    // pattern's EdgeMatchConfig via seedBinaryControlsFromConfig(), invoked by
    // rebuildPropertyBrowser() on every selection change (and once here through
    // wirePropertyBrowser() above — no pattern bound yet, so controls start
    // disabled).
    setMonitorPage(kMonitorPageRaw);
}

// ── Wiring helpers ───────────────────────────────────────────────────────────

/// Connects every toolbar control (camera trigger/choose-image/set-workspace/run-match
/// buttons, view-tab buttons, binary-threshold slider+spinboxes, group/camera combos, ROI
/// checkboxes) to their handler slots, then seeds the camera combo and "Use ROI" checkbox and
/// disables Run Match until an image and a group are available.
void LocalizationPatternsWidget::wireToolbar() {
    connect(ui->btn_trigger_camera, &QToolButton::clicked,
            this, &LocalizationPatternsWidget::onTriggerCameraClicked);
    connect(ui->btn_choose_image, &QToolButton::clicked,
            this, &LocalizationPatternsWidget::onChooseImageClicked);
    connect(ui->btn_set_workspace, &QToolButton::clicked,
            this, &LocalizationPatternsWidget::onSetWorkspaceClicked);
    connect(ui->btn_run_match, &QToolButton::clicked,
            this, &LocalizationPatternsWidget::onRunMatchingClicked);

    connect(ui->btn_view_raw, &QPushButton::clicked,
            this, &LocalizationPatternsWidget::onShowRawView);
    connect(ui->btn_view_result, &QPushButton::clicked,
            this, &LocalizationPatternsWidget::onShowResultView);
    connect(ui->btn_view_binary, &QPushButton::clicked,
            this, &LocalizationPatternsWidget::onShowBinaryView);

    // Binary-view threshold controls (slider <-> spin kept in sync).
    connect(ui->sld_binary_threshold, &QSlider::valueChanged, this, [this](int v) {
        QSignalBlocker b(ui->spn_binary_threshold);
        ui->spn_binary_threshold->setValue(v);
        onBinaryThresholdChanged();
    });
    connect(ui->spn_binary_threshold, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        QSignalBlocker b(ui->sld_binary_threshold);
        ui->sld_binary_threshold->setValue(v);
        onBinaryThresholdChanged();
    });
    connect(ui->spn_binary_maxvalue, qOverload<int>(&QSpinBox::valueChanged),
            this, &LocalizationPatternsWidget::onBinaryThresholdChanged);
    connect(ui->chk_binary_auto, &QCheckBox::toggled,
            this, &LocalizationPatternsWidget::onBinaryThresholdChanged);

    connect(ui->comboBox_pattern_group,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LocalizationPatternsWidget::onActiveGroupChanged);

    // Active camera change → resync the "Use ROI" checkbox + ROI overlay
    // (workspace state is per-camera).
    connect(ui->comboBox_camera,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        syncUseRoiCheckbox();
        updateWorkspaceRoiOverlay();
    });

    // "Show ROI": pure view toggle for the raw-view ROI overlay.
    connect(ui->cbx_show_match_area, &QCheckBox::toggled,
            this, [this](bool) { updateWorkspaceRoiOverlay(); });

    // "Use ROI": toggle useWorkspace of the active camera's workspace.
    connect(ui->cbx_use_match_area, &QCheckBox::toggled,
            this, &LocalizationPatternsWidget::onUseRoiToggled);

    // Camera combo: rebuild whenever the task's assigned-device list changes,
    // then seed it once with the current state.  The host is expected to
    // listen for requestCameraImage(id) and feed an image back via
    // setCameraImage(), so the widget itself does not touch the camera here.
    if (m_localizeTask) {
        connect(m_localizeTask, &vc::model::ITask::devicesChanged,
                this, &LocalizationPatternsWidget::rebuildCameraCombo);
    }
    rebuildCameraCombo();
    syncUseRoiCheckbox();

    ui->btn_run_match->setEnabled(false);
}

/// Connects the pattern tree widget's click/request/change signals (group and pattern click,
/// add/delete requests, edit request, and change notifications) to their handler slots.
/// @note editPatternRequested is deferred via QTimer::singleShot(0, ...) so the triggering
///       row's Edit button is not destroyed mid-emission when the tree rebuilds on accept.
void LocalizationPatternsWidget::wireTree() {
    auto *tree = ui->treeWidget_patternEditor;
    connect(tree, &PatternTreeWidget::groupClicked,
            this, &LocalizationPatternsWidget::onTreeGroupClicked);
    connect(tree, &PatternTreeWidget::patternClicked,
            this, &LocalizationPatternsWidget::onTreePatternClicked);

    connect(tree, &PatternTreeWidget::addGroupRequested,
            this, &LocalizationPatternsWidget::onTreeAddGroupRequested);
    connect(tree, &PatternTreeWidget::addPatternRequested,
            this, &LocalizationPatternsWidget::onTreeAddPatternRequested);
    connect(tree, &PatternTreeWidget::editPatternRequested,
            this, [this](int groupIndex, int patternIndex) {
                // Defer: the request comes from the row's Edit button, whose
                // click is still on the stack.  Opening the modal wizard now and
                // then rebuilding the tree (on apply) would delete that button
                // mid-emission.  Let the click unwind first (matches the tree's
                // own deferred-rebuild idiom).
                QTimer::singleShot(0, this, [this, groupIndex, patternIndex]() {
                    editPattern(groupIndex, patternIndex);
                });
            });
    connect(tree, &PatternTreeWidget::deleteGroupRequested,
            this, &LocalizationPatternsWidget::onTreeDeleteGroupRequested);
    connect(tree, &PatternTreeWidget::deletePatternRequested,
            this, &LocalizationPatternsWidget::onTreeDeletePatternRequested);

    connect(tree, &PatternTreeWidget::groupsChanged,
            this, &LocalizationPatternsWidget::onTreeGroupsChanged);
    connect(tree, &PatternTreeWidget::groupChanged,
            this, &LocalizationPatternsWidget::onTreeGroupChanged);
    connect(tree, &PatternTreeWidget::patternChanged,
            this, &LocalizationPatternsWidget::onTreePatternChanged);
}

/// Rebuilds the tree from the current manager state, then subscribes to PatternGroupManager's
/// groupAdded/Removed/Changed and patternAdded/Removed/Changed signals to keep the pattern
/// tree, group combo, groups/patterns KPI, and cached selection in sync with manager mutations
/// coming from any source (this widget's own edits, or programmatic/project-load changes).
void LocalizationPatternsWidget::wireManagerSignals() {
    if (!m_patternManager) return;

    // ── Initial population ──────────────────────────────────────────────────
    // The tree is empty when widget is constructed.  Push current manager
    // state into it so existing groups/patterns (loaded from project JSON)
    // appear immediately — without this, the user sees an empty library.
    rebuildTreeFromManager();

    // ── Group added ────────────────────────────────────────────────────────
    connect(m_patternManager, &mtc::PatternGroupManager::groupAdded,
            this, [this] (std::shared_ptr<mtc::MatchGroup> group) {
        if (!group) return;
        MatchGroupConfig gr;
        gr.name    = QString::fromStdWString(group->name());
        gr.number  = group->number();
        // // Mirror pre-existing patterns so the tree shows complete state.
        for (const auto &p : group->patterns()) {
            MatchPatternConfig pc;
            pc.name        = QString::fromStdWString(p->name());
            pc.number      = p->number();
            pc.threshScore = p->config().m_minScore;
            pc.thumbnail   = matToPixmap(p->config().m_rawImage);
            gr.patterns.append(pc);
        }
        ui->treeWidget_patternEditor->addGroup(gr);
        rebuildGroupCombo();
        updateGroupsCount();
    });

    // ── Group removed ──────────────────────────────────────────────────────
    connect(m_patternManager, &mtc::PatternGroupManager::groupRemoved,
            this, [this] (const mtc::MatchGroupConfig &removed) {
        // Mirror the removal in the tree so the row disappears.
        ui->treeWidget_patternEditor->removeGroup(removed.m_groupIndex);

        // If the removed group was selected, clear selection panel.
        if (m_selectedGroupIndex == removed.m_groupIndex) clearSelection();

        rebuildGroupCombo();
        updateGroupsCount();
    });

    // ── Group changed (rename/renumber/picking-box edit) ───────────────────
    // Fires for any successful PatternGroupManager group mutation.  The
    // signal does not carry the *previous* identity of the group, so the
    // safest refresh is a full tree rebuild (it preserves expansion state).
    connect(m_patternManager, &mtc::PatternGroupManager::groupChanged,
            this, [this] (std::shared_ptr<mtc::MatchGroup> group, const QString &) {
        if (!group) return;

        // Keep our cached selection in sync if the currently-bound group
        // was the one that changed.  The pointer is stable across edits
        // (setConfig updates in place), so identity tracking via raw
        // pointer is reliable here.
        if (m_boundMatchGroup == group.get())
            m_selectedGroupIndex = group->number();

        rebuildTreeFromManager();
        rebuildGroupCombo();

        // rebuildGroupCombo() restores selection by previous currentData()
        // (the old group number), which won't match after a renumber.
        // Re-select explicitly by the cached (possibly updated) number.
        const int idx = ui->comboBox_pattern_group->findData(m_selectedGroupIndex);
        if (idx >= 0) {
            QSignalBlocker b(ui->comboBox_pattern_group);
            ui->comboBox_pattern_group->setCurrentIndex(idx);
        }
    });

    // ── Pattern added ──────────────────────────────────────────────────────
    connect(m_patternManager, &mtc::PatternGroupManager::patternAdded,
            this, [this] (mtc::MatchGroup *group, mtc::MatchPattern *pattern) {
        if (!group || !pattern) return;
        MatchPatternConfig p;
        p.name        = QString::fromStdWString(pattern->name());
        p.number      = pattern->number();
        p.threshScore = pattern->config().m_minScore;
        p.thumbnail   = matToPixmap(pattern->config().m_rawImage);
        ui->treeWidget_patternEditor->addPattern(group->number(), p);
        updateGroupsCount();
    });

    // ── Pattern removed ────────────────────────────────────────────────────
    connect(m_patternManager, &mtc::PatternGroupManager::patternRemoved,
            this, [this] (mtc::MatchGroup *group, const mtc::MatchPatternConfig &removed) {
        if (group)
            ui->treeWidget_patternEditor->removePattern(group->number(),
                                                        removed.m_patternIndex);
        // Clear selection if the removed pattern was selected.
        if (group
            && m_selectedGroupIndex   == group->number()
            && m_selectedPatternIndex == removed.m_patternIndex) {
            // Fall back to group-only selection
            m_selectedPatternIndex = -1;
        }
        updateGroupsCount();
    });

    // ── Pattern changed (config edit / rename / renumber / image) ──────────
    // Fires for any successful PatternGroupManager pattern mutation.  Keep
    // the cached selection identity in sync (number may have changed) and
    // refresh the tree + thumbnail.
    connect(m_patternManager, &mtc::PatternGroupManager::patternChanged,
            this, [this] (mtc::MatchGroup *group, mtc::MatchPattern *pattern,
                          const QString &) {
        if (!group || !pattern) return;

        if (m_boundPattern == pattern) {
            m_selectedPatternIndex = pattern->number();
            updatePatternThumb(pattern);
        }

        rebuildTreeFromManager();
    });
}

/// Pulls the full library state from the manager and rebuilds the tree from scratch. Used at
/// init and after bulk re-orderings.
void LocalizationPatternsWidget::rebuildTreeFromManager() {
    if (!m_patternManager || !ui->treeWidget_patternEditor) return;

    QList<MatchGroupConfig> rows;
    for (auto group : m_patternManager->groups()) {
        if (!group) continue;
        MatchGroupConfig gr;
        gr.name   = QString::fromStdWString(group->name());
        gr.number = group->number();
        for (const auto &p : group->patterns()) {
            MatchPatternConfig pc;
            pc.name        = QString::fromStdWString(p->name());
            pc.number      = p->number();
            pc.threshScore = p->config().m_minScore;
            pc.thumbnail   = matToPixmap(p->config().m_rawImage);
            gr.patterns.append(pc);
        }
        rows.append(gr);
    }
    ui->treeWidget_patternEditor->setGroups(rows);
}

/// Connects the shared QtVariantPropertyManager's valueChanged signal to
/// onPropertyValueChanged() (group-level property edits only — pattern-level edits are
/// absorbed by m_configAdapter), then builds the initial property-browser contents.
void LocalizationPatternsWidget::wirePropertyBrowser() {
    // Manager-level valueChanged: pattern-level edits are absorbed by the
    // adapter (it ignores anything not in its key map).  This handler is
    // only for the group-level properties that we own ourselves.
    connect(m_variantManager, &QtVariantPropertyManager::valueChanged,
            this, &LocalizationPatternsWidget::onPropertyValueChanged);

    rebuildPropertyBrowser();
}

// ── Property browser construction ────────────────────────────────────────────

/// Tears down all property-browser state owned by this widget: clears the variant editor,
/// unbinds the Edge-Based config adapter, and deletes the group/common root QtVariantProperty
/// instances (their child properties go with them).
void LocalizationPatternsWidget::clearPropertyBrowserState() {
    if (m_variantEditor) {
        m_variantEditor->clear();
    }

    if (m_configAdapter) {
        m_configAdapter->bind(nullptr);
    }

    m_groupProps.clear();
    m_groupPropKeys.clear();
    delete m_groupVariant;
    m_groupVariant = nullptr;

    m_commonProps.clear();
    m_commonPropKeys.clear();
    delete m_commonVariant;
    m_commonVariant = nullptr;
}

/// Builds the "Group Settings" property-browser group from kMatchGroupSpecs, bound to
/// m_workingGroupConfig, and records each generated QtProperty's spec key in
/// m_groupPropKeys/m_groupProps for later dispatch/refresh.
void LocalizationPatternsWidget::buildGroupProperties() {
    m_groupVariant = m_variantManager->addProperty(
        QtVariantPropertyManager::groupTypeId(), tr("Group Settings"));

    m_groupPropKeys.clear();
    m_groupProps.clear();
    auto sub = PropSpecHelper::buildGroup<mtc::MatchGroupConfig>(m_variantManager, m_groupVariant,
                                          kMatchGroupSpecs, m_workingGroupConfig, m_groupPropKeys);
    for (auto it = sub.cbegin(); it != sub.cend(); ++it)
        m_groupProps[it.key()] = it.value();
}

/// Builds the "Pattern" (Common) property-browser group from kCommonSpecs, bound to
/// m_workingPatternCfg, and records each generated QtProperty's spec key in
/// m_commonPropKeys/m_commonProps for later dispatch/refresh.
void LocalizationPatternsWidget::buildCommonProperties() {
    m_commonVariant = m_variantManager->addProperty(
        QtVariantPropertyManager::groupTypeId(), tr("Pattern"));

    m_commonPropKeys.clear();
    m_commonProps.clear();
    auto sub = PropSpecHelper::buildGroup<mtc::MatchPatternConfig>(m_variantManager, m_commonVariant,
                                          kCommonSpecs, m_workingPatternCfg, m_commonPropKeys);
    for (auto it = sub.cbegin(); it != sub.cend(); ++it)
        m_commonProps[it.key()] = it.value();
}

/// Rebuilds the entire property browser from scratch: tears down the current tree, then
/// (re)builds the Group Settings block when a group is bound, the Edge-Based block via
/// m_configAdapter, and the Pattern (Common) block when a pattern is bound; mounts them in
/// Group Settings -> Edge-Based -> Pattern order and resyncs the binary-view controls.
void LocalizationPatternsWidget::rebuildPropertyBrowser() {
    if (!m_variantManager || !m_variantEditor || !m_configAdapter) return;

    // Tear down the current property tree while the shared manager is still
    // alive. The adapter, group block and common block each own only their
    // root group; child properties are released with the parent.
    clearPropertyBrowserState();

    // Group block — group settings + the shared Edge-Based algorithm config
    // (the latter via the adapter, bound to the group's typeConfig).  Shown
    // whenever a group is bound, even without a pattern selected.
    if (m_boundMatchGroup) {
        m_workingGroupConfig = m_boundMatchGroup->config();
        buildGroupProperties();
        m_configAdapter->bind(&m_workingGroupConfig);
    }

    // Pattern block — per-pattern identity + search params.
    if (m_boundPattern) {
        buildCommonProperties();
    }

    // Mount into the browser: Group Settings → Edge-Based → Pattern.
    if (m_groupVariant)
        m_variantEditor->addProperty(m_groupVariant);
    for (auto *p : m_configAdapter->rootProperties())
        m_variantEditor->addProperty(p);
    if (m_commonVariant)
        m_variantEditor->addProperty(m_commonVariant);

    // Resync the binary-view controls with the (possibly new) bound group.
    seedBinaryControlsFromConfig();
}

/// Binds `pattern` as the active pattern for the property browser: copies its config into
/// m_workingPatternCfg (or resets to a default config if `pattern` is null) and rebuilds the
/// property browser to reflect it.
/// @param pattern pattern to bind, or nullptr to clear the working pattern config
void LocalizationPatternsWidget::bindPatternToBrowser(mtc::MatchPattern *pattern) {
    m_boundPattern      = pattern;
    m_workingPatternCfg = pattern ? pattern->config() : mtc::MatchPatternConfig{};
    rebuildPropertyBrowser();
}

/// Clears the bound pattern and resets the working pattern config to its defaults, then
/// rebuilds the property browser (drops the Pattern block).
void LocalizationPatternsWidget::unbindPattern() {
    m_boundPattern = nullptr;
    m_workingPatternCfg = mtc::MatchPatternConfig{};
    rebuildPropertyBrowser();
}

// ── Selection ────────────────────────────────────────────────────────────────

/// Selects group `groupIndex` as the active group: clears any pattern selection, resolves and
/// binds the group from the pattern manager (making its config plus Edge-Based and binary
/// controls editable even without a pattern selected), rebuilds the property browser, and
/// resyncs the editor image.
/// @param groupIndex user-facing group number to select
void LocalizationPatternsWidget::selectGroup(int groupIndex) {
    m_selectedGroupIndex   = groupIndex;
    m_selectedPatternIndex = -1;
    m_boundPattern = nullptr;
    m_boundMatchGroup = nullptr;
    updatePatternThumb(nullptr);

    // Resolve and cache the selected group so its config + Edge-Based + binary
    // controls become editable even without a pattern selected.
    if (m_patternManager) {
        auto group = m_patternManager->findGroupByNumber(groupIndex);
        m_boundMatchGroup = group.get();
        if (m_boundMatchGroup)
            m_workingGroupConfig = m_boundMatchGroup->config();
    }

    rebuildPropertyBrowser();
    syncEditorImageForSelection();
}

/// Selects pattern `patternIndex` within group `groupIndex`: binds the owning group (for the
/// Group Settings and Edge-Based blocks) and the pattern itself (for the Common block), then
/// refreshes the pattern thumbnail and editor image. No-op if the group or pattern cannot be
/// resolved.
/// @param groupIndex user-facing group number containing the pattern
/// @param patternIndex user-facing pattern number to select
void LocalizationPatternsWidget::selectPattern(int groupIndex, int patternIndex) {
    m_selectedGroupIndex   = groupIndex;
    m_selectedPatternIndex = patternIndex;

    if (!m_patternManager) return;
    auto group = m_patternManager->findGroupByNumber(groupIndex);
    if (!group) return;

    auto *pattern = group->findPatternByNumber(patternIndex);
    if (pattern) {
        // build group
        m_boundMatchGroup = group.get();
        m_workingGroupConfig = m_boundMatchGroup->config();

        // build pattern
        bindPatternToBrowser(pattern);
        updatePatternThumb(pattern);
        syncEditorImageForSelection();
    }
}

/// Clears both group and pattern selection, resets the working group config, unbinds the
/// pattern, clears the thumbnail, and resyncs the editor image.
void LocalizationPatternsWidget::clearSelection() {
    m_selectedGroupIndex   = -1;
    m_selectedPatternIndex = -1;
    m_boundMatchGroup = nullptr;
    m_workingGroupConfig = mtc::MatchGroupConfig{};
    unbindPattern();
    updatePatternThumb(nullptr);
    syncEditorImageForSelection();
}

/// Refreshes the pattern-thumbnail view for `pattern`: shows "No pattern selected" and an
/// empty pixmap when `pattern` is null or has no image; otherwise renders the pick-position
/// overlay image (falling back to the raw image) and updates the caption with name, pixel
/// size, and minimum score.
/// @param pattern pattern to display, or nullptr to clear the thumbnail
void LocalizationPatternsWidget::updatePatternThumb(mtc::MatchPattern *pattern) {
    if (!pattern || pattern->isImageEmpty()) {
        m_thumbPixmap->setPixmap(QPixmap{});
        ui->label_pattern_caption->setText(tr("No pattern selected"));
        return;
    }

    cv::Mat img = pattern->getImageWithPickPosition();
    if (img.empty()) img = pattern->getRawImage();

    QPixmap pm = matToPixmap(img);
    m_thumbPixmap->setPixmap(pm);
    m_thumbScene->setSceneRect(pm.rect());
    ui->imageView_PatternThumb->fitInView(m_thumbPixmap, Qt::KeepAspectRatio);

    ui->label_pattern_caption->setText(
        tr("%1   |   %2 × %3 px   |   min score %4")
            .arg(QString::fromStdWString(pattern->name()))
            .arg(pm.width()).arg(pm.height())
            .arg(pattern->config().m_minScore, 0, 'f', 2));
}

// ── Camera combo ────────────────────────────────────────────────────
/// Rebuilds the camera combo box from the task's currently-assigned Camera-type devices,
/// preserving the previous selection (by device id) if it is still present, and enables the
/// "Trigger Camera" button only when at least one camera is assigned.
void LocalizationPatternsWidget::rebuildCameraCombo() {
    if (!m_localizeTask || !ui->comboBox_camera) return;

    QSignalBlocker blocker(ui->comboBox_camera);

    // Remember current selection so we can restore it if still present.
    const QString previousId = ui->comboBox_camera->currentData().toString();

    ui->comboBox_camera->clear();

    // Pull only camera-typed devices from the task's assigned list.
    const auto cameras =
        m_localizeTask->assignedDevicesOfType(vc::device::DeviceType::Camera);
    for (const auto &dev : cameras) {
        if (!dev) continue;
        // Display: "<name>  ·  <id>" — id kept as itemData for the trigger.
        ui->comboBox_camera->addItem(
            tr("%1").arg(dev->name()),
            dev->id());
    }

    // Restore previous selection if it still exists.
    if (!previousId.isEmpty()) {
        const int idx = ui->comboBox_camera->findData(previousId);
        if (idx >= 0) ui->comboBox_camera->setCurrentIndex(idx);
    }

    // Trigger button is meaningful only when at least one camera is assigned.
    const bool hasCamera = ui->comboBox_camera->count() > 0;
    if (ui->btn_trigger_camera)
        ui->btn_trigger_camera->setEnabled(hasCamera);
}

// ── Group combo ──────────────────────────────────────────────────────────────

/// Rebuilds the active-group combo box from the pattern manager's current groups, preserving
/// the previous selection (by group number) if still present, or selecting the first group
/// otherwise; enables "Run Match" only when at least one group exists and a test image is
/// loaded.
void LocalizationPatternsWidget::rebuildGroupCombo() {
    if (!m_patternManager) return;

    QSignalBlocker blocker(ui->comboBox_pattern_group);
    const int previous = ui->comboBox_pattern_group->currentData().toInt();

    ui->comboBox_pattern_group->clear();
    for (auto group : m_patternManager->groups()) {
        const QString name = QString::fromStdWString(group->name());
        const int     num  = group->number();
        ui->comboBox_pattern_group->addItem(
            tr("[%1]  %2").arg(num).arg(name), num);
    }

    // Restore previous selection if still present.
    int restoreIndex = ui->comboBox_pattern_group->findData(previous);
    if (restoreIndex < 0 && ui->comboBox_pattern_group->count() > 0)
        restoreIndex = 0;

    if (restoreIndex >= 0)
        ui->comboBox_pattern_group->setCurrentIndex(restoreIndex);

    // Run-match button is only meaningful with a non-empty group + image.
    const bool hasGroup = ui->comboBox_pattern_group->count() > 0;
    ui->btn_run_match->setEnabled(hasGroup && !m_currentImage.empty());
}

/// Selects the group corresponding to the newly-active combo entry.
/// @param comboIndex new current index of comboBox_pattern_group; ignored if negative
void LocalizationPatternsWidget::onActiveGroupChanged(int comboIndex) {
    if (comboIndex < 0) return;
    const int groupNumber =
        ui->comboBox_pattern_group->itemData(comboIndex).toInt();
    selectGroup(groupNumber);
}

// ── Tree click handlers ─────────────────────────────────────────────────────

/// Handles a group row click in the pattern tree: selects the group and syncs the
/// active-group combo to match (without re-triggering onActiveGroupChanged).
void LocalizationPatternsWidget::onTreeGroupClicked(int groupIndex,
                                                    const MatchGroupConfig &) {
    selectGroup(groupIndex);

    // Sync the active-group combo so user always sees what they clicked.
    int idx = ui->comboBox_pattern_group->findData(groupIndex);
    if (idx >= 0) {
        QSignalBlocker b(ui->comboBox_pattern_group);
        ui->comboBox_pattern_group->setCurrentIndex(idx);
    }
}

/// Handles a pattern row click in the pattern tree: selects the pattern and syncs the
/// active-group combo to its owning group (without re-triggering onActiveGroupChanged).
void LocalizationPatternsWidget::onTreePatternClicked(int groupIndex,
                                                      int patternIndex,
                                                      const MatchPatternConfig &) {
    selectPattern(groupIndex, patternIndex);

    int idx = ui->comboBox_pattern_group->findData(groupIndex);
    if (idx >= 0) {
        QSignalBlocker b(ui->comboBox_pattern_group);
        ui->comboBox_pattern_group->setCurrentIndex(idx);
    }
}

// ── Tree intent handlers ────────────────────────────────────────────────────

/// Prompts for a new pattern group's name/number via AddGroupDialog, validates that both are
/// unique, and adds the group through the pattern manager. Shows a warning dialog and aborts
/// on a duplicate name/number or a manager-reported failure.
void LocalizationPatternsWidget::onTreeAddGroupRequested() {
    if (!m_patternManager) return;

    QString title = tr("Input new Pattern Group information");
    AddGroupDialog dialog(title, this);
    if (dialog.exec() != QDialog::Accepted) return;

    const QString name  = dialog.getGroupName();
    const int     index = dialog.getGroupIndex();

    if (m_patternManager->containsGroupName(name)) {
        QMessageBox::warning(this, tr("Duplicated group name"),
                             tr("Pattern group name already exists."));
        return;
    }
    if (m_patternManager->containsGroupNumber(index)) {
        QMessageBox::warning(this, tr("Duplicated group index"),
                             tr("Pattern group index already exists."));
        return;
    }

    mtc::MatchGroupConfig cfg;
    cfg.m_groupName  = name.toStdWString();
    cfg.m_groupIndex = index;
    auto r = m_patternManager->addGroup(cfg);
    if (!r) {
        QMessageBox::warning(this, tr("Add group failed"),
                             resultMessage(r));
    }
}

/// Runs the 5-step Add Pattern wizard for group `groupIndex`: seeds it with the group's
/// existing pattern names/numbers (for inline uniqueness validation), forwards its
/// camera-capture requests to onTriggerCameraClicked(), and on acceptance adds the captured
/// pattern image through the pattern manager, then writes back the wizard's pick-position and
/// picking-box geometry into the newly-created pattern's config.
/// @param groupIndex group to add the new pattern to; no-op (with a warning) if not found
void LocalizationPatternsWidget::onTreeAddPatternRequested(int groupIndex) {
    if (!m_patternManager) return;

    auto group = m_patternManager->findGroupByNumber(groupIndex);
    if (!group) {
        QMessageBox::warning(this, tr("Add pattern error"),
                             tr("Group not found."));
        return;
    }
    const QString groupName = QString::fromStdWString(group->name());

    // Build "used" lists from existing patterns in the group so the wizard
    // can validate uniqueness inline.
    QStringList usedNames;
    QList<int>  usedNumbers;
    {
        std::vector<std::shared_ptr<mtc::MatchPattern>> pats = group->patterns();
        for (const auto &p : pats) {
            usedNames   << QString::fromStdWString(p->name());
            usedNumbers << p->number();
        }
    }

    // ── New 5-step Add Pattern wizard ─────────────────────────────────────
    // Replaces the old two-step flow (image dialog + simple AddPatternDialog).
    // Mirrors `ui_scratch/design_handoff_full_project/components/PatternWizard.jsx`.
    AddPatternWizard wiz(groupName, usedNames, usedNumbers, this);
    m_activeAddWizard = &wiz;
    // Forward camera-capture request from wizard → host (same plumbing as
    // the legacy AddPatternImageDialog).  The wizard's signal is parameterless;
    // adapt to the host signal by injecting the currently-selected camera id
    // from the toolbar combo.
    connect(&wiz, &AddPatternWizard::requestCameraImage,
            this, [this]() {
        // const QString cameraId = ui->comboBox_camera
        //     ? ui->comboBox_camera->currentData().toString()
        //     : QString();
        // emit requestCameraImage(cameraId);
        onTriggerCameraClicked();
    });

    const int wizResult = wiz.exec();
    m_activeAddWizard = nullptr;
    if (wizResult != QDialog::Accepted) return;

    cv::Mat patternImage = wiz.patternImage();
    if (patternImage.empty()) return;

    // // Apply crop if user did not check "use original frame"
    // if (!wiz.keepOriginal()) {
    //     const QRect r = wiz.cropRect();
    //     if (r.width() > 0 && r.height() > 0
    //         && r.x() >= 0 && r.y() >= 0
    //         && r.x() + r.width()  <= patternImage.cols
    //         && r.y() + r.height() <= patternImage.rows)
    //     {
    //         patternImage = patternImage(cv::Rect(r.x(), r.y(),
    //                                              r.width(), r.height())).clone();
    //     }
    // }

    mtc::MatchPatternConfig cfg;
    cfg.m_patternName  = wiz.patternName().toStdWString();
    cfg.m_patternIndex = wiz.patternNumber();
    cfg.m_rawImage     = patternImage.clone();

    auto r = m_patternManager->addPattern(groupName, cfg);
    if (!r) {
        QMessageBox::warning(this, tr("Add pattern failed"),
                             resultMessage(r));
        return;
    }

    // Push pick position + picking-box config back into the freshly created
    // pattern.  Picking box is per-pattern (Edge-Based collision geometry).
    if (auto *pat = group->findPatternByNumber(wiz.patternNumber())) {
        auto pcfg = pat->config();
        pcfg.m_pickPosition = cv::Point2f(static_cast<float>(wiz.pickX()),
                                           static_cast<float>(wiz.pickY()));
        pcfg.m_pickingBoxSize.width  = static_cast<float>(wiz.pickBoxW());
        pcfg.m_pickingBoxSize.height = static_cast<float>(wiz.pickBoxH());
        pcfg.m_pickingBoxDistance    = wiz.pickBoxDist();
        pcfg.m_pickingBoxAngle       = wiz.pickBoxAngle();
        pat->setConfig(pcfg);
    }
}

// ── Edit Pattern Wizard entry points ────────────────────────────────────────

/// Opens the Edit Pattern Wizard for pattern `patternNumber` in group `groupNumber`: prefills
/// it from the pattern's current name/pick-position/picking-box config, then — on acceptance —
/// commits the edited name/number/pick-position/picking-box back through the pattern manager
/// (the raw image itself is left untouched; the Edit wizard locks it).
/// @param groupNumber user-facing group number containing the pattern
/// @param patternNumber user-facing pattern number to edit
bool LocalizationPatternsWidget::editPattern(int groupNumber, int patternNumber) {
    if (!m_patternManager) return false;

    auto group = m_patternManager->findGroupByNumber(groupNumber);
    if (!group) return false;

    auto *pat = group->findPatternByNumber(patternNumber);
    if (!pat) return false;

    const QString groupName = QString::fromStdWString(group->name());

    // Build "used" lists excluding the pattern being edited so its current
    // name/number stays valid.
    QStringList usedNames;
    QList<int>  usedNumbers;
    {
        std::vector<std::shared_ptr<mtc::MatchPattern>> pats = group->patterns();
        for (const auto &p : pats) {
            if (p->number() == patternNumber) continue;
            usedNames   << QString::fromStdWString(p->name());
            usedNumbers << p->number();
        }
    }

    // Prefill from the current saved state
    EditPatternWizard::Pattern initial;
    {
        const auto &pcfg = pat->config();
        initial.name         = QString::fromStdWString(pat->name());
        initial.number       = pat->number();
        initial.pickX        = static_cast<int>(pcfg.m_pickPosition.x);
        initial.pickY        = static_cast<int>(pcfg.m_pickPosition.y);
        initial.pickBoxW     = pcfg.m_pickingBoxSize.width;
        initial.pickBoxH     = pcfg.m_pickingBoxSize.height;
        initial.pickBoxDist  = pcfg.m_pickingBoxDistance;
        initial.pickBoxAngle = pcfg.m_pickingBoxAngle;
        initial.image        = pcfg.m_rawImage.clone();
    }

    EditPatternWizard wiz(groupName, initial, usedNames, usedNumbers, this);
    if (wiz.exec() != QDialog::Accepted) return false;

    const auto out = wiz.result();

    // Re-fetch by old number in case the user changed it.
    auto *editTarget = group->findPatternByNumber(patternNumber);
    if (!editTarget) return false;

    const QString currentName = QString::fromStdWString(editTarget->name());

    // Apply pattern-level changes — identity, pick position, and the
    // per-pattern picking-box geometry.  The raw image is left untouched
    // (the Edit wizard locks it).
    auto pcfg = editTarget->config();
    pcfg.m_patternName  = out.name.toStdWString();
    pcfg.m_patternIndex = out.number;
    pcfg.m_pickPosition = cv::Point2f(static_cast<float>(out.pickX),
                                       static_cast<float>(out.pickY));
    pcfg.m_pickingBoxSize.width  = static_cast<float>(out.pickBoxW);
    pcfg.m_pickingBoxSize.height = static_cast<float>(out.pickBoxH);
    pcfg.m_pickingBoxDistance    = out.pickBoxDist;
    pcfg.m_pickingBoxAngle       = out.pickBoxAngle;

    // Commit through the manager so the rename/renumber is validated and a
    // patternChanged signal fires — that refreshes the tree row (name, number,
    // thumbnail) and the bound property browser / thumbnail.
    auto r = m_patternManager->setPatternConfig(groupName, currentName, pcfg);
    if (!r) {
        QMessageBox::warning(this, tr("Edit pattern failed"), resultMessage(r));
        return false;
    }

    return true;
}

/// Opens the Edit Pattern Wizard for the currently-selected pattern; shows an informational
/// message and does nothing if no pattern is selected.
void LocalizationPatternsWidget::editSelectedPattern() {
    if (m_selectedGroupIndex < 0 || m_selectedPatternIndex < 0) {
        QMessageBox::information(this, tr("Edit pattern"),
                                  tr("Select a pattern in the library first."));
        return;
    }
    editPattern(m_selectedGroupIndex, m_selectedPatternIndex);
}

/// Confirms with the user, then removes group `groupIndex` (and all its patterns) through the
/// pattern manager; on success, mirrors the removal in the tree widget and clears the
/// selection if the removed group was selected.
/// @param groupIndex user-facing group number to delete
void LocalizationPatternsWidget::onTreeDeleteGroupRequested(int groupIndex) {
    if (!m_patternManager) return;

    auto group = m_patternManager->findGroupByNumber(groupIndex);
    if (!group) return;

    const QString name = QString::fromStdWString(group->name());
    auto reply = QMessageBox::question(
        this, tr("Delete group"),
        tr("Delete group \"%1\" and all its patterns?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    auto r = m_patternManager->removeGroup(name);
    if (!r) {
        QMessageBox::warning(this, tr("Delete group failed"),
                             resultMessage(r));
        return;
    }

    // Mirror the removal in the tree widget.
    // removeGroup() takes the user-defined group number (mapped internally).
    ui->treeWidget_patternEditor->removeGroup(groupIndex);

    if (m_selectedGroupIndex == groupIndex) clearSelection();
}

/// Confirms with the user, then removes pattern `patternIndex` from group `groupIndex` through
/// the pattern manager; on success, locates and removes the matching row in the tree widget
/// (by pattern number, since the tree indexes patterns by list position) and clears the
/// pattern selection if it was the one removed.
/// @param groupIndex user-facing group number containing the pattern
/// @param patternIndex user-facing pattern number to delete
void LocalizationPatternsWidget::onTreeDeletePatternRequested(int groupIndex,
                                                              int patternIndex) {
    if (!m_patternManager) return;

    auto group = m_patternManager->findGroupByNumber(groupIndex);
    if (!group) return;

    auto *pattern = group->findPatternByNumber(patternIndex);
    if (!pattern) return;

    const QString groupName = QString::fromStdWString(group->name());
    const QString patName   = QString::fromStdWString(pattern->name());

    auto reply = QMessageBox::question(
        this, tr("Delete pattern"),
        tr("Delete pattern \"%1\" from group \"%2\"?").arg(patName, groupName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    auto r = m_patternManager->removePattern(groupName, patName);
    if (!r) {
        QMessageBox::warning(this, tr("Delete pattern failed"),
                             resultMessage(r));
        return;
    }

    // Mirror the removal in the tree widget.
    // removePattern() takes user group number + pattern list index.
    auto groups = ui->treeWidget_patternEditor->groups();
    for (int gi = 0; gi < groups.size(); ++gi) {
        if (groups[gi].number != groupIndex) continue;
        for (int pi = 0; pi < groups[gi].patterns.size(); ++pi) {
            if (groups[gi].patterns[pi].number == patternIndex) {
                ui->treeWidget_patternEditor->removePattern(groupIndex, pi);
                break;
            }
        }
        break;
    }

    if (m_selectedGroupIndex == groupIndex
            && m_selectedPatternIndex == patternIndex) {
        unbindPattern();
        updatePatternThumb(nullptr);
        m_selectedPatternIndex = -1;
    }
}

// Tree-state change notifications: used for live rename / renumber from the
// tree's inline editors.  We mirror those into the manager.

/// Currently unhandled: bulk group-list change notifications from the tree's inline editors
/// are not yet mirrored into the pattern manager.
void LocalizationPatternsWidget::onTreeGroupsChanged(
        const QList<MatchGroupConfig> &) {

}

/// Live-rename notification from the tree's inline editor for group `groupIndex`. Not yet
/// wired to the pattern manager — see the in-body note for why.
void LocalizationPatternsWidget::onTreeGroupChanged(int /*groupIndex*/,
                                                    const MatchGroupConfig &) {
    // Not wiring rename here yet — manager update would need both old and
    // new names.  Tree widget exposes only the new state, so a future
    // enhancement is to track previous name per row.
}

/// Live-rename/renumber notification from the tree's inline editor for a pattern. Not yet
/// wired to the pattern manager (same caveat as onTreeGroupChanged()).
void LocalizationPatternsWidget::onTreePatternChanged(int /*groupIndex*/,
                                                      int /*patternIndex*/,
                                                      const MatchPatternConfig &) {
    // Same caveat as above.
}

// --
/// Camera single-shot completion handler: forwards the grabbed frame into setCameraImage() on
/// success; ignored on grab failure.
void LocalizationPatternsWidget::onCameraGrabFinished(vc::device::GrabResult result) {
    if (result.isGrabSuccess) {
        this->setCameraImage(result.frame);
    }
}


// ── Toolbar ─────────────────────────────────────────────────────────────────

/// Requests a single-shot grab from the active camera's runner: validates a camera is
/// selected, the task is in Commission state, and the runner resolves to a CameraRunner;
/// connects a one-shot handler (onCameraGrabFinished) before issuing the request.
void LocalizationPatternsWidget::onTriggerCameraClicked() {
    // Should not normally fire when no camera is assigned — the Trigger
    // button is disabled via rebuildCameraCombo() in that case.  Guard
    // anyway for the race where the assignment changes between the click
    // hit-test and this slot running.
    const QString cameraId = ui->comboBox_camera
        ? ui->comboBox_camera->currentData().toString()
        : QString();

    if (cameraId.isEmpty()) {
        setState(State::Warning, tr("No camera selected."));
        return;
    }

    if (m_localizeTask->taskState() != vc::model::TaskState::Commission) {
        setState(State::Warning, tr("Task not in Commission state."));
        return;
    }

    vc::runtime::IDeviceRunner *runner = m_localizeTask->taskRunner()->runnerFor(cameraId);
    if (!runner) {
        setState(State::Warning, tr("Camera device ID invalid."));
        return;
    }

    vc::runtime::CameraRunner *cam_runner = qobject_cast<vc::runtime::CameraRunner *>(runner);
    if (!cam_runner) {
        setState(State::Warning, tr("Camera device ID invalid (Device ID %1 is not camera device).")
                                     .arg(cameraId));
        return;
    }

    connect(cam_runner, &vc::runtime::CameraRunner::grabFinished,
            this, &LocalizationPatternsWidget::onCameraGrabFinished, Qt::SingleShotConnection);
    cam_runner->requestSingleShot();

    setState(State::Busy, tr("Waiting for camera image…"));
    // emit requestCameraImage(cameraId);
}

/// Prompts for an image file via a file dialog, loads it with cv::imread (unchanged, i.e.
/// preserving channel count/alpha), and feeds it into the widget as the current test image via
/// setCameraImage(). Shows an error status message on load failure.
void LocalizationPatternsWidget::onChooseImageClicked() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open test image"),
        {}, tr("Images (*.png *.bmp *.jpg *.jpeg *.tif *.tiff)"));
    if (path.isEmpty()) return;

    cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        setState(State::Error, tr("Failed to load image."));
        return;
    }

    setCameraImage(img);
    ui->label_status_image->setText(
        tr("Image: %1  ·  %2 × %3")
            .arg(QFileInfo(path).fileName())
            .arg(img.cols).arg(img.rows));
}

/// Returns the device id stored as itemData for the current camera-combo selection, or an
/// empty string if there is no combo or no selection.
QString LocalizationPatternsWidget::activeCameraId() const {
    return ui->comboBox_camera ? ui->comboBox_camera->currentData().toString()
                               : QString();
}

/// Refreshes the workspace/condition ROI overlay on the raw preview from the active camera's
/// workspace config, and — if the result view is currently showing a previous match result —
/// redraws that overlay too (ROI visibility affects it). Falls back to clearing the legacy
/// imageView_Raw ROI when the VisionCanvas preview is not installed.
void LocalizationPatternsWidget::updateWorkspaceRoiOverlay() {
    const QVector<VisionRoi> overlays = buildWorkspaceOverlayRois();
    if (m_rawPreview) {
        m_rawPreview->setAuxiliaryRois(overlays);
    }

    if (m_hasLastMatchResult
        && m_resultViewer
        && ui->stackedWidget_ImageView->currentIndex() == kMonitorPageResult) {
        displayResultOverlay(m_currentImage, m_lastMatchResult);
    }

    if (m_rawPreview || !ui->imageView_Raw) return;
    ui->imageView_Raw->removeAllROI();
}

/// Toggles the active camera's workspace `useWorkspace` flag and persists it into the task's
/// TaskLocalizeConfig; refreshes the ROI overlay to reflect the new use/muted color. No-op if
/// there is no active task/camera or the flag already equals `enabled`.
/// @param enabled new "Use ROI" state from the checkbox
void LocalizationPatternsWidget::onUseRoiToggled(bool enabled) {
    if (!m_localizeTask) return;

    const QString camId = activeCameraId();
    if (camId.isEmpty()) return;

    vc::model::TaskLocalizeConfig cfg = m_localizeTask->taskLocalizeConfig();
    vc::model::CameraWorkspace ws = cfg.cameraWorkspace(camId);
    if (ws.useWorkspace == enabled) return;   // no change

    ws.useWorkspace = enabled;
    cfg.setCameraWorkspace(camId, ws);
    m_localizeTask->setTaskLocalizeConfig(cfg);
    m_config = cfg;

    // Overlay colour reflects the use state.
    updateWorkspaceRoiOverlay();
}

/// Reflects the active camera's workspace `useWorkspace` flag into the "Use ROI" checkbox,
/// blocking signals so the read-back does not re-trigger onUseRoiToggled().
void LocalizationPatternsWidget::syncUseRoiCheckbox() {
    if (!ui->cbx_use_match_area) return;

    bool useWs = false;
    const QString camId = activeCameraId();
    if (m_localizeTask && !camId.isEmpty())
        useWs = m_localizeTask->taskLocalizeConfig().cameraWorkspace(camId).useWorkspace;

    QSignalBlocker block(ui->cbx_use_match_area);
    ui->cbx_use_match_area->setChecked(useWs);
}

/// Opens WorkspaceSettingDialog for the active camera, seeded with its current reference
/// image and working/condition ROIs; wires the dialog's "grab from camera" request to a
/// single-shot camera grab. On acceptance, writes the resulting ROIs (presence of a positive
/// width/height implies "use it") and reference image back into the task's
/// TaskLocalizeConfig, then resyncs the "Use ROI" checkbox and ROI overlay.
void LocalizationPatternsWidget::onSetWorkspaceClicked() {
    if (!m_localizeTask) return;

    const QString camId = activeCameraId();
    if (camId.isEmpty()) {
        setState(State::Warning, tr("Select a camera first."));
        return;
    }

    vc::model::TaskLocalizeConfig cfg = m_localizeTask->taskLocalizeConfig();
    vc::model::CameraWorkspace ws = cfg.cameraWorkspace(camId);

    WorkspaceSettingDialog dialog(this);
    dialog.setTitleText(tr("Workspace — %1").arg(ui->comboBox_camera->currentText()));
    dialog.setInitial(
        matToPixmap(ws.referenceImage),
        QRectF(ws.roi.x, ws.roi.y, ws.roi.width, ws.roi.height),
        QRectF(ws.conditionRoi.x, ws.conditionRoi.y,
               ws.conditionRoi.width, ws.conditionRoi.height));

    // Serve "Grab from camera": grab a single shot through the camera runner.
    connect(&dialog, &WorkspaceSettingDialog::requestGrab, &dialog,
            [this, camId, &dialog]() {
        auto *runner = m_localizeTask->taskRunner()
            ? qobject_cast<vc::runtime::CameraRunner *>(
                  m_localizeTask->taskRunner()->runnerFor(camId))
            : nullptr;
        if (!runner) {
            QMessageBox::warning(&dialog, tr("Workspace"),
                tr("Camera is not available. Start Commission and connect the camera first."));
            return;
        }
        connect(runner, &vc::runtime::CameraRunner::grabFinished, &dialog,
                [&dialog](vc::device::GrabResult result) {
            if (result.isGrabSuccess && !result.frame.empty())
                dialog.setMainViewImage(LocalizationPatternsWidget::matToPixmap(result.frame));
            else
                QMessageBox::warning(&dialog, QObject::tr("Workspace"),
                                     QObject::tr("Camera grab failed."));
        }, Qt::SingleShotConnection);
        runner->requestSingleShot();
    });

    if (dialog.exec() != QDialog::Accepted || !dialog.hasResult())
        return;

    // Both ROIs come back from the shared dialog; presence implies "use it".
    const QRectF r  = dialog.resultRoi();
    const QRectF cr = dialog.resultConditionRoi();
    const bool hasWorking   = r.width()  > 0.0 && r.height()  > 0.0;
    const bool hasCondition = cr.width() > 0.0 && cr.height() > 0.0;

    ws.roi = cv::Rect2f(static_cast<float>(r.x()), static_cast<float>(r.y()),
                        static_cast<float>(r.width()), static_cast<float>(r.height()));
    ws.useWorkspace = hasWorking;

    ws.conditionRoi = cv::Rect2f(static_cast<float>(cr.x()), static_cast<float>(cr.y()),
                                 static_cast<float>(cr.width()), static_cast<float>(cr.height()));
    ws.useConditionWorkspace = hasCondition;

    ws.referenceImage = pixmapToMat(dialog.resultImage());
    cfg.setCameraWorkspace(camId, ws);
    m_localizeTask->setTaskLocalizeConfig(cfg);
    m_config = cfg;           // keep the local snapshot in sync

    syncUseRoiCheckbox();     // useWorkspace reflects whether a working ROI exists
    updateWorkspaceRoiOverlay();
    setState(State::Idle, tr("Workspace updated."));
}

/// Switches the monitor to the Raw page and refreshes its ROI overlay.
void LocalizationPatternsWidget::onShowRawView() {
    setMonitorPage(kMonitorPageRaw);
    updateWorkspaceRoiOverlay();
}

/// Switches the monitor to the Result page, redrawing the last match-result overlay if one
/// exists.
void LocalizationPatternsWidget::onShowResultView() {
    if (m_hasLastMatchResult) {
        displayResultOverlay(m_currentImage, m_lastMatchResult);
    }
    setMonitorPage(kMonitorPageResult);
}

/// Switches the monitor to the Binary page and (re)renders the binarized preview.
void LocalizationPatternsWidget::onShowBinaryView() {
    setMonitorPage(kMonitorPageBinary);
    displayBinaryImage();
}

/// Reacts to any binary-view control change (auto toggle, threshold slider/spin, maxValue
/// spin): enables/disables the threshold controls per auto-mode, persists the threshold and
/// maxValue onto the bound group's shared EdgeMatchConfig (auto mode stores threshold as -1),
/// refreshes the Edge-Based property-browser mirror on successful commit, and re-renders the
/// binarized preview. No persistence happens when no group is bound.
void LocalizationPatternsWidget::onBinaryThresholdChanged() {
    const bool autoMode = ui->chk_binary_auto->isChecked();
    const bool hasGroup = (m_boundMatchGroup != nullptr);

    // Threshold slider/spin are meaningful only in manual mode; maxValue and
    // the auto toggle are usable whenever a group is bound.
    ui->sld_binary_threshold->setEnabled(hasGroup && !autoMode);
    ui->spn_binary_threshold->setEnabled(hasGroup && !autoMode);

    // Persist onto the group's shared EdgeMatchConfig (engine wiring is
    // deferred — Preview-first).  No-op when no group is selected.
    if (hasGroup) {
        if (auto *ecfg = m_workingGroupConfig.edgeConfig()) {
            ecfg->binaryThreshold = autoMode ? -1 : ui->sld_binary_threshold->value();
            ecfg->binaryMaxValue  = ui->spn_binary_maxvalue->value();
            if (commitWorkingGroupConfig())
                m_configAdapter->refresh();   // mirror into the Edge-Based group
        }
    }

    displayBinaryImage();
}

/// Runs a matching test pass on the current test image against the selected group: shows a
/// warning and returns early if no image is loaded or no group is selected; otherwise sets a
/// "Matching..." busy state, flushes the state-pill repaint, and starts the test via
/// runMatchingTest(). Shows an error state if the test could not be started.
void LocalizationPatternsWidget::onRunMatchingClicked() {
    if (m_currentImage.empty()) {
        setState(State::Warning, tr("No image loaded."));
        return;
    }
    if (m_selectedGroupIndex < 0) {
        setState(State::Warning, tr("Select an active group first."));
        return;
    }

    setState(State::Busy, tr("Matching…"));
    QApplication::processEvents();   // flush state-pill repaint

    mtc::MatchResult result;
    if (!runMatchingTest(result)) {
        setState(State::Error, tr("Matching failed (see log)."));
        return;
    }

}

// ── External image input ─────────────────────────────────────────────────────

/// External entry point for image input (camera, file, network, or the toolbar's
/// choose-image/trigger-camera actions). Routes the image to whichever consumer currently
/// wants it: the active Add Pattern wizard, the legacy AddPatternImageDialog if visible, or
/// — as the default case — stores it as the current test image and refreshes the raw/binary
/// views, ROI overlay, and status label. Shows an error state on an empty image.
/// @param image captured/loaded frame; ignored (with an error state) if empty
void LocalizationPatternsWidget::setCameraImage(const cv::Mat &image) {
    if (image.empty()) {
        setState(State::Error, tr("Received empty image."));
        return;
    }

    // Route the image to whoever asked for it.

    // 1. Active Add Pattern wizard takes precedence over everything else —
    //    the user explicitly requested capture from inside the wizard.
    if (m_activeAddWizard) {
        m_activeAddWizard->setCameraImage(image);
        return;
    }

    // 2. Legacy AddPatternImage dialog (used by older flows / external code).
    if (m_addPatternImageDialog && m_addPatternImageDialog->isVisible()) {
        m_addPatternImageDialog->setMainViewImage(matToPixmap(image));
        return;
    }

    m_currentImage = image.clone();
    syncEditorImageForSelection();
    syncUseRoiCheckbox();
    updateWorkspaceRoiOverlay();
    displayBinaryImage();
    setMonitorPage(kMonitorPageRaw);

    ui->label_status_image->setText(
        tr("Image: %1 × %2 px").arg(image.cols).arg(image.rows));

    // Enable matching now that we have an image (and at least one group).
    ui->btn_run_match->setEnabled(ui->comboBox_pattern_group->count() > 0);

    setState(State::Idle, tr("Image ready."));
}

/// Installs the VisionCanvas (raw view) and VisionResultViewerWidget (result view) in place of
/// the designer-placed plain image-view widgets, and wires the result viewer's
/// object-selection signal to syncResultSelectionFromViewer(). No-op if already installed.
void LocalizationPatternsWidget::installVisionWidgets()
{
    if (m_rawPreview || m_resultViewer) return;

    m_rawPreview = new VisionCanvas(this);
    m_resultViewer = new VisionResultViewerWidget(this);

    m_rawPreview->setReadOnly(true);
    m_rawPreview->setToolMode(VisionToolPalette::ToolMode::Pan);

    replacePageWidget(ui->hl_page_raw, ui->imageView_Raw, m_rawPreview);
    replacePageWidget(ui->hl_page_result, ui->imageView_Result, m_resultViewer);
    connect(m_resultViewer, &VisionResultViewerWidget::resultObjectSelectionChanged,
            this, &LocalizationPatternsWidget::syncResultSelectionFromViewer);
}

/// Refreshes the raw-view image to reflect the current selection: shows the loaded test image
/// if one exists, else the selected pattern's raw image if bound, else clears the raw preview.
void LocalizationPatternsWidget::syncEditorImageForSelection()
{
    if (!m_currentImage.empty()) {
        displayRawImage(m_currentImage);
        return;
    }

    if (m_boundPattern && !m_boundPattern->config().m_rawImage.empty()) {
        displayRawImage(m_boundPattern->config().m_rawImage);
        return;
    }

    if (m_rawPreview) {
        m_rawPreview->clearImage();
    }
}

/// Builds the VisionRoi overlays for the active camera's workspace and condition ROIs, colored
/// by whether each is currently in use (accent/info color) or only defined (muted color).
/// Returns an empty list if there is no task, no test image loaded, or the "Show ROI" checkbox
/// is unchecked; ROIs with non-positive width/height are omitted.
QVector<VisionRoi> LocalizationPatternsWidget::buildWorkspaceOverlayRois() const
{
    QVector<VisionRoi> rois;
    if (!m_localizeTask) return rois;
    if (m_currentImage.empty()) return rois;
    if (ui->cbx_show_match_area && !ui->cbx_show_match_area->isChecked()) return rois;

    const QString camId = activeCameraId();
    if (camId.isEmpty()) return rois;

    const vc::model::CameraWorkspace ws =
        m_localizeTask->taskLocalizeConfig().cameraWorkspace(camId);

    auto appendRoi = [&rois](const QString &id,
                             const QString &label,
                             const cv::Rect2f &rect,
                             const QColor &color) {
        if (rect.width <= 0.0f || rect.height <= 0.0f) return;
        VisionRoi roi;
        roi.id = id;
        roi.label = label;
        roi.shape = VisionRoiShape::AxisAlignedRect;
        roi.center = QPointF(rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f);
        roi.size = QSizeF(rect.width, rect.height);
        roi.editable = false;
        roi.visible = true;
        roi.color = color;
        rois.append(roi);
    };

    if (ws.hasRoi()) {
        appendRoi(QStringLiteral("workspace"),
                  QStringLiteral("Workspace ROI"),
                  ws.roi,
                  ws.useWorkspace ? themeTokenColor(QStringLiteral("accent.primary"))
                                  : themeTokenColor(QStringLiteral("text.muted")));
    }

    if (ws.hasConditionRoi()) {
        appendRoi(QStringLiteral("condition"),
                  QStringLiteral("Condition ROI"),
                  ws.conditionRoi,
                  ws.useConditionWorkspace ? themeTokenColor(QStringLiteral("state.info"))
                                           : themeTokenColor(QStringLiteral("text.muted")));
    }

    return rois;
}

// ── Property browser change handlers ─────────────────────────────────────────

/// Commits m_workingPatternCfg (pattern "Common" params) through the pattern manager so the
/// live MatchPattern picks up the change; re-resolves m_boundPattern by the (possibly new)
/// name afterward.
bool LocalizationPatternsWidget::commitWorkingPatternConfig() {
    // Commit m_workingPatternCfg (pattern "Common" params) through the manager
    // so the live MatchPattern picks up the change.
    if (!m_patternManager || !m_boundPattern) return false;
    if (m_selectedGroupIndex < 0)              return false;

    auto group = m_patternManager->findGroupByNumber(m_selectedGroupIndex);
    if (!group) return false;

    const QString groupName  = QString::fromStdWString(group->name());
    const QString patternKey = QString::fromStdWString(m_boundPattern->name());

    auto r = m_patternManager->setPatternConfig(
        groupName, patternKey, m_workingPatternCfg);
    if (!r) {
        QMessageBox::warning(this, tr("Update failed"), resultMessage(r));

        // Roll the working copy back to the live config and refresh the Common
        // editors.  PropSpecHelper::refresh blocks manager signals internally.
        m_workingPatternCfg = m_boundPattern->config();
        PropSpecHelper::refresh(m_variantManager, kCommonSpecs,
                                m_workingPatternCfg, m_commonProps);
        return false;
    }

    // Re-resolve in case the rename/renumber changed identity.  The tree
    // row and pattern thumb are refreshed by the patternChanged listener.
    m_boundPattern = group->findPatternByName(
        m_workingPatternCfg.m_patternName);
    return true;
}

/// Commits m_workingGroupConfig through the pattern manager (shared by the Group Settings and
/// Edge-Based property groups plus the binary-view controls), using the group's current live
/// name as the lookup key (a rename may already be staged in the working copy).
bool LocalizationPatternsWidget::commitWorkingGroupConfig() {
    // Commit m_workingGroupConfig through the manager.  Shared by the Group
    // Settings + Edge-Based property groups and the binary-view controls.
    if (!m_patternManager || !m_boundMatchGroup) return false;

    // Snapshot the current name — a dispatch may already have written a new
    // name into m_workingGroupConfig, but the live MatchGroup still carries the
    // old name, which is the lookup key for setGroupConfig.
    const QString oldName = QString::fromStdWString(m_boundMatchGroup->name());

    auto r = m_patternManager->setGroupConfig(oldName, m_workingGroupConfig);
    if (!r) {
        QMessageBox::warning(this, tr("Update failed"), resultMessage(r));

        // Roll back to the live state and refresh every editor bound to the
        // group config.  refresh() blocks manager signals internally.
        m_workingGroupConfig = m_boundMatchGroup->config();
        PropSpecHelper::refresh(m_variantManager, kMatchGroupSpecs,
                                m_workingGroupConfig, m_groupProps);
        if (m_configAdapter) m_configAdapter->refresh();
        seedBinaryControlsFromConfig();
        return false;
    }

    // Success.  Tree row, combo, and m_selectedGroupIndex are refreshed by the
    // PatternGroupManager::groupChanged listener (wireManagerSignals).
    return true;
}

/// Handles an Edge-Based property edit forwarded by m_configAdapter (which has already written
/// into m_workingGroupConfig's typeConfig): commits the change, resyncs the binary-view
/// controls (the edit may have touched binaryThreshold/binaryMaxValue), and re-renders the
/// binary preview if it is the page currently shown.
void LocalizationPatternsWidget::onGroupTypeConfigModified() {
    // Edge-Based edit — the adapter has already written to m_workingGroupConfig's
    // typeConfig.  Commit, then keep the binary-view controls in sync (the edit
    // may have touched binaryThreshold / binaryMaxValue).
    if (!commitWorkingGroupConfig()) return;

    seedBinaryControlsFromConfig();
    if (ui->stackedWidget_ImageView->currentIndex() == kMonitorPageBinary) {
        displayBinaryImage();
    }
}

/// Dispatches a property-browser value change to the matching config: Group Settings edits
/// (key found in m_groupPropKeys) are written into m_workingGroupConfig and committed via
/// commitWorkingGroupConfig(); Pattern "Common" edits (key found in m_commonPropKeys) are
/// written into m_workingPatternCfg and committed via commitWorkingPatternConfig(). Edge-Based
/// edits are not handled here — they flow through the adapter to onGroupTypeConfigModified().
/// @param property the QtProperty whose value changed
/// @param value the new value to dispatch via PropSpecHelper
void LocalizationPatternsWidget::onPropertyValueChanged(QtProperty *property,
                                                        const QVariant &value) {
    // Group Settings edits.  (Edge-Based edits flow through the adapter →
    // onGroupTypeConfigModified(); their keys are not in m_groupPropKeys.)
    if (m_groupPropKeys.contains(property)) {
        if (!m_patternManager || !m_boundMatchGroup) return;
        const QString key = m_groupPropKeys.value(property);
        if (key.isEmpty()) return;
        if (!PropSpecHelper::dispatch(kMatchGroupSpecs, key, value, m_workingGroupConfig))
            return;
        commitWorkingGroupConfig();
        return;
    }

    // Pattern "Common" edits.
    if (m_commonPropKeys.contains(property)) {
        if (!m_patternManager || !m_boundPattern) return;
        const QString key = m_commonPropKeys.value(property);
        if (key.isEmpty()) return;
        if (!PropSpecHelper::dispatch(kCommonSpecs, key, value, m_workingPatternCfg))
            return;
        commitWorkingPatternConfig();
        return;
    }
}

// ── Matching commission ─────────────────────────────────────────────

/// Handles the asynchronous completion of a commissioned matching run: caches the result,
/// updates the KPI strip and result table, redraws the result overlay, switches to the Result
/// page, and sets the status pill/message based on whether a match was found and whether the
/// matched area is below the configured limit.
/// @param result the completed matching pass's result
void LocalizationPatternsWidget::onMatchingCommissionDone(mtc::MatchResult result) {
    m_lastMatchResult = result;
    m_hasLastMatchResult = true;
    applyKpis(result);
    populateResultTable(result);     // ← fill the new result-table widget
    displayResultOverlay(m_currentImage, result);
    setMonitorPage(kMonitorPageResult);

    if (!result.isFoundMatchObject) {
        setState(State::Warning, tr("No match found."));
    } else if (result.isAreaLessThanLimits) {
        setState(State::Warning,
                 tr("Found %1 — material area below limit.")
                     .arg(result.totalPossiblePicking));
    } else {
        setState(State::Success,
                 tr("Found %1 object(s) in %2 ms")
                     .arg(result.totalPossiblePicking)
                     .arg(result.ExecutionTime, 0, 'f', 1));
    }
}

// ── Status / KPIs ────────────────────────────────────────────────────────────

/// Updates the status pill's text/color role and the status message label.
/// @param state new pill state (drives both the label text and its "pillState" style property)
void LocalizationPatternsWidget::setState(State state, const QString &message) {
    static const QHash<State, QString> kLabel = {
        { State::Idle,    QStringLiteral("● IDLE")    },
        { State::Busy,    QStringLiteral("● BUSY")    },
        { State::Success, QStringLiteral("● OK")      },
        { State::Warning, QStringLiteral("● WARNING") },
        { State::Error,   QStringLiteral("● ERROR")   },
    };
    static const QHash<State, QString> kRole = {
        { State::Idle,    QStringLiteral("idle")    },
        { State::Busy,    QStringLiteral("busy")    },
        { State::Success, QStringLiteral("success") },
        { State::Warning, QStringLiteral("warning") },
        { State::Error,   QStringLiteral("error")   },
    };

    ui->label_state_pill->setText(kLabel.value(state));
    ui->label_state_pill->setProperty("pillState", kRole.value(state, QStringLiteral("idle")));
    ui->label_state_pill->style()->unpolish(ui->label_state_pill);
    ui->label_state_pill->style()->polish(ui->label_state_pill);
    ui->label_state_pill->update();

    ui->label_status_text->setText(message.isEmpty() ? tr("Ready") : message);
}

/// Refreshes the "Groups: N - Patterns: M" status label from the pattern manager's current
/// group/pattern counts; shows zero counts when there is no pattern manager.
void LocalizationPatternsWidget::updateGroupsCount() {
    if (!m_patternManager) {
        ui->label_status_groups->setText(tr("Groups: 0  ·  Patterns: 0"));
        return;
    }
    int totalPatterns = 0;
    for (auto g : m_patternManager->groups())
        totalPatterns += g->patternCount();
    ui->label_status_groups->setText(
        tr("Groups: %1  ·  Patterns: %2")
            .arg(m_patternManager->groupCount()).arg(totalPatterns));
}

/// Resets the KPI strip labels (result count, execution time, below-limit flag) to placeholder
/// dashes and clears the result table.
void LocalizationPatternsWidget::resetKpis() {
    ui->label_match_result_num->setText("—");
    ui->label_match_execution_time->setText("—");
    ui->label_match_lessthanlimit->setText("—");
    clearResultTable();
}

/// Populates the KPI strip labels from a completed match result: total object count, possible-
/// picking count, execution time (ms), and whether the matched area is below the configured
/// limit.
/// @param result the completed matching pass's result
void LocalizationPatternsWidget::applyKpis(const mtc::MatchResult &result) {
    ui->label_match_total_objects->setText(
        QString::number(result.Objects.size()));
    ui->label_match_result_num->setText(
        QString::number(result.totalPossiblePicking));
    ui->label_match_execution_time->setText(
        tr("%1 ms").arg(result.ExecutionTime, 0, 'f', 1));
    ui->label_match_lessthanlimit ->setText(
        result.isAreaLessThanLimits ? tr("YES") : tr("NO"));
}

// ── Image display ────────────────────────────────────────────────────────────

/// Displays `image` in the raw-view: routes through the VisionCanvas preview (with the
/// workspace ROI overlay refreshed) when installed, else falls back to the legacy
/// imageView_Raw widget.
/// @param image image to display
void LocalizationPatternsWidget::displayRawImage(const cv::Mat &image) {
    cv::Mat img = image;
    if (m_rawPreview) {
        m_rawPreview->setImage(img);
        m_rawPreview->setAuxiliaryRois(buildWorkspaceOverlayRois());
    }
    if (!m_rawPreview) {
        ui->imageView_Raw->loadImageOpenCv(img, true);
    }
}

/// Renders the match-result overlay for `image`/`result`: builds a VisionResultOverlay from
/// the result (using the active camera's workspace for ROI context), hides the ROI overlays
/// when "Show ROI" is unchecked, and displays it via the VisionResultViewerWidget — preferring
/// `image` but falling back to `result.Image` if `image` is empty/unrenderable. Falls back to
/// the legacy imageView_Result widget (rendering result.Image directly) when the result viewer
/// is not installed.
/// @param image the source image the match was run against
/// @param result the completed matching pass's result
void LocalizationPatternsWidget::displayResultOverlay(const cv::Mat &image,
                                                      const mtc::MatchResult &result) {
    const QString camId = activeCameraId();
    vc::model::CameraWorkspace workspaceValue;
    const vc::model::CameraWorkspace *workspace = nullptr;
    if (m_localizeTask && !camId.isEmpty()) {
        workspaceValue = m_localizeTask->taskLocalizeConfig().cameraWorkspace(camId);
        workspace = &workspaceValue;
    }

    if (m_resultViewer) {
        const cv::Mat *displayImage = firstRenderableImage({&image, &result.Image});
        if (!displayImage) {
            m_resultViewer->clearResult();
            return;
        }

        VisionResultOverlay overlay = VisionResultAdapter::fromMatchResult(
            result, QSize(displayImage->cols, displayImage->rows), workspace);
        if (ui->cbx_show_match_area && !ui->cbx_show_match_area->isChecked()) {
            overlay.roiOverlays.clear();
        }

        m_resultViewer->setImage(*displayImage);
        m_resultViewer->setOverlay(overlay);
        return;
    }

    cv::Mat img = result.Image;
    if (img.empty()) return;
    ui->imageView_Result->loadImageOpenCv(img, true);
}

/// Binarizes m_currentImage at the current threshold (auto/Otsu when the "Auto" checkbox is
/// checked, else the slider value) and maxValue, displays it in imageView_Binary without
/// resetting zoom/pan (so dragging the threshold slider does not jump the view), and updates
/// the info label with the threshold actually used. Clears the view and shows "No image" when
/// there is no current test image.
void LocalizationPatternsWidget::displayBinaryImage() {
    if (m_currentImage.empty()) {
        ui->imageView_Binary->clearCurrentImage();
        ui->lbl_binary_info->setText(tr("No image"));
        return;
    }

    const int threshold = ui->chk_binary_auto->isChecked()
                              ? -1 : ui->sld_binary_threshold->value();
    const int maxValue = ui->spn_binary_maxvalue->value();
    double used = 0.0;
    cv::Mat bin = vsu::binarizeSourceImage(m_currentImage, threshold, maxValue, &used);
    // fitsize=false keeps zoom/pan while dragging the threshold slider.
    ui->imageView_Binary->loadImageOpenCv(bin, false);

    ui->lbl_binary_info->setText(
        threshold < 0 ? tr("Auto (Otsu = %1)  ·  max %2").arg(static_cast<int>(used)).arg(maxValue)
                      : tr("Manual = %1  ·  max %2").arg(threshold).arg(maxValue));
}

/// Pulls the binary threshold/maxValue from the bound group's shared EdgeMatchConfig and seeds
/// the binary-view controls (auto checkbox, threshold slider/spin, maxValue spin) from them,
/// blocking their signals during the sync; all controls are disabled when no group is bound.
void LocalizationPatternsWidget::seedBinaryControlsFromConfig() {
    // Pull the binary threshold / maxValue from the bound group's shared edge
    // config.  Controls are disabled when no group is selected.
    const bool hasGroup = (m_boundMatchGroup != nullptr);
    int  threshold = -1;
    int  maxValue  = 255;
    if (const auto *ecfg = m_workingGroupConfig.edgeConfig(); hasGroup && ecfg) {
        threshold = ecfg->binaryThreshold;
        maxValue  = ecfg->binaryMaxValue;
    }
    const bool autoMode = threshold < 0;

    const QSignalBlocker bAuto(ui->chk_binary_auto);
    const QSignalBlocker bSld (ui->sld_binary_threshold);
    const QSignalBlocker bSpn (ui->spn_binary_threshold);
    const QSignalBlocker bMax (ui->spn_binary_maxvalue);

    ui->chk_binary_auto->setChecked(autoMode);
    ui->sld_binary_threshold->setValue(autoMode ? 128 : threshold);
    ui->spn_binary_threshold->setValue(autoMode ? 128 : threshold);
    ui->spn_binary_maxvalue->setValue(maxValue);

    ui->chk_binary_auto     ->setEnabled(hasGroup);
    ui->sld_binary_threshold->setEnabled(hasGroup && !autoMode);
    ui->spn_binary_threshold->setEnabled(hasGroup && !autoMode);
    ui->spn_binary_maxvalue ->setEnabled(hasGroup);
}

/// Switches stackedWidget_ImageView to `page` and updates the Raw/Result/Binary view-toggle
/// buttons' checked state to match.
/// @param page one of kMonitorPageRaw, kMonitorPageResult, kMonitorPageBinary
void LocalizationPatternsWidget::setMonitorPage(int page) {
    ui->stackedWidget_ImageView->setCurrentIndex(page);
    ui->btn_view_raw->setChecked(page == kMonitorPageRaw);
    ui->btn_view_result->setChecked(page == kMonitorPageResult);
    ui->btn_view_binary->setChecked(page == kMonitorPageBinary);
}

// ── Matching test ────────────────────────────────────────────────────────────

/// Starts an asynchronous commissioned matching pass on m_currentImage against the selected
/// group: validates the group is non-empty, clips the active camera's workspace ROI to the
/// image bounds (falling back to matching the full frame with a warning if the ROI would end
/// up empty/out of bounds), then kicks off TaskLocalization::startCommissionMatching and
/// connects a one-shot handler (onMatchingCommissionDone) for its completion.
/// @param outResult unused; the result arrives asynchronously via onMatchingCommissionDone
bool LocalizationPatternsWidget::runMatchingTest(mtc::MatchResult &outResult) {
    if (!m_patternManager) return false;
    if (m_selectedGroupIndex < 0) return false;
    if (m_currentImage.empty()) return false;

    auto group = m_patternManager->findGroupByNumber(m_selectedGroupIndex);
    if (!group || group->isEmpty()) {
        setState(State::Warning, tr("Selected group has no patterns."));
        return false;
    }

    const QString camId = activeCameraId();
    vc::model::CameraWorkspace ws;
    if (!camId.isEmpty()) {
        ws = m_localizeTask->taskLocalizeConfig().cameraWorkspace(camId);
        if (ws.useWorkspace && ws.hasRoi()) {
            cv::Rect roi(qRound(ws.roi.x), qRound(ws.roi.y),
                         qRound(ws.roi.width), qRound(ws.roi.height));
            roi &= cv::Rect(0, 0, m_currentImage.cols, m_currentImage.rows);
            if (!(roi.width > 0 && roi.height > 0)) {
                setState(State::Warning,
                         tr("Workspace ROI is outside the image; matching the full frame."));
                ws.useWorkspace = false;
            }
        }
    }

    m_localizeTask->startCommissionMatching(group, m_currentImage, ws);
    connect(m_localizeTask, &vc::model::TaskLocalization::commissionMatchingFinished,
            this, &LocalizationPatternsWidget::onMatchingCommissionDone, Qt::SingleShotConnection);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Result table (Pane 1 bottom)
//
//  Mirrors the design handoff `MonitorPane → Result Table`.  Columns:
//      #  |  Pat #  |  Pattern Name  |  Score  |  X  |  Y  |  Angle  |  OK
//
//  Inserted into pane_monitor's vertical layout right after frame_kpi so it
//  sits at the bottom of the monitor area.  Score is colour-coded:
//      ≥ 0.85 → green   ≥ 0.70 → orange   else → red
//
// -> disasbled OK/NG Row
// -> considering using OK/NG Row for dectected object under thresh score
// ─────────────────────────────────────────────────────────────────────────────

/// Builds the result table (Pane 1 bottom): configures result_table's columns/headers,
/// selection/edit behavior, default column widths, and wires row-selection sync plus an
/// Escape-key shortcut to clear the result selection.
void LocalizationPatternsWidget::buildResultTable() {
    if (!ui->pane_monitor) return;

    // Table
    m_resultTable = ui->result_table;
    m_resultTable->setColumnCount(8);
    m_resultTable->setRowCount(0);
    m_resultTable->setObjectName(QStringLiteral("patternResultTable"));
    QStringList headers = {tr("#"), tr("Pat #"), tr("Pattern Name"),
                            tr("Score"), tr("X"), tr("Y"), tr("Angle") , tr("OK") };
    m_resultTable->setHorizontalHeaderLabels(headers);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultTable->setAlternatingRowColors(false);
    m_resultTable->setShowGrid(false);
    m_resultTable->setMinimumHeight(120);
    m_resultTable->setMaximumHeight(220);
    m_resultTable->horizontalHeader()->setStretchLastSection(false);
    m_resultTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    // Reasonable default column widths
    const int widths[8] = { 36, 48, 0 /* stretch */, 70, 70, 70, 60 , 100 };
    for (int i = 0; i < 7; ++i)
        if (widths[i] > 0) m_resultTable->setColumnWidth(i, widths[i]);

    connect(m_resultTable, &QTableWidget::itemSelectionChanged,
            this, &LocalizationPatternsWidget::syncResultSelectionFromTable);

    auto *clearSelectionShortcut =
        new QShortcut(QKeySequence(Qt::Key_Escape), m_resultTable);
    connect(clearSelectionShortcut, &QShortcut::activated,
            this, &LocalizationPatternsWidget::clearResultSelection);
}

/// Clears the result-object selection and empties the result table (removes all rows).
void LocalizationPatternsWidget::clearResultTable() {
    clearResultSelection();
    if (m_resultTable) m_resultTable->setRowCount(0);
}

/// Repopulates the result table from a completed match result: one row per detected object,
/// with pattern number/name, score, center X/Y, angle, and an OK/Collision/Outside status —
/// each colour-coded via the `ptn` palette (OK green, collision/mismatch warn/err). Clears any
/// existing selection and rows first; leaves the table empty if the result has no objects.
/// @param result the completed matching pass's result
void LocalizationPatternsWidget::populateResultTable(const mtc::MatchResult &result) {
    if (!m_resultTable) return;

    clearResultSelection();
    m_resultTable->setRowCount(0);
    if (result.Objects.empty()) return;

    auto cell = [](const QString &text) {
        auto *it = new QTableWidgetItem(text);
        it->setTextAlignment(Qt::AlignCenter);
        return it;
    };
    auto colored = [](const QString &text, const QString &hex, bool bold = false) {
        auto *it = new QTableWidgetItem(text);
        it->setForeground(QBrush(QColor(hex)));
        it->setTextAlignment(Qt::AlignCenter);
        QFont f = it->font(); f.setBold(bold); it->setFont(f);
        return it;
    };

    int row = 0;
    for (const auto &obj : result.Objects) {
        m_resultTable->insertRow(row);

        const double score   = obj.matched_Score;
        const QString scoreClr = obj.hasCollision() ? QString(ptn::WARN) : QString(ptn::OK);
        // const QString scoreClr = obj.isOutsideConditionRoi()
        //                              ? QString(ptn::ERR)
        //                              : (obj.hasCollision()
        //                                     ? QString(ptn::WARN) : QString(ptn::OK));

        const bool ok = !obj.hasCollision() && !obj.isOutsideConditionRoi();

        auto *indexCell = cell(QString::number(row + 1));
        indexCell->setData(Qt::UserRole, row + 1);
        m_resultTable->setItem(row, 0, indexCell);
        m_resultTable->setItem(row, 1, colored(QString("#%1").arg(obj.pattern_index),
                                                 ptn::OUTPUT, true));
        m_resultTable->setItem(row, 2, cell(QString::fromStdWString(obj.pattern_name)));
        m_resultTable->setItem(row, 3, colored(QString::number(score, 'f', 3),
                                                 scoreClr, true));
        m_resultTable->setItem(row, 4, cell(QString::number(obj.point_Center.x, 'f', 1)));
        m_resultTable->setItem(row, 5, cell(QString::number(obj.point_Center.y, 'f', 1)));
        m_resultTable->setItem(row, 6, cell(QString::number(obj.matched_Angle, 'f', 1) + "°"));
        QString stateStr;
        QString stateClr;
        if (ok) {
            stateStr = "OK";
            stateClr = ptn::OK;
        } else {
            if (obj.hasCollision() && obj.isOutsideConditionRoi()) {
                stateStr = "Collision, Outside";
                stateClr = ptn::ERR;
            } else if (obj.hasCollision()) {
                stateStr = "Collision";
                stateClr = ptn::WARN;
            } else if (obj.isOutsideConditionRoi()) {
                stateStr = "OutSide";
                stateClr = ptn::ERR;
            }
        }
        m_resultTable->setItem(row, 7, colored(stateStr, stateClr, true));
        ++row;
    }
}

/// Mirrors the result table's current row selection into the result viewer: resolves the row
/// to its overlay object index (via resultOverlayIndexForRow()) and selects/clears the
/// matching object in the viewer.
void LocalizationPatternsWidget::syncResultSelectionFromTable()
{
    if (!m_resultViewer || !m_resultTable) return;

    const int row = m_resultTable->currentRow();
    if (row < 0) {
        m_resultViewer->clearSelectedResultObject();
        return;
    }

    const int objectIndex = resultOverlayIndexForRow(row);
    if (objectIndex > 0) {
        m_resultViewer->setSelectedResultObject(objectIndex);
    } else {
        m_resultViewer->clearSelectedResultObject();
    }
}

/// Mirrors a result-viewer object selection back into the result table: finds and selects the
/// table row whose stored object index matches `objectIndex`, or clears the table selection if
/// `objectIndex` is non-positive or no row matches. Blocks the table's signals during the sync
/// to avoid feeding the change back into syncResultSelectionFromTable().
/// @param objectIndex 1-based object index as reported by the result viewer, or <= 0 for "none"
void LocalizationPatternsWidget::syncResultSelectionFromViewer(int objectIndex)
{
    if (!m_resultTable) return;

    QSignalBlocker blocker(m_resultTable);
    if (objectIndex <= 0) {
        m_resultTable->clearSelection();
        return;
    }

    for (int row = 0; row < m_resultTable->rowCount(); ++row) {
        if (resultOverlayIndexForRow(row) == objectIndex) {
            m_resultTable->selectRow(row);
            return;
        }
    }

    m_resultTable->clearSelection();
}

/// Clears the result-object selection in both the result table (signals blocked) and the
/// result viewer.
void LocalizationPatternsWidget::clearResultSelection()
{
    if (m_resultTable) {
        QSignalBlocker blocker(m_resultTable);
        m_resultTable->clearSelection();
    }
    if (m_resultViewer) {
        m_resultViewer->clearSelectedResultObject();
    }
}

/// Returns the overlay object index stored (in Qt::UserRole) on column 0 of table row `row`,
/// or -1 if there is no result table, `row` is out of range, or no index was stored.
/// @param row result-table row index
int LocalizationPatternsWidget::resultOverlayIndexForRow(int row) const
{
    if (!m_resultTable || row < 0 || row >= m_resultTable->rowCount()) {
        return -1;
    }

    const QTableWidgetItem *item = m_resultTable->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}
