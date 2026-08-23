#include <QtTest/QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QScopedPointer>
#include <QStandardPaths>

#include <memory>

#include "core/app_settings/app_settings.h"
#include "core/app_version.h"
#include "core/auth/access_control.h"
#include "core/auth/settings_admin_credential_provider.h"
#include "core/utils/shell_handoff.h"
#include "core/utils/single_instance_guard.h"
#include "device/camera/camera_basler_gige.h"
#include "device/device_capabilities.h"
#include "device/device_factory.h"
#include "device/device_registry.h"
#include "device/output_device/vision_tcpip_device.h"
#include "device/plc/mc_context_3e.h"
#include "device/plc/mc_protocol_device.h"
#include "device/robot/kawasaki_robot_device.h"
#include "device/robot/nachi_robot_device.h"
#include "device/virtual/virtual_device.h"
#include "device/virtual/virtual_camera_config.h"
#include "device/virtual/virtual_camera_device.h"
#include "device/virtual/virtual_plc_device.h"
#include "device/virtual/virtual_vision_output_device.h"
#include "model/localization_signal_mapper.h"
#include "model/localization_recovery_policy.h"
#include "model/localization_fault_code.h"
#include "model/task_factory.h"
#include "model/task_localization.h"
#include "model/task_state_machine.h"
#include "runtime/device_command.h"
#include "runtime/camera_runner.h"
#include "runtime/plc_runner.h"
#include "runtime/task_runner.h"
#include "runtime/vision_output_runner.h"
#include "ui/widgets/vision/vision_result_adapter.h"

using namespace vc::device;
using namespace vc::model;
using namespace vc::runtime;

namespace {

QJsonObject baseDeviceJson(const QString &id,
                           const QString &name,
                           DeviceType type,
                           const QJsonObject &config)
{
    return {
        { DEVICE_JSK_ID, id },
        { DEVICE_JSK_NAME, name },
        { DEVICE_JSK_TYPE, DeviceTypeToString(type) },
        { DEVICE_JSK_CONFIG, config }
    };
}

QJsonObject baslerDeviceJson(const QString &id = QStringLiteral("cam1"))
{
    BaslerGigeCfg cfg;
    cfg.m_modelName = QStringLiteral("ContractTestModel");
    cfg.m_userDefinedName = QStringLiteral("ContractTestCamera");
    cfg.m_serialNumber = QStringLiteral("SN-ARCH-001");
    cfg.m_ipAddress = QStringLiteral("192.168.10.10");
    cfg.m_autoExposureMode = vc::device::basler::BaslerExposureMode::Exposure_Off;

    QJsonObject obj = baseDeviceJson(id,
                                     QStringLiteral("Basler Camera"),
                                     DeviceType::Camera,
                                     cfg.toJson());
    obj[DEVICE_JSK_CAM_TYPE] = CameraTypeToString(CameraType::BaslerGigE);
    return obj;
}

QJsonObject plcDeviceJson(const QString &id = QStringLiteral("plc1"))
{
    McProtocolConfig cfg;
    cfg.configMcProtocol(vc::device::mc::McFrameType::Frame_3E,
                         vc::device::mc::McDataCode::Binary);
    return baseDeviceJson(id,
                          QStringLiteral("Mitsubishi MC PLC"),
                          DeviceType::PLC,
                          cfg.toJson());
}

QJsonObject visionOutputDeviceJson(const QString &id = QStringLiteral("vout1"))
{
    VisionTcpipDeviceCfg cfg;
    cfg.m_listenAddress = QStringLiteral("127.0.0.1");
    cfg.m_mainPort = 25001;
    cfg.m_heartbeatPort = 25002;
    cfg.m_heartbeatIntervalMs = 200;
    cfg.m_heartbeatTimeoutMs = 1000;
    return baseDeviceJson(id,
                          QStringLiteral("Vision TCP/IP Output"),
                          DeviceType::VisionOutput,
                          cfg.toJson());
}

QJsonObject kawasakiRobotDeviceJson(const QString &id = QStringLiteral("robot1"))
{
    KawasakiRobotCfg cfg;
    return baseDeviceJson(id,
                          QStringLiteral("Kawasaki Robot"),
                          DeviceType::Robot,
                          cfg.toJson());
}

QJsonObject nachiRobotDeviceJson(const QString &id = QStringLiteral("robot2"))
{
    NachiRobotCfg cfg;
    return baseDeviceJson(id,
                          QStringLiteral("Nachi Robot"),
                          DeviceType::Robot,
                          cfg.toJson());
}

QJsonObject unsupportedCameraJson(CameraType type)
{
    QJsonObject cfg;
    cfg[DEVICE_JSK_CAM_TYPE] = CameraTypeToString(type);
    QJsonObject obj = baseDeviceJson(QStringLiteral("unsupported_cam"),
                                     QStringLiteral("Unsupported Camera"),
                                     DeviceType::Camera,
                                     cfg);
    obj[DEVICE_JSK_CAM_TYPE] = CameraTypeToString(type);
    return obj;
}

QJsonObject unsupportedVisionSerialJson()
{
    QJsonObject cfg;
    cfg[DEVICE_JSK_VOUT_TYPE] = VisionOutputTypeToString(VisionOutputType::VisionSerial);
    return baseDeviceJson(QStringLiteral("unsupported_vout"),
                          QStringLiteral("Unsupported Vision Serial"),
                          DeviceType::VisionOutput,
                          cfg);
}

QJsonObject unsupportedHuayanRobotJson()
{
    QJsonObject cfg;
    cfg[DEVICE_JSK_ROBOT_TYPE] = RobotTypeToString(RobotType::Huayan);
    return baseDeviceJson(QStringLiteral("unsupported_robot"),
                          QStringLiteral("Unsupported Huayan Robot"),
                          DeviceType::Robot,
                          cfg);
}

QJsonObject localizationTaskJson()
{
    TaskLocalizeConfig cfg;
    QJsonObject taskObj;
    taskObj["id"] = QStringLiteral("task-localization-1");
    taskObj["name"] = QStringLiteral("Localization Task");
    taskObj["taskType"] = taskTypeToString(TaskType::LocalizationTask);
    taskObj["cameraSourceType"] = qenumToString(CameraSourceType::Source_Owned);
    taskObj["ownedCameraId"] = QString();
    taskObj["assignedDeviceIds"] = QJsonArray();
    taskObj["taskConfig"] = cfg.toJson();
    taskObj["patternManager"] = QJsonObject{{"groups", QJsonArray()}};
    return taskObj;
}

DeviceCommandResult findResultByCommandId(const QSignalSpy &spy, const QString &commandId)
{
    for (const QList<QVariant> &signalArgs : spy) {
        const DeviceCommandResult result =
            qvariant_cast<DeviceCommandResult>(signalArgs.at(0));
        if (result.commandId == commandId) {
            return result;
        }
    }
    return DeviceCommandResult();
}


struct LocalizationRuntimeFixture {
    QScopedPointer<VirtualPlcDevice> plc;
    QScopedPointer<VirtualCameraDevice> camera1;
    QScopedPointer<VirtualCameraDevice> camera2;
    QScopedPointer<VirtualVisionOutputDevice> visionOutput;
    QScopedPointer<PlcRunner> plcRunner;
    QScopedPointer<CameraRunner> cameraRunner1;
    QScopedPointer<CameraRunner> cameraRunner2;
    QScopedPointer<VisionOutputRunner> visionOutputRunner;
    LocalizationRuntimeController controller;

    LocalizationRuntimeFixture()
    {
        plc.reset(new VirtualPlcDevice(QStringLiteral("plc1"), QStringLiteral("PLC")));
        camera1.reset(new VirtualCameraDevice(QStringLiteral("cam1"), QStringLiteral("Camera 1")));
        camera2.reset(new VirtualCameraDevice(QStringLiteral("cam2"), QStringLiteral("Camera 2")));
        visionOutput.reset(new VirtualVisionOutputDevice(QStringLiteral("vout1"),
                                                      QStringLiteral("Vision Output")));

        const calib::Calibrator calibrator = makeCalibrator();
        camera1->setCalibrator(calibrator);
        camera2->setCalibrator(calibrator);

        plcRunner.reset(new PlcRunner(plc.data()));
        cameraRunner1.reset(new CameraRunner(camera1.data()));
        cameraRunner2.reset(new CameraRunner(camera2.data()));
        visionOutputRunner.reset(new VisionOutputRunner(visionOutput.data()));

        startRunner(plcRunner.data());
        startRunner(cameraRunner1.data());
        startRunner(cameraRunner2.data());
        startRunner(visionOutputRunner.data());
    }

    ~LocalizationRuntimeFixture()
    {
        stopRunner(visionOutputRunner.data());
        stopRunner(cameraRunner2.data());
        stopRunner(cameraRunner1.data());
        stopRunner(plcRunner.data());
    }

    LocalizationRuntimeController::RuntimeContext context(bool calibrated = true) const
    {
        LocalizationRuntimeController::RuntimeContext ctx;
        ctx.config = config();
        ctx.primaryPlcDeviceId = plc->id();
        ctx.visionOutputDeviceId = visionOutput->id();
        ctx.primaryPlcRunner = plcRunner.data();
        ctx.visionOutputRunner = visionOutputRunner.data();
        ctx.cameraDeviceIds.insert(1, camera1->id());
        ctx.cameraDeviceIds.insert(2, camera2->id());
        ctx.cameraRunners.insert(1, cameraRunner1.data());
        ctx.cameraRunners.insert(2, cameraRunner2.data());
        if (calibrated) {
            ctx.cameraCalibrators.insert(1, makeCalibrator());
            ctx.cameraCalibrators.insert(2, makeCalibrator());
        }
        ctx.patternGroups.insert(1, makePatternGroup());
        ctx.activeCameraNumber = 1;
        ctx.activePatternGroupNumber = 1;
        return ctx;
    }

    static TaskLocalizeConfig config()
    {
        TaskLocalizeConfig cfg;
        cfg.setbExecuteTrigger(QStringLiteral("M10"));
        cfg.setbTaskReady(QStringLiteral("M11"));
        cfg.setbMatchingBusy(QStringLiteral("M12"));
        cfg.setbMatchingFinished(QStringLiteral("M13"));
        cfg.setbMatchingDetected(QStringLiteral("M14"));
        cfg.setbMatchingLowArea(QStringLiteral("M15"));
        cfg.setbCameraValid(QStringLiteral("M16"));
        cfg.setbPatternValid(QStringLiteral("M17"));
        cfg.setbTaskFault(QStringLiteral("M18"));
        cfg.setbErrorReset(QStringLiteral("M19"));
        cfg.setnActiveCamera(QStringLiteral("D100"));
        cfg.setnActivePatternGroup(QStringLiteral("D101"));
        cfg.setnDetectedNumber(QStringLiteral("D102"));
        cfg.setnFaultCode(QStringLiteral("D103"));
        return cfg;
    }

    static calib::Calibrator makeCalibrator()
    {
        calib::Calibrator calibrator;
        calibrator.addCorrespondences(
            {cv::Point2f(0.0f, 0.0f),
             cv::Point2f(100.0f, 0.0f),
             cv::Point2f(0.0f, 100.0f),
             cv::Point2f(100.0f, 100.0f)},
            {cv::Point3f(0.0f, 0.0f, 0.0f),
             cv::Point3f(100.0f, 0.0f, 0.0f),
             cv::Point3f(0.0f, 100.0f, 0.0f),
             cv::Point3f(100.0f, 100.0f, 0.0f)});
        calibrator.calibrate();
        return calibrator;
    }

    static std::shared_ptr<mtc::MatchGroup> makePatternGroup()
    {
        auto group = std::make_shared<mtc::MatchGroup>(L"Group 1", 1);
        mtc::MatchPatternConfig pattern;
        pattern.m_patternName = L"Pattern 1";
        pattern.m_patternIndex = 1;
        pattern.m_rawImage = cv::Mat(8, 8, CV_8UC1, cv::Scalar(255));
        group->addPattern(pattern);
        return group;
    }

    static mtc::MatchResult makeMatchResult()
    {
        mtc::MatchResult result;
        mtc::MatchedObject object;
        object.pattern_name = L"Pattern 1";
        object.pattern_index = 1;
        object.matched_Score = 0.95;
        object.point_LT = cv::Point2f(10.0f, 20.0f);
        object.point_RT = cv::Point2f(30.0f, 20.0f);
        object.point_RB = cv::Point2f(30.0f, 40.0f);
        object.point_LB = cv::Point2f(10.0f, 40.0f);
        object.point_Center = cv::Point2f(20.0f, 30.0f);
        object.matched_Angle = 5.0;
        object.point_angle = 10.0;
        object.setPossibleToPick(true);
        result.Objects.push_back(object);
        result.totalPossiblePicking = 1;
        result.ExecutionTime = 4.2;
        result.cropOffsetPoint = cv::Point2f(7.0f, 9.0f);
        result.imageCols = 64;
        result.imageRows = 48;
        result.Image = cv::Mat(32, 32, CV_8UC3, cv::Scalar(0, 255, 0));
        return result;
    }

private:
    static void startRunner(IDeviceRunner *runner)
    {
        runner->start();
        runner->attach();
    }

    static void stopRunner(IDeviceRunner *runner)
    {
        if (!runner) {
            return;
        }
        if (runner->isAttached()) {
            runner->detach(nullptr);
        }
        if (runner->isRunning()) {
            runner->stop();
        }
    }
};

QVariant lastSignalValue(const QSignalSpy &spy, const QString &signalName)
{
    for (int i = spy.size() - 1; i >= 0; --i) {
        const QList<QVariant> args = spy.at(i);
        if (args.size() >= 2 && args.at(0).toString() == signalName) {
            return args.at(1);
        }
    }
    return {};
}

} // namespace

class TaskLocalizationProbe : public TaskLocalization {
public:
    using TaskLocalization::TaskLocalization;
    using ITask::transitionTaskState;
};

class ArchitectureContractTest : public QObject {
    Q_OBJECT

private slots:
    /// Redirects every QStandardPaths location into Qt's per-test sandbox **before** any
    /// test can touch AppSettings.
    ///
    /// AppSettings is a lazily-constructed singleton that writes on the first setValue(),
    /// and the access-control tests below seed an admin credential. Without this, running
    /// the suite would edit the developer's real settings.dat — a test that changes the
    /// machine it runs on is a test nobody can run twice with confidence.
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void test_device_factory_supported_subtypes()
    {
        std::unique_ptr<IDevice> camera(DeviceFactory::fromJson(baslerDeviceJson()));
        QVERIFY(camera != nullptr);
        QCOMPARE(camera->deviceType(), DeviceType::Camera);
        QVERIFY(qobject_cast<BaslerGigECamera *>(camera.get()) != nullptr);

        std::unique_ptr<IDevice> plc(DeviceFactory::fromJson(plcDeviceJson()));
        QVERIFY(plc != nullptr);
        QCOMPARE(plc->deviceType(), DeviceType::PLC);
        QVERIFY(qobject_cast<McProtocolDevice *>(plc.get()) != nullptr);

        std::unique_ptr<IDevice> output(DeviceFactory::fromJson(visionOutputDeviceJson()));
        QVERIFY(output != nullptr);
        QCOMPARE(output->deviceType(), DeviceType::VisionOutput);
        QVERIFY(qobject_cast<VisionTcpipDevice *>(output.get()) != nullptr);

        std::unique_ptr<IDevice> kawasaki(DeviceFactory::fromJson(kawasakiRobotDeviceJson()));
        QVERIFY(kawasaki != nullptr);
        QCOMPARE(kawasaki->deviceType(), DeviceType::Robot);
        QVERIFY(qobject_cast<KawasakiRobotDevice *>(kawasaki.get()) != nullptr);

        std::unique_ptr<IDevice> nachi(DeviceFactory::fromJson(nachiRobotDeviceJson()));
        QVERIFY(nachi != nullptr);
        QCOMPARE(nachi->deviceType(), DeviceType::Robot);
        QVERIFY(qobject_cast<NachiRobotDevice *>(nachi.get()) != nullptr);
    }

    void test_device_factory_unsupported_declared_subtypes_return_null()
    {
        std::unique_ptr<IDevice> realsense(
            DeviceFactory::fromJson(unsupportedCameraJson(CameraType::Realsense)));
        QVERIFY(realsense == nullptr);

        std::unique_ptr<IDevice> baslerUsb(
            DeviceFactory::fromJson(unsupportedCameraJson(CameraType::BaslerUSB)));
        QVERIFY(baslerUsb == nullptr);

        std::unique_ptr<IDevice> serialOutput(
            DeviceFactory::fromJson(unsupportedVisionSerialJson()));
        QVERIFY(serialOutput == nullptr);

        std::unique_ptr<IDevice> huayan(
            DeviceFactory::fromJson(unsupportedHuayanRobotJson()));
        QVERIFY(huayan == nullptr);
    }

    void test_concrete_device_configs_round_trip_json()
    {
        BaslerGigeCfg baslerCfg;
        baslerCfg.m_modelName = QStringLiteral("Model");
        baslerCfg.m_serialNumber = QStringLiteral("Serial");
        baslerCfg.m_ipAddress = QStringLiteral("10.0.0.2");
        baslerCfg.m_autoExposureMode = vc::device::basler::BaslerExposureMode::Exposure_Off;
        BaslerGigeCfg baslerRestored;
        QVERIFY(baslerRestored.fromJson(baslerCfg.toJson()));
        QCOMPARE(baslerRestored.cameraType(), CameraType::BaslerGigE);
        QCOMPARE(baslerRestored.m_ipAddress, baslerCfg.m_ipAddress);

        McProtocolConfig plcCfg;
        QVERIFY(plcCfg.configMcProtocol(vc::device::mc::McFrameType::Frame_3E,
                                        vc::device::mc::McDataCode::Binary));
        McProtocolConfig plcRestored;
        QVERIFY(plcRestored.fromJson(plcCfg.toJson()));
        QCOMPARE(plcRestored.plcType(), PlcType::MitsubishiMc);
        QCOMPARE(plcRestored.currentFrameType(), vc::device::mc::McFrameType::Frame_3E);

        VisionTcpipDeviceCfg outputCfg;
        outputCfg.m_listenAddress = QStringLiteral("127.0.0.1");
        outputCfg.m_mainPort = 26001;
        outputCfg.m_heartbeatPort = 26002;
        VisionTcpipDeviceCfg outputRestored;
        QVERIFY(outputRestored.fromJson(outputCfg.toJson()));
        QCOMPARE(outputRestored.visionOutputType(), VisionOutputType::VisionTCPIP);
        QCOMPARE(outputRestored.m_mainPort, outputCfg.m_mainPort);

        KawasakiRobotCfg kawasakiCfg;
        KawasakiRobotCfg kawasakiRestored;
        QVERIFY(kawasakiRestored.fromJson(kawasakiCfg.toJson()));
        QCOMPARE(kawasakiRestored.robotType(), RobotType::Kawasaki);

        NachiRobotCfg nachiCfg;
        NachiRobotCfg nachiRestored;
        QVERIFY(nachiRestored.fromJson(nachiCfg.toJson()));
        QCOMPARE(nachiRestored.robotType(), RobotType::Nachi);
    }

    void test_task_localize_config_round_trips_device_bindings()
    {
        TaskLocalizeConfig cfg;
        cfg.setnActivePatternGroup(QStringLiteral("D101"));
        cfg.setnFaultCode(QStringLiteral("D102"));
        cfg.setbTaskFault(QStringLiteral("M105"));
        cfg.d->m_deviceBindings.setPrimaryPlcDeviceId(QStringLiteral("plc1"));
        cfg.d->m_deviceBindings.setVisionOutputDeviceId(QStringLiteral("vout1"));
        cfg.d->m_deviceBindings.setCameraNumberMap({
            {1, QStringLiteral("cam1")},
            {2, QStringLiteral("cam2")}
        });

        TaskLocalizeConfig restored;
        QVERIFY(restored.fromJson(cfg.toJson()));
        QCOMPARE(restored.nActivePatternGroup(), QStringLiteral("D101"));
        QCOMPARE(restored.nFaultCode(), QStringLiteral("D102"));
        QCOMPARE(restored.bTaskFault(), QStringLiteral("M105"));
        QCOMPARE(restored.d->m_deviceBindings.primaryPlcDeviceId(),
                 QStringLiteral("plc1"));
        QCOMPARE(restored.d->m_deviceBindings.visionOutputDeviceId(),
                 QStringLiteral("vout1"));
        QCOMPARE(restored.d->m_deviceBindings.cameraDeviceId(1),
                 QStringLiteral("cam1"));
        QCOMPARE(restored.d->m_deviceBindings.cameraDeviceId(2),
                 QStringLiteral("cam2"));
    }

    // Phase 6 / A1: the fault-acknowledge input is a normal signal binding, and the
    // schema bump that carries it must not lock out projects written before it.
    void test_error_reset_signal_round_trips_and_v1_documents_still_load()
    {
        QCOMPARE(TaskLocalizeConfig::kSchemaVersion, 2);

        TaskLocalizeConfig cfg;
        cfg.setbErrorReset(QStringLiteral("M120"));

        TaskLocalizeConfig restored;
        QVERIFY(restored.fromJson(cfg.toJson()));
        QCOMPARE(restored.bErrorReset(), QStringLiteral("M120"));

        // A v1 document has no "bErrorReset" key at all. It must load, leaving the
        // acknowledge input unbound — which is a supported configuration, because a
        // latched fault also clears itself after kFaultAutoRecoverMs.
        QJsonObject v1 = cfg.toJson();
        v1["version"] = 1;
        v1.remove(QStringLiteral("bErrorReset"));

        TaskLocalizeConfig legacy;
        QVERIFY(legacy.fromJson(v1));
        QVERIFY(legacy.bErrorReset().isEmpty());
        QCOMPARE(legacy.bTaskFault(), cfg.bTaskFault());

        // The gate still refuses a document from a newer build.
        QJsonObject v3 = cfg.toJson();
        v3["version"] = TaskLocalizeConfig::kSchemaVersion + 1;
        TaskLocalizeConfig tooNew;
        QVERIFY(!tooNew.fromJson(v3));
    }

    void test_camera_workspace_config_round_trips()
    {
        TaskLocalizeConfig cfg;
        CameraWorkspace ws;
        ws.useWorkspace = true;
        ws.roi = cv::Rect2f(10.0f, 20.0f, 100.0f, 80.0f);
        cfg.setCameraWorkspace(QStringLiteral("cam-id-1"), ws);

        TaskLocalizeConfig restored;
        QVERIFY(restored.fromJson(cfg.toJson()));

        const CameraWorkspace r = restored.cameraWorkspace(QStringLiteral("cam-id-1"));
        QCOMPARE(r.useWorkspace, true);
        QCOMPARE(r.roi.x, 10.0f);
        QCOMPARE(r.roi.y, 20.0f);
        QCOMPARE(r.roi.width, 100.0f);
        QCOMPARE(r.roi.height, 80.0f);

        // Unknown camera defaults to workspace-off (backward compatible).
        QCOMPARE(restored.cameraWorkspace(QStringLiteral("unknown")).useWorkspace, false);

        // Reference-image BLOB key round-trips and is distinct from pattern keys.
        QCOMPARE(CameraWorkspaceMap::imageKey(QStringLiteral("cam-id-1")),
                 QStringLiteral("ws_cam-id-1"));
        QString parsedId;
        QVERIFY(CameraWorkspaceMap::parseImageKey(QStringLiteral("ws_cam-id-1"), parsedId));
        QCOMPARE(parsedId, QStringLiteral("cam-id-1"));
        QVERIFY(!CameraWorkspaceMap::parseImageKey(QStringLiteral("g1_p2"), parsedId));
    }

    void test_runtime_matching_payload_metatypes_support_queued_connection()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeController controller;
        QObject receiver;
        bool delivered = false;
        int deliveredCycleId = 0;
        CameraWorkspace deliveredWorkspace;
        cv::Mat deliveredImage;

        QObject::connect(&controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &receiver,
                         [&](int cycleId,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace workspace,
                             cv::Mat image,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            delivered = true;
            deliveredCycleId = cycleId;
            deliveredWorkspace = workspace;
            deliveredImage = image;
        }, Qt::QueuedConnection);

        CameraWorkspace workspace;
        workspace.useWorkspace = true;
        workspace.roi = cv::Rect2f(3.0f, 4.0f, 12.0f, 16.0f);

        emit controller.runtimeMatchingRequested(
            42,
            std::shared_ptr<mtc::MatchGroup>(),
            workspace,
            cv::Mat(2, 3, CV_8UC1, cv::Scalar(7)),
            std::shared_ptr<mtc::IRobotPickingChecker>());

        QTRY_VERIFY_WITH_TIMEOUT(delivered, 1000);
        QCOMPARE(deliveredCycleId, 42);
        QCOMPARE(deliveredWorkspace.useWorkspace, true);
        QCOMPARE(deliveredWorkspace.roi.x, 3.0f);
        QCOMPARE(deliveredWorkspace.roi.y, 4.0f);
        QCOMPARE(deliveredWorkspace.roi.width, 12.0f);
        QCOMPARE(deliveredWorkspace.roi.height, 16.0f);
        QCOMPARE(deliveredImage.rows, 2);
        QCOMPARE(deliveredImage.cols, 3);
    }

    void test_localization_signal_mapper_translates_configured_tags()
    {
        TaskLocalizeConfig cfg;
        cfg.setnActiveCamera(QStringLiteral("D100"));
        cfg.setnActivePatternGroup(QStringLiteral("D101"));
        cfg.setnFaultCode(QStringLiteral("D102"));
        cfg.setbExecuteTrigger(QStringLiteral("M10"));
        cfg.setbTaskFault(QStringLiteral("M11"));

        LocalizationSignalMapper mapper;
        mapper.configure(cfg);

        const QList<LocalizationSignalEvent> events = mapper.mapValues({
            {QStringLiteral("D100"), 2},
            {QStringLiteral("D101"), 4},
            {QStringLiteral("D102"), 102},
            {QStringLiteral("M10"), true},
            {QStringLiteral("M11"), true},
            {QStringLiteral("M999"), false}
        });

        QCOMPARE(events.size(), 5);
        QCOMPARE(events.at(0).name, QStringLiteral("nActiveCamera"));
        QCOMPARE(events.at(0).value.toInt(), 2);
        QCOMPARE(events.at(1).name, QStringLiteral("nActivePatternGroup"));
        QCOMPARE(events.at(1).value.toInt(), 4);
        QCOMPARE(events.at(2).name, QStringLiteral("nFaultCode"));
        QCOMPARE(events.at(2).value.toInt(), 102);
        QCOMPARE(events.at(3).name, QStringLiteral("bExecuteTrigger"));
        QCOMPARE(events.at(3).value.toBool(), true);
        QCOMPARE(events.at(4).name, QStringLiteral("bTaskFault"));
        QCOMPARE(events.at(4).value.toBool(), true);
        QCOMPARE(mapper.tagForSignalName(QStringLiteral("nFaultCode")),
                 QStringLiteral("D102"));
    }

    void test_localization_fault_code_values_are_stable()
    {
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::None), 0);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::CameraLost), 100);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::CameraConnectFailed), 101);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::CameraGrabTimeout), 102);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::VisionOutputLost), 200);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::VisionOutputSendFailed), 201);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::PlcLost), 300);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::PatternInvalid), 400);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::CalibrationInvalid), 401);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::InternalError), 500);
    }

    void test_task_factory_restores_localization_task()
    {
        std::unique_ptr<ITask> task(TaskFactory::fromJson(localizationTaskJson()));
        QVERIFY(task != nullptr);
        QCOMPARE(task->taskType(), TaskType::LocalizationTask);
        QCOMPARE(task->id(), QStringLiteral("task-localization-1"));
        QCOMPARE(task->name(), QStringLiteral("Localization Task"));
        QVERIFY(qobject_cast<TaskLocalization *>(task.get()) != nullptr);

        task->stopAll();
    }

    void test_task_runner_creates_supported_family_runners()
    {
        TaskRunner runner;

        auto camera = std::make_shared<BaslerGigECamera>(
            QStringLiteral("cam_runner"), QStringLiteral("Camera Runner Device"));
        runner.registerDevice(camera->id(), camera);
        QVERIFY(runner.hasRunner(camera->id()));
        QVERIFY(qobject_cast<CameraRunner *>(runner.runnerFor(camera->id())) != nullptr);

        auto plc = std::make_shared<McProtocolDevice>(
            QStringLiteral("plc_runner"), QStringLiteral("PLC Runner Device"));
        runner.registerDevice(plc->id(), plc);
        QVERIFY(runner.hasRunner(plc->id()));
        QVERIFY(qobject_cast<PlcRunner *>(runner.runnerFor(plc->id())) != nullptr);

        auto output = std::make_shared<VisionTcpipDevice>(
            QStringLiteral("vout_runner"), QStringLiteral("Output Runner Device"));
        runner.registerDevice(output->id(), output);
        QVERIFY(runner.hasRunner(output->id()));
        QVERIFY(qobject_cast<VisionOutputRunner *>(runner.runnerFor(output->id())) != nullptr);

        auto robot = std::make_shared<KawasakiRobotDevice>(
            QStringLiteral("robot_runner"), QStringLiteral("Robot Runner Device"));
        runner.registerDevice(robot->id(), robot);
        QVERIFY(!runner.hasRunner(robot->id()));
        QVERIFY(runner.runnerFor(robot->id()) == nullptr);

        runner.enterIdle();
    }

    void test_task_runner_phase_toggle_disconnects_and_reuses_runner()
    {
        TaskRunner runner;
        auto plc = std::make_shared<VirtualPlcDevice>(
            QStringLiteral("plc_toggle"), QStringLiteral("PLC Toggle"));
        runner.registerDevice(plc->id(), plc);

        // Commission: the runner is started + attached; connect through it.
        runner.enterCommission();
        auto *plcRunner = qobject_cast<PlcRunner *>(runner.runnerFor(plc->id()));
        QVERIFY(plcRunner != nullptr);
        plcRunner->requestConnect();
        QTRY_VERIFY_WITH_TIMEOUT(plc->isDeviceConnected(), 1000);

        // Leaving the phase must close the connection on the worker thread,
        // not leave it open while the device is moved/stopped.
        runner.enterIdle();
        QVERIFY(!plc->isDeviceConnected());

        // Re-entering a phase reuses the SAME runner instance (it is cached by
        // device id, never recreated) and the device reconnects cleanly.
        runner.enterCommission();
        auto *plcRunnerAgain = qobject_cast<PlcRunner *>(runner.runnerFor(plc->id()));
        QCOMPARE(plcRunnerAgain, plcRunner);
        plcRunnerAgain->requestConnect();
        QTRY_VERIFY_WITH_TIMEOUT(plc->isDeviceConnected(), 1000);

        runner.enterIdle();
        QVERIFY(!plc->isDeviceConnected());
    }

    void test_mc_device_disconnect_without_connect_is_safe()
    {
        // A never-connected MC device must tear down without dereferencing a
        // null transport or an uninitialized polling timer.
        auto plc = std::make_shared<McProtocolDevice>(
            QStringLiteral("plc_safe"), QStringLiteral("PLC Safe"));
        QVERIFY(!plc->isDeviceConnected());
        QVERIFY(plc->deviceDisconnect());
        QVERIFY(!plc->isDeviceConnected());
    }

    void test_device_capability_interfaces_match_supported_families()
    {
        auto camera = std::make_shared<BaslerGigECamera>(
            QStringLiteral("cap_cam"), QStringLiteral("Capability Camera"));
        auto plc = std::make_shared<McProtocolDevice>(
            QStringLiteral("cap_plc"), QStringLiteral("Capability PLC"));
        auto output = std::make_shared<VisionTcpipDevice>(
            QStringLiteral("cap_vout"), QStringLiteral("Capability Vision Output"));
        auto robot = std::make_shared<KawasakiRobotDevice>(
            QStringLiteral("cap_robot"), QStringLiteral("Capability Robot"));

        QVERIFY(dynamic_cast<IImageSourceDevice *>(camera.get()) != nullptr);
        QVERIFY(dynamic_cast<IPlcTagProvider *>(plc.get()) != nullptr);
        QVERIFY(dynamic_cast<IDigitalIoProvider *>(plc.get()) != nullptr);
        QVERIFY(dynamic_cast<IWordIoProvider *>(plc.get()) != nullptr);
        QVERIFY(dynamic_cast<IPlcIoWriter *>(plc.get()) != nullptr);
        QVERIFY(dynamic_cast<IResultOutputDevice *>(output.get()) != nullptr);

        QVERIFY(dynamic_cast<IPlcTagProvider *>(camera.get()) == nullptr);
        QVERIFY(dynamic_cast<IPlcTagProvider *>(output.get()) == nullptr);
        QVERIFY(dynamic_cast<IPlcTagProvider *>(robot.get()) == nullptr);
    }

    void test_device_registry_lists_supported_subtypes()
    {
        // Order is asserted, not just membership. displayNamesFor() preserves the registry
        // table's order, the Add Device wizard's combo leaves index 0 current, and the wizard
        // writes back whatever is current — so the FIRST entry of a family is what an operator
        // creates by not choosing. A virtual sub-type that drifts to the front would make the
        // default camera a simulated one, silently.
        QCOMPARE(DeviceRegistry::displayNamesFor(DeviceType::Camera),
                 (QStringList{CameraTypeToString(CameraType::BaslerGigE),
                              CameraTypeToString(CameraType::VirtualCamera)}));
        QCOMPARE(DeviceRegistry::displayNamesFor(DeviceType::PLC),
                 (QStringList{PlcTypeToString(PlcType::MitsubishiMc),
                              PlcTypeToString(PlcType::VirtualPlc)}));
        QCOMPARE(DeviceRegistry::displayNamesFor(DeviceType::VisionOutput),
                 (QStringList{VisionOutputTypeToString(VisionOutputType::VisionTCPIP),
                              VisionOutputTypeToString(VisionOutputType::VisionTcpipClient),
                              VisionOutputTypeToString(VisionOutputType::VirtualVisionOutput)}));

        const DeviceRegistryEntry *cameraEntry =
            DeviceRegistry::find(DeviceType::Camera,
                                 CameraTypeToString(CameraType::BaslerGigE));
        QVERIFY(cameraEntry != nullptr);
        QCOMPARE(cameraEntry->configJsonKey, QStringLiteral(DEVICE_JSK_CAM_TYPE));

        const DeviceRegistryEntry *virtualCameraEntry =
            DeviceRegistry::find(DeviceType::Camera,
                                 CameraTypeToString(CameraType::VirtualCamera));
        QVERIFY2(virtualCameraEntry != nullptr,
                 "the virtual camera is not registered, so it cannot be created from a "
                 "project file no matter what the class does");
        QCOMPARE(virtualCameraEntry->configJsonKey, QStringLiteral(DEVICE_JSK_CAM_TYPE));
    }

    // Phase 7 / D2: a virtual camera must survive a save/load round trip as a VIRTUAL camera.
    //
    // The failure this guards against is risk R8 — a virtual device passing for a real one in
    // a commissioned project. It nearly happened: while the virtual camera reused BaslerGigeCfg
    // its nested config wrote "Basler_GigE" even though the top-level key said "Virtual", and
    // DeviceRegistry reads the top level FIRST. Both keys agreeing is what makes the sub-type
    // survive a hand-edited or re-serialised project file.
    void test_virtual_camera_round_trips_as_virtual_not_as_hardware()
    {
        VirtualCameraDevice device(QStringLiteral("vcam1"), QStringLiteral("Virtual Camera"));
        QCOMPARE(device.cameraType(), CameraType::VirtualCamera);

        const QJsonObject json = device.toJson();
        const QJsonObject config = json.value(QStringLiteral(DEVICE_JSK_CONFIG)).toObject();
        QCOMPARE(config.value(QStringLiteral(DEVICE_JSK_CAM_TYPE)).toString(),
                 QStringLiteral(CAM_TYPE_VIRTUAL));

        // The registry must resolve the saved object back to the virtual entry, and the
        // factory must actually build a VirtualCameraDevice from it.
        const DeviceRegistryEntry *entry = DeviceRegistry::find(json, DeviceType::Camera);
        QVERIFY(entry != nullptr);
        QCOMPARE(entry->subTypeValue, QStringLiteral(CAM_TYPE_VIRTUAL));

        QScopedPointer<IDevice> rebuilt(entry->creator(json, nullptr));
        QVERIFY(rebuilt != nullptr);
        auto *rebuiltCamera = dynamic_cast<CameraDevice *>(rebuilt.data());
        QVERIFY(rebuiltCamera != nullptr);
        QCOMPARE(rebuiltCamera->cameraType(), CameraType::VirtualCamera);
        QCOMPARE(rebuiltCamera->id(), QStringLiteral("vcam1"));
    }

    // Phase 7 / D2a: a reloaded virtual camera must GRAB the configured image, not just
    // remember its path.
    //
    // The earlier round-trip test asserted the sub-type survived a save/load and passed while
    // this was broken: IDevice::fromJson() writes into the config through its stored pointer
    // and never calls setDeviceConfig(), so the device got no hook to rebuild its frame. The
    // property browser showed the right path and every grab returned the default flat grey
    // frame. Asserting the CONFIG round-trips is not the same as asserting the DEVICE behaves,
    // and only the second one is what an operator sees.
    void test_reloaded_virtual_camera_grabs_its_configured_image()
    {
        const QString imagePath =
            QDir(QDir::tempPath()).absoluteFilePath(QStringLiteral("ncr_virtual_cam_test.png"));
        // Deliberately not 32x32 (the generated fallback size) so the two are distinguishable.
        QVERIFY(cv::imwrite(imagePath.toStdString(),
                            cv::Mat(64, 48, CV_8UC1, cv::Scalar(200))));

        QJsonObject saved;
        {
            VirtualCameraDevice device(QStringLiteral("vcam_img"), QStringLiteral("VCam"));
            VirtualCameraCfg cfg;
            cfg.setimagePath(imagePath);
            device.setDeviceConfig(cfg.clone());
            saved = device.toJson();
        }

        QCOMPARE(saved.value(QStringLiteral(DEVICE_JSK_CONFIG))
                     .toObject().value(QStringLiteral("ImagePath")).toString(),
                 imagePath);

        VirtualCameraDevice reloaded(QStringLiteral("vcam_img"), QStringLiteral("VCam"));
        QVERIFY(reloaded.fromJson(saved));

        const GrabResult result = reloaded.grabSingleShot();
        QVERIFY(result.isGrabSuccess);
        QVERIFY2(!result.frame.empty(), "reloaded virtual camera grabbed an empty frame");
        QCOMPARE(result.frame.rows, 64);
        QCOMPARE(result.frame.cols, 48);

        QFile::remove(imagePath);
    }

    // Phase 7 / D2a: a virtual camera must arrive calibrated, or the runtime refuses to run it.
    //
    // LocalizationRuntimeController::validateActiveCameraCalibration() faults a task whose
    // active camera reports isCalibrated() == false, and the real calibration workflow lives
    // inside the Basler widget — so a virtual camera without this could be created, opened and
    // configured, and never run. Also asserts the declared scale is honoured, because a
    // calibration that is merely *valid* would satisfy the runtime while reporting positions
    // nobody chose.
    void test_virtual_camera_is_calibrated_from_its_declared_scale()
    {
        VirtualCameraDevice device(QStringLiteral("vcam_cal"), QStringLiteral("VCam"));

        VirtualCameraCfg cfg;
        cfg.setframeWidth(100);
        cfg.setframeHeight(100);
        cfg.setmillimetresPerPixel(2.0);
        cfg.setoriginXMm(10.0);
        cfg.setoriginYMm(-5.0);
        device.setDeviceConfig(cfg.clone());

        QScopedPointer<IDeviceCfg> readBack(device.deviceConfig());
        auto *cameraCfg = dynamic_cast<CameraCfg *>(readBack.data());
        QVERIFY(cameraCfg != nullptr);

        const calib::Calibrator calibrator = cameraCfg->calibrator();
        QVERIFY2(calibrator.isCalibrated(),
                 "an uncalibrated virtual camera is refused by the runtime, so it could never "
                 "run a cycle");

        // pixel (0,0) is the declared origin; (10,20) is that plus the declared scale.
        const cv::Point3f origin = calibrator.imageToRobot(cv::Point2f(0.0f, 0.0f));
        QVERIFY(std::abs(origin.x - 10.0f) < 0.01f);
        QVERIFY(std::abs(origin.y + 5.0f) < 0.01f);

        const cv::Point3f offset = calibrator.imageToRobot(cv::Point2f(10.0f, 20.0f));
        QVERIFY(std::abs(offset.x - (10.0f + 20.0f)) < 0.01f);
        QVERIFY(std::abs(offset.y - (-5.0f + 40.0f)) < 0.01f);

        // Scale 0 is the documented way to opt out and be refused like a real uncalibrated
        // camera, so it must genuinely leave the calibrator unfitted.
        VirtualCameraDevice optedOut(QStringLiteral("vcam_nocal"), QStringLiteral("VCam2"));
        VirtualCameraCfg noCal;
        noCal.setmillimetresPerPixel(0.0);
        optedOut.setDeviceConfig(noCal.clone());
        QScopedPointer<IDeviceCfg> noCalRead(optedOut.deviceConfig());
        QVERIFY(!dynamic_cast<CameraCfg *>(noCalRead.data())->calibrator().isCalibrated());
    }

    // Phase 7 / D2b: a virtual PLC must offer tags, or no task using it can be configured.
    //
    // LocalizationSettingWidget builds the signal-map editor's lists from
    // IDigitalIoProvider/IWordIoProvider and clears BOTH lists when the device does not
    // implement them. The virtual PLC shipped without them, so the editor silently showed no
    // tags and the signal map could not be bound — the device existed and was useless.
    void test_virtual_plc_offers_tags_to_the_signal_map_editor()
    {
        VirtualPlcDevice plc(QStringLiteral("vplc_tags"), QStringLiteral("VPLC"));

        auto *digital = dynamic_cast<IDigitalIoProvider *>(&plc);
        auto *word = dynamic_cast<IWordIoProvider *>(&plc);
        QVERIFY2(digital != nullptr && word != nullptr,
                 "the signal-map editor dynamic_casts to these; without them it clears the "
                 "tag lists and the task cannot be configured");

        const QStringList bits = digital->availableDigitalIoNames();
        const QStringList words = word->availableWordIoNames();
        QVERIFY(!bits.isEmpty());
        QVERIFY(!words.isEmpty());
        QCOMPARE(bits.first(), QStringLiteral("M0"));
        QCOMPARE(words.first(), QStringLiteral("D0"));

        // Every advertised tag must actually be writable: offering a tag the device then
        // rejects would put the mistake in the operator's signal map instead of here.
        for (const QString &tag : bits) {
            QVERIFY2(plc.writeDigitalIoByName(tag, true), qPrintable(tag));
        }
        for (const QString &tag : words) {
            QVERIFY2(plc.writeWordIoByName(tag, 1), qPrintable(tag));
        }
    }

    // Phase 7 / D3: isVirtualDevice() is the single answer three markers depend on.
    //
    // The widget factory picks the panel from it, the project tree draws the VIRT chip from
    // it, and the task log writes its warning from it. If any of those disagreed the marker
    // would become untrustworthy, which is worse than having no marker: an operator who has
    // seen one wrong VIRT badge stops reading them. Asserted both ways — every virtual device
    // is virtual, and no real device is.
    void test_is_virtual_device_answers_for_every_family()
    {
        VirtualCameraDevice virtualCamera(QStringLiteral("vc"), QStringLiteral("VC"));
        VirtualPlcDevice virtualPlc(QStringLiteral("vp"), QStringLiteral("VP"));
        VirtualVisionOutputDevice virtualOutput(QStringLiteral("vo"), QStringLiteral("VO"));

        QVERIFY(isVirtualDevice(&virtualCamera));
        QVERIFY(isVirtualDevice(&virtualPlc));
        QVERIFY(isVirtualDevice(&virtualOutput));

        BaslerGigECamera realCamera(QStringLiteral("rc"), QStringLiteral("RC"));
        McProtocolDevice realPlc(QStringLiteral("rp"), QStringLiteral("RP"));
        VisionTcpipDevice realOutput(QStringLiteral("ro"), QStringLiteral("RO"));
        NachiRobotDevice realRobot(QStringLiteral("rr"), QStringLiteral("RR"));

        QVERIFY(!isVirtualDevice(&realCamera));
        QVERIFY(!isVirtualDevice(&realPlc));
        QVERIFY(!isVirtualDevice(&realOutput));
        QVERIFY(!isVirtualDevice(&realRobot));
        QVERIFY(!isVirtualDevice(nullptr));
    }

    // Phase 7 / D2b + D2c: the same round trip for the other two families.
    //
    // Both keys have to agree — the top-level family key AND the one inside DeviceConfig.
    // DeviceRegistry::find() reads the top level first, so a device whose nested config
    // disagrees still loads correctly today and silently becomes the WRONG sub-type the
    // moment that key is dropped or hand-edited. Asserting the nested key is what makes that
    // impossible rather than merely unlikely.
    void test_virtual_plc_and_vision_output_round_trip_as_virtual()
    {
        {
            VirtualPlcDevice plc(QStringLiteral("vplc1"), QStringLiteral("Virtual PLC"));
            QCOMPARE(plc.plcType(), PlcType::VirtualPlc);

            const QJsonObject json = plc.toJson();
            const QJsonObject config = json.value(QStringLiteral(DEVICE_JSK_CONFIG)).toObject();
            QCOMPARE(config.value(QStringLiteral(DEVICE_JSK_PLC_TYPE)).toString(),
                     QStringLiteral(PLC_TYPE_VIRTUAL));

            const DeviceRegistryEntry *entry = DeviceRegistry::find(json, DeviceType::PLC);
            QVERIFY(entry != nullptr);
            QCOMPARE(entry->subTypeValue, QStringLiteral(PLC_TYPE_VIRTUAL));

            QScopedPointer<IDevice> rebuilt(entry->creator(json, nullptr));
            auto *rebuiltPlc = dynamic_cast<PlcDevice *>(rebuilt.data());
            QVERIFY(rebuiltPlc != nullptr);
            QCOMPARE(rebuiltPlc->plcType(), PlcType::VirtualPlc);
            QCOMPARE(rebuiltPlc->id(), QStringLiteral("vplc1"));
        }
        {
            VirtualVisionOutputDevice output(QStringLiteral("vout1"),
                                             QStringLiteral("Virtual Vision Output"));
            QCOMPARE(output.visionOutputType(), VisionOutputType::VirtualVisionOutput);

            const QJsonObject json = output.toJson();
            const QJsonObject config = json.value(QStringLiteral(DEVICE_JSK_CONFIG)).toObject();
            QCOMPARE(config.value(QStringLiteral(DEVICE_JSK_VOUT_TYPE)).toString(),
                     QStringLiteral("Virtual"));

            const DeviceRegistryEntry *entry =
                DeviceRegistry::find(json, DeviceType::VisionOutput);
            QVERIFY(entry != nullptr);
            QCOMPARE(entry->subTypeValue, QStringLiteral("Virtual"));

            QScopedPointer<IDevice> rebuilt(entry->creator(json, nullptr));
            auto *rebuiltOutput = dynamic_cast<VisionOutputDevice *>(rebuilt.data());
            QVERIFY(rebuiltOutput != nullptr);
            QCOMPARE(rebuiltOutput->visionOutputType(), VisionOutputType::VirtualVisionOutput);
            QCOMPARE(rebuiltOutput->id(), QStringLiteral("vout1"));
        }
    }

    void test_vision_tcpip_runtime_state_is_not_persisted()
    {
        VisionTcpipDevice device(QStringLiteral("vision_state"), QStringLiteral("Vision State"));

        const VisionTcpipRuntimeState runtimeState = device.runtimeState();
        QCOMPARE(runtimeState.mainClientConnected, false);
        QCOMPARE(runtimeState.heartbeatClientConnected, false);

        const VisionTcpipDiagnostics diagnostics = device.diagnostics();
        QCOMPARE(diagnostics.mainPayloadsReceived, quint64(0));
        QCOMPARE(diagnostics.resultPayloadsSent, quint64(0));

        const QJsonObject json = device.toJson();
        QVERIFY(!json.contains(QStringLiteral("mainClientConnected")));
        QVERIFY(!json.contains(QStringLiteral("heartbeatClientConnected")));
        QVERIFY(!json.contains(QStringLiteral("lastError")));
        QVERIFY(json.contains(QStringLiteral(DEVICE_JSK_CONFIG)));
    }

    void test_camera_runner_standard_command_rejects_overlap()
    {
        auto camera = std::make_shared<BaslerGigECamera>(
            QStringLiteral("cmd_cam"), QStringLiteral("Command Camera"));
        CameraRunner runner(camera.get());

        const DeviceCommand first = DeviceCommand::create(
            DeviceCommandKind::CameraSingleShot,
            camera->id());
        const DeviceCommandResult firstResult = runner.submitCommand(first);
        QVERIFY(firstResult.status == DeviceCommandResultStatus::Accepted);
        QVERIFY(firstResult.code == DeviceCommandResultCode::None);
        QCOMPARE(firstResult.commandId, first.id);

        const DeviceCommand second = DeviceCommand::create(
            DeviceCommandKind::Connect,
            camera->id());
        const DeviceCommandResult busyResult = runner.submitCommand(second);
        QVERIFY(busyResult.status == DeviceCommandResultStatus::Rejected);
        QVERIFY(busyResult.code == DeviceCommandResultCode::Busy);
        QCOMPARE(busyResult.commandId, second.id);
    }

    void test_camera_runner_standard_command_rejects_invalid_target_and_kind()
    {
        auto camera = std::make_shared<BaslerGigECamera>(
            QStringLiteral("cmd_cam_target"), QStringLiteral("Command Camera Target"));
        CameraRunner runner(camera.get());

        const DeviceCommand wrongTarget = DeviceCommand::create(
            DeviceCommandKind::Connect,
            QStringLiteral("another_device"));
        const DeviceCommandResult wrongTargetResult = runner.submitCommand(wrongTarget);
        QVERIFY(wrongTargetResult.status == DeviceCommandResultStatus::Rejected);
        QVERIFY(wrongTargetResult.code == DeviceCommandResultCode::InvalidTarget);

        const DeviceCommand unsupported = DeviceCommand::create(
            DeviceCommandKind::Unknown,
            camera->id());
        const DeviceCommandResult unsupportedResult = runner.submitCommand(unsupported);
        QVERIFY(unsupportedResult.status == DeviceCommandResultStatus::Rejected);
        QVERIFY(unsupportedResult.code == DeviceCommandResultCode::UnsupportedCommand);
    }

    void test_camera_runner_standard_command_emits_success_result()
    {
        auto camera = std::make_shared<BaslerGigECamera>(
            QStringLiteral("cmd_cam_success"), QStringLiteral("Command Camera Success"));
        CameraRunner runner(camera.get());

        QSignalSpy finishedSpy(&runner, &CameraRunner::commandFinished);
        QVERIFY(finishedSpy.isValid());

        const DeviceCommand singleShotCmd = DeviceCommand::create(
            DeviceCommandKind::CameraSingleShot,
            camera->id(),
            800);
        const DeviceCommandResult accepted = runner.submitCommand(singleShotCmd);
        QCOMPARE(accepted.status, DeviceCommandResultStatus::Accepted);

        vc::device::GrabResult done;
        done.isGrabSuccess = true;
        done.msg = QStringLiteral("single shot success");
        QVERIFY(QMetaObject::invokeMethod(
            &runner,
            "onGrabFinished",
            Qt::DirectConnection,
            Q_ARG(vc::device::GrabResult, done)));

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 1000);
        const DeviceCommandResult success =
            findResultByCommandId(finishedSpy, singleShotCmd.id);
        QCOMPARE(success.commandId, singleShotCmd.id);
        QCOMPARE(success.status, DeviceCommandResultStatus::Succeeded);
        QCOMPARE(success.code, DeviceCommandResultCode::None);
    }

    void test_camera_runner_standard_command_timeout_result()
    {
        auto camera = std::make_shared<BaslerGigECamera>(
            QStringLiteral("cmd_cam_timeout"), QStringLiteral("Command Camera Timeout"));
        CameraRunner runner(camera.get());

        QSignalSpy finishedSpy(&runner, &CameraRunner::commandFinished);
        QVERIFY(finishedSpy.isValid());

        const DeviceCommand singleShotCmd = DeviceCommand::create(
            DeviceCommandKind::CameraSingleShot,
            camera->id(),
            40);
        const DeviceCommandResult accepted = runner.submitCommand(singleShotCmd);
        QCOMPARE(accepted.status, DeviceCommandResultStatus::Accepted);

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 1200);
        const DeviceCommandResult timeoutResult =
            findResultByCommandId(finishedSpy, singleShotCmd.id);
        QCOMPARE(timeoutResult.commandId, singleShotCmd.id);
        QCOMPARE(timeoutResult.status, DeviceCommandResultStatus::Failed);
        QCOMPARE(timeoutResult.code, DeviceCommandResultCode::TimedOut);
    }

    void test_camera_runner_standard_command_queue_policy()
    {
        auto camera = std::make_shared<BaslerGigECamera>(
            QStringLiteral("cmd_cam_queue"), QStringLiteral("Command Camera Queue"));
        CameraRunner runner(camera.get());

        QSignalSpy finishedSpy(&runner, &CameraRunner::commandFinished);
        QVERIFY(finishedSpy.isValid());

        const DeviceCommand first = DeviceCommand::create(
            DeviceCommandKind::CameraSingleShot,
            camera->id(),
            600);
        const DeviceCommand second = DeviceCommand::create(
            DeviceCommandKind::CameraSingleShot,
            camera->id(),
            600);
        const DeviceCommand busyRejected = DeviceCommand::create(
            DeviceCommandKind::Connect,
            camera->id(),
            600);

        QCOMPARE(runner.submitCommand(first).status, DeviceCommandResultStatus::Accepted);
        QCOMPARE(runner.submitCommand(second).status, DeviceCommandResultStatus::Accepted);
        const DeviceCommandResult rejectedResult = runner.submitCommand(busyRejected);
        QCOMPARE(rejectedResult.status, DeviceCommandResultStatus::Rejected);
        QCOMPARE(rejectedResult.code, DeviceCommandResultCode::Busy);

        vc::device::GrabResult firstDone;
        firstDone.isGrabSuccess = true;
        firstDone.msg = QStringLiteral("first done");
        QVERIFY(QMetaObject::invokeMethod(
            &runner,
            "onGrabFinished",
            Qt::DirectConnection,
            Q_ARG(vc::device::GrabResult, firstDone)));

        vc::device::GrabResult secondDone;
        secondDone.isGrabSuccess = true;
        secondDone.msg = QStringLiteral("second done");
        QVERIFY(QMetaObject::invokeMethod(
            &runner,
            "onGrabFinished",
            Qt::DirectConnection,
            Q_ARG(vc::device::GrabResult, secondDone)));

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 3, 1500);

        const DeviceCommandResult firstResult = findResultByCommandId(finishedSpy, first.id);
        QCOMPARE(firstResult.commandId, first.id);
        QCOMPARE(firstResult.status, DeviceCommandResultStatus::Succeeded);

        const DeviceCommandResult secondResult = findResultByCommandId(finishedSpy, second.id);
        QCOMPARE(secondResult.commandId, second.id);
        QCOMPARE(secondResult.status, DeviceCommandResultStatus::Succeeded);

        const DeviceCommandResult busyResult =
            findResultByCommandId(finishedSpy, busyRejected.id);
        QCOMPARE(busyResult.commandId, busyRejected.id);
        QCOMPARE(busyResult.status, DeviceCommandResultStatus::Rejected);
        QCOMPARE(busyResult.code, DeviceCommandResultCode::Busy);
    }

    // Bench finding (2026-08-20): the runner's single-shot watchdog shared the generic
    // 3000 ms default while the camera's own blocking grab waits 5000 ms, so the runner
    // gave up before the camera could possibly answer — every slow grab was reported
    // TimedOut and the real result then arrived with no command to resolve. The ordering
    // between these two constants is the invariant; assert it so a future tweak to
    // either side cannot silently reintroduce it.
    void test_camera_runner_watchdog_outlasts_device_grab_timeout()
    {
        QVERIFY2(vc::runtime::CameraRunner::kSingleShotTimeoutMs >
                     vc::device::BaslerGigECamera::kDefaultGrabTimeoutMs,
                 "CameraRunner single-shot watchdog must outlast the camera's own grab timeout");

        // A retry gets its own watchdog window, so the whole chain needs room for every
        // attempt rather than one shared timeout.
        QVERIFY(vc::runtime::CameraRunner::kMaxGrabAttempts >= 2);
    }

    void test_localization_recovery_policy_defaults_match_runtime_spec()
    {
        const LocalizationRecoveryPolicy cameraPolicy = defaultCameraRecoveryPolicy();
        QCOMPARE(cameraPolicy.roleName, QStringLiteral("camera"));
        QCOMPARE(cameraPolicy.retryIntervalMs, 5000);
        QCOMPARE(cameraPolicy.connectTimeoutMs, 3000);

        const LocalizationRecoveryPolicy plcPolicy = defaultPlcRecoveryPolicy();
        QCOMPARE(plcPolicy.roleName, QStringLiteral("primary_plc"));
        QCOMPARE(plcPolicy.retryIntervalMs, 5000);

        const LocalizationRecoveryPolicy visionPolicy = defaultVisionOutputRecoveryPolicy();
        QCOMPARE(visionPolicy.roleName, QStringLiteral("vision_output"));
        QCOMPARE(visionPolicy.retryIntervalMs, 5000);
    }

    // Phase 6 / B1: reconnect is unbounded. The policy answers "how often", never
    // "how many times", so no attempt count can turn a recoverable status into a fault.
    void test_localization_recovery_policy_decision_rules()
    {
        const LocalizationRecoveryPolicy policy = defaultCameraRecoveryPolicy();

        QCOMPARE(decideRecoveryAction(policy,
                                      vc::device::ConnectStatus::Connected,
                                      false),
                 LocalizationRecoveryAction::Ignore);

        QCOMPARE(decideRecoveryAction(policy,
                                      vc::device::ConnectStatus::LostConnected,
                                      false),
                 LocalizationRecoveryAction::RetryScheduled);

        // A retry already pending is still the one case that suppresses another.
        QCOMPARE(decideRecoveryAction(policy,
                                      vc::device::ConnectStatus::LostConnected,
                                      true),
                 LocalizationRecoveryAction::Ignore);

        // Far past the old budget of 10, and still retrying.
        QCOMPARE(decideRecoveryAction(policy,
                                      vc::device::ConnectStatus::ConnectFailed,
                                      false),
                 LocalizationRecoveryAction::RetryScheduled);

        // A status this policy does not consider recoverable is still ignored.
        LocalizationRecoveryPolicy noConnectFailed = policy;
        noConnectFailed.retryOnConnectFailed = false;
        QCOMPARE(decideRecoveryAction(noConnectFailed,
                                      vc::device::ConnectStatus::ConnectFailed,
                                      false),
                 LocalizationRecoveryAction::Ignore);
    }

    // Phase 6 / B2+B3: an unreachable device is retried indefinitely. It must never turn
    // into a task fault, and one outage must produce one "recovering" notice rather than
    // one per attempt. Drives far past the old budget of 10 to prove the budget is gone.
    void test_localization_role_outage_retries_forever_without_faulting()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;

        // Compress the retry interval so a test can cover many attempts; everything else
        // stays at the shipped defaults.
        LocalizationRecoveryPolicy fastCamera = defaultCameraRecoveryPolicy();
        fastCamera.retryIntervalMs = 20;
        fixture.controller.setRecoveryPolicies(fastCamera,
                                               defaultPlcRecoveryPolicy(),
                                               defaultVisionOutputRecoveryPolicy());

        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy faultSpy(&fixture.controller, &LocalizationRuntimeController::runtimeFault);
        QSignalSpy recoveringSpy(&fixture.controller,
                                 &LocalizationRuntimeController::runtimeRecovering);
        QVERIFY(signalSpy.isValid());
        QVERIFY(faultSpy.isValid());
        QVERIFY(recoveringSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        // Camera goes away and refuses to come back.
        fixture.camera1->connectSucceeds = false;
        fixture.camera1->forceConnectionStatus(ConnectStatus::LostConnected);

        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  false, 1000);

        // ~1 s at a 20 ms interval is roughly 50 attempts — five times the retry budget
        // this phase deleted.
        QTest::qWait(1000);

        QCOMPARE(faultSpy.count(), 0);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), false);

        // One outage, at most one notice per distinct unhealthy status (LostConnected,
        // then ConnectFailed once the reconnect attempts start failing) — not one per
        // attempt, which is what buried the operator's event log before.
        QVERIFY2(recoveringSpy.count() <= 2,
                 qPrintable(QStringLiteral("recovering notices: %1").arg(recoveringSpy.count())));

        // The device comes back; the runtime re-arms with no operator action.
        fixture.camera1->connectSucceeds = true;
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 3000);
        QCOMPARE(faultSpy.count(), 0);
    }

    void test_task_state_machine_transition_rules()
    {
        QVERIFY(canTransitionTaskState(TaskState::Idle, TaskState::CommissionStarting));
        QVERIFY(canTransitionTaskState(TaskState::Commission, TaskState::RuntimeStarting));
        QVERIFY(canTransitionTaskState(TaskState::Ready, TaskState::RunningCycle));
        QVERIFY(canTransitionTaskState(TaskState::Recovering, TaskState::Ready));
        QVERIFY(canTransitionTaskState(TaskState::Faulted, TaskState::Stopping));

        // Phase 6 / A4: a fault can be acknowledged (bErrorReset) or auto-cleared, and
        // both arrive as a runtime-ready event. Without this edge the PLC flags would
        // clear while the task stayed Faulted.
        QVERIFY(canTransitionTaskState(TaskState::Faulted, TaskState::Ready));

        QVERIFY(!canTransitionTaskState(TaskState::Idle, TaskState::Ready));
        QVERIFY(!canTransitionTaskState(TaskState::Commission, TaskState::Recovering));
        QVERIFY(!canTransitionTaskState(TaskState::RunningCycle, TaskState::Commission));
        // Recovery re-arms the runtime; it never starts a cycle by itself.
        QVERIFY(!canTransitionTaskState(TaskState::Faulted, TaskState::RunningCycle));
    }

    void test_task_localization_state_machine_rejects_invalid_transition()
    {
        TaskLocalizationProbe task(QStringLiteral("Task State Probe"));
        QCOMPARE(task.taskState(), TaskState::Idle);
        QVERIFY(!task.transitionTaskState(TaskState::Ready,
                                          QStringLiteral("invalid test jump")));
        QCOMPARE(task.taskState(), TaskState::Idle);
    }

    void test_task_localization_state_machine_follows_commission_lifecycle()
    {
        TaskLocalizationProbe task(QStringLiteral("Task Commission Probe"));
        QCOMPARE(task.taskState(), TaskState::Idle);

        task.beginCommission();
        QCOMPARE(task.taskState(), TaskState::Commission);

        task.endCommission();
        QCOMPARE(task.taskState(), TaskState::Idle);
    }

    void test_vision_result_adapter_maps_match_result_with_crop_offset()
    {
        const mtc::MatchResult result = LocalizationRuntimeFixture::makeMatchResult();
        vc::model::CameraWorkspace workspace;
        workspace.conditionRoi = cv::Rect2f(5.0f, 6.0f, 20.0f, 18.0f);
        workspace.useConditionWorkspace = true;

        const VisionResultOverlay overlay = VisionResultAdapter::fromMatchResult(
            result, QSize(64, 48), &workspace);

        QCOMPARE(overlay.sourceImageSize, QSize(64, 48));
        QCOMPARE(overlay.acceptedObjects.size(), 1);
        const VisionResultObject object = overlay.acceptedObjects.front();
        QCOMPARE(object.center, QPointF(27.0, 39.0));
        QCOMPARE(object.corners.at(0), QPointF(17.0, 29.0));
        QCOMPARE(object.corners.at(2), QPointF(37.0, 49.0));
        QCOMPARE(object.pointAngleDeg, 10.0);
        QVERIFY(!overlay.roiOverlays.isEmpty());
    }

    void test_vision_result_adapter_marks_sent_runtime_objects()
    {
        vc::model::LocalizationRuntimeController::CycleResult cycleResult;
        cycleResult.rawImage = cv::Mat(48, 64, CV_8UC1, cv::Scalar(42));
        cycleResult.matchResult = LocalizationRuntimeFixture::makeMatchResult();

        vc::model::LocalizationRuntimeController::ResultRow row;
        row.index = 1;
        row.patternName = QStringLiteral("Pattern 1");
        row.score = 0.95;
        row.status = QStringLiteral("Sent");
        cycleResult.rows.append(row);

        QMap<QString, QVariant> runtimeSignals;
        runtimeSignals.insert(QStringLiteral("nDetectedNumber"), 1);
        runtimeSignals.insert(QStringLiteral("bMatchingFinished"), true);

        const VisionResultOverlay overlay = VisionResultAdapter::fromCycleResult(
            cycleResult, nullptr, &runtimeSignals);

        QCOMPARE(overlay.acceptedObjects.size(), 1);
        QVERIFY(overlay.acceptedObjects.front().sentToOutput);
        QCOMPARE(overlay.runtimeSignalValues.value(QStringLiteral("nDetectedNumber")).toInt(), 1);
    }

    void test_task_localization_runtime_signals_drive_recovering_and_faulted_states()
    {
        TaskLocalizationProbe task(QStringLiteral("Task Runtime Probe"));
        QVERIFY(task.findChild<LocalizationRuntimeController *>() == nullptr);
        QVERIFY(task.transitionTaskState(TaskState::CommissionStarting,
                                         QStringLiteral("test setup")));
        QVERIFY(task.transitionTaskState(TaskState::Commission,
                                         QStringLiteral("test setup")));
        QVERIFY(task.transitionTaskState(TaskState::RuntimeStarting,
                                         QStringLiteral("test setup")));
        QVERIFY(task.transitionTaskState(TaskState::Ready,
                                         QStringLiteral("test setup")));

        QVERIFY(QMetaObject::invokeMethod(&task,
                                          "onRuntimeRecovering",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("recovering"))));
        QCOMPARE(task.taskState(), TaskState::Recovering);

        QVERIFY(QMetaObject::invokeMethod(&task,
                                          "onRuntimeReady",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("ready"))));
        QCOMPARE(task.taskState(), TaskState::Ready);

        QVERIFY(QMetaObject::invokeMethod(&task,
                                          "onRuntimeFault",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("fault"))));
        QCOMPARE(task.taskState(), TaskState::Faulted);

        task.stopAll();
        QCOMPARE(task.taskState(), TaskState::Idle);
    }

    void test_localization_runtime_trigger_cycle_uses_matching_worker_contract()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy cycleSpy(&fixture.controller, &LocalizationRuntimeController::cycleResultUpdated);
        int matchingCount = 0;
        int lastCycleId = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int cycleId,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace,
                             cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            matchingCount += 1;
            lastCycleId = cycleId;
        });
        QVERIFY(signalSpy.isValid());
        QVERIFY(cycleSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(matchingCount, 1, 1000);

        fixture.controller.onRuntimeMatchingFinished(
            lastCycleId,
            LocalizationRuntimeFixture::makeMatchResult());

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 1000);
        auto result = qvariant_cast<LocalizationRuntimeController::CycleResult>(
            cycleSpy.takeFirst().at(0));
        QCOMPARE(result.faulted, false);
        QCOMPARE(result.detectedNumber, 1);
        QCOMPARE(result.sentNumber, 1);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.visionOutput->requestCount, 1, 1000);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTest::qWait(50);
        QCOMPARE(matchingCount, 1);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), false}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bMatchingFinished")).toBool(), false);
    }

    void test_localization_runtime_grab_timeout_faults_without_vision_output()
    {
        LocalizationRuntimeFixture fixture;
        fixture.camera1->grabSucceeds = false;
        QSignalSpy cycleSpy(&fixture.controller, &LocalizationRuntimeController::cycleResultUpdated);
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(cycleSpy.isValid());
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 1000);

        auto result = qvariant_cast<LocalizationRuntimeController::CycleResult>(
            cycleSpy.takeFirst().at(0));
        QCOMPARE(result.faulted, true);
        QCOMPARE(result.faultCode, LocalizationFaultCode::CameraGrabTimeout);
        QCOMPARE(fixture.visionOutput->requestCount, 0);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(), 102);
    }

    void test_localization_runtime_vision_output_failure_faults_cycle()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        fixture.visionOutput->sendSucceeds = false;
        QSignalSpy cycleSpy(&fixture.controller, &LocalizationRuntimeController::cycleResultUpdated);
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        int matchingCount = 0;
        int lastCycleId = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int cycleId,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace,
                             cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            matchingCount += 1;
            lastCycleId = cycleId;
        });
        QVERIFY(cycleSpy.isValid());
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(matchingCount, 1, 1000);

        fixture.controller.onRuntimeMatchingFinished(
            lastCycleId,
            LocalizationRuntimeFixture::makeMatchResult());

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 1000);
        auto result = qvariant_cast<LocalizationRuntimeController::CycleResult>(
            cycleSpy.takeFirst().at(0));
        QCOMPARE(result.faulted, true);
        QCOMPARE(result.faultCode, LocalizationFaultCode::VisionOutputSendFailed);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(), 201);
    }

    // Drives a real cycle to a fault, lets the execute trigger fall (which is where the
    // fault used to come to rest with no way out), and returns the shared spy/state so
    // the two recovery tests below differ only in HOW the fault is cleared.
    static void faultCycleAndReleaseTrigger(LocalizationRuntimeFixture &fixture,
                                            QSignalSpy &signalSpy)
    {
        int matchingCount = 0;
        int lastCycleId = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int cycleId,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace,
                             cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            matchingCount += 1;
            lastCycleId = cycleId;
        });

        fixture.visionOutput->sendSucceeds = false;
        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(matchingCount, 1, 1000);
        fixture.controller.onRuntimeMatchingFinished(
            lastCycleId, LocalizationRuntimeFixture::makeMatchResult());

        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(),
                                  true, 1000);

        // Falling edge of the execute trigger: the cycle fault comes to rest here.
        fixture.controller.handlePlcValues({{QStringLiteral("M10"), false}});
    }

    // Phase 6 / A3: the bErrorReset rising edge clears the latched fault and re-arms.
    void test_localization_runtime_error_reset_clears_fault_and_rearms()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        faultCycleAndReleaseTrigger(fixture, signalSpy);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);

        fixture.controller.handlePlcValues({{QStringLiteral("M19"), true}});

        // Well inside kFaultAutoRecoverMs, so this proves the acknowledge did it rather
        // than the automatic timer that the next test covers.
        QVERIFY(LocalizationRuntimeController::kFaultAutoRecoverMs > 500);
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(),
                                  false, 500);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(), 0);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), true);
    }

    // Phase 6 / A5: with bErrorReset never asserted, the same fault clears itself. This
    // is the case that used to park the runtime until the task was restarted.
    void test_localization_runtime_fault_auto_clears_without_error_reset()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        faultCycleAndReleaseTrigger(fixture, signalSpy);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);

        // No bErrorReset is ever written here.
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
            true,
            LocalizationRuntimeController::kFaultAutoRecoverMs + 2000);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(), 0);
    }

    void test_localization_runtime_rejects_camera_change_while_running()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        int matchingCount = 0;
        int lastCycleId = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int cycleId,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace,
                             cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            matchingCount += 1;
            lastCycleId = cycleId;
        });
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(matchingCount, 1, 1000);

        fixture.controller.setActiveCameraNumber(2);
        QTest::qWait(50);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nActiveCamera")).toInt(), 0);

        fixture.controller.onRuntimeMatchingFinished(
            lastCycleId,
            LocalizationRuntimeFixture::makeMatchResult());
    }

    void test_localization_runtime_setup_faults_on_invalid_pattern_group()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        auto context = fixture.context();
        context.activePatternGroupNumber = 99;

        const auto setup = fixture.controller.setup(context);
        QVERIFY(!setup.valid);
        QVERIFY(setup.errors.contains(QStringLiteral("Active pattern group is missing.")));
        QCOMPARE(fixture.controller.isValid(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bPatternValid")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(), 400);
    }

    void test_localization_runtime_setup_faults_on_invalid_calibration()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context(/*calibrated=*/false));
        QVERIFY(!setup.valid);
        QVERIFY(setup.errors.contains(QStringLiteral("Active camera calibration is invalid.")));
        QCOMPARE(fixture.controller.isValid(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bCameraValid")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(), 401);
    }

    void test_localization_runtime_ready_and_cycle_outputs_write_plc_tags()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy cycleSpy(&fixture.controller, &LocalizationRuntimeController::cycleResultUpdated);
        int matchingCount = 0;
        int lastCycleId = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int cycleId,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace,
                             cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            matchingCount += 1;
            lastCycleId = cycleId;
        });
        QVERIFY(signalSpy.isValid());
        QVERIFY(cycleSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);
        // Ready-output tags are written asynchronously via PlcRunner in no
        // guaranteed order. Wait for the whole batch to land before asserting
        // values; a tag checked with a plain QVERIFY right after a single QTRY
        // can otherwise race the writer thread under load.
        QTRY_VERIFY_WITH_TIMEOUT(
            fixture.plc->digitalWrites.contains(QStringLiteral("M11")) &&
                fixture.plc->digitalWrites.contains(QStringLiteral("M12")) &&
                fixture.plc->digitalWrites.contains(QStringLiteral("M18")) &&
                fixture.plc->wordWrites.contains(QStringLiteral("D102")) &&
                fixture.plc->wordWrites.contains(QStringLiteral("D103")),
            1000);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M11")), true);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M12")), false);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M18")), false);
        QCOMPARE(fixture.plc->wordWrites.value(QStringLiteral("D102")), qint16(0));
        QCOMPARE(fixture.plc->wordWrites.value(QStringLiteral("D103")), qint16(0));

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(matchingCount, 1, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.plc->digitalWrites.value(QStringLiteral("M12")),
                                  true,
                                  1000);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M13")), false);

        fixture.controller.onRuntimeMatchingFinished(
            lastCycleId,
            LocalizationRuntimeFixture::makeMatchResult());

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 1000);
        // Wait for every tag that changes value in the cycle-complete batch
        // before asserting the tags that stay put — same async-ordering reason
        // as the ready phase. These tags already exist from the ready write, so
        // gate on their new values rather than presence.
        QTRY_VERIFY_WITH_TIMEOUT(
            fixture.plc->digitalWrites.value(QStringLiteral("M12")) == false &&
                fixture.plc->digitalWrites.value(QStringLiteral("M13")) == true &&
                fixture.plc->digitalWrites.value(QStringLiteral("M14")) == true &&
                fixture.plc->wordWrites.value(QStringLiteral("D102")) == qint16(1),
            1000);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M18")), false);
        QCOMPARE(fixture.plc->wordWrites.value(QStringLiteral("D103")), qint16(0));
    }

    void test_plc_runner_rejects_invalid_tag_writes()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy errorSpy(fixture.plcRunner.data(), &PlcRunner::errorOccurred);
        QVERIFY(errorSpy.isValid());

        fixture.plcRunner->requestWriteDigitalIo(QStringLiteral("D100"), true);
        fixture.plcRunner->requestWriteWordIo(QStringLiteral("M10"), 7);

        QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 2, 1000);
        QVERIFY(!fixture.plc->digitalWrites.contains(QStringLiteral("D100")));
        QVERIFY(!fixture.plc->wordWrites.contains(QStringLiteral("M10")));
    }

    void test_localization_runtime_camera_loss_faults_running_cycle()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy cycleSpy(&fixture.controller, &LocalizationRuntimeController::cycleResultUpdated);
        int matchingCount = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace,
                             cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            matchingCount += 1;
        });
        QVERIFY(signalSpy.isValid());
        QVERIFY(cycleSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(matchingCount, 1, 1000);

        fixture.camera1->forceConnectionStatus(ConnectStatus::LostConnected);

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 1000);
        auto result = qvariant_cast<LocalizationRuntimeController::CycleResult>(
            cycleSpy.takeFirst().at(0));
        QCOMPARE(result.faulted, true);
        QCOMPARE(result.faultCode, LocalizationFaultCode::CameraLost);
        QCOMPARE(fixture.visionOutput->requestCount, 0);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(), 100);
    }

    // Phase 6 / E1: one shell, one process. Both shells own their camera, PLC socket and
    // vision-output port exclusively, so a second instance cannot work — and on a field
    // machine a second launch is routine (boot-start plus an operator clicking the icon).
    void test_single_instance_guard_blocks_a_second_acquire()
    {
        const QString key = QStringLiteral("ncr_test_guard_primary");

        SingleInstanceGuard first(key);
        QVERIFY2(first.tryAcquire(), "the first instance must win the key");

        SingleInstanceGuard second(key);
        QVERIFY2(!second.tryAcquire(), "a second instance must not acquire the same key");
    }

    // Distinct keys stay independent — the mechanism still works that way, and the guard
    // is a general utility, so this is asserted separately from the policy below.
    void test_single_instance_guard_keys_are_independent()
    {
        SingleInstanceGuard first(QStringLiteral("ncr_test_guard_key_a"));
        SingleInstanceGuard second(QStringLiteral("ncr_test_guard_key_b"));

        QVERIFY(first.tryAcquire());
        QVERIFY2(second.tryAcquire(),
                 "a different instance key must not be blocked by another key's lock");
    }

    // Phase 7 / B1 — THIS INVERTS A PHASE 6 DECISION, deliberately.
    //
    // Phase 6 / E1 gave each shell its own instance key so they would NOT block each
    // other, on the stated grounds that stopping them co-running needed an ordered device
    // hand-off rather than a refusal to start. Phase 7 built that hand-off
    // (vc::shell::ShellHandoff), so the refusal is now the right default: both shells take
    // ONE key, and only one of the two applications runs at a time.
    //
    // The assertion is inverted rather than deleted so the reversal is visible in the diff
    // instead of looking like a test someone quietly dropped.
    void test_both_shells_take_the_same_instance_key()
    {
        using vc::shell::ShellHandoff;
        using vc::shell::ShellKind;

        // The key is one constant, not a string repeated in two mains.
        const QString key = QLatin1String(ShellHandoff::kInstanceKey);
        QVERIFY(!key.isEmpty());

        SingleInstanceGuard editorLikeGuard(key);
        SingleInstanceGuard runtimeLikeGuard(key);

        QVERIFY(editorLikeGuard.tryAcquire());
        QVERIFY2(!runtimeLikeGuard.tryAcquire(),
                 "the two shells share one instance key, so the second must be refused — "
                 "they own the same camera, PLC socket and output port");

        // Neither shell's main() may reintroduce a private key.
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);
        // Phase 7 / C1 moved the runtime shell's entry point into runtime_app/src/.
        for (const QString &shellMain : {QStringLiteral("/app/main.cpp"),
                                         QStringLiteral("/runtime_app/src/main.cpp")}) {
            QFile file(repoRoot + shellMain);
            QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(shellMain));
            const QString text = QString::fromUtf8(file.readAll());
            QVERIFY2(text.contains(QStringLiteral("ShellHandoff::kInstanceKey")),
                     qPrintable(QStringLiteral("shell does not use the shared instance key: ")
                                + shellMain));
        }

        // The two shells are still distinguishable to a losing launch, which is what lets
        // it say WHICH one is running instead of raising an unexpected window in silence.
        QVERIFY(ShellHandoff::applicationName(ShellKind::Commissioning)
                != ShellHandoff::applicationName(ShellKind::OperatorRuntime));
        QVERIFY(ShellHandoff::executableName(ShellKind::Commissioning)
                != ShellHandoff::executableName(ShellKind::OperatorRuntime));
        QCOMPARE(ShellHandoff::siblingOf(ShellKind::Commissioning), ShellKind::OperatorRuntime);
        QCOMPARE(ShellHandoff::siblingOf(ShellKind::OperatorRuntime), ShellKind::Commissioning);
    }

    // Phase 7 / B2: a hand-off launch must WAIT for the outgoing shell, not give up.
    //
    // This is the phase's highest-risk detail and its failure mode is silence: the
    // incoming shell exits with no window, which from the operator's side is
    // indistinguishable from the menu item doing nothing. The timeout is what prevents it,
    // so the timeout is asserted rather than trusted.
    void test_handoff_launch_waits_for_the_outgoing_shell()
    {
        using vc::shell::ShellHandoff;

        QVERIFY2(ShellHandoff::kHandoffAcquireTimeoutMs > 0,
                 "a hand-off launch that does not wait races the shell it is replacing");

        QVERIFY(ShellHandoff::wasStartedForHandoff(
            QStringList{ QStringLiteral("ncr_runtime.exe"),
                         QLatin1String(ShellHandoff::kHandoffFlag) }));
        QVERIFY(!ShellHandoff::wasStartedForHandoff(
            QStringList{ QStringLiteral("ncr_runtime.exe") }));

        // A guard given a timeout must actually honour it rather than returning at once.
        const QString key = QStringLiteral("ncr_test_guard_handoff_wait");
        SingleInstanceGuard holder(key);
        QVERIFY(holder.tryAcquire());

        QElapsedTimer elapsed;
        elapsed.start();
        SingleInstanceGuard waiter(key);
        QVERIFY2(!waiter.tryAcquire(300),
                 "the lock is held, so acquiring must fail even with a timeout");
        QVERIFY2(elapsed.elapsed() >= 250,
                 "tryAcquire(timeout) returned immediately — the hand-off would race");
    }

    // A crash must not lock the application out of its own machine: releasing the key
    // has to make it immediately available again.
    void test_single_instance_guard_releases_its_key_on_destruction()
    {
        const QString key = QStringLiteral("ncr_test_guard_release");

        {
            SingleInstanceGuard owner(key);
            QVERIFY(owner.tryAcquire());
        }  // destroyed — lock released

        SingleInstanceGuard next(key);
        QVERIFY2(next.tryAcquire(),
                 "the key must be reusable once the previous owner is gone");
    }

    // Phase 6 / E4: two executables ship in one folder and must come from the same build.
    // The version is defined once and both shells publish it; this catches the drift mode
    // where someone hardcodes a version string into one shell.
    void test_both_shells_report_one_shared_version()
    {
        QVERIFY(!vc::version::applicationVersion().isEmpty());
        QCOMPARE(vc::version::applicationVersion(),
                 QString::fromLatin1(vc::version::kApplicationVersion));

        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);
        // Phase 7 / C1 moved the runtime shell's entry point into runtime_app/src/.
        for (const QString &shellMain : {QStringLiteral("/app/main.cpp"),
                                         QStringLiteral("/runtime_app/src/main.cpp")}) {
            QFile file(repoRoot + shellMain);
            QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(shellMain));
            const QString text = QString::fromUtf8(file.readAll());

            QVERIFY2(text.contains(QStringLiteral("core/app_version.h")),
                     qPrintable(QStringLiteral("shell does not use the shared version "
                                               "header: ") + shellMain));
            QVERIFY2(text.contains(QStringLiteral("setApplicationVersion")),
                     qPrintable(QStringLiteral("shell does not publish its version: ")
                                + shellMain));
        }
    }

    // Phase 7 / A4: access control starts closed and needs a real credential to open.
    //
    // The role used to be decorative — clicking "Admin" changed a menu title and nothing
    // read it. Everything below is the behaviour that replaced it, driven through the
    // public API rather than through the UI, and against an injected provider so the test
    // does not depend on whatever credential this machine happens to have stored.
    void test_access_control_starts_as_operator_and_requires_a_password()
    {
        using vc::auth::AccessControl;
        using vc::auth::AccessRole;

        /// Accepts exactly one password. Standing in for a dongle: the point of the seam
        /// is that AccessControl neither knows nor cares where the answer comes from.
        class FixedPasswordProvider : public vc::auth::IAdminCredentialProvider {
        public:
            bool isConfigured() const override { return true; }
            bool verify(const QString &password) const override
            {
                return password == QStringLiteral("s3cret");
            }
        };

        AccessControl *access = AccessControl::instance();
        access->dropToOperator();
        access->setCredentialProvider(std::make_unique<FixedPasswordProvider>());

        QCOMPARE(access->role(), AccessRole::Operator);
        QVERIFY(!access->isAdmin());
        QVERIFY(access->canElevate());

        QSignalSpy roleSpy(access, &AccessControl::roleChanged);

        // A wrong password must change nothing at all — not the role, and not the signal
        // that other code uses to follow it.
        QVERIFY(!access->elevate(QStringLiteral("wrong")));
        QCOMPARE(access->role(), AccessRole::Operator);
        QCOMPARE(roleSpy.count(), 0);

        QVERIFY(access->elevate(QStringLiteral("s3cret")));
        QCOMPARE(access->role(), AccessRole::Admin);
        QCOMPARE(roleSpy.count(), 1);

        // Elevating again while already Admin is a no-op, not a second notification.
        QVERIFY(access->elevate(QStringLiteral("s3cret")));
        QCOMPARE(roleSpy.count(), 1);

        access->dropToOperator();
        QCOMPARE(access->role(), AccessRole::Operator);
        QCOMPARE(roleSpy.count(), 2);

        // Restore the default provider so later tests see a clean process.
        access->setCredentialProvider(nullptr);
    }

    // Phase 7 / A4: the admin credential is stored as a salted hash, never as the password.
    //
    // The stored form is the whole security value of this gate: it means copying
    // settings.dat off the machine does not hand over the password. Runs against the
    // QStandardPaths test sandbox set up in initTestCase(), so it seeds and rewrites a
    // throwaway settings file rather than the developer's.
    void test_admin_credential_is_stored_hashed_and_salted()
    {
        using vc::auth::SettingsAdminCredentialProvider;

        // Start from "no credential stored". The sandbox settings file SURVIVES between
        // runs, and this test ends with a changed password — without this the second run
        // would find a configured credential, skip seeding, and fail on a default password
        // the first run had already replaced. Depending on run order is a bug in the test,
        // not a quirk to work around.
        AppSettings *mutableSettings = AppSettings::instance();
        mutableSettings->setValue(QLatin1String(AppKey::adminPasswordSalt), QString());
        mutableSettings->setValue(QLatin1String(AppKey::adminPasswordHash), QString());

        SettingsAdminCredentialProvider provider;

        // A fresh installation seeds itself with the shipped default, so an operator is
        // never locked out of a machine that has never been configured.
        QVERIFY(provider.isConfigured());
        QVERIFY(provider.verify(QString::fromLatin1(
            SettingsAdminCredentialProvider::kDefaultPassword)));
        QVERIFY(!provider.verify(QStringLiteral("not the password")));
        QVERIFY2(!provider.verify(QString()),
                 "an empty password must never verify");

        const AppSettings *settings = AppSettings::instance();
        const QString salt = settings->value(QLatin1String(AppKey::adminPasswordSalt)).toString();
        const QString hash = settings->value(QLatin1String(AppKey::adminPasswordHash)).toString();

        QVERIFY(!salt.isEmpty());
        QVERIFY(!hash.isEmpty());
        QVERIFY2(!hash.contains(QLatin1String(SettingsAdminCredentialProvider::kDefaultPassword)),
                 "the stored credential must not contain the password");
        QVERIFY2(hash != QString::fromLatin1(
                     SettingsAdminCredentialProvider::kDefaultPassword),
                 "the stored credential must not BE the password");

        // Changing the password takes effect and invalidates the old one.
        QVERIFY(provider.setPassword(QStringLiteral("another-one")));
        QVERIFY(provider.verify(QStringLiteral("another-one")));
        QVERIFY(!provider.verify(QString::fromLatin1(
            SettingsAdminCredentialProvider::kDefaultPassword)));

        // A fresh salt per write: the same password must not produce the same stored hash
        // twice, or two machines with the default password would be identifiable from the
        // file alone.
        const QString firstHash =
            settings->value(QLatin1String(AppKey::adminPasswordHash)).toString();
        QVERIFY(provider.setPassword(QStringLiteral("another-one")));
        const QString secondHash =
            settings->value(QLatin1String(AppKey::adminPasswordHash)).toString();
        QVERIFY2(firstHash != secondHash,
                 "the same password must hash differently each time it is stored");

        QVERIFY2(!provider.setPassword(QString()),
                 "an empty admin password must be refused");

        // Leave the sandbox on the shipped default, so a later test or a rerun starts from
        // the same place this one did.
        QVERIFY(provider.setPassword(
            QString::fromLatin1(SettingsAdminCredentialProvider::kDefaultPassword)));
    }

    // Phase 7 / A3: the two shells are ONE product and share ONE settings file.
    //
    // They set different application names ("NCRN Pick" / "NCRN Pick Runtime"), and the
    // settings path used to be derived from that name — which is exactly why a theme or
    // language chosen in the editor was invisible to the runtime. The invariant is
    // therefore not "the path looks right", it is "the path does not move when the
    // application name does", and that is what this drives directly.
    void test_settings_path_is_product_scoped_not_application_scoped()
    {
        const QString original = QCoreApplication::applicationName();

        QCoreApplication::setApplicationName(QStringLiteral("NCRN Pick"));
        const QString sharedAsEditor = AppSettings::filePath();
        const QString legacyAsEditor = AppSettings::legacyFilePath();

        QCoreApplication::setApplicationName(QStringLiteral("NCRN Pick Runtime"));
        const QString sharedAsRuntime = AppSettings::filePath();
        const QString legacyAsRuntime = AppSettings::legacyFilePath();

        QCoreApplication::setApplicationName(original);

        QCOMPARE(sharedAsEditor, sharedAsRuntime);
        QVERIFY2(sharedAsEditor.contains(QLatin1String(AppSettings::kProductFolder)),
                 qPrintable(QStringLiteral("settings path is not under the product folder: ")
                            + sharedAsEditor));
        QVERIFY(sharedAsEditor.endsWith(QLatin1String("settings.dat")));

        // The legacy path is the one that DOES follow the application name. That is what
        // makes it a one-way fallback for settings written before the unification, rather
        // than a second live location — and asserting it here stops someone "fixing" the
        // two to agree, which would quietly delete the migration path.
        QVERIFY2(legacyAsEditor != legacyAsRuntime,
                 "legacyFilePath() must still be application-scoped; it is the pre-Phase-7 "
                 "location each shell wrote to, and load() reads it to migrate those values");
    }

    // Phase 7 / C1 + C2: the runtime shell is split into src/ + ui/, and its window is
    // authored in Designer rather than constructed in code.
    //
    // The UI rule ("Layout structure lives in .ui") was checkable only by reading until
    // now, which is how the runtime shell shipped in Phase 6 with its whole widget tree
    // new-ed in a .cpp and nobody noticed (backlog #35). This makes it mechanical for the
    // shell that was fixed.
    void test_runtime_shell_is_structured_and_form_driven()
    {
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);
        const QString root = repoRoot + QStringLiteral("/runtime_app");

        QVERIFY2(QDir(root + QStringLiteral("/src")).exists(),
                 "runtime_app/src is missing — the shell structure was flattened again");
        QVERIFY2(QDir(root + QStringLiteral("/ui")).exists(),
                 "runtime_app/ui is missing — the shell structure was flattened again");

        const QString form = root + QStringLiteral("/ui/runtime_shell_window.ui");
        QVERIFY2(QFile::exists(form), qPrintable(form));

        QFile pri(root + QStringLiteral("/runtime_app.pri"));
        QVERIFY(pri.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString priText = QString::fromUtf8(pri.readAll());
        QVERIFY2(priText.contains(QStringLiteral("FORMS")),
                 "runtime_app.pri declares no FORMS, so the .ui is not compiled at all");
        QVERIFY2(priText.contains(QStringLiteral("ui/runtime_shell_window.ui")),
                 "runtime_app.pri does not list the shell window form");

        // Structure in the .ui means these are not constructed in code. Scoped to
        // runtime_app deliberately: app/mainwindow.cpp still has one (the ADS dock host's
        // layout), recorded in the backlog rather than fixed here — widening this check to
        // the editor is a separate change to a window nobody asked to touch.
        static const QRegularExpression layoutPrimitive(
            QStringLiteral("\\bnew\\s+(QVBoxLayout|QHBoxLayout|QGridLayout|QFormLayout"
                           "|QStackedWidget|QToolBar|QMenuBar|QStatusBar)\\b"));

        // Own headers are included with the runtime_app/ prefix. A bare "runtime_shell_window.h"
        // would work today and become ambiguous the moment src/ grows a file of the same
        // name — and src/ is already on the include path, so which one won would depend on
        // INCLUDEPATH order.
        QStringList ownHeaders;
        QDirIterator headerIt(root, { QStringLiteral("*.h") }, QDir::Files,
                              QDirIterator::Subdirectories);
        while (headerIt.hasNext()) {
            ownHeaders << QFileInfo(headerIt.next()).fileName();
        }
        QVERIFY(!ownHeaders.isEmpty());

        int scanned = 0;
        QDirIterator it(root, { QStringLiteral("*.h"), QStringLiteral("*.cpp") },
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (path.contains(QStringLiteral("/build/"))) {
                continue;
            }
            QFile file(path);
            QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
            const QString text = QString::fromUtf8(file.readAll());
            ++scanned;

            const QRegularExpressionMatch primitive = layoutPrimitive.match(text);
            QVERIFY2(!primitive.hasMatch(),
                     qPrintable(QStringLiteral("%1 constructs %2 in code; layout structure "
                                               "belongs in the .ui (ui_design_rules.md "
                                               "Rule 1.1)")
                                    .arg(path, primitive.captured(1))));

            for (const QString &header : ownHeaders) {
                QVERIFY2(!text.contains(QStringLiteral("#include \"") + header + QLatin1Char('"')),
                         qPrintable(QStringLiteral("%1 includes its own header \"%2\" "
                                                   "unprefixed; use \"runtime_app/...\" so "
                                                   "it cannot be confused with src/")
                                        .arg(path, header)));
            }
        }
        QVERIFY2(scanned >= 5, qPrintable(QStringLiteral("scanned only %1 files under "
                                                         "runtime_app").arg(scanned)));
    }

    // Phase 7 / C: blocking a QAction's signals silently breaks the QActionGroup it is in.
    //
    // QActionGroup tracks its checked action through QAction::changed. Block that while
    // calling setChecked() and the group never learns anything was checked, so
    // checkedAction() returns nullptr. In the runtime shell this made selectLayoutAction()
    // look like it worked while applyCurrentLayout() quietly docked nothing: the operator
    // got an empty window with the task docks stranded at 100x30 over the menu bar. It cost
    // two rounds of manual testing to find, because nothing logs and nothing crashes.
    //
    // The blocker also protects against nothing. setChecked() emits toggled()/changed(),
    // never triggered(), and QActionGroup::triggered is the only signal either shell wires
    // to a slot — so re-entrancy was never possible in the first place.
    void test_action_signals_are_never_blocked_in_the_shells()
    {
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);

        // "QSignalBlocker b(act)" / "QSignalBlocker b{action}" / "m_actTheme->blockSignals("
        static const QRegularExpression blockerOnAction(
            QStringLiteral("\\bQSignalBlocker\\s+\\w+\\s*[({][^)}]*\\b\\w*[Aa]ct(?:ion)?\\w*"
                           "\\s*[)}]"));
        static const QRegularExpression blockSignalsOnAction(
            QStringLiteral("\\b\\w*[Aa]ct(?:ion)?\\w*\\s*->\\s*blockSignals\\s*\\("));

        int scanned = 0;
        for (const QString &shell : {QStringLiteral("app"), QStringLiteral("runtime_app")}) {
            QDirIterator it(repoRoot + QStringLiteral("/") + shell,
                            { QStringLiteral("*.h"), QStringLiteral("*.cpp") },
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                if (path.contains(QStringLiteral("/build/"))) {
                    continue;
                }
                QFile file(path);
                QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(path));
                const QString text = QString::fromUtf8(file.readAll());
                ++scanned;

                const QString reason =
                    QStringLiteral("%1 blocks a QAction's signals. QActionGroup tracks its "
                                   "checked action through QAction::changed, so this leaves "
                                   "checkedAction() reporting nothing is checked. Call "
                                   "setChecked() unguarded — it never emits triggered(), "
                                   "which is the only signal wired to a slot here.");

                QVERIFY2(!blockerOnAction.match(text).hasMatch(), qPrintable(reason.arg(path)));
                QVERIFY2(!blockSignalsOnAction.match(text).hasMatch(),
                         qPrintable(reason.arg(path)));
            }
        }
        QVERIFY2(scanned >= 8, qPrintable(QStringLiteral("scanned only %1 shell files")
                                              .arg(scanned)));
    }

    // Phase 6 / D2: the commissioning shell and the operator runtime shell are peers
    // over the same src/ modules, not a hierarchy. The layering test above already
    // forbids each from including the other, but only for files it scans — this asserts
    // the second shell's directory actually exists and carries its own scope card, so a
    // future rename cannot quietly turn the peer rule into a no-op.
    void test_runtime_shell_is_a_peer_of_the_app_shell()
    {
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);

        for (const QString &shell : {QStringLiteral("app"), QStringLiteral("runtime_app")}) {
            const QString dir = repoRoot + QStringLiteral("/") + shell;
            QVERIFY2(QDir(dir).exists(), qPrintable(dir));
            QVERIFY2(QFile::exists(dir + QStringLiteral("/AGENTS.md")),
                     qPrintable(QStringLiteral("shell is missing its scope card: ") + dir));
        }

        // Both shells must build from the shared qmake include rather than their own
        // copies of the module list; a copy drifts the first time a module is added.
        QVERIFY(QFile::exists(repoRoot + QStringLiteral("/qmake/app_common.pri")));

        for (const QString &pro : {QStringLiteral("/ncr_picking.pro"),
                                   QStringLiteral("/runtime_app/ncr_runtime.pro")}) {
            QFile file(repoRoot + pro);
            QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(pro));
            const QString text = QString::fromUtf8(file.readAll());
            QVERIFY2(text.contains(QStringLiteral("app_common.pri")),
                     qPrintable(QStringLiteral("shell .pro does not include the shared "
                                               "qmake config: ") + pro));
        }

        // The umbrella must list every shell. A shell missing from it builds only when
        // someone remembers to build it separately, which is exactly how one shell ends
        // up stale against a shared src/ change.
        QFile umbrella(repoRoot + QStringLiteral("/ncr_picking_all.pro"));
        QVERIFY2(umbrella.open(QIODevice::ReadOnly | QIODevice::Text),
                 "ncr_picking_all.pro is missing");
        const QString umbrellaText = QString::fromUtf8(umbrella.readAll());
        QVERIFY(umbrellaText.contains(QStringLiteral("subdirs")));
        QVERIFY2(umbrellaText.contains(QStringLiteral("ncr_picking.pro")),
                 "umbrella does not build the editor shell");
        QVERIFY2(umbrellaText.contains(QStringLiteral("ncr_runtime.pro")),
                 "umbrella does not build the runtime shell");
    }

    // Phase 6 / E7c: src/ is compiled ONCE into the ncr_shared static library, and the
    // .qrc files must not go with it.
    //
    // Qt registers a resource through a static initialiser in the generated qrc_*.cpp.
    // Inside a static library nothing references a symbol in that object file, so the
    // linker drops it and the resource DOES NOT EXIST at runtime: icons, the QSS theme
    // and :/i18n all resolve to nothing — with no build error and no warning. The build
    // cannot catch this and neither can a link. This test is the guard that can, which is
    // why it asserts the split from both sides rather than trusting a comment.
    void test_resources_belong_to_the_shells_not_the_shared_library()
    {
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);

        auto read = [](const QString &path) {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                return QString();
            return QString::fromUtf8(f.readAll());
        };

        // qmake comments explain this rule at length, and those comments name the very
        // files being searched for. Strip them, or the test passes on prose.
        auto code = [](const QString &text) {
            QStringList out;
            const QStringList lines = text.split(QLatin1Char('\n'));
            for (const QString &line : lines) {
                const int hash = line.indexOf(QLatin1Char('#'));
                out << (hash < 0 ? line : line.left(hash));
            }
            return out.join(QLatin1Char('\n'));
        };

        // --- The shells own every .qrc in the project.
        const QString appCommon = code(read(repoRoot + QStringLiteral("/qmake/app_common.pri")));
        QVERIFY2(!appCommon.isEmpty(), "qmake/app_common.pri is missing or unreadable");
        for (const QString &qrc : {QStringLiteral("resrc.qrc"),
                                   QStringLiteral("ads.qrc"),
                                   QStringLiteral("qtpropertybrowser.qrc")}) {
            QVERIFY2(appCommon.contains(qrc),
                     qPrintable(QStringLiteral("qmake/app_common.pri no longer lists ")
                                + qrc
                                + QStringLiteral(" — a resource no executable registers "
                                                 "does not exist at runtime")));
        }

        // --- The library carries none, and keeps the line that takes back any a module
        // --- .pri added on its own (qtpropertybrowser_vendor.pri does exactly that).
        const QString srcPro = code(read(repoRoot + QStringLiteral("/src/src.pro")));
        QVERIFY2(!srcPro.isEmpty(), "src/src.pro is missing or unreadable");
        QVERIFY2(srcPro.contains(QStringLiteral("staticlib")),
                 "src/src.pro is no longer a static library — the resource split below "
                 "exists only because it is one");
        QVERIFY2(!srcPro.contains(QStringLiteral(".qrc")),
                 "src/src.pro references a .qrc; resources must stay at shell level");

        static const QRegularExpression resourcesReset(
            QStringLiteral("(?m)^\\s*RESOURCES\\s*=\\s*$"));
        QVERIFY2(resourcesReset.match(srcPro).hasMatch(),
                 "src/src.pro lost its bare `RESOURCES =` reset — without it an included "
                 ".pri can put a .qrc into the library, which is the silent failure this "
                 "whole split avoids");

        // --- No module .pri may add one either.
        const QStringList modules = {
            QStringLiteral("core"),   QStringLiteral("device"),
            QStringLiteral("calibration"), QStringLiteral("matching"),
            QStringLiteral("model"),  QStringLiteral("runtime"),
            QStringLiteral("ui")
        };
        for (const QString &module : modules) {
            const QString path = repoRoot + QStringLiteral("/src/") + module
                                 + QStringLiteral("/") + module + QStringLiteral(".pri");
            const QString text = code(read(path));
            QVERIFY2(!text.isEmpty(), qPrintable(path));
            QVERIFY2(!text.contains(QStringLiteral(".qrc")),
                     qPrintable(QStringLiteral("module .pri adds a resource, which the "
                                               "static library would then swallow: ")
                                + path));
        }

        // --- Both shells link the library instead of compiling the modules. A shell that
        // --- includes a module .pri again compiles all of src/ a second time, which is
        // --- the duplication E7a removed — and it links, so nothing else would notice.
        static const QRegularExpression moduleInclude(
            QStringLiteral("include\\s*\\([^)]*src/(core|device|calibration|matching|"
                           "model|runtime|ui)/\\1\\.pri"));
        for (const QString &pro : {QStringLiteral("/ncr_picking.pro"),
                                   QStringLiteral("/runtime_app/ncr_runtime.pro")}) {
            const QString text = code(read(repoRoot + pro));
            QVERIFY2(!text.isEmpty(), qPrintable(pro));
            QVERIFY2(!moduleInclude.match(text).hasMatch(),
                     qPrintable(QStringLiteral("shell .pro includes a module .pri directly "
                                               "instead of linking ncr_shared: ") + pro));
        }

        // The link itself is declared once, in the shared shell config.
        QVERIFY2(appCommon.contains(QStringLiteral("NCR_SHARED_LIB_NAME")),
                 "qmake/app_common.pri no longer links the shared library");
    }

    // ---- Module include-layering contract --------------------------------
    // Modules may only include same-or-lower dependency levels:
    //   level 0: core
    //   level 1: device, calibration, matching   (+ core)
    //   level 2: model, runtime                  (+ L0/L1; model<->runtime allowed)
    //   UI     : ui                              (everything except the shells)
    //   shells : app, runtime_app                (everything except each other)
    //
    // The two shells are PEERS over the same src/ modules, not a hierarchy: the
    // commissioning app and the operator runtime must not include each other's
    // headers, which is asserted separately below.
    // Scope: quoted includes whose first path segment is a known module name.
    // Same-directory includes (no slash), Qt/OpenCV angle includes, generated
    // ui_*.h, and third-party prefixes (qtpropertybrowser/...) are not module
    // references and are ignored. "../" escapes are always violations.
    void test_module_include_layering_contract()
    {
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);
        QVERIFY2(QDir(repoRoot).exists(),
                 qPrintable(QStringLiteral("repo root missing: ") + repoRoot));

        // A top-level folder missing from this list is not scanned at all — it would
        // pass the contract by being invisible rather than by being correct. Add every
        // new shell or module here at the same time it is created.
        const QStringList modules = {
            QStringLiteral("core"),   QStringLiteral("device"),
            QStringLiteral("calibration"), QStringLiteral("matching"),
            QStringLiteral("model"),  QStringLiteral("runtime"),
            QStringLiteral("ui"),     QStringLiteral("app"),
            QStringLiteral("runtime_app")
        };

        const QSet<QString> level01 = { QStringLiteral("core"),
                                        QStringLiteral("device"),
                                        QStringLiteral("calibration"),
                                        QStringLiteral("matching") };
        QSet<QString> level2 = level01;
        level2 |= { QStringLiteral("model"), QStringLiteral("runtime") };
        QSet<QString> ui = level2;
        ui |= { QStringLiteral("ui") };
        // Each shell may reach every module, but NOT the other shell.
        QSet<QString> appShell = ui;
        appShell |= { QStringLiteral("app") };
        QSet<QString> runtimeShell = ui;
        runtimeShell |= { QStringLiteral("runtime_app") };

        QHash<QString, QSet<QString>> allowed;
        allowed[QStringLiteral("core")] = { QStringLiteral("core") };
        // Sibling exception: camera devices own their Calibrator, so device
        // may use calibration (calibration never includes device back).
        allowed[QStringLiteral("device")] =
            { QStringLiteral("core"), QStringLiteral("device"),
              QStringLiteral("calibration") };
        allowed[QStringLiteral("calibration")] =
            { QStringLiteral("core"), QStringLiteral("calibration") };
        allowed[QStringLiteral("matching")] =
            { QStringLiteral("core"), QStringLiteral("matching") };
        allowed[QStringLiteral("model")] = level2;
        allowed[QStringLiteral("runtime")] = level2;
        allowed[QStringLiteral("ui")] = ui;
        allowed[QStringLiteral("app")] = appShell;
        allowed[QStringLiteral("runtime_app")] = runtimeShell;

        const QRegularExpression includeRe(
            QStringLiteral("^\\s*#\\s*include\\s*\"([^\"]+)\""));

        QStringList violations;
        int scannedFiles = 0;

        // Shells live at the repo root; every other module lives under src/.
        const QSet<QString> shellModules = { QStringLiteral("app"),
                                             QStringLiteral("runtime_app") };

        for (const QString &module : modules) {
            const QString dirPath = shellModules.contains(module)
                ? repoRoot + QStringLiteral("/") + module
                : repoRoot + QStringLiteral("/src/") + module;
            QVERIFY2(QDir(dirPath).exists(),
                     qPrintable(QStringLiteral("module directory missing (listed but not "
                                               "scanned — the contract would pass by "
                                               "being blind): ") + dirPath));
            QDirIterator it(dirPath,
                            { QStringLiteral("*.h"), QStringLiteral("*.cpp") },
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString filePath = it.next();

                // Skip qmake build output. Generated moc_*.cpp files reference their
                // sources with "../.." paths that are correct for the generator and
                // meaningless to this contract. Shells build next to their own .pro
                // (see build_and_verification.md), so their output lands inside the
                // scanned tree — src/ modules simply never had a build dir to trip on.
                if (filePath.contains(QStringLiteral("/build/"))) {
                    continue;
                }

                QFile file(filePath);
                QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
                         qPrintable(filePath));
                scannedFiles += 1;
                int lineNo = 0;
                while (!file.atEnd()) {
                    lineNo += 1;
                    const QString line = QString::fromUtf8(file.readLine());
                    const auto match = includeRe.match(line);
                    if (!match.hasMatch())
                        continue;
                    const QString inc = match.captured(1);
                    if (inc.startsWith(QLatin1String("../"))) {
                        violations << QStringLiteral("%1:%2: \"%3\" (no ../ escapes)")
                                          .arg(filePath).arg(lineNo).arg(inc);
                        continue;
                    }
                    if (inc.startsWith(QLatin1String("src/"))) {
                        violations << QStringLiteral(
                            "%1:%2: \"%3\" (module includes are rooted at src/ "
                            "— write core/..., not src/core/...)")
                            .arg(filePath).arg(lineNo).arg(inc);
                        continue;
                    }
                    const int slash = inc.indexOf(QLatin1Char('/'));
                    if (slash <= 0)
                        continue;
                    const QString target = inc.left(slash);
                    if (!modules.contains(target))
                        continue;
                    if (!allowed.value(module).contains(target)) {
                        violations << QStringLiteral(
                            "%1:%2: \"%3\" (%4 must not include %5)")
                            .arg(filePath).arg(lineNo).arg(inc)
                            .arg(module, target);
                    }
                }
            }
        }

        QVERIFY2(scannedFiles > 100,
                 qPrintable(QStringLiteral("scanned only %1 files — wrong repo root?")
                                .arg(scannedFiles)));
        QVERIFY2(violations.isEmpty(),
                 qPrintable(QStringLiteral("include-layering violations:\n")
                            + violations.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(ArchitectureContractTest)

#include <main.moc>
