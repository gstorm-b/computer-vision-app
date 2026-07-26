#include "UnitsTests.h"

#include <RobotKinematics/Core/Units.h>

#include <QtTest/QtTest>

#include <cmath>

namespace {
/// Reference value of pi used to check units::deg()/toDeg() conversions against a known
/// high-precision constant rather than std::acos(-1) or similar.
constexpr double kPi = 3.141592653589793238462643383279502884;
}

void UnitsTests::convertsMillimetersToMeters()
{
    QCOMPARE(RobotKinematics::units::mm(0.0), 0.0);
    QCOMPARE(RobotKinematics::units::mm(1000.0), 1.0);
    QCOMPARE(RobotKinematics::units::mm(-250.0), -0.25);
}

void UnitsTests::convertsMetersToMillimeters()
{
    QCOMPARE(RobotKinematics::units::toMm(0.0), 0.0);
    QCOMPARE(RobotKinematics::units::toMm(1.0), 1000.0);
    QCOMPARE(RobotKinematics::units::toMm(-0.25), -250.0);
}

void UnitsTests::convertsDegreesToRadians()
{
    QVERIFY(std::abs(RobotKinematics::units::deg(180.0) - kPi) < 1e-12);
    QVERIFY(std::abs(RobotKinematics::units::deg(-90.0) + kPi / 2.0) < 1e-12);
}

void UnitsTests::convertsRadiansToDegrees()
{
    QVERIFY(std::abs(RobotKinematics::units::toDeg(kPi) - 180.0) < 1e-12);
    QVERIFY(std::abs(RobotKinematics::units::toDeg(-kPi / 2.0) + 90.0) < 1e-12);
}

/// Entry point invoked by TestMain to run the UnitsTests suite under QtTest.
/// @return the number of failing test functions (0 on success)
int runUnitsTests(int argc, char** argv)
{
    UnitsTests tests;
    return QTest::qExec(&tests, argc, argv);
}
