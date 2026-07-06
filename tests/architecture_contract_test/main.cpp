#include <QtTest/QtTest>

#include <QDirIterator>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QScopedPointer>

#include <memory>

#include "device/camera/camera_basler_gige.h"
#include "device/device_capabilities.h"
#include "device/device_factory.h"
#include "device/device_registry.h"
#include "device/output_device/vision_tcpip_device.h"
#include "device/plc/mc_context_3e.h"
#include "device/plc/mc_protocol_device.h"
#include "device/robot/kawasaki_robot_device.h"
#include "device/robot/nachi_robot_device.h"
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
#include "widgets/vision/vision_result_adapter.h"

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

bool isMcTag(const QString &tag, QChar expectedPrefix)
{
    const QString trimmed = tag.trimmed();
    if (trimmed.size() < 2 ||
        trimmed.at(0).toUpper() != expectedPrefix.toUpper()) {
        return false;
    }

    bool ok = false;
    const int address = trimmed.mid(1).toInt(&ok, 10);
    return ok && address >= 0;
}

class FakeCameraDevice : public CameraDevice {
public:
    explicit FakeCameraDevice(const QString &id, const QString &name)
        : CameraDevice(id, name)
    {
        m_frame = cv::Mat(32, 32, CV_8UC1, cv::Scalar(128));
    }

    bool deviceConnect() override
    {
        setConnectionStatus(ConnectStatus::Connected);
        return true;
    }

    bool deviceDisconnect() override
    {
        setConnectionStatus(ConnectStatus::Disconnected);
        return true;
    }

    bool isDeviceConnected() const override
    {
        return connectStatus() == ConnectStatus::Connected;
    }

    void deviceTerminate() override {}
    CameraType cameraType() const override { return CameraType::BaslerGigE; }
    bool hasIOPort() const override { return false; }
    bool isRgbCamera() const override { return false; }
    void setGrabTimeout(int) override {}
    bool setExposure(double) override { return true; }
    bool setGain(double) override { return true; }
    void setBacklightControl(bool) override {}
    bool readIO(QString) override { return false; }
    bool writeIO(QString, bool) override { return false; }
    bool applyParametersChange() override
    {
        emit parametersApplied(true);
        return true;
    }

    GrabResult grabSingleShot() override
    {
        GrabResult result;
        result.isGrabSuccess = grabSucceeds;
        result.msg = grabSucceeds ? QStringLiteral("fake grab ok")
                                  : QStringLiteral("fake grab timeout");
        if (grabSucceeds) {
            result.frame = m_frame.clone();
        }
        emit grabFinished(result);
        return result;
    }

    bool startAutoContinuousShot() override { return true; }
    void stopAutoContinousShot() override {}
    bool startContinuousShot() override { return true; }
    void stopContinuousShot() override {}
    GrabResult softwareTriggerShot() override { return grabSingleShot(); }

    void setDeviceConfig(IDeviceCfg *cfg) override
    {
        if (auto *cameraCfg = dynamic_cast<CameraCfg *>(cfg)) {
            m_config.setCalibrator(cameraCfg->calibrator());
        }
        delete cfg;
        emit configChanged();
    }

    IDeviceCfg *deviceConfig() override
    {
        return new BaslerGigeCfg(m_config);
    }

    void setCalibrator(const calib::Calibrator &calibrator)
    {
        m_config.setCalibrator(calibrator);
    }

    void forceConnectionStatus(ConnectStatus status)
    {
        QMetaObject::invokeMethod(this, [this, status]() {
            setConnectionStatus(status);
        }, Qt::QueuedConnection);
    }

    bool grabSucceeds{true};

private:
    BaslerGigeCfg m_config;
    cv::Mat m_frame;
};

class FakePlcDevice : public PlcDevice, public IPlcIoWriter {
public:
    explicit FakePlcDevice(const QString &id, const QString &name)
        : PlcDevice(id, name)
    {
    }

    bool deviceConnect() override
    {
        setConnectionStatus(ConnectStatus::Connected);
        return true;
    }

    bool deviceDisconnect() override
    {
        setConnectionStatus(ConnectStatus::Disconnected);
        return true;
    }

    bool isDeviceConnected() const override
    {
        return connectStatus() == ConnectStatus::Connected;
    }

    void deviceTerminate() override {}
    PlcType plcType() const override { return PlcType::MitsubishiMc; }
    bool pushRequest(IRequest *) override { return false; }

    bool writeDigitalIoByName(const QString &tag, bool value) override
    {
        if (!isMcTag(tag, QLatin1Char('M'))) {
            return false;
        }
        digitalWrites.insert(tag, value);
        return true;
    }

    bool writeWordIoByName(const QString &tag, qint16 value) override
    {
        if (!isMcTag(tag, QLatin1Char('D'))) {
            return false;
        }
        wordWrites.insert(tag, value);
        return true;
    }

    QMap<QString, bool> digitalWrites;
    QMap<QString, qint16> wordWrites;
};

class FakeVisionOutputDevice : public VisionOutputDevice {
public:
    explicit FakeVisionOutputDevice(const QString &id, const QString &name)
        : VisionOutputDevice(id, name)
    {
    }

    bool deviceConnect() override
    {
        setConnectionStatus(ConnectStatus::Connected);
        return true;
    }

    bool deviceDisconnect() override
    {
        setConnectionStatus(ConnectStatus::Disconnected);
        return true;
    }

    bool isDeviceConnected() const override
    {
        return connectStatus() == ConnectStatus::Connected;
    }

    void deviceTerminate() override {}
    VisionOutputType visionOutputType() const override
    {
        return VisionOutputType::VisionTCPIP;
    }

    bool pushRequest(IRequest *request) override
    {
        auto *outputRequest = dynamic_cast<VisionOutputRequest *>(request);
        if (outputRequest) {
            capturedPositions = outputRequest->positions();
        }
        requestCount += 1;
        return sendSucceeds;
    }

    bool sendSucceeds{true};
    int requestCount{0};
    QVector<VisionOutputPosition> capturedPositions;
};

struct LocalizationRuntimeFixture {
    QScopedPointer<FakePlcDevice> plc;
    QScopedPointer<FakeCameraDevice> camera1;
    QScopedPointer<FakeCameraDevice> camera2;
    QScopedPointer<FakeVisionOutputDevice> visionOutput;
    QScopedPointer<PlcRunner> plcRunner;
    QScopedPointer<CameraRunner> cameraRunner1;
    QScopedPointer<CameraRunner> cameraRunner2;
    QScopedPointer<VisionOutputRunner> visionOutputRunner;
    LocalizationRuntimeController controller;

    LocalizationRuntimeFixture()
    {
        plc.reset(new FakePlcDevice(QStringLiteral("plc1"), QStringLiteral("PLC")));
        camera1.reset(new FakeCameraDevice(QStringLiteral("cam1"), QStringLiteral("Camera 1")));
        camera2.reset(new FakeCameraDevice(QStringLiteral("cam2"), QStringLiteral("Camera 2")));
        visionOutput.reset(new FakeVisionOutputDevice(QStringLiteral("vout1"),
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
        auto plc = std::make_shared<FakePlcDevice>(
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
        QCOMPARE(DeviceRegistry::displayNamesFor(DeviceType::Camera),
                 QStringList{CameraTypeToString(CameraType::BaslerGigE)});
        QCOMPARE(DeviceRegistry::displayNamesFor(DeviceType::PLC),
                 QStringList{PlcTypeToString(PlcType::MitsubishiMc)});
        QCOMPARE(DeviceRegistry::displayNamesFor(DeviceType::VisionOutput),
                 (QStringList{VisionOutputTypeToString(VisionOutputType::VisionTCPIP),
                              VisionOutputTypeToString(VisionOutputType::VisionTcpipClient)}));

        const DeviceRegistryEntry *cameraEntry =
            DeviceRegistry::find(DeviceType::Camera,
                                 CameraTypeToString(CameraType::BaslerGigE));
        QVERIFY(cameraEntry != nullptr);
        QCOMPARE(cameraEntry->configJsonKey, QStringLiteral(DEVICE_JSK_CAM_TYPE));
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

    void test_localization_recovery_policy_defaults_match_runtime_spec()
    {
        const LocalizationRecoveryPolicy cameraPolicy = defaultCameraRecoveryPolicy();
        QCOMPARE(cameraPolicy.roleName, QStringLiteral("camera"));
        QCOMPARE(cameraPolicy.maxRetries, 10);
        QCOMPARE(cameraPolicy.retryIntervalMs, 5000);
        QCOMPARE(cameraPolicy.connectTimeoutMs, 3000);

        const LocalizationRecoveryPolicy plcPolicy = defaultPlcRecoveryPolicy();
        QCOMPARE(plcPolicy.roleName, QStringLiteral("primary_plc"));
        QCOMPARE(plcPolicy.maxRetries, 10);
        QCOMPARE(plcPolicy.retryIntervalMs, 5000);

        const LocalizationRecoveryPolicy visionPolicy = defaultVisionOutputRecoveryPolicy();
        QCOMPARE(visionPolicy.roleName, QStringLiteral("vision_output"));
        QCOMPARE(visionPolicy.maxRetries, 10);
        QCOMPARE(visionPolicy.retryIntervalMs, 5000);
    }

    void test_localization_recovery_policy_decision_rules()
    {
        LocalizationRecoveryPolicy policy = defaultCameraRecoveryPolicy();
        policy.maxRetries = 2;

        QCOMPARE(decideRecoveryAction(policy,
                                      vc::device::ConnectStatus::Connected,
                                      0,
                                      false),
                 LocalizationRecoveryAction::Ignore);

        QCOMPARE(decideRecoveryAction(policy,
                                      vc::device::ConnectStatus::LostConnected,
                                      0,
                                      false),
                 LocalizationRecoveryAction::RetryScheduled);

        QCOMPARE(decideRecoveryAction(policy,
                                      vc::device::ConnectStatus::LostConnected,
                                      1,
                                      true),
                 LocalizationRecoveryAction::Ignore);

        QCOMPARE(decideRecoveryAction(policy,
                                      vc::device::ConnectStatus::ConnectFailed,
                                      2,
                                      false),
                 LocalizationRecoveryAction::EscalateFault);
    }

    void test_localization_recovery_fault_message_includes_role_and_retry_history()
    {
        LocalizationRecoveryPolicy policy = defaultVisionOutputRecoveryPolicy();
        policy.maxRetries = 10;
        policy.retryIntervalMs = 5000;
        policy.connectTimeoutMs = 3000;

        const QString message = buildRecoveryFaultMessage(
            policy,
            QStringLiteral("vout-01"),
            vc::device::ConnectStatus::ConnectFailed,
            10);

        QVERIFY(message.contains(QStringLiteral("role=vision_output")));
        QVERIFY(message.contains(QStringLiteral("deviceId=vout-01")));
        QVERIFY(message.contains(QStringLiteral("status=ConnectFailed")));
        QVERIFY(message.contains(QStringLiteral("retries=10/10")));
        QVERIFY(message.contains(QStringLiteral("intervalMs=5000")));
        QVERIFY(message.contains(QStringLiteral("timeoutMs=3000")));
    }

    void test_task_state_machine_transition_rules()
    {
        QVERIFY(canTransitionTaskState(TaskState::Idle, TaskState::CommissionStarting));
        QVERIFY(canTransitionTaskState(TaskState::Commission, TaskState::RuntimeStarting));
        QVERIFY(canTransitionTaskState(TaskState::Ready, TaskState::RunningCycle));
        QVERIFY(canTransitionTaskState(TaskState::Recovering, TaskState::Ready));
        QVERIFY(canTransitionTaskState(TaskState::Faulted, TaskState::Stopping));

        QVERIFY(!canTransitionTaskState(TaskState::Idle, TaskState::Ready));
        QVERIFY(!canTransitionTaskState(TaskState::Commission, TaskState::Recovering));
        QVERIFY(!canTransitionTaskState(TaskState::RunningCycle, TaskState::Commission));
        QVERIFY(!canTransitionTaskState(TaskState::Faulted, TaskState::Ready));
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
        QTRY_VERIFY_WITH_TIMEOUT(fixture.plc->digitalWrites.contains(QStringLiteral("M11")),
                                 1000);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M11")), true);
        QVERIFY(fixture.plc->digitalWrites.contains(QStringLiteral("M12")));
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M12")), false);
        QVERIFY(fixture.plc->digitalWrites.contains(QStringLiteral("M18")));
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M18")), false);
        QTRY_VERIFY_WITH_TIMEOUT(fixture.plc->wordWrites.contains(QStringLiteral("D102")),
                                 1000);
        QCOMPARE(fixture.plc->wordWrites.value(QStringLiteral("D102")), qint16(0));
        QVERIFY(fixture.plc->wordWrites.contains(QStringLiteral("D103")));
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
        QTRY_COMPARE_WITH_TIMEOUT(fixture.plc->digitalWrites.value(QStringLiteral("M12")),
                                  false,
                                  1000);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M13")), true);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M14")), true);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M18")), false);
        QCOMPARE(fixture.plc->wordWrites.value(QStringLiteral("D102")), qint16(1));
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

    // ---- Module include-layering contract --------------------------------
    // Modules may only include same-or-lower dependency levels:
    //   level 0: core
    //   level 1: device, calibration, matching   (+ core)
    //   level 2: model, runtime                  (+ L0/L1; model<->runtime allowed)
    //   UI     : form, widgets, libwg            (everything except app)
    //   app    : everything
    // Scope: quoted includes whose first path segment is a known module name.
    // Same-directory includes (no slash), Qt/OpenCV angle includes, generated
    // ui_*.h, and third-party prefixes (qtpropertybrowser/...) are not module
    // references and are ignored. "../" escapes are always violations.
    void test_module_include_layering_contract()
    {
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);
        QVERIFY2(QDir(repoRoot).exists(),
                 qPrintable(QStringLiteral("repo root missing: ") + repoRoot));

        const QStringList modules = {
            QStringLiteral("core"),   QStringLiteral("device"),
            QStringLiteral("calibration"), QStringLiteral("matching"),
            QStringLiteral("model"),  QStringLiteral("runtime"),
            QStringLiteral("form"),   QStringLiteral("widgets"),
            QStringLiteral("libwg"),  QStringLiteral("app")
        };

        const QSet<QString> level01 = { QStringLiteral("core"),
                                        QStringLiteral("device"),
                                        QStringLiteral("calibration"),
                                        QStringLiteral("matching") };
        QSet<QString> level2 = level01;
        level2 |= { QStringLiteral("model"), QStringLiteral("runtime") };
        QSet<QString> ui = level2;
        ui |= { QStringLiteral("form"), QStringLiteral("widgets"),
                QStringLiteral("libwg") };
        QSet<QString> all = ui;
        all |= { QStringLiteral("app") };

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
        allowed[QStringLiteral("form")] = ui;
        allowed[QStringLiteral("widgets")] = ui;
        allowed[QStringLiteral("libwg")] = ui;
        allowed[QStringLiteral("app")] = all;

        const QRegularExpression includeRe(
            QStringLiteral("^\\s*#\\s*include\\s*\"([^\"]+)\""));

        QStringList violations;
        int scannedFiles = 0;

        for (const QString &module : modules) {
            const QString dirPath = (module == QLatin1String("app"))
                ? repoRoot + QStringLiteral("/app")
                : repoRoot + QStringLiteral("/src/") + module;
            QDirIterator it(dirPath,
                            { QStringLiteral("*.h"), QStringLiteral("*.cpp") },
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString filePath = it.next();
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
