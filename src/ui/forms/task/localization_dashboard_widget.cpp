#include "localization_dashboard_widget.h"
#include "ui_localization_dashboard_widget.h"

#include "ui/widgets/signals_monitor_widget.h"
#include "ui/widgets/task_event_log_widget.h"
#include "ui/widgets/image_widget/image_view_only.h"
#include "ui/widgets/vision/vision_geometry.h"
#include "ui/widgets/vision/vision_result_adapter.h"
#include "ui/widgets/vision/vision_result_viewer_widget.h"
#include "ui/widgets/controls/status_lamp.h"

#include "core/logger/app_logger.h"
#include "model/project.h"
#include "model/localization_fault_code.h"
#include "device/device_manager.h"
#include "runtime/idevice_runner.h"
#include "runtime/task_runner.h"

#include <QColor>
#include <QLayout>
#include <QMetaProperty>
#include <QSignalBlocker>
#include <QShortcut>
#include <QStyle>
#include <QTableWidgetItem>

#include <initializer_list>
#include <utility>

using vc::model::TaskLocalization;
using vc::model::TaskLocalizeConfig;

namespace {

/// Returns the first candidate in `candidates` that is non-null, non-empty, and converts to a
/// non-null QPixmap (via vision::pixmapFromMat), or nullptr if none qualify.
/// @param candidates images to try, in priority order
/// @return pointer to the first renderable image, or nullptr
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

/// Schema for the signal monitor — internalName, displayName and type
/// declared inline. Display names live here (per yêu cầu) rather than being
/// resolved through Q_CLASSINFO so a missing/renamed field is a visible diff
/// in code review.
struct SignalRowSpec {
    const char *internalName;  ///< Signal name as used in TaskLocalization::signalChanged and config properties.
    const char *displayName;   ///< Human-readable label shown in the signal monitor row.
    SignalsMonitorWidget::Type type;  ///< Whether the monitor should render/refresh this row as a Bool or Number.
};

/// Fixed schema of signals shown in the dashboard's signal monitor: number signals
/// (nActiveCamera, nActivePatternGroup, nDetectedNumber, nFaultCode) followed by bool signals
/// (bCameraValid, bPatternValid, bTaskReady, bExecuteTrigger, bMatchingFinished, bMatchingBusy,
/// bMatchingDetected, bMatchingLowArea, bTaskFault).
constexpr SignalRowSpec kSignalRows[] = {
    // Number signals
    { "nActiveCamera",       "Active camera",       SignalsMonitorWidget::Type::Number },
    { "nActivePatternGroup", "Active pattern group", SignalsMonitorWidget::Type::Number },
    { "nDetectedNumber",     "Detected count",      SignalsMonitorWidget::Type::Number },
    { "nFaultCode",          "Fault code",          SignalsMonitorWidget::Type::Number },
    // Bool signals
    { "bCameraValid",      "Camera valid",      SignalsMonitorWidget::Type::Bool   },
    { "bPatternValid",     "Pattern valid",     SignalsMonitorWidget::Type::Bool   },
    { "bTaskReady",        "Task ready",        SignalsMonitorWidget::Type::Bool   },
    { "bExecuteTrigger",   "Execute trigger",   SignalsMonitorWidget::Type::Bool   },
    { "bMatchingFinished", "Matching finished", SignalsMonitorWidget::Type::Bool   },
    { "bMatchingBusy",     "Matching busy",     SignalsMonitorWidget::Type::Bool   },
    { "bMatchingDetected", "Matching detected", SignalsMonitorWidget::Type::Bool   },
    { "bMatchingLowArea",  "Low binary area",   SignalsMonitorWidget::Type::Bool   },
    { "bTaskFault",        "Task fault",        SignalsMonitorWidget::Type::Bool   },
};

/// Reads `internalName` as a Q_GADGET property off TaskLocalizeConfig via QMetaObject, converting
/// the value to its string form (used to populate a signal row's config tag).
/// @param cfg the config gadget to read the property from
/// @param internalName the property name to look up
/// @return the property's string value, or an empty string if no such property exists
QString readConfigField(const TaskLocalizeConfig &cfg, const QString &internalName) {
    const QMetaObject &meta = TaskLocalizeConfig::staticMetaObject;
    int idx = meta.indexOfProperty(internalName.toUtf8().constData());
    if (idx < 0) return {};
    return meta.property(idx).readOnGadget(&cfg).toString();
}

/// Looks up `internalName`'s SignalsMonitorWidget::Type in kSignalRows.
/// @param internalName the signal's internal name
/// @return the matching row's type, or Number as a fallback if the name is not in kSignalRows
///         (an unknown name is logged separately by the monitor widget)
SignalsMonitorWidget::Type typeOf(const QString &internalName) {
    for (const auto &s : kSignalRows) {
        if (internalName == QLatin1String(s.internalName)) return s.type;
    }
    return SignalsMonitorWidget::Type::Number;   // fallback; unknown name is logged by widget
}

/// Map a runtime log severity string to the operator-log severity level.
/// @param severity the runtime log entry's severity string (case-insensitive, trimmed)
/// @return Error for ERROR/FATAL/CRITICAL, Warning for WARN/WARNING, Success for
///         OK/SUCCESS/DONE, and Info for anything else
TaskEventLevel severityToLevel(const QString &severity) {
    const QString s = severity.trimmed().toUpper();
    if (s == QLatin1String("ERROR") || s == QLatin1String("FATAL") ||
        s == QLatin1String("CRITICAL"))
        return TaskEventLevel::Error;
    if (s == QLatin1String("WARN") || s == QLatin1String("WARNING"))
        return TaskEventLevel::Warning;
    if (s == QLatin1String("OK") || s == QLatin1String("SUCCESS") ||
        s == QLatin1String("DONE"))
        return TaskEventLevel::Success;
    return TaskEventLevel::Info;
}

/// Formats a fault code as "<code> — <name>" using the name resolved via
/// vc::model::localizationFaultCodeName.
/// @param code the fault code to format
/// @return the "<code> — <name>" display string
QString faultCodeText(int code) {
    const auto name = vc::model::localizationFaultCodeName(
        static_cast<vc::model::LocalizationFaultCode>(code));
    return QStringLiteral("%1 — %2").arg(code).arg(name);
}

/// Swaps `newWidget` into `layout` in place of `oldWidget` (via QLayout::replaceWidget), then
/// hides the old widget and shows the new one. No-op if any argument is null.
/// @param layout the layout containing `oldWidget`
/// @param oldWidget the widget to remove from view
/// @param newWidget the widget to display in its place
void replaceDashboardWidget(QLayout *layout, QWidget *oldWidget, QWidget *newWidget)
{
    if (!layout || !oldWidget || !newWidget) return;
    layout->replaceWidget(oldWidget, newWidget);
    oldWidget->hide();
    newWidget->show();
}

} // namespace

/// Constructs the generated UI, registers the dark/light dashboard stylesheets for theme
/// reloading, then delegates the rest of the setup to initWidget().
LocalizationDashboardWidget::LocalizationDashboardWidget(std::shared_ptr<vc::model::ITask> task,
                                                         ads::CDockWidget *dock, QWidget *parent)
    : ITaskWidget(task, dock, parent),
    ui(new Ui::LocalizationDashboardWidget) {

    ui->setupUi(this);
    setupThemeReload(QStringLiteral(":/styles/localization_dashboard_widget_dark.qss"),
                     QStringLiteral(":/styles/localization_dashboard_widget_light.qss"));
    initWidget();
}

/// Deletes the generated UI object.
LocalizationDashboardWidget::~LocalizationDashboardWidget()
{
    delete ui;
}

/// Resolves m_localizeTask from the base-class task pointer (logging an error and returning early
/// if the task is not a TaskLocalization), then sets up the lamps, connection lamps, vision
/// viewer, and result table; connects to the task's devicesChanged/taskStateChanged/
/// cycleResultUpdated/taskLogAppended/signalChanged signals; appends the fixed signal-monitor
/// schema; and finally calls loadConfigToWidget() to populate the initial state. Dashboard v1 is
/// read-only, so the signal monitor's requestWriteValue is only logged, and
/// setDeviceConnected(false) keeps writes disabled in the monitor widget.
void LocalizationDashboardWidget::initWidget() {
    m_localizeTask = static_cast<TaskLocalization *>(m_task.get());
    if (!m_localizeTask) {
        LOG_DEV_ERR << "LocalizationDashboardWidget: task is not TaskLocalization";
        return;
    }

    m_config = m_localizeTask->taskLocalizeConfig();

    setupLamps();
    setupConnectionLamps();
    installVisionViewer();
    setupResultTable();
    setFaultState(false, 0);
    updateTaskContext();
    updateTaskStateLabel();

    // ── Signal monitor: append fixed schema ────────────────────────────
    for (const auto &spec : kSignalRows) {
        ui->wg_signal_monitor->appendRow(
            QString::fromUtf8(spec.internalName),
            QString::fromUtf8(spec.displayName),
            spec.type);
    }

    // ── User-initiated writes ──────────────────────────────────────────
    // Dashboard v1 is read-only. Keep the signal visible in logs in case a
    // row becomes editable by mistake, but do not route writes from here.
    connect(ui->wg_signal_monitor,
            &SignalsMonitorWidget::requestWriteValue,
            this, [](const QString &name, const QVariant &v) {
        LOG_USER_WARN << "SignalsMonitor write requested (not wired):"
                      << name << "=" << v.toString();
    });

    // ── React to assignment changes from the task ──────────────────────
    connect(m_localizeTask, &vc::model::ITask::devicesChanged,
            this, [this] {
        m_config = m_localizeTask->taskLocalizeConfig();
        pushSignalTagsFromConfig();
        updateTaskContext();
        rebuildConnectionWiring();
    });
    connect(m_localizeTask, &vc::model::ITask::taskStateChanged,
            this, [this] {
        updateTaskStateLabel();
    });
    connect(m_localizeTask, &TaskLocalization::cycleResultUpdated,
            this, &LocalizationDashboardWidget::updateCycleResult);
    connect(m_localizeTask, &TaskLocalization::taskLogAppended,
            this, &LocalizationDashboardWidget::appendTaskLog);

    // ── Live signal value updates ──────────────────────────────────────
    // The dashboard does NOT subscribe to the communication device directly;
    // TaskLocalization exposes a unified signalChanged(name, value) that the
    // monitor refreshes from. The widget refreshes by signal name (the
    // internalName), not by PLC tag, so we just dispatch by configured type.
    //
    connect(m_localizeTask, &TaskLocalization::signalChanged,
            this, [this](const QString &name, const QVariant &value) {
        m_liveSignalValues.insert(name, value);
        switch (typeOf(name)) {
        case SignalsMonitorWidget::Type::Bool:
            ui->wg_signal_monitor->refreshBool(name, value.toBool());
            break;
        case SignalsMonitorWidget::Type::Number:
            ui->wg_signal_monitor->refreshNumber(name, value.toInt());
            break;
        }
        applySignalToDashboard(name, value);
    });

    // Dashboard v1 remains read-only, so writes stay disabled even when the
    // runtime device is connected.
    ui->wg_signal_monitor->setDeviceConnected(false);

    loadConfigToWidget();
}

/// Sets the display names and idle (Off) status/text for the task/camera/pattern/cycle lamps.
void LocalizationDashboardWidget::setupLamps() {
    ui->lamp_task->setLampName(tr("Task"));
    ui->lamp_camera->setLampName(tr("Camera"));
    ui->lamp_pattern->setLampName(tr("Pattern"));
    ui->lamp_cycle->setLampName(tr("Cycle"));

    ui->lamp_task->setStatus(StatusLamp::Status::Off, tr("—"));
    ui->lamp_camera->setStatus(StatusLamp::Status::Off, tr("—"));
    ui->lamp_pattern->setStatus(StatusLamp::Status::Off, tr("—"));
    ui->lamp_cycle->setStatus(StatusLamp::Status::Off, tr("Idle"));
}

/// Sets the display names and idle (Off) status/text for the three connection lamps, then calls
/// rebuildConnectionWiring() to wire them to the currently bound devices.
void LocalizationDashboardWidget::setupConnectionLamps() {
    ui->lamp_connection_plc->setLampName(tr("PLC"));
    ui->lamp_connection_camera->setLampName(tr("Camera"));
    ui->lamp_connection_output->setLampName(tr("Output"));

    ui->lamp_connection_plc->setStatus(StatusLamp::Status::Off, tr("—"));
    ui->lamp_connection_camera->setStatus(StatusLamp::Status::Off, tr("—"));
    ui->lamp_connection_output->setStatus(StatusLamp::Status::Off, tr("—"));

    rebuildConnectionWiring();
}

/// Disconnects all current connection-lamp subscriptions (m_connectionConns) and rewires the PLC,
/// camera, and vision-output lamps from the current device bindings/active camera.
void LocalizationDashboardWidget::rebuildConnectionWiring() {
    // Drop any previous runner subscriptions before re-resolving devices.
    for (const auto &conn : std::as_const(m_connectionConns))
        QObject::disconnect(conn);
    m_connectionConns.clear();

    if (!m_localizeTask) return;

    const auto &bindings = m_config.d->m_deviceBindings;
    wireConnectionLamp(ui->lamp_connection_plc,    bindings.primaryPlcDeviceId());
    wireConnectionLamp(ui->lamp_connection_camera, resolveActiveCameraDeviceId());
    wireConnectionLamp(ui->lamp_connection_output, bindings.visionOutputDeviceId());
}

/// Wires a single connection lamp to `deviceId`'s runner: seeds the lamp from the runner's
/// current connect status and subscribes to further connectStatusChanged updates, appending the
/// connection to m_connectionConns. Sets the lamp to "Not set"/"—" when the id is empty or no
/// runner is registered yet for it.
void LocalizationDashboardWidget::wireConnectionLamp(StatusLamp *lamp,
                                                     const QString &deviceId) {
    if (!lamp) return;

    if (deviceId.isEmpty()) {
        lamp->setStatus(StatusLamp::Status::Off, tr("Not set"));
        return;
    }

    auto *taskRunner = m_localizeTask->taskRunner();
    auto *runner = taskRunner ? taskRunner->runnerFor(deviceId) : nullptr;
    if (!runner) {
        // Bound, but no runner yet (device not registered / wrong phase).
        lamp->setStatus(StatusLamp::Status::Off, tr("—"));
        return;
    }

    // Seed the lamp from the current status. connectStatus() is a plain enum
    // read; a momentarily stale value is acceptable for an indicator and is
    // corrected by the next forwarded change below.
    applyConnectStatusToLamp(
        lamp, runner->device() ? runner->device()->connectStatus()
                               : vc::device::ConnectStatus::NoConnection);

    // The runner forwards the device's connectStatusChanged onto the GUI thread
    // (QueuedConnection), so updating the lamp from this slot is thread-safe.
    m_connectionConns.append(connect(
        runner, &vc::runtime::IDeviceRunner::connectStatusChanged, this,
        [this, lamp](vc::device::ConnectStatus status) {
            applyConnectStatusToLamp(lamp, status);
        }));
}

/// Maps a vc::device::ConnectStatus to the lamp's Status/text pair (Connected → Ok/"Connected",
/// Connecting → Warning/"Connecting", LostConnected/ConnectFailed → Error/"Lost"/"Failed",
/// Disconnected/NoConnection → Off) and applies it.
void LocalizationDashboardWidget::applyConnectStatusToLamp(
    StatusLamp *lamp, vc::device::ConnectStatus status) {
    if (!lamp) return;
    using CS = vc::device::ConnectStatus;
    switch (status) {
    case CS::Connected:
        lamp->setStatus(StatusLamp::Status::Ok, tr("Connected"));
        break;
    case CS::Connecting:
        lamp->setStatus(StatusLamp::Status::Warning, tr("Connecting"));
        break;
    case CS::LostConnected:
        lamp->setStatus(StatusLamp::Status::Error, tr("Lost"));
        break;
    case CS::ConnectFailed:
        lamp->setStatus(StatusLamp::Status::Error, tr("Failed"));
        break;
    case CS::Disconnected:
        lamp->setStatus(StatusLamp::Status::Off, tr("Disconnected"));
        break;
    // No default: — all ConnectStatus values are enumerated so -Wswitch / C4062
    // flags a new value here.
    case CS::NoConnection:
        lamp->setStatus(StatusLamp::Status::Off, tr("—"));
        break;
    }
}

/// Active camera device for the camera lamp: the device mapped to the live nActiveCamera number,
/// falling back to the first bound camera.
QString LocalizationDashboardWidget::resolveActiveCameraDeviceId() const {
    const QMap<int, QString> cameras = m_config.d->m_deviceBindings.cameraNumberMap();
    if (cameras.isEmpty()) return QString();
    // Prefer the live active camera when it maps to a bound device; otherwise
    // fall back to the first bound camera (QMap iterates by ascending number).
    if (cameras.contains(m_activeCameraNumber))
        return cameras.value(m_activeCameraNumber);
    return cameras.first();
}

/// Re-reads the task's TaskLocalizeConfig and refreshes the signal tags, task context labels, and
/// task-state lamp from it.
void LocalizationDashboardWidget::loadConfigToWidget() {
    if (!m_localizeTask) return;
    m_config = m_localizeTask->taskLocalizeConfig();
    pushSignalTagsFromConfig();
    updateTaskContext();
    updateTaskStateLabel();
}

/// No-op: the dashboard is read-only and never pushes edits back to the task.
void LocalizationDashboardWidget::loadConfigToTask() {
    // Dashboard is read-only — no config to push back to the task.
}

/// Refreshes each signal-monitor row's tag from the current config (readConfigField), so the
/// monitor can show "(not mapped)" for unbound signals; live values themselves still arrive via
/// TaskLocalization::signalChanged.
void LocalizationDashboardWidget::pushSignalTagsFromConfig() {
    // Tag string is still sourced from the config — the widget uses it only
    // to decide whether to render "(not mapped)". Live values arrive via
    // TaskLocalization::signalChanged keyed by signal name, not by tag.
    for (const auto &spec : kSignalRows) {
        const QString name = QString::fromUtf8(spec.internalName);
        ui->wg_signal_monitor->setRowTag(name, readConfigField(m_config, name));
    }
}

/// Refreshes the vision-output/PLC device labels (via the local deviceName lambda, which formats
/// "<name> (<id>)" or "<id> (missing)" when the device manager doesn't know the id) and resets
/// the camera/pattern-group value labels to "—" from the current device bindings in m_config.
void LocalizationDashboardWidget::updateTaskContext()
{
    if (!m_localizeTask) return;

    auto deviceName = [this](const QString &deviceId) {
        if (deviceId.isEmpty()) return QStringLiteral("—");
        auto project = m_localizeTask->project();
        auto manager = project ? project->deviceManager() : nullptr;
        auto device = manager ? manager->deviceById(deviceId) : nullptr;
        return device ? QStringLiteral("%1 (%2)").arg(device->name(), deviceId)
                      : QStringLiteral("%1 (missing)").arg(deviceId);
    };

    const auto &bindings = m_config.d->m_deviceBindings;
    // Captions are static in the .ui; the widget only writes value labels.
    ui->lbl_val_vision_device->setText(deviceName(bindings.visionOutputDeviceId()));
    ui->lbl_val_plc_device->setText(deviceName(bindings.primaryPlcDeviceId()));
    ui->lbl_val_camera->setText(QStringLiteral("—"));
    ui->lbl_val_pattern_group->setText(QStringLiteral("—"));
}

/// Maps the task's current TaskState to the "Cycle" lamp's status (Faulted → Error,
/// RunningCycle/Recovering → Warning, Ready → Ok, otherwise Off) and applies it with the state's
/// display name as the lamp text.
void LocalizationDashboardWidget::updateTaskStateLabel()
{
    if (!m_localizeTask) return;
    const auto state = m_localizeTask->taskState();

    // The "Cycle" lamp reflects runtime cycle activity derived from task state.
    StatusLamp::Status status;
    switch (state) {
    case vc::model::TaskState::Faulted:
        status = StatusLamp::Status::Error;
        break;
    case vc::model::TaskState::RunningCycle:
    case vc::model::TaskState::Recovering:
        status = StatusLamp::Status::Warning;
        break;
    case vc::model::TaskState::Ready:
        status = StatusLamp::Status::Ok;
        break;
    default:
        status = StatusLamp::Status::Off;
        break;
    }
    ui->lamp_cycle->setStatus(status, vc::model::taskStateDisplayName(state));
}

/// Routes a single live signal value to its dashboard visual: bCameraValid/bPatternValid drive
/// the camera/pattern lamps; bTaskReady drives the task lamp unless a fault is active;
/// bMatchingBusy drives the cycle lamp; bTaskFault calls setFaultState(); nFaultCode updates the
/// fault-code label; nActiveCamera updates the camera value label and, on change, re-resolves the
/// connection lamp wiring; nActivePatternGroup updates the pattern-group value label. Unrecognized
/// signal names are ignored here (the signal-monitor-list refresh for all names is handled
/// separately at the call site).
void LocalizationDashboardWidget::applySignalToDashboard(const QString &name,
                                                         const QVariant &value)
{
    if (name == QLatin1String("bCameraValid")) {
        const bool ok = value.toBool();
        ui->lamp_camera->setStatus(ok ? StatusLamp::Status::Ok : StatusLamp::Status::Off,
                                   ok ? tr("Valid") : tr("Invalid"));
    } else if (name == QLatin1String("bPatternValid")) {
        const bool ok = value.toBool();
        ui->lamp_pattern->setStatus(ok ? StatusLamp::Status::Ok : StatusLamp::Status::Off,
                                    ok ? tr("Valid") : tr("Invalid"));
    } else if (name == QLatin1String("bTaskReady")) {
        // Don't override a fault-driven Error state on the task lamp.
        if (!m_taskFaultActive) {
            const bool ready = value.toBool();
            ui->lamp_task->setStatus(ready ? StatusLamp::Status::Ok : StatusLamp::Status::Off,
                                     ready ? tr("Ready") : tr("Not ready"));
        }
    } else if (name == QLatin1String("bMatchingBusy")) {
        const bool busy = value.toBool();
        ui->lamp_cycle->setStatus(busy ? StatusLamp::Status::Warning : StatusLamp::Status::Off,
                                  busy ? tr("Running") : tr("Idle"));
    } else if (name == QLatin1String("bTaskFault")) {
        setFaultState(value.toBool(), m_lastFaultCode);
    } else if (name == QLatin1String("nFaultCode")) {
        m_lastFaultCode = value.toInt();
        ui->lbl_val_fault_code->setText(faultCodeText(m_lastFaultCode));
    } else if (name == QLatin1String("nActiveCamera")) {
        const int number = value.toInt();
        ui->lbl_val_camera->setText(QString::number(number));
        if (number != m_activeCameraNumber) {
            m_activeCameraNumber = number;
            // The camera lamp follows the active camera; re-resolve its device.
            rebuildConnectionWiring();
        }
    } else if (name == QLatin1String("nActivePatternGroup")) {
        ui->lbl_val_pattern_group->setText(QString::number(value.toInt()));
    }
}

/// Drives the fault panel: records the active/faultCode state, updates the fault flag/code
/// labels, toggles the QFrame#frame_fault[active] style property (re-polishing the frame so the
/// stylesheet selector takes effect), and, when a fault becomes active, sets the task lamp to
/// Error. Clearing a fault does not restore the task lamp itself — that falls back to whatever
/// the next bTaskReady update reports.
void LocalizationDashboardWidget::setFaultState(bool active, int faultCode)
{
    m_taskFaultActive = active;
    m_lastFaultCode   = faultCode;

    ui->lbl_val_fault_flag->setText(active ? QStringLiteral("true")
                                           : QStringLiteral("false"));
    ui->lbl_val_fault_code->setText(faultCodeText(faultCode));

    // Drive the QFrame#frame_fault[active] attribute selector (Rule 4.5).
    ui->frame_fault->setProperty("active", active);
    ui->frame_fault->style()->unpolish(ui->frame_fault);
    ui->frame_fault->style()->polish(ui->frame_fault);
    ui->frame_fault->update();

    // The task lamp surfaces the fault as an Error; cleared faults fall back
    // to whatever the next bTaskReady update reports.
    if (active)
        ui->lamp_task->setStatus(StatusLamp::Status::Error, tr("Fault"));
}

/// Configures tbl_result's 11 result columns/headers, clears its rows, and wires selection sync
/// (itemSelectionChanged) and an Escape shortcut to clear the selection.
void LocalizationDashboardWidget::setupResultTable()
{
    ui->tbl_result->setColumnCount(11);
    ui->tbl_result->setHorizontalHeaderLabels({
        tr("#"),
        tr("Pattern"),
        tr("Score"),
        tr("Img X"),
        tr("Img Y"),
        tr("Img R"),
        tr("World X"),
        tr("World Y"),
        tr("World Z"),
        tr("World R"),
        tr("Status"),
    });
    ui->tbl_result->setRowCount(0);

    connect(ui->tbl_result, &QTableWidget::itemSelectionChanged,
            this, &LocalizationDashboardWidget::syncResultSelectionFromTable);

    auto *clearSelectionShortcut =
        new QShortcut(QKeySequence(Qt::Key_Escape), ui->tbl_result);
    connect(clearSelectionShortcut, &QShortcut::activated,
            this, &LocalizationDashboardWidget::clearResultSelection);
}

/// Updates the KPI labels (detected/sent counts, matching time, low-area flag), then either feeds
/// the vision result viewer (image + overlay built from VisionResultAdapter::fromCycleResult, or
/// clearResult() if no renderable image is available) or, when the viewer hasn't been installed,
/// falls back to loading result.displayImage directly into the legacy gv_match_view. Finally
/// clears the current selection and repopulates tbl_result with one row per result object,
/// stashing each row's overlay index in column 0's Qt::UserRole data.
void LocalizationDashboardWidget::updateCycleResult(
    const vc::model::LocalizationRuntimeController::CycleResult &result)
{
    ui->lbl_kpi_detected_val->setText(QString::number(result.detectedNumber));
    ui->lbl_kpi_sent_val->setText(QString::number(result.sentNumber));
    ui->lbl_kpi_cycle_time_val->setText(QStringLiteral("%1 ms").arg(result.matchingTimeMs, 0, 'f', 1));
    ui->lbl_kpi_low_area_val->setText(result.lowArea ? tr("Yes") : tr("No"));

    if (m_resultViewer) {
        const QString camId = resolveActiveCameraDeviceId();
        vc::model::CameraWorkspace workspaceValue;
        const vc::model::CameraWorkspace *workspace = nullptr;
        if (m_localizeTask && !camId.isEmpty()) {
            workspaceValue = m_localizeTask->taskLocalizeConfig().cameraWorkspace(camId);
            workspace = &workspaceValue;
        }

        const cv::Mat *displayImage = firstRenderableImage(
            {&result.rawImage, &result.matchResult.Image, &result.displayImage});
        if (!displayImage) {
            m_resultViewer->clearResult();
        } else {
            m_resultViewer->setImage(*displayImage);
            m_resultViewer->setOverlay(
                VisionResultAdapter::fromCycleResult(result, workspace, &m_liveSignalValues));
        }
    } else if (result.displayImage.empty()) {
        ui->gv_match_view->clearCurrentImage();
    } else {
        cv::Mat display = result.displayImage;
        ui->gv_match_view->loadImageOpenCv(display, true);
    }

    clearResultSelection();
    ui->tbl_result->setRowCount(result.rows.size());
    for (int row = 0; row < result.rows.size(); ++row) {
        const auto &r = result.rows.at(row);
        const QStringList values = {
            QString::number(r.index),
            r.patternName,
            QString::number(r.score, 'f', 3),
            QString::number(r.imageX, 'f', 2),
            QString::number(r.imageY, 'f', 2),
            QString::number(r.imageR, 'f', 2),
            QString::number(r.world.x, 'f', 3),
            QString::number(r.world.y, 'f', 3),
            QString::number(r.world.z, 'f', 3),
            QString::number(r.world.r, 'f', 3),
            r.status,
        };
        for (int col = 0; col < values.size(); ++col) {
            auto *item = new QTableWidgetItem(values.at(col));
            if (col == 0) {
                item->setData(Qt::UserRole, r.index);
            }
            ui->tbl_result->setItem(row, col, item);
        }
    }
}

/// Converts a runtime task-log entry to a TaskEvent (mapping its severity string via
/// severityToLevel) and appends it to the operator log view.
void LocalizationDashboardWidget::appendTaskLog(
    const vc::model::LocalizationRuntimeController::TaskLogEntry &entry)
{
    TaskEvent ev;
    ev.timestamp = entry.timestamp;
    ev.level     = severityToLevel(entry.severity);
    ev.message   = entry.message;
    // The operator log is task-scoped; no per-component source tag for now.
    ui->log_task_view->appendEvent(ev);
}

/// Lazily creates the VisionResultViewerWidget (no-op if already installed), swaps it in place of
/// the legacy gv_match_view in the layout, and connects its resultObjectSelectionChanged signal to
/// the table sync slot.
void LocalizationDashboardWidget::installVisionViewer()
{
    if (m_resultViewer) return;
    m_resultViewer = new VisionResultViewerWidget(this);
    replaceDashboardWidget(ui->verticalLayout, ui->gv_match_view, m_resultViewer);
    connect(m_resultViewer, &VisionResultViewerWidget::resultObjectSelectionChanged,
            this, &LocalizationDashboardWidget::syncResultSelectionFromViewer);
}

/// Selects the vision-viewer result object matching the currently selected table row (or clears
/// the viewer selection if no row, or no matching overlay index, is selected).
void LocalizationDashboardWidget::syncResultSelectionFromTable()
{
    if (!m_resultViewer) return;

    const int row = ui->tbl_result->currentRow();
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

/// Selects the table row whose overlay index matches `objectIndex` (or clears the table selection
/// if `objectIndex` is non-positive or has no matching row); blocks the table's signals while
/// doing so to avoid re-triggering syncResultSelectionFromTable().
void LocalizationDashboardWidget::syncResultSelectionFromViewer(int objectIndex)
{
    QSignalBlocker blocker(ui->tbl_result);
    if (objectIndex <= 0) {
        ui->tbl_result->clearSelection();
        return;
    }

    for (int row = 0; row < ui->tbl_result->rowCount(); ++row) {
        if (resultOverlayIndexForRow(row) == objectIndex) {
            ui->tbl_result->selectRow(row);
            return;
        }
    }

    ui->tbl_result->clearSelection();
}

/// Clears both the result table's selection (signal-blocked) and the vision viewer's selected
/// result object.
void LocalizationDashboardWidget::clearResultSelection()
{
    {
        QSignalBlocker blocker(ui->tbl_result);
        ui->tbl_result->clearSelection();
    }

    if (m_resultViewer) {
        m_resultViewer->clearSelectedResultObject();
    }
}

/// Looks up the overlay object index stashed in column 0's Qt::UserRole data for `row`.
int LocalizationDashboardWidget::resultOverlayIndexForRow(int row) const
{
    if (row < 0 || row >= ui->tbl_result->rowCount()) {
        return -1;
    }

    const QTableWidgetItem *item = ui->tbl_result->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}
