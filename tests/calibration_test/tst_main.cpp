#include <QtTest/QtTest>
#include <QFile>

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <cstring>
#include <random>
#include <stdexcept>

#include "calibration/calibration_board.h"
#include "calibration/calibration_board_factory.h"
#include "calibration/fanuc_irvision_board.h"
#include "calibration/calibrator.h"

// ============================================================
// TestCalibrationBoardFactory
// ============================================================
class TestCalibrationBoardFactory : public QObject
{
    Q_OBJECT
private slots:
    void create_FanucIRvision_returnsConcrete();
    void createFromPreset_knownName_returnsBoard();
    void createFromPreset_unknownName_returnsNull();
    void availablePresets_includesAllIRvisionEntries();
    void polymorphicUsage_throughBasePointer();
    void toJson_roundTrip_viaFactory();
    void createFromJson_unknownType_returnsNull();
};

void TestCalibrationBoardFactory::create_FanucIRvision_returnsConcrete()
{
    auto b = calib::CalibrationBoardFactory::create(calib::CalibrationBoardType::FanucIRvision);
    QVERIFY(b != nullptr);
    QCOMPARE(QString::fromStdString(b->typeName()), QStringLiteral("FanucIRvision"));
    QVERIFY(dynamic_cast<const calib::FanucIRvisionBoard*>(b.get()) != nullptr);
}

void TestCalibrationBoardFactory::createFromPreset_knownName_returnsBoard()
{
    auto b = calib::CalibrationBoardFactory::createFromPreset("iRvision-5mm");
    QVERIFY(b != nullptr);
    QCOMPARE(QString::fromStdString(b->typeName()), QStringLiteral("FanucIRvision"));

    const auto* irv = dynamic_cast<const calib::FanucIRvisionBoard*>(b.get());
    QVERIFY(irv != nullptr);
    // Don't pin to specific dimensions — preset constants get tuned. Verify
    // only the structural invariants required by the design.
    const auto& p = irv->params();
    QVERIFY(p.rows >= 5 && p.rows % 2 == 1);
    QVERIFY(p.cols >= 7 && p.cols % 2 == 1);
    QVERIFY(p.dotSpacingMm > 0.0);
    QCOMPARE(b->totalDots(), p.rows * p.cols);
}

void TestCalibrationBoardFactory::createFromPreset_unknownName_returnsNull()
{
    auto b = calib::CalibrationBoardFactory::createFromPreset("doesNotExist");
    QVERIFY(b == nullptr);
}

void TestCalibrationBoardFactory::availablePresets_includesAllIRvisionEntries()
{
    const auto presets = calib::CalibrationBoardFactory::availablePresets();
    QVERIFY(std::find(presets.begin(), presets.end(), std::string("iRvision-5mm"))    != presets.end());
    QVERIFY(std::find(presets.begin(), presets.end(), std::string("iRvision-11.5mm")) != presets.end());
    QVERIFY(std::find(presets.begin(), presets.end(), std::string("iRvision-15mm"))   != presets.end());
    QVERIFY(std::find(presets.begin(), presets.end(), std::string("iRvision-22.5mm")) != presets.end());
    QVERIFY(std::find(presets.begin(), presets.end(), std::string("iRvision-30mm"))   != presets.end());
}

void TestCalibrationBoardFactory::toJson_roundTrip_viaFactory()
{
    // Build a board, serialise, reconstruct via the factory, compare Params.
    calib::FanucIRvisionBoard::Params p;
    p.rows = 7; p.cols = 9; p.dotSpacingMm = 13.0; p.marginMm = 8.0;
    p.normalDotSizeMm = 3.5; p.coorDotSizeMm = 7.0; p.innerTargetDotSizeMm = 1.2;
    p.binarizeThreshold = 137;
    calib::FanucIRvisionBoard original(p);

    const std::string json = original.toJson();
    QVERIFY(!json.empty());
    QVERIFY(json.find("FanucIRvision") != std::string::npos);

    auto restored = calib::CalibrationBoardFactory::createFromJson(json);
    QVERIFY(restored != nullptr);
    QCOMPARE(QString::fromStdString(restored->typeName()),
             QStringLiteral("FanucIRvision"));

    const auto* rv = dynamic_cast<const calib::FanucIRvisionBoard*>(restored.get());
    QVERIFY(rv != nullptr);
    const auto& q = rv->params();
    QCOMPARE(q.rows, p.rows);
    QCOMPARE(q.cols, p.cols);
    QVERIFY(std::abs(q.dotSpacingMm          - p.dotSpacingMm)          < 1e-9);
    QVERIFY(std::abs(q.marginMm              - p.marginMm)              < 1e-9);
    QVERIFY(std::abs(q.normalDotSizeMm       - p.normalDotSizeMm)       < 1e-9);
    QVERIFY(std::abs(q.coorDotSizeMm         - p.coorDotSizeMm)         < 1e-9);
    QVERIFY(std::abs(q.innerTargetDotSizeMm  - p.innerTargetDotSizeMm)  < 1e-9);
    QCOMPARE(q.binarizeThreshold, p.binarizeThreshold);
}

void TestCalibrationBoardFactory::createFromJson_unknownType_returnsNull()
{
    auto b = calib::CalibrationBoardFactory::createFromJson(
        R"({"type":"SomeOtherBoard","rows":11})");
    QVERIFY(b == nullptr);
}

void TestCalibrationBoardFactory::polymorphicUsage_throughBasePointer()
{
    // Use the board through its abstract base interface only — generate, detect,
    // and round-trip the row-major points. No subclass-specific calls.
    std::unique_ptr<calib::CalibrationBoard> board =
        calib::CalibrationBoardFactory::createFromPreset("iRvision-5mm");
    QVERIFY(board != nullptr);

    const cv::Mat img = board->generateImage(10.0);
    QVERIFY(!img.empty());

    std::vector<cv::Point2f> pts, corners;
    QVERIFY(board->detect(img, pts, &corners));
    QCOMPARE(static_cast<int>(pts.size()), board->totalDots());
    QCOMPARE(static_cast<int>(corners.size()), 4);

    QCOMPARE(static_cast<int>(board->objectPoints().size()),    board->totalDots());
    QCOMPARE(static_cast<int>(board->objectPointsXY().size()),  board->totalDots());
    QCOMPARE(static_cast<int>(board->cornerObjectPointsXY().size()), 4);
}

// ============================================================
// TestFanucIRvisionBoard
// ============================================================
class TestFanucIRvisionBoard : public QObject
{
    Q_OBJECT
private slots:
    void params_evenRowsOrCols_throws();
    void params_validOdd_constructsOk();
    void generateImage_hasExpectedSize();
    void objectPoints_countAndSpacing();
    void markerIndices_returnsFourOrientationMarkers();
    void targetIndices_returnsEightTargets();
    void coordIndices_returnsThreeCoordDots();
    void specialCells_doNotOverlap();
    void cornerObjectPoints_areBoardCorners();
    void detect_onGeneratedImage_returnsAllDots();
    void detect_returnsFourCornersInImageOrder();
    void detect_orientationOrder_isRowMajor();
    void writePrintableImage_embedsPhysChunk();
};

void TestFanucIRvisionBoard::params_evenRowsOrCols_throws()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 8; p.cols = 11;
    QVERIFY_EXCEPTION_THROWN(calib::FanucIRvisionBoard{p}, std::invalid_argument);

    p.rows = 9; p.cols = 10;
    QVERIFY_EXCEPTION_THROWN(calib::FanucIRvisionBoard{p}, std::invalid_argument);

    p.rows = 8; p.cols = 8;
    QVERIFY_EXCEPTION_THROWN(calib::FanucIRvisionBoard{p}, std::invalid_argument);
}

void TestFanucIRvisionBoard::params_validOdd_constructsOk()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 5; p.cols = 7;   // minimum supported size
    try {
        calib::FanucIRvisionBoard board(p);
        QCOMPARE(board.params().rows, 5);
        QCOMPARE(board.params().cols, 7);
    } catch (...) {
        QFAIL("Construction with valid odd dims should not throw");
    }
}

void TestFanucIRvisionBoard::generateImage_hasExpectedSize()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 9; p.cols = 11;
    p.normalDotSizeMm = 4.0; p.dotSpacingMm = 15.0; p.marginMm = 20.0;
    calib::FanucIRvisionBoard board(p);

    const double pxPerMm = 10.0;
    const cv::Mat img = board.generateImage(pxPerMm);

    const int expectedW = static_cast<int>(std::round(((p.cols - 1) * p.dotSpacingMm + 2 * p.marginMm) * pxPerMm));
    const int expectedH = static_cast<int>(std::round(((p.rows - 1) * p.dotSpacingMm + 2 * p.marginMm) * pxPerMm));

    QCOMPARE(img.cols, expectedW);
    QCOMPARE(img.rows, expectedH);
    QCOMPARE(img.channels(), 1);
}

void TestFanucIRvisionBoard::objectPoints_countAndSpacing()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 5; p.cols = 7; p.dotSpacingMm = 10.0; p.coorDotSizeMm = 8.0;
    calib::FanucIRvisionBoard board(p);

    const auto pts = board.objectPoints();
    QCOMPARE(static_cast<int>(pts.size()), p.rows * p.cols);

    QVERIFY(std::abs(pts.front().x) < 1e-5);
    QVERIFY(std::abs(pts.front().y) < 1e-5);
    QVERIFY(std::abs(pts.front().z) < 1e-5);

    QVERIFY(std::abs(pts[1].x - pts[0].x - p.dotSpacingMm) < 1e-4);
    QVERIFY(std::abs(pts[1].y - pts[0].y) < 1e-4);
    QVERIFY(std::abs(pts[p.cols].y - pts[0].y - p.dotSpacingMm) < 1e-4);
}

void TestFanucIRvisionBoard::markerIndices_returnsFourOrientationMarkers()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 7; p.cols = 9;
    calib::FanucIRvisionBoard board(p);

    const auto idx = board.markerIndices();
    QCOMPARE(static_cast<int>(idx.size()), 4);

    const int R = p.rows, C = p.cols;
    const int rmid = (R - 1) / 2, cmid = (C - 1) / 2;
    const std::vector<int> expected = {
        rmid       * C +  cmid,        // origin
        rmid       * C + (cmid + 1),   // x-mid
        rmid       * C + (cmid + 2),   // x-tip
        (rmid - 1) * C +  cmid         // y-tip
    };
    QCOMPARE(idx, expected);
}

void TestFanucIRvisionBoard::targetIndices_returnsEightTargets()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 7; p.cols = 9;
    calib::FanucIRvisionBoard board(p);

    const auto idx = board.targetIndices();
    QCOMPARE(static_cast<int>(idx.size()), 8);

    const int R = p.rows, C = p.cols;
    const int rmid = (R - 1) / 2, cmid = (C - 1) / 2;
    const std::vector<int> expected = {
        0 * C + 0,                 // TL
        0 * C + (C - 1),           // TR
        (R - 1) * C + (C - 1),     // BR
        (R - 1) * C + 0,           // BL
        0 * C + cmid,              // top mid
        (R - 1) * C + cmid,        // bottom mid
        rmid * C + 0,              // left mid
        rmid * C + (C - 1)         // right mid
    };
    QCOMPARE(idx, expected);
}

void TestFanucIRvisionBoard::coordIndices_returnsThreeCoordDots()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 7; p.cols = 9;
    calib::FanucIRvisionBoard board(p);

    const auto idx = board.coordIndices();
    QCOMPARE(static_cast<int>(idx.size()), 3);

    const int C = p.cols;
    const int rmid = (p.rows - 1) / 2, cmid = (C - 1) / 2;
    const std::vector<int> expected = {
        rmid       * C + (cmid + 1),
        rmid       * C + (cmid + 2),
        (rmid - 1) * C +  cmid
    };
    QCOMPARE(idx, expected);
}

void TestFanucIRvisionBoard::specialCells_doNotOverlap()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 5; p.cols = 7;
    calib::FanucIRvisionBoard board(p);

    std::vector<int> all;
    for (int i : board.targetIndices()) all.push_back(i);
    for (int i : board.centerIndex())   all.push_back(i);
    for (int i : board.coordIndices())  all.push_back(i);
    QCOMPARE(static_cast<int>(all.size()), 12);

    std::sort(all.begin(), all.end());
    QVERIFY(std::adjacent_find(all.begin(), all.end()) == all.end());
}

void TestFanucIRvisionBoard::cornerObjectPoints_areBoardCorners()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 5; p.cols = 9; p.dotSpacingMm = 10.0;
    calib::FanucIRvisionBoard board(p);

    const auto corners = board.cornerObjectPointsXY();
    QCOMPARE(static_cast<int>(corners.size()), 4);
    // TL, TR, BR, BL
    QVERIFY(cv::norm(corners[0] - cv::Point2f(0,  0))  < 1e-4);
    QVERIFY(cv::norm(corners[1] - cv::Point2f(80, 0))  < 1e-4);
    QVERIFY(cv::norm(corners[2] - cv::Point2f(80, 40)) < 1e-4);
    QVERIFY(cv::norm(corners[3] - cv::Point2f(0,  40)) < 1e-4);
}

void TestFanucIRvisionBoard::detect_onGeneratedImage_returnsAllDots()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 7; p.cols = 9;
    p.normalDotSizeMm = 4.0; p.dotSpacingMm = 15.0; p.marginMm = 20.0;
    calib::FanucIRvisionBoard board(p);

    const cv::Mat img = board.generateImage(10.0);
    std::vector<cv::Point2f> pts;
    std::vector<cv::Point2f> corners;
    const bool ok = board.detect(img, pts, &corners);

    QVERIFY2(ok, "Detection failed on generated image");
    QCOMPARE(static_cast<int>(pts.size()), p.rows * p.cols);
    QCOMPARE(static_cast<int>(corners.size()), 4);
}

void TestFanucIRvisionBoard::detect_returnsFourCornersInImageOrder()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 5; p.cols = 7;
    calib::FanucIRvisionBoard board(p);

    const double pxPerMm = 10.0;
    const cv::Mat img = board.generateImage(pxPerMm);
    std::vector<cv::Point2f> pts, corners;
    QVERIFY(board.detect(img, pts, &corners));

    // Expected corner positions on generated image (axis-aligned, no rotation).
    const float marginPx = static_cast<float>(p.marginMm * pxPerMm);
    const float spanX    = static_cast<float>((p.cols - 1) * p.dotSpacingMm * pxPerMm);
    const float spanY    = static_cast<float>((p.rows - 1) * p.dotSpacingMm * pxPerMm);

    const cv::Point2f tl(marginPx,          marginPx);
    const cv::Point2f tr(marginPx + spanX,  marginPx);
    const cv::Point2f br(marginPx + spanX,  marginPx + spanY);
    const cv::Point2f bl(marginPx,          marginPx + spanY);

    QVERIFY(cv::norm(corners[0] - tl) < 2.0);
    QVERIFY(cv::norm(corners[1] - tr) < 2.0);
    QVERIFY(cv::norm(corners[2] - br) < 2.0);
    QVERIFY(cv::norm(corners[3] - bl) < 2.0);
}

void TestFanucIRvisionBoard::detect_orientationOrder_isRowMajor()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 7; p.cols = 9;
    calib::FanucIRvisionBoard board(p);

    const cv::Mat img = board.generateImage(10.0);
    std::vector<cv::Point2f> imgPts;
    QVERIFY(board.detect(img, imgPts));

    const int cols = p.cols;
    QVERIFY(imgPts[1].x > imgPts[0].x);
    QVERIFY(imgPts[cols].y > imgPts[0].y);
}

void TestFanucIRvisionBoard::writePrintableImage_embedsPhysChunk()
{
    calib::FanucIRvisionBoard::Params p;
    p.rows = 7; p.cols = 9;
    calib::FanucIRvisionBoard board(p);

    const QString file = QDir::temp().filePath("board_print.png");
    const double dpi   = 300.0;
    QVERIFY(board.writePrintableImage(file.toStdString(), dpi));

    QFile fh(file);
    QVERIFY(fh.open(QIODevice::ReadOnly));
    const QByteArray data = fh.readAll();
    fh.close();
    QVERIFY(data.size() > 64);

    // PNG signature
    static const unsigned char sig[8] = { 0x89, 'P','N','G', 0x0D, 0x0A, 0x1A, 0x0A };
    for (int i = 0; i < 8; ++i)
        QCOMPARE(static_cast<unsigned char>(data[i]), sig[i]);

    // Locate pHYs chunk; verify it sits before the first IDAT.
    int physOffset = -1, idatOffset = -1;
    int pos = 8;
    while (pos + 12 <= data.size()) {
        const unsigned char* b = reinterpret_cast<const unsigned char*>(data.constData()) + pos;
        const quint32 len = (quint32(b[0]) << 24) | (quint32(b[1]) << 16)
                          | (quint32(b[2]) <<  8) |  quint32(b[3]);
        if (std::memcmp(b + 4, "pHYs", 4) == 0) physOffset = pos;
        if (std::memcmp(b + 4, "IDAT", 4) == 0) { idatOffset = pos; break; }
        pos += 12 + static_cast<int>(len);
    }
    QVERIFY2(physOffset >= 0, "pHYs chunk not found in PNG");
    QVERIFY2(idatOffset > physOffset, "pHYs must appear before IDAT");

    // Decode pHYs data: 4B ppx, 4B ppy, 1B unit. Unit must be 1 (meters).
    const unsigned char* d = reinterpret_cast<const unsigned char*>(data.constData()) + physOffset + 8;
    const quint32 ppx = (quint32(d[0]) << 24) | (quint32(d[1]) << 16)
                     | (quint32(d[2]) <<  8) |  quint32(d[3]);
    const quint32 ppy = (quint32(d[4]) << 24) | (quint32(d[5]) << 16)
                     | (quint32(d[6]) <<  8) |  quint32(d[7]);
    const quint8  unit = d[8];
    QCOMPARE(unit, quint8(1));

    const double expectedPxPerMeter = (dpi / 25.4) * 1000.0;
    QVERIFY(std::abs(double(ppx) - expectedPxPerMeter) < 2.0);
    QVERIFY(std::abs(double(ppy) - expectedPxPerMeter) < 2.0);
}

// ============================================================
// TestCalibrator
// ============================================================
class TestCalibrator : public QObject
{
    Q_OBJECT
private slots:
    void calibrate_lessThan4Points_returnsFalse();
    void calibrate_perfectAffine_lowError();
    void calibrate_fromFourCornersOnly();
    void roundTrip_imageToRobotToImage();
    void imageToRobot_zOnFittedPlane();
    void rotateImageToRobot_roundTripIsExact();
    void rotateImageToRobot_pureRotationMatchesAppliedAngle();
    void saveLoad_homographyAndPlanePreserved();
    void toJson_fromJson_preservesFullState();
};

namespace {
// Apply 2D affine in XY and lift to Z = tiltA*x + tiltB*y + tiltC (in robot frame).
std::vector<cv::Point3f> applyAffine3D(const std::vector<cv::Point2f>& src,
                                       double rotDeg, cv::Point2f t, double scale,
                                       double noiseStd = 0.0,
                                       double tiltA = 0.0, double tiltB = 0.0, double tiltC = 0.0)
{
    const double a = rotDeg * CV_PI / 180.0;
    const float cosA = static_cast<float>(std::cos(a) * scale);
    const float sinA = static_cast<float>(std::sin(a) * scale);

    std::mt19937 rng(42);
    const bool useNoise = (noiseStd > 0.0);
    std::normal_distribution<float> n(0.0f,
                                      useNoise ? static_cast<float>(noiseStd) : 1.0f);

    std::vector<cv::Point3f> dst;
    dst.reserve(src.size());
    for (const auto& p : src) {
        const float nx = useNoise ? n(rng) : 0.0f;
        const float ny = useNoise ? n(rng) : 0.0f;
        const float nz = useNoise ? n(rng) : 0.0f;
        const float x = cosA * p.x - sinA * p.y + t.x + nx;
        const float y = sinA * p.x + cosA * p.y + t.y + ny;
        const float z = static_cast<float>(tiltA * x + tiltB * y + tiltC) + nz;
        dst.emplace_back(x, y, z);
    }
    return dst;
}
}

void TestCalibrator::calibrate_lessThan4Points_returnsFalse()
{
    calib::Calibrator c;
    std::vector<cv::Point2f> img = { {0,0}, {1,0}, {0,1} };
    std::vector<cv::Point3f> rob = { {0,0,0}, {1,0,0}, {0,1,0} };
    c.addCorrespondences(img, rob);
    QVERIFY(!c.calibrate());
    QVERIFY(!c.isCalibrated());
}

void TestCalibrator::calibrate_perfectAffine_lowError()
{
    std::vector<cv::Point2f> img;
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 5; ++c)
            img.emplace_back(static_cast<float>(c * 50 + 100),
                             static_cast<float>(r * 50 + 100));
    const auto rob = applyAffine3D(img, 12.0, {200, -50}, 0.2, 0.0);

    calib::Calibrator c;
    c.addCorrespondences(img, rob);
    QVERIFY(c.calibrate());
    QVERIFY(c.isCalibrated());
    QVERIFY(c.reprojectionErrorMm() < 1e-3);
    QVERIFY(c.reprojectionErrorPx() < 1e-2);
}

void TestCalibrator::calibrate_fromFourCornersOnly()
{
    // Real-world flow: only the 4 corner markers are used to calibrate.
    calib::FanucIRvisionBoard::Params p;
    p.rows = 7; p.cols = 9;
    calib::FanucIRvisionBoard board(p);

    const double pxPerMm = 10.0;
    const cv::Mat img = board.generateImage(pxPerMm);
    std::vector<cv::Point2f> allPts, cornersImg;
    QVERIFY(board.detect(img, allPts, &cornersImg));
    QCOMPARE(static_cast<int>(cornersImg.size()), 4);

    const auto cornersBoard = board.cornerObjectPointsXY();   // mm
    const auto cornersRobot = applyAffine3D(cornersBoard, 5.0, {100, 200}, 1.0);

    calib::Calibrator c;
    c.addCorrespondences(cornersImg, cornersRobot);
    QVERIFY(c.calibrate());

    // Round-trip each corner image point.
    for (size_t i = 0; i < cornersImg.size(); ++i) {
        const cv::Point3f robotProjected = c.imageToRobot(cornersImg[i]);
        const cv::Point2f xyDiff(robotProjected.x - cornersRobot[i].x,
                                 robotProjected.y - cornersRobot[i].y);
        QVERIFY(cv::norm(xyDiff) < 0.5);
    }
}

void TestCalibrator::roundTrip_imageToRobotToImage()
{
    std::vector<cv::Point2f> img;
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 5; ++c)
            img.emplace_back(static_cast<float>(c * 40), static_cast<float>(r * 40));
    const auto rob = applyAffine3D(img, -8.0, {15, 22}, 0.5);

    calib::Calibrator c;
    c.addCorrespondences(img, rob);
    QVERIFY(c.calibrate());

    const cv::Point2f probe(123.4f, 87.6f);
    const cv::Point3f r3       = c.imageToRobot(probe);
    const cv::Point2f roundTrip = c.robotToImage(r3);
    QVERIFY(cv::norm(roundTrip - probe) < 1e-3);
}

void TestCalibrator::imageToRobot_zOnFittedPlane()
{
    // Build a tilted work plane z = 0.01 * x - 0.02 * y + 5 in robot frame and
    // verify imageToRobot recovers Z lying on that exact plane.
    std::vector<cv::Point2f> img;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            img.emplace_back(static_cast<float>(c * 50 + 100),
                             static_cast<float>(r * 50 + 100));
    const auto rob = applyAffine3D(img, 0.0, {0, 0}, 1.0,
                                   /*noise*/ 0.0,
                                   /*tiltA*/ 0.01,
                                   /*tiltB*/ -0.02,
                                   /*tiltC*/ 5.0);

    calib::Calibrator c;
    c.addCorrespondences(img, rob);
    QVERIFY(c.calibrate());
    QVERIFY(c.planeFitMaxErrorMm() < 1e-3);

    // Probe an interior point; Z should follow the same plane equation.
    const cv::Point2f probe(175.0f, 175.0f);
    const cv::Point3f r3 = c.imageToRobot(probe);
    const double zPlane  = 0.01 * r3.x - 0.02 * r3.y + 5.0;
    QVERIFY(std::abs(r3.z - zPlane) < 1e-3);
}

void TestCalibrator::rotateImageToRobot_roundTripIsExact()
{
    std::vector<cv::Point2f> img;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            img.emplace_back(static_cast<float>(c * 30), static_cast<float>(r * 30));
    const auto rob = applyAffine3D(img, 17.5, {50, -20}, 0.8);

    calib::Calibrator c;
    c.addCorrespondences(img, rob);
    QVERIFY(c.calibrate());

    for (double a : {0.0, 10.0, 30.0, 90.0, 180.0, -45.0, 270.0}) {
        const double rob_ang  = c.rotateImageToRobot(a);
        const double back_ang = c.rotateRobotToImage(rob_ang);
        // Normalise both to (-180, 180].
        auto norm = [](double x){
            while (x >  180.0) x -= 360.0;
            while (x <= -180.0) x += 360.0;
            return x;
        };
        QVERIFY(std::abs(norm(back_ang - a)) < 1e-3);
    }
}

void TestCalibrator::rotateImageToRobot_pureRotationMatchesAppliedAngle()
{
    // Build a pure-rotation similarity (no Y reflection) between image and
    // robot frames. Then rotateImageToRobot(theta) should produce a robot
    // angle equal to (theta + applied_rotation), up to sign of the image Y
    // axis flip baked into the convention.
    //
    // image angle convention has +Y "down"; applyAffine3D treats robot +Y as
    // standard math up. Going through cv::findHomography preserves the
    // mathematical transform exactly. The empirical method must agree.
    std::vector<cv::Point2f> img;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            img.emplace_back(static_cast<float>(c * 40),
                             static_cast<float>(r * 40));

    const double appliedRotDeg = 25.0;
    const auto rob = applyAffine3D(img, appliedRotDeg, {0, 0}, 1.0);

    calib::Calibrator c;
    c.addCorrespondences(img, rob);
    QVERIFY(c.calibrate());

    // Probe direction = image +X (angleImg = 0).
    const double robAngForZero = c.rotateImageToRobot(0.0);
    // image direction (1, 0) -> in image coords just (1,0). Through pure-rotation
    // affine with angle alpha, vector becomes (cos a, sin a) which is robot angle alpha.
    QVERIFY(std::abs(robAngForZero - appliedRotDeg) < 1e-2);
}

void TestCalibrator::toJson_fromJson_preservesFullState()
{
    std::vector<cv::Point2f> img;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            img.emplace_back(static_cast<float>(c * 30),
                             static_cast<float>(r * 30));
    const auto rob = applyAffine3D(img, 11.0, {7, -3}, 0.7,
                                   0.0, 0.004, -0.002, 8.0);

    calib::Calibrator c;
    c.addCorrespondences(img, rob);
    QVERIFY(c.calibrate());

    const std::string json = c.toJson();
    QVERIFY(!json.empty());
    QVERIFY(json.find("homography_image_to_robot") != std::string::npos);

    calib::Calibrator restored;
    QVERIFY(restored.fromJson(json));
    QVERIFY(restored.isCalibrated());
    QCOMPARE(restored.sampleCount(), c.sampleCount());

    // Homography matches bit-for-bit.
    QVERIFY(cv::norm(c.homography() - restored.homography()) < 1e-12);

    // Plane matches.
    const cv::Vec4d p1 = c.workPlane();
    const cv::Vec4d p2 = restored.workPlane();
    for (int i = 0; i < 4; ++i)
        QVERIFY(std::abs(p1[i] - p2[i]) < 1e-12);

    // Diagnostics preserved.
    QVERIFY(std::abs(c.reprojectionErrorPx()  - restored.reprojectionErrorPx())  < 1e-12);
    QVERIFY(std::abs(c.reprojectionErrorMm()  - restored.reprojectionErrorMm())  < 1e-12);
    QVERIFY(std::abs(c.planeFitMaxErrorMm()   - restored.planeFitMaxErrorMm())   < 1e-9);

    // Functional check: same input -> same output for image->robot and angle.
    const cv::Point2f probe(55.0f, 17.5f);
    const cv::Point3f r1 = c.imageToRobot(probe);
    const cv::Point3f r2 = restored.imageToRobot(probe);
    QVERIFY(std::abs(r1.x - r2.x) < 1e-4);
    QVERIFY(std::abs(r1.y - r2.y) < 1e-4);
    QVERIFY(std::abs(r1.z - r2.z) < 1e-4);

    QVERIFY(std::abs(c.rotateImageToRobot(42.0) - restored.rotateImageToRobot(42.0)) < 1e-6);
}

void TestCalibrator::saveLoad_homographyAndPlanePreserved()
{
    std::vector<cv::Point2f> img;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            img.emplace_back(static_cast<float>(c * 30), static_cast<float>(r * 30));
    const auto rob = applyAffine3D(img, 3.0, {5, -2}, 0.8,
                                   0.0, 0.005, -0.003, 12.0);

    calib::Calibrator c;
    c.addCorrespondences(img, rob);
    QVERIFY(c.calibrate());

    const QString file = QDir::temp().filePath("calib_test.yml");
    QVERIFY(c.save(file.toStdString()));

    calib::Calibrator loaded;
    QVERIFY(loaded.load(file.toStdString()));
    QVERIFY(loaded.isCalibrated());

    const cv::Mat hDiff = c.homography() - loaded.homography();
    QVERIFY(cv::norm(hDiff) < 1e-9);

    const cv::Vec4d p1 = c.workPlane();
    const cv::Vec4d p2 = loaded.workPlane();
    for (int i = 0; i < 4; ++i)
        QVERIFY(std::abs(p1[i] - p2[i]) < 1e-9);

    const cv::Point2f probe(42.0f, 13.5f);
    const cv::Point3f r1 = c.imageToRobot(probe);
    const cv::Point3f r2 = loaded.imageToRobot(probe);
    QVERIFY(std::abs(r1.x - r2.x) < 1e-4);
    QVERIFY(std::abs(r1.y - r2.y) < 1e-4);
    QVERIFY(std::abs(r1.z - r2.z) < 1e-4);
}

int main(int argc, char** argv)
{
    int status = 0;
    {
        TestFanucIRvisionBoard t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestCalibrationBoardFactory t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestCalibrator t;
        status |= QTest::qExec(&t, argc, argv);
    }
    return status;
}

#include "tst_main.moc"
