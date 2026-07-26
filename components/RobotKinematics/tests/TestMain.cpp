#include <QtTest/QtTest>

#include <iostream>
#include <string>
#include <vector>

/// Forward declarations of the per-suite QtTest entry points. Each constructs its suite's
/// QObject-derived test class and runs it via QTest::qExec, returning the number of failing
/// test functions (0 = all tests in the suite passed). Defined in the suite's own .cpp file.
int runSmokeTests(int argc, char** argv);
int runDhAdapterTests(int argc, char** argv);
int runUnitsTests(int argc, char** argv);
int runPoseTests(int argc, char** argv);
int runRobotModelConfigTests(int argc, char** argv);
int runRobotModelValidatorTests(int argc, char** argv);
int runJointLimitValidatorTests(int argc, char** argv);
int runFrameToolTests(int argc, char** argv);
int runForwardKinematicsTests(int argc, char** argv);
int runRobot3DVisualizerLogicTests(int argc, char** argv);
int runFrameToolFkTests(int argc, char** argv);
int runIKApiTests(int argc, char** argv);
int runCollisionApiTests(int argc, char** argv);
int runCollisionBackendTests(int argc, char** argv);
int runCollisionProfileValidatorTests(int argc, char** argv);
int runCollisionCheckerTests(int argc, char** argv);
int runCollisionPrimitiveDistanceTests(int argc, char** argv);
int runStlMeshLoaderTests(int argc, char** argv);
int runStlPrimitiveAuthoringHelperTests(int argc, char** argv);
int runIKSolutionRankerTests(int argc, char** argv);
int runNumericalIKSolverTests(int argc, char** argv);
int runPostureResolverTests(int argc, char** argv);
int runCustomPresetTests(int argc, char** argv);
int runCollisionProfileJsonTests(int argc, char** argv);
int runMeshCollisionProfileJsonTests(int argc, char** argv);
int runNachiMeshCollisionTests(int argc, char** argv);
int runIKIntegrationTests(int argc, char** argv);
int runVirtual6DofTestArmTests(int argc, char** argv);
int runNachiMZ04DTests(int argc, char** argv);
int runAnalyticIKSolverTests(int argc, char** argv);
int runUrdfAdapterTests(int argc, char** argv);

namespace {
/// Binds a human-readable suite name to its QtTest entry-point function pointer, so `main` can
/// look suites up by name (for filtering) while iterating them in a fixed list.
struct NamedSuite
{
    const char* name;  ///< Suite name as matched against a command-line filter argument.
    int (*suite)(int, char**);  ///< Entry point that runs the suite and returns its failure count.
};

/// Runs one named suite, printing a "Running .../PASS|FAIL" progress line to stdout around it.
/// @param name display name of the suite, used only for console output
/// @param suite entry-point function pointer to invoke
/// @return the failure count returned by `suite` (0 = all tests passed)
int runSuite(const char* name, int (*suite)(int, char**), int argc, char** argv)
{
    std::cout << "Running " << name << "..." << std::endl;
    const int failures = suite(argc, argv);
    std::cout << name << ": " << (failures == 0 ? "PASS" : "FAIL");
    if (failures != 0) {
        std::cout << " (" << failures << " failing test function(s))";
    }
    std::cout << std::endl;
    return failures;
}

/// True when `filter` is non-empty and exactly equals `suiteName` (used to implement the
/// single-suite-name command-line filter in `main`).
bool suiteMatchesFilter(const char* suiteName, const std::string& filter)
{
    return !filter.empty() && filter == suiteName;
}
}

/// Test-runner entry point: builds the fixed list of known suites, treats any command-line
/// argument matching a suite name as a filter (running only that suite) and forwards all other
/// arguments through to QtTest, then runs the selected suite(s) and OR-reduces their failure
/// counts into the process exit status.
/// @return 0 if every executed suite passed; a non-zero (bitwise-OR'd) value if any suite failed
int main(int argc, char** argv)
{
    const std::vector<NamedSuite> suites = {
        {"SmokeTests", runSmokeTests},
        {"DhAdapterTests", runDhAdapterTests},
        {"UnitsTests", runUnitsTests},
        {"PoseTests", runPoseTests},
        {"RobotModelConfigTests", runRobotModelConfigTests},
        {"RobotModelValidatorTests", runRobotModelValidatorTests},
        {"JointLimitValidatorTests", runJointLimitValidatorTests},
        {"FrameToolTests", runFrameToolTests},
        {"ForwardKinematicsTests", runForwardKinematicsTests},
        {"Robot3DVisualizerLogicTests", runRobot3DVisualizerLogicTests},
        {"FrameToolFkTests", runFrameToolFkTests},
        {"IKApiTests", runIKApiTests},
        {"CollisionApiTests", runCollisionApiTests},
        {"CollisionBackendTests", runCollisionBackendTests},
        {"CollisionProfileValidatorTests", runCollisionProfileValidatorTests},
        {"CollisionCheckerTests", runCollisionCheckerTests},
        {"CollisionPrimitiveDistanceTests", runCollisionPrimitiveDistanceTests},
        {"StlMeshLoaderTests", runStlMeshLoaderTests},
        {"StlPrimitiveAuthoringHelperTests", runStlPrimitiveAuthoringHelperTests},
        {"IKSolutionRankerTests", runIKSolutionRankerTests},
        {"NumericalIKSolverTests", runNumericalIKSolverTests},
        {"PostureResolverTests", runPostureResolverTests},
        {"CustomPresetTests", runCustomPresetTests},
        {"CollisionProfileJsonTests", runCollisionProfileJsonTests},
        {"MeshCollisionProfileJsonTests", runMeshCollisionProfileJsonTests},
        {"NachiMeshCollisionTests", runNachiMeshCollisionTests},
        {"IKIntegrationTests", runIKIntegrationTests},
        {"Virtual6DofTestArmTests", runVirtual6DofTestArmTests},
        {"NachiMZ04DTests", runNachiMZ04DTests},
        {"AnalyticIKSolverTests", runAnalyticIKSolverTests},
        {"UrdfAdapterTests", runUrdfAdapterTests},
    };

    std::string suiteFilter;
    std::vector<char*> forwardedArgs;
    forwardedArgs.reserve(static_cast<std::size_t>(argc));
    if (argc > 0) {
        forwardedArgs.push_back(argv[0]);
    }
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        bool matchedSuite = false;
        for (const NamedSuite& suite : suites) {
            if (argument == suite.name) {
                suiteFilter = argument;
                matchedSuite = true;
                break;
            }
        }
        if (!matchedSuite) {
            forwardedArgs.push_back(argv[index]);
        }
    }

    int forwardedArgc = static_cast<int>(forwardedArgs.size());
    char** forwardedArgv = forwardedArgs.data();

    int status = 0;
    for (const NamedSuite& suite : suites) {
        if (!suiteFilter.empty() && !suiteMatchesFilter(suite.name, suiteFilter)) {
            continue;
        }
        status |= runSuite(suite.name, suite.suite, forwardedArgc, forwardedArgv);
    }
    return status;
}
