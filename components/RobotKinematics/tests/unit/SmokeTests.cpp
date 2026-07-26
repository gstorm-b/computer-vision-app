#include "SmokeTests.h"

#include <QtTest/QtTest>

namespace RobotKinematics {
/// Forward declaration of the library's anchor symbol; called here only to confirm the
/// RobotKinematics library links successfully into the test binary.
int libraryAnchor();
}

void SmokeTests::qtTestHarnessRuns()
{
    QCOMPARE(RobotKinematics::libraryAnchor(), 0);
}

/// Entry point that instantiates SmokeTests and runs it under QTest::qExec.
/// @param argc, argv forwarded to QTest::qExec for command-line test option parsing
/// @return the QtTest process exit code (0 on all tests passing)
int runSmokeTests(int argc, char** argv)
{
    SmokeTests tests;
    return QTest::qExec(&tests, argc, argv);
}
