#include <QtTest/QtTest>

#ifdef Q_OS_WIN
// For the bare-name LoadLibraryW in the JAI GenICam delay-load check.
#include <windows.h>
#endif

#include <QCoreApplication>
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
#include "core/qgadget_macro.h"
#include "core/utils/translation_loader.h"
#include "device/camera/camera_basler_gige.h"
#include "device/camera/camera_jai_gige.h"
#include "device/camera/jai_runtime.h"
#include "device/device_capabilities.h"
#include "device/device_factory.h"
#include "device/device_registry.h"
#include "device/output_device/vision_tcpip_device.h"
#include "device/output_device/vision_tcpip_client_config.h"
#include "device/plc/mc_context.h"
#include "device/plc/mc_context_1c.h"
#include "device/plc/mc_context_3c.h"
#include "device/plc/mc_context_3e.h"
#include "device/plc/mc_context_factory.h"
#include "device/plc/mc_define.h"
#include "device/plc/mc_msg_interface.h"
#include "device/plc/mc_msg_serial_port.h"
#include "device/plc/mc_msg_tcp_client.h"
#include "device/plc/mc_protocol_device.h"
#include "device/plc/modbus/modbus_config.h"
#include "device/plc/modbus/modbus_register_map.h"
#include "device/plc/mc_device_map.h"
#include "device/plc/modbus/modbus_result_layout.h"
#include "device/plc/modbus/modbus_tcp_client_config.h"
#include "device/plc/modbus/modbus_tcp_client_device.h"
#include "device/plc/modbus/modbus_tcp_server_config.h"
#include "device/plc/modbus/modbus_tcp_server_device.h"
#include "device/robot/kawasaki_robot_device.h"
#include "device/robot/nachi_robot_device.h"
#include "device/virtual/virtual_device.h"
#include "device/virtual/virtual_camera_config.h"
#include "device/virtual/virtual_camera_device.h"
#include "device/virtual/virtual_plc_config.h"
#include "device/virtual/virtual_plc_device.h"
#include "device/virtual/virtual_vision_output_device.h"
#include "model/itask.h"
#include "model/task_localization_config.h"
#include "model/localization_signal_mapper.h"
#include "model/localization_recovery_policy.h"
#include "model/localization_fault_code.h"
#include "model/project.h"
#include "model/task_factory.h"
#include "model/task_localization.h"
#include "model/task_state_machine.h"
#include "runtime/device_command.h"
#include "runtime/camera_runner.h"
#include "runtime/plc_runner.h"
#include "runtime/task_runner.h"
#include "runtime/vision_output_runner.h"
#include "ui/widgets/vision/vision_result_adapter.h"
#include "ui/widgets/signals_map_widget.h"

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

QJsonObject modbusClientDeviceJson(const QString &id = QStringLiteral("mbclient1"))
{
    ModbusTcpClientCfg cfg;
    cfg.m_hostAddress = QStringLiteral("10.0.0.9");
    cfg.m_port = 5020;
    return baseDeviceJson(id,
                          QStringLiteral("Modbus TCP Client"),
                          DeviceType::PLC,
                          cfg.toJson());
}

QJsonObject modbusServerDeviceJson(const QString &id = QStringLiteral("mbserver1"))
{
    ModbusTcpServerCfg cfg;
    cfg.m_listenAddress = QStringLiteral("127.0.0.1");
    cfg.m_port = 15020;
    return baseDeviceJson(id,
                          QStringLiteral("Modbus TCP Server"),
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
        // Phase 9 / C3. D100/D101 are the master's command registers and the runtime must never
        // write them; D104/D105 are where it reports what it actually adopted.
        cfg.setnActiveCameraStatus(QStringLiteral("D104"));
        cfg.setnActivePatternGroupStatus(QStringLiteral("D105"));
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

/**
 * @brief A PLC that also carries the `vision_output` role — the mixed-family case.
 *
 * The real ones are the Modbus client and server, which publish result positions into their
 * register map. This stands in for them without a socket. It deliberately reports **nothing
 * commissioned** for the robot pick check, because that is the whole point of Phase 9 / F1: a
 * device with no check configured answers "disabled", which used to be indistinguishable from
 * "this cell does not want the check" and silently un-commissioned the gate.
 */
class ResultOutputPlcDevice : public VirtualPlcDevice, public vc::device::IResultOutputDevice {
public:
    using VirtualPlcDevice::VirtualPlcDevice;

    bool sendVisionResult(const QVector<VisionOutputPosition> &positions, QString *message) override
    {
        sentPositions = positions;
        if (message) *message = QStringLiteral("ok");
        return true;
    }

    RobotKinematicCheckConfig robotKinematicCheckConfig() const override
    {
        return RobotKinematicCheckConfig();  // nothing commissioned here; see the class note
    }

    QVector<VisionOutputPosition> sentPositions;
};

/**
 * @brief A whole TaskLocalization taken through beginRuntime() with nothing but virtual devices.
 *
 * LocalizationRuntimeFixture stops one layer short: it drives LocalizationRuntimeController
 * directly, so it never exercises TaskLocalization's own runtime entry point, the runner
 * registration inside it, or the controller -> task signal forwards at
 * `task_localization.cpp:593-606`. That gap is what both halves of backlog item 25 and the whole
 * of item 58 walked through unnoticed.
 *
 * Nothing here touches hardware or a GUI: a VirtualPlcDevice, two VirtualCameraDevices and a
 * VirtualVisionOutputDevice, registered in a real Project/DeviceManager and bound to a real task.
 */
struct TaskLocalizationRuntimeFixture {
    Project project;
    TaskLocalizationProbe *task{nullptr};   ///< Owned by `project` once addTask() succeeds.
    std::shared_ptr<VirtualPlcDevice> plc;
    std::shared_ptr<VirtualCameraDevice> camera1;
    std::shared_ptr<VirtualCameraDevice> camera2;
    std::shared_ptr<VirtualVisionOutputDevice> visionOutput;

    /// @param bindPrimaryPlc when false the primary-PLC role is left unbound, which is the
    ///        incomplete-bindings project that must end Faulted rather than half-running.
    /// @param visionOutputOnPlc when true the PLC is a ResultOutputPlcDevice and carries the
    ///        `vision_output` role as well as `primary_plc` — the dual-role Modbus cell, the
    ///        configuration item 57 is about. The VirtualVisionOutputDevice is still registered
    ///        and assigned, just not bound to the role.
    explicit TaskLocalizationRuntimeFixture(bool bindPrimaryPlc = true,
                                            bool visionOutputOnPlc = false)
    {
        // Ids must match the product's own format ("01", "02", …): DeviceManager::reserveDevice()
        // validates it and silently refuses anything else, which leaves the task with no
        // registered devices and a runtime that enters Commission with zero of them.
        plc = visionOutputOnPlc
                  ? std::static_pointer_cast<VirtualPlcDevice>(
                        std::make_shared<ResultOutputPlcDevice>(QStringLiteral("01"),
                                                                QStringLiteral("PLC")))
                  : std::make_shared<VirtualPlcDevice>(QStringLiteral("01"),
                                                       QStringLiteral("PLC"));
        camera1 = std::make_shared<VirtualCameraDevice>(QStringLiteral("02"),
                                                        QStringLiteral("Camera 1"));
        camera2 = std::make_shared<VirtualCameraDevice>(QStringLiteral("03"),
                                                        QStringLiteral("Camera 2"));
        visionOutput = std::make_shared<VirtualVisionOutputDevice>(
            QStringLiteral("04"), QStringLiteral("Vision Output"));

        const calib::Calibrator calibrator = LocalizationRuntimeFixture::makeCalibrator();
        camera1->setCalibrator(calibrator);
        camera2->setCalibrator(calibrator);

        auto dm = project.deviceManager();
        dm->reserveDevice(plc->id(), plc->name(), plc);
        dm->reserveDevice(camera1->id(), camera1->name(), camera1);
        dm->reserveDevice(camera2->id(), camera2->name(), camera2);
        dm->reserveDevice(visionOutput->id(), visionOutput->name(), visionOutput);

        task = new TaskLocalizationProbe(QStringLiteral("Runtime Task"));
        project.addTask(task);

        for (const auto &device : {std::static_pointer_cast<vc::device::IDevice>(plc),
                                   std::static_pointer_cast<vc::device::IDevice>(camera1),
                                   std::static_pointer_cast<vc::device::IDevice>(camera2),
                                   std::static_pointer_cast<vc::device::IDevice>(visionOutput)}) {
            task->assignDevice(device->id());
            device->setAssignedTaskId(task->id());
        }

        TaskLocalizeConfig cfg = LocalizationRuntimeFixture::config();
        if (bindPrimaryPlc) {
            cfg.d->m_deviceBindings.setPrimaryPlcDeviceId(plc->id());
        }
        cfg.d->m_deviceBindings.setVisionOutputDeviceId(visionOutputOnPlc ? plc->id()
                                                                         : visionOutput->id());
        cfg.d->m_deviceBindings.setCameraNumberMap({{1, camera1->id()}, {2, camera2->id()}});
        task->setTaskLocalizeConfig(cfg);

        addPatternGroup();
    }

    ~TaskLocalizationRuntimeFixture()
    {
        if (task) {
            task->endRuntime();
            task->stopAll();
        }
    }

    /// The task's own PlcRunner, resolved the way a widget has to resolve it: through the
    /// TaskRunner. TaskLocalization::plcRunner() is private, and reaching past that with a probe
    /// would test an access path no caller has.
    PlcRunner *plcRunner() const
    {
        auto *taskRunner = task ? task->taskRunner() : nullptr;
        return qobject_cast<PlcRunner *>(taskRunner ? taskRunner->runnerFor(plc->id()) : nullptr);
    }

    /// Parks a value in the virtual PLC **before** the runtime starts, the way a master holds a
    /// register across a restart. Called before beginCommission(), while the device still lives
    /// on this thread and no runner owns it.
    bool parkPlcInput(const QString &tag, const QVariant &value)
    {
        plc->deviceConnect();
        return plc->injectInputValue(tag, value);
    }

    /// Registers one usable pattern group so the runtime has something to validate against.
    void addPatternGroup()
    {
        mtc::MatchGroupConfig groupConfig;
        groupConfig.m_groupName = L"Group 1";
        groupConfig.m_groupIndex = 1;
        task->patternManager()->addGroup(groupConfig);

        auto group = task->patternManager()->findGroupByNumber(1);
        if (!group) {
            return;
        }
        mtc::MatchPatternConfig pattern;
        pattern.m_patternName = L"Pattern 1";
        pattern.m_patternIndex = 1;
        pattern.m_rawImage = cv::Mat(8, 8, CV_8UC1, cv::Scalar(255));
        group->addPattern(pattern);
    }
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

        // Phase 8 / B5. Both Modbus sub-types must come back as themselves, with the settings
        // the document carried. The registry is what the Add Device wizard lists, so a sub-type
        // that creates here is a sub-type an operator can actually make.
        std::unique_ptr<IDevice> modbusClient(
            DeviceFactory::fromJson(modbusClientDeviceJson()));
        QVERIFY(modbusClient != nullptr);
        QCOMPARE(modbusClient->deviceType(), DeviceType::PLC);
        auto *typedClient = qobject_cast<ModbusTcpClientDevice *>(modbusClient.get());
        QVERIFY(typedClient != nullptr);
        QCOMPARE(typedClient->plcType(), PlcType::ModbusTcpClient);
        QCOMPARE(typedClient->modbusConfig().m_hostAddress, QStringLiteral("10.0.0.9"));
        QCOMPARE(typedClient->modbusConfig().m_port, 5020);
        // The register map has to be rebuilt on load, or a restored device offers no tags to the
        // signal-map editor and the commissioned mapping cannot be re-edited.
        QVERIFY(!typedClient->availableDigitalIoNames().isEmpty());
        QVERIFY(!typedClient->availableWordIoNames().isEmpty());

        std::unique_ptr<IDevice> modbusServer(
            DeviceFactory::fromJson(modbusServerDeviceJson()));
        QVERIFY(modbusServer != nullptr);
        auto *typedServer = qobject_cast<ModbusTcpServerDevice *>(modbusServer.get());
        QVERIFY(typedServer != nullptr);
        QCOMPARE(typedServer->plcType(), PlcType::ModbusTcpServer);
        QCOMPARE(typedServer->modbusConfig().m_port, 15020);
        QVERIFY(!typedServer->availableWordIoNames().isEmpty());

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

    // The check that stands in for a crash. `PvDevice64.dll` and `PvGenICam64.dll` DELAY-LOAD
    // GenApi/GCBase from a folder the eBUS installer leaves off PATH, so the first JAI connect
    // raised an SEH delay-load exception (0xC06D007E) and killed the process — no C++ exception
    // to catch, nothing written to the log. Camera discovery kept working throughout, because
    // PvSystem64.dll delay-loads nothing, which is what disguised it as a bug in connect.
    //
    // This asserts the runtime is resolvable on a machine that can build at all. It cannot open
    // a camera, but it fails for the same reason the application did, and it fails with a
    // message instead of a dead process.
    void test_jai_genicam_delay_load_runtime_is_resolvable()
    {
        QString detail;
        const bool ready = vc::device::jai::ensureGenICamRuntime(&detail);
        QVERIFY2(ready, qPrintable(QStringLiteral(
            "The eBUS GenICam runtime is not loadable, so any JAI camera connect would take the "
            "whole application down: %1").arg(detail)));
        QVERIFY2(detail.isEmpty(), "a successful runtime check must report no failure detail");

        // Idempotent: the device calls it on every connect and the select dialog on every scan.
        QString second;
        QVERIFY(vc::device::jai::ensureGenICamRuntime(&second));
        QVERIFY(second.isEmpty());

#ifdef Q_OS_WIN
        // The decisive part: load GenApi by BARE NAME, which is exactly what the delay-loader
        // does. Loading it by full path would prove nothing — that works whatever PATH says, and
        // it is the by-name lookup that was failing. This line fails on an unfixed build.
        const QString genicamDir =
            QDir::cleanPath(qEnvironmentVariable("CommonProgramFiles")
                            + QStringLiteral("/Pleora/eBUS SDK/GenICam/bin/Win64_x64"));
        const QStringList genApi =
            QDir(genicamDir).entryList({QStringLiteral("GenApi_MD_*.dll")}, QDir::Files);
        if (genApi.isEmpty()) {
            // A non-default install; ensureGenICamRuntime() found it elsewhere and already
            // said so. Nothing to add rather than a misleading failure.
            QSKIP("eBUS GenICam runtime is not at the default location; "
                  "the resolvability check above still applies.");
        }
        const QString bareName = genApi.first();
        QVERIFY2(LoadLibraryW(reinterpret_cast<const wchar_t *>(bareName.utf16())) != nullptr,
                 qPrintable(QStringLiteral(
                     "%1 does not resolve by bare name, so the delay-load inside PvDevice64.dll "
                     "would raise 0xC06D007E and kill the process on the first JAI connect")
                                .arg(bareName)));
#endif
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

        JaiGigeCfg jaiCfg;
        jaiCfg.m_modelName = QStringLiteral("JaiModel");
        jaiCfg.m_serialNumber = QStringLiteral("JaiSerial");
        jaiCfg.m_ipAddress = QStringLiteral("10.0.0.3");
        jaiCfg.m_pixelFormat = QStringLiteral("BayerRG8");
        jaiCfg.m_isColor = true;
        jaiCfg.m_autoExposureMode = vc::device::jai::JaiExposureMode::Exposure_Continuous;
        jaiCfg.m_paramsGain = 3.5;                 // fractional on purpose: SFNC Gain is a float
        jaiCfg.m_autoBacklightLine = QStringLiteral("Line2");
        jaiCfg.m_autoBacklightInvert = true;
        jaiCfg.m_grabTimeoutMs = 900;
        vc::device::jai::JaiIOLine line;
        line.name = QStringLiteral("Line2");
        line.can_be_output = true;
        jaiCfg.m_ioCapabilities.append(line);

        JaiGigeCfg jaiRestored;
        QVERIFY(jaiRestored.fromJson(jaiCfg.toJson()));
        QCOMPARE(jaiRestored.cameraType(), CameraType::JaiGigE);
        QCOMPARE(jaiRestored.m_ipAddress, jaiCfg.m_ipAddress);
        QCOMPARE(jaiRestored.m_pixelFormat, jaiCfg.m_pixelFormat);
        QCOMPARE(jaiRestored.m_isColor, true);
        QCOMPARE(jaiRestored.m_autoExposureMode,
                 vc::device::jai::JaiExposureMode::Exposure_Continuous);
        // Not truncated on the way through JSON: the config member is a double even though the
        // CameraCfg::gain() contract returns int.
        QCOMPARE(jaiRestored.m_paramsGain, 3.5);
        QCOMPARE(jaiRestored.m_autoBacklightInvert, true);
        QCOMPARE(jaiRestored.m_grabTimeoutMs, 900);
        QCOMPARE(jaiRestored.m_ioCapabilities.size(), 1);
        QCOMPARE(jaiRestored.m_ioCapabilities.at(0).name, QStringLiteral("Line2"));

        // A project saved before the grab-timeout key existed must load a usable timeout, not 0 —
        // a zero timeout makes every grab fail instantly, with no clue that a JSON key is why.
        QJsonObject legacy = jaiCfg.toJson();
        legacy.remove(QStringLiteral("GrabTimeoutMs"));
        JaiGigeCfg jaiLegacy;
        QVERIFY(jaiLegacy.fromJson(legacy));
        QCOMPARE(jaiLegacy.m_grabTimeoutMs, JaiGigECamera::kDefaultGrabTimeoutMs);

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
        // 3 → 4 in Phase 9 / F1, which moved the robot pick check onto the task.
        QCOMPARE(TaskLocalizeConfig::kSchemaVersion, 4);

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

        // A v2 document predates the two status tags. It must load with both unbound — the
        // migration C3 promises, and the state every commissioned project is in until its
        // owner maps them.
        QJsonObject v2 = cfg.toJson();
        v2["version"] = 2;
        v2.remove(QStringLiteral("nActiveCameraStatus"));
        v2.remove(QStringLiteral("nActivePatternGroupStatus"));

        TaskLocalizeConfig fromV2;
        QVERIFY(fromV2.fromJson(v2));
        QVERIFY(fromV2.nActiveCameraStatus().isEmpty());
        QVERIFY(fromV2.nActivePatternGroupStatus().isEmpty());
        QCOMPARE(fromV2.bErrorReset(), QStringLiteral("M120"));

        // The gate still refuses a document from a newer build.
        QJsonObject tooNewDoc = cfg.toJson();
        tooNewDoc["version"] = TaskLocalizeConfig::kSchemaVersion + 1;
        TaskLocalizeConfig tooNew;
        QVERIFY(!tooNew.fromJson(tooNewDoc));
    }

    // Phase 9 / F1, backlog item 57. The robot pick check moved from the bound output device onto
    // the task, so it now has to survive a save/load like every other task setting — including
    // the nested pick path, which is the half a naive "copy the scalars" migration drops.
    void test_robot_pick_check_round_trips_and_v3_documents_still_load()
    {
        TaskLocalizeConfig cfg;
        RobotKinematicCheckConfig check;
        check.enabled = true;
        check.collisionCheckEnabled = true;
        check.presetName = QStringLiteral("Nachi MZ04D");
        check.tcpZ = 120.0;
        PickPathPoint approach;
        approach.dz = -50.0;
        approach.absZ = true;
        approach.shoulder = QStringLiteral("lefty");
        check.pickPath.append(approach);
        cfg.setRobotCheckConfig(check);

        TaskLocalizeConfig restored;
        QVERIFY(restored.fromJson(cfg.toJson()));
        const RobotKinematicCheckConfig r = restored.robotCheckConfig();
        QCOMPARE(r.enabled, true);
        QCOMPARE(r.collisionCheckEnabled, true);
        QCOMPARE(r.presetName, QStringLiteral("Nachi MZ04D"));
        QCOMPARE(r.tcpZ, 120.0);
        // The nested path, asserted field by field: a migration that carries the scalars and
        // drops the waypoints leaves a check that passes everything, which is worse than one
        // that is off — it looks commissioned.
        QCOMPARE(r.pickPath.size(), 1);
        QCOMPARE(r.pickPath.at(0).dz, -50.0);
        QCOMPARE(r.pickPath.at(0).absZ, true);
        QCOMPARE(r.pickPath.at(0).shoulder, QStringLiteral("lefty"));

        // A v3 document has no "robotCheckConfig" key. It must load with the check DISABLED —
        // which is what a v3-era build produced for every project whose output role was not a
        // vision-output device, so this is not a behaviour change for them.
        QJsonObject v3 = cfg.toJson();
        v3["version"] = 3;
        v3.remove(QStringLiteral("robotCheckConfig"));

        TaskLocalizeConfig fromV3;
        QVERIFY(fromV3.fromJson(v3));
        QCOMPARE(fromV3.robotCheckConfig().enabled, false);
        QVERIFY(fromV3.robotCheckConfig().pickPath.isEmpty());

        // And a v3-era build must refuse a v4 document rather than load it and go back to
        // reading the check off the device. Stated as the gate's rule, since this build cannot
        // literally be an older one: kSchemaVersion 3 refuses a document declaring 4.
        QJsonObject v4 = cfg.toJson();
        QCOMPARE(v4.value(QStringLiteral("version")).toInt(), 4);
        v4["version"] = TaskLocalizeConfig::kSchemaVersion + 1;
        TaskLocalizeConfig tooNew;
        QVERIFY(!tooNew.fromJson(v4));
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
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::CameraNotRegistered), 103);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::VisionOutputLost), 200);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::VisionOutputSendFailed), 201);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::PlcLost), 300);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::PlcWriteFailed), 301);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::PatternNotRegistered), 400);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::CalibrationInvalid), 401);
        QCOMPARE(localizationFaultCodeValue(LocalizationFaultCode::InternalError), 500);
    }

    /// Every declared code must have a case in localizationFaultCodeName(). The switch has no
    /// default, so a code added to the enum and forgotten here compiles clean and reaches the
    /// dashboard as the literal text "Unknown" — which is what happened to CameraNotRegistered
    /// between its declaration and this test. The dashboard's fault panel calls this function
    /// directly (localization_dashboard_widget.cpp faultCodeText), so a missing case is a UI
    /// defect, not only a logging one.
    void test_every_localization_fault_code_has_a_name()
    {
        const QVector<LocalizationFaultCode> all = {
            LocalizationFaultCode::None,
            LocalizationFaultCode::CameraLost,
            LocalizationFaultCode::CameraConnectFailed,
            LocalizationFaultCode::CameraGrabTimeout,
            LocalizationFaultCode::CameraNotRegistered,
            LocalizationFaultCode::VisionOutputLost,
            LocalizationFaultCode::VisionOutputSendFailed,
            LocalizationFaultCode::PlcLost,
            LocalizationFaultCode::PlcWriteFailed,
            LocalizationFaultCode::PatternNotRegistered,
            LocalizationFaultCode::CalibrationInvalid,
            LocalizationFaultCode::InternalError,
        };
        for (const LocalizationFaultCode code : all) {
            QVERIFY2(localizationFaultCodeName(code) != QStringLiteral("Unknown"),
                     qPrintable(QStringLiteral("fault code %1 has no name mapping")
                                    .arg(localizationFaultCodeValue(code))));
        }

        // Spot-check the two names this phase changed, so a rename cannot silently pass the
        // "not Unknown" check above by mapping to some other code's string.
        QCOMPARE(localizationFaultCodeName(LocalizationFaultCode::CameraNotRegistered),
                 QStringLiteral("CameraNotRegistered"));
        QCOMPARE(localizationFaultCodeName(LocalizationFaultCode::PatternNotRegistered),
                 QStringLiteral("PatternNotRegistered"));
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

    // Phase 8 / B1. The localization task's `vision_output` role is filled by capability, not by
    // device family. A runner that reports supportsResultOutput() must actually carry a send
    // through to its device — claiming the capability and leaving the base-class default in place
    // would leave the controller connected to resultRequestFinished() and waiting, which stalls a
    // cycle instead of faulting it.
    //
    // Proven to fail before being trusted: with VisionOutputRunner::requestSendResult() removed so
    // the base default runs, this case fails on the ok flag (the default refuses the request), and
    // with supportsResultOutput() left at its default the first QVERIFY fails.
    void test_runner_that_claims_result_output_actually_delivers_it()
    {
        VirtualVisionOutputDevice device(QStringLiteral("cap_send_vout"),
                                         QStringLiteral("Result Output"));
        VisionOutputRunner runner(&device);
        runner.start();
        runner.attach();

        QVERIFY(runner.supportsResultOutput());

        QSignalSpy finishedSpy(&runner, &IDeviceRunner::resultRequestFinished);
        QVERIFY(finishedSpy.isValid());

        VisionOutputPosition position;
        position.x = 12.5;
        position.y = -3.25;
        position.rz = 45.0;
        runner.requestSendResult({position});

        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 1000);
        QCOMPARE(finishedSpy.at(0).at(0).toBool(), true);
        QCOMPARE(device.requestCount, 1);
        QCOMPARE(device.capturedPositions.size(), 1);
        QCOMPARE(device.capturedPositions.at(0).x, 12.5);

        // The other half of the contract: a runner that does not claim the capability must refuse
        // the request out loud. Silence here is the failure mode that hangs a cycle.
        VirtualPlcDevice plcDevice(QStringLiteral("cap_send_plc"), QStringLiteral("PLC"));
        PlcRunner plcRunner(&plcDevice);
        QVERIFY(!plcRunner.supportsResultOutput());

        QSignalSpy refusedSpy(&plcRunner, &IDeviceRunner::resultRequestFinished);
        QVERIFY(refusedSpy.isValid());
        plcRunner.requestSendResult({position});
        QCOMPARE(refusedSpy.count(), 1);
        QCOMPARE(refusedSpy.at(0).at(0).toBool(), false);

        runner.detach(nullptr);
        runner.stop();
    }

    // ── Phase 9 / E4: the handshake write policy (decision D3) ───────────────────────────
    //
    // The five signals the PLC's own program blocks on are retried and, on exhaustion, abort the
    // cycle with 301. Everything else stays log-only: retrying all fourteen would turn a degraded
    // link into a write storm. The device stays CONNECTED throughout — a disconnected role is the
    // recovery policy's business and already reports PlcLost.

    /// A write that fails once and succeeds on the retry must leave no trace: no fault, no abort.
    /// The budget exists to absorb exactly this.
    ///
    /// Driven through the real ready path rather than a test-only publish hook: markRuntimeReady()
    /// → publishInitialReadyOutputs() writes bTaskReady, nDetectedNumber and nFaultCode — three of
    /// the five tracked signals — and by then the PLC role is Connected, which is the condition
    /// tracking requires.
    void test_a_handshake_write_that_succeeds_on_retry_raises_no_fault()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy faultSpy(&fixture.controller, &LocalizationRuntimeController::runtimeFault);
        QVERIFY(faultSpy.isValid());

        // Refuse the first bTaskReady write, once. One failure, one retry, then through.
        fixture.plc->failWritesForTag.insert(QStringLiteral("M11"), 1);

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));

        QTRY_VERIFY_WITH_TIMEOUT(
            fixture.plc->digitalWrites.value(QStringLiteral("M11"), false), 4000);
        QCOMPARE(faultSpy.count(), 0);
    }

    /// Exhausting the budget aborts the cycle with 301 and names the signal and the tag. This is
    /// the whole point of D3: a handshake the PLC is waiting on that never arrived must become a
    /// fault the PLC can read, not a silence it waits out.
    void test_a_handshake_write_retried_to_exhaustion_aborts_with_301()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy logSpy(&fixture.controller, &LocalizationRuntimeController::taskLogAppended);
        QVERIFY(signalSpy.isValid());

        fixture.plc->failWritesForTag.insert(QStringLiteral("M11"), -1);   // refuse forever

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));

        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(),
            localizationFaultCodeValue(LocalizationFaultCode::PlcWriteFailed), 5000);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);

        QString detail;
        for (const QList<QVariant> &emission : logSpy) {
            const auto entry = qvariant_cast<LocalizationRuntimeController::TaskLogEntry>(
                emission.at(0));
            if (entry.severity == QStringLiteral("ERROR")
                && entry.message.contains(QStringLiteral("bTaskReady"))) {
                detail = entry.message;
                break;
            }
        }
        QVERIFY2(!detail.isEmpty(),
                 "the abort must log an ERROR naming the signal that could not be written");
        QVERIFY2(detail.contains(QStringLiteral("M11")),
                 qPrintable(QStringLiteral("the log must name the tag too; got: %1").arg(detail)));
    }

    /// An advisory signal is log-only. It must not retry and must not abort — the distinction
    /// between the five and the rest is the whole reason the policy is bounded.
    void test_an_advisory_write_failure_does_not_retry_and_does_not_abort()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy faultSpy(&fixture.controller, &LocalizationRuntimeController::runtimeFault);
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);

        // bMatchingLowArea (M15) is advisory and is published by the ready path. Refuse it forever.
        fixture.plc->failWritesForTag.insert(QStringLiteral("M15"), -1);

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_VERIFY_WITH_TIMEOUT(
            fixture.plc->digitalWrites.value(QStringLiteral("M11"), false), 3000);
        QTest::qWait(500);   // longer than kPlcWriteRetryBudget * kPlcWriteRetryDelayMs

        QCOMPARE(faultSpy.count(), 0);
        QVERIFY2(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt()
                     != localizationFaultCodeValue(LocalizationFaultCode::PlcWriteFailed),
                 "an advisory write failure must not raise 301");
        // Never retried, so the device saw exactly one attempt and it stayed refused.
        QVERIFY2(!fixture.plc->digitalWrites.contains(QStringLiteral("M15")),
                 "the advisory write was refused and must not have been re-issued");
    }

    /// The recursion trap, asserted rather than assumed. abortCycle() publishes bTaskFault and
    /// nFaultCode — two of the five tracked signals — over the very link that just refused a
    /// write. Without the guard those publishes are tracked, fail, retry and escalate again,
    /// forever. The test must FAIL rather than hang if the guard is removed, so it asserts a
    /// bounded abort count.
    void test_a_link_that_refuses_everything_escalates_once_and_stops()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy faultSpy(&fixture.controller, &LocalizationRuntimeController::runtimeFault);
        QVERIFY(faultSpy.isValid());

        // Every handshake tag refused forever, including the two the abort itself publishes.
        for (const QString &tag : {QStringLiteral("M11"), QStringLiteral("M13"),
                                   QStringLiteral("M18"), QStringLiteral("D102"),
                                   QStringLiteral("D103")}) {
            fixture.plc->failWritesForTag.insert(tag, -1);
        }

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_VERIFY_WITH_TIMEOUT(faultSpy.count() >= 1, 5000);

        // Bounded, not merely "eventually stops": the escalation is attempted ONCE. A handful of
        // extra faults from unrelated paths would be tolerable; an unbounded loop would not, and
        // this is the assertion that tells them apart without hanging the suite.
        QTest::qWait(1000);
        QVERIFY2(faultSpy.count() <= 3,
                 qPrintable(QStringLiteral("escalation must not re-enter itself; got %1 faults")
                                .arg(faultSpy.count())));
    }

    // ── Phase 9 / E3: PlcRunner carries the write result ─────────────────────────────────
    //
    // requestWriteDigitalIo() was void, and a failure surfaced only as errorOccurred() with no
    // way to tell WHICH write failed. D3's retry policy needs the opposite: an id handed to the
    // caller before the write is even queued, and exactly one completion carrying it.

    /// Parameterised over the families whose writes are synchronous, because the point is that
    /// one contract covers them without each restating it. The MC family — the asynchronous one
    /// — is covered at device level by mc_frame_test's seven E1 cases, which drive its real
    /// request/response state machine; reproducing that transport stub here would duplicate it
    /// to prove the same thing twice.
    void test_a_plc_write_resolves_on_every_family()
    {
        struct Family {
            QString label;
            std::function<vc::device::PlcDevice *()> make;
            QString goodTag;
            QString badTag;
        };

        VirtualPlcDevice virtualDevice(QStringLiteral("e3_virtual"), QStringLiteral("Virtual"));
        ModbusTcpServerDevice serverDevice(QStringLiteral("e3_server"), QStringLiteral("Server"));
        ModbusTcpServerCfg serverCfg;
        serverCfg.m_listenAddress = QStringLiteral("127.0.0.1");
        serverCfg.m_port = 25391;
        serverDevice.setDeviceConfig(&serverCfg);

        const QVector<Family> families = {
            {QStringLiteral("virtual"), [&]() -> vc::device::PlcDevice * { return &virtualDevice; },
             QStringLiteral("M0"), QStringLiteral("NOTATAG")},
            {QStringLiteral("modbus-server"), [&]() -> vc::device::PlcDevice * { return &serverDevice; },
             QStringLiteral("COIL00000"), QStringLiteral("NOTATAG")},
        };

        for (const Family &family : families) {
            vc::device::PlcDevice *device = family.make();
            QVERIFY2(device->deviceConnect(), qPrintable(family.label));

            PlcRunner runner(device);
            runner.start();
            runner.attach();

            QSignalSpy finished(&runner, &PlcRunner::writeFinished);
            QVERIFY(finished.isValid());

            const quint64 okId = runner.requestWriteDigitalIo(family.goodTag, true);
            const quint64 badId = runner.requestWriteDigitalIo(family.badTag, true);
            QVERIFY2(okId != 0 && badId != 0 && okId != badId,
                     qPrintable(QStringLiteral("%1: ids must be distinct and never 0")
                                    .arg(family.label)));

            // Named, not QTRY_COMPARE: the parameterisation is only worth having if a failure
            // says WHICH family broke, and QTRY_COMPARE's message cannot carry the label. Proven
            // by the negative check — with the Modbus server's completion suppressed, the bare
            // count assertion failed with "Actual (finished.count()): 0" and nothing else.
            const bool bothResolved =
                QTest::qWaitFor([&]() { return finished.count() == 2; }, 3000);
            QVERIFY2(bothResolved,
                     qPrintable(QStringLiteral("%1: both writes must resolve; got %2 of 2")
                                    .arg(family.label).arg(finished.count())));

            QHash<quint64, bool> okById;
            for (const QList<QVariant> &emission : finished) {
                okById.insert(emission.at(0).toULongLong(), emission.at(1).toBool());
            }
            QVERIFY2(okById.value(okId, false),
                     qPrintable(QStringLiteral("%1: a good write must resolve ok")
                                    .arg(family.label)));
            QVERIFY2(okById.contains(badId) && !okById.value(badId),
                     qPrintable(QStringLiteral("%1: a rejected write must resolve FAILED, not "
                                               "vanish").arg(family.label)));

            runner.detach(nullptr);
            runner.stop();
            device->deviceDisconnect();
        }
    }

    /// A PLC device that implements no writer at all must resolve as failed, never silently.
    /// This is the case the old void signature could not express: the runner emitted
    /// errorOccurred() and the caller, having no id, could not tell which write it belonged to.
    void test_a_write_to_a_plc_with_no_writer_resolves_as_failed()
    {
        /// Minimal PlcDevice that deliberately does NOT implement IPlcIoWriter. All four shipped
        /// families do, so the case is unreachable without one.
        class WriterlessPlc : public vc::device::PlcDevice {
        public:
            WriterlessPlc() : PlcDevice(QStringLiteral("e3_nowriter"), QStringLiteral("No Writer")) {}
            bool deviceConnect() override {
                setConnectionStatus(vc::device::ConnectStatus::Connected);
                return true;
            }
            bool deviceDisconnect() override {
                setConnectionStatus(vc::device::ConnectStatus::Disconnected);
                return true;
            }
            bool isDeviceConnected() const override {
                return connectStatus() == vc::device::ConnectStatus::Connected;
            }
            vc::device::PlcType plcType() const override {
                return vc::device::PlcType::VirtualPlc;
            }
            bool pushRequest(vc::device::IRequest *) override { return false; }
            void deviceTerminate() override { deviceDisconnect(); }
        };

        WriterlessPlc device;
        QVERIFY(device.deviceConnect());

        PlcRunner runner(&device);
        runner.start();
        runner.attach();

        QSignalSpy finished(&runner, &PlcRunner::writeFinished);
        QSignalSpy errors(&runner, &IDeviceRunner::errorOccurred);
        QVERIFY(finished.isValid());

        const quint64 id = runner.requestWriteWordIo(QStringLiteral("D100"), 5);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 2000);
        QCOMPARE(finished.at(0).at(0).toULongLong(), id);
        QCOMPARE(finished.at(0).at(1).toBool(), false);
        QVERIFY2(!finished.at(0).at(2).toString().isEmpty(), "a failure must say why");

        // errorOccurred() is preserved, not replaced: the device panels and the recovery policy
        // still listen to it, and a failed write must stay visible to a consumer that has not
        // been taught about writeFinished().
        QCOMPARE(errors.count(), 1);

        runner.detach(nullptr);
        runner.stop();
    }

    // ── Phase 9 / D1: the runner must not be wedged by a redundant connect ────────────────
    //
    // Asserted at RUNNER level because that is where the damage was. m_busy is set by
    // requestConnect() and cleared ONLY by connectStatusChanged or connectionFailed. A device
    // whose deviceConnect() returned true in silence therefore left m_busy true forever: the
    // second requestConnect() and every retry scheduleRoleReconnect() queued afterwards became
    // a no-op, and the task parked in Recovering with bTaskReady=false AND bTaskFault=false.
    //
    // A real VisionTcpipDevice, not the virtual one: the virtual device publishes
    // unconditionally and so can never reproduce this. m_busy is private with no accessor, so
    // the assertion is the observable consequence — one status per request, or none at all.
    void test_vision_output_runner_is_not_wedged_by_a_redundant_connect()
    {
        VisionTcpipDevice device(QStringLiteral("d1_wedge"), QStringLiteral("Wedge Server"));
        VisionTcpipDeviceCfg cfg;
        cfg.m_listenAddress       = QStringLiteral("127.0.0.1");
        cfg.m_mainPort            = 25171;
        cfg.m_heartbeatPort       = 25172;
        cfg.m_heartbeatIntervalMs = 200;
        cfg.m_heartbeatTimeoutMs  = 600;
        device.setVisionTcpipConfig(cfg);

        VisionOutputRunner runner(&device);
        runner.start();
        runner.attach();

        QSignalSpy statusSpy(&runner, &IDeviceRunner::connectStatusChanged);
        QVERIFY(statusSpy.isValid());

        runner.requestConnect();
        QTRY_COMPARE_WITH_TIMEOUT(statusSpy.count(), 1, 2000);
        QCOMPARE(statusSpy.last().at(0).value<vc::device::ConnectStatus>(),
                 vc::device::ConnectStatus::Connected);

        // The redundant one. This is the call that used to disappear: the device was already
        // active, so it returned true without publishing, and m_busy never came back down.
        runner.requestConnect();
        QTRY_COMPARE_WITH_TIMEOUT(statusSpy.count(), 2, 2000);
        QCOMPARE(statusSpy.last().at(0).value<vc::device::ConnectStatus>(),
                 vc::device::ConnectStatus::Connected);

        // And the runner is still live afterwards — the point is that it can be driven again,
        // not merely that one extra signal arrived.
        runner.requestConnect();
        QTRY_COMPARE_WITH_TIMEOUT(statusSpy.count(), 3, 2000);

        runner.requestDisconnect();
        QTRY_COMPARE_WITH_TIMEOUT(
            statusSpy.last().at(0).value<vc::device::ConnectStatus>(),
            vc::device::ConnectStatus::Disconnected, 2000);

        runner.detach(nullptr);
        runner.stop();
    }

    // Phase 8 / Checkpoint B. One Modbus device filling BOTH the primary_plc and vision_output
    // roles is a supported configuration, and it did not work: the owner's task faulted at setup
    // with "Device bound to vision_output cannot output results".
    //
    // Nothing was missing from the device or the task. ModbusTcpServerDevice implements
    // IResultOutputDevice, and buildRuntimeContext() resolved and passed its runner correctly.
    // The capability was simply declared on the DEVICE and only ever read off the RUNNER, and
    // PlcRunner — unlike VisionOutputRunner — never forwarded the question.
    //
    // The answer has to be per-device, not per-family. The PLC family is mixed: Modbus
    // client/server publish results into their register map, McProtocolDevice does not. A flat
    // `true` would let an MC PLC be accepted for the role and then hang the first cycle waiting
    // for a send that never comes — the exact failure IDeviceRunner::supportsResultOutput() warns
    // about, and worse than the bug being fixed here.
    void test_plc_runner_reports_result_output_per_device_not_per_family()
    {
        // Capable: the Modbus server publishes results into its own register space.
        ModbusTcpServerDevice modbusServer(QStringLiteral("cap_modbus_srv"),
                                           QStringLiteral("Modbus Server"));
        PlcRunner serverRunner(&modbusServer);
        QVERIFY2(serverRunner.supportsResultOutput(),
                 "a Modbus server bound to vision_output must be accepted at setup");

        // Incapable: same family, same runner class, no IResultOutputDevice.
        McProtocolDevice mcPlc(QStringLiteral("cap_mc_plc"), QStringLiteral("MC PLC"));
        PlcRunner mcRunner(&mcPlc);
        QVERIFY2(!mcRunner.supportsResultOutput(),
                 "an MC PLC must NOT be accepted for vision_output; the cycle would hang");

        // And the capable one must actually answer a send request rather than going silent —
        // the controller waits on this signal, so silence stalls a cycle instead of failing it.
        // The device is not listening here, so the send legitimately fails; what is under test is
        // that exactly one outcome arrives, with a reason.
        serverRunner.start();
        serverRunner.attach();

        QSignalSpy answeredSpy(&serverRunner, &IDeviceRunner::resultRequestFinished);
        QVERIFY(answeredSpy.isValid());

        VisionOutputPosition position;
        position.x = 1.0;
        position.y = 2.0;
        position.rz = 3.0;
        serverRunner.requestSendResult({position});

        QTRY_COMPARE_WITH_TIMEOUT(answeredSpy.count(), 1, 2000);
        QVERIFY2(!answeredSpy.at(0).at(1).toString().isEmpty(),
                 "a refused send must carry a reason, not an empty message");

        serverRunner.detach(nullptr);
        serverRunner.stop();
    }

    // ── Backlog 43: driving a hardware-free runtime ───────────────────────────
    // A virtual PLC could record what the runtime WROTE but had no way to drive a value IN. Every
    // runtime state past Ready begins with the PLC changing an input (bExecuteTrigger, bErrorReset,
    // nActiveCamera, nActivePatternGroup), so a hardware-free project could be built, opened,
    // configured and taken to Ready — and then nothing. Every transition the task state machine has
    // was unreachable, including the fault paths that matter most.

    void test_virtual_plc_publishes_an_injected_input()
    {
        VirtualPlcDevice plc(QStringLiteral("inj_plc"), QStringLiteral("PLC"));
        QVERIFY(plc.deviceConnect());

        QSignalSpy valueSpy(&plc, &vc::device::PlcDevice::valueChanged);
        QVERIFY(valueSpy.isValid());

        QVERIFY(plc.injectInputValue(QStringLiteral("M10"), true));
        QCOMPARE(valueSpy.count(), 1);
        const auto bitValues = valueSpy.takeFirst().at(0).value<QMap<QString, QVariant>>();
        QCOMPARE(bitValues.value(QStringLiteral("M10")).toBool(), true);

        // Coerced BY PREFIX, not by the QVariant handed in. A bit area yields a bool and a
        // register area a number on real hardware, and LocalizationRuntimeController leans on
        // exactly that to tell a bad index apart from a number signal mapped onto a coil.
        QVERIFY(plc.injectInputValue(QStringLiteral("D100"), QStringLiteral("7")));
        QCOMPARE(valueSpy.count(), 1);
        const auto wordValues = valueSpy.takeFirst().at(0).value<QMap<QString, QVariant>>();
        bool numeric = false;
        QCOMPARE(wordValues.value(QStringLiteral("D100")).toInt(&numeric), 7);
        QVERIFY2(numeric, "a D tag must publish a number, or a mapping mistake becomes invisible");

        QCOMPARE(plc.simulatedInputs().size(), 2);
    }

    void test_virtual_plc_refuses_an_invalid_input_tag_with_a_reason()
    {
        VirtualPlcDevice plc(QStringLiteral("inj_bad"), QStringLiteral("PLC"));
        QVERIFY(plc.deviceConnect());

        QSignalSpy valueSpy(&plc, &vc::device::PlcDevice::valueChanged);
        QVERIFY(valueSpy.isValid());

        // As strict as the write path. A virtual PLC that accepted any tag would let a
        // signal-mapping mistake pass here and surface only on real hardware, which is the most
        // expensive place to find one and the exact thing this device exists to avoid.
        QString reason;
        QVERIFY(!plc.injectInputValue(QStringLiteral("COIL00007"), true, &reason));
        QVERIFY2(!reason.isEmpty(), "a refusal must carry a reason");
        QCOMPARE(valueSpy.count(), 0);

        reason.clear();
        QVERIFY(!plc.injectInputValue(QStringLiteral("D100"), QStringLiteral("abc"), &reason));
        QVERIFY(!reason.isEmpty());
        QCOMPARE(valueSpy.count(), 0);
        QVERIFY(plc.simulatedInputs().isEmpty());
    }

    void test_virtual_plc_refuses_injection_while_disconnected()
    {
        VirtualPlcDevice plc(QStringLiteral("inj_down"), QStringLiteral("PLC"));
        QSignalSpy valueSpy(&plc, &vc::device::PlcDevice::valueChanged);
        QVERIFY(valueSpy.isValid());

        // A real PLC cannot deliver a value over a link that is down. Reported rather than
        // silently dropped: the runtime would refuse to act on it anyway, and a poke that
        // vanishes with no explanation reads as a broken panel, not a disconnected device.
        QString reason;
        QVERIFY(!plc.injectInputValue(QStringLiteral("M10"), true, &reason));
        QVERIFY(!reason.isEmpty());
        QCOMPARE(valueSpy.count(), 0);

        QVERIFY(plc.deviceConnect());
        QVERIFY(plc.injectInputValue(QStringLiteral("M10"), true));
        QCOMPARE(valueSpy.count(), 1);
    }

    // The design rule this device would be worthless without. If a value the runtime WROTE read
    // back as an input, the handshake would complete itself — the runtime publishes
    // bMatchingFinished, reads it back, and the cycle appears to work — and it would hide the
    // mapping mistake where two logical signals are bound to the same tag. Real hardware keeps
    // them apart because the plant owns the inputs, not the vision system.
    void test_virtual_plc_keeps_injected_inputs_and_recorded_writes_apart()
    {
        VirtualPlcDevice plc(QStringLiteral("inj_split"), QStringLiteral("PLC"));
        QVERIFY(plc.deviceConnect());

        QVERIFY(plc.writeDigitalIoByName(QStringLiteral("M10"), true));
        QVERIFY2(plc.simulatedInputs().isEmpty(),
                 "a value the runtime wrote must not appear as a driven input");

        QVERIFY(plc.injectInputValue(QStringLiteral("M20"), true));
        QVERIFY2(!plc.digitalWrites.contains(QStringLiteral("M20")),
                 "a driven input must not appear in the record of what the runtime wrote");

        // And the same tag in both directions stays two independent values.
        QVERIFY(plc.injectInputValue(QStringLiteral("M10"), false));
        QCOMPARE(plc.digitalWrites.value(QStringLiteral("M10")), true);
        QCOMPARE(plc.simulatedInputs().value(QStringLiteral("M10")).toBool(), false);
    }

    void test_plc_runner_reports_input_simulation_per_device_not_per_family()
    {
        VirtualPlcDevice virtualPlc(QStringLiteral("sim_virtual"), QStringLiteral("Virtual PLC"));
        PlcRunner virtualRunner(&virtualPlc);
        QVERIFY2(virtualRunner.supportsInputSimulation(),
                 "the hardware-free PLC is the whole reason this capability exists");

        // Same family, same runner class. A real PLC whose inputs this software could forge would
        // be lying about the plant, so this must stay false for every device with hardware behind
        // it — and the UI asks the runner, not the sub-type, before offering the controls.
        McProtocolDevice mcPlc(QStringLiteral("sim_mc"), QStringLiteral("MC PLC"));
        PlcRunner mcRunner(&mcPlc);
        QVERIFY2(!mcRunner.supportsInputSimulation(),
                 "a real PLC must never advertise drivable inputs");
    }

    // Phase 8 / B1. buildRuntimeContext() used to read the robot pick-check settings by casting
    // the bound device's config to VisionOutputDeviceCfg. That cast succeeds only for the
    // vision-output family, so binding any other family to the role would silently hand the
    // runtime a default-constructed config — pick checking disabled on a station commissioned with
    // it enabled, with nothing shown and nothing logged. The settings now come through
    // IResultOutputDevice, and this case pins both halves: a capable device answers with what was
    // commissioned, and an incapable one is refused at setup rather than accepted and defaulted.
    void test_result_output_capability_carries_the_robot_pick_check_settings()
    {
        VirtualVisionOutputDevice device(QStringLiteral("kcheck_vout"),
                                         QStringLiteral("Result Output"));

        auto *cfg = new VirtualVisionOutputCfg();
        cfg->m_kinematicCheck.enabled = true;
        cfg->m_kinematicCheck.collisionCheckEnabled = true;
        cfg->m_kinematicCheck.presetName = QStringLiteral("Nachi MZ04D");
        cfg->m_kinematicCheck.tcpZ = 120.0;
        device.setDeviceConfig(cfg);

        auto *capability = dynamic_cast<IResultOutputDevice *>(&device);
        QVERIFY(capability != nullptr);

        const RobotKinematicCheckConfig readBack = capability->robotKinematicCheckConfig();
        QCOMPARE(readBack.enabled, true);
        QCOMPARE(readBack.collisionCheckEnabled, true);
        QCOMPARE(readBack.presetName, QStringLiteral("Nachi MZ04D"));
        QCOMPARE(readBack.tcpZ, 120.0);

        // A PLC device today implements no result-output capability, so it cannot answer for the
        // check settings — and must therefore not be accepted for the role.
        VirtualPlcDevice plcDevice(QStringLiteral("kcheck_plc"), QStringLiteral("PLC"));
        QVERIFY(dynamic_cast<IResultOutputDevice *>(&plcDevice) == nullptr);

        LocalizationRuntimeFixture fixture;
        auto context = fixture.context();
        context.visionOutputRunner = fixture.plcRunner.data();
        const auto setup = fixture.controller.setup(context);
        QVERIFY(!setup.valid);
        QVERIFY2(setup.errors.join(QStringLiteral("; "))
                     .contains(QStringLiteral("cannot output results")),
                 qPrintable(setup.errors.join(QStringLiteral("; "))));
    }

    // Phase 9 / F1. An ENABLED check whose preset does not resolve is the failure this guard
    // exists for. RobotKinematicPickingChecker fails CLOSED on an unresolved preset — isPickable()
    // returns false for every pose — so the cell grabs, matches, and sends nothing, which reads on
    // the dashboard as "nothing is pickable today". The setup refusal has to name the preset,
    // because a typo in one settings field is otherwise indistinguishable from a fixture problem.
    void test_enabled_pick_check_with_an_unregistered_preset_refuses_setup_and_names_it()
    {
        LocalizationRuntimeFixture fixture;
        auto context = fixture.context();
        RobotKinematicCheckConfig check;
        check.enabled = true;
        check.presetName = QStringLiteral("Nachi MZ99Z");
        context.robotCheckConfig = check;

        const auto setup = fixture.controller.setup(context);
        const QString joined = setup.errors.join(QStringLiteral("; "));
        QVERIFY2(!setup.valid, qPrintable(QStringLiteral("setup must refuse; errors: %1").arg(joined)));
        QVERIFY2(joined.contains(QStringLiteral("Nachi MZ99Z")),
                 qPrintable(QStringLiteral("the refusal must name the preset that did not resolve, "
                                           "or it sends the engineer to the robot; got: %1")
                                .arg(joined)));
    }

    // The other half of the guard: the preset is fine but nothing can be checked, because the
    // active camera has no usable calibration and no pick pose can reach robot coordinates.
    // Reported ALONGSIDE the calibration error rather than instead of it — they are two facts
    // about two settings, and an operator who fixes only the calibration still needs to know the
    // gate they commissioned was not running.
    void test_enabled_pick_check_without_calibration_names_the_reason()
    {
        LocalizationRuntimeFixture fixture;
        auto context = fixture.context(/*calibrated=*/false);
        RobotKinematicCheckConfig check;
        check.enabled = true;
        check.presetName = QStringLiteral("Nachi MZ04D");
        context.robotCheckConfig = check;

        const auto setup = fixture.controller.setup(context);
        const QString joined = setup.errors.join(QStringLiteral("; "));
        QVERIFY(!setup.valid);
        QVERIFY2(joined.contains(QStringLiteral("Robot pick check is enabled"))
                     && joined.contains(QStringLiteral("calibration")),
                 qPrintable(QStringLiteral("an enabled check with no usable calibration must say "
                                           "so; got: %1").arg(joined)));
    }

    // And the case that must NOT change: a cell that never wanted the check. The checker is null
    // here too, so a guard written as "no checker is an error" would refuse every project in the
    // field that has the gate switched off.
    void test_disabled_pick_check_leaves_setup_valid()
    {
        LocalizationRuntimeFixture fixture;
        auto context = fixture.context();
        RobotKinematicCheckConfig check;
        check.enabled = false;
        check.presetName = QStringLiteral("Nachi MZ99Z");  // nonsense, and irrelevant while off
        context.robotCheckConfig = check;

        const auto setup = fixture.controller.setup(context);
        QVERIFY2(setup.valid,
                 qPrintable(setup.errors.join(QStringLiteral("; "))));
        QVERIFY2(!setup.errors.join(QStringLiteral("; "))
                      .contains(QStringLiteral("Robot pick check")),
                 "a disabled check must not produce a message at all");
    }

    // Phase 9 / F1, backlog item 57 — the case that has never worked. The pick check used to be
    // read off the device bound to `vision_output`. On a dual-role cell that device is a PLC, and
    // a PLC with nothing commissioned answers "disabled" — indistinguishable from "this cell does
    // not want the check". A station signed off with reachability checking therefore ran without
    // it, silently, and the setting the operator edited was on a device panel nobody associated
    // with the task.
    //
    // Asserted end to end through beginRuntime() rather than by reaching into buildRuntimeContext()
    // (which is private, and reaching past that would test an access path no caller has): the task
    // config carries an unresolvable preset, so if — and only if — the runtime read the TASK's
    // setting, F1's guard refuses the start and names it. Under the device-sourced read the device
    // answers "disabled", nothing is checked, and the runtime comes up Ready.
    void test_runtime_reads_the_pick_check_from_the_task_when_a_plc_carries_the_output_role()
    {
        TaskLocalizationRuntimeFixture fixture(/*bindPrimaryPlc=*/true,
                                               /*visionOutputOnPlc=*/true);
        QVERIFY2(dynamic_cast<IResultOutputDevice *>(fixture.plc.get()) != nullptr,
                 "the premise of this case: a PLC that really can carry the output role");

        TaskLocalizeConfig cfg = fixture.task->taskLocalizeConfig();
        RobotKinematicCheckConfig check;
        check.enabled = true;
        check.presetName = QStringLiteral("Nachi MZ99Z");
        cfg.setRobotCheckConfig(check);
        fixture.task->setTaskLocalizeConfig(cfg);

        QSignalSpy logSpy(fixture.task, &TaskLocalization::taskLogAppended);
        QVERIFY(logSpy.isValid());

        fixture.task->beginCommission();
        fixture.task->beginRuntime(false);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Faulted, 3000);
        QVERIFY(!fixture.task->isValid());

        // Spun, not read once. The controller -> task log forward is a QueuedConnection
        // (task_localization.cpp:591), and the Faulted transition above is synchronous — so the
        // QTRY_COMPARE returns on its first check, before the event loop has delivered a single
        // entry. Measured: the log came back empty while the refusal was demonstrably emitted.
        const auto loggedMessages = [&logSpy]() {
            QStringList out;
            for (const QList<QVariant> &emission : logSpy) {
                out.append(qvariant_cast<LocalizationRuntimeController::TaskLogEntry>(
                               emission.at(0)).message);
            }
            return out;
        };
        QTRY_VERIFY_WITH_TIMEOUT(!loggedMessages().filter(QStringLiteral("Nachi MZ99Z")).isEmpty(),
                                 3000);

        // The whole log goes into the failure text on purpose. A bare "not found" cannot
        // distinguish "the runtime read the device instead" from "it refused for some unrelated
        // reason and never reached the pick check" — and those need opposite fixes.
        const QStringList logged = loggedMessages();
        QVERIFY2(logged.filter(QStringLiteral("Nachi MZ99Z")).size() == 1,
                 qPrintable(QStringLiteral(
                                "the runtime must have read the TASK's pick check: the device "
                                "bound to vision_output has none commissioned, so a device-sourced "
                                "read would have started this runtime with the gate silently off. "
                                "Task log was:\n  %1")
                                .arg(logged.join(QStringLiteral("\n  ")))));
    }

    // The companion case, and the one that guards the migration: an ordinary vision-output cell
    // whose check is commissioned on the task starts normally. Green under both the old and the
    // new source — which is the point. It is what tells a failing run of the case above that the
    // PLC binding is what broke, not the pick check as a whole.
    void test_vision_output_family_cell_still_starts_with_the_pick_check_commissioned()
    {
        TaskLocalizationRuntimeFixture fixture;

        TaskLocalizeConfig cfg = fixture.task->taskLocalizeConfig();
        RobotKinematicCheckConfig check;
        check.enabled = true;
        check.presetName = QStringLiteral("Nachi MZ04D");
        cfg.setRobotCheckConfig(check);
        fixture.task->setTaskLocalizeConfig(cfg);

        fixture.task->beginCommission();
        fixture.task->beginRuntime(false);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Ready, 3000);
        QVERIFY(fixture.task->isValid());
    }

    // Phase 8 / B2. Modbus tag names are persisted in task signal maps, so the prefix and the
    // padding width are as frozen as any wire format. Two properties matter and neither is
    // obvious from reading the formatter: every name parses back to the area and address it was
    // made from, and no Modbus name can be mistaken for a Mitsubishi one. If the prefixes did
    // collide, a project switched from an MC device to a Modbus device would resolve *some* tags
    // by accident and bind them to unrelated registers — working in part, silently.
    void test_modbus_tag_names_stay_unambiguous_and_parse_back()
    {
        const QList<QPair<ModbusArea, int>> samples = {
            { ModbusArea::Coils, 0 },
            { ModbusArea::Coils, 100 },
            { ModbusArea::DiscreteInputs, 8 },
            { ModbusArea::HoldingRegisters, 1000 },
            { ModbusArea::InputRegisters, 65535 },
        };

        QSet<QString> seen;
        for (const auto &sample : samples) {
            const QString tag = modbus_tags::format(sample.first, sample.second);
            QVERIFY2(!seen.contains(tag), qPrintable(tag));
            seen.insert(tag);

            ModbusArea area = ModbusArea::Coils;
            int address = -1;
            QVERIFY2(modbus_tags::parse(tag, &area, &address), qPrintable(tag));
            QCOMPARE(int(area), int(sample.first));
            QCOMPARE(address, sample.second);
        }

        // The exact spellings the contract doc publishes and saved projects contain.
        QCOMPARE(modbus_tags::format(ModbusArea::Coils, 100), QStringLiteral("COIL00100"));
        QCOMPARE(modbus_tags::format(ModbusArea::DiscreteInputs, 8), QStringLiteral("DI00008"));
        QCOMPARE(modbus_tags::format(ModbusArea::HoldingRegisters, 1000),
                 QStringLiteral("HR01000"));
        QCOMPARE(modbus_tags::format(ModbusArea::InputRegisters, 16), QStringLiteral("IR00016"));

        // Mitsubishi tags must not resolve as Modbus tags, in either direction of confusion.
        QVERIFY(!modbus_tags::parse(QStringLiteral("M0100"), nullptr, nullptr));
        QVERIFY(!modbus_tags::parse(QStringLiteral("D0100"), nullptr, nullptr));
        QVERIFY(!modbus_tags::parse(QStringLiteral("HR"), nullptr, nullptr));
        QVERIFY(!modbus_tags::parse(QStringLiteral("HRxyz"), nullptr, nullptr));

        // Writability is two different questions, and conflating them is a defect this project
        // has already shipped once: the server refused to write the two areas it is the ONLY
        // legitimate writer of, because it asked the master's question.
        //
        // A master has no function code that writes 1x or 3x — that is the specification.
        QVERIFY(modbus_tags::isMasterWritable(ModbusArea::Coils));
        QVERIFY(modbus_tags::isMasterWritable(ModbusArea::HoldingRegisters));
        QVERIFY(!modbus_tags::isMasterWritable(ModbusArea::DiscreteInputs));
        QVERIFY(!modbus_tags::isMasterWritable(ModbusArea::InputRegisters));

        // A server owns the register space, so it writes all four. "Input" is named from the
        // server's point of view: data entering the model from the server's own process.
        for (const ModbusArea area : kModbusAreas) {
            QVERIFY2(modbus_tags::isServerWritable(area),
                     qPrintable(modbus_tags::prefix(area)));
        }
    }

    // Phase 8 / B2. The register map feeds PlcDevice::valueChanged(), which drives the task's
    // signal edges. Two behaviours are load-bearing: the first read after connect must report
    // nothing (or every signal that happens to be true would fire a rising edge at connect, and
    // the task would see a trigger that never happened), and word values must come out signed,
    // matching what the Mitsubishi family publishes for the same register content.
    void test_modbus_register_map_reports_only_what_changed()
    {
        ModbusRegisterMap map;
        map.configure(/*coils*/ { 0, 4 }, /*discreteInputs*/ { 0, 0 },
                      /*holding*/ { 100, 3 }, /*inputRegisters*/ { 0, 0 });

        QCOMPARE(map.digitalTagNames().size(), 4);
        QCOMPARE(map.wordTagNames().size(), 3);
        QCOMPARE(map.digitalTagNames().first(), QStringLiteral("COIL00000"));
        QCOMPARE(map.wordTagNames().first(), QStringLiteral("HR00100"));

        // First poll after configure: values arrive, but they are a starting state, not edges.
        QVERIFY(map.setBit(ModbusArea::Coils, 1, true));
        QVERIFY(map.setWord(ModbusArea::HoldingRegisters, 100, 5));
        QVERIFY(map.takeChangedValues().isEmpty());

        // Second poll: only what actually moved.
        QVERIFY(map.setBit(ModbusArea::Coils, 1, false));
        QVERIFY(map.setWord(ModbusArea::HoldingRegisters, 101, 7));
        const QMap<QString, QVariant> changed = map.takeChangedValues();
        QCOMPARE(changed.size(), 2);
        QCOMPARE(changed.value(QStringLiteral("COIL00001")).toBool(), false);
        QCOMPARE(changed.value(QStringLiteral("HR00101")).toInt(), 7);

        // Nothing moved since: no report at all, not a report of zero changes.
        QVERIFY(map.takeChangedValues().isEmpty());

        // Words are published signed, exactly as the Mitsubishi family publishes D devices.
        QVERIFY(map.setWord(ModbusArea::HoldingRegisters, 102, 40000));
        QCOMPARE(map.takeChangedValues().value(QStringLiteral("HR00102")).toInt(), -25536);

        // Addresses outside the configured spans are refused rather than silently created, and
        // an area's kind is enforced: a coil is not a register.
        QVERIFY(!map.setBit(ModbusArea::Coils, 99, true));
        QVERIFY(!map.setWord(ModbusArea::HoldingRegisters, 99, 1));
        QVERIFY(!map.setWord(ModbusArea::Coils, 0, 1));
        QVERIFY(!map.setBit(ModbusArea::HoldingRegisters, 100, true));
    }

    // Phase 8 / B2. The result layout is a published wire contract
    // (docs/domains/task_localization/modbus_result_contract.md). These are the exact registers
    // that document's worked example promises, asserted literally: if this case has to be edited
    // to make it pass, the contract has moved and every PLC program written against it is now
    // wrong. That is the reason to assert the bytes rather than a round trip alone.
    void test_modbus_result_layout_matches_the_published_contract()
    {
        VisionOutputPosition position;
        position.x  = 125.50;
        position.y  = -40.25;
        position.z  = 0.0;
        position.rx = 0.0;
        position.ry = 0.0;
        position.rz = 37.10;

        bool truncated = true;
        const QList<quint16> payload =
            ModbusResultLayout::encodePayload({ position }, 8, &truncated);
        QVERIFY(!truncated);

        // Full capacity, always: the tail past the last valid position is written as zeros so a
        // program trusting a stale count cannot read the previous cycle's poses as current.
        QCOMPARE(payload.size(), 8 * ModbusResultLayout::kRegistersPerPosition);

        // x = 125.50 -> 12550 -> 0x0000 0x3106   (high word first)
        QCOMPARE(payload.at(0), quint16(0x0000));
        QCOMPARE(payload.at(1), quint16(0x3106));
        // y = -40.25 -> -4025 -> 0xFFFF 0xF047
        QCOMPARE(payload.at(2), quint16(0xFFFF));
        QCOMPARE(payload.at(3), quint16(0xF047));
        // z, rx, ry all zero
        for (int i = 4; i < 10; ++i) {
            QCOMPARE(payload.at(i), quint16(0x0000));
        }
        // rz = 37.10 -> 3710 -> 0x0000 0x0E7E
        QCOMPARE(payload.at(10), quint16(0x0000));
        QCOMPARE(payload.at(11), quint16(0x0E7E));
        // Second slot onwards is zero-filled.
        for (int i = ModbusResultLayout::kRegistersPerPosition; i < payload.size(); ++i) {
            QCOMPARE(payload.at(i), quint16(0x0000));
        }

        const QList<quint16> header = ModbusResultLayout::encodeHeader(1, 3, 0);
        QCOMPARE(header.size(), ModbusResultLayout::kHeaderRegisters);
        QCOMPARE(header.at(ModbusResultLayout::kOffsetCount), quint16(1));
        QCOMPARE(header.at(ModbusResultLayout::kOffsetSequence), quint16(3));
        QCOMPARE(header.at(ModbusResultLayout::kOffsetFlags), quint16(0));
        QCOMPARE(header.at(ModbusResultLayout::kOffsetReserved), quint16(0));

        // Decoding gets the pose back at the contract's stated resolution of two decimals.
        const QVector<VisionOutputPosition> decoded =
            ModbusResultLayout::decodePayload(payload, 1);
        QCOMPARE(decoded.size(), 1);
        QCOMPARE(decoded.at(0).x, 125.50);
        QCOMPARE(decoded.at(0).y, -40.25);
        QCOMPARE(decoded.at(0).rz, 37.10);

        // Truncation is reported, and count never exceeds capacity.
        QVector<VisionOutputPosition> many;
        for (int i = 0; i < 5; ++i) {
            many.append(position);
        }
        // Not named `small`: windows.h defines that as a typedef for char, and the error it
        // produces points at the line after the declaration.
        const QList<quint16> clipped = ModbusResultLayout::encodePayload(many, 2, &truncated);
        QVERIFY(truncated);
        QCOMPARE(clipped.size(), 2 * ModbusResultLayout::kRegistersPerPosition);

        // Block size and the single-request boundary the handshake rule depends on.
        QCOMPARE(ModbusResultLayout::registerCount(8), 100);
        QCOMPARE(ModbusResultLayout::registerCount(ModbusResultLayout::kMaxPositionsInSingleRequest),
                 112);
        QVERIFY(ModbusResultLayout::registerCount(
                    ModbusResultLayout::kMaxPositionsInSingleRequest) <= 123);
        QVERIFY(ModbusResultLayout::registerCount(
                    ModbusResultLayout::kMaxPositionsInSingleRequest + 1) > 123);

        // Sequence never reaches 0 (which means "nothing published since power-on") and never
        // leaves the positive range of a signed 16-bit word.
        QCOMPARE(ModbusResultLayout::nextSequence(0), quint16(1));
        QCOMPARE(ModbusResultLayout::nextSequence(ModbusResultLayout::kMaxSequence), quint16(1));
        QCOMPARE(ModbusResultLayout::nextSequence(5), quint16(6));

        // A non-finite axis encodes as 0 rather than as an arbitrary bit pattern: this register
        // becomes a pose a robot moves to.
        quint16 high = 0xFFFF;
        quint16 low = 0xFFFF;
        ModbusResultLayout::encodeAxis(std::numeric_limits<double>::quiet_NaN(), &high, &low);
        QCOMPARE(high, quint16(0));
        QCOMPARE(low, quint16(0));
    }

    // Phase 8 / B2. Both Modbus configs must survive the save/load round trip, keep the register
    // map and result block they were commissioned with, and stay distinguishable — a client
    // config must not load from a server document.
    void test_modbus_configs_round_trip_json()
    {
        ModbusTcpClientCfg client;
        client.m_unitId = 7;
        client.m_coilStart = 10;
        client.m_coilCount = 32;
        client.m_holdingStart = 200;
        client.m_holdingCount = 16;
        client.m_resultStartAddress = 2000;
        client.m_resultMaxPositions = 4;
        client.m_hostAddress = QStringLiteral("10.0.0.5");
        client.m_port = 5020;
        client.m_refreshInterval = 250;
        client.m_responseTimeout = 750;
        client.m_retryCount = 2;

        ModbusTcpClientCfg clientLoaded;
        QVERIFY(clientLoaded.fromJson(client.toJson()));
        QCOMPARE(clientLoaded.m_unitId, 7);
        QCOMPARE(clientLoaded.m_coilStart, 10);
        QCOMPARE(clientLoaded.m_coilCount, 32);
        QCOMPARE(clientLoaded.m_resultStartAddress, 2000);
        QCOMPARE(clientLoaded.m_resultMaxPositions, 4);
        QCOMPARE(clientLoaded.m_hostAddress, QStringLiteral("10.0.0.5"));
        QCOMPARE(clientLoaded.m_port, 5020);
        QCOMPARE(clientLoaded.m_refreshInterval, 250);
        QCOMPARE(clientLoaded.m_responseTimeout, 750);
        QCOMPARE(clientLoaded.m_retryCount, 2);

        ModbusTcpServerCfg server;
        server.m_listenAddress = QStringLiteral("127.0.0.1");
        server.m_port = 15020;
        server.m_resultStartAddress = 500;

        ModbusTcpServerCfg serverLoaded;
        QVERIFY(serverLoaded.fromJson(server.toJson()));
        QCOMPARE(serverLoaded.m_listenAddress, QStringLiteral("127.0.0.1"));
        QCOMPARE(serverLoaded.m_port, 15020);
        QCOMPARE(serverLoaded.m_resultStartAddress, 500);

        // The PlcType tag keeps the two apart: loading a server document into a client config
        // must fail rather than half-apply the shared fields.
        ModbusTcpClientCfg crossed;
        QVERIFY(!crossed.fromJson(server.toJson()));
        QVERIFY(!serverLoaded.fromJson(client.toJson()));

        // The register map built from a config covers exactly what was configured.
        ModbusRegisterMap map;
        clientLoaded.configureRegisterMap(map);
        QCOMPARE(map.range(ModbusArea::Coils).start, 10);
        QCOMPARE(map.range(ModbusArea::Coils).count, 32);
        QCOMPARE(map.digitalTagNames().size(), 32 + clientLoaded.m_discreteInputCount);

        // Result-block geometry the widgets and the device both depend on.
        QCOMPARE(clientLoaded.resultRange().start, 2000);
        QCOMPARE(clientLoaded.resultRange().count, ModbusResultLayout::registerCount(4));
        QVERIFY(clientLoaded.resultFitsSingleRequest());
        QVERIFY(!clientLoaded.overlapsPolledResultRange());

        // Defaults: the result block sits clear of the polled range on purpose, and the default
        // capacity publishes in a single Modbus request.
        ModbusTcpClientCfg defaults;
        QVERIFY(!defaults.overlapsPolledResultRange());
        QVERIFY(defaults.resultFitsSingleRequest());

        // Overlap is detected, not merely assumed impossible.
        ModbusTcpClientCfg overlapping;
        overlapping.m_holdingStart = 1000;
        overlapping.m_holdingCount = 64;
        QVERIFY(overlapping.overlapsPolledResultRange());

        // A client publishes into holding registers and has no say in it: a master has no
        // function code that writes an input register.
        QCOMPARE(int(defaults.resultArea()), int(ModbusArea::HoldingRegisters));
        QCOMPARE(int(clientLoaded.resultArea()), int(ModbusArea::HoldingRegisters));

        // A server may publish into input registers instead — the layout a master cannot
        // overwrite. Default stays holding registers so an unchanged project keeps the published
        // contract, and the choice survives the round trip.
        QCOMPARE(int(serverLoaded.resultArea()), int(ModbusArea::HoldingRegisters));
        ModbusTcpServerCfg inputResult;
        inputResult.m_resultInInputRegisters = true;
        inputResult.m_resultStartAddress = 300;
        QCOMPARE(int(inputResult.resultArea()), int(ModbusArea::InputRegisters));

        ModbusTcpServerCfg inputResultLoaded;
        QVERIFY(inputResultLoaded.fromJson(inputResult.toJson()));
        QCOMPARE(int(inputResultLoaded.resultArea()), int(ModbusArea::InputRegisters));
        QCOMPARE(inputResultLoaded.m_resultStartAddress, 300);

        // Overlap is now asked of the area the result actually lives in. With the result in input
        // registers, the mapped *holding* span is irrelevant to it.
        inputResultLoaded.m_holdingStart = 300;
        inputResultLoaded.m_holdingCount = 64;
        QVERIFY(!inputResultLoaded.overlapsPolledResultRange());
        inputResultLoaded.m_inputRegisterStart = 300;
        inputResultLoaded.m_inputRegisterCount = 64;
        QVERIFY(inputResultLoaded.overlapsPolledResultRange());
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
                              CameraTypeToString(CameraType::JaiGigE),
                              CameraTypeToString(CameraType::VirtualCamera)}));
        QCOMPARE(DeviceRegistry::displayNamesFor(DeviceType::PLC),
                 (QStringList{PlcTypeToString(PlcType::MitsubishiMc),
                              PlcTypeToString(PlcType::ModbusTcpClient),
                              PlcTypeToString(PlcType::ModbusTcpServer),
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

        const DeviceRegistryEntry *jaiEntry =
            DeviceRegistry::find(DeviceType::Camera,
                                 CameraTypeToString(CameraType::JaiGigE));
        QVERIFY2(jaiEntry != nullptr,
                 "the JAI camera is not registered, so it cannot be created from the wizard "
                 "or from a project file no matter what the class does");
        QCOMPARE(jaiEntry->configJsonKey, QStringLiteral(DEVICE_JSK_CAM_TYPE));

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

    void test_camera_runner_continuous_start_and_stop_each_resolve_once()
    {
        auto camera = std::make_shared<VirtualCameraDevice>(
            QStringLiteral("cmd_cam_cont"), QStringLiteral("Command Camera Continuous"));
        CameraRunner runner(camera.get());
        // attach()+start(), unlike the sibling command tests which drive the runner's slots
        // directly. wireSignals() only runs on attach(), so without it the device's state report
        // never reaches the runner and every continuous command would time out — which is
        // precisely the regression this test has to be able to see.
        runner.attach();
        runner.start();

        QSignalSpy finishedSpy(&runner, &CameraRunner::commandFinished);
        QSignalSpy stateSpy(&runner, &CameraRunner::continuousStateChanged);
        QVERIFY(finishedSpy.isValid());
        QVERIFY(stateSpy.isValid());

        const DeviceCommand startCmd = DeviceCommand::create(
            DeviceCommandKind::CameraContinuousStart, camera->id(), 2000);
        QCOMPARE(runner.submitCommand(startCmd).status, DeviceCommandResultStatus::Accepted);

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 4000);
        const DeviceCommandResult started = findResultByCommandId(finishedSpy, startCmd.id);
        QCOMPARE(started.status, DeviceCommandResultStatus::Succeeded);
        QCOMPARE(stateSpy.count(), 1);
        QCOMPARE(stateSpy.at(0).at(0).toBool(), true);

        const DeviceCommand stopCmd = DeviceCommand::create(
            DeviceCommandKind::CameraContinuousStop, camera->id(), 2000);
        QCOMPARE(runner.submitCommand(stopCmd).status, DeviceCommandResultStatus::Accepted);

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 2, 4000);
        const DeviceCommandResult stopped = findResultByCommandId(finishedSpy, stopCmd.id);
        QCOMPARE(stopped.status, DeviceCommandResultStatus::Succeeded);

        // Exactly one outcome each — the whole point of keeping frames off grabFinished().
        QCOMPARE(finishedSpy.count(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(stateSpy.count(), 2, 1000);
        QCOMPARE(stateSpy.at(1).at(0).toBool(), false);

        runner.stop();
    }

    void test_camera_runner_continuous_stop_succeeds_when_nothing_is_streaming()
    {
        auto camera = std::make_shared<VirtualCameraDevice>(
            QStringLiteral("cmd_cam_cont_idem"), QStringLiteral("Command Camera Idempotent"));
        CameraRunner runner(camera.get());
        runner.attach();
        runner.start();

        QSignalSpy finishedSpy(&runner, &CameraRunner::commandFinished);
        QVERIFY(finishedSpy.isValid());

        // A toggle button cannot always know, and the device reports its state after every
        // request precisely so this resolves instead of waiting out the watchdog. A regression
        // here shows up as a UI that freezes for the timeout on a redundant click.
        const DeviceCommand stopCmd = DeviceCommand::create(
            DeviceCommandKind::CameraContinuousStop, camera->id(), 2000);
        QCOMPARE(runner.submitCommand(stopCmd).status, DeviceCommandResultStatus::Accepted);

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 4000);
        const DeviceCommandResult stopped = findResultByCommandId(finishedSpy, stopCmd.id);
        QCOMPARE(stopped.status, DeviceCommandResultStatus::Succeeded);
        QCOMPARE(stopped.code, DeviceCommandResultCode::None);

        runner.stop();
    }

    void test_camera_runner_refuses_backlight_on_a_camera_with_no_io_port()
    {
        // VirtualCameraDevice is the only camera in the tree that answers hasIOPort() false
        // (both GigE cameras return true), which makes it the only vehicle this refusal path
        // has. There is no production camera exercising it, so this test is the only thing
        // keeping the reason attached to the failure.
        auto camera = std::make_shared<VirtualCameraDevice>(
            QStringLiteral("cmd_cam_backlight_none"), QStringLiteral("Command Camera No IO"));
        QVERIFY(!camera->hasIOPort());

        CameraRunner runner(camera.get());
        runner.attach();
        runner.start();

        QSignalSpy finishedSpy(&runner, &CameraRunner::commandFinished);
        QVERIFY(finishedSpy.isValid());

        runner.requestBacklight(true);

        // Refused immediately, not after the watchdog: the capability answer is known before any
        // thread hop, so waiting for a timeout would only lose the reason.
        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 2000);
        const DeviceCommandResult result =
            qvariant_cast<DeviceCommandResult>(finishedSpy.at(0).at(0));
        QCOMPARE(result.status, DeviceCommandResultStatus::Rejected);
        QCOMPARE(result.code, DeviceCommandResultCode::UnsupportedCommand);
        QVERIFY(!result.message.isEmpty());

        runner.stop();
    }

    void test_camera_runner_backlight_commands_resolve_once_on_the_reported_level()
    {
        auto camera = std::make_shared<VirtualCameraDevice>(
            QStringLiteral("cmd_cam_backlight"), QStringLiteral("Command Camera Backlight"));
        CameraRunner runner(camera.get());
        runner.attach();
        runner.start();

        QSignalSpy finishedSpy(&runner, &CameraRunner::commandFinished);
        QSignalSpy stateSpy(&runner, &CameraRunner::backlightStateChanged);
        QVERIFY(finishedSpy.isValid());
        QVERIFY(stateSpy.isValid());

        // Driven through the runner's slot rather than a device, the way the sibling command
        // tests do: what is under test is the command lifecycle, and a device that could really
        // drive a lamp is not available here.
        const DeviceCommand onCmd = DeviceCommand::create(
            DeviceCommandKind::CameraBacklightOn, camera->id(), 2000);
        QCOMPARE(runner.submitCommand(onCmd).status, DeviceCommandResultStatus::Accepted);
        QVERIFY(QMetaObject::invokeMethod(&runner, "onBacklightStateChanged",
                                          Qt::DirectConnection, Q_ARG(bool, true)));

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 4000);
        const DeviceCommandResult lit = findResultByCommandId(finishedSpy, onCmd.id);
        QCOMPARE(lit.status, DeviceCommandResultStatus::Succeeded);

        // Exactly one outcome per command — a second report must not resolve it again.
        const int afterOn = finishedSpy.count();
        QVERIFY(QMetaObject::invokeMethod(&runner, "onBacklightStateChanged",
                                          Qt::DirectConnection, Q_ARG(bool, true)));
        QCOMPARE(finishedSpy.count(), afterOn);

        // A level that is not the one asked for is a failure with a reason, not a silent success.
        const DeviceCommand offCmd = DeviceCommand::create(
            DeviceCommandKind::CameraBacklightOff, camera->id(), 2000);
        QCOMPARE(runner.submitCommand(offCmd).status, DeviceCommandResultStatus::Accepted);
        QVERIFY(QMetaObject::invokeMethod(&runner, "onBacklightStateChanged",
                                          Qt::DirectConnection, Q_ARG(bool, true)));

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= afterOn + 1, 4000);
        const DeviceCommandResult refused = findResultByCommandId(finishedSpy, offCmd.id);
        QCOMPARE(refused.status, DeviceCommandResultStatus::Failed);
        QCOMPARE(refused.code, DeviceCommandResultCode::DeviceError);

        // Every report reaches listeners, resolved or not: the button follows the lamp.
        QVERIFY(stateSpy.count() >= 3);

        runner.stop();
    }

    void test_camera_runner_backlight_reports_never_resolve_a_grab()
    {
        auto camera = std::make_shared<VirtualCameraDevice>(
            QStringLiteral("cmd_cam_backlight_grab"), QStringLiteral("Command Camera Backlight Grab"));
        CameraRunner runner(camera.get());

        QSignalSpy finishedSpy(&runner, &CameraRunner::commandFinished);
        QVERIFY(finishedSpy.isValid());

        // The auto-backlight sequence switches the lamp on before the trigger and off after it,
        // so a grab produces two backlight reports of its own. Resolving the grab from either
        // would end it before there was an image — the same trap continuous frames pose.
        const DeviceCommand grab = DeviceCommand::create(
            DeviceCommandKind::CameraSingleShot, camera->id(), 4000);
        QCOMPARE(runner.submitCommand(grab).status, DeviceCommandResultStatus::Accepted);

        QVERIFY(QMetaObject::invokeMethod(&runner, "onBacklightStateChanged",
                                          Qt::DirectConnection, Q_ARG(bool, true)));
        QVERIFY(QMetaObject::invokeMethod(&runner, "onBacklightStateChanged",
                                          Qt::DirectConnection, Q_ARG(bool, false)));
        QCOMPARE(finishedSpy.count(), 0);

        vc::device::GrabResult result;
        result.isGrabSuccess = true;
        result.msg = QStringLiteral("single shot success");
        QVERIFY(QMetaObject::invokeMethod(&runner, "onGrabFinished", Qt::DirectConnection,
                                          Q_ARG(vc::device::GrabResult, result)));

        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 2000);
        const DeviceCommandResult grabbed = findResultByCommandId(finishedSpy, grab.id);
        QCOMPARE(grabbed.status, DeviceCommandResultStatus::Succeeded);
    }

    void test_camera_runner_continuous_frames_never_resolve_a_command()
    {
        auto camera = std::make_shared<VirtualCameraDevice>(
            QStringLiteral("cmd_cam_cont_frames"), QStringLiteral("Command Camera Frames"));
        CameraRunner runner(camera.get());

        QSignalSpy finishedSpy(&runner, &CameraRunner::commandFinished);
        QSignalSpy frameSpy(&runner, &CameraRunner::continuousFrameReady);
        QVERIFY(finishedSpy.isValid());
        QVERIFY(frameSpy.isValid());

        // A single shot is in flight. Continuous frames arriving now must not resolve it, and
        // neither must the state report that a pre-empting stop produces — routing either through
        // grabFinished() would resolve a command from something that is not its outcome.
        // Driven through the runner's own slots, like the sibling command tests, so the device
        // stays on this thread and the ordering is deterministic.
        const DeviceCommand singleShotCmd = DeviceCommand::create(
            DeviceCommandKind::CameraSingleShot, camera->id(), 5000);
        QCOMPARE(runner.submitCommand(singleShotCmd).status, DeviceCommandResultStatus::Accepted);

        vc::device::GrabResult frame;
        frame.isGrabSuccess = true;
        frame.msg = QStringLiteral("continuous frame");
        for (int i = 0; i < 5; ++i) {
            emit runner.continuousFrameReady(frame);
        }
        QVERIFY(QMetaObject::invokeMethod(&runner, "onContinuousStateChanged",
                                          Qt::DirectConnection, Q_ARG(bool, false)));

        QCOMPARE(frameSpy.count(), 5);
        QCOMPARE(finishedSpy.count(), 0);

        // ...and the single shot still resolves normally afterwards, from its own signal.
        vc::device::GrabResult done;
        done.isGrabSuccess = true;
        done.msg = QStringLiteral("single shot success");
        QVERIFY(QMetaObject::invokeMethod(&runner, "onGrabFinished", Qt::DirectConnection,
                                          Q_ARG(vc::device::GrabResult, done)));
        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() >= 1, 1000);
        QCOMPARE(findResultByCommandId(finishedSpy, singleShotCmd.id).status,
                 DeviceCommandResultStatus::Succeeded);
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

    // Phase 9 / F3. Nothing measured a cycle end to end before this: CycleResult carried only
    // matchingTimeMs, which is the matcher's own number for one stage of four. That made the
    // standing "revisit threading only on measured latency evidence" criterion impossible to
    // evaluate, and a customer asking "what is my cycle time" unanswerable.
    void test_successful_cycle_stamps_every_stage_monotonically()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy cycleSpy(&fixture.controller,
                            &LocalizationRuntimeController::cycleResultUpdated);
        int lastCycleId = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int cycleId, std::shared_ptr<mtc::MatchGroup>, CameraWorkspace,
                             cv::Mat, std::shared_ptr<mtc::IRobotPickingChecker>) {
            lastCycleId = cycleId;
        });
        QVERIFY(cycleSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), true, 1000);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(lastCycleId, 1, 1000);
        fixture.controller.onRuntimeMatchingFinished(
            lastCycleId, LocalizationRuntimeFixture::makeMatchResult());

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 1000);
        const auto result = qvariant_cast<LocalizationRuntimeController::CycleResult>(
            cycleSpy.takeFirst().at(0));
        QCOMPARE(result.faulted, false);

        const auto &t = result.timings;
        QVERIFY2(t.grabFinishedMs && t.matchingFinishedMs && t.sendFinishedMs
                     && t.outputsPublishedMs,
                 "a cycle that completed reached all four boundaries, so all four must be set");

        // Non-decreasing, not strictly increasing: on a fast machine two boundaries can land
        // inside the same tick, and demanding strict growth would make this flaky rather than
        // correct. (nsecsElapsed() makes that unlikely, not impossible.)
        QVERIFY2(*t.grabFinishedMs <= *t.matchingFinishedMs
                     && *t.matchingFinishedMs <= *t.sendFinishedMs
                     && *t.sendFinishedMs <= *t.outputsPublishedMs,
                 qPrintable(QStringLiteral("stages must not go backwards; got %1, %2, %3, %4")
                                .arg(*t.grabFinishedMs).arg(*t.matchingFinishedMs)
                                .arg(*t.sendFinishedMs).arg(*t.outputsPublishedMs)));

        // The total covers the match, so it cannot be smaller than what the matcher reported for
        // its own share. This is the assertion that catches a total taken from the wrong origin.
        QVERIFY2(*t.outputsPublishedMs >= result.matchingTimeMs,
                 qPrintable(QStringLiteral("total %1 ms cannot be less than matching %2 ms")
                                .arg(*t.outputsPublishedMs).arg(result.matchingTimeMs)));

        // And matchingTimeMs keeps its own meaning — it is not redefined as a derived stage.
        QCOMPARE(result.matchingTimeMs,
                 double(LocalizationRuntimeFixture::makeMatchResult().ExecutionTime));
    }

    // The half that matters more in the field: a cycle that faulted must carry the stages it
    // reached and leave the rest UNSET. Zero-filling would read as "instant", and the slow stage
    // before a timeout is the most interesting number in the phase.
    void test_faulted_cycle_carries_only_the_stages_it_reached()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy cycleSpy(&fixture.controller,
                            &LocalizationRuntimeController::cycleResultUpdated);
        QVERIFY(cycleSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), true, 1000);

        // Fail the grab. The cycle reaches the grab boundary and stops there.
        fixture.camera1->grabSucceeds = false;
        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 2000);
        const auto result = qvariant_cast<LocalizationRuntimeController::CycleResult>(
            cycleSpy.takeFirst().at(0));
        QCOMPARE(result.faulted, true);

        const auto &t = result.timings;
        QVERIFY2(t.grabFinishedMs.has_value(),
                 "the camera did answer — a failed grab still took however long it took, and that "
                 "is exactly the number this instrumentation exists to capture");
        QVERIFY2(!t.matchingFinishedMs && !t.sendFinishedMs && !t.outputsPublishedMs,
                 "boundaries the cycle never reached must stay UNSET, never zero-filled");
    }

    // A camera or pattern number the project does not define must FAIL, visibly and durably.
    //
    // Both used to publish their "invalid" bit and return, leaving m_cycleState at
    // ReadyForTrigger — so the rejection had no memory. A trigger was still accepted and ran on
    // the previously bound camera, and the next markRuntimeReady() republished
    // bTaskReady/bCameraValid/bPatternValid as true, erasing the evidence. The owner found this
    // on the cell: "đổi part và camera number sang một index chưa được đăng ký, task vẫn ready".
    void test_localization_unregistered_camera_number_faults_and_stays_faulted()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy faultSpy(&fixture.controller, &LocalizationRuntimeController::runtimeFault);
        QVERIFY(signalSpy.isValid());
        QVERIFY(faultSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        // Cameras 1 and 2 are registered; 7 is not.
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 7}});

        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bCameraValid")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        QVERIFY2(faultSpy.count() >= 1, "an unregistered camera number did not raise runtimeFault");

        // And the state must hold: a trigger arriving now must NOT start a cycle on the camera
        // that happens to still be bound.
        QSignalSpy cycleSpy(&fixture.controller,
                            &LocalizationRuntimeController::cycleResultUpdated);
        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTest::qWait(50);
        QCOMPARE(cycleSpy.count(), 0);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
    }

    void test_localization_unregistered_pattern_group_faults_and_stays_faulted()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy faultSpy(&fixture.controller, &LocalizationRuntimeController::runtimeFault);
        QVERIFY(signalSpy.isValid());
        QVERIFY(faultSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        // Only pattern group 1 is registered.
        fixture.controller.handlePlcValues({{QStringLiteral("D101"), 9}});

        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bPatternValid")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        QVERIFY2(faultSpy.count() >= 1, "an unregistered pattern group did not raise runtimeFault");

        QSignalSpy cycleSpy(&fixture.controller,
                            &LocalizationRuntimeController::cycleResultUpdated);
        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTest::qWait(50);
        QCOMPARE(cycleSpy.count(), 0);
    }

    // Zero is not a reserved "no selection" value, and never was — it is simply a number that can
    // never name a camera (slots are 1..16) or a pattern group (indices are 1..32).
    //
    // It used to be swallowed by a `> 0` guard in handlePlcValues(), so a PLC power-up register
    // sitting at 0 left the task Ready on whatever index the saved project had selected, while any
    // OTHER unregistered number faulted correctly. The owner found the inconsistency on the cell.
    // Nothing documented the guard: the "unset" sentinel in this class is -1, and both sibling
    // entry points pass the value through unfiltered.
    void test_localization_zero_active_index_is_refused_by_the_range_check()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy faultSpy(&fixture.controller, &LocalizationRuntimeController::runtimeFault);
        QVERIFY(signalSpy.isValid());
        QVERIFY(faultSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 0}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bCameraValid")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        QVERIFY2(faultSpy.count() >= 1, "camera number 0 was accepted");
        // Refused BY THE RANGE CHECK, which is the point. 0 would fault anyway when the runner
        // lookup missed, so asserting only "it faulted" would pass with the range check deleted
        // and would pin nothing. The reason is the requirement.
        QVERIFY2(faultSpy.last().at(0).toString().contains(QStringLiteral("range")),
                 "0 must be refused as out of range, not reported as an unregistered camera");

        // Recovers on a valid number, with no operator action — the same rule as any other
        // refused index.
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 1}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        const int faultsBefore = faultSpy.count();
        fixture.controller.handlePlcValues({{QStringLiteral("D101"), 0}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bPatternValid")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        QVERIFY2(faultSpy.count() > faultsBefore, "pattern group 0 was accepted");
        QVERIFY2(faultSpy.last().at(0).toString().contains(QStringLiteral("range")),
                 "0 must be refused as out of range, not reported as a missing pattern group");

        fixture.controller.handlePlcValues({{QStringLiteral("D101"), 1}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);
    }

    // Out of range and unregistered are different failures and must not be conflated: 99 can never
    // name a camera, while 3 could but was not bound in this project. Both fault; only the message
    // tells the integrator which mistake they made.
    void test_localization_out_of_range_active_index_is_refused()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy faultSpy(&fixture.controller, &LocalizationRuntimeController::runtimeFault);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        // 99 is past kMaxCameraNumber (16); a negative is past the bottom.
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 99}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        QVERIFY(faultSpy.count() >= 1);
        QVERIFY2(faultSpy.last().at(0).toString().contains(QStringLiteral("range")),
                 "an out-of-range number must say so, not report it as unregistered");

        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 1}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        fixture.controller.handlePlcValues({{QStringLiteral("D100"), -5}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
    }

    // A number signal mapped to a bit area is a mapping mistake, not a bad index, and no range
    // check can diagnose it. It must fault and name the tag.
    void test_localization_non_numeric_active_index_reports_the_mapping_not_the_value()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy faultSpy(&fixture.controller, &LocalizationRuntimeController::runtimeFault);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        fixture.controller.handlePlcValues(
            {{QStringLiteral("D100"), QVariant(QStringLiteral("not a number"))}});

        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        QVERIFY(faultSpy.count() >= 1);
        const QString reason = faultSpy.last().at(0).toString();
        QVERIFY2(reason.contains(QStringLiteral("D100")),
                 "the fault must name the tag, which is the only way to find the mapping");
        QVERIFY2(!reason.contains(QStringLiteral("range")),
                 "a non-numeric value must not be reported as an out-of-range index");

        // The contract lists bCameraValid=false among the outputs of ALL THREE failure modes.
        // This path used to publish bTaskReady/bTaskFault/nFaultCode and skip the domain flag,
        // leaving bCameraValid standing at true underneath the fault.
        QVERIFY2(!lastSignalValue(signalSpy, QStringLiteral("bCameraValid")).toBool(),
                 "a mis-mapped nActiveCamera left bCameraValid true");

        // And it latches like a bad index: an unrelated valid write must not re-arm the task,
        // because the mapping is still wrong and no camera has actually been selected.
        fixture.controller.handlePlcValues({{QStringLiteral("D101"), 1}});
        QTest::qWait(50);
        QVERIFY2(!lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                 "a valid pattern group re-armed a task whose nActiveCamera mapping is wrong");
    }

    // The cross-signal hole the owner found on the cell, described exactly as they hit it: set
    // camera 1 (ready), set camera 0 (fault, correct), then set pattern group 0 (fault) and back
    // to 1 — and the task returned to Ready with bCameraValid true, while the master's own
    // nActiveCamera register still held the 0 the task had refused. It then accepted triggers and
    // ran cycles on a camera nobody had selected.
    //
    // Root cause: a refused number is deliberately never adopted, so the refusal left nothing for
    // markRuntimeReady()'s checks to notice — they read the last GOOD camera, which still
    // validates fine. Every re-arm path in the controller funnels through markRuntimeReady(), so
    // that is where the refusal has to be remembered, and each signal clears only its own latch.
    void test_localization_a_refused_index_is_not_forgiven_by_the_other_index()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        // Camera 0: refused, as it should be.
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 0}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bCameraValid")).toBool(), false);

        // A perfectly good pattern group. It settles its OWN signal and nothing else.
        fixture.controller.handlePlcValues({{QStringLiteral("D101"), 1}});
        QTest::qWait(50);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bPatternValid")).toBool(), true);
        QVERIFY2(!lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                 "a valid pattern group re-armed the task while the camera selection was refused");
        QVERIFY2(!lastSignalValue(signalSpy, QStringLiteral("bCameraValid")).toBool(),
                 "bCameraValid was republished as true while nActiveCamera was still refused");

        // Nor does a trigger get through on the camera that happens to still be bound.
        QSignalSpy cycleSpy(&fixture.controller,
                            &LocalizationRuntimeController::cycleResultUpdated);
        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTest::qWait(50);
        QCOMPARE(cycleSpy.count(), 0);
        fixture.controller.handlePlcValues({{QStringLiteral("M10"), false}});

        // Only a valid camera number lifts it, and then the task is genuinely ready.
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 1}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bCameraValid")).toBool(), true);
    }

    // The same hole in the other direction. It is not symmetric by accident: an in-range but
    // unregistered GROUP is committed to m_activePatternGroupNumber, so validateActivePatternGroup()
    // keeps failing and blocks the re-arm on its own. An out-of-range group is refused before the
    // commit, so only the latch stops it — which is why 0 is the value to test here.
    void test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        fixture.controller.handlePlcValues({{QStringLiteral("D101"), 0}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bPatternValid")).toBool(), false);

        // Camera 2 is registered and calibrated: a fully valid selection on the other signal.
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 2}});
        QTest::qWait(50);
        QVERIFY2(!lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                 "a valid camera re-armed the task while the pattern selection was refused");
        QVERIFY2(!lastSignalValue(signalSpy, QStringLiteral("bPatternValid")).toBool(),
                 "bPatternValid was republished as true while nActivePatternGroup was refused");

        fixture.controller.handlePlcValues({{QStringLiteral("D101"), 1}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bPatternValid")).toBool(), true);
    }

    // bErrorReset is an acknowledge, not a repair — the rule the contract already states for a
    // PatternNotRegistered fault, applied to the same class of problem. It must clear bTaskFault and
    // nFaultCode, and must NOT re-arm a task whose commanded index is still refused: nothing has
    // told the runtime which camera to use, so declaring it ready would be a lie the PLC acts on.
    void test_localization_error_reset_does_not_lift_a_refused_index()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 0}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);

        // Rising edge on the acknowledge input.
        fixture.controller.handlePlcValues({{QStringLiteral("M19"), true}});
        QTest::qWait(50);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), false);
        QVERIFY2(!lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                 "bErrorReset re-armed a task whose camera selection was still refused");

        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 1}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);
    }

    // The other half of the same contract, and the reason the fix is not simply "fault on
    // rejection": the runtime must recover on its own when a valid number follows a bad one.
    // Faulting the pattern setter WITHOUT adding its missing re-arm would strand the runtime here
    // with bTaskReady false until an operator pressed bErrorReset — which contradicts the rule
    // that it must never park on a transient fault waiting for a person.
    void test_localization_recovers_when_a_valid_index_follows_a_rejected_one()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        fixture.controller.handlePlcValues({{QStringLiteral("D101"), 9}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);

        // The correction, with no operator action in between.
        fixture.controller.handlePlcValues({{QStringLiteral("D101"), 1}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bPatternValid")).toBool(), true);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), false);

        // Same for the camera, whose setter already had a re-arm on its success path.
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 7}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 2}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bCameraValid")).toBool(), true);
    }

    // ── Backlog 43, the acceptance criterion ──────────────────────────────────
    // Everything else in this suite drives the controller by calling handlePlcValues() directly,
    // which proves the controller's logic but skips the entire delivery path. This one drives it
    // the way the cell does: a value is injected into the PLC DEVICE, crosses the runner's thread
    // boundary as valueChanged(), and reaches the controller through the connection setup() made.
    //
    // That is the difference between "a hardware-free project reaches Ready" and "a hardware-free
    // project runs a cycle", which is the single thing item 43 existed to close.
    void test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QSignalSpy cycleSpy(&fixture.controller,
                            &LocalizationRuntimeController::cycleResultUpdated);
        QVERIFY(signalSpy.isValid());
        QVERIFY(cycleSpy.isValid());

        int lastCycleId = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int cycleId,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace,
                             cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            lastCycleId = cycleId;
        });

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        // Not fixture.controller.handlePlcValues(...) — the point is the delivery path. The device
        // lives on the runner's worker thread, so this is queued through PlcRunner exactly as the
        // device panel does it from the GUI thread.
        fixture.plcRunner->requestInjectInputValue(QStringLiteral("M10"), true);

        QTRY_VERIFY_WITH_TIMEOUT(lastCycleId != 0, 2000);
        fixture.controller.onRuntimeMatchingFinished(
            lastCycleId, LocalizationRuntimeFixture::makeMatchResult());

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 2000);
        const auto result = qvariant_cast<LocalizationRuntimeController::CycleResult>(
            cycleSpy.takeFirst().at(0));
        QCOMPARE(result.faulted, false);
        QCOMPARE(result.detectedNumber, 1);

        // Release the trigger the same way, and the runtime re-arms: the full handshake ran with
        // no hardware and no direct call into the controller.
        fixture.plcRunner->requestInjectInputValue(QStringLiteral("M10"), false);
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 2000);

        // The active-index signals reach it the same way. D100 is nActiveCamera; camera 2 is
        // registered and calibrated in the fixture.
        fixture.plcRunner->requestInjectInputValue(QStringLiteral("D100"), 2);
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("nActiveCamera")).toInt(), 2, 2000);
    }

    // ── Phase 9 / A2: the active camera workspace at startup (backlog 59.2) ───
    //
    // m_context.activeCameraWorkspace and m_activeCameraWorkspace were written in exactly one
    // place: setActiveCameraNumber(). buildRuntimeContext() never filled them and setup() never
    // derived them, so a runtime that started and was never COMMANDED to change camera ran its
    // whole session on a default workspace — useWorkspace false made the matcher search the full
    // frame instead of the commissioned ROI, and useConditionWorkspace false meant
    // outSideConditionRoiCheck() was never called, so every object kept
    // m_isOutsideConditionRoi == false and was admitted as a pick target. Objects the
    // commissioning engineer had fenced out were sent to the robot on the first cycle of every
    // session, with nothing logged and every lamp green.
    //
    // The assertion is on the workspace `runtimeMatchingRequested` actually CARRIES, which every
    // other fixture in this file discards.
    void test_setup_applies_the_active_camera_workspace_before_the_first_cycle()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        auto ctx = fixture.context();

        CameraWorkspace commissioned;
        commissioned.useWorkspace = true;
        commissioned.useConditionWorkspace = true;
        ctx.config.setCameraWorkspace(fixture.camera1->id(), commissioned);
        fixture.controller.configure(ctx.config);

        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        CameraWorkspace carried;
        int matchingCount = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace workspace,
                             cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            carried = workspace;
            matchingCount += 1;
        });

        const auto setup = fixture.controller.setup(ctx);
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        // First cycle, with NO camera-change command anywhere.
        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(matchingCount, 1, 2000);

        QVERIFY2(carried.useWorkspace,
                 "the commissioned crop ROI was not applied on the first cycle after setup");
        QVERIFY2(carried.useConditionWorkspace,
                 "the condition ROI filter was off, so fenced-out objects would be sent");
    }

    /// A camera with nothing commissioned keeps the default, and the no-runner path assigns both
    /// members rather than dereferencing. The cached member is not reset by setup(), so clearing
    /// it here is what stops a workspace from a previous session surviving into this one.
    void test_a_camera_with_no_workspace_keeps_the_default()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        CameraWorkspace carried;
        carried.useWorkspace = true;   // poisoned, so an unwritten argument cannot pass by accident
        int matchingCount = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int,
                             std::shared_ptr<mtc::MatchGroup>,
                             CameraWorkspace workspace,
                             cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) {
            carried = workspace;
            matchingCount += 1;
        });

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        fixture.controller.handlePlcValues({{QStringLiteral("M10"), true}});
        QTRY_COMPARE_WITH_TIMEOUT(matchingCount, 1, 2000);

        QCOMPARE(carried.useWorkspace, false);
        QCOMPARE(carried.useConditionWorkspace, false);
    }

    // ── Phase 9 / A3: clearRoleContext() drops only what it wired (backlog 53) ─
    //
    // The blanket `disconnect(context.runner, nullptr, this, nullptr)` removed EVERY signal from
    // that runner to the controller. One ModbusTcpServerDevice can serve primary_plc AND
    // vision_output in the same task — the configuration Phase 8/B1 enabled and the owner runs —
    // so clearing either role also dropped m_plcValueConnection, and with it every
    // bExecuteTrigger, bErrorReset and index write, with bTaskReady still true and the device
    // still reporting Connected. Nothing failed; nothing was logged.
    //
    // ⚠️ MEASURED 2026-09-08: NEITHER CASE BELOW PINS THAT DEFECT. Both stay green with the
    // blanket disconnect restored, with or without the setup() line order that used to protect it.
    // The reason is structural: clearRoleContext() only ever runs with a populated context on the
    // Camera role (via bindActiveCameraRole), and a camera runner is a DIFFERENT QObject from the
    // PLC runner that carries m_plcValueConnection — so the blanket form has nothing of the value
    // stream to take. On every other path resetRuntimeBindings() has already emptied
    // m_recoveryContexts, so clearRoleContext() returns before disconnecting anything.
    //
    // The defect is reachable ONLY when one device serves both roles, and no hardware-free fixture
    // can express that today: the dual-role binding needs a device that is both a PlcDevice and an
    // IResultOutputDevice, and VirtualPlcDevice is deliberately not one (Open Question O-1).
    //
    // These two are kept as genuine regression guards for the value stream — an invariant worth
    // holding either way — but they are NOT evidence for item 53. The fix there is defensive and
    // reasoned, not test-proven. Do not let the green tick read as coverage.
    void test_a_camera_rebind_does_not_drop_the_plc_input_stream()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        // Rebinds the Camera role: bindActiveCameraRole -> bindRoleContext -> clearRoleContext.
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 2}});
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        // The proof is a PLC input arriving through the REAL delivery path after the rebind:
        // device -> runner thread -> valueChanged -> handlePlcValues. Calling handlePlcValues()
        // directly would bypass m_plcValueConnection and pass even with it destroyed.
        int matchingCount = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int, std::shared_ptr<mtc::MatchGroup>, CameraWorkspace, cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) { matchingCount += 1; });

        fixture.plcRunner->requestInjectInputValue(QStringLiteral("M10"), true);
        QTRY_VERIFY2_WITH_TIMEOUT(matchingCount == 1,
                                  "the PLC value stream was lost when the camera role was rebound",
                                  2000);
    }

    /// Binding the fixed roles must not disturb the value stream either — this is the path that
    /// runs on every setup(), and the one whose line ordering used to be load-bearing.
    void test_the_plc_value_stream_survives_binding_the_fixed_roles()
    {
        qRegisterMetaType<std::shared_ptr<mtc::MatchGroup>>("std::shared_ptr<mtc::MatchGroup>");
        qRegisterMetaType<cv::Mat>("cv::Mat");

        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        // Two setups in a row: the second re-enters bindRoleContext for every role, and each of
        // those clears the previous context first.
        QVERIFY(fixture.controller.setup(fixture.context()).valid);
        const auto second = fixture.controller.setup(fixture.context());
        QVERIFY2(second.valid, qPrintable(second.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true, 1000);

        int matchingCount = 0;
        QObject::connect(&fixture.controller,
                         &LocalizationRuntimeController::runtimeMatchingRequested,
                         &fixture.controller,
                         [&](int, std::shared_ptr<mtc::MatchGroup>, CameraWorkspace, cv::Mat,
                             std::shared_ptr<mtc::IRobotPickingChecker>) { matchingCount += 1; });

        fixture.plcRunner->requestInjectInputValue(QStringLiteral("M10"), true);
        QTRY_VERIFY2_WITH_TIMEOUT(matchingCount == 1,
                                  "a second setup() left the PLC value stream disconnected", 2000);

        // Qt::UniqueConnection must still hold: one trigger, one cycle, not two.
        QTest::qWait(100);
        QCOMPARE(matchingCount, 1);
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
        // MESSAGE CHANGED BY Phase 9 / C2, deliberately; everything else in this case is
        // unchanged. 99 is outside MatchGroup's 1..32, so it is not a *missing* group — it is a
        // number that can never name one, and saying "missing" sent a commissioning engineer
        // looking for a group that was never expected to exist (backlog item 55). What a
        // rejection *does* is untouched: the three publishes and the fault code below are the
        // D7 behaviour this case exists to pin, and they still hold. The genuinely-missing case
        // (an in-range number with no group bound to it) still reports "missing" — that path
        // runs validateActivePatternGroup() exactly as before.
        QVERIFY2(setup.errors.contains(
                     QStringLiteral("Pattern group number 99 is outside the valid range 1..32.")),
                 qPrintable(setup.errors.join(QStringLiteral("; "))));
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

    // ── Phase 9 / B1: the other two roles' liveness ──────────────────────────────────────
    //
    // handleRoleStatusChanged() treats all three roles the same way and gives each its own
    // fault code, but only the camera arm had a test. The two below are not new behaviour —
    // they pin behaviour that has always been there and that nothing would have caught losing.
    // The negative check for them is recorded in the plan: making handleRoleStatusChanged()
    // ignore RunnerRole::PrimaryPlc must turn the PLC cases red and leave the camera case green.

    void test_localization_runtime_plc_loss_faults_running_cycle()
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

        // The cycle is mid-flight: matching was requested and is deliberately never answered.
        fixture.plc->forceConnectionStatus(ConnectStatus::LostConnected);

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 1000);
        auto result = qvariant_cast<LocalizationRuntimeController::CycleResult>(
            cycleSpy.takeFirst().at(0));
        QCOMPARE(result.faulted, true);
        QCOMPARE(result.faultCode, LocalizationFaultCode::PlcLost);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(), 300);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        // Nothing reached the robot: the abort lands before any result is sent.
        QCOMPARE(fixture.visionOutput->requestCount, 0);
    }

    void test_localization_runtime_vision_output_loss_faults_running_cycle()
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

        fixture.visionOutput->forceConnectionStatus(ConnectStatus::LostConnected);

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 1000);
        auto result = qvariant_cast<LocalizationRuntimeController::CycleResult>(
            cycleSpy.takeFirst().at(0));
        QCOMPARE(result.faulted, true);
        QCOMPARE(result.faultCode, LocalizationFaultCode::VisionOutputLost);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(), 200);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        QCOMPARE(fixture.visionOutput->requestCount, 0);
    }

    // The documented unbounded-retry contract (plc_signal_contract.md, "Connection loss"):
    // a role lost OUTSIDE a cycle withdraws readiness and reconnects forever, and never
    // escalates to a task fault. Nothing stated that in a test until now, which made the
    // no-fault half — the deliberate part — indistinguishable from an oversight.
    void test_localization_runtime_plc_loss_outside_a_cycle_withdraws_ready_without_fault()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);

        fixture.plc->forceConnectionStatus(ConnectStatus::LostConnected);

        // Readiness is withdrawn, because the master triggers only while bTaskReady is true.
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  false,
                                  1000);
        // But an outage is not a fault: retrying is unbounded, so raising bTaskFault here would
        // demand an operator acknowledge a link that is expected to return on its own.
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), false);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M18")), false);

        // Re-arms with no operator action once the role is healthy again.
        fixture.plc->forceConnectionStatus(ConnectStatus::Connected);
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);
    }

    void test_localization_runtime_vision_output_loss_outside_a_cycle_withdraws_ready_without_fault()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);

        fixture.visionOutput->forceConnectionStatus(ConnectStatus::LostConnected);

        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  false,
                                  1000);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), false);
        QCOMPARE(fixture.plc->digitalWrites.value(QStringLiteral("M18")), false);

        fixture.visionOutput->forceConnectionStatus(ConnectStatus::Connected);
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);
    }

    // One place, all three runner families. VisionOutputRunner was missing this forward
    // entirely while PlcRunner and CameraRunner had it, and nothing said so — the runner
    // families are wired independently and there was no test that compared them.
    //
    // Asserts the forward by making a device report an error and watching the runner re-emit
    // it, not by inspecting connections: a test that checks connect() was called passes just
    // as happily when the signal it wires is never delivered.
    void test_every_runner_family_forwards_device_errors_to_the_controller()
    {
        LocalizationRuntimeFixture fixture;

        // Emitted from the device's OWN thread, which is where a device reports an error from.
        // The runners wire this queued; emitting across threads from here would exercise a
        // path the product never takes.
        auto emitDeviceError = [](vc::device::IDevice *device, const QString &msg) {
            QMetaObject::invokeMethod(device, [device, msg]() {
                emit device->errorOccurred(msg);
            }, Qt::QueuedConnection);
        };

        QSignalSpy plcSpy(fixture.plcRunner.data(), &IDeviceRunner::errorOccurred);
        QSignalSpy cameraSpy(fixture.cameraRunner1.data(), &IDeviceRunner::errorOccurred);
        QSignalSpy outputSpy(fixture.visionOutputRunner.data(), &IDeviceRunner::errorOccurred);
        QVERIFY(plcSpy.isValid());
        QVERIFY(cameraSpy.isValid());
        QVERIFY(outputSpy.isValid());

        emitDeviceError(fixture.plc.data(), QStringLiteral("plc device error"));
        emitDeviceError(fixture.camera1.data(), QStringLiteral("camera device error"));
        emitDeviceError(fixture.visionOutput.data(), QStringLiteral("vision output device error"));

        QTRY_COMPARE_WITH_TIMEOUT(plcSpy.count(), 1, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(cameraSpy.count(), 1, 1000);
        QTRY_VERIFY2_WITH_TIMEOUT(
            outputSpy.count() == 1,
            "VisionOutputRunner must forward IDevice::errorOccurred like the other two runner "
            "families; without it a vision-output device error reaches nobody.",
            1000);

        QCOMPARE(plcSpy.takeFirst().at(0).toString(), QStringLiteral("plc device error"));
        QCOMPARE(cameraSpy.takeFirst().at(0).toString(), QStringLiteral("camera device error"));
        QCOMPARE(outputSpy.takeFirst().at(0).toString(),
                 QStringLiteral("vision output device error"));
    }

    // ── Phase 9 / B2(a): the TaskLocalization hop ────────────────────────────────────────
    //
    // Everything above drives LocalizationRuntimeController directly. Nothing exercised
    // TaskLocalization::beginRuntime() itself, so the ordering it depends on — runners
    // registered and attached before runtimeStarted() tells the UI to go look at them — was
    // an assumption. F2's only automatable assertion is this one.
    void test_task_localization_begin_runtime_emits_runtime_started_after_runners_exist()
    {
        TaskLocalizationRuntimeFixture fixture;

        int startedCount = 0;
        bool runnersReadyAtSignal = false;
        QObject::connect(fixture.task, &ITask::runtimeStarted, fixture.task, [&]() {
            startedCount += 1;
            auto *taskRunner = fixture.task->taskRunner();
            auto *runner = taskRunner ? taskRunner->runnerFor(fixture.plc->id()) : nullptr;
            runnersReadyAtSignal = runner != nullptr && runner->isAttached();
        });

        fixture.task->beginCommission();
        fixture.task->beginRuntime(false);

        QTRY_COMPARE_WITH_TIMEOUT(startedCount, 1, 3000);
        QVERIFY2(runnersReadyAtSignal,
                 "runtimeStarted() must not be emitted before the runners are registered and "
                 "attached: a widget that wires itself on that signal resolves runnerFor() to "
                 "null and never retries.");
        QVERIFY(fixture.task->isValid());
    }

    // The :194-198 branch. A project whose bindings do not resolve must end Faulted and
    // invalid rather than half-started — and must NOT announce runtimeStarted(), which would
    // tell every listener a runtime exists.
    // Also found by the 2026-09-08 field run: the startup summary reported
    // "camera= 1 ( commanded )" on a runtime nobody had commanded anything on, because
    // buildRuntimeContext() pre-resolved both indices to firstKey() before setup() could tell the
    // two apart. The provenance is the only thing that distinguishes "the task is on camera 1"
    // from "the master and the task agree", so a label that is always wrong is worse than none.
    void test_build_runtime_context_leaves_the_selection_for_setup_to_resolve()
    {
        TaskLocalizationRuntimeFixture fixture;
        QSignalSpy logSpy(fixture.task, &TaskLocalization::taskLogAppended);
        QVERIFY(logSpy.isValid());

        fixture.task->beginCommission();
        fixture.task->beginRuntime(false);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Ready, 3000);

        // The runtime still resolves to the first bound camera and group — setup() applies the
        // identical fallback, so the outcome is unchanged and only the provenance survives.
        QVERIFY(fixture.task->isValid());

        QString summary;
        for (const QList<QVariant> &emission : logSpy) {
            const auto entry = qvariant_cast<LocalizationRuntimeController::TaskLogEntry>(
                emission.at(0));
            if (entry.message.startsWith(QStringLiteral("Runtime selection:"))) {
                summary = entry.message;
                break;
            }
        }
        QVERIFY2(!summary.isEmpty(), "the startup summary must reach the operator's task log");
        // Asserted as the ABSENCE of "commanded", not the presence of "project default": the
        // summary carries a source for each of the two indices, so a contains() on the positive
        // string passes while one of the halves is still wrong. Measured — that is exactly how
        // the first version of this case passed its own negative check.
        QVERIFY2(!summary.contains(QStringLiteral("commanded")),
                 qPrintable(QStringLiteral("nothing commanded either index, so neither half of the "
                                           "summary may claim it was commanded; got: %1")
                                .arg(summary)));
    }

    void test_task_localization_incomplete_bindings_end_runtime_faulted_and_invalid()
    {
        TaskLocalizationRuntimeFixture fixture(/*bindPrimaryPlc=*/false);
        QSignalSpy startedSpy(fixture.task, &ITask::runtimeStarted);
        QVERIFY(startedSpy.isValid());

        fixture.task->beginCommission();
        fixture.task->beginRuntime(false);

        QCOMPARE(fixture.task->taskState(), TaskState::Faulted);
        QVERIFY(!fixture.task->isValid());
        QCOMPARE(startedSpy.count(), 0);
    }

    // ── Phase 9 / B2(b): a whole cycle, one layer up ─────────────────────────────────────
    //
    // The controller-level cycle test intercepts runtimeMatchingRequested and feeds a canned
    // MatchResult back. This one does not: matching is served by the task's own matchingRunner
    // worker and the real LocalizationPipeline, so the hop the controller test stubs out is the
    // one under test here.
    //
    // **What is deliberately NOT asserted: how many objects the matcher finds.** The pattern is
    // a synthetic 8x8 field and the frame comes from a virtual camera; whatever the matcher makes
    // of that is not a contract, and pinning it would turn a matcher tuning change into a failure
    // in a test about signal plumbing. The cycle completing and re-arming is the contract.
    void test_task_localization_runs_a_full_cycle_through_the_real_matching_worker()
    {
        TaskLocalizationRuntimeFixture fixture;
        QSignalSpy cycleSpy(fixture.task, &TaskLocalization::cycleResultUpdated);
        QSignalSpy signalSpy(fixture.task, &ITask::signalChanged);
        QVERIFY(cycleSpy.isValid());
        QVERIFY(signalSpy.isValid());

        fixture.task->beginCommission();
        fixture.task->beginRuntime(false);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Ready, 3000);

        auto *plcRunner = fixture.plcRunner();
        QVERIFY2(plcRunner != nullptr, "the task must expose the PLC runner it registered");

        // Driven only by a PLC input, queued onto the device thread exactly as the device panel
        // does it. Nothing calls into the controller directly.
        plcRunner->requestInjectInputValue(QStringLiteral("M10"), true);

        QTRY_COMPARE_WITH_TIMEOUT(cycleSpy.count(), 1, 10000);

        // Release the trigger; the runtime re-arms with no operator action.
        plcRunner->requestInjectInputValue(QStringLiteral("M10"), false);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Ready, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), true, 2000);
    }

    // The forwards at task_localization.cpp:593-606. Everything the controller publishes has to
    // arrive on the task's own signals with the same name and value, because that is the only
    // path any widget has to it — the dashboard subscribes to the task, never to the controller.
    void test_task_localization_forwards_controller_signals_to_its_own_listeners()
    {
        TaskLocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(fixture.task, &ITask::signalChanged);
        QSignalSpy stateSpy(fixture.task, &ITask::taskStateChanged);
        QSignalSpy logSpy(fixture.task, &TaskLocalization::taskLogAppended);
        QVERIFY(signalSpy.isValid());
        QVERIFY(stateSpy.isValid());
        QVERIFY(logSpy.isValid());

        fixture.task->beginCommission();
        fixture.task->beginRuntime(false);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Ready, 3000);

        // Readiness reached the task's own listeners, not just the controller's.
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), true, 2000);

        auto *plcRunner = fixture.plcRunner();
        QVERIFY(plcRunner != nullptr);
        plcRunner->requestInjectInputValue(QStringLiteral("M10"), true);

        // The cycle's own signals arrive the same way.
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("bMatchingFinished")).toBool(), true, 10000);
        QVERIFY2(logSpy.count() > 0,
                 "taskLogAppended must carry the runtime's operator log; the dashboard has no "
                 "other source for it.");

        plcRunner->requestInjectInputValue(QStringLiteral("M10"), false);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Ready, 5000);

        // Ready -> RunningCycle -> Ready, observed through the task's state signal.
        QList<TaskState> observed;
        for (const QList<QVariant> &emission : stateSpy) {
            observed.append(emission.at(0).value<TaskState>());
        }
        QVERIFY2(observed.contains(TaskState::RunningCycle),
                 "taskStateChanged must report the cycle, not only its endpoints");
        QCOMPARE(observed.last(), TaskState::Ready);
    }

    // ── Phase 9 / C2: setup() validates the active indices, and says what it refused ──────
    //
    // The range and registration checks existed only inside the two setters. setup() ran none of
    // them, so an unregistered active camera fell through to validateActiveCameraCalibration()
    // and came out as "Active camera calibration is invalid." — the wrong screen and the wrong
    // fault code for a binding problem. That is backlog item 55.
    //
    // The setter-level cases above are the stay-green half: this extraction must not change what
    // a rejection *does* (D7), only where the check lives.

    void test_setup_refuses_an_out_of_range_active_camera_with_a_range_message()
    {
        LocalizationRuntimeFixture fixture;
        auto ctx = fixture.context();
        ctx.activeCameraNumber = 0;

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY(!setup.valid);
        const QString errors = setup.errors.join(QStringLiteral("; "));
        QVERIFY2(errors.contains(QStringLiteral("0")) && errors.contains(QStringLiteral("1..16")),
                 qPrintable(QStringLiteral("setup must name the number and the legal range; got: %1")
                                .arg(errors)));
        QVERIFY2(!errors.contains(QStringLiteral("calibration"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("a number that can never name a camera is not a "
                                           "calibration problem; got: %1").arg(errors)));
    }

    void test_setup_refuses_an_unregistered_active_camera_distinctly_from_calibration()
    {
        LocalizationRuntimeFixture fixture;
        auto ctx = fixture.context();
        ctx.activeCameraNumber = 5;   // in range 1..16, but only 1 and 2 are bound

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY(!setup.valid);
        const QString errors = setup.errors.join(QStringLiteral("; "));
        QVERIFY2(errors.contains(QStringLiteral("registered")) &&
                     errors.contains(QStringLiteral("5")),
                 qPrintable(QStringLiteral("setup must report a binding problem as one; got: %1")
                                .arg(errors)));
        QVERIFY2(!errors.contains(QStringLiteral("calibration"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("item 55: an unregistered camera was reported as a "
                                           "calibration failure; got: %1").arg(errors)));
    }

    void test_setup_refuses_an_out_of_range_active_pattern_group_with_a_range_message()
    {
        LocalizationRuntimeFixture fixture;
        auto ctx = fixture.context();
        ctx.activePatternGroupNumber = 0;

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY(!setup.valid);
        const QString errors = setup.errors.join(QStringLiteral("; "));
        QVERIFY2(errors.contains(QStringLiteral("Pattern group number 0")),
                 qPrintable(QStringLiteral("setup must name the refused group number; got: %1")
                                .arg(errors)));
        QVERIFY2(errors.contains(QString::number(mtc::MatchGroup::getMinGroupRange())) &&
                     errors.contains(QString::number(mtc::MatchGroup::getMaxGroupRange())),
                 qPrintable(QStringLiteral("the range must come from MatchGroup, not a copied "
                                           "constant; got: %1").arg(errors)));
    }

    // -1 means "first available". buildRuntimeContext() resolves it before setup() ever sees it,
    // so this path is dead for the production caller — but the contract tests build contexts
    // directly, and C6 will make the distinction load-bearing again.
    void test_setup_resolves_the_first_camera_and_group_when_the_context_asks_for_the_default()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        auto ctx = fixture.context();
        ctx.activeCameraNumber = -1;
        ctx.activePatternGroupNumber = -1;

        const auto setup = fixture.controller.setup(ctx);

        // Validity IS the assertion here, and it discriminates: the controller has no public
        // accessor for the resolved numbers, but -1 reaches the new validators unchanged if the
        // firstKey() fallback is removed, and -1 is out of range for both. So this case goes red
        // the moment the fallback stops running, which is what it exists to pin.
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);
    }

    // ── Phase 9 / C3: report the selection, never echo the command register ──────────────
    //
    // nActiveCamera and nActivePatternGroup are INPUTS the master owns. The runtime used to
    // publish the accepted value straight back onto them, which is a write race against the
    // master on a server binding and is rejected outright on a client binding whose command
    // registers are input registers — field-confirmed 2026-09-08, backlog item 60.
    //
    // These assert against VirtualPlcDevice::wordWrites, not against the signalChanged spy:
    // handlePlcValues() emits signalChanged for the *input* too, so a spy cannot tell a write
    // from an echo.

    void test_setup_announces_the_active_selection_on_status_signals()
    {
        LocalizationRuntimeFixture fixture;

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));

        QTRY_VERIFY_WITH_TIMEOUT(fixture.plc->wordWrites.contains(QStringLiteral("D104")) &&
                                     fixture.plc->wordWrites.contains(QStringLiteral("D105")),
                                 2000);
        QCOMPARE(fixture.plc->wordWrites.value(QStringLiteral("D104")), qint16(1));
        QCOMPARE(fixture.plc->wordWrites.value(QStringLiteral("D105")), qint16(1));
    }

    void test_the_runtime_never_writes_the_command_registers()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(),
                                  true,
                                  1000);

        // A master-commanded change, through the same path a real PLC uses.
        fixture.controller.handlePlcValues({{QStringLiteral("D100"), 2}});
        QTRY_COMPARE_WITH_TIMEOUT(fixture.plc->wordWrites.value(QStringLiteral("D104")),
                                  qint16(2),
                                  2000);
        fixture.controller.setActivePatternGroupNumber(1);

        QVERIFY2(!fixture.plc->wordWrites.contains(QStringLiteral("D100")),
                 "the runtime wrote the master's own camera command register");
        QVERIFY2(!fixture.plc->wordWrites.contains(QStringLiteral("D101")),
                 "the runtime wrote the master's own pattern-group command register");
    }

    void test_status_signals_with_no_tag_emit_but_do_not_write()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        auto ctx = fixture.context();
        ctx.config.setnActiveCameraStatus(QString());
        ctx.config.setnActivePatternGroupStatus(QString());

        const auto setup = fixture.controller.setup(ctx);
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));

        // The dashboard still sees them — publishNumberSignal() always emits...
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("nActiveCameraStatus")).toInt(), 1, 2000);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nActivePatternGroupStatus")).toInt(), 1);

        // ...and with no tag mapped, nothing at all reaches the PLC. Leaving these unbound is a
        // supported configuration, not a degraded one.
        QVERIFY(!fixture.plc->wordWrites.contains(QStringLiteral("D104")));
        QVERIFY(!fixture.plc->wordWrites.contains(QStringLiteral("D105")));
    }

    void test_a_refused_selection_is_not_announced()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        const auto setup = fixture.controller.setup(fixture.context());
        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QTRY_COMPARE_WITH_TIMEOUT(fixture.plc->wordWrites.value(QStringLiteral("D104")),
                                  qint16(1),
                                  2000);

        // 5 is in range but nothing is bound to it. The status register must keep reporting the
        // camera the runtime is actually on, or it becomes a second command register that lies.
        fixture.controller.setActiveCameraNumber(5);

        QCOMPARE(fixture.plc->wordWrites.value(QStringLiteral("D104")), qint16(1));
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
    }

    // ── Phase 9 / C7: an index that names nothing is not a lost connection ────────────────
    //
    // Every one of these paths used to publish nFaultCode 100 (CameraLost) for a camera number
    // that named no camera. A master reading 100 checks cabling and power for what is a
    // selection problem — the same wrong signpost item 55 removed from the message text, still
    // standing in the code the PLC actually branches on. 103 (CameraNotRegistered) says what
    // failed. All three camera sites are asserted separately because they are three distinct
    // publish statements, and a partial change is exactly the failure this guards.

    void test_an_index_that_names_nothing_reports_not_registered_not_lost()
    {
        const int kCameraNotRegistered = 103;
        const int kPatternNotRegistered = 400;
        const int kCameraLost = 100;

        // (1) The runtime setter: in range, nothing bound to it.
        {
            LocalizationRuntimeFixture fixture;
            QSignalSpy signalSpy(&fixture.controller,
                                 &LocalizationRuntimeController::signalChanged);
            QVERIFY(signalSpy.isValid());
            QVERIFY2(fixture.controller.setup(fixture.context()).valid, "fixture must set up");

            fixture.controller.setActiveCameraNumber(5);

            const int code = lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt();
            QCOMPARE(code, kCameraNotRegistered);
            QVERIFY2(code != kCameraLost, "an unregistered camera was never connected to lose");
        }

        // (2) setup() with the number carried in the context.
        {
            LocalizationRuntimeFixture fixture;
            QSignalSpy signalSpy(&fixture.controller,
                                 &LocalizationRuntimeController::signalChanged);
            QVERIFY(signalSpy.isValid());
            auto ctx = fixture.context();
            ctx.activeCameraNumber = 5;

            const auto setup = fixture.controller.setup(ctx);

            QVERIFY2(!setup.valid, "a context naming an unbound camera is not a usable config");
            QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(),
                     kCameraNotRegistered);
        }

        // (3) setup() with the number read from the master's register — the field case.
        {
            LocalizationRuntimeFixture fixture;
            QSignalSpy signalSpy(&fixture.controller,
                                 &LocalizationRuntimeController::signalChanged);
            QVERIFY(signalSpy.isValid());
            auto ctx = fixture.context();
            ctx.activeCameraNumber = -1;
            ctx.plcSnapshot = std::make_shared<vc::device::VirtualPlcValueMap>(
                QMap<QString, QVariant>{{QStringLiteral("D100"), 0}});

            const auto setup = fixture.controller.setup(ctx);

            QVERIFY2(setup.valid, "the configuration is usable; only the selection was refused");
            QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(),
                     kCameraNotRegistered);
        }

        // (4) The pattern half keeps value 400 across the rename, so a deployed PLC program that
        //     branches on 400 is unaffected. Only the name changed.
        {
            LocalizationRuntimeFixture fixture;
            QSignalSpy signalSpy(&fixture.controller,
                                 &LocalizationRuntimeController::signalChanged);
            QVERIFY(signalSpy.isValid());
            QVERIFY2(fixture.controller.setup(fixture.context()).valid, "fixture must set up");

            // 0 is below the group range [1, 32] — the number the field master was holding.
            fixture.controller.setActivePatternGroupNumber(0);

            QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("nFaultCode")).toInt(),
                     kPatternNotRegistered);
        }
    }

    // ── Phase 9 / C4: the signal-map gate (backlog item 1) ───────────────────────────────
    //
    // A map with a required signal unmapped, or a tag the device does not have, used to start
    // cleanly and then do nothing — indistinguishable from a PLC that stopped talking. The gate
    // refuses it at setup with a message naming the signal.

    void test_each_required_signal_unmapped_fails_setup_naming_it()
    {
        // Five separate assertions, deliberately: one loop that stops at the first failure would
        // leave four of the five unproven the moment anything regressed.
        const QStringList required = {QStringLiteral("bExecuteTrigger"),
                                      QStringLiteral("bTaskReady"),
                                      QStringLiteral("bMatchingFinished"),
                                      QStringLiteral("bTaskFault"),
                                      QStringLiteral("nFaultCode")};
        for (const QString &signalName : required) {
            LocalizationRuntimeFixture fixture;
            auto ctx = fixture.context();
            const QMetaObject &meta = TaskLocalizeConfig::staticMetaObject;
            const int idx = meta.indexOfProperty(signalName.toUtf8().constData());
            QVERIFY(idx >= 0);
            QVERIFY(meta.property(idx).writeOnGadget(&ctx.config, QString()));

            const auto setup = fixture.controller.setup(ctx);
            const QString errors = setup.errors.join(QStringLiteral("; "));
            QVERIFY2(!setup.valid,
                     qPrintable(QStringLiteral("%1 unmapped must fail setup").arg(signalName)));
            QVERIFY2(errors.contains(signalName),
                     qPrintable(QStringLiteral("the error must name %1; got: %2")
                                    .arg(signalName, errors)));
        }
    }

    void test_an_unmapped_optional_signal_is_valid_and_warns()
    {
        LocalizationRuntimeFixture fixture;
        auto ctx = fixture.context();
        ctx.config.setbMatchingLowArea(QString());

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
    }

    // bErrorReset unbound is explicitly supported: a latched cycle fault also clears itself after
    // kFaultAutoRecoverMs, so a cell with no acknowledge wired is a working configuration.
    void test_an_unmapped_error_reset_is_supported_and_does_not_fail_setup()
    {
        LocalizationRuntimeFixture fixture;
        auto ctx = fixture.context();
        ctx.config.setbErrorReset(QString());

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
    }

    void test_an_orphan_tag_fails_setup_on_required_and_optional_signals_alike()
    {
        // M9999 is outside VirtualPlcDevice's offered range whatever its configured tag count is
        // within the sane defaults, so the device does not provide it.
        {
            LocalizationRuntimeFixture fixture;
            auto ctx = fixture.context();
            ctx.config.setbTaskReady(QStringLiteral("M9999"));
            const auto setup = fixture.controller.setup(ctx);
            const QString errors = setup.errors.join(QStringLiteral("; "));
            QVERIFY(!setup.valid);
            QVERIFY2(errors.contains(QStringLiteral("M9999")) &&
                         errors.contains(QStringLiteral("bTaskReady")),
                     qPrintable(errors));
        }
        {
            LocalizationRuntimeFixture fixture;
            auto ctx = fixture.context();
            ctx.config.setbMatchingLowArea(QStringLiteral("M9999"));
            const auto setup = fixture.controller.setup(ctx);
            const QString errors = setup.errors.join(QStringLiteral("; "));
            QVERIFY2(!setup.valid,
                     "an orphan tag is a hard error on an OPTIONAL signal too: the tag is wrong, "
                     "which is a different statement from the signal being unused");
            QVERIFY2(errors.contains(QStringLiteral("M9999")), qPrintable(errors));
        }
    }

    void test_a_bit_signal_mapped_to_a_word_tag_is_an_orphan_for_that_signal()
    {
        LocalizationRuntimeFixture fixture;
        auto ctx = fixture.context();
        // D0 exists on the device — as a REGISTER. For a bit signal it is still an orphan, and
        // checking against the union of both tag lists would let exactly this mistake through.
        ctx.config.setbTaskReady(QStringLiteral("D0"));

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY(!setup.valid);
        QVERIFY2(setup.errors.join(QStringLiteral("; ")).contains(QStringLiteral("D0")),
                 qPrintable(setup.errors.join(QStringLiteral("; "))));
    }

    void test_two_signals_mapped_to_one_tag_fail_setup()
    {
        LocalizationRuntimeFixture fixture;
        auto ctx = fixture.context();
        ctx.config.setbMatchingBusy(ctx.config.bTaskReady());

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY2(!setup.valid,
                 "two signals on one tag is a wiring fault: the mapper keeps the first mapping "
                 "and the second signal simply never arrives");
        QVERIFY2(setup.errors.join(QStringLiteral("; ")).contains(ctx.config.bTaskReady()),
                 qPrintable(setup.errors.join(QStringLiteral("; "))));
    }

    // setup() reset m_lastExecuteTrigger and never m_lastErrorReset — two edge-detected inputs
    // initialised by different rules for no stated reason (backlog item 54's surviving half).
    // The consequence: an acknowledge left asserted across a restart is not seen as a rising edge,
    // so the first acknowledge of the new session is swallowed.
    void test_setup_resets_the_error_reset_edge_detector()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        QVERIFY(fixture.controller.setup(fixture.context()).valid);
        fixture.controller.setActiveCameraNumber(5);          // refused -> faulted
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        fixture.controller.handlePlcValues({{QStringLiteral("M19"), true}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), false);

        // A fresh session. The acknowledge input is still asserted from the operator's point of
        // view, and the runtime must treat the next true as a new edge.
        QVERIFY(fixture.controller.setup(fixture.context()).valid);
        fixture.controller.setActiveCameraNumber(5);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);

        fixture.controller.handlePlcValues({{QStringLiteral("M19"), true}});
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), false);
    }

    // ── Found by the 2026-09-08 field run of C4, not by the suite ────────────────────────
    //
    // The gate refused the runtime correctly, but its message went only to the developer log:
    // the operator saw "Runtime start aborted: setupTask failed", which names nothing. A refusal
    // nobody can read is barely better than the silent start the gate replaced.
    // ── Phase 9 / C6(a): one snapshot question, every PLC family ─────────────────────────
    //
    // PlcValueMap was an empty polymorphic base with only clone(). It now answers "what do you
    // hold for this tag?", which is what lets setup() resolve the active selection from the PLC
    // that owns those registers instead of from the project file.
    //
    // **The distinction these cases exist for is absent vs zero.** A register that was never
    // polled holds nothing; one the master parked at 0 holds a value. A runtime that reads the
    // first as 0 selects camera 0 — and 0 names no camera. That is backlog item 58.

    void test_virtual_plc_snapshot_distinguishes_an_undriven_tag_from_a_zero_one()
    {
        vc::device::VirtualPlcValueMap map({{QStringLiteral("D100"), 0}});

        QVariant value;
        QVERIFY2(map.valueForTag(QStringLiteral("D100"), &value),
                 "a tag driven to 0 HAS a value");
        QCOMPARE(value.toInt(), 0);

        QVariant untouched(QStringLiteral("sentinel"));
        QVERIFY2(!map.valueForTag(QStringLiteral("D101"), &untouched),
                 "a tag nobody drove has no value — answering 0 here is the whole defect");
        QCOMPARE(untouched.toString(), QStringLiteral("sentinel"));
    }

    void test_modbus_register_map_answers_by_tag_per_area()
    {
        using vc::device::ModbusArea;
        vc::device::ModbusRegisterMap map;
        map.configure({0, 4}, {0, 4}, {0, 4}, {0, 4});
        map.setBit(ModbusArea::Coils, 1, true);
        map.setWord(ModbusArea::InputRegisters, 2, 7);

        QVariant value;
        QVERIFY(map.valueForTag(QStringLiteral("COIL00001"), &value));
        QCOMPARE(value.toBool(), true);
        QVERIFY(map.valueForTag(QStringLiteral("IR00002"), &value));
        QCOMPARE(value.toInt(), 7);

        // Configured but never written still counts as held — configure() creates every address
        // at zero, which is what lets the signal-map editor list them before the first poll.
        QVERIFY(map.valueForTag(QStringLiteral("HR00000"), &value));
        QCOMPARE(value.toInt(), 0);

        // Outside the configured span: nothing is known about it.
        QVERIFY(!map.valueForTag(QStringLiteral("HR00099"), &value));
        // Not a tag this family uses at all.
        QVERIFY(!map.valueForTag(QStringLiteral("M0100"), &value));
    }

    void test_mc_device_map_answers_by_tag_and_keeps_bit_and_word_apart()
    {
        vc::device::McDeviceMap map;
        map.device_map_m[100] = 1;
        map.device_map_d[200] = -5;

        QVariant value;
        QVERIFY(map.valueForTag(QStringLiteral("M100"), &value));
        QCOMPARE(value.typeId(), QMetaType::Bool);
        QCOMPARE(value.toBool(), true);

        QVERIFY(map.valueForTag(QStringLiteral("D200"), &value));
        QCOMPARE(value.toInt(), -5);

        QVERIFY2(!map.valueForTag(QStringLiteral("D100"), &value),
                 "D100 was never polled; M100 having a value must not answer for it");
        QVERIFY(!map.valueForTag(QStringLiteral("COIL00001"), &value));
    }

    // ── Phase 9 / C6(b): setup() resolves the selection from the PLC that owns it ────────
    //
    // These four are the acceptance test for the whole phase. The owner's 2026-09-07 report was
    // a task that reached Ready with the master's index registers at 0 and ran cycles on
    // whichever camera sorted first.

    void test_setup_adopts_the_camera_the_plc_commands_over_the_project_default()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        auto ctx = fixture.context();
        ctx.activeCameraNumber = -1;   // no project-side choice; the PLC decides
        ctx.plcSnapshot = std::make_shared<vc::device::VirtualPlcValueMap>(
            QMap<QString, QVariant>{{QStringLiteral("D100"), 2}});

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        // Camera 2 is bound and calibrated in the fixture; firstKey() would have given 1.
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("nActiveCameraStatus")).toInt(), 2, 2000);
    }

    // The owner-reported symptom, and the half of it that was got wrong the first time.
    //
    // A startup index the PLC is holding is a **recoverable** fault, not an unusable
    // configuration: `setup()` stays valid, the selection is refused, and the next good write
    // re-arms with no operator action. Reporting it as a SetupResult error instead left
    // `m_valid` false — and `markRuntimeReady()` gates on `m_valid`, so the runtime could never
    // come back. Field-confirmed 2026-09-08: camera and pattern re-selected and valid, camera
    // reconnected, `bTaskReady` off until the task was stopped.
    void test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy signalSpy(&fixture.controller, &LocalizationRuntimeController::signalChanged);
        QVERIFY(signalSpy.isValid());

        auto ctx = fixture.context();
        ctx.activeCameraNumber = -1;
        ctx.plcSnapshot = std::make_shared<vc::device::VirtualPlcValueMap>(
            QMap<QString, QVariant>{{QStringLiteral("D100"), 0}});

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY2(setup.valid,
                 qPrintable(QStringLiteral("the CONFIGURATION is usable; only the selection was "
                                           "refused: %1").arg(setup.errors.join(QStringLiteral("; ")))));
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskFault")).toBool(), true);
        QCOMPARE(lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), false);

        // The master corrects itself. This is what the field run did and what stayed broken.
        fixture.controller.setActiveCameraNumber(1);
        QTRY_COMPARE_WITH_TIMEOUT(
            lastSignalValue(signalSpy, QStringLiteral("bTaskReady")).toBool(), true, 2000);
    }

    // A cell that never switches cameras must not be broken by any of this.
    void test_setup_falls_back_to_the_project_default_when_the_index_signal_is_unmapped()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy logSpy(&fixture.controller, &LocalizationRuntimeController::taskLogAppended);
        QVERIFY(logSpy.isValid());

        auto ctx = fixture.context();
        ctx.activeCameraNumber = -1;
        ctx.activePatternGroupNumber = -1;
        ctx.config.setnActiveCamera(QString());
        ctx.config.setnActivePatternGroup(QString());
        // A snapshot exists and even holds a 0 under the old tag — it must not be consulted for
        // a signal that is not mapped to it.
        ctx.plcSnapshot = std::make_shared<vc::device::VirtualPlcValueMap>(
            QMap<QString, QVariant>{{QStringLiteral("D100"), 0}});

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY2(setup.valid, qPrintable(setup.errors.join(QStringLiteral("; "))));
        QString summary;
        for (const QList<QVariant> &emission : logSpy) {
            const auto entry = qvariant_cast<LocalizationRuntimeController::TaskLogEntry>(
                emission.at(0));
            if (entry.message.startsWith(QStringLiteral("Runtime selection:"))) {
                summary = entry.message;
                break;
            }
        }
        QVERIFY2(!summary.isEmpty(), "the startup summary must reach the operator's task log");
        QVERIFY2(!summary.contains(QStringLiteral("commanded")), qPrintable(summary));
    }

    // **This distinction is the entire defect** — a PLC that has not been read yet must be
    // treated as unmapped, never as one holding 0.
    void test_a_plc_with_no_snapshot_is_not_read_as_holding_zero()
    {
        LocalizationRuntimeFixture fixture;
        auto ctx = fixture.context();
        ctx.activeCameraNumber = -1;
        ctx.activePatternGroupNumber = -1;
        ctx.plcSnapshot.reset();   // mapped signals, but the PLC has never been read

        const auto setup = fixture.controller.setup(ctx);

        QVERIFY2(setup.valid,
                 qPrintable(QStringLiteral("no snapshot must fall back to the project default, "
                                           "not fault as though the register held 0; got: %1")
                                .arg(setup.errors.join(QStringLiteral("; ")))));
    }

    // The 2026-09-07 field report, end to end and with no hardware: the master is holding 0 in
    // the camera register when the runtime starts. Before Phase 9 / C6 the task reached Ready on
    // whichever camera sorted first and accepted triggers on it.
    //
    // This is the case backlog 43's input injection was resolved for — so that this class of
    // defect stops needing the cell.
    void test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready()
    {
        TaskLocalizationRuntimeFixture fixture;
        QVERIFY2(fixture.parkPlcInput(QStringLiteral("D100"), 0),
                 "the fixture must be able to park a value the way a master holds one");

        fixture.task->beginCommission();
        fixture.task->beginRuntime(false);

        // Faulted, and it must GET there rather than pass through Ready on the way: the fault is
        // delivered by a queued runtimeFault, so this is a QTRY.
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Faulted, 3000);

        // The runtime itself stays usable. That is the difference between "the master is holding
        // a number I cannot use" and "this project cannot run", and conflating them is what made
        // the first cut of this task unrecoverable in the field.
        QVERIFY2(fixture.task->isValid(),
                 "a refused startup index must leave the runtime able to re-arm");

        // And it does re-arm, with no operator action, when the master corrects itself.
        auto *plcRunner = fixture.plcRunner();
        QVERIFY(plcRunner != nullptr);
        plcRunner->requestInjectInputValue(QStringLiteral("D100"), 1);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Ready, 5000);
    }

    void test_a_runtime_started_with_the_master_holding_a_valid_number_adopts_it()
    {
        TaskLocalizationRuntimeFixture fixture;
        QSignalSpy logSpy(fixture.task, &TaskLocalization::taskLogAppended);
        QVERIFY(logSpy.isValid());
        // Camera 2 is bound and calibrated; the project default would have given 1.
        QVERIFY(fixture.parkPlcInput(QStringLiteral("D100"), 2));

        fixture.task->beginCommission();
        fixture.task->beginRuntime(false);
        QTRY_COMPARE_WITH_TIMEOUT(fixture.task->taskState(), TaskState::Ready, 3000);

        QString summary;
        for (const QList<QVariant> &emission : logSpy) {
            const auto entry = qvariant_cast<LocalizationRuntimeController::TaskLogEntry>(
                emission.at(0));
            if (entry.message.startsWith(QStringLiteral("Runtime selection:"))) {
                summary = entry.message;
                break;
            }
        }
        QVERIFY2(summary.contains(QStringLiteral("camera 2 (commanded")), qPrintable(summary));
    }

    // ── Phase 9 / C4, commissioning half: the orphan purge must not run blind ────────────
    //
    // `checkEmpty()` cleared every orphaned row and *then* returned their names, so a caller
    // could not classify them before the damage was done. It had no caller anywhere (backlog
    // item 1) and the blind-clear contract is why: wired at project save it would silently
    // unmap an orphaned REQUIRED signal and brick the next runtime start. It is replaced by a
    // query and a purge, and these two cases are what keep them separate.

    void test_orphan_row_names_reports_orphans_without_clearing_them()
    {
        SignalsMapWidget widget;
        widget.appendRow(QStringLiteral("bTaskReady"), QStringLiteral("Task ready"),
                         SignalsMapWidget::Type::Bool);
        widget.setBoolTags({QStringLiteral("M10"), QStringLiteral("M11")});
        widget.setRowValue(QStringLiteral("bTaskReady"), QStringLiteral("M11"));

        QSignalSpy changeSpy(&widget, &SignalsMapWidget::signalMappingChanged);
        QVERIFY(changeSpy.isValid());

        // The device's range shrinks — M11 is gone. The row orphans.
        widget.setBoolTags({QStringLiteral("M10")});

        QCOMPARE(widget.orphanRowNames(), QStringList{QStringLiteral("bTaskReady")});
        QVERIFY2(widget.rowValue(QStringLiteral("bTaskReady")) == QStringLiteral("M11"),
                 "reporting an orphan must not clear it: the operator has to be able to see "
                 "what the tag used to be in order to decide what to do about it");
        QCOMPARE(changeSpy.count(), 0);
    }

    void test_clear_row_tags_clears_only_the_named_rows()
    {
        SignalsMapWidget widget;
        widget.appendRow(QStringLiteral("bTaskReady"), QStringLiteral("Task ready"),
                         SignalsMapWidget::Type::Bool);
        widget.appendRow(QStringLiteral("bMatchingLowArea"), QStringLiteral("Low area"),
                         SignalsMapWidget::Type::Bool);
        widget.setBoolTags({QStringLiteral("M10"), QStringLiteral("M11")});
        widget.setRowValue(QStringLiteral("bTaskReady"), QStringLiteral("M10"));
        widget.setRowValue(QStringLiteral("bMatchingLowArea"), QStringLiteral("M11"));
        widget.setBoolTags({});   // both orphan

        QCOMPARE(widget.orphanRowNames().size(), 2);

        widget.clearRowTags({QStringLiteral("bMatchingLowArea")});

        QVERIFY2(widget.rowValue(QStringLiteral("bTaskReady")) == QStringLiteral("M10"),
                 "a row the operator chose to keep must survive the purge — this is the whole "
                 "difference from the blind clear that preceded it");
        QVERIFY(widget.rowValue(QStringLiteral("bMatchingLowArea")).isEmpty());
    }

    void test_setup_errors_reach_the_operator_log_naming_the_signal()
    {
        LocalizationRuntimeFixture fixture;
        QSignalSpy logSpy(&fixture.controller, &LocalizationRuntimeController::taskLogAppended);
        QVERIFY(logSpy.isValid());

        auto ctx = fixture.context();
        ctx.config.setbTaskReady(QStringLiteral("M9999"));

        const auto setup = fixture.controller.setup(ctx);
        QVERIFY(!setup.valid);

        bool named = false;
        for (const QList<QVariant> &emission : logSpy) {
            const auto entry = qvariant_cast<LocalizationRuntimeController::TaskLogEntry>(
                emission.at(0));
            if (entry.severity.compare(QStringLiteral("ERROR"), Qt::CaseInsensitive) == 0 &&
                entry.message.contains(QStringLiteral("M9999"))) {
                named = true;
                break;
            }
        }
        QVERIFY2(named,
                 "every reason the runtime refused to start must reach the operator's task log at "
                 "ERROR, naming the tag — not just the developer log");
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
        for (const QString &shellMain : {QStringLiteral("/components/app/main.cpp"),
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
        for (const QString &shellMain : {QStringLiteral("/components/app/main.cpp"),
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
        // runtime_app deliberately: components/app/mainwindow.cpp still has one (the ADS dock
        // host's layout), recorded in the backlog rather than fixed here — widening this check
        // to the editor is a separate change to a window nobody asked to touch.
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
        for (const QString &shell : {QStringLiteral("components/app"), QStringLiteral("runtime_app")}) {
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

        for (const QString &shell : {QStringLiteral("components/app"), QStringLiteral("runtime_app")}) {
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

    // Phase 7 / E4: the translation scan set must stay the whole product.
    //
    // lupdate updates only the .ts files a project lists in TRANSLATIONS, and it scans only
    // that project's own sources. Once E7a moved all of src/ into the ncr_shared static
    // library, the shells compiled a handful of files each — so an update driven from a
    // shell saw ~40 strings out of ~974 and marked the other 861 "vanished". The build was
    // green throughout; the only symptom was the Japanese UI reverting to English.
    //
    // Neither the compiler nor the linker can see this, which is the same reason the
    // resource split above needs a test. The invariant has two halves and both are asserted:
    // no shell may drive lupdate, and the one project that does must reach every source.
    void test_translations_are_updated_from_one_project_that_sees_every_source()
    {
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);

        auto read = [](const QString &path) {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                return QString();
            return QString::fromUtf8(f.readAll());
        };

        // The qmake comments explain this rule and name the very variables searched for.
        // Strip them, or the test passes on prose.
        auto code = [](const QString &text) {
            QStringList out;
            const QStringList lines = text.split(QLatin1Char('\n'));
            for (const QString &line : lines) {
                const int hash = line.indexOf(QLatin1Char('#'));
                out << (hash < 0 ? line : line.left(hash));
            }
            return out.join(QLatin1Char('\n'));
        };

        // "TRANSLATIONS" is a substring of "EXTRA_TRANSLATIONS", so anchor on the
        // assignment and require nothing but whitespace in front of the name.
        static const QRegularExpression declaresTranslations(
            QStringLiteral("(?m)^\\s*TRANSLATIONS\\s*\\+?="));
        static const QRegularExpression declaresExtraTranslations(
            QStringLiteral("(?m)^\\s*EXTRA_TRANSLATIONS\\s*\\+?="));

        // --- No shell may declare TRANSLATIONS. EXTRA_TRANSLATIONS is what they use: Qt's
        // --- lrelease.prf releases and embeds both, but lupdate touches only the former.
        for (const QString &pri : {QStringLiteral("/components/app/app.pri"),
                                   QStringLiteral("/runtime_app/runtime_app.pri")}) {
            const QString text = code(read(repoRoot + pri));
            QVERIFY2(!text.isEmpty(), qPrintable(pri));

            QVERIFY2(!declaresTranslations.match(text).hasMatch(),
                     qPrintable(QStringLiteral("shell .pri declares TRANSLATIONS, so an "
                                               "lupdate run from it will mark every string "
                                               "outside this shell as vanished — use "
                                               "EXTRA_TRANSLATIONS: ") + pri));
            QVERIFY2(declaresExtraTranslations.match(text).hasMatch(),
                     qPrintable(QStringLiteral("shell .pri no longer declares "
                                               "EXTRA_TRANSLATIONS, so it ships no .qm at "
                                               "all: ") + pri));
        }

        // --- One project owns the update, and its scan set is assembled from the same .pri
        // --- files the build uses. A hand-written source list here would be the identical
        // --- failure one module later, silently.
        const QString lupdatePro =
            code(read(repoRoot + QStringLiteral("/translations/ncr_translations.pro")));
        QVERIFY2(!lupdatePro.isEmpty(),
                 "translations/ncr_translations.pro is missing — nothing updates the .ts");
        QVERIFY2(declaresTranslations.match(lupdatePro).hasMatch(),
                 "translations/ncr_translations.pro no longer declares TRANSLATIONS");

        const QStringList mustScan = {
            QStringLiteral("src/core/core.pri"),
            QStringLiteral("src/device/device.pri"),
            QStringLiteral("src/calibration/calibration.pri"),
            QStringLiteral("src/matching/matching.pri"),
            QStringLiteral("src/model/model.pri"),
            QStringLiteral("src/runtime/runtime.pri"),
            QStringLiteral("src/ui/ui.pri"),
            // Not part of any module, compiled into the library, and carrying ~17
            // translatable contexts of its own (QtBoolEdit, QtColorEditWidget, …).
            QStringLiteral("qtpropertybrowser_vendor.pri"),
            QStringLiteral("components/app/app.pri"),
            QStringLiteral("runtime_app/runtime_app.pri"),
        };
        for (const QString &needle : mustScan) {
            QVERIFY2(lupdatePro.contains(needle),
                     qPrintable(QStringLiteral("translations/ncr_translations.pro does not "
                                               "include ") + needle
                                + QStringLiteral(" — every string it holds would be marked "
                                                 "vanished on the next update")));
        }

        // --- The umbrella must list it, or Qt Creator's Update Translations never reaches
        // --- it and the only correct path is a script someone has to know about.
        const QString umbrella =
            code(read(repoRoot + QStringLiteral("/ncr_picking_all.pro")));
        QVERIFY2(umbrella.contains(QStringLiteral("ncr_translations.pro")),
                 "ncr_picking_all.pro does not list the translations project");

        // --- ...and must exclude it from the default build target. The project lists all of
        // --- src/ plus both shells; without this it is a third compilation of everything.
        // --- TEMPLATE = aux does not prevent that, which is why the guard is here.
        static const QRegularExpression translationsNoDefaultTarget(
            QStringLiteral("(?m)^\\s*translations\\.CONFIG\\s*\\+?=[^\\n]*no_default_target"));
        QVERIFY2(translationsNoDefaultTarget.match(umbrella).hasMatch(),
                 "ncr_picking_all.pro no longer excludes the translations project from the "
                 "default target — it would be built, compiling the whole product a third "
                 "time and failing for want of Qt include paths");

        // --- -no-obsolete deletes vanished entries, and a vanished entry still carries its
        // --- translation. On 2026-08-24 that flag would have destroyed 835 recoverable
        // --- Japanese strings. It must not appear in anything that drives lupdate.
        // Both files warn about the flag in prose, so the block comment goes first and the
        // line comments second — otherwise the warning itself would fail the test.
        static const QRegularExpression powerShellBlockComment(
            QStringLiteral("(?s)<#.*?#>"));
        for (const QString &driver : {QStringLiteral("/translations/ncr_translations.pro"),
                                      QStringLiteral("/scripts/update_translations.ps1")}) {
            QString text = read(repoRoot + driver);
            QVERIFY2(!text.isEmpty(), qPrintable(driver));
            text = code(text.remove(powerShellBlockComment));
            QVERIFY2(!text.contains(QStringLiteral("-no-obsolete")),
                     qPrintable(QStringLiteral("-no-obsolete deletes vanished entries along "
                                               "with their translations: ") + driver));
        }
    }

    // Phase 7 / E5: translating this product's strings is only half the UI.
    //
    // "OK", "Cancel" and "Close" on every QMessageBox and QDialogButtonBox belong to Qt, not
    // to components/app/translations/ncr_picking_ja_JP.ts, and no amount of lupdate work will
    // ever put them there. They come from Qt's own catalogs, which something has to load — and
    // until E5 nothing did, in either shell. The report that surfaced it was an operator seeing
    // three English buttons in an otherwise Japanese UI.
    //
    // This asserts the behaviour rather than the wiring: it installs Japanese and then asks
    // Qt for a standard button label. "QPlatformTheme" is the context QDialogButtonBox gets
    // its standard button text from.
    //
    // The probe is "Close", NOT "OK". Qt's Japanese catalog translates OK -> "OK", because
    // that is the Japanese convention — so the button the defect was reported against can
    // never demonstrate the fix, and a test written around it would pass only while broken
    // and fail once correct. Close -> 閉じる is the same context, same catalog, and actually
    // changes.
    void test_qt_own_translations_are_installed_not_just_the_application_catalog()
    {
        const QString before = QCoreApplication::translate("QPlatformTheme", "Close");
        QCOMPARE(before, QStringLiteral("Close"));  // Baseline: nothing installed yet.

        const vc::core::TranslationLoadResult result =
            vc::core::installTranslations(*qApp, QStringLiteral("ja_JP"));

        // Undo before asserting. A failed assertion returns immediately, and leaving the
        // suite running in Japanese would break every later test that compares English text
        // — turning one real failure into a dozen misleading ones.
        struct Restore {
            const vc::core::TranslationLoadResult &r;
            ~Restore() {
                if (r.applicationTranslator) qApp->removeTranslator(r.applicationTranslator);
                if (r.qtTranslator)          qApp->removeTranslator(r.qtTranslator);
            }
        } restore{ result };

        QVERIFY2(result.qtLoaded,
                 "no Qt catalog was found. Qt ships qtbase_<locale>.qm in its own "
                 "translations directory and windeployqt writes a merged qt_<locale>.qm "
                 "into the deployed folder — installTranslations() must find one of them");
        QVERIFY(!result.qtCatalog.isEmpty());

        const QString translated = QCoreApplication::translate("QPlatformTheme", "Close");
        QVERIFY2(translated != before,
                 qPrintable(QStringLiteral("a Qt catalog was installed but standard button "
                                           "text is still English — the file that loaded "
                                           "does not actually carry QPlatformTheme, or its "
                                           "dependencies were not resolvable beside it: ")
                            + result.qtCatalog));
    }

    // ...and the loading itself lives in ONE place. Both shells had their own copy of the
    // translator block, which is how one of them could be fixed and the other left behind.
    void test_both_shells_load_translations_through_the_shared_helper()
    {
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);

        for (const QString &shellMain : {QStringLiteral("/components/app/main.cpp"),
                                         QStringLiteral("/runtime_app/src/main.cpp")}) {
            QFile file(repoRoot + shellMain);
            QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(shellMain));
            const QString text = QString::fromUtf8(file.readAll());

            QVERIFY2(text.contains(QStringLiteral("installTranslations")),
                     qPrintable(QStringLiteral("shell does not install translations through "
                                               "the shared helper: ") + shellMain));
            QVERIFY2(!text.contains(QStringLiteral("QTranslator")),
                     qPrintable(QStringLiteral("shell builds its own QTranslator again — the "
                                               "duplication that let Qt's catalogs go "
                                               "unloaded in both shells at once: ")
                                + shellMain));
        }
    }

    // Phase 7 / E6a: reading "<prop>_name" was copied into six places — the helper below
    // plus five property browsers — and every copy translated the enum keys beside it while
    // none translated the label. They now all call this one function, so this pins what it
    // returns.
    //
    // With no translator installed, QCoreApplication::translate() returns its source, so the
    // registered Q_CLASSINFO value must come back verbatim. That is the "behaviour unchanged"
    // half of E6a; the translated half arrives with E6b's marker tables.
    void test_reflected_display_name_resolves_through_one_helper()
    {
        const QMetaObject &meta = TaskLocalizeConfig::staticMetaObject;

        QCOMPARE(vc::gadget_meta::displayName(meta, "nActiveCamera"),
                 QStringLiteral("Camera selection"));
        QCOMPARE(vc::gadget_meta::displayName(meta, "bErrorReset"),
                 QStringLiteral("Error reset"));

        // No Q_CLASSINFO entry: falls back to the property name, which is what the property
        // browser had already created the row with.
        QCOMPARE(vc::gadget_meta::displayName(meta, "noSuchProperty"),
                 QStringLiteral("noSuchProperty"));

        // The context the markers are written against. A marker recorded under any other
        // context can never be found at runtime.
        QCOMPARE(QString::fromLatin1(meta.className()),
                 QStringLiteral("vc::model::TaskLocalizeConfig"));
    }

    // Phase 7 / E6c: every Q_CLASSINFO display name must have a translation marker, under
    // the right context.
    //
    // Q_CLASSINFO is invisible to lupdate, so each class carries a kDisplayNameSources table
    // of QT_TRANSLATE_NOOP entries purely so the strings can be extracted. That is a second
    // list of the same data, and a second list drifts — a property added with a display name
    // and no marker simply stays English, with nothing to notice it. This is the check that
    // notices.
    //
    // Both directions are asserted, because they fail differently: a missing marker means an
    // untranslatable label, a stale marker means a .ts entry nothing will ever ask for.
    void test_every_display_name_has_a_translation_marker()
    {
        struct Spec {
            const QMetaObject *meta;
            const char *const *sources;
            int sourceCount;
            const char *header;
        };

        const QList<Spec> specs = {
            { &TaskLocalizeConfig::staticMetaObject,
              TaskLocalizeConfig::kDisplayNameSources,
              int(std::size(TaskLocalizeConfig::kDisplayNameSources)),
              "/src/model/task_localization_config.h" },
            { &vc::model::ITask::staticMetaObject,
              vc::model::ITask::kDisplayNameSources,
              int(std::size(vc::model::ITask::kDisplayNameSources)),
              "/src/model/itask.h" },
            { &vc::device::IDevice::staticMetaObject,
              vc::device::IDevice::kDisplayNameSources,
              int(std::size(vc::device::IDevice::kDisplayNameSources)),
              "/src/device/idevice.h" },
            { &vc::device::BaslerGigeCfg::staticMetaObject,
              vc::device::BaslerGigeCfg::kDisplayNameSources,
              int(std::size(vc::device::BaslerGigeCfg::kDisplayNameSources)),
              "/src/device/camera/camera_basler_gige.h" },
            { &vc::device::JaiGigeCfg::staticMetaObject,
              vc::device::JaiGigeCfg::kDisplayNameSources,
              int(std::size(vc::device::JaiGigeCfg::kDisplayNameSources)),
              "/src/device/camera/camera_jai_gige.h" },
            { &vc::device::VirtualCameraCfg::staticMetaObject,
              vc::device::VirtualCameraCfg::kDisplayNameSources,
              int(std::size(vc::device::VirtualCameraCfg::kDisplayNameSources)),
              "/src/device/virtual/virtual_camera_config.h" },
            { &vc::device::VirtualPlcCfg::staticMetaObject,
              vc::device::VirtualPlcCfg::kDisplayNameSources,
              int(std::size(vc::device::VirtualPlcCfg::kDisplayNameSources)),
              "/src/device/virtual/virtual_plc_config.h" },
            { &vc::device::VisionTcpipDeviceCfg::staticMetaObject,
              vc::device::VisionTcpipDeviceCfg::kDisplayNameSources,
              int(std::size(vc::device::VisionTcpipDeviceCfg::kDisplayNameSources)),
              "/src/device/output_device/vision_tcpip_config.h" },
            { &vc::device::VisionTcpipClientDeviceCfg::staticMetaObject,
              vc::device::VisionTcpipClientDeviceCfg::kDisplayNameSources,
              int(std::size(vc::device::VisionTcpipClientDeviceCfg::kDisplayNameSources)),
              "/src/device/output_device/vision_tcpip_client_config.h" },
            { &vc::device::McContext::staticMetaObject,
              vc::device::McContext::kDisplayNameSources,
              int(std::size(vc::device::McContext::kDisplayNameSources)),
              "/src/device/plc/mc_context.h" },
            { &vc::device::McMsgItfConfig::staticMetaObject,
              vc::device::McMsgItfConfig::kDisplayNameSources,
              int(std::size(vc::device::McMsgItfConfig::kDisplayNameSources)),
              "/src/device/plc/mc_msg_interface.h" },
            { &vc::device::McMsgEthernetTcpCfg::staticMetaObject,
              vc::device::McMsgEthernetTcpCfg::kDisplayNameSources,
              int(std::size(vc::device::McMsgEthernetTcpCfg::kDisplayNameSources)),
              "/src/device/plc/mc_msg_tcp_client.h" },
            { &vc::device::McMsgSerialCfg::staticMetaObject,
              vc::device::McMsgSerialCfg::kDisplayNameSources,
              int(std::size(vc::device::McMsgSerialCfg::kDisplayNameSources)),
              "/src/device/plc/mc_msg_serial_port.h" },
            { &vc::device::Context_Mc1C::staticMetaObject,
              vc::device::Context_Mc1C::kDisplayNameSources,
              int(std::size(vc::device::Context_Mc1C::kDisplayNameSources)),
              "/src/device/plc/mc_context_1c.h" },
            { &vc::device::Context_Mc3C::staticMetaObject,
              vc::device::Context_Mc3C::kDisplayNameSources,
              int(std::size(vc::device::Context_Mc3C::kDisplayNameSources)),
              "/src/device/plc/mc_context_3c.h" },
            { &vc::device::ModbusConfig::staticMetaObject,
              vc::device::ModbusConfig::kDisplayNameSources,
              int(std::size(vc::device::ModbusConfig::kDisplayNameSources)),
              "/src/device/plc/modbus/modbus_config.h" },
            { &vc::device::ModbusTcpClientCfg::staticMetaObject,
              vc::device::ModbusTcpClientCfg::kDisplayNameSources,
              int(std::size(vc::device::ModbusTcpClientCfg::kDisplayNameSources)),
              "/src/device/plc/modbus/modbus_tcp_client_config.h" },
            { &vc::device::ModbusTcpServerCfg::staticMetaObject,
              vc::device::ModbusTcpServerCfg::kDisplayNameSources,
              int(std::size(vc::device::ModbusTcpServerCfg::kDisplayNameSources)),
              "/src/device/plc/modbus/modbus_tcp_server_config.h" },
        };

        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);
        int totalNames = 0;

        for (const Spec &spec : specs) {
            const QString className = QString::fromLatin1(spec.meta->className());

            // Only this class's own entries: indexOfClassInfo() walks base classes, but the
            // marker table is per-class and a base declares its own.
            QSet<QString> declared;
            for (int i = spec.meta->classInfoOffset(); i < spec.meta->classInfoCount(); ++i) {
                const QMetaClassInfo info = spec.meta->classInfo(i);
                const QString key = QString::fromLatin1(info.name());
                if (key.endsWith(QLatin1String("_name")))
                    declared.insert(QString::fromUtf8(info.value()));
            }

            QSet<QString> marked;
            for (int i = 0; i < spec.sourceCount; ++i)
                marked.insert(QString::fromUtf8(spec.sources[i]));

            const QSet<QString> missing = declared - marked;
            const QSet<QString> stale = marked - declared;

            QVERIFY2(missing.isEmpty(),
                     qPrintable(className + QStringLiteral(": display name(s) with no "
                                                           "QT_TRANSLATE_NOOP marker, so they "
                                                           "can never be translated: ")
                                + QStringList(missing.values()).join(QLatin1String(", "))));
            QVERIFY2(stale.isEmpty(),
                     qPrintable(className + QStringLiteral(": marker(s) matching no display "
                                                           "name — a .ts entry nothing will "
                                                           "ask for: ")
                                + QStringList(stale.values()).join(QLatin1String(", "))));
            QVERIFY2(!declared.isEmpty(), qPrintable(className));
            totalNames += declared.size();

            // The context must be the class name vc::gadget_meta::displayName() looks up
            // with. This is read from the source, because the context is a plain string that
            // only lupdate ever sees — nothing in the compiled program can check it.
            QFile header(repoRoot + QString::fromLatin1(spec.header));
            QVERIFY2(header.open(QIODevice::ReadOnly | QIODevice::Text),
                     qPrintable(QString::fromLatin1(spec.header)));
            const QString text = QString::fromUtf8(header.readAll());

            static const QRegularExpression noop(
                QStringLiteral("QT_TRANSLATE_NOOP\\(\"([^\"]+)\""));
            auto it = noop.globalMatch(text);
            int contexts = 0;
            while (it.hasNext()) {
                const QString context = it.next().captured(1);
                QVERIFY2(context == className,
                         qPrintable(QStringLiteral("%1: marker context \"%2\" is not the "
                                                   "class name \"%3\"; it would extract and "
                                                   "translate, and never be found at runtime")
                                        .arg(QString::fromLatin1(spec.header), context,
                                             className)));
                ++contexts;
            }
            QCOMPARE(contexts, spec.sourceCount);
        }

        // Guards the inventory itself: a whole config class added with no entry in the table
        // above would otherwise pass silently.
        // 122 → 124: Phase 9 / C3 adds "Active camera (status)" and
        // "Active pattern group (status)" to TaskLocalizeConfig.
        QCOMPARE(totalNames, 124);
    }

    // Phase 8 / A2: the computer-link contexts must survive a save/load round trip and must
    // not be interchangeable with each other or with 3E.
    //
    // A context is what a commissioned project stores about how to reach the PLC. Two failures
    // matter and neither shows up as a crash: a field that does not round-trip comes back as a
    // default (a station number silently reset to 0 addresses the wrong PLC), and a context
    // that accepts another frame's document loads a 3E setup into a 1C device and then talks
    // nonsense down the wire.
    void test_computer_link_contexts_round_trip_and_stay_distinct()
    {
        using namespace vc::device;

        Context_Mc1C c1;
        QCOMPARE(c1.frameType(), mc::McFrameType::Frame_1C);
        QCOMPARE(c1.msgIntefaceType(), mc::McMsgItfType::SerialPort);
        c1.m_stationNumber = 5;
        c1.m_plcNumber = 0x10;
        c1.m_messageWaitTime = 3;
        c1.m_useSumCheck = true;
        c1.m_frameFormat = mc::McFrameFormat::Format_1;
        c1.m_startMAddress = 1000;
        static_cast<McMsgSerialCfg *>(c1.msgConfig())->m_portName = QStringLiteral("COM4");

        Context_Mc1C c1Loaded;
        QVERIFY(c1Loaded.fromJson(c1.toJson()));
        QCOMPARE(c1Loaded.m_stationNumber, 5);
        QCOMPARE(c1Loaded.m_plcNumber, 0x10);
        QCOMPARE(c1Loaded.m_messageWaitTime, 3);
        QCOMPARE(c1Loaded.m_useSumCheck, true);
        QCOMPARE(c1Loaded.m_frameFormat, mc::McFrameFormat::Format_1);
        QCOMPARE(c1Loaded.m_startMAddress, 1000);
        QCOMPARE(static_cast<McMsgSerialCfg *>(c1Loaded.msgConfig())->m_portName,
                 QStringLiteral("COM4"));

        Context_Mc3C c3;
        QCOMPARE(c3.frameType(), mc::McFrameType::Frame_3C);
        QCOMPARE(c3.msgIntefaceType(), mc::McMsgItfType::SerialPort);
        c3.m_plcSeries = mc::McPlcSeries::PlcSeries_iQR;
        c3.m_stationNumber = 2;
        c3.m_networkNumber = 1;
        c3.m_pcNumber = 0x0A;
        c3.m_selfStationNumber = 7;
        c3.m_useSumCheck = true;

        Context_Mc3C c3Loaded;
        QVERIFY(c3Loaded.fromJson(c3.toJson()));
        QCOMPARE(c3Loaded.m_plcSeries, mc::McPlcSeries::PlcSeries_iQR);
        QCOMPARE(c3Loaded.m_stationNumber, 2);
        QCOMPARE(c3Loaded.m_networkNumber, 1);
        QCOMPARE(c3Loaded.m_pcNumber, 0x0A);
        QCOMPARE(c3Loaded.m_selfStationNumber, 7);
        QCOMPARE(c3Loaded.m_useSumCheck, true);

        // clone() must be a DEEP copy: the implicit one shares the shared_ptr to the serial
        // config, so two "independent" contexts would write each other's port settings.
        // Context_Mc3E has exactly that behaviour today (backlog #46) — this asserts the new
        // contexts did not inherit it by writing clone() the same way.
        std::unique_ptr<McContext> c1Clone(c1.clone());
        QVERIFY(c1Clone->msgConfig() != c1.msgConfig());
        static_cast<McMsgSerialCfg *>(c1Clone->msgConfig())->m_portName =
            QStringLiteral("COM9");
        QCOMPARE(static_cast<McMsgSerialCfg *>(c1.msgConfig())->m_portName,
                 QStringLiteral("COM4"));

        // The factory answers for the frames that exist and keeps refusing 1E, which is still
        // unimplemented. A non-null 1E context would let a device connect and then build
        // frames with a codec that does not exist.
        QVERIFY(Factory::contextFactory(mc::McFrameType::Frame_1C) != nullptr);
        QVERIFY(Factory::contextFactory(mc::McFrameType::Frame_3C) != nullptr);
        QVERIFY(Factory::contextFactory(mc::McFrameType::Frame_3E) != nullptr);
        QVERIFY(Factory::contextFactory(mc::McFrameType::Frame_1E) == nullptr);
        QVERIFY(Factory::contextFactory(mc::McFrameType::Frame_User) == nullptr);
        QCOMPARE(Factory::contextFactory(mc::McFrameType::Frame_1C)->frameType(),
                 mc::McFrameType::Frame_1C);
        QCOMPARE(Factory::contextFactory(mc::McFrameType::Frame_3C)->frameType(),
                 mc::McFrameType::Frame_3C);

        // A whole config round trip picks the context back up by its recorded frame type. This
        // is the path a saved project actually takes.
        McProtocolConfig cfg;
        QVERIFY(cfg.configMcProtocol(mc::McFrameType::Frame_3C));
        QCOMPARE(cfg.currentFrameType(), mc::McFrameType::Frame_3C);
        static_cast<Context_Mc3C *>(cfg.context())->m_stationNumber = 11;

        McProtocolConfig cfgLoaded;
        QVERIFY(cfgLoaded.fromJson(cfg.toJson()));
        QCOMPARE(cfgLoaded.currentFrameType(), mc::McFrameType::Frame_3C);
        QCOMPARE(static_cast<Context_Mc3C *>(cfgLoaded.context())->m_stationNumber, 11);

        // The computer-link frames are ASCII on the wire; a context that came back Binary
        // would build frames the PLC cannot read.
        QCOMPARE(Context_Mc1C().dataCode(), mc::McDataCode::Ascii);
        QCOMPARE(Context_Mc3C().dataCode(), mc::McDataCode::Ascii);
    }

    // Phase 8 / A1: the serial transport must honour the McMsgInterface contract that
    // McProtocolDevice relies on, without a serial port attached.
    //
    // The device's send/receive/retry logic is transport-agnostic by design: it calls
    // SetConfig / ConnectToPort / SendMsg / ReceiveMsg / DestroyMsgPort and reads
    // GetErrorDescription(). Every one of those has a failure path that must report rather
    // than throw or wedge — a transport that returns "connected" from a port that never
    // opened produces a device that polls forever into nothing, which reads on screen as a
    // dead PLC rather than as a misconfiguration.
    void test_mc_serial_transport_honours_the_message_interface_contract()
    {
        vc::device::McMsgSerialCfg cfg;
        QCOMPARE(cfg.type(), vc::device::mc::McMsgItfType::SerialPort);

        // Enums persist as key names, so reordering an enumerator cannot silently change what
        // a saved project means. A numeric round-trip would pass this test and still corrupt
        // a customer's line settings on the next release.
        cfg.m_portName = QStringLiteral("COM7");
        cfg.m_baudRate = 19200;
        cfg.m_dataBits = vc::device::mc::McSerialDataBits::DataBits_8;
        cfg.m_parity = vc::device::mc::McSerialParity::Parity_Odd;
        cfg.m_stopBits = vc::device::mc::McSerialStopBits::StopBits_Two;
        cfg.m_flowControl = vc::device::mc::McSerialFlowControl::FlowControl_Hardware;

        const QJsonObject json = cfg.toJson();
        QCOMPARE(json["dataBits"].toString(), QStringLiteral("DataBits_8"));
        QCOMPARE(json["parity"].toString(), QStringLiteral("Parity_Odd"));

        vc::device::McMsgSerialCfg restored;
        QVERIFY(restored.fromJson(json));
        QCOMPARE(restored.m_portName, QStringLiteral("COM7"));
        QCOMPARE(restored.m_baudRate, 19200);
        QCOMPARE(restored.m_dataBits, vc::device::mc::McSerialDataBits::DataBits_8);
        QCOMPARE(restored.m_parity, vc::device::mc::McSerialParity::Parity_Odd);
        QCOMPARE(restored.m_stopBits, vc::device::mc::McSerialStopBits::StopBits_Two);
        QCOMPARE(restored.m_flowControl,
                 vc::device::mc::McSerialFlowControl::FlowControl_Hardware);

        // An unknown key keeps the current value instead of collapsing to the zero
        // enumerator: a half-written document must not silently reconfigure the line.
        QJsonObject damaged = json;
        damaged["parity"] = QStringLiteral("Parity_Nonsense");
        vc::device::McMsgSerialCfg tolerant;
        tolerant.m_parity = vc::device::mc::McSerialParity::Parity_Even;
        QVERIFY(tolerant.fromJson(damaged));
        QCOMPARE(tolerant.m_parity, vc::device::mc::McSerialParity::Parity_Even);

        vc::device::McMsgSerialPort port;
        QCOMPARE(port.type(), vc::device::mc::McMsgItfType::SerialPort);
        QVERIFY(port.ioDevice() != nullptr);

        // A config of the wrong transport must be refused, not cast blindly: the two configs
        // are unrelated types and the static_cast behind an accepted mismatch is undefined
        // behaviour, not a wrong IP.
        vc::device::McMsgEthernetTcpCfg tcpCfg;
        QVERIFY(!port.SetConfig(&tcpCfg));
        QVERIFY(!port.SetConfig(nullptr));
        QVERIFY(port.SetConfig(&cfg));

        // No port configured, and a port that does not exist: both must fail visibly.
        vc::device::McMsgSerialCfg unset;
        QVERIFY(port.SetConfig(&unset));
        QCOMPARE(port.ConnectToPort(), vc::device::McMsgInterface::MsgIfState::ConnectFail);
        QVERIFY(!port.GetErrorDescription().isEmpty());
        QVERIFY(!port.ConnectionCheck());

        vc::device::McMsgSerialCfg absent;
        absent.m_portName = QStringLiteral("COM_DOES_NOT_EXIST");
        QVERIFY(port.SetConfig(&absent));
        QCOMPARE(port.ConnectToPort(), vc::device::McMsgInterface::MsgIfState::ConnectFail);
        QVERIFY(!port.GetErrorDescription().isEmpty());
        QVERIFY(!port.ConnectionCheck());

        // Writing to a port that never opened is an error, never a silent success — the
        // device treats NoError as "sent" and would then wait out the full response timeout
        // on every single request.
        QByteArray payload("\x05""00FF", 5);
        QCOMPARE(port.SendMsg(payload),
                 vc::device::McMsgInterface::MsgErrorState::ErrorOcurred);
        QByteArray empty;
        QCOMPARE(port.SendMsg(empty),
                 vc::device::McMsgInterface::MsgErrorState::BufferEmpty);

        // Teardown is synchronous and idempotent: it runs from the device's own worker
        // thread during a phase teardown, when the event loop may already be stopped.
        port.DestroyMsgPort();
        QVERIFY(port.ioDevice() == nullptr);
        QCOMPARE(port.ConnectToPort(), vc::device::McMsgInterface::MsgIfState::ConnectFail);
        port.DestroyMsgPort();
    }

    // Phase 7 / E6c: enum labels are translated in the enum's own scope, not the config
    // class's. Every property browser used to pass meta.className(), which for every enum in
    // this project is a different string from the one lupdate recorded — so a translated key
    // could never be found. This pins the two together.
    void test_enum_key_translation_scope_matches_the_marker_context()
    {
        QCOMPARE(QString::fromLatin1(
                     QMetaEnum::fromType<vc::device::basler::BaslerExposureMode>().scope()),
                 QStringLiteral("vc::device::basler"));
        QCOMPARE(QString::fromLatin1(
                     QMetaEnum::fromType<vc::device::mc::McFrameType>().scope()),
                 QStringLiteral("vc::device::mc"));

        // ...and that is NOT the config class the browsers used to pass.
        QVERIFY(QString::fromLatin1(vc::device::BaslerGigeCfg::staticMetaObject.className())
                != QStringLiteral("vc::device::basler"));

        // The lookup helper must be the one place doing this, so the five browsers cannot
        // drift back to className().
        const QString repoRoot = QStringLiteral(NCR_REPO_ROOT);
        for (const QString &widget :
             {QStringLiteral("/src/ui/forms/task_widget.h"),
              QStringLiteral("/src/ui/forms/camera/basler_camera_widget.cpp"),
              QStringLiteral("/src/ui/forms/plc/mitsubishi_mc_device_widget.cpp"),
              QStringLiteral("/src/ui/forms/vision_output/vision_tcpip_device_widget.cpp"),
              QStringLiteral("/src/ui/forms/vision_output/vision_tcpip_client_device_widget.cpp")}) {
            QFile file(repoRoot + widget);
            QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(widget));
            const QString text = QString::fromUtf8(file.readAll());
            QVERIFY2(!text.contains(QStringLiteral("translate(meta.className()")),
                     qPrintable(QStringLiteral("property browser translates in the config "
                                               "class's context again, which is never where "
                                               "the enum markers live: ") + widget));
        }
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
    // Scope: quoted includes whose first path segment is a known module name, or that start
    // with a shell's directory ("components/app/..." is the "app" module).
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

        // Where each shell lives; every other module lives under src/<name>. For the
        // commissioning shell the MODULE NAME and the DIRECTORY differ: it lives at
        // components/app/ but its module is still "app", because these rules are about which
        // code may reach which — not about folders.
        const QHash<QString, QString> shellDirs = {
            { QStringLiteral("app"), QStringLiteral("components/app") },
            { QStringLiteral("runtime_app"), QStringLiteral("runtime_app") },
        };

        for (const QString &module : modules) {
            const QString dirPath = shellDirs.contains(module)
                ? repoRoot + QStringLiteral("/") + shellDirs.value(module)
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
                    QString target = inc.left(slash);
                    // A shell whose directory is not its module name is reached by an include
                    // that starts with the DIRECTORY: "components/app/mainwindow.h". Its first
                    // segment is "components", which is no module, so without this mapping the
                    // check below would skip it — and the peer rule would be blind to exactly
                    // the include it exists to refuse, while still passing.
                    for (auto shell = shellDirs.cbegin(); shell != shellDirs.cend(); ++shell) {
                        if (inc.startsWith(shell.value() + QLatin1Char('/')))
                            target = shell.key();
                    }
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
