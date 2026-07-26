#ifndef CALIBRATION_BOARD_H
#define CALIBRATION_BOARD_H

#include <opencv2/core.hpp>
#include <string>
#include <vector>

/// Calibration board synthesis, detection, and factory types.
namespace calib {

/// Writes a PNG file with an embedded pHYs chunk so the printer / viewer can
/// render the image at the correct physical size (1:1 mm scale).
/// @param pxPerMm the rendering resolution of `img` (e.g. 300/25.4 for 300 DPI)
/// @return true on success
bool writePrintablePng(const std::string& path,
                       const cv::Mat& img,
                       double pxPerMm);

/// Abstract base class for calibration board synthesis/detection.
///
/// Every concrete board type (Fanuc iRVision, Halcon, ChArUco, ...) implements
/// this interface so that downstream code (Calibrator, GUI, automation scripts)
/// can stay board-agnostic.
///
/// Concrete subclasses live in their own headers, e.g. FanucIRvisionBoard in
/// "fanuc_irvision_board.h". Use CalibrationBoardFactory (in
/// "calibration_board_factory.h") to construct instances polymorphically.
class CalibrationBoard
{
public:
    /// Virtual destructor to allow safe deletion through a CalibrationBoard pointer.
    virtual ~CalibrationBoard() = default;

    // --- Synthesis ---
    /// Renders the board into a single-channel image at `pixelsPerMm` pixels per millimeter.
    virtual cv::Mat generateImage(double pixelsPerMm = 10.0) const = 0;

    /// Renders the board at `dpi` DPI and writes it as a PNG with an embedded
    /// pHYs chunk so that printer drivers reproduce the exact physical size.
    /// Default 300 DPI is fine for laser printers; use 600 DPI for finer ink.
    /// @return true on success (see writePrintablePng())
    bool writePrintableImage(const std::string& path, double dpi = 300.0) const;

    // --- Detection ---
    /// Detects the board's dot pattern in `image`.
    /// @param imagePoints all detected dot centres in row-major (objectPoints) order
    /// @param cornerImagePoints optional; receives the 4 corner dots used for homography
    ///        fit, ordered [TL, TR, BR, BL] in board coordinates
    /// @param debugOverlay optional; receives a BGR image with overlay annotations
    /// @return true if detection succeeded
    virtual bool detect(const cv::Mat& image,
                        std::vector<cv::Point2f>& imagePoints,
                        std::vector<cv::Point2f>* cornerImagePoints = nullptr,
                        cv::Mat* debugOverlay = nullptr) const = 0;

    /// Binarizes `image` (grayscale or BGR) into a single-channel mask where the
    /// dark board dots become foreground (255). When binarizeThreshold() < 0 the
    /// threshold is computed automatically (Otsu); otherwise the fixed value is
    /// applied (THRESH_BINARY_INV). If `usedThreshold` is non-null it receives
    /// the value actually applied (the Otsu-computed value in auto mode).
    /// @note detect() uses this same routine, so a preview built from binarize()
    ///       matches exactly what detection sees.
    virtual cv::Mat binarize(const cv::Mat& image,
                             double* usedThreshold = nullptr) const;

    /// Binarization threshold used by detect(): -1 => auto (Otsu), 0..255 => fixed
    /// manual threshold. The base defaults to auto; subclasses that expose the value
    /// through their Params override these.
    /// @return the configured threshold, or -1 for automatic (Otsu) thresholding
    virtual int  binarizeThreshold() const { return -1; }
    /// Base no-op; subclasses that expose a configurable threshold override this to
    /// store `threshold` for later use by binarizeThreshold().
    virtual void setBinarizeThreshold(int /*threshold*/) {}

    // --- Geometry (board coordinates in mm, z = 0 on a planar board) ---
    /// Returns the total number of dots on the board.
    virtual int                      totalDots()             const = 0;
    /// Returns all dot positions in board coordinates (mm, z = 0), row-major order.
    virtual std::vector<cv::Point3f> objectPoints()          const = 0;
    /// Returns all dot positions in board coordinates (mm) as 2D (x, y) points, same order as objectPoints().
    virtual std::vector<cv::Point2f> objectPointsXY()        const = 0;
    /// Returns the 4 corner dot positions in board coordinates (mm), ordered [TL, TR, BR, BL].
    virtual std::vector<cv::Point2f> cornerObjectPointsXY()  const = 0;

    /// Human-readable identifier of the concrete board type, e.g.
    /// "FanucIRvision", "HalconChessboard", ... Defaults to the C++ typeid name
    /// but subclasses are encouraged to return a stable string.
    virtual std::string typeName() const;

    // --- JSON serialisation ---
    /// Serializes this board to a JSON string. NVI: writes the type identifier and
    /// delegates the subclass-specific fields to writeJsonFields(). To rebuild a board
    /// instance from JSON, call CalibrationBoardFactory::createFromJson().
    std::string toJson() const;

    // --- Static helpers ---
    /// Converts a vector of 2D points to a single-column CV_32FC2 cv::Mat (deep copy).
    static cv::Mat pointsToMat(const std::vector<cv::Point2f>& pts);
    /// Converts a vector of 3D points to a single-column CV_32FC3 cv::Mat (deep copy).
    static cv::Mat pointsToMat(const std::vector<cv::Point3f>& pts);
    /// Converts a DPI value to pixels-per-millimeter (dpi / 25.4).
    static double  pixelsPerMmFromDpi(double dpi)     { return dpi / 25.4; }
    /// Converts a pixels-per-millimeter value to DPI (pxPerMm * 25.4).
    static double  dpiFromPixelsPerMm(double pxPerMm) { return pxPerMm * 25.4; }

protected:
    /// Writes subclass-specific JSON fields. Implementations write their Params under
    /// top-level keys; the "type" key is already emitted by toJson().
    virtual void writeJsonFields(cv::FileStorage& fs) const = 0;
};

} // namespace calib

#endif // CALIBRATION_BOARD_H
