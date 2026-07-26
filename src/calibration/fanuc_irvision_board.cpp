#include "fanuc_irvision_board.h"
#include "opencv2/imgcodecs.hpp"
#include <opencv2/core/persistence.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#define PI_NUM 3.141592653589793238462643383279502884197

namespace calib {

// =====================================================================
// Params validation
// =====================================================================
/// Checks internal consistency of the parameter set: rows/cols must be odd and
/// meet their minimums, spacings/sizes must be positive, and the dot-size ordering
/// normalDotSizeMm < coorDotSizeMm < dotSpacingMm (with innerTargetDotSizeMm < normalDotSizeMm)
/// must hold, plus binarizeThreshold must be -1 or in 0..255.
bool FanucIRvisionBoard::Params::isValid(std::string* errorOut) const
{
    auto fail = [errorOut](const char* msg) {
        if (errorOut) *errorOut = msg;
        return false;
    };

    if (rows < 5) return fail("rows must be >= 5");
    if (cols < 7) return fail("cols must be >= 7");
    if (rows % 2 == 0 || cols % 2 == 0) return fail("rows and cols must be odd");
    if (dotSpacingMm <= 0.0) return fail("dotSpacingMm must be > 0");
    if (marginMm     <  0.0) return fail("marginMm must be >= 0");
    if (innerTargetDotSizeMm <= 0.0)            return fail("innerTargetDotSizeMm must be > 0");
    if (normalDotSizeMm  <= innerTargetDotSizeMm)
        return fail("normalDotSizeMm must be greater than innerTargetDotSizeMm");
    if (coorDotSizeMm    <= normalDotSizeMm)
        return fail("coorDotSizeMm must be greater than normalDotSizeMm");
    if (coorDotSizeMm    >= dotSpacingMm)
        return fail("coorDotSizeMm must be smaller than dotSpacingMm");
    if (binarizeThreshold < -1 || binarizeThreshold > 255)
        return fail("binarizeThreshold must be -1 (auto/Otsu) or in 0..255");
    return true;
}

// =====================================================================
// Construction
// =====================================================================
/// Constructs the board, validating `params` up front.
/// @note throws std::invalid_argument (with the Params::isValid() error message) if `params` is invalid.
FanucIRvisionBoard::FanucIRvisionBoard(const Params& params)
    : m_params(params)
{
    std::string err;
    if (!m_params.isValid(&err))
        throw std::invalid_argument("FanucIRvisionBoard: invalid params - " + err);
}

// =====================================================================
// JSON serialisation
// =====================================================================
/// Writes every Params field (rows, cols, spacings, dot sizes, binarizeThreshold) into `fs`.
/// @param fs the OpenCV FileStorage to append to; invoked by CalibrationBoard::toJson()
void FanucIRvisionBoard::writeJsonFields(cv::FileStorage& fs) const
{
    fs << "rows"                 << m_params.rows;
    fs << "cols"                 << m_params.cols;
    fs << "dotSpacingMm"         << m_params.dotSpacingMm;
    fs << "marginMm"             << m_params.marginMm;
    fs << "normalDotSizeMm"      << m_params.normalDotSizeMm;
    fs << "coorDotSizeMm"        << m_params.coorDotSizeMm;
    fs << "innerTargetDotSizeMm" << m_params.innerTargetDotSizeMm;
    fs << "binarizeThreshold"    << m_params.binarizeThreshold;
}

/// Parses Params from a JSON document produced by writeJsonFields()/toJson(). The parsed Params
/// leaves binarizeThreshold at its default -1 when absent, for backward compatibility with JSON
/// saved before that field existed.
/// @note throws std::invalid_argument if the JSON cannot be opened as an OpenCV FileStorage.
FanucIRvisionBoard::Params FanucIRvisionBoard::paramsFromJson(const std::string& json)
{
    cv::FileStorage fs(json,
                       cv::FileStorage::READ | cv::FileStorage::MEMORY
                                             | cv::FileStorage::FORMAT_JSON);
    if (!fs.isOpened())
        throw std::invalid_argument("FanucIRvisionBoard::paramsFromJson: cannot open JSON");

    Params p;
    fs["rows"]                 >> p.rows;
    fs["cols"]                 >> p.cols;
    fs["dotSpacingMm"]         >> p.dotSpacingMm;
    fs["marginMm"]             >> p.marginMm;
    fs["normalDotSizeMm"]      >> p.normalDotSizeMm;
    fs["coorDotSizeMm"]        >> p.coorDotSizeMm;
    fs["innerTargetDotSizeMm"] >> p.innerTargetDotSizeMm;
    // Optional / backward-compatible: absent in older JSON -> keep default (-1).
    if (!fs["binarizeThreshold"].isNone())
        fs["binarizeThreshold"] >> p.binarizeThreshold;
    return p;
}

// =====================================================================
// Cell roles
// =====================================================================
/// Returns the 8 target-ring grid cells: the 4 grid corners plus the 4 edge midpoints.
std::vector<FanucIRvisionBoard::GridCell> FanucIRvisionBoard::targetCells() const
{
    const int rmid = (m_params.rows - 1) / 2;
    const int cmid = (m_params.cols - 1) / 2;
    const int rmax = m_params.rows - 1;
    const int cmax = m_params.cols - 1;
    return {
        { 0,    0    }, { 0,    cmax },
        { rmax, cmax }, { rmax, 0    },
        { 0,    cmid }, { rmax, cmid },
        { rmid, 0    }, { rmid, cmax }
    };
}

/// Returns the grid cell at the exact centre of the (odd rows x odd cols) grid.
FanucIRvisionBoard::GridCell FanucIRvisionBoard::centerCell() const
{
    return { (m_params.rows - 1) / 2, (m_params.cols - 1) / 2 };
}

/// Returns the 3 coord-dot grid cells, in [xMid, xTip, yTip] order: two cells to the right
/// of centre (encoding +X) and one cell above centre (encoding +Y).
std::vector<FanucIRvisionBoard::GridCell> FanucIRvisionBoard::coordCells() const
{
    const int rmid = (m_params.rows - 1) / 2;
    const int cmid = (m_params.cols - 1) / 2;
    return {
        { rmid,     cmid + 1 },
        { rmid,     cmid + 2 },
        { rmid - 1, cmid     },
    };
}

/// Returns the 4 corner grid cells, in [TL, TR, BR, BL] order.
std::vector<FanucIRvisionBoard::GridCell> FanucIRvisionBoard::cornerCells() const
{
    const int rmax = m_params.rows - 1;
    const int cmax = m_params.cols - 1;
    return { { 0, 0 }, { 0, cmax }, { rmax, cmax }, { rmax, 0 } };
}

/// Classifies the dot at (row, col) by checking it against the centre, coord, and
/// target cell sets in that priority order.
FanucIRvisionBoard::DotRole FanucIRvisionBoard::roleOf(int row, int col) const
{
    const auto centre = centerCell();
    if (centre.row == row && centre.col == col) return DotRole::Center;
    for (const auto& g : coordCells())  if (g.row == row && g.col == col) return DotRole::Coord;
    for (const auto& g : targetCells()) if (g.row == row && g.col == col) return DotRole::Target;
    return DotRole::Normal;
}

/// Converts targetCells() to flat row-major grid indices (row * cols + col).
std::vector<int> FanucIRvisionBoard::targetIndices() const
{
    std::vector<int> v;
    v.reserve(8);
    for (const auto& g : targetCells())
        v.push_back(g.row * m_params.cols + g.col);
    return v;
}

/// Converts centerCell() to a flat row-major grid index (row * cols + col).
std::vector<int> FanucIRvisionBoard::centerIndex() const
{
    const auto c = centerCell();
    return { c.row * m_params.cols + c.col };
}

/// Converts coordCells() to flat row-major grid indices (row * cols + col).
std::vector<int> FanucIRvisionBoard::coordIndices() const
{
    std::vector<int> v;
    v.reserve(3);
    for (const auto& g : coordCells())
        v.push_back(g.row * m_params.cols + g.col);
    return v;
}

/// Returns the 4 orientation-marker indices, in [centre, xMid, xTip, yTip] order
/// (centerIndex() followed by coordIndices()).
std::vector<int> FanucIRvisionBoard::markerIndices() const
{
    std::vector<int> v;
    v.push_back(centerIndex().front());
    for (int i : coordIndices()) v.push_back(i);
    return v;
}

// =====================================================================
// Object points
// =====================================================================
/// Returns the 3D (Z=0, in mm) position of every grid dot in row-major order:
/// x = col * dotSpacingMm, y = row * dotSpacingMm.
std::vector<cv::Point3f> FanucIRvisionBoard::objectPoints() const
{
    std::vector<cv::Point3f> pts;
    pts.reserve(totalDots());
    for (int r = 0; r < m_params.rows; ++r)
        for (int c = 0; c < m_params.cols; ++c)
            pts.emplace_back(static_cast<float>(c * m_params.dotSpacingMm),
                             static_cast<float>(r * m_params.dotSpacingMm),
                             0.0f);
    return pts;
}

/// Returns the 2D (XY only, in mm) position of every grid dot in row-major order;
/// same values as objectPoints() with the Z coordinate dropped.
std::vector<cv::Point2f> FanucIRvisionBoard::objectPointsXY() const
{
    std::vector<cv::Point2f> pts;
    pts.reserve(totalDots());
    for (int r = 0; r < m_params.rows; ++r)
        for (int c = 0; c < m_params.cols; ++c)
            pts.emplace_back(static_cast<float>(c * m_params.dotSpacingMm),
                             static_cast<float>(r * m_params.dotSpacingMm));
    return pts;
}

/// Returns the 2D object-space position (in mm) of the 4 corner dots, in
/// [TL, TR, BR, BL] order (via cornerCells()).
std::vector<cv::Point2f> FanucIRvisionBoard::cornerObjectPointsXY() const
{
    std::vector<cv::Point2f> pts;
    pts.reserve(4);
    for (const auto& g : cornerCells())
        pts.emplace_back(static_cast<float>(g.col * m_params.dotSpacingMm),
                         static_cast<float>(g.row * m_params.dotSpacingMm));
    return pts;
}

// =====================================================================
// Synthesis: draw board image
// =====================================================================
/// Rasterizes the board per Params onto a white background: each dot is drawn per its
/// DotRole (plain solid Normal dots; Target/Center dots get a solid outer circle plus a
/// white inner ring; Coord dots are solid at the larger coord size). The resulting image is
/// sized to the full grid plus margin.
cv::Mat FanucIRvisionBoard::generateImage(double pixelsPerMm) const
{
    const double widthMm  = (m_params.cols - 1) * m_params.dotSpacingMm + 2.0 * m_params.marginMm;
    const double heightMm = (m_params.rows - 1) * m_params.dotSpacingMm + 2.0 * m_params.marginMm;

    const int widthPx  = static_cast<int>(std::round(widthMm  * pixelsPerMm));
    const int heightPx = static_cast<int>(std::round(heightMm * pixelsPerMm));

    cv::Mat img(heightPx, widthPx, CV_8UC1, cv::Scalar(255));

    const int rNormalPx = std::max(2,
        static_cast<int>(std::round(m_params.normalDotSizeMm      * pixelsPerMm * 0.5)));
    const int rCoorPx   = std::max(2,
        static_cast<int>(std::round(m_params.coorDotSizeMm        * pixelsPerMm * 0.5)));
    const int rInnerPx  = std::max(1,
        static_cast<int>(std::round(m_params.innerTargetDotSizeMm * pixelsPerMm * 0.5)));

    for (int r = 0; r < m_params.rows; ++r) {
        for (int c = 0; c < m_params.cols; ++c) {
            const double xMm = m_params.marginMm + c * m_params.dotSpacingMm;
            const double yMm = m_params.marginMm + r * m_params.dotSpacingMm;
            const cv::Point2f center(static_cast<float>(xMm * pixelsPerMm),
                                     static_cast<float>(yMm * pixelsPerMm));

            switch (roleOf(r, c)) {
            case DotRole::Normal:
                cv::circle(img, center, rNormalPx, cv::Scalar(0),   cv::FILLED, cv::LINE_AA);
                break;
            case DotRole::Target:
                cv::circle(img, center, rNormalPx, cv::Scalar(0),   cv::FILLED, cv::LINE_AA);
                cv::circle(img, center, rInnerPx,  cv::Scalar(255), cv::FILLED, cv::LINE_AA);
                break;
            case DotRole::Center:
                cv::circle(img, center, rCoorPx,   cv::Scalar(0),   cv::FILLED, cv::LINE_AA);
                cv::circle(img, center, rInnerPx,  cv::Scalar(255), cv::FILLED, cv::LINE_AA);
                break;
            case DotRole::Coord:
                cv::circle(img, center, rCoorPx,   cv::Scalar(0),   cv::FILLED, cv::LINE_AA);
                break;
            }
        }
    }
    return img;
}

// =====================================================================
// Detection helpers
// =====================================================================
namespace {

/// Converts `src` to single-channel grayscale via BGR2GRAY; returns it unchanged
/// if it is already single-channel.
cv::Mat ensureGray(const cv::Mat& src)
{
    if (src.channels() == 1) return src;
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

/// A detected circular blob candidate from contour analysis: its centroid and
/// contour area (in pixels), used before the blob is matched to a grid cell.
struct Blob {
    cv::Point2f center;
    double      area;
};

/// @return the z-component of the 2D cross product a x b (signed-area term used
///         to test three points for colinearity).
double crossZ(const cv::Point2f& a, const cv::Point2f& b)
{
    return static_cast<double>(a.x) * b.y - static_cast<double>(a.y) * b.x;
}

/// Among the 4 large marker blobs (centre + 3 coord dots), identifies which is which
/// purely from geometry: the centre/xMid/xTip triple is the one best fitting a straight
/// line (smallest cross-product residual), leaving yTip as the off-line point; xMid is
/// then picked out of the triple as the mid-distance point, and centre vs xTip are told
/// apart by proximity to yTip. A final check rejects the fit if xMid isn't close to the
/// midpoint of centre/xTip.
/// @param m the 4 marker blob centres, in arbitrary order
/// @param centre output; the identified centre-ring marker position
/// @param xMid output; the identified +X mid coord-dot position
/// @param xTip output; the identified +X tip coord-dot position
/// @param yTip output; the identified +Y coord-dot position
/// @return true if a consistent colinear triple + off-line point was found and passed the midpoint check
bool identifyOrientationMarkers4(const std::vector<cv::Point2f>& m,
                                  cv::Point2f& centre,
                                  cv::Point2f& xMid,
                                  cv::Point2f& xTip,
                                  cv::Point2f& yTip)
{
    if (m.size() != 4) return false;

    double bestResidual = std::numeric_limits<double>::max();
    int yIdx = -1;
    std::array<int, 3> triple{ -1, -1, -1 };

    for (int yi = 0; yi < 4; ++yi) {
        std::array<int, 3> other{};
        int k = 0;
        for (int i = 0; i < 4; ++i) if (i != yi) other[k++] = i;

        const cv::Point2f A  = m[other[0]];
        const cv::Point2f B  = m[other[1]];
        const cv::Point2f C  = m[other[2]];
        const cv::Point2f AB = B - A;
        const cv::Point2f AC = C - A;
        const double denom    = std::max(1e-3, cv::norm(AB));
        const double residual = std::abs(crossZ(AB, AC)) / denom;
        if (residual < bestResidual) {
            bestResidual = residual;
            yIdx         = yi;
            triple       = other;
        }
    }
    if (yIdx < 0) return false;
    yTip = m[yIdx];

    const cv::Point2f A = m[triple[0]];
    const cv::Point2f B = m[triple[1]];
    const cv::Point2f C = m[triple[2]];
    const double dAB = cv::norm(A - B);
    const double dBC = cv::norm(B - C);
    const double dAC = cv::norm(A - C);

    int midIdx = -1, end1Idx = -1, end2Idx = -1;
    if (dAB >= dBC && dAB >= dAC) { midIdx = triple[2]; end1Idx = triple[0]; end2Idx = triple[1]; }
    else if (dAC >= dBC)          { midIdx = triple[1]; end1Idx = triple[0]; end2Idx = triple[2]; }
    else                          { midIdx = triple[0]; end1Idx = triple[1]; end2Idx = triple[2]; }

    xMid = m[midIdx];

    const double d1 = cv::norm(m[end1Idx] - yTip);
    const double d2 = cv::norm(m[end2Idx] - yTip);
    if (d1 < d2) { centre = m[end1Idx]; xTip = m[end2Idx]; }
    else         { centre = m[end2Idx]; xTip = m[end1Idx]; }

    const cv::Point2f mid = (centre + xTip) * 0.5f;
    const double midErr = cv::norm(xMid - mid);
    const double xSpan  = cv::norm(xTip - centre);
    if (xSpan < 1e-3 || midErr / xSpan > 0.25) return false;
    return true;
}

} // namespace

// =====================================================================
// Detection
// =====================================================================
/// Detects the dot grid in `image`: binarizes it (via binarize()), extracts near-circular
/// blobs from contours (circularity + bounding-box squareness filters, plus a median-area
/// outlier cap), takes the 4 largest surviving blobs as the orientation markers and
/// resolves their roles via identifyOrientationMarkers4(), then projects the expected
/// grid from those markers and greedily assigns each remaining blob to its nearest
/// expected cell (within maxAcceptDist).
bool FanucIRvisionBoard::detect(const cv::Mat& image,
                                std::vector<cv::Point2f>& imagePoints,
                                std::vector<cv::Point2f>* cornerImagePoints,
                                cv::Mat* debugOverlay) const
{
    imagePoints.clear();
    if (cornerImagePoints) cornerImagePoints->clear();
    if (image.empty()) return false;

    const cv::Mat gray = ensureGray(image);

    // Binarize with the configured threshold (auto/Otsu when binarizeThreshold
    // < 0, fixed manual value otherwise). Shared with the tuning dialog preview.
    const cv::Mat binary = binarize(gray);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(binary, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

    std::vector<Blob> blobs;
    blobs.reserve(contours.size());
    for (int i = 0; i < static_cast<int>(contours.size()); ++i) {
        if (hierarchy[i][3] != -1) continue;
        const auto& c = contours[i];
        if (c.size() < 5) continue;
        const cv::Moments mm = cv::moments(c);
        if (mm.m00 <= 4.0) continue;
        const double area = mm.m00;
        const double perim = cv::arcLength(c, true);
        if (perim <= 1.0) continue;
        const double circularity = 4.0 * PI_NUM * area / (perim * perim);
        if (circularity < 0.55) continue;

        // Reject non-dot contours by bounding-box squareness. The big surrounding
        // background region (e.g. table outside the board) traces a rectangular
        // outer boundary with image-wide bbox, so its w/h ratio is far from 1
        // even when its circularity sneaks past 0.55.
        const cv::Rect bbox = cv::boundingRect(c);
        if (bbox.width < 3 || bbox.height < 3) continue;
        const int    wMax = std::max(bbox.width, bbox.height);
        const int    wMin = std::min(bbox.width, bbox.height);
        if (static_cast<double>(wMax) / wMin > 1.4) continue;

        const cv::Point2f centroid(static_cast<float>(mm.m10 / mm.m00),
                                   static_cast<float>(mm.m01 / mm.m00));
        blobs.push_back({ centroid, area });
    }

    const int expectedTotal = totalDots();
    if (static_cast<int>(blobs.size()) < expectedTotal) return false;

    std::sort(blobs.begin(), blobs.end(),
              [](const Blob& a, const Blob& b) { return a.area > b.area; });

    // Median-area defence: small dots dominate the lower half of the sorted
    // area list. Any blob >> 12x the median is almost certainly background or
    // a stray label; drop it before picking the 4 orientation markers.
    {
        const size_t mid = blobs.size() / 2 + (blobs.size() / 4);   // ~75th-percentile-from-top -> small dots
        const double medianSmallArea = blobs[std::min(mid, blobs.size() - 1)].area;
        const double areaCap         = medianSmallArea * 12.0;
        blobs.erase(std::remove_if(blobs.begin(), blobs.end(),
                                   [areaCap](const Blob& b) { return b.area > areaCap; }),
                    blobs.end());
    }

    if (static_cast<int>(blobs.size()) < expectedTotal) return false;
    if (blobs[0].area > 3.0 * blobs[3].area) return false;

    const std::vector<cv::Point2f> large4{
        blobs[0].center, blobs[1].center, blobs[2].center, blobs[3].center
    };

    cv::Point2f centrePt, xMidPt, xTipPt, yTipPt;
    if (!identifyOrientationMarkers4(large4, centrePt, xMidPt, xTipPt, yTipPt))
        return false;

    const int rmid = (m_params.rows - 1) / 2;
    const int cmid = (m_params.cols - 1) / 2;

    const cv::Point2f vCol = (xTipPt - centrePt) * 0.5f;
    const cv::Point2f vRow = (centrePt - yTipPt);
    if (cv::norm(vCol) < 1e-3 || cv::norm(vRow) < 1e-3) return false;

    auto projectGridCell = [&](int row, int col) -> cv::Point2f {
        return centrePt
             + static_cast<float>(col - cmid) * vCol
             + static_cast<float>(row - rmid) * vRow;
    };

    std::vector<cv::Point2f> expectedImg;
    expectedImg.reserve(expectedTotal);
    for (int r = 0; r < m_params.rows; ++r)
        for (int c = 0; c < m_params.cols; ++c)
            expectedImg.push_back(projectGridCell(r, c));

    imagePoints.assign(expectedTotal, cv::Point2f(-1.f, -1.f));
    std::vector<float> bestDist(expectedTotal, std::numeric_limits<float>::max());
    const float maxAcceptDist = static_cast<float>(std::min(cv::norm(vCol), cv::norm(vRow)) * 0.45);

    for (const auto& b : blobs) {
        int   bestIdx = -1;
        float bestD   = maxAcceptDist;
        for (int i = 0; i < expectedTotal; ++i) {
            const float dd = static_cast<float>(cv::norm(b.center - expectedImg[i]));
            if (dd < bestD) { bestD = dd; bestIdx = i; }
        }
        if (bestIdx >= 0 && bestD < bestDist[bestIdx]) {
            bestDist[bestIdx]    = bestD;
            imagePoints[bestIdx] = b.center;
        }
    }

    for (const auto& p : imagePoints) {
        if (p.x < 0.f) { imagePoints.clear(); return false; }
    }

    if (cornerImagePoints) {
        const auto cells = cornerCells();
        cornerImagePoints->reserve(4);
        for (const auto& g : cells)
            cornerImagePoints->push_back(imagePoints[g.row * m_params.cols + g.col]);
    }

    if (debugOverlay) {
        cv::cvtColor(gray, *debugOverlay, cv::COLOR_GRAY2BGR);
        for (const auto& p : imagePoints)
            cv::circle(*debugOverlay, p, 3, cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
        cv::circle(*debugOverlay, centrePt, 14, cv::Scalar(0,   0, 255), 2);
        cv::circle(*debugOverlay, xMidPt,   10, cv::Scalar(0, 165, 255), 2);
        cv::circle(*debugOverlay, xTipPt,   10, cv::Scalar(0, 255,   0), 2);
        cv::circle(*debugOverlay, yTipPt,   10, cv::Scalar(255, 0,   0), 2);
        cv::putText(*debugOverlay, "O",  centrePt + cv::Point2f(16, -12),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
        cv::putText(*debugOverlay, "+X", xTipPt   + cv::Point2f(14, -10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
        cv::putText(*debugOverlay, "+Y", yTipPt   + cv::Point2f(14, -10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 0), 2);

        if (cornerImagePoints) {
            const std::array<const char*, 4> lbl{ "TL", "TR", "BR", "BL" };
            for (int i = 0; i < 4; ++i) {
                cv::circle(*debugOverlay, (*cornerImagePoints)[i], 16,
                           cv::Scalar(255, 255, 0), 2);
                cv::putText(*debugOverlay, lbl[i],
                            (*cornerImagePoints)[i] + cv::Point2f(18, 0),
                            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);
            }
        }
    }

    return true;
}

} // namespace calib
