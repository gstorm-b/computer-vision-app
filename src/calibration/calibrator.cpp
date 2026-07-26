#include "calibrator.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core/persistence.hpp>

#include <algorithm>
#include <cmath>

/// Implementation of the Calibrator class: homography + work-plane fitting,
/// angle conversion, and cv::FileStorage-based persistence (see calibrator.h).
namespace calib {

/// File-local helpers used only by Calibrator's implementation: plane
/// fitting, plane-Z lookup, and FileStorage state (de)serialization.
namespace {

/// Fits a plane ax + by + cz + d = 0 through `pts` by SVD on the centred
/// coordinates, then normalizes the normal and flips its sign so c >= 0
/// (keeps a non-vertical work plane's normal pointing toward +Z).
/// @param pts robot-space points to fit; need at least 3
/// @param plane output plane coefficients (a, b, c, d) with a^2+b^2+c^2 = 1
/// @return false if pts.size() < 3, the SVD result is degenerate (fewer than
///         3 rows in vt), or the resulting normal is ~zero length
bool fitPlane(const std::vector<cv::Point3f>& pts, cv::Vec4d& plane)
{
    if (pts.size() < 3) return false;

    cv::Vec3d centroid(0.0, 0.0, 0.0);
    for (const auto& p : pts) {
        centroid[0] += p.x;
        centroid[1] += p.y;
        centroid[2] += p.z;
    }
    centroid *= (1.0 / static_cast<double>(pts.size()));

    cv::Mat A(static_cast<int>(pts.size()), 3, CV_64F);
    for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
        A.at<double>(i, 0) = pts[i].x - centroid[0];
        A.at<double>(i, 1) = pts[i].y - centroid[1];
        A.at<double>(i, 2) = pts[i].z - centroid[2];
    }

    cv::Mat w, u, vt;
    cv::SVD::compute(A, w, u, vt, cv::SVD::MODIFY_A);
    if (vt.rows < 3) return false;

    const cv::Vec3d normal(vt.at<double>(2, 0),
                           vt.at<double>(2, 1),
                           vt.at<double>(2, 2));
    const double nLen = std::sqrt(normal[0]*normal[0]
                                + normal[1]*normal[1]
                                + normal[2]*normal[2]);
    if (nLen < 1e-9) return false;

    plane[0] = normal[0] / nLen;
    plane[1] = normal[1] / nLen;
    plane[2] = normal[2] / nLen;
    plane[3] = -(plane[0] * centroid[0]
               + plane[1] * centroid[1]
               + plane[2] * centroid[2]);

    // Sign convention: keep the normal pointing toward +Z when possible so that
    // c > 0 (a non-vertical work plane has a well-defined z = f(x, y)).
    if (plane[2] < 0.0) {
        plane[0] = -plane[0];
        plane[1] = -plane[1];
        plane[2] = -plane[2];
        plane[3] = -plane[3];
    }
    return true;
}

/// Evaluates the fitted plane at (x, y) to get the corresponding z.
/// @return 0.0 if the plane is (near-)vertical (|c| < 1e-12), since z is then undefined
double planeZ(const cv::Vec4d& plane, double x, double y)
{
    const double a = plane[0], b = plane[1], c = plane[2], d = plane[3];
    if (std::abs(c) < 1e-12) return 0.0;   // vertical plane: z undefined
    return -(a * x + b * y + d) / c;
}

constexpr double kDeg2Rad = CV_PI / 180.0;   ///< Degrees-to-radians conversion factor.
constexpr double kRad2Deg = 180.0 / CV_PI;   ///< Radians-to-degrees conversion factor.

/// Serializes the full calibrator state (homography, work plane, centroids,
/// raw correspondences, and diagnostics) into an already-open cv::FileStorage;
/// shared by save() (file-backed) and toJson() (in-memory JSON).
/// @param fs target storage, left open; the caller releases/retrieves it
/// @param calibrated written as the integer "calibrated" flag (1 or 0)
void writeStateTo(cv::FileStorage& fs,
                  const cv::Mat& H,
                  const cv::Vec4d& plane,
                  const cv::Point2f& imgC,
                  const cv::Point2f& robC,
                  const std::vector<cv::Point2f>& imagePts,
                  const std::vector<cv::Point3f>& robotPts,
                  double reprojErrPx,
                  double reprojErrMm,
                  double planeFitMaxErr,
                  bool calibrated)
{
    fs << "calibrated"                << (calibrated ? 1 : 0);
    fs << "homography_image_to_robot" << H;
    fs << "work_plane_abcd"           << cv::Mat(plane, false);
    fs << "image_centroid"            << cv::Mat(cv::Vec2f(imgC.x, imgC.y), false);
    fs << "robot_centroid"            << cv::Mat(cv::Vec2f(robC.x, robC.y), false);
    fs << "reprojection_error_px"     << reprojErrPx;
    fs << "reprojection_error_mm"     << reprojErrMm;
    fs << "plane_fit_max_error_mm"    << planeFitMaxErr;
    fs << "sample_count"              << static_cast<int>(imagePts.size());
    fs << "image_points"              << imagePts;
    fs << "robot_points"              << robotPts;
}

} // namespace

// =====================================================================
// Construction / state
// =====================================================================
/// Default-constructs an uncalibrated instance (see clear() for the initial
/// field values).
Calibrator::Calibrator() = default;

/// Resets all accumulated correspondences, the fitted homography/plane,
/// cached centroids, and diagnostics back to their initial (uncalibrated) state.
void Calibrator::clear()
{
    m_imagePts.clear();
    m_robotPts.clear();
    m_H.release();
    m_Hinv.release();
    m_plane          = cv::Vec4d(0.0, 0.0, 1.0, 0.0);
    m_imgCentroid    = {};
    m_robCentroid    = {};
    m_calibrated     = false;
    m_reprojErrorPx  = -1.0;
    m_reprojErrorMm  = -1.0;
    m_planeFitMaxErr = -1.0;
}

/// Appends image/robot point correspondences to the accumulated dataset used
/// by the next calibrate() call; each robot point carries its measured Z so a
/// (possibly tilted) work plane can be fitted later.
/// @note no-op if the two arrays differ in length; also marks the calibrator
///       as no longer calibrated since the fit is now stale
void Calibrator::addCorrespondences(const std::vector<cv::Point2f>& imagePoints,
                                    const std::vector<cv::Point3f>& robotPointsMm)
{
    if (imagePoints.size() != robotPointsMm.size()) return;
    m_imagePts.insert(m_imagePts.end(), imagePoints.begin(),  imagePoints.end());
    m_robotPts.insert(m_robotPts.end(), robotPointsMm.begin(), robotPointsMm.end());
    m_calibrated = false;
}

// =====================================================================
// Calibration
// =====================================================================
/// Fits the image<->robot homography (RANSAC) and the 3D work plane from the
/// accumulated correspondences, then computes reprojection/plane-fit
/// diagnostics and caches the point centroids used by the angle-conversion
/// helpers. Leaves the calibrator uncalibrated if there are fewer than 4
/// correspondences, the homography could not be estimated or inverted, or
/// the plane fit failed.
bool Calibrator::calibrate(double ransacReprojThreshold)
{
    m_calibrated = false;
    m_H.release();
    m_Hinv.release();

    if (m_imagePts.size() < 4) return false;
    if (m_imagePts.size() != m_robotPts.size()) return false;

    // ---- 1. Homography from image px to robot XY ----
    std::vector<cv::Point2f> robotXY;
    robotXY.reserve(m_robotPts.size());
    for (const auto& p : m_robotPts) robotXY.emplace_back(p.x, p.y);

    cv::Mat H = cv::findHomography(m_imagePts, robotXY, cv::RANSAC, ransacReprojThreshold);
    if (H.empty()) return false;

    cv::Mat Hinv;
    try { Hinv = H.inv(); } catch (const cv::Exception&) { return false; }
    if (Hinv.empty()) return false;

    m_H    = H;
    m_Hinv = Hinv;

    // ---- 2. Work plane (3D) fit through robot 3D points ----
    if (!fitPlane(m_robotPts, m_plane)) return false;

    // ---- 3. Diagnostics ----
    std::vector<cv::Point2f> projRobotXY, projImage;
    cv::perspectiveTransform(m_imagePts, projRobotXY, m_H);
    cv::perspectiveTransform(robotXY,    projImage,   m_Hinv);

    double sumMm = 0.0, sumPx = 0.0, maxZErr = 0.0;
    for (size_t i = 0; i < m_imagePts.size(); ++i) {
        sumMm += cv::norm(projRobotXY[i] - robotXY[i]);
        sumPx += cv::norm(projImage[i]   - m_imagePts[i]);
        const double zPredicted = planeZ(m_plane, m_robotPts[i].x, m_robotPts[i].y);
        maxZErr = std::max(maxZErr, std::abs(zPredicted - m_robotPts[i].z));
    }
    const double n = static_cast<double>(m_imagePts.size());
    m_reprojErrorMm  = sumMm  / n;
    m_reprojErrorPx  = sumPx  / n;
    m_planeFitMaxErr = maxZErr;

    // ---- 4. Cache centroids used as anchors for angle conversions ----
    cv::Point2f imgC(0.f, 0.f), robC(0.f, 0.f);
    for (const auto& p : m_imagePts) imgC += p;
    for (const auto& p : m_robotPts) robC += cv::Point2f(p.x, p.y);
    m_imgCentroid = imgC * (1.0f / static_cast<float>(n));
    m_robCentroid = robC * (1.0f / static_cast<float>(n));

    m_calibrated = true;
    return true;
}

// =====================================================================
// image <-> robot
// =====================================================================
/// Maps a single image pixel to robot mm (X, Y) via the homography, with Z
/// read off the fitted work plane at that (X, Y).
cv::Point3f Calibrator::imageToRobot(const cv::Point2f& imagePx) const
{
    if (!m_calibrated) return { 0.f, 0.f, 0.f };
    std::vector<cv::Point2f> in{ imagePx }, out;
    cv::perspectiveTransform(in, out, m_H);
    const cv::Point2f& xy = out.front();
    const double z = planeZ(m_plane, xy.x, xy.y);
    return cv::Point3f(xy.x, xy.y, static_cast<float>(z));
}

/// Maps a single robot-space point back to image pixels via the inverse
/// homography; the input Z is ignored.
cv::Point2f Calibrator::robotToImage(const cv::Point3f& robotMm) const
{
    if (!m_calibrated) return { 0.f, 0.f };
    std::vector<cv::Point2f> in{ cv::Point2f(robotMm.x, robotMm.y) }, out;
    cv::perspectiveTransform(in, out, m_Hinv);
    return out.front();
}

/// Batch version of imageToRobot(const cv::Point2f&): maps every image pixel
/// to robot (X, Y, Z) using a single perspectiveTransform call.
/// @return empty vector if uncalibrated or imagePts is empty
std::vector<cv::Point3f> Calibrator::imageToRobot(const std::vector<cv::Point2f>& imagePts) const
{
    std::vector<cv::Point3f> out;
    if (!m_calibrated || imagePts.empty()) return out;
    std::vector<cv::Point2f> xy;
    cv::perspectiveTransform(imagePts, xy, m_H);
    out.reserve(xy.size());
    for (const auto& p : xy) {
        const double z = planeZ(m_plane, p.x, p.y);
        out.emplace_back(p.x, p.y, static_cast<float>(z));
    }
    return out;
}

/// Batch version of robotToImage(const cv::Point3f&): maps every robot XY
/// (Z ignored) back to image pixels using a single perspectiveTransform call.
/// @return empty vector if uncalibrated or robotPts is empty
std::vector<cv::Point2f> Calibrator::robotToImage(const std::vector<cv::Point3f>& robotPts) const
{
    std::vector<cv::Point2f> out;
    if (!m_calibrated || robotPts.empty()) return out;
    std::vector<cv::Point2f> xy;
    xy.reserve(robotPts.size());
    for (const auto& p : robotPts) xy.emplace_back(p.x, p.y);
    cv::perspectiveTransform(xy, out, m_Hinv);
    return out;
}

// =====================================================================
// Angle conversion
// =====================================================================
/// Converts an image-plane angle to the equivalent robot-plane angle by
/// mapping a unit-direction step, anchored at the dataset image centroid,
/// through the homography H and reading off the resulting direction's angle.
/// @note Empirical method: anchoring at the centroid keeps the angle estimate
///       well-conditioned even when H carries mild perspective distortion.
///       Image convention: CCW from +X with image Y pointing down, i.e.
///       direction(theta) = (cos theta, -sin theta). Robot convention: CCW
///       from +X around +Z (right-hand rule, top-down), i.e.
///       direction(R) = (cos R, sin R).
double Calibrator::rotateImageToRobot(double angleImg, bool radians) const
{
    if (!m_calibrated) return 0.0;
    const double theta = radians ? angleImg : angleImg * kDeg2Rad;

    const float step = 100.0f;   // arbitrary length; only angle of result matters
    const cv::Point2f imgDir(static_cast<float>( std::cos(theta)),
                             static_cast<float>(-std::sin(theta)));
    const cv::Point2f imgP0 = m_imgCentroid;
    const cv::Point2f imgP1 = m_imgCentroid + imgDir * step;

    std::vector<cv::Point2f> in{ imgP0, imgP1 }, out;
    cv::perspectiveTransform(in, out, m_H);
    const cv::Point2f robDir = out[1] - out[0];
    const double angleRob = std::atan2(robDir.y, robDir.x);
    return radians ? angleRob : angleRob * kRad2Deg;
}

/// Converts a robot-plane angle to the equivalent image-plane angle: the
/// inverse of rotateImageToRobot(), mapping a unit-direction step anchored at
/// the robot centroid through the inverse homography Hinv.
double Calibrator::rotateRobotToImage(double angleRob, bool radians) const
{
    if (!m_calibrated) return 0.0;
    const double theta = radians ? angleRob : angleRob * kDeg2Rad;

    const float step = 100.0f;
    const cv::Point2f robDir(static_cast<float>(std::cos(theta)),
                             static_cast<float>(std::sin(theta)));
    const cv::Point2f robP0 = m_robCentroid;
    const cv::Point2f robP1 = m_robCentroid + robDir * step;

    std::vector<cv::Point2f> in{ robP0, robP1 }, out;
    cv::perspectiveTransform(in, out, m_Hinv);
    const cv::Point2f imgDir = out[1] - out[0];
    const double angleImg = std::atan2(-imgDir.y, imgDir.x);
    return radians ? angleImg : angleImg * kRad2Deg;
}

/// Rotates `offset` by `theta` about the Z axis and adds it to `A`, giving
/// B = A + Rz(theta) * offset (the Z component of offset passes through unchanged).
cv::Point3f Calibrator::translateWithZAxis(const cv::Point3f &A, const cv::Point3f &offset, double theta, double isRad) const
{
    double theta_rad = theta;
    if (!isRad) {
        theta_rad = (CV_PI / 180.0) * theta;
    }

    double x_new = offset.x * cos(theta_rad) - offset.y * sin(theta_rad);
    double y_new = offset.x * sin(theta_rad) + offset.y * cos(theta_rad);
    double z_new = offset.z;

    cv::Point3f B;
    B.x = A.x + x_new;
    B.y = A.y + y_new;
    B.z = A.z + z_new;
    return B;
}

// =====================================================================
// Persistence
// =====================================================================
/// Writes the full calibrator state to a cv::FileStorage-backed file (format
/// determined by the file extension, e.g. YAML/XML).
bool Calibrator::save(const std::string& filePath) const
{
    if (!m_calibrated) return false;
    cv::FileStorage fs(filePath, cv::FileStorage::WRITE);
    if (!fs.isOpened()) return false;
    writeStateTo(fs, m_H, m_plane, m_imgCentroid, m_robCentroid,
                 m_imagePts, m_robotPts,
                 m_reprojErrorPx, m_reprojErrorMm, m_planeFitMaxErr,
                 m_calibrated);
    fs.release();
    return true;
}

/// Loads calibrator state from a cv::FileStorage-backed file via readFromStorage().
bool Calibrator::load(const std::string& filePath)
{
    cv::FileStorage fs(filePath, cv::FileStorage::READ);
    if (!fs.isOpened()) return false;
    const bool ok = readFromStorage(fs);
    fs.release();
    return ok;
}

// =====================================================================
// Shared parser used by load(file) and fromJson(string)
// =====================================================================
/// Reads homography, work plane, centroids, raw correspondences, and
/// diagnostics from `fs` and commits them to the instance; used by both
/// load(file) and fromJson(string).
/// @note on success, m_calibrated ends up true whenever a valid homography
///       was read, regardless of the stored "calibrated" flag (H is
///       guaranteed non-empty by the time m_calibrated is assigned)
bool Calibrator::readFromStorage(cv::FileStorage& fs)
{
    int calibFlag = 0;
    if (!fs["calibrated"].empty()) fs["calibrated"] >> calibFlag;

    cv::Mat H;
    fs["homography_image_to_robot"] >> H;
    if (H.empty() || H.rows != 3 || H.cols != 3) return false;

    cv::Mat Hinv;
    try { Hinv = H.inv(); } catch (const cv::Exception&) { return false; }
    if (Hinv.empty()) return false;

    cv::Mat planeMat;
    fs["work_plane_abcd"] >> planeMat;
    if (planeMat.empty() || planeMat.total() < 4) return false;
    cv::Vec4d plane(planeMat.at<double>(0),
                    planeMat.at<double>(1),
                    planeMat.at<double>(2),
                    planeMat.at<double>(3));

    cv::Mat imgC, robC;
    cv::Point2f imgCentroid{}, robCentroid{};
    fs["image_centroid"] >> imgC;
    fs["robot_centroid"] >> robC;
    if (!imgC.empty() && imgC.total() >= 2)
        imgCentroid = cv::Point2f(imgC.at<float>(0), imgC.at<float>(1));
    if (!robC.empty() && robC.total() >= 2)
        robCentroid = cv::Point2f(robC.at<float>(0), robC.at<float>(1));

    std::vector<cv::Point2f> imagePts;
    std::vector<cv::Point3f> robotPts;
    if (!fs["image_points"].empty()) fs["image_points"] >> imagePts;
    if (!fs["robot_points"].empty()) fs["robot_points"] >> robotPts;

    double reprojErrPx   = -1.0;
    double reprojErrMm   = -1.0;
    double planeFitMaxEr = -1.0;
    fs["reprojection_error_px"]  >> reprojErrPx;
    fs["reprojection_error_mm"]  >> reprojErrMm;
    fs["plane_fit_max_error_mm"] >> planeFitMaxEr;

    // Commit.
    m_H              = H;
    m_Hinv           = Hinv;
    m_plane          = plane;
    m_imgCentroid    = imgCentroid;
    m_robCentroid    = robCentroid;
    m_imagePts       = std::move(imagePts);
    m_robotPts       = std::move(robotPts);
    m_reprojErrorPx  = reprojErrPx;
    m_reprojErrorMm  = reprojErrMm;
    m_planeFitMaxErr = planeFitMaxEr;
    m_calibrated     = (calibFlag != 0) || !m_H.empty();
    return true;
}

/// Serializes the full instance state to an in-memory JSON string via
/// writeStateTo() (same fields as save(), without touching disk).
std::string Calibrator::toJson() const
{
    cv::FileStorage fs(".json",
                       cv::FileStorage::WRITE | cv::FileStorage::MEMORY
                                              | cv::FileStorage::FORMAT_JSON);
    if (!fs.isOpened()) return {};
    writeStateTo(fs, m_H, m_plane, m_imgCentroid, m_robCentroid,
                 m_imagePts, m_robotPts,
                 m_reprojErrorPx, m_reprojErrorMm, m_planeFitMaxErr,
                 m_calibrated);
    return fs.releaseAndGetString();
}

/// Restores calibrator state from a JSON string produced by toJson(), via
/// clear() followed by readFromStorage(); the instance ends up in an
/// "already-calibrated" state without re-running the homography fit.
bool Calibrator::fromJson(const std::string& json)
{
    cv::FileStorage fs(json,
                       cv::FileStorage::READ | cv::FileStorage::MEMORY
                                             | cv::FileStorage::FORMAT_JSON);
    if (!fs.isOpened()) return false;
    clear();
    const bool ok = readFromStorage(fs);
    fs.release();
    return ok;
}

} // namespace calib
