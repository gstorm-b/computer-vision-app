#include "model/localization_runtime_controller.h"

#include <QMetaObject>
#include <QTimer>

#include <memory>

#include "core/logger/app_logger.h"
#include "matching/match_group.h"
#include "matching/match_pattern.h"
#include "model/robot_kinematic_picking_checker.h"
#include "runtime/camera_runner.h"
#include "runtime/plc_runner.h"
#include "runtime/vision_output_runner.h"

namespace vc::model {

namespace {

bool isHealthyStatus(vc::device::ConnectStatus status)
{
    return status == vc::device::ConnectStatus::Connected;
}

QString connectStatusName(vc::device::ConnectStatus status)
{
    switch (status) {
    case vc::device::ConnectStatus::NoConnection:
        return QStringLiteral("NoConnection");
    case vc::device::ConnectStatus::Disconnected:
        return QStringLiteral("Disconnected");
    case vc::device::ConnectStatus::Connected:
        return QStringLiteral("Connected");
    case vc::device::ConnectStatus::LostConnected:
        return QStringLiteral("LostConnected");
    case vc::device::ConnectStatus::ConnectFailed:
        return QStringLiteral("ConnectFailed");
    case vc::device::ConnectStatus::Connecting:
        return QStringLiteral("Connecting");
    }

    return QStringLiteral("Unknown");
}

QString buildRecoveryProgressMessage(const LocalizationRecoveryPolicy &policy,
                                     const QString &deviceId,
                                     int retryCount,
                                     vc::device::ConnectStatus status)
{
    return QStringLiteral("Task runtime recovering: role=%1 deviceId=%2 status=%3 retries=%4/%5 intervalMs=%6")
        .arg(policy.roleName,
             deviceId,
             connectStatusName(status),
             QString::number(retryCount),
             QString::number(policy.maxRetries),
             QString::number(policy.retryIntervalMs));
}

QString buildRecoveryReadyMessage(const LocalizationRecoveryPolicy &policy,
                                  const QString &deviceId,
                                  int retryCount)
{
    return QStringLiteral("Task runtime ready: role=%1 deviceId=%2 retries=%3/%4")
        .arg(policy.roleName,
             deviceId,
             QString::number(retryCount),
             QString::number(policy.maxRetries));
}

} // namespace

LocalizationRuntimeController::LocalizationRuntimeController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<LocalizationRuntimeController::CycleResult>(
        "vc::model::LocalizationRuntimeController::CycleResult");
    qRegisterMetaType<LocalizationRuntimeController::TaskLogEntry>(
        "vc::model::LocalizationRuntimeController::TaskLogEntry");
    qRegisterMetaType<LocalizationFaultCode>("vc::model::LocalizationFaultCode");
    qRegisterMetaType<CameraWorkspace>("CameraWorkspace");
    qRegisterMetaType<CameraWorkspace>("vc::model::CameraWorkspace");
    qRegisterMetaType<std::shared_ptr<mtc::IRobotPickingChecker>>(
        "std::shared_ptr<mtc::IRobotPickingChecker>");
}

void LocalizationRuntimeController::configure(const TaskLocalizeConfig &config)
{
    m_config = config;
    m_signalMapper.configure(m_config);
}

void LocalizationRuntimeController::setActiveCameraNumber(int cameraNumber)
{
    if (m_cycleState == CycleState::Running) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Camera change ignored while cycle is running."));
        return;
    }

    if (!m_context.cameraRunners.value(cameraNumber)) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Camera change ignored: camera runner is not available."));
        publishBoolSignal(QStringLiteral("bCameraValid"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::CameraLost));
        return;
    }

    auto *previousRunner = activeCameraRunner();
    m_activeCameraNumber = cameraNumber;
    publishBoolSignal(QStringLiteral("bTaskReady"), false);
    bindActiveCameraRole(m_activeCameraNumber);
    publishNumberSignal(QStringLiteral("nActiveCamera"), cameraNumber);

    auto *newRunner = activeCameraRunner();
    if (previousRunner && previousRunner != newRunner) {
        previousRunner->requestDisconnect();
    }

    const bool calibrated = validateActiveCameraCalibration();
    // Rebuild the pickability checker against the new camera's calibration.
    rebuildPickingChecker();
    publishBoolSignal(QStringLiteral("bCameraValid"), calibrated);
    if (!calibrated) {
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::CalibrationInvalid));
        m_cycleState = CycleState::Faulted;
        emit runtimeFault(QStringLiteral("Active camera calibration is invalid."));
        return;
    }

    if (newRunner) {
        newRunner->requestConnect();
        // Refresh the active workspace from the new camera, guarded like the
        // requestConnect above so a degraded switch cannot deref a null runner.
        m_context.activeCameraWorkspace =
            m_config.cameraWorkspace(newRunner->device()->id());
        m_activeCameraWorkspace = m_context.activeCameraWorkspace;
    }
    if (allRequiredRolesHealthy()) {
        markRuntimeReady(QStringLiteral("Active camera changed. Runtime ready."));
    }
}

void LocalizationRuntimeController::setActivePatternGroupNumber(int patternGroupNumber)
{
    if (m_cycleState == CycleState::Running) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Pattern group change ignored while cycle is running."));
        return;
    }

    m_activePatternGroupNumber = patternGroupNumber;
    publishNumberSignal(QStringLiteral("nActivePatternGroup"), patternGroupNumber);
    const bool valid = validateActivePatternGroup();
    publishBoolSignal(QStringLiteral("bPatternValid"), valid);
    if (!valid) {
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::PatternInvalid));
    }
}

void LocalizationRuntimeController::setRecoveryPolicies(
    const LocalizationRecoveryPolicy &cameraPolicy,
    const LocalizationRecoveryPolicy &plcPolicy,
    const LocalizationRecoveryPolicy &visionOutputPolicy)
{
    m_cameraRecoveryPolicy = cameraPolicy;
    m_plcRecoveryPolicy = plcPolicy;
    m_visionOutputRecoveryPolicy = visionOutputPolicy;
}

LocalizationRuntimeController::SetupResult
LocalizationRuntimeController::setup(const RuntimeContext &context)
{
    SetupResult result;
    m_valid = false;
    m_context = context;
    m_config = context.config;
    m_signalMapper.configure(m_config);
    m_cycleState = CycleState::NotReady;
    m_lastExecuteTrigger = false;
    m_activeCycleId = 0;
    m_pendingCycleResult = CycleResult();
    resetRuntimeBindings();

    result.primaryPlcDeviceId = context.primaryPlcDeviceId;
    result.visionOutputDeviceId = context.visionOutputDeviceId;
    const QMap<int, QString> cameraMap = context.cameraDeviceIds;

    if (result.primaryPlcDeviceId.isEmpty() || !context.primaryPlcRunner) {
        result.errors.append(QStringLiteral("Missing primary_plc role runner."));
    }
    if (result.visionOutputDeviceId.isEmpty() || !context.visionOutputRunner) {
        result.errors.append(QStringLiteral("Missing vision_output role runner."));
    }

    if (cameraMap.isEmpty() || context.cameraRunners.isEmpty()) {
        result.errors.append(QStringLiteral("Missing camera_number role binding."));
    } else {
        for (auto it = cameraMap.cbegin(); it != cameraMap.cend(); ++it) {
            if (!context.cameraRunners.value(it.key())) {
                result.errors.append(QStringLiteral("camera_number_%1 runner is not registered: %2")
                                         .arg(it.key())
                                         .arg(it.value()));
            }
        }
    }

    disconnect(m_plcValueConnection);
    if (context.primaryPlcRunner) {
        m_plcValueConnection = connect(
            context.primaryPlcRunner.data(),
            &vc::runtime::PlcRunner::valueChanged,
            this,
            &LocalizationRuntimeController::handlePlcValues,
            Qt::UniqueConnection);
    }

    bindFixedRoleRunners();
    m_activeCameraNumber = context.activeCameraNumber;
    if (m_activeCameraNumber < 0 && !cameraMap.isEmpty()) {
        m_activeCameraNumber = cameraMap.firstKey();
    }
    bindActiveCameraRole(m_activeCameraNumber);
    m_activePatternGroupNumber = context.activePatternGroupNumber;
    if (m_activePatternGroupNumber < 0 && !context.patternGroups.isEmpty()) {
        m_activePatternGroupNumber = context.patternGroups.firstKey();
    }

    if (!validateActivePatternGroup(&result.errors)) {
        publishBoolSignal(QStringLiteral("bPatternValid"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::PatternInvalid));
    }

    if (!validateActiveCameraCalibration(&result.errors)) {
        publishBoolSignal(QStringLiteral("bCameraValid"), false);
        publishBoolSignal(QStringLiteral("bTaskFault"), true);
        publishNumberSignal(QStringLiteral("nFaultCode"),
                            localizationFaultCodeValue(LocalizationFaultCode::CalibrationInvalid));
    }

    // Build the robot-pickability checker once for the runtime (active camera +
    // robot config now established); rebuilt later only on active-camera change.
    rebuildPickingChecker();

    result.valid = result.errors.isEmpty();
    m_valid = result.valid;

    for (const QString &error : result.errors)
        LOG_DEV_ERR << "LocalizationRuntimeController setup:" << error;

    if (m_valid) {
        requestRoleConnectNow(RunnerRole::PrimaryPlc);
        requestRoleConnectNow(RunnerRole::VisionOutput);
        requestRoleConnectNow(RunnerRole::Camera);
        if (allRequiredRolesHealthy()) {
            markRuntimeReady(QStringLiteral("Runtime ready."));
        } else {
            appendTaskLog(QStringLiteral("INFO"),
                          QStringLiteral("Runtime setup valid. Waiting for device connections."));
        }
    } else {
        m_cycleState = CycleState::Faulted;
    }

    return result;
}

void LocalizationRuntimeController::execute()
{
    if (!m_valid) {
        LOG_DEV_ERR << "LocalizationRuntimeController::execute - runtime is not set up";
        return;
    }

    if (m_cycleState != CycleState::ReadyForTrigger) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Manual localization request ignored: task is not ready."));
        return;
    }

    startCycle();
}

void LocalizationRuntimeController::handlePlcValues(const QMap<QString, QVariant> &values)
{
    if (values.isEmpty()) {
        LOG_USER_ERR << "Communication device passed an empty values map.";
        return;
    }

    const QList<LocalizationSignalEvent> events = m_signalMapper.mapValues(values);
    for (const LocalizationSignalEvent &event : events) {
        emit signalChanged(event.name, event.value);

        if (event.name == QStringLiteral("nActiveCamera")) {
            emit cameraNumberChanged(event.value);
            const int cameraNumber = event.value.toInt();
            if (cameraNumber > 0) {
                setActiveCameraNumber(cameraNumber);
            }
        } else if (event.name == QStringLiteral("nActivePatternGroup")) {
            emit patternNumberChanged(event.value);
            bool ok = false;
            const int patternGroupNumber = event.value.toInt(&ok);
            if (ok && patternGroupNumber > 0) {
                setActivePatternGroupNumber(patternGroupNumber);
            }
        } else if (event.name == QStringLiteral("bExecuteTrigger")) {
            const bool trigger = event.value.toBool();
            const bool risingEdge = trigger && !m_lastExecuteTrigger;
            const bool fallingEdge = !trigger && m_lastExecuteTrigger;
            m_lastExecuteTrigger = trigger;

            if (risingEdge) {
                if (m_cycleState == CycleState::ReadyForTrigger) {
                    startCycle();
                } else {
                    appendTaskLog(QStringLiteral("WARN"),
                                  QStringLiteral("Trigger ignored: task is not ready."));
                }
            } else if (fallingEdge &&
                       m_cycleState == CycleState::WaitingTriggerReset) {
                publishBoolSignal(QStringLiteral("bMatchingFinished"), false);
                if (m_pendingCycleResult.faulted) {
                    m_cycleState = CycleState::Recovering;
                } else {
                    markRuntimeReady(QStringLiteral("Trigger reset. Runtime ready."));
                }
            }
        }
    }
}

void LocalizationRuntimeController::resetRuntimeBindings()
{
    disconnect(m_plcValueConnection);
    m_plcValueConnection = QMetaObject::Connection();
    disconnect(m_cameraGrabConnection);
    disconnect(m_cameraCommandConnection);
    disconnect(m_visionOutputResultConnection);
    m_cameraGrabConnection = QMetaObject::Connection();
    m_cameraCommandConnection = QMetaObject::Connection();
    m_visionOutputResultConnection = QMetaObject::Connection();

    clearRoleContext(RunnerRole::PrimaryPlc);
    clearRoleContext(RunnerRole::VisionOutput);
    clearRoleContext(RunnerRole::Camera);
}

void LocalizationRuntimeController::bindFixedRoleRunners()
{
    const QString plcId = m_context.primaryPlcDeviceId;
    if (m_context.primaryPlcRunner) {
        bindRoleContext(RunnerRole::PrimaryPlc,
                        m_context.primaryPlcRunner.data(),
                        plcId,
                        m_plcRecoveryPolicy);
    }

    const QString visionOutputId = m_context.visionOutputDeviceId;
    if (m_context.visionOutputRunner) {
        bindRoleContext(RunnerRole::VisionOutput,
                        m_context.visionOutputRunner.data(),
                        visionOutputId,
                        m_visionOutputRecoveryPolicy);
    }
}

void LocalizationRuntimeController::bindActiveCameraRole(int cameraNumber)
{
    const QString cameraId = m_context.cameraDeviceIds.value(cameraNumber);
    auto runner = m_context.cameraRunners.value(cameraNumber);
    if (cameraId.isEmpty() || !runner) {
        clearRoleContext(RunnerRole::Camera);
        return;
    }

    bindRoleContext(RunnerRole::Camera,
                    runner.data(),
                    cameraId,
                    m_cameraRecoveryPolicy);
}

void LocalizationRuntimeController::bindRoleContext(
    RunnerRole role,
    vc::runtime::IDeviceRunner *runner,
    const QString &deviceId,
    const LocalizationRecoveryPolicy &policy)
{
    clearRoleContext(role);

    if (!runner) {
        return;
    }

    RoleRecoveryContext context;
    context.roleName = policy.roleName;
    context.deviceId = deviceId;
    context.runner = runner;
    context.policy = policy;

    m_recoveryContexts.insert(roleKey(role), context);

    switch (role) {
    case RunnerRole::PrimaryPlc:
        connect(runner, &vc::runtime::IDeviceRunner::connectStatusChanged,
                this, &LocalizationRuntimeController::onPrimaryPlcStatusChanged,
                Qt::UniqueConnection);
        connect(runner, &vc::runtime::IDeviceRunner::errorOccurred,
                this, &LocalizationRuntimeController::onPrimaryPlcError,
                Qt::UniqueConnection);
        break;
    case RunnerRole::VisionOutput:
        connect(runner, &vc::runtime::IDeviceRunner::connectStatusChanged,
                this, &LocalizationRuntimeController::onVisionOutputStatusChanged,
                Qt::UniqueConnection);
        connect(runner, &vc::runtime::IDeviceRunner::errorOccurred,
                this, &LocalizationRuntimeController::onVisionOutputError,
                Qt::UniqueConnection);
        break;
    case RunnerRole::Camera:
        connect(runner, &vc::runtime::IDeviceRunner::connectStatusChanged,
                this, &LocalizationRuntimeController::onCameraStatusChanged,
                Qt::UniqueConnection);
        connect(runner, &vc::runtime::IDeviceRunner::errorOccurred,
                this, &LocalizationRuntimeController::onCameraError,
                Qt::UniqueConnection);
        break;
    }
}

void LocalizationRuntimeController::clearRoleContext(RunnerRole role)
{
    const int key = roleKey(role);
    if (!m_recoveryContexts.contains(key)) {
        return;
    }

    const RoleRecoveryContext context = m_recoveryContexts.value(key);
    if (context.runner) {
        disconnect(context.runner, nullptr, this, nullptr);
    }
    m_recoveryContexts.remove(key);
}

bool LocalizationRuntimeController::allRequiredRolesHealthy() const
{
    for (RunnerRole role : {RunnerRole::PrimaryPlc, RunnerRole::VisionOutput, RunnerRole::Camera}) {
        const int key = roleKey(role);
        if (!m_recoveryContexts.contains(key)) {
            return false;
        }

        const RoleRecoveryContext context = m_recoveryContexts.value(key);
        if (!context.runner || !context.runner->device() ||
            !isHealthyStatus(context.runner->device()->connectStatus())) {
            return false;
        }
    }

    return true;
}

void LocalizationRuntimeController::markRuntimeReady(const QString &message)
{
    if (m_cycleState == CycleState::Running) {
        return;
    }

    if (!m_valid || !allRequiredRolesHealthy() ||
        !validateActivePatternGroup() ||
        !validateActiveCameraCalibration()) {
        return;
    }

    m_cycleState = CycleState::ReadyForTrigger;
    publishInitialReadyOutputs();
    appendTaskLog(QStringLiteral("INFO"), message);
    emit runtimeReady(message);
}

void LocalizationRuntimeController::publishBoolSignal(const QString &name, bool value)
{
    emit signalChanged(name, value);

    const QString tag = m_signalMapper.tagForSignalName(name);
    if (tag.isEmpty()) {
        return;
    }

    if (auto *runner = primaryPlcRunner()) {
        runner->requestWriteDigitalIo(tag, value);
    }
}

void LocalizationRuntimeController::publishNumberSignal(const QString &name, int value)
{
    emit signalChanged(name, value);

    const QString tag = m_signalMapper.tagForSignalName(name);
    if (tag.isEmpty()) {
        return;
    }

    if (auto *runner = primaryPlcRunner()) {
        runner->requestWriteWordIo(tag, static_cast<qint16>(value));
    }
}

void LocalizationRuntimeController::publishInitialReadyOutputs()
{
    publishBoolSignal(QStringLiteral("bTaskReady"), true);
    publishBoolSignal(QStringLiteral("bCameraValid"), true);
    publishBoolSignal(QStringLiteral("bPatternValid"), true);
    publishBoolSignal(QStringLiteral("bMatchingBusy"), false);
    publishBoolSignal(QStringLiteral("bMatchingFinished"), false);
    publishBoolSignal(QStringLiteral("bMatchingDetected"), false);
    publishBoolSignal(QStringLiteral("bMatchingLowArea"), false);
    publishBoolSignal(QStringLiteral("bTaskFault"), false);
    publishNumberSignal(QStringLiteral("nDetectedNumber"), 0);
    publishNumberSignal(QStringLiteral("nFaultCode"),
                        localizationFaultCodeValue(LocalizationFaultCode::None));
}

void LocalizationRuntimeController::publishCycleStartOutputs()
{
    publishBoolSignal(QStringLiteral("bTaskReady"), false);
    publishBoolSignal(QStringLiteral("bMatchingBusy"), true);
    publishBoolSignal(QStringLiteral("bMatchingFinished"), false);
    publishBoolSignal(QStringLiteral("bMatchingDetected"), false);
    publishBoolSignal(QStringLiteral("bMatchingLowArea"), false);
    publishBoolSignal(QStringLiteral("bTaskFault"), false);
    publishNumberSignal(QStringLiteral("nDetectedNumber"), 0);
    publishNumberSignal(QStringLiteral("nFaultCode"),
                        localizationFaultCodeValue(LocalizationFaultCode::None));
}

void LocalizationRuntimeController::publishCycleSuccessOutputs(const CycleResult &result)
{
    publishBoolSignal(QStringLiteral("bMatchingBusy"), false);
    publishBoolSignal(QStringLiteral("bMatchingFinished"), true);
    publishBoolSignal(QStringLiteral("bMatchingDetected"), result.detectedNumber > 0);
    publishBoolSignal(QStringLiteral("bMatchingLowArea"), result.lowArea);
    publishBoolSignal(QStringLiteral("bTaskFault"), false);
    publishNumberSignal(QStringLiteral("nDetectedNumber"), result.detectedNumber);
    publishNumberSignal(QStringLiteral("nFaultCode"),
                        localizationFaultCodeValue(LocalizationFaultCode::None));
}

void LocalizationRuntimeController::publishCycleFaultOutputs(LocalizationFaultCode code)
{
    publishBoolSignal(QStringLiteral("bMatchingBusy"), false);
    publishBoolSignal(QStringLiteral("bMatchingFinished"), true);
    publishBoolSignal(QStringLiteral("bTaskReady"), false);
    publishBoolSignal(QStringLiteral("bMatchingDetected"), false);
    publishBoolSignal(QStringLiteral("bMatchingLowArea"), false);
    publishBoolSignal(QStringLiteral("bTaskFault"), true);
    publishNumberSignal(QStringLiteral("nDetectedNumber"), 0);
    publishNumberSignal(QStringLiteral("nFaultCode"), localizationFaultCodeValue(code));
}

void LocalizationRuntimeController::appendTaskLog(const QString &severity,
                                                  const QString &message)
{
    TaskLogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.severity = severity;
    entry.message = message;
    emit taskLogAppended(entry);
}

bool LocalizationRuntimeController::validateActivePatternGroup(QStringList *errors) const
{
    int groupNumber = m_activePatternGroupNumber;
    if (groupNumber < 0 && !m_context.patternGroups.isEmpty()) {
        groupNumber = m_context.patternGroups.firstKey();
    }

    auto group = m_context.patternGroups.value(groupNumber);
    if (!group) {
        if (errors) errors->append(QStringLiteral("Active pattern group is missing."));
        return false;
    }

    for (const auto &pattern : group->patterns()) {
        if (pattern && !pattern->config().m_rawImage.empty()) {
            return true;
        }
    }

    if (errors) errors->append(QStringLiteral("Active pattern group has no train images."));
    return false;
}

bool LocalizationRuntimeController::validateActiveCameraCalibration(QStringList *errors) const
{
    const calib::Calibrator calibrator = m_context.cameraCalibrators.value(m_activeCameraNumber);
    if (!calibrator.isCalibrated()) {
        if (errors) errors->append(QStringLiteral("Active camera calibration is invalid."));
        return false;
    }

    return true;
}

void LocalizationRuntimeController::rebuildPickingChecker()
{
    m_pickingChecker.reset();
    if (!m_context.robotCheckConfig.enabled)
        return;

    const calib::Calibrator calibrator =
        m_context.cameraCalibrators.value(m_activeCameraNumber);
    if (!calibrator.isCalibrated())
        return;

    m_pickingChecker = std::make_shared<RobotKinematicPickingChecker>(
        calibrator, m_context.robotCheckConfig);
}

std::shared_ptr<mtc::MatchGroup>
LocalizationRuntimeController::snapshotActivePatternGroup() const
{
    int groupNumber = m_activePatternGroupNumber;
    if (groupNumber < 0 && !m_context.patternGroups.isEmpty()) {
        groupNumber = m_context.patternGroups.firstKey();
    }

    return m_context.patternGroups.value(groupNumber);
}

QVector<vc::device::VisionOutputPosition>
LocalizationRuntimeController::buildVisionOutputPositions(
    const mtc::MatchResult &matchResult,
    QVector<ResultRow> *rows,
    LocalizationFaultCode *faultCode) const
{
    QVector<vc::device::VisionOutputPosition> positions;
    if (faultCode) {
        *faultCode = LocalizationFaultCode::None;
    }

    const calib::Calibrator calibrator = m_context.cameraCalibrators.value(m_activeCameraNumber);
    if (!calibrator.isCalibrated()) {
        if (faultCode) *faultCode = LocalizationFaultCode::CalibrationInvalid;
        return positions;
    }

    int index = 1;
    for (const mtc::MatchedObject &object : matchResult.Objects) {
        ResultRow row;
        row.index = index++;
        row.patternName = QString::fromStdWString(object.pattern_name);
        row.score = object.matched_Score;
        row.imageX = object.point_Center.x;
        row.imageY = object.point_Center.y;
        row.imageR = object.point_angle;

        // image to real world position
        cv::Point2f pick_center = object.point_Center;
        if (m_context.activeCameraWorkspace.useWorkspace) {
            pick_center += matchResult.cropOffsetPoint;
        }

        cv::Point3f worldPoint = calibrator.imageToRobot(pick_center);
            const double worldR = calibrator.rotateImageToRobot(object.point_angle);
            // row.world.r = worldR;
            row.world.r = -worldR;

        worldPoint = calibrator.translateWithZAxis(
            worldPoint, object.point_offset, row.world.r, false);

        row.world.x = worldPoint.x;
        row.world.y = worldPoint.y;
        row.world.z = worldPoint.z;

        // if (object.hasCollision() && object.isOutsideConditionRoi()) {
        //     row.status = QStringLiteral("Skipped: collision, outside");
        // } else if (object.hasCollision()) {
        //     row.status = QStringLiteral("Skipped: collision");
        // } else if (object.isOutsideConditionRoi()) {
        //     row.status = QStringLiteral("Skipped: outside");
        // }  else {
        //     row.status = QStringLiteral("Sent");
        //     positions.append(row.world);
        // }

        if ((!object.hasCollision()) && (!object.isOutsideConditionRoi()) && object.isPossibleToPick()) {
            if (positions.size() < 2) {
                row.status = QStringLiteral("Sent");
                positions.append(row.world);
            } else {
                row.status = QStringLiteral("Skipped");
            }
        } else {
            QString tempstr = object.hasCollision() ? tr("Collision") : "";
            tempstr += (object.isOutsideConditionRoi()
                                    ? ((tempstr.isEmpty() ? "" : ", ") + tr("Outside"))
                                    : "");
            tempstr += (!object.isPossibleToPick()
                                    ? ((tempstr.isEmpty() ? "" : ", ") + tr("Unpickable"))
                                    : "");
            row.status = QStringLiteral("Skipped: %1").arg(tempstr);
        }

        if (rows) {
            rows->append(row);
        }
    }

    return positions;
}

vc::runtime::CameraRunner *LocalizationRuntimeController::activeCameraRunner() const
{
    const int key = roleKey(RunnerRole::Camera);
    if (!m_recoveryContexts.contains(key) || !m_recoveryContexts.value(key).runner) {
        return nullptr;
    }
    return qobject_cast<vc::runtime::CameraRunner *>(m_recoveryContexts.value(key).runner.data());
}

vc::runtime::PlcRunner *LocalizationRuntimeController::primaryPlcRunner() const
{
    const int key = roleKey(RunnerRole::PrimaryPlc);
    if (!m_recoveryContexts.contains(key) || !m_recoveryContexts.value(key).runner) {
        return nullptr;
    }
    return qobject_cast<vc::runtime::PlcRunner *>(m_recoveryContexts.value(key).runner.data());
}

vc::runtime::VisionOutputRunner *LocalizationRuntimeController::visionOutputRunner() const
{
    const int key = roleKey(RunnerRole::VisionOutput);
    if (!m_recoveryContexts.contains(key) || !m_recoveryContexts.value(key).runner) {
        return nullptr;
    }
    return qobject_cast<vc::runtime::VisionOutputRunner *>(m_recoveryContexts.value(key).runner.data());
}

void LocalizationRuntimeController::startCycle()
{
    if (m_cycleState != CycleState::ReadyForTrigger) {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Cycle start ignored: runtime is not ready."));
        return;
    }

    if (!validateActivePatternGroup()) {
        abortCycle(LocalizationFaultCode::PatternInvalid,
                   QStringLiteral("Active pattern group is invalid."));
        return;
    }

    if (!validateActiveCameraCalibration()) {
        abortCycle(LocalizationFaultCode::CalibrationInvalid,
                   QStringLiteral("Active camera calibration is invalid."));
        return;
    }

    auto *cameraRunner = activeCameraRunner();
    if (!cameraRunner) {
        abortCycle(LocalizationFaultCode::CameraLost,
                   QStringLiteral("Active camera runner is not available."));
        return;
    }

    m_cycleState = CycleState::Running;
    ++m_activeCycleId;
    m_pendingCycleResult = CycleResult();
    publishCycleStartOutputs();
    emit runtimeCycleStarted(QStringLiteral("Localization cycle started."));
    appendTaskLog(QStringLiteral("INFO"),
                  QStringLiteral("Trigger accepted. Localization cycle started."));

    disconnect(m_cameraGrabConnection);
    disconnect(m_cameraCommandConnection);
    m_cameraGrabConnection = connect(cameraRunner,
                                     &vc::runtime::CameraRunner::grabFinished,
                                     this,
                                     &LocalizationRuntimeController::onCameraGrabFinished,
                                     Qt::SingleShotConnection);
    m_cameraCommandConnection = connect(cameraRunner,
                                        &vc::runtime::CameraRunner::commandFinished,
                                        this,
                                        &LocalizationRuntimeController::onCameraCommandFinished,
                                        Qt::UniqueConnection);
    cameraRunner->requestSingleShot();
}

void LocalizationRuntimeController::abortCycle(LocalizationFaultCode code,
                                               const QString &message)
{
    disconnect(m_cameraGrabConnection);
    disconnect(m_cameraCommandConnection);
    disconnect(m_visionOutputResultConnection);
    m_cameraGrabConnection = QMetaObject::Connection();
    m_cameraCommandConnection = QMetaObject::Connection();
    m_visionOutputResultConnection = QMetaObject::Connection();
    ++m_activeCycleId;

    m_pendingCycleResult.faulted = true;
    m_pendingCycleResult.faultCode = code;
    m_pendingCycleResult.detectedNumber = 0;
    m_pendingCycleResult.sentNumber = 0;

    publishCycleFaultOutputs(code);
    emit cycleResultUpdated(m_pendingCycleResult);
    appendTaskLog(QStringLiteral("ERROR"),
                  QStringLiteral("%1 fault=%2")
                      .arg(message, localizationFaultCodeName(code)));

    m_cycleState = m_lastExecuteTrigger ? CycleState::WaitingTriggerReset
                                        : CycleState::Recovering;
    emit runtimeRecovering(message);
}

void LocalizationRuntimeController::handleRoleStatusChanged(RunnerRole role,
                                                            vc::device::ConnectStatus status)
{
    const int key = roleKey(role);
    if (!m_recoveryContexts.contains(key)) {
        return;
    }

    RoleRecoveryContext context = m_recoveryContexts.value(key);
    if (!context.runner) {
        m_recoveryContexts.remove(key);
        return;
    }

    if (isHealthyStatus(status)) {
        const int retryHistory = context.retryCount;
        const bool wasRecovering = retryHistory > 0 || context.retryScheduled;
        context.retryCount = 0;
        context.retryScheduled = false;
        context.faultRaised = false;
        m_recoveryContexts.insert(key, context);
        if (m_cycleState == CycleState::Running ||
            m_cycleState == CycleState::WaitingTriggerReset) {
            return;
        }
        if (allRequiredRolesHealthy()) {
            const QString readyMessage = wasRecovering
                                             ? buildRecoveryReadyMessage(
                context.policy,
                context.deviceId,
                retryHistory)
                                             : QStringLiteral("Runtime ready.");
            markRuntimeReady(readyMessage);
        }
        return;
    }

    if (m_cycleState == CycleState::Running) {
        switch (role) {
        case RunnerRole::Camera:
            abortCycle(LocalizationFaultCode::CameraLost,
                       QStringLiteral("Active camera lost during localization cycle."));
            break;
        case RunnerRole::VisionOutput:
            abortCycle(LocalizationFaultCode::VisionOutputLost,
                       QStringLiteral("Vision output lost during localization cycle."));
            break;
        case RunnerRole::PrimaryPlc:
            abortCycle(LocalizationFaultCode::PlcLost,
                       QStringLiteral("Primary PLC lost during localization cycle."));
            break;
        }
    }

    const LocalizationRecoveryAction action = decideRecoveryAction(
        context.policy,
        status,
        context.retryCount,
        context.retryScheduled);

    if (action == LocalizationRecoveryAction::RetryScheduled) {
        context.retryScheduled = true;
        context.retryCount += 1;
        m_recoveryContexts.insert(key, context);
        emit runtimeRecovering(buildRecoveryProgressMessage(
            context.policy,
            context.deviceId,
            context.retryCount,
            status));
        scheduleRoleReconnect(role);
        return;
    }

    if (action == LocalizationRecoveryAction::EscalateFault) {
        context.faultRaised = true;
        m_recoveryContexts.insert(key, context);
        raiseRoleFault(role, status);
    }
}

void LocalizationRuntimeController::scheduleRoleReconnect(RunnerRole role)
{
    const int key = roleKey(role);
    if (!m_recoveryContexts.contains(key)) {
        return;
    }

    const RoleRecoveryContext context = m_recoveryContexts.value(key);
    if (!context.runner) {
        return;
    }

    LOG_USER_WARN << "Recovery retry scheduled."
                  << "role=" << context.policy.roleName
                  << "deviceId=" << context.deviceId
                  << "attempt=" << context.retryCount
                  << "max=" << context.policy.maxRetries
                  << "intervalMs=" << context.policy.retryIntervalMs;

    QTimer::singleShot(context.policy.retryIntervalMs, this, [this, role]() {
        const int currentKey = roleKey(role);
        if (!m_recoveryContexts.contains(currentKey)) {
            return;
        }
        RoleRecoveryContext current = m_recoveryContexts.value(currentKey);
        current.retryScheduled = false;
        m_recoveryContexts.insert(currentKey, current);
        requestRoleConnectNow(role);
    });
}

void LocalizationRuntimeController::requestRoleConnectNow(RunnerRole role)
{
    const int key = roleKey(role);
    if (!m_recoveryContexts.contains(key)) {
        return;
    }

    RoleRecoveryContext context = m_recoveryContexts.value(key);
    if (!context.runner) {
        m_recoveryContexts.remove(key);
        return;
    }

    switch (role) {
    case RunnerRole::Camera: {
        auto *cameraRunner = qobject_cast<vc::runtime::CameraRunner *>(context.runner.data());
        if (cameraRunner) {
            cameraRunner->requestConnect();
        }
        break;
    }
    case RunnerRole::PrimaryPlc: {
        auto *plcRunner = qobject_cast<vc::runtime::PlcRunner *>(context.runner.data());
        if (plcRunner) {
            plcRunner->requestConnect();
        }
        break;
    }
    case RunnerRole::VisionOutput: {
        auto *visionRunner = qobject_cast<vc::runtime::VisionOutputRunner *>(context.runner.data());
        if (visionRunner) {
            visionRunner->requestConnect();
        }
        break;
    }
    }

    LOG_USER_WARN << "Recovery reconnect requested."
                  << "role=" << context.policy.roleName
                  << "deviceId=" << context.deviceId
                  << "attempt=" << context.retryCount
                  << "max=" << context.policy.maxRetries;
}

void LocalizationRuntimeController::raiseRoleFault(RunnerRole role,
                                                   vc::device::ConnectStatus status)
{
    const int key = roleKey(role);
    if (!m_recoveryContexts.contains(key)) {
        return;
    }

    const RoleRecoveryContext context = m_recoveryContexts.value(key);
    LocalizationFaultCode code = LocalizationFaultCode::InternalError;
    switch (role) {
    case RunnerRole::Camera:
        code = status == vc::device::ConnectStatus::ConnectFailed
                   ? LocalizationFaultCode::CameraConnectFailed
                   : LocalizationFaultCode::CameraLost;
        break;
    case RunnerRole::PrimaryPlc:
        code = LocalizationFaultCode::PlcLost;
        break;
    case RunnerRole::VisionOutput:
        code = LocalizationFaultCode::VisionOutputLost;
        break;
    }

    publishBoolSignal(QStringLiteral("bTaskFault"), true);
    publishNumberSignal(QStringLiteral("nFaultCode"), localizationFaultCodeValue(code));
    m_cycleState = CycleState::Faulted;

    const QString message = buildRecoveryFaultMessage(
        context.policy,
        context.deviceId,
        status,
        context.retryCount);

    LOG_USER_ERR << message;
    emit runtimeFault(message);
}

void LocalizationRuntimeController::onCameraGrabFinished(vc::device::GrabResult result)
{
    disconnect(m_cameraGrabConnection);
    disconnect(m_cameraCommandConnection);
    m_cameraGrabConnection = QMetaObject::Connection();
    m_cameraCommandConnection = QMetaObject::Connection();

    if (m_cycleState != CycleState::Running) {
        return;
    }

    if (!result.isGrabSuccess || result.frame.empty()) {
        abortCycle(LocalizationFaultCode::CameraGrabTimeout,
                   QStringLiteral("Camera grab failed or returned an empty frame."));
        return;
    }

    const auto group = snapshotActivePatternGroup();
    if (!group) {
        abortCycle(LocalizationFaultCode::PatternInvalid,
                   QStringLiteral("Active pattern group snapshot failed."));
        return;
    }

    // The robot-pickability checker is built once at runtime setup and rebuilt
    // only on active-camera change (rebuildPickingChecker), not per cycle.
    m_pendingCycleResult.rawImage = result.frame.clone();
    emit runtimeMatchingRequested(m_activeCycleId, group, m_activeCameraWorkspace,
                                  result.frame.clone(), m_pickingChecker);
}

void LocalizationRuntimeController::onRuntimeMatchingFinished(int cycleId,
                                                              mtc::MatchResult matchResult)
{
    if (cycleId != m_activeCycleId || m_cycleState != CycleState::Running) {
        return;
    }

    QVector<ResultRow> rows;
    LocalizationFaultCode conversionFault = LocalizationFaultCode::None;
    const QVector<vc::device::VisionOutputPosition> positions =
        buildVisionOutputPositions(matchResult, &rows, &conversionFault);
    if (conversionFault != LocalizationFaultCode::None) {
        abortCycle(conversionFault,
                   QStringLiteral("Failed to convert match result to world coordinates."));
        return;
    }

    m_pendingCycleResult.faulted = false;
    m_pendingCycleResult.faultCode = LocalizationFaultCode::None;
    m_pendingCycleResult.detectedNumber = matchResult.totalPossiblePicking;
    m_pendingCycleResult.sentNumber = positions.size();
    m_pendingCycleResult.matchingTimeMs = matchResult.ExecutionTime;
    m_pendingCycleResult.lowArea = matchResult.isAreaLessThanLimits;
    m_pendingCycleResult.displayImage = matchResult.Image.clone();
    m_pendingCycleResult.matchResult = matchResult;
    m_pendingCycleResult.rows = rows;

    auto *visionRunner = visionOutputRunner();
    if (!visionRunner) {
        abortCycle(LocalizationFaultCode::VisionOutputLost,
                   QStringLiteral("Vision output runner is not available."));
        return;
    }

    disconnect(m_visionOutputResultConnection);
    m_visionOutputResultConnection = connect(
        visionRunner,
        &vc::runtime::VisionOutputRunner::resultRequestFinished,
        this,
        &LocalizationRuntimeController::onVisionOutputResultFinished,
        Qt::SingleShotConnection);
    visionRunner->requestSendResult(positions);
}

void LocalizationRuntimeController::onCameraCommandFinished(
    vc::runtime::DeviceCommandResult result)
{
    if (m_cycleState != CycleState::Running ||
        result.kind != vc::runtime::DeviceCommandKind::CameraSingleShot ||
        result.status != vc::runtime::DeviceCommandResultStatus::Failed) {
        return;
    }

    if (result.code == vc::runtime::DeviceCommandResultCode::TimedOut) {
        abortCycle(LocalizationFaultCode::CameraGrabTimeout, result.message);
    }
}

void LocalizationRuntimeController::onVisionOutputResultFinished(bool ok,
                                                                 const QString &message)
{
    disconnect(m_visionOutputResultConnection);
    m_visionOutputResultConnection = QMetaObject::Connection();

    if (m_cycleState != CycleState::Running) {
        return;
    }

    if (!ok) {
        abortCycle(LocalizationFaultCode::VisionOutputSendFailed, message);
        return;
    }

    publishCycleSuccessOutputs(m_pendingCycleResult);
    emit cycleResultUpdated(m_pendingCycleResult);
    appendTaskLog(QStringLiteral("INFO"),
                  QStringLiteral("Vision output sent. detected=%1 sent=%2 time=%3 ms.")
                      .arg(m_pendingCycleResult.detectedNumber)
                      .arg(m_pendingCycleResult.sentNumber)
                      .arg(m_pendingCycleResult.matchingTimeMs, 0, 'f', 1));

    m_cycleState = m_lastExecuteTrigger ? CycleState::WaitingTriggerReset
                                        : CycleState::ReadyForTrigger;
    if (!m_lastExecuteTrigger) {
        markRuntimeReady(QStringLiteral("Localization cycle completed."));
    } else {
        appendTaskLog(QStringLiteral("WARN"),
                      QStringLiteral("Waiting for execute trigger reset."));
    }
}

void LocalizationRuntimeController::onPrimaryPlcStatusChanged(vc::device::ConnectStatus status)
{
    handleRoleStatusChanged(RunnerRole::PrimaryPlc, status);
}

void LocalizationRuntimeController::onVisionOutputStatusChanged(vc::device::ConnectStatus status)
{
    handleRoleStatusChanged(RunnerRole::VisionOutput, status);
}

void LocalizationRuntimeController::onCameraStatusChanged(vc::device::ConnectStatus status)
{
    handleRoleStatusChanged(RunnerRole::Camera, status);
}

void LocalizationRuntimeController::onPrimaryPlcError(const QString &message)
{
    LOG_USER_WARN << "primary_plc runtime error:" << message;
}

void LocalizationRuntimeController::onVisionOutputError(const QString &message)
{
    LOG_USER_WARN << "vision_output runtime error:" << message;
}

void LocalizationRuntimeController::onCameraError(const QString &message)
{
    LOG_USER_WARN << "camera runtime error:" << message;
}

} // namespace vc::model
