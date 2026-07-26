#ifndef FANUC_IRVISION_BOARD_H
#define FANUC_IRVISION_BOARD_H

#include "calibration_board.h"

namespace calib {

/// Fanuc iRVision-style dot grid calibration board (concrete CalibrationBoard).
/// Pattern: R x C grid of dots with 4 visual roles:
///   - Normal dots : small solid black dots filling the grid
///   - Target dots : 8 ring dots (4 corners + 4 edge midpoints), white inner
///   - Centre dot  : 1 larger ring dot at the grid centre, white inner
///   - Coord dots  : 3 larger solid dots near centre encoding +X (two dots
///                   right of centre) and +Y (one dot above centre)
/// @note rows and cols MUST be odd; rows >= 5 and cols >= 7.
class FanucIRvisionBoard : public CalibrationBoard
{
public:
    /// Geometry and detection parameters for a FanucIRvisionBoard instance.
    struct Params {
        int    rows                  = 11;    ///< Grid row count; must be odd, >= 5.
        int    cols                  = 11;    ///< Grid column count; must be odd, >= 7.
        double dotSpacingMm          = 15.0;  ///< Centre-to-centre spacing between adjacent dots, in mm.
        double marginMm              = 15.0;  ///< Blank border around the grid on the generated image, in mm.
        /// Diameter of (a) plain solid normal dots, and (b) outer of target rings.
        double normalDotSizeMm       = 4.0;
        /// Diameter of (a) outer of the centre ring, and (b) the 3 solid coord dots.
        double coorDotSizeMm         = 8.0;
        /// Inner white circle of target rings and the centre ring.
        double innerTargetDotSizeMm  = 1.0;
        /// Binarization threshold used by detect(): -1 => auto (Otsu),
        /// 0..255 => fixed manual threshold. Defaults to auto so existing
        /// presets / saved JSON keep their previous behaviour.
        int    binarizeThreshold     = -1;

        /// Checks internal consistency of the parameter set (odd/min row-col counts,
        /// positive spacings, and the normal < coor dot-size < spacing ordering).
        /// @param errorOut if non-null and validation fails, receives a description of the first violated rule
        /// @return true when all fields are within their required ranges/relationships
        bool isValid(std::string* errorOut = nullptr) const;
    };

    /// Built-in preset: iRVision part with 5 mm dot pitch (17x17 grid).
    constexpr static const Params iRvisionPattern5mm  {17, 17,  5.0,    10.0,   2.0,    4,      1.0,    -1};
    /// Built-in preset: iRVision part with 11 mm dot pitch (11x11 grid).
    constexpr static const Params iRvisionPattern11m  {11, 11,  11.5,   10.0,   2.0,    4,      1.0,    -1};
    /// Built-in preset: iRVision part with 15 mm dot pitch (11x11 grid).
    constexpr static const Params iRvisionPattern15mm {11, 11,  15.0,   10.0,   6.0,    11.0,   1.0,    -1};
    /// Built-in preset: iRVision part with 22 mm dot pitch (11x11 grid).
    constexpr static const Params iRvisionPattern22mm {11, 11,  22.5,   10.0,   10.0,   16.0,   1.0,    -1};
    /// Built-in preset: iRVision part with 30 mm dot pitch (17x17 grid).
    constexpr static const Params iRvisionPattern30mm {17, 17,  30.0,   10.0,   2,      4.0,    1.0,    -1};

    /// Constructs the board with the given params.
    /// @param params board geometry/detection parameters (defaults to the standard 11x11, 15mm layout)
    /// @note throws std::invalid_argument if `params` fails Params::isValid().
    explicit FanucIRvisionBoard(const Params& params = Params{11, 11, 15.0, 15.0, 4.0, 8.0, 1.0, -1});

    /// @return the board's current parameter set.
    const Params& params() const { return m_params; }

    // --- CalibrationBoard interface ---
    /// Synthesizes the board as a grayscale dot-grid image (white background,
    /// black dots per DotRole, using the configured dot sizes and margin).
    /// @param pixelsPerMm rasterization scale used to convert the mm-based Params into pixels
    /// @return an 8-bit single-channel (CV_8UC1) image of the generated board
    cv::Mat generateImage(double pixelsPerMm = 10.0) const override;

    /// Detects the dot grid in `image`: binarizes, extracts near-circular blobs,
    /// identifies the 4 orientation markers (centre + 3 coord dots) by geometry,
    /// then projects the expected grid and matches each blob to its nearest cell.
    /// @param image input image (colour or grayscale)
    /// @param imagePoints output; on success, one detected point per grid cell in row-major order
    /// @param cornerImagePoints optional output; the 4 corner dot positions (TL, TR, BR, BL) when non-null
    /// @param debugOverlay optional output; a BGR visualisation of the detected markers/grid/corners when non-null
    /// @return true if the full grid (all totalDots() cells) was matched; false otherwise (imagePoints left empty)
    bool    detect(const cv::Mat& image,
                   std::vector<cv::Point2f>& imagePoints,
                   std::vector<cv::Point2f>* cornerImagePoints = nullptr,
                   cv::Mat* debugOverlay = nullptr) const override;

    /// @return the total number of dots in the grid (rows * cols).
    int                      totalDots()            const override { return m_params.rows * m_params.cols; }
    /// @return the 3D (Z=0) object-space position of every grid dot, in row-major order, scaled by dotSpacingMm.
    std::vector<cv::Point3f> objectPoints()         const override;
    /// @return the 2D (XY only) object-space position of every grid dot, in row-major order.
    std::vector<cv::Point2f> objectPointsXY()       const override;
    /// @return the 2D object-space position of the 4 corner dots (TL, TR, BR, BL).
    std::vector<cv::Point2f> cornerObjectPointsXY() const override;

    /// @return the configured binarization threshold (-1 for auto/Otsu, else 0..255).
    int  binarizeThreshold() const override { return m_params.binarizeThreshold; }
    /// Sets the binarization threshold used by detect(), clamping to the valid range.
    /// @param threshold desired threshold; values < 0 collapse to -1 (auto/Otsu), values > 255 clamp to 255
    void setBinarizeThreshold(int threshold) override {
        m_params.binarizeThreshold =
            (threshold < 0) ? -1 : (threshold > 255 ? 255 : threshold);
    }

    /// @return the board type identifier ("FanucIRvision").
    std::string typeName() const override { return "FanucIRvision"; }

    /// Reads the Params from a JSON document produced by toJson().
    /// @param json serialised board JSON
    /// @return the parsed, validated Params
    /// @note throws std::invalid_argument if the JSON cannot be opened or any parsed field is invalid.
    static FanucIRvisionBoard::Params paramsFromJson(const std::string& json);

    // --- iRVision-specific accessors ---
    /// @return the row-major grid indices of the 8 target ring dots (4 corners + 4 edge midpoints).
    std::vector<int> targetIndices() const;
    /// @return the row-major grid index of the single centre ring dot, as a size-1 vector.
    std::vector<int> centerIndex()   const;
    /// @return the row-major grid indices of the 3 coord dots, in [xMid, xTip, yTip] order.
    std::vector<int> coordIndices()  const;
    /// @return the row-major grid indices of the 4 orientation markers, in [centre, xMid, xTip, yTip] order.
    std::vector<int> markerIndices() const;

protected:
    /// Writes this board's Params fields into `fs` (used by CalibrationBoard::toJson()).
    void writeJsonFields(cv::FileStorage& fs) const override;

private:
    /// A single grid cell location, identified by zero-based row and column.
    struct GridCell { int row; int col; };

    /// @return the grid cells of the 8 target ring dots (4 corners + 4 edge midpoints).
    std::vector<GridCell> targetCells() const;
    /// @return the grid cell of the centre ring dot.
    GridCell              centerCell() const;
    /// @return the grid cells of the 3 coord dots, in [xMid, xTip, yTip] order.
    std::vector<GridCell> coordCells() const;
    /// @return the grid cells of the 4 corner dots (TL, TR, BR, BL).
    std::vector<GridCell> cornerCells() const;

    /// Visual role a grid dot can play: plain fill dot, one of the 8 target rings,
    /// the single centre ring, or one of the 3 solid coordinate-axis dots.
    enum class DotRole { Normal, Target, Center, Coord };
    /// Classifies the dot at (row, col) by comparing it against the centre/coord/target cell sets.
    /// @return DotRole::Normal unless (row, col) matches a centre, coord, or target cell.
    DotRole roleOf(int row, int col) const;

    Params m_params;   ///< This board's geometry and detection parameters.
};

} // namespace calib

#endif // FANUC_IRVISION_BOARD_H
