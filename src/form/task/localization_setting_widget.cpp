#include "localization_setting_widget.h"
#include "ui_localization_setting_widget.h"

#include "widgets/signals_map_widget.h"
#include "widgets/camera_mapping_widget.h"
#include "widgets/camera_workspace_widget.h"
#include "form/task/workspace_setting_dialog.h"

#include "core/logger/app_logger.h"
#include "model/project.h"
#include "device/device_capabilities.h"
#include "device/device_manager.h"
#include "runtime/task_runner.h"
#include "runtime/camera_runner.h"

#include <QComboBox>
#include <QImage>
#include <QMessageBox>
#include <QPixmap>
#include <QRectF>
#include <QSignalBlocker>

#include <opencv2/imgproc.hpp>

using vc::model::TaskLocalization;
using vc::model::TaskLocalizeConfig;
using vc::device::IDevice;
using vc::device::DeviceType;

namespace {

// Static schema of signal-tag rows, derived from TaskLocalizeConfigPrivate.
// Order = visual order in the SignalsMapWidget. Keeping it as a table here
// (rather than scraping the meta object) means a missed/renamed field becomes
// a visible diff in code review, per the project's "table-driven UI" rule.
struct SignalRowSpec {
    const char *internalName;   // matches the P_PROPERTY_STRING_READWRITE name
    SignalsMapWidget::Type type;
};

constexpr SignalRowSpec kSignalRows[] = {
    // Number signals
    { "nActiveCamera",      SignalsMapWidget::Type::Number },
    { "nActivePatternGroup", SignalsMapWidget::Type::Number },
    { "nDetectedNumber",    SignalsMapWidget::Type::Number },
    { "nFaultCode",         SignalsMapWidget::Type::Number },
    // Bool signals
    { "bCameraValid",       SignalsMapWidget::Type::Bool },
    { "bPatternValid",      SignalsMapWidget::Type::Bool },
    { "bTaskReady",         SignalsMapWidget::Type::Bool },
    { "bExecuteTrigger",    SignalsMapWidget::Type::Bool },
    { "bMatchingFinished",  SignalsMapWidget::Type::Bool },
    { "bMatchingBusy",      SignalsMapWidget::Type::Bool },
    { "bMatchingDetected",  SignalsMapWidget::Type::Bool },
    { "bMatchingLowArea",   SignalsMapWidget::Type::Bool },
    { "bTaskFault",         SignalsMapWidget::Type::Bool },
};

// Thin wrappers over the shared vc::gadget_meta helpers (qgadget_marco.h),
// bound to TaskLocalizeConfig's meta object. Falls back to the internal name
// when no Q_CLASSINFO("<name>_name", …) entry is present.
QString displayNameOf(const char *internalName) {
    return vc::gadget_meta::displayName(TaskLocalizeConfig::staticMetaObject,
                                        internalName);
}

// Setter dispatch — writes `value` into the matching field of the config's
// private data via the Q_PROPERTY system. Returns true on success.
bool writeConfigField(TaskLocalizeConfig &cfg, const QString &internalName,
                      const QString &value) {
    if (!vc::gadget_meta::writeProperty(TaskLocalizeConfig::staticMetaObject,
                                        &cfg, internalName, value)) {
        LOG_DEV_ERR << "writeConfigField: failed to write field" << internalName;
        return false;
    }
    return true;
}

QString readConfigField(const TaskLocalizeConfig &cfg, const QString &internalName) {
    return vc::gadget_meta::readProperty(TaskLocalizeConfig::staticMetaObject,
                                         &cfg, internalName).toString();
}

// ── Image conversion (matches the helpers used by the pattern dialogs) ──────
QPixmap matToPixmap(const cv::Mat &mat) {
    if (mat.empty()) return {};

    QImage img;
    if (mat.type() == CV_8UC1) {
        img = QImage(mat.data, mat.cols, mat.rows,
                     static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        img = QImage(rgb.data, rgb.cols, rgb.rows,
                     static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        img = QImage(rgba.data, rgba.cols, rgba.rows,
                     static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    } else {
        return {};
    }
    return QPixmap::fromImage(img);
}

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

} // namespace

LocalizationSettingWidget::LocalizationSettingWidget(
        std::shared_ptr<vc::model::ITask> task,
        ads::CDockWidget *dock, QWidget *parent)
    : ITaskWidget(task, dock, parent),
    ui(new Ui::LocalizationSettingWidget) {

    ui->setupUi(this);
    initWidget();
}

LocalizationSettingWidget::~LocalizationSettingWidget()
{
    delete ui;
}

void LocalizationSettingWidget::initWidget() {
    m_localizeTask = dynamic_cast<TaskLocalization *>(m_task.get());
    if (!m_localizeTask) {
        LOG_DEV_ERR << "LocalizationSettingWidget: task is not TaskLocalization";
        return;
    }

    m_config = m_localizeTask->taskLocalizeConfig();

    // ── Signals table: append fixed schema ───────────────────────────────
    for (const auto &spec : kSignalRows) {
        ui->listView_signals_map->appendRow(
            QString::fromUtf8(spec.internalName),
            displayNameOf(spec.internalName),
            spec.type);
    }

    // ── Wire combobox change → write config + push to task ──────────────
    connect(ui->cbb_vision_output_device,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        const QString id = ui->cbb_vision_output_device->currentData().toString();
        m_config.d->m_deviceBindings.setVisionOutputDeviceId(id);
        pushConfigToTask();
    });

    connect(ui->cbb_comm_device,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) {
        const QString id = ui->cbb_comm_device->currentData().toString();
        m_config.d->m_deviceBindings.setPrimaryPlcDeviceId(id);
        pushConfigToTask();
        // Refresh the SignalsMapWidget tag lists from the new comm device.
        refreshCommTags(id);
    });

    // ── Camera map ──────────────────────────────────────────────────────
    connect(ui->listView_cameras_map, &CameraMappingWidget::mappingChanged,
            this, [this](const QMap<int, QString> &m) {
        m_config.d->m_deviceBindings.setCameraNumberMap(m);
        pushConfigToTask();
    });

    // ── Camera workspace (ROI) ──────────────────────────────────────────
    connect(ui->listView_cameras_workspace, &CameraWorkspaceWidget::useWorkspaceToggled,
            this, &LocalizationSettingWidget::onWorkspaceUseToggled);
    connect(ui->listView_cameras_workspace, &CameraWorkspaceWidget::useConditionToggled,
            this, &LocalizationSettingWidget::onWorkspaceConditionToggled);
    connect(ui->listView_cameras_workspace, &CameraWorkspaceWidget::setWorkspaceRequested,
            this, &LocalizationSettingWidget::onWorkspaceSetRequested);

    // ── Signals map ─────────────────────────────────────────────────────
    connect(ui->listView_signals_map, &SignalsMapWidget::signalMappingChanged,
            this, [this](const QString &internalName, const QString &tag) {
        writeConfigField(m_config, internalName, tag);
        pushConfigToTask();
    });

    // ── React to assignment changes from the task ───────────────────────
    connect(m_localizeTask, &vc::model::ITask::devicesChanged,
            this, [this] {
        rebuildDeviceCombos();
        rebuildCameraList();
        rebuildCameraWorkspaceList();
        // The primary PLC binding may now point to a stale device - refresh tags.
        refreshCommTags(ui->cbb_comm_device->currentData().toString());
    });

    if (auto proj = m_localizeTask->project()) {
        if (auto dm = proj->deviceManager()) {
            connect(dm.get(), &vc::device::DeviceManager::deviceModified,
                    this, &LocalizationSettingWidget::refreshDevicePresentation);
        }
    }

    loadConfigToWidget();
}

void LocalizationSettingWidget::rebuildDeviceCombos() {
    if (!m_localizeTask) return;

    const QString currentOutput = ui->cbb_vision_output_device->currentData().toString();
    const QString currentComm   = ui->cbb_comm_device->currentData().toString();

    auto fill = [](QComboBox *cb,
                   const QList<std::shared_ptr<IDevice>> &devs,
                   const QString &keepId) {
        QSignalBlocker block(cb);
        cb->clear();
        cb->addItem(QString(), QString());          // empty
        for (const auto &d : devs) {
            cb->addItem(d->name(), d->id());
        }
        int idx = cb->findData(keepId);
        cb->setCurrentIndex(idx < 0 ? 0 : idx);
    };

    fill(ui->cbb_vision_output_device,
         m_localizeTask->assignedDevicesOfType(DeviceType::VisionOutput),
         currentOutput);

    // Communication device = any assigned PLC. Vendor sub-type (Mitsubishi MC,
    // future Omron FINS, …) is left for callers that need it via plcType().
    const auto commDevs = m_localizeTask->assignedDevicesOfType(DeviceType::PLC);
    fill(ui->cbb_comm_device, commDevs, currentComm);
}

void LocalizationSettingWidget::rebuildCameraList() {
    if (!m_localizeTask) return;
    const auto cams = m_localizeTask->assignedDevicesOfType(DeviceType::Camera);
    QMap<QString, QString> idToName;
    for (const auto &d : cams) {
        if (d) idToName.insert(d->id(), d->name());
    }

    QSignalBlocker block(ui->listView_cameras_map);
    ui->listView_cameras_map->setCameraOptions(idToName);
}

void LocalizationSettingWidget::rebuildCameraWorkspaceList() {
    if (!m_localizeTask) return;

    const auto cams = m_localizeTask->assignedDevicesOfType(DeviceType::Camera);
    QMap<QString, QString> idToName;
    for (const auto &d : cams) {
        if (d) idToName.insert(d->id(), d->name());
    }

    ui->listView_cameras_workspace->setCameras(idToName);

    for (auto it = idToName.cbegin(); it != idToName.cend(); ++it)
        refreshWorkspaceRow(it.key());
}

void LocalizationSettingWidget::refreshWorkspaceRow(const QString &cameraId) {
    const vc::model::CameraWorkspace ws = m_config.cameraWorkspace(cameraId);
    const QRectF roi(ws.roi.x, ws.roi.y, ws.roi.width, ws.roi.height);
    const QRectF condRoi(ws.conditionRoi.x, ws.conditionRoi.y,
                         ws.conditionRoi.width, ws.conditionRoi.height);
    ui->listView_cameras_workspace->setWorkspaceState(
        cameraId, ws.useWorkspace, roi,
        ws.useConditionWorkspace, condRoi,
        !ws.referenceImage.empty());
}

void LocalizationSettingWidget::refreshDevicePresentation(const QString &deviceId) {
    if (!m_localizeTask || deviceId.isEmpty() || !m_localizeTask->hasDevice(deviceId))
        return;

    loadConfigToWidget();
}

void LocalizationSettingWidget::onWorkspaceUseToggled(const QString &cameraId, bool enabled) {
    if (cameraId.isEmpty()) return;

    vc::model::CameraWorkspace ws = m_config.cameraWorkspace(cameraId);
    ws.useWorkspace = enabled;
    m_config.setCameraWorkspace(cameraId, ws);
    pushConfigToTask();

    refreshWorkspaceRow(cameraId);
}

void LocalizationSettingWidget::onWorkspaceConditionToggled(const QString &cameraId, bool enabled) {
    if (cameraId.isEmpty()) return;

    vc::model::CameraWorkspace ws = m_config.cameraWorkspace(cameraId);
    ws.useConditionWorkspace = enabled;
    m_config.setCameraWorkspace(cameraId, ws);
    pushConfigToTask();

    refreshWorkspaceRow(cameraId);
}

void LocalizationSettingWidget::onWorkspaceSetRequested(const QString &cameraId) {
    if (!m_localizeTask || cameraId.isEmpty()) return;

    vc::model::CameraWorkspace ws = m_config.cameraWorkspace(cameraId);

    QString camName = cameraId;
    if (auto proj = m_localizeTask->project()) {
        if (auto dm = proj->deviceManager()) {
            if (auto dev = dm->deviceById(cameraId)) camName = dev->name();
        }
    }

    WorkspaceSettingDialog dialog(this);
    dialog.setTitleText(tr("Workspace — %1").arg(camName));
    dialog.setInitial(
        matToPixmap(ws.referenceImage),
        QRectF(ws.roi.x, ws.roi.y, ws.roi.width, ws.roi.height),
        QRectF(ws.conditionRoi.x, ws.conditionRoi.y,
               ws.conditionRoi.width, ws.conditionRoi.height));

    // Serve "Grab from camera": grab a single shot through the camera runner.
    // Only available while the camera has a runner (Commission / Runtime).
    connect(&dialog, &WorkspaceSettingDialog::requestGrab, &dialog,
            [this, cameraId, &dialog]() {
        auto *runner = m_localizeTask->taskRunner()
            ? qobject_cast<vc::runtime::CameraRunner *>(
                  m_localizeTask->taskRunner()->runnerFor(cameraId))
            : nullptr;
        if (!runner) {
            QMessageBox::warning(&dialog, tr("Workspace"),
                tr("Camera is not available. Start Commission and connect the camera first."));
            return;
        }
        connect(runner, &vc::runtime::CameraRunner::grabFinished, &dialog,
                [&dialog](vc::device::GrabResult result) {
            if (result.isGrabSuccess && !result.frame.empty())
                dialog.setMainViewImage(matToPixmap(result.frame));
            else
                QMessageBox::warning(&dialog, QObject::tr("Workspace"),
                                     QObject::tr("Camera grab failed."));
        }, Qt::SingleShotConnection);
        runner->requestSingleShot();
    });

    if (dialog.exec() != QDialog::Accepted || !dialog.hasResult())
        return;

    // The dialog's ROIs are authoritative. Presence of an ROI implies "use it"
    // (matching the original behaviour); the row checkboxes still allow turning
    // either workspace off afterwards without losing the ROI.
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
    m_config.setCameraWorkspace(cameraId, ws);
    pushConfigToTask();

    refreshWorkspaceRow(cameraId);
}

void LocalizationSettingWidget::refreshCommTags(const QString &deviceId) {
    if (!m_localizeTask || deviceId.isEmpty()) {
        ui->listView_signals_map->setBoolTags({});
        ui->listView_signals_map->setNumberTags({});
        return;
    }
    auto proj = m_localizeTask->project();
    if (!proj) return;
    auto dm = proj->deviceManager();
    if (!dm) return;
    auto dev = dm->deviceById(deviceId);
    if (!dev) {
        ui->listView_signals_map->setBoolTags({});
        ui->listView_signals_map->setNumberTags({});
        return;
    }
    auto *digitalProvider = dynamic_cast<vc::device::IDigitalIoProvider *>(dev.get());
    auto *wordProvider = dynamic_cast<vc::device::IWordIoProvider *>(dev.get());
    if (!digitalProvider || !wordProvider) {
        LOG_DEV_ERR << "LocalizationSettingWidget: selected PLC device does not expose IO tag capability"
                    << deviceId;
        ui->listView_signals_map->setBoolTags({});
        ui->listView_signals_map->setNumberTags({});
        return;
    }

    ui->listView_signals_map->setBoolTags(digitalProvider->availableDigitalIoNames());
    ui->listView_signals_map->setNumberTags(wordProvider->availableWordIoNames());
}

void LocalizationSettingWidget::loadConfigToWidget() {
    if (!m_localizeTask) return;
    m_config = m_localizeTask->taskLocalizeConfig();

    rebuildDeviceCombos();
    rebuildCameraList();

    // Select stored device ids
    {
        QSignalBlocker b(ui->cbb_vision_output_device);
        int idx = ui->cbb_vision_output_device->findData(
            m_config.d->m_deviceBindings.visionOutputDeviceId());
        ui->cbb_vision_output_device->setCurrentIndex(idx < 0 ? 0 : idx);
    }
    {
        QSignalBlocker b(ui->cbb_comm_device);
        int idx = ui->cbb_comm_device->findData(
            m_config.d->m_deviceBindings.primaryPlcDeviceId());
        ui->cbb_comm_device->setCurrentIndex(idx < 0 ? 0 : idx);
    }

    refreshCommTags(m_config.d->m_deviceBindings.primaryPlcDeviceId());

    // Camera map
    {
        QSignalBlocker b(ui->listView_cameras_map);
        ui->listView_cameras_map->setCurrentMapping(
            m_config.d->m_deviceBindings.cameraNumberMap());
    }

    // Camera workspace (ROI) rows
    rebuildCameraWorkspaceList();

    // Signal map values
    for (const auto &spec : kSignalRows) {
        const QString name = QString::fromUtf8(spec.internalName);
        ui->listView_signals_map->setRowValue(name, readConfigField(m_config, name));
    }
}

void LocalizationSettingWidget::loadConfigToTask() {
    pushConfigToTask();
}

void LocalizationSettingWidget::pushConfigToTask() {
    if (!m_localizeTask) return;
    m_localizeTask->setTaskLocalizeConfig(m_config);
}
