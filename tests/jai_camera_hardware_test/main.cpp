/**
 * @file main.cpp
 * @brief Hardware-in-the-loop test for JaiGigECamera: connects to a real camera and grabs real
 *        frames.
 *
 * **Why this test exists and why it is not like the others.** Every defect this device has had
 * so far — the GenApi delay-load crash, the `StreamEnable`/`PvAcquisitionStateManager` conflict,
 * the SingleFrame re-arm — was invisible to an offline test and only appeared against the
 * hardware. The turnaround for each was "change code, ask the owner to run the UI, read the
 * application log", which is slow and puts the diagnosis in someone else's hands. This test
 * closes that loop: it drives the shipped device directly and asserts on actual pixels.
 *
 * **It skips, it does not fail, when there is no camera.** A station without one still builds and
 * runs it; every case reports SKIP with the reason. That is deliberate — a red suite on a
 * developer machine without hardware teaches people to ignore red suites.
 *
 * Point it at a camera with `NCR_JAI_TEST_IP` (default 192.168.0.70).
 *
 * @note Not `CONFIG += testcase`: `make check` should not pay the discovery timeout on every
 *       build of a machine that has no camera.
 */

#include <QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSize>
#include <QString>
#include <QThread>

#include <functional>

#include <opencv2/opencv.hpp>

#include "device/camera/camera_jai_gige.h"
#include "device/camera/jai_runtime.h"

namespace {

/// The camera under test; overridable so this runs against whatever is on the bench.
QString testCameraIp()
{
    const QByteArray fromEnv = qgetenv("NCR_JAI_TEST_IP");
    return fromEnv.isEmpty() ? QStringLiteral("192.168.0.70") : QString::fromLocal8Bit(fromEnv);
}

/// Grab timeout used by the tests. Deliberately generous: these cases are diagnosing a link, and
/// a tight bound would turn "slow" and "broken" into the same failure.
constexpr int kGrabTimeoutMs = 3000;

/// How many grabs the stability case runs. Enough to catch a fault that only shows up after the
/// first acquisition cycle — which is exactly the class of bug this device has produced twice.
constexpr int kStabilityGrabs = 10;

/// How long the continuous case streams for, in milliseconds.
constexpr int kContinuousWindowMs = 3000;

/// Frames the continuous case must see in that window. A floor well under the paced ceiling
/// (~9 fps here): the assertion is that streaming streams, not that it hits a given rate.
constexpr int kMinContinuousFrames = 8;

/// Budget for stopContinuousShot() to take effect, in milliseconds. Generous next to the pump's
/// 250 ms retrieve window, tight enough that a non-re-posting implementation blows through it.
constexpr int kStopDeadlineMs = 1500;

}   // namespace

/**
 * @class JaiCameraHardwareTest
 * @brief Connects to a real JAI camera once, then exercises grabbing against it.
 */
class JaiCameraHardwareTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_genicam_runtime_is_resolvable();
    void test_camera_is_connected();
    void test_exposure_and_gain_ranges_are_usable();
    void test_link_diagnostics_report_packet_size_and_counters();
    void test_grab_returns_a_real_frame();
    void test_grab_pixels_are_not_uniform();
    void test_repeated_grabs_all_return_frames();
    void test_grab_still_works_after_an_exposure_change();

    void test_continuous_streams_real_frames();
    void test_continuous_stop_is_honoured_from_another_thread();
    void test_single_shot_works_immediately_after_continuous();
    void test_continuous_start_and_stop_are_idempotent();

    void test_backlight_output_is_configured_on_the_selected_line();
    void test_backlight_polarity_goes_into_the_camera_not_the_value();
    void test_backlight_switches_around_a_grab();

    void test_backlight_override_drives_the_lamp_and_reports_it();
    void test_backlight_override_survives_a_grab();

    void test_connect_pushes_the_configured_settings_onto_the_camera();
    void test_backlight_is_configured_by_connect_alone();

private:
    /// QSKIPs the calling test when the camera never connected. Returns true when it did.
    bool requireCamera();

    /// Runs `body` on a throwaway thread and waits for it, so a test can prove the device thread
    /// is not the only one that can reach the camera. Returns false if the thread did not finish.
    static bool runOnAnotherThread(std::function<void()> body);

    /// Configures the camera's backlight on the first output-capable line, with `invert`.
    /// @return the line name used, or an empty string when the camera exposes no output line.
    QString applyBacklightConfig(bool invert);

    std::unique_ptr<vc::device::JaiGigECamera> m_camera;
    QString m_skipReason;
    bool m_connected{false};
};

void JaiCameraHardwareTest::initTestCase()
{
    const QString ip = testCameraIp();
    qInfo("Target camera: %s (override with NCR_JAI_TEST_IP)", qPrintable(ip));

    QString runtimeDetail;
    if (!vc::device::jai::ensureGenICamRuntime(&runtimeDetail)) {
        m_skipReason = QStringLiteral("eBUS GenICam runtime unavailable: %1").arg(runtimeDetail);
        return;
    }

    m_camera = std::make_unique<vc::device::JaiGigECamera>(QStringLiteral("hw"),
                                                           QStringLiteral("JAI hardware test"));

    vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    cfg.m_ipAddress    = ip;
    cfg.m_grabTimeoutMs = kGrabTimeoutMs;
    m_camera->setJaiGigeConfig(cfg);

    QElapsedTimer connectTime;
    connectTime.start();
    m_connected = m_camera->deviceConnect();
    if (!m_connected) {
        // The device's own reason, not a guess. "No camera reachable" and "the camera is there
        // but another application holds it" both skip this suite, and only one of them means the
        // hardware is absent — reporting the first for both sent a real, connected camera's
        // *Access denied* out as "not reachable", which reads like a network problem.
        m_skipReason = QStringLiteral("camera at %1 unavailable: %2")
                           .arg(ip, m_camera->lastMsg());
        return;
    }
    qInfo("Connected in %lld ms.", connectTime.elapsed());
}

void JaiCameraHardwareTest::cleanupTestCase()
{
    if (m_camera && m_connected) {
        m_camera->deviceDisconnect();
    }
    m_camera.reset();
}

bool JaiCameraHardwareTest::requireCamera()
{
    if (!m_connected) {
        // QSKIP returns from the CALLING function, which is why this helper reports instead.
        return false;
    }
    return true;
}

void JaiCameraHardwareTest::test_genicam_runtime_is_resolvable()
{
    QString detail;
    const bool ready = vc::device::jai::ensureGenICamRuntime(&detail);
    QVERIFY2(ready, qPrintable(QStringLiteral("GenICam runtime not resolvable: %1").arg(detail)));
    qInfo("%s", qPrintable(detail));
}

void JaiCameraHardwareTest::test_camera_is_connected()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    QVERIFY(m_camera->isDeviceConnected());

    const vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    QVERIFY2(!cfg.m_modelName.isEmpty(), "the camera reported no model name");
    QVERIFY2(!cfg.m_pixelFormat.isEmpty(), "the camera reported no pixel format");
    qInfo("Camera: %s %s, pixel format %s",
          qPrintable(cfg.m_modelName),
          qPrintable(cfg.m_serialNumber),
          qPrintable(cfg.m_pixelFormat));
}

void JaiCameraHardwareTest::test_exposure_and_gain_ranges_are_usable()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    const vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();

    // A degenerate range is the signature of a range read that failed — the exact defect that made
    // the property browser show an exposure field pinned to zero. min == max would also be a
    // legitimate reading for a fixed feature, but not for exposure or gain on this camera family.
    QVERIFY2(cfg.m_paramsExposureMin < cfg.m_paramsExposureMax,
             qPrintable(QStringLiteral("exposure range is degenerate: %1..%2")
                            .arg(cfg.m_paramsExposureMin)
                            .arg(cfg.m_paramsExposureMax)));
    QVERIFY2(cfg.m_paramsGainMin < cfg.m_paramsGainMax,
             qPrintable(QStringLiteral("gain range is degenerate: %1..%2")
                            .arg(cfg.m_paramsGainMin)
                            .arg(cfg.m_paramsGainMax)));

    QVERIFY2(cfg.m_paramsExposureTime >= cfg.m_paramsExposureMin
                 && cfg.m_paramsExposureTime <= cfg.m_paramsExposureMax,
             "the exposure read back from the camera lies outside its own reported range");

    qInfo("Exposure %.1f..%.1f = %.1f us | gain %d..%d = %.2f",
          cfg.m_paramsExposureMin, cfg.m_paramsExposureMax, cfg.m_paramsExposureTime,
          cfg.m_paramsGainMin, cfg.m_paramsGainMax, cfg.m_paramsGain);
}

void JaiCameraHardwareTest::test_link_diagnostics_report_packet_size_and_counters()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    const QString diagnostics = m_camera->streamDiagnostics();
    qInfo("Link: %s", qPrintable(diagnostics));

    QVERIFY2(diagnostics != QStringLiteral("not connected"),
             "streamDiagnostics() reported disconnected on a connected camera");
    QVERIFY2(diagnostics != QStringLiteral("no counters available"),
             "the camera exposed none of the GigE transport or stream counters");

    // Not an assertion, a warning: a ~1500-byte packet means jumbo frames are off somewhere in the
    // path, which is a network setting this code cannot change but which dominates whether large
    // mono frames survive. Saying it here is how the number reaches whoever runs the test.
    if (diagnostics.contains(QStringLiteral("GevSCPSPacketSize="))) {
        const int at = diagnostics.indexOf(QStringLiteral("GevSCPSPacketSize="));
        const QString tail = diagnostics.mid(at).section(QLatin1Char('='), 1, 1).section(
            QLatin1Char(' '), 0, 0);
        const int packetSize = tail.toInt();
        if (packetSize > 0 && packetSize < 4000) {
            qWarning("Packet size is %d bytes: jumbo frames appear to be OFF on this path. "
                     "A 5 MP mono frame then arrives as ~3500 packets, and any loss among them "
                     "shows up as an incomplete frame.",
                     packetSize);
        }
    }
}

void JaiCameraHardwareTest::test_grab_returns_a_real_frame()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    QElapsedTimer elapsed;
    elapsed.start();
    const vc::device::GrabResult result = m_camera->grabSingleShot();
    const qint64 took = elapsed.elapsed();

    qInfo("Grab took %lld ms. Link after: %s", took, qPrintable(m_camera->streamDiagnostics()));

    QVERIFY2(result.isGrabSuccess,
             qPrintable(QStringLiteral("grab failed: %1").arg(result.msg)));

    // The actual point of this test: real pixels, not a success flag.
    QVERIFY2(!result.frame.empty(), "the grab reported success but returned an empty cv::Mat");
    QVERIFY(result.frame.cols > 0);
    QVERIFY(result.frame.rows > 0);

    const vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    qInfo("Frame: %dx%d, %d channel(s), %zu bytes",
          result.frame.cols, result.frame.rows, result.frame.channels(),
          result.frame.total() * result.frame.elemSize());

    QCOMPARE(result.frame.channels(), cfg.m_isColor ? 3 : 1);
}

void JaiCameraHardwareTest::test_grab_pixels_are_not_uniform()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    const vc::device::GrabResult result = m_camera->grabSingleShot();
    QVERIFY2(result.isGrabSuccess, qPrintable(result.msg));
    QVERIFY(!result.frame.empty());

    // A buffer that was allocated but never filled converts to a uniform image. That passes an
    // "is it empty" check while carrying no picture at all, so it is worth separating: with a lens
    // on a real scene the standard deviation is far from zero. A capped lens would also read as
    // uniform, hence the message.
    cv::Mat gray = result.frame;
    if (gray.channels() == 3) {
        cv::cvtColor(result.frame, gray, cv::COLOR_BGR2GRAY);
    }
    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(gray, mean, stddev);

    qInfo("Pixel mean %.2f, stddev %.2f", mean[0], stddev[0]);
    QVERIFY2(stddev[0] > 0.5,
             "the frame is uniform — the buffer may never have been filled (or the lens is "
             "capped / the scene is blank)");
}

void JaiCameraHardwareTest::test_repeated_grabs_all_return_frames()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    int succeeded = 0;
    qint64 worst = 0;
    qint64 total = 0;
    QString firstFailure;

    for (int i = 0; i < kStabilityGrabs; ++i) {
        QElapsedTimer elapsed;
        elapsed.start();
        const vc::device::GrabResult result = m_camera->grabSingleShot();
        const qint64 took = elapsed.elapsed();

        total += took;
        worst = qMax(worst, took);

        if (result.isGrabSuccess && !result.frame.empty()) {
            ++succeeded;
        } else if (firstFailure.isEmpty()) {
            firstFailure = QStringLiteral("grab %1 of %2: %3")
                               .arg(i + 1).arg(kStabilityGrabs).arg(result.msg);
        }
    }

    qInfo("%d/%d grabs returned a frame. Mean %lld ms, worst %lld ms.",
          succeeded, kStabilityGrabs, total / kStabilityGrabs, worst);
    qInfo("Link after: %s", qPrintable(m_camera->streamDiagnostics()));

    QVERIFY2(succeeded == kStabilityGrabs,
             qPrintable(QStringLiteral("only %1 of %2 grabs returned a frame. First failure — %3")
                            .arg(succeeded).arg(kStabilityGrabs).arg(firstFailure)));
}

void JaiCameraHardwareTest::test_grab_still_works_after_an_exposure_change()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    const double original = cfg.m_paramsExposureTime;

    // Somewhere clearly inside the range, and clearly different from where we started: a change
    // that lands on the same value would not exercise the write path at all.
    const double target = cfg.m_paramsExposureMin
                          + (cfg.m_paramsExposureMax - cfg.m_paramsExposureMin) * 0.25;

    cfg.m_paramsExposureTime = target;
    m_camera->setJaiGigeConfig(cfg);
    QVERIFY2(m_camera->applyParametersChange(), "applying the exposure change failed");

    const vc::device::JaiGigeCfg after = m_camera->jaiGigeConfig();
    qInfo("Exposure %.1f -> requested %.1f, camera reports %.1f (range now %.1f..%.1f)",
          original, target, after.m_paramsExposureTime,
          after.m_paramsExposureMin, after.m_paramsExposureMax);

    const vc::device::GrabResult result = m_camera->grabSingleShot();
    QVERIFY2(result.isGrabSuccess,
             qPrintable(QStringLiteral("grab after exposure change failed: %1").arg(result.msg)));
    QVERIFY(!result.frame.empty());

    // Put it back, so a rerun starts from where this one did.
    vc::device::JaiGigeCfg restore = m_camera->jaiGigeConfig();
    restore.m_paramsExposureTime = original;
    m_camera->setJaiGigeConfig(restore);
    m_camera->applyParametersChange();
}

bool JaiCameraHardwareTest::runOnAnotherThread(std::function<void()> body)
{
    class Worker : public QThread {
    public:
        explicit Worker(std::function<void()> fn) : m_fn(std::move(fn)) {}
        void run() override { m_fn(); }
    private:
        std::function<void()> m_fn;
    };

    Worker worker(std::move(body));
    worker.start();
    return worker.wait(15000);
}

void JaiCameraHardwareTest::test_continuous_streams_real_frames()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    int frames = 0;
    int emptyFrames = 0;
    QSize firstSize;
    const auto connection =
        connect(m_camera.get(), &vc::device::CameraDevice::continuousFrameReady, this,
                [&](vc::device::GrabResult result) {
                    if (result.frame.empty()) {
                        ++emptyFrames;
                        return;
                    }
                    if (frames == 0) {
                        firstSize = QSize(result.frame.cols, result.frame.rows);
                    }
                    ++frames;
                },
                Qt::DirectConnection);

    QVERIFY2(m_camera->startContinuousShot(), "continuous acquisition would not start");
    QVERIFY(m_camera->isContinuousActive());

    // The pump re-posts itself through the event loop, so the loop has to run for frames to
    // arrive — spinning here rather than sleeping is the whole reason this works.
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < kContinuousWindowMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    m_camera->stopContinuousShot();
    disconnect(connection);

    const double seconds = kContinuousWindowMs / 1000.0;
    qInfo("Continuous: %d frame(s) in %.1f s (%.1f fps), %d empty. First frame %dx%d.",
          frames, seconds, frames / seconds, emptyFrames, firstSize.width(), firstSize.height());
    qInfo("Link after: %s", qPrintable(m_camera->streamDiagnostics()));

    QVERIFY2(emptyFrames == 0, "an empty cv::Mat was emitted as a continuous frame");
    // A floor, not an exact rate: the paced ceiling is ~9 fps on this link and the point of the
    // assertion is that streaming actually streams, not that it hits a particular number.
    QVERIFY2(frames >= kMinContinuousFrames,
             qPrintable(QStringLiteral("only %1 frame(s) in %2 s; expected at least %3")
                            .arg(frames).arg(seconds).arg(kMinContinuousFrames)));

    const vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    QCOMPARE(firstSize.width() > 0 && firstSize.height() > 0, true);
    QCOMPARE(cfg.m_isColor, false);
}

void JaiCameraHardwareTest::test_continuous_stop_is_honoured_from_another_thread()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    QVERIFY(m_camera->startContinuousShot());

    // The regression that matters. The frame pump runs on the same thread that must deliver
    // the stop, so an implementation that loops instead of re-posting itself would never see
    // this call and the only way out would be killing the task.
    QElapsedTimer stopClock;
    stopClock.start();
    vc::device::JaiGigECamera *camera = m_camera.get();
    QVERIFY2(runOnAnotherThread([camera]() { camera->stopContinuousShot(); }),
             "the stopping thread never finished — stop blocked");
    const qint64 took = stopClock.elapsed();

    qInfo("Stop took %lld ms.", took);
    QVERIFY(!m_camera->isContinuousActive());
    QVERIFY2(took < kStopDeadlineMs,
             qPrintable(QStringLiteral("stop took %1 ms, over the %2 ms budget")
                            .arg(took).arg(kStopDeadlineMs)));
}

void JaiCameraHardwareTest::test_single_shot_works_immediately_after_continuous()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    QVERIFY(m_camera->startContinuousShot());
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < 500) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    m_camera->stopContinuousShot();

    // The point: stop must restore SingleFrame and the operator's frame rate. A camera left in
    // Continuous floods the link, and grabSingleShot()'s re-arm path behaves differently in each
    // mode — so this failing is how "live view broke grabbing" would show up.
    const vc::device::GrabResult result = m_camera->grabSingleShot();
    QVERIFY2(result.isGrabSuccess,
             qPrintable(QStringLiteral("single shot after continuous failed: %1").arg(result.msg)));
    QVERIFY(!result.frame.empty());
}

void JaiCameraHardwareTest::test_continuous_start_and_stop_are_idempotent()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    // Stop when not started is a no-op, not a fault: a toggle button cannot always know.
    m_camera->stopContinuousShot();
    QVERIFY(!m_camera->isContinuousActive());

    QVERIFY(m_camera->startContinuousShot());
    QVERIFY2(m_camera->startContinuousShot(), "a second start must succeed, not report failure");
    QVERIFY(m_camera->isContinuousActive());

    m_camera->stopContinuousShot();
    m_camera->stopContinuousShot();
    QVERIFY(!m_camera->isContinuousActive());
}

QString JaiCameraHardwareTest::applyBacklightConfig(bool invert)
{
    vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();

    QString line;
    for (const vc::device::jai::JaiIOLine &candidate : cfg.m_ioCapabilities) {
        if (candidate.can_be_output) {
            line = candidate.name;
            break;
        }
    }
    if (line.isEmpty()) {
        return QString();
    }

    cfg.m_autoBacklightControl = true;
    cfg.m_autoBacklightLine    = line;
    cfg.m_autoBacklightInvert  = invert;
    cfg.m_autoBacklightDelay   = 0;
    m_camera->setJaiGigeConfig(cfg);
    m_camera->applyParametersChange();
    return line;
}

void JaiCameraHardwareTest::test_backlight_output_is_configured_on_the_selected_line()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    const QString line = applyBacklightConfig(false);
    if (line.isEmpty()) {
        QSKIP("this camera exposes no output-capable line");
    }

    const QString diagnostics = m_camera->backlightDiagnostics();
    qInfo("Backlight: %s", qPrintable(diagnostics));

    QVERIFY2(diagnostics != QStringLiteral("not configured"),
             "the backlight output was not set up on the configured line");
    // The commissioned convention: the selected line is driven by UserOutput0, so switching the
    // light is one register write rather than a re-route.
    QVERIFY2(diagnostics.contains(QStringLiteral("LineSource=UserOutput0")),
             qPrintable(QStringLiteral("expected LineSource=UserOutput0, got: %1")
                            .arg(diagnostics)));
    QVERIFY(diagnostics.contains(QStringLiteral("line=%1").arg(line)));
}

void JaiCameraHardwareTest::test_backlight_polarity_goes_into_the_camera_not_the_value()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }
    if (applyBacklightConfig(true).isEmpty()) {
        QSKIP("this camera exposes no output-capable line");
    }

    // Read back from the CAMERA. The old implementation inverted the value in software before
    // writing it, which passes any test that only checks the light state we asked for and leaves
    // the pin at the wrong level whenever something else drives it — the camera's power-up
    // default included. LineInverter=1 is the only evidence that the polarity is really in the
    // hardware.
    const QString inverted = m_camera->backlightDiagnostics();
    qInfo("Backlight (invert on):  %s", qPrintable(inverted));
    QVERIFY2(inverted.contains(QStringLiteral("LineInverter=1")),
             qPrintable(QStringLiteral("invert was not written to the camera: %1").arg(inverted)));

    QVERIFY(!applyBacklightConfig(false).isEmpty());
    const QString upright = m_camera->backlightDiagnostics();
    qInfo("Backlight (invert off): %s", qPrintable(upright));
    QVERIFY2(upright.contains(QStringLiteral("LineInverter=0")),
             qPrintable(QStringLiteral("invert was not cleared on the camera: %1").arg(upright)));
}

void JaiCameraHardwareTest::test_backlight_switches_around_a_grab()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }
    if (applyBacklightConfig(false).isEmpty()) {
        QSKIP("this camera exposes no output-capable line");
    }

    // A grab drives the output on before exposing and off afterwards, so the value settles low.
    // What this pins is that the grab path still works with backlight control enabled, and that
    // the output ends up back off rather than left on.
    const vc::device::GrabResult result = m_camera->grabSingleShot();
    QVERIFY2(result.isGrabSuccess,
             qPrintable(QStringLiteral("grab with backlight control failed: %1").arg(result.msg)));
    QVERIFY(!result.frame.empty());

    const QString after = m_camera->backlightDiagnostics();
    qInfo("Backlight after grab: %s", qPrintable(after));
    QVERIFY2(after.contains(QStringLiteral("UserOutputValue=0")),
             qPrintable(QStringLiteral("the backlight was left on after the grab: %1").arg(after)));

    // Leave the camera as this suite found it.
    vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    cfg.m_autoBacklightControl = false;
    m_camera->setJaiGigeConfig(cfg);
    m_camera->applyParametersChange();
}

void JaiCameraHardwareTest::test_backlight_override_drives_the_lamp_and_reports_it()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }
    if (applyBacklightConfig(false).isEmpty()) {
        QSKIP("this camera exposes no output-capable line");
    }

    QSignalSpy stateSpy(m_camera.get(), &vc::device::CameraDevice::backlightStateChanged);
    QVERIFY(stateSpy.isValid());

    QVERIFY(m_camera->setBacklightOverride(true));
    QVERIFY(m_camera->isBacklightOverridden());
    const QString lit = m_camera->backlightDiagnostics();
    qInfo("Backlight with override on: %s", qPrintable(lit));
    QVERIFY2(lit.contains(QStringLiteral("UserOutputValue=1")),
             qPrintable(QStringLiteral("override did not light the lamp: %1").arg(lit)));

    QVERIFY(m_camera->setBacklightOverride(false));
    QVERIFY(!m_camera->isBacklightOverridden());
    const QString dark = m_camera->backlightDiagnostics();
    qInfo("Backlight with override released: %s", qPrintable(dark));
    QVERIFY2(dark.contains(QStringLiteral("UserOutputValue=0")),
             qPrintable(QStringLiteral("releasing the override left the lamp on: %1").arg(dark)));

    // Reported on both paths, because CameraRunner resolves its commands from this signal and a
    // silent write would leave the command hanging until its watchdog fired.
    QCOMPARE(stateSpy.count(), 2);

    vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    cfg.m_autoBacklightControl = false;
    m_camera->setJaiGigeConfig(cfg);
    m_camera->applyParametersChange();
}

void JaiCameraHardwareTest::test_backlight_override_survives_a_grab()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }
    if (applyBacklightConfig(false).isEmpty()) {
        QSKIP("this camera exposes no output-capable line");
    }

    // The defect this exists for: auto-backlight switches the lamp off after every grab. If the
    // override does not suppress that, the operator's manual "on" appears to work and is undone
    // at the next trigger — which reads as a failing lamp, not as software.
    QVERIFY(m_camera->setBacklightOverride(true));

    const vc::device::GrabResult result = m_camera->grabSingleShot();
    QVERIFY2(result.isGrabSuccess,
             qPrintable(QStringLiteral("grab under backlight override failed: %1").arg(result.msg)));
    QVERIFY(!result.frame.empty());

    const QString after = m_camera->backlightDiagnostics();
    qInfo("Backlight after a grab under override: %s", qPrintable(after));
    QVERIFY2(after.contains(QStringLiteral("UserOutputValue=1")),
             qPrintable(QStringLiteral("the grab switched the lamp off despite the override: %1")
                            .arg(after)));

    // Released, the auto sequence takes the lamp back and leaves it off after a grab.
    QVERIFY(m_camera->setBacklightOverride(false));
    const vc::device::GrabResult second = m_camera->grabSingleShot();
    QVERIFY(second.isGrabSuccess);
    const QString released = m_camera->backlightDiagnostics();
    QVERIFY2(released.contains(QStringLiteral("UserOutputValue=0")),
             qPrintable(QStringLiteral("auto-backlight did not resume after release: %1")
                            .arg(released)));

    vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    cfg.m_autoBacklightControl = false;
    m_camera->setJaiGigeConfig(cfg);
    m_camera->applyParametersChange();
}

void JaiCameraHardwareTest::test_connect_pushes_the_configured_settings_onto_the_camera()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }

    // The camera has to be left holding DIFFERENT values from the config, or this proves
    // nothing. A plain disconnect/reconnect does not reset it — it comes back with whatever it
    // had — so a test built on that passes even when connect never writes anything, which is
    // exactly what an earlier version of this case did.
    //
    // Step 1: drive the camera to something like its power-up state and apply, so the CAMERA
    // holds a fast frame rate and a short exposure.
    vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    cfg.m_enableAcquisitionFrameRate = true;
    cfg.m_paramsAcquisitionFrameRate = 22.0;
    cfg.m_paramsExposureTime         = 3000.0;
    m_camera->setJaiGigeConfig(cfg);
    QVERIFY(m_camera->applyParametersChange());
    qInfo("Camera driven to defaults: %s", qPrintable(m_camera->cameraFeatureSnapshot()));

    // Step 2: put the commissioned values into the config WITHOUT applying them. Config and
    // camera now disagree the same way they do after a power cycle: the panel holds what was
    // commissioned, the camera holds what it came up with.
    //
    // 120000 us is deliberately legal at 5 fps and NOT legal at 22 fps. It is the value the old
    // pre-push read threw away: it looked out of range at connect, moments before the push that
    // was about to make it legal again.
    const double targetRate     = 5.0;
    const double targetExposure = 120000.0;
    cfg = m_camera->jaiGigeConfig();
    cfg.m_enableAcquisitionFrameRate = true;
    cfg.m_paramsAcquisitionFrameRate = targetRate;
    cfg.m_paramsExposureTime         = targetExposure;
    m_camera->setJaiGigeConfig(cfg);

    QVERIFY(m_camera->deviceDisconnect());
    QVERIFY2(m_camera->deviceConnect(), "reconnect failed");

    const vc::device::JaiGigeCfg afterReconnect = m_camera->jaiGigeConfig();
    const QString snapshot = m_camera->cameraFeatureSnapshot();
    qInfo("After reconnect:  %s", qPrintable(snapshot));
    qInfo("Config after reconnect: exposure %.1f, frame rate %.2f, limiting %s",
          afterReconnect.m_paramsExposureTime,
          afterReconnect.m_paramsAcquisitionFrameRate,
          afterReconnect.m_enableAcquisitionFrameRate ? "on" : "off");

    // The config kept what was commissioned...
    QVERIFY2(qAbs(afterReconnect.m_paramsAcquisitionFrameRate - targetRate) < 0.5,
             qPrintable(QStringLiteral("frame rate came back as %1, expected %2")
                            .arg(afterReconnect.m_paramsAcquisitionFrameRate).arg(targetRate)));
    QVERIFY2(afterReconnect.m_enableAcquisitionFrameRate,
             "the frame-rate limit was switched off by the reconnect");
    QVERIFY2(qAbs(afterReconnect.m_paramsExposureTime - targetExposure) < 1000.0,
             qPrintable(QStringLiteral("exposure came back as %1, expected %2")
                            .arg(afterReconnect.m_paramsExposureTime).arg(targetExposure)));

    // ...and, the part that actually matters, so does the CAMERA. Read straight off it, because
    // asserting on the config alone passes on a build where connect never wrote anything: the
    // config would simply still hold what the test put there.
    QVERIFY2(snapshot.contains(QStringLiteral("= %1").arg(targetExposure, 0, 'g')),
             qPrintable(QStringLiteral("the camera is not holding the configured exposure: %1")
                            .arg(snapshot)));
    QVERIFY2(snapshot.contains(QStringLiteral("= %1 |").arg(targetRate, 0, 'g')),
             qPrintable(QStringLiteral("the camera is not holding the configured frame rate: %1")
                            .arg(snapshot)));
}

void JaiCameraHardwareTest::test_backlight_is_configured_by_connect_alone()
{
    if (!requireCamera()) {
        QSKIP(qPrintable(m_skipReason));
    }
    const QString line = applyBacklightConfig(true);
    if (line.isEmpty()) {
        QSKIP("this camera exposes no output-capable line");
    }

    // No Apply after the reconnect — that is the whole point. configureBacklightOutput() ran
    // inside deviceConnect() but guarded on isDeviceConnected(), which deviceConnect() only sets
    // on its last line, so it returned early and did nothing. The backlight then worked in
    // testing (someone had pressed Apply) and came up unconfigured on a fresh session.
    QVERIFY(m_camera->deviceDisconnect());
    QVERIFY2(m_camera->deviceConnect(), "reconnect failed");

    const QString diagnostics = m_camera->backlightDiagnostics();
    qInfo("Backlight after reconnect: %s", qPrintable(diagnostics));

    QVERIFY2(diagnostics != QStringLiteral("not configured"),
             "connect alone did not set the backlight up");
    QVERIFY(diagnostics.contains(QStringLiteral("line=%1").arg(line)));
    QVERIFY2(diagnostics.contains(QStringLiteral("LineSource=UserOutput0")),
             qPrintable(diagnostics));
    QVERIFY2(diagnostics.contains(QStringLiteral("LineInverter=1")),
             qPrintable(QStringLiteral("the configured polarity did not survive connect: %1")
                            .arg(diagnostics)));

    vc::device::JaiGigeCfg cfg = m_camera->jaiGigeConfig();
    cfg.m_autoBacklightControl = false;
    cfg.m_autoBacklightInvert  = false;
    m_camera->setJaiGigeConfig(cfg);
    m_camera->applyParametersChange();
}

QTEST_MAIN(JaiCameraHardwareTest)
#include "main.moc"
