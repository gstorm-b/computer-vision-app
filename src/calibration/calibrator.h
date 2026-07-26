#ifndef CALIBRATOR_H
#define CALIBRATOR_H

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>
#include <string>
#include <vector>

/// Calibration and coordinate-conversion utilities for mapping between camera
/// image space and robot world space.
namespace calib {

/// Holds a planar 2D <-> 3D calibration between the camera image and the robot
/// world. Internally it fits:
///   * a 3x3 homography H mapping image (px) <-> robot XY (mm) from N >= 4
///     point correspondences, AND
///   * a 3D plane (in robot mm) through the supplied 3D robot points so that
///     imageToRobot() can report a Z that follows a slightly-tilted work surface.
///
/// Angle conversions go through H empirically (mapped vectors at the dataset
/// centroid), with image angle measured counter-clockwise from +X in the
/// "Y-points-down" image frame and robot R measured counter-clockwise from +X
/// around +Z (standard right-hand-rule top-down). Degrees by default, radians
/// available via an optional flag.
class Calibrator
{
public:
    /// Constructs an empty, uncalibrated instance (no correspondences, identity
    /// state) — call addCorrespondences() and calibrate() before use.
    Calibrator();

    /// Resets all correspondences, the fitted homography/plane, cached
    /// centroids, and diagnostics back to their default (uncalibrated) state.
    void clear();

    /// Appends a batch of image/robot point correspondences to the working set
    /// used by the next calibrate() call; ignored (no-op) if the two vectors
    /// differ in size. Marks the instance as no-longer-calibrated.
    /// Each robot point carries its measured Z, so a (possibly tilted) work
    /// plane can be fitted during calibrate().
    /// @param imagePoints pixel coordinates observed in the camera image
    /// @param robotPointsMm corresponding robot-frame points in millimetres
    void addCorrespondences(const std::vector<cv::Point2f>& imagePoints,
                            const std::vector<cv::Point3f>& robotPointsMm);

    /// Fits the image-to-robot homography (via RANSAC) and the 3D work plane
    /// from the accumulated correspondences, then recomputes reprojection and
    /// plane-fit diagnostics and caches the point centroids used by the angle
    /// conversion methods. Requires at least 4 correspondences; fails and
    /// leaves the instance uncalibrated if the homography or plane fit fails.
    /// @param ransacReprojThreshold RANSAC inlier reprojection threshold (px) passed to cv::findHomography
    /// @return true on success, false if there are too few points or the fit failed
    bool calibrate(double ransacReprojThreshold = 3.0);

    /// Returns true once calibrate() (or a successful load()/fromJson()) has produced a usable homography.
    bool isCalibrated() const { return m_calibrated; }
    /// Returns a copy of the accumulated image-space correspondence points.
    std::vector<cv::Point2f>  getImagePts() const { return m_imagePts; };
    /// Returns a copy of the accumulated robot-space correspondence points.
    std::vector<cv::Point3f> getRobotPts() const { return m_robotPts; };

    /// Maps a single image point to robot space via the fitted homography.
    /// image (px) -> robot (mm). Returned Z lies on the fitted work plane.
    /// @param imagePx point in image pixel coordinates
    /// @return robot-frame point (mm) with Z evaluated from the fitted plane; (0,0,0) if not yet calibrated
    cv::Point3f imageToRobot(const cv::Point2f& imagePx) const;

    /// Maps a single robot point to image space via the inverse homography.
    /// robot (mm) -> image (px). Input Z is ignored; only XY is back-projected.
    /// @param robotMm point in robot-frame millimetres (Z component unused)
    /// @return corresponding image pixel coordinates; (0,0) if not yet calibrated
    cv::Point2f robotToImage(const cv::Point3f& robotMm) const;

    /// Batch form of imageToRobot(const cv::Point2f&): converts every point in one
    /// perspectiveTransform call. Returns an empty vector if uncalibrated or the input is empty.
    std::vector<cv::Point3f> imageToRobot(const std::vector<cv::Point2f>& imagePts) const;
    /// Batch form of robotToImage(const cv::Point3f&): converts every point in one
    /// perspectiveTransform call. Returns an empty vector if uncalibrated or the input is empty.
    std::vector<cv::Point2f> robotToImage(const std::vector<cv::Point3f>& robotPts) const;

    /// Converts an image-space direction angle to the equivalent robot-space
    /// rotation, empirically, by mapping a unit step from the cached image
    /// centroid through the homography H.
    /// Angle conversion. radians=false (default) interprets/returns degrees.
    /// image angle convention: CCW from +X, taken in the Y-down image frame.
    /// robot angle convention: CCW from +X around +Z (top-down view).
    /// @param angleImg image-space angle, in degrees unless radians is true
    /// @param radians when true, angleImg is radians and the result is radians
    /// @return equivalent robot-space angle, or 0.0 if not yet calibrated
    double rotateImageToRobot(double angleImg, bool radians = false) const;
    /// Converts a robot-space rotation angle to the equivalent image-space
    /// direction angle, empirically, by mapping a unit step from the cached
    /// robot centroid through the inverse homography Hinv.
    /// @param angleRob robot-space angle, in degrees unless radians is true
    /// @param radians when true, angleRob is radians and the result is radians
    /// @return equivalent image-space angle, or 0.0 if not yet calibrated
    double rotateRobotToImage(double angleRob, bool radians = false) const;

    /// Rotates `offset` by `theta` around the Z axis and adds it to `A`, i.e.
    /// translates point A by an offset vector that has itself been rotated
    /// about Z; the Z component of offset passes through unchanged.
    /// @param A base point to translate from
    /// @param offset translation vector, rotated about Z before being applied
    /// @param theta rotation angle about Z, in radians unless isRad is false
    /// @param isRad when false, theta is treated as degrees
    /// @return A plus the Z-rotated offset
    cv::Point3f translateWithZAxis(const cv::Point3f &A, const cv::Point3f &offset, double theta, double isRad = true) const;

    // --- Diagnostics ---
    /// Mean image-space (px) reprojection residual from the last calibrate(), or -1.0 if uncalibrated.
    double reprojectionErrorPx()   const { return m_reprojErrorPx; }
    double reprojectionErrorMm()   const { return m_reprojErrorMm; }   ///< Mean robot-space XY residual (mm) from the last calibrate(), or -1.0 if uncalibrated.
    double planeFitMaxErrorMm()    const { return m_planeFitMaxErr; } ///< Largest absolute robot-Z residual (mm) from the fitted work plane, or -1.0 if uncalibrated.

    cv::Mat   homography()        const { return m_H.clone(); }       ///< Returns a clone of the image(px) -> robot(mm XY) homography matrix.
    cv::Mat   homographyInverse() const { return m_Hinv.clone(); }    ///< Returns a clone of the robot(mm XY) -> image(px) inverse homography matrix.
    cv::Vec4d workPlane()         const { return m_plane; }           ///< Fitted work-plane coefficients (a,b,c,d) such that a*x + b*y + c*z + d = 0.

    /// Returns the number of accumulated image/robot correspondence points.
    size_t sampleCount() const { return m_imagePts.size(); }

    /// Serialises the current calibrated state (homography, plane, centroids,
    /// raw correspondences, diagnostics) to an OpenCV FileStorage file.
    /// @param filePath destination path; format is inferred by cv::FileStorage from the extension
    /// @return true on success; false if uncalibrated or the file could not be opened
    bool save(const std::string& filePath) const;
    /// Loads a previously save()-d state from an OpenCV FileStorage file,
    /// replacing the homography, plane, centroids, correspondences, and diagnostics.
    /// @param filePath path to a file previously written by save()
    /// @return true on success; false if the file could not be opened or is missing required fields
    bool load(const std::string& filePath);

    /// Serialises the current state to a JSON string (same fields as save()), via an in-memory cv::FileStorage.
    /// @return the JSON document as a string, or an empty string if the in-memory storage could not be opened
    std::string toJson()                       const;
    /// Restores the calibrator from a JSON string produced by toJson() (or an
    /// equivalent document), without re-running the homography/plane fit.
    /// Calls clear() first, so a failed parse leaves the instance uncalibrated.
    /// @param json JSON document as produced by toJson()
    /// @return true on success, false if the document could not be parsed or was missing required fields
    bool        fromJson(const std::string& json);

private:
    std::vector<cv::Point2f> m_imagePts;   ///< Accumulated image-space correspondence points (px).
    std::vector<cv::Point3f> m_robotPts;   ///< Accumulated robot-space correspondence points (mm), Z included for plane fitting.

    cv::Mat   m_H;      ///< Image(px) -> robot(mm XY) homography, fitted by calibrate() or restored by readFromStorage().
    cv::Mat   m_Hinv;   ///< Inverse of m_H: robot(mm XY) -> image(px).
    cv::Vec4d m_plane{ 0.0, 0.0, 1.0, 0.0 };   ///< Fitted work-plane coefficients (a,b,c,d); defaults to the z = 0 plane before calibration.

    cv::Point2f m_imgCentroid{};   ///< Cached centroid of the image-space correspondence points; anchor for rotateImageToRobot().
    cv::Point2f m_robCentroid{};   ///< Centroid of the robot-space correspondence points; anchor for rotateRobotToImage().

    bool   m_calibrated      = false;   ///< True once a usable homography/plane has been fitted or restored.
    double m_reprojErrorPx   = -1.0;    ///< Cached mean image-space reprojection error (px); -1.0 until calibrated.
    double m_reprojErrorMm   = -1.0;    ///< Cached mean robot-space XY reprojection error (mm); -1.0 until calibrated.
    double m_planeFitMaxErr  = -1.0;    ///< Cached largest robot-Z plane-fit residual (mm); -1.0 until calibrated.

    /// Shared parser used by both load(file) and fromJson(string): reads the
    /// homography, work plane, centroids, correspondences, and diagnostics out
    /// of an already-opened cv::FileStorage and commits them to this instance.
    /// @param fs an opened FileStorage (file- or memory-backed) positioned at the document root
    /// @return true if the required fields (homography, plane) were present and valid
    bool readFromStorage(cv::FileStorage& fs);
};

} // namespace calib

#endif // CALIBRATOR_H
