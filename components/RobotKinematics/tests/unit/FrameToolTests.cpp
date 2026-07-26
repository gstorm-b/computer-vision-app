#include "FrameToolTests.h"

#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Model/FrameRegistry.h>
#include <RobotKinematics/Model/RobotModelConfig.h>
#include <RobotKinematics/Model/ToolRegistry.h>

#include <QtTest/QtTest>

#include <string>

using namespace RobotKinematics;

void FrameToolTests::toolRegistryAddGetAndDefault()
{
    ToolRegistry registry;
    Tool defaultTool;
    defaultTool.id = "default";
    defaultTool.name = "Default Tool";
    Tool welder;
    welder.id = "welder";
    welder.name = "Welder";
    welder.flangeToTcp = Pose::fromXYZRPY_m_rad(0.0, 0.0, 0.1, 0.0, 0.0, 0.0);

    registry.add(defaultTool);
    registry.add(welder);
    registry.setDefault(ToolId{"default"});

    QVERIFY(registry.contains(ToolId{"welder"}));

    const Result<Tool> got = registry.get(ToolId{"welder"});
    QVERIFY(got.ok());
    QCOMPARE(got.value.name, std::string("Welder"));

    const Result<Tool> def = registry.getDefault();
    QVERIFY(def.ok());
    QCOMPARE(def.value.id, std::string("default"));
}

void FrameToolTests::toolRegistryMissingReturnsToolNotFound()
{
    ToolRegistry registry;
    const Result<Tool> got = registry.get(ToolId{"missing"});

    QVERIFY(!got.ok());
    QCOMPARE(got.status, KinematicsStatus::ToolNotFound);
}

void FrameToolTests::frameRegistryAddAndGet()
{
    FrameRegistry registry;
    UserFrame frame;
    frame.id = "vision";
    frame.parentLinkId = "base_link";
    frame.transform = Pose::fromXYZRPY_m_rad(1.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    registry.add(frame);

    QVERIFY(registry.contains(FrameId{"vision"}));

    const Result<UserFrame> got = registry.get(FrameId{"vision"});
    QVERIFY(got.ok());
    QCOMPARE(got.value.parentLinkId, std::string("base_link"));
}

void FrameToolTests::frameRegistryMissingReturnsFrameNotFound()
{
    FrameRegistry registry;
    const Result<UserFrame> got = registry.get(FrameId{"missing"});

    QVERIFY(!got.ok());
    QCOMPARE(got.status, KinematicsStatus::FrameNotFound);
}

/// Entry point that instantiates FrameToolTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runFrameToolTests(int argc, char** argv)
{
    FrameToolTests tests;
    return QTest::qExec(&tests, argc, argv);
}
