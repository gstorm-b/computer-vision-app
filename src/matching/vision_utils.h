#ifndef VISION_UTILS_H
#define VISION_UTILS_H

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
// #include <opencv2/calib3d.hpp>
// #include <opencv2/objdetect/aruco_detector.hpp>
#include <math.h>

#include <vector>
#include <fstream>
#include <iostream>
#include <string>

/// Default BGR draw colors (OpenCV channel order) for axes/box overlays.
#define DEFAULT_COLOR_RED          cv::Scalar(0, 0, 255)
#define DEFAULT_COLOR_GREEN        cv::Scalar(0, 255, 0)
#define DEFAULT_COLOR_BLUE         cv::Scalar(255, 0, 0)
#define DEFAULT_COLOR_YELLOW       cv::Scalar(0, 255, 255)

/// Degrees-to-radians conversion factor (multiply a degree value by this to get radians).
#define D2R (CV_PI / 180.0)
/// Radians-to-degrees conversion factor (multiply a radian value by this to get degrees).
#define R2D (180.0 / CV_PI)

/// Static utility bag ("Vision Support Utilities"): OpenCV-based helpers for wide-path image
/// I/O, contour simplification, PCA orientation, pixel-histogram statistics, and debug drawing
/// used across the matching pipeline. All members are static; the class is never instantiated.
class vsu {
public:

// class ArrowAxes {
// public:
//     ArrowAxes() {}
//     ~ArrowAxes() {}

//     void drawAxes(cv::Mat& img);
//     void drawAxes(cv::Mat& img, cv::Point position, double angle = 0, int length = 20);

//     cv::Scalar cl_x = DEFAULT_COLOR_RED;
//     cv::Scalar cl_y = DEFAULT_COLOR_BLUE;
//     cv::Scalar cl_center = DEFAULT_COLOR_GREEN;
//     int length = 20;
//     double y_axis_ratio = 0.8;
//     double hooksize = 0.4;
//     double angle = 0;
//     cv::Point center;
// };

/// Encodes `img` using the codec inferred from `filename`'s extension and writes the result to
/// a file opened via a wide-character path (works around cv::imwrite's lack of Unicode path
/// support on Windows).
/// @param filename destination path (wide string so non-ASCII paths are supported)
/// @param img image to encode
/// @param params optional cv::imencode parameters (e.g. JPEG quality)
/// @return true on success; false if encoding or opening the file for writing fails
static bool imwrite_wstring(const std::wstring& filename,
                            const cv::Mat& img,
                            const std::vector<int>& params = std::vector<int>()) {
    // Encode the image into a buffer
    std::vector<uchar> buffer;
    // Extract file extension to determine format (e.g., ".png", ".jpg")
    std::wstring extension = filename.substr(filename.find_last_of(L"."));
    std::string narrowExtension(extension.begin(), extension.end());

    if (!cv::imencode(narrowExtension, img, buffer, params)) {
        std::wcerr << L"Error: Could not encode image to buffer" << std::endl;
        return false;
    }

    // Write the buffer to a file with the wide string name in binary mode
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::wcerr << L"Error: Could not open file for writing " << filename << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    file.close();

    return true;
}

/// Reads and decodes an image from a wide-character path (works around cv::imread's lack of
/// Unicode path support on Windows): loads the raw file bytes then decodes via cv::imdecode.
/// @param filename source path (wide string so non-ASCII paths are supported)
/// @param flags cv::imdecode flags controlling color conversion (default: color)
/// @return the decoded image, or an empty cv::Mat if the file could not be opened
static cv::Mat imread_wstring(const std::wstring& filename,
                              int flags = cv::IMREAD_COLOR) {
    // Open the file with the wide string name in binary mode
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::wcerr << L"Error: Could not open file " << filename << std::endl;
        return cv::Mat();
    }

    // Read the file data into a buffer
    std::vector<uchar> buffer(std::istreambuf_iterator<char>(file), {});
    file.close();

    // Decode the buffer using OpenCV
    return cv::imdecode(buffer, flags);
}


/// Simplifies a contour by greedily replacing runs of up to `T` consecutive points with a
/// single segment whenever every skipped point lies within `tolerance` of that segment
/// (perpendicular distance), scanning from the farthest candidate neighbor back to the nearest.
/// @param contour ordered contour points to simplify
/// @param T maximum look-ahead neighbor count per compression step
/// @param tolerance max perpendicular distance (pixels) an intermediate point may deviate
/// @return the simplified point sequence; returns `contour` unchanged if it has fewer than 2
///         points or T < 2
static std::vector<cv::Point> tNeighborSimplify(const std::vector<cv::Point>& contour,
                                                int T = 5, double tolerance = 2.0) {
    const size_t N = contour.size();
    if (N < 2 || T < 2) return contour;               // Nothing to do

    std::vector<cv::Point> simplified;
    simplified.reserve(N);
    simplified.push_back(contour.front());            // Always keep the first point

    size_t i = 0;
    while (i < N - 1) {
        bool compressed = false;
        // j goes from far neighbor (i+T) back to the immediate next (i+1)
        const size_t j_max = std::min(i + static_cast<size_t>(T), N - 1);
        for (size_t j = j_max; j > i + 0; --j) {
            const cv::Point& p1 = simplified.back();  // start point
            const cv::Point& p2 = contour[j];         // candidate end point
            const cv::Point2f seg = p2 - p1;
            const double seg_len = cv::norm(seg);
            if (seg_len == 0.0) continue;             // degenerate; skip

            // Compute max distance of intermediate points k ∈ (i, j)
            double dmax = 0.0;
            for (size_t k = i + 1; k < j; ++k) {
                const cv::Point& pk = contour[k];
                // perpendicular distance from pk to line p1‑p2
                double cross = std::abs(seg.x * (pk.y - p1.y) - seg.y * (pk.x - p1.x));
                double dist = cross / seg_len;
                if (dist > dmax) dmax = dist;
                if (dmax > tolerance) break;          // early exit
            }

            if (dmax <= tolerance) {
                // All intermediate points are close enough -> compress them
                simplified.push_back(p2);
                i = j;                                // jump forward
                compressed = true;
                break;
            }
        }

        if (!compressed) {
            // Could not compress with any neighbor → keep next point
            simplified.push_back(contour[i + 1]);
            ++i;
        }
    }

    return simplified;
}

/// Draws an X/Y orientation gizmo (two arrowed axis lines plus a center dot) onto `img` at
/// `position`, rotated by `angle`; the Y axis is drawn at a fixed 0.8x length ratio vs X.
/// @param img image to draw onto (modified in place)
/// @param position gizmo origin, in image pixel coordinates
/// @param angle rotation of the X axis; interpreted as radians or degrees per `radian`
/// @param radian true if `angle` is already in radians; false to convert from degrees
/// @param length X-axis arrow length in pixels (Y axis uses length * 0.8)
/// @param hookSize arrowhead size fraction passed through to cv::arrowedLine
static void drawAxes2Img(cv::Mat& img, cv::Point position, double angle, bool radian, int length, double hookSize = 0.4,
                         cv::Scalar xColour = DEFAULT_COLOR_BLUE, cv::Scalar yColour = DEFAULT_COLOR_RED,
                         cv::Scalar centerColour = DEFAULT_COLOR_YELLOW)  {
    const double yAxisRatio = 0.8;
    if (!radian) {
        angle = angle * D2R;
    }
    // create the axis
    cv::Point xPoint((int)(position.x + cos(angle)*length), (int)(position.y + sin(angle)*length));
    cv::Point yPoint((int)(position.x - sin(angle)*length*yAxisRatio), (int)(position.y + cos(angle)*length*yAxisRatio));
    arrowedLine(img, position, xPoint, xColour, 1, cv::LINE_AA, 0, hookSize);
    arrowedLine(img, position, yPoint, yColour, 1, cv::LINE_AA, 0, hookSize);
    circle(img, position, 2, centerColour, 2);
}

/// Runs PCA over `pts` and derives a principal-axis orientation and centroid, as used to
/// estimate an object's pick angle from its contour points.
/// @param pts contour/blob points to analyze
/// @param angleOutput set to the angle (radians) of the first eigenvector via atan2
/// @param centerOutput set to the PCA mean (centroid) of `pts`
static void computePcaOrientation(const std::vector<cv::Point>& pts, double& angleOutput, cv::Point2f& centerOutput) {
    int sz = static_cast<int>(pts.size());
    cv::Mat data_pts = cv::Mat(sz, 2, CV_64F);
    for (int i = 0; i < data_pts.rows; i++)
    {
        data_pts.at<double>(i, 0) = pts[i].x;
        data_pts.at<double>(i, 1) = pts[i].y;
    }
    // Perform PCA analysis
    cv::PCA pca_analysis(data_pts, cv::Mat(), cv::PCA::DATA_AS_ROW);
    // Store the center of the object
    cv::Point cntr = cv::Point(static_cast<int>(pca_analysis.mean.at<double>(0, 0)),
                       static_cast<int>(pca_analysis.mean.at<double>(0, 1)));
    // Store the eigenvalues and eigenvectors
    std::vector<cv::Point2d> eigen_vecs(2);
    std::vector<double> eigen_val(2);
    for (int i = 0; i < 2; i++)
    {
        eigen_vecs[i] = cv::Point2d(pca_analysis.eigenvectors.at<double>(i, 0),
                                pca_analysis.eigenvectors.at<double>(i, 1));
        eigen_val[i] = pca_analysis.eigenvalues.at<double>(i);
    }

    double angle = atan2(eigen_vecs[0].y, eigen_vecs[0].x); // orientation in radians
    angleOutput = angle;
    centerOutput = cntr;
}


/// Computes the median pixel intensity of a single-channel 8-bit image via a 256-bin histogram
/// running sum.
/// @param Input single-channel image to analyze (passed by value; not modified)
/// @return the median intensity bin, or -1.0 if the histogram never reaches half the pixel count
static double median(cv::Mat Input) {
    double m = (Input.rows*Input.cols) / 2;
    int bin = 0;
    double med = -1.0;

    int histSize = 256;
    float range[] = { 0, 256 };
    const float* histRange = { range };
    bool uniform = true;
    bool accumulate = false;
    cv::Mat hist;
    cv::calcHist( &Input, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange, uniform, accumulate );

    for ( int i = 0; i < histSize && med < 0.0; ++i )
    {
        bin += cvRound( hist.at< float >( i ) );
        if ( bin > m && med < 0.0 )
            med = i;
    }

    return med;
}

/// Crops `sourceImage` to `boxBounding` expanded by `border` pixels on every side, clamped to
/// the source image bounds.
/// @param sourceImage image to crop (passed by value)
/// @param boxBounding region to crop around, before border expansion
/// @param border pixels added on each side of `boxBounding` before clamping
/// @return the cropped sub-image (a view into `sourceImage`'s data)
static cv::Mat cropImageWithBorderOffset(cv::Mat sourceImage, cv::Rect boxBounding,int border) {
    cv::Mat outputMat;
    int minOffset_X, maxOffset_X;
    int minOffset_Y, maxOffset_Y;

    cv::Point topLeft = boxBounding.tl();
    cv::Point botRight = boxBounding.br();

    minOffset_X = topLeft.x - border;
    maxOffset_X = botRight.x + border;

    minOffset_Y = topLeft.y - border;
    maxOffset_Y = botRight.y + border;

    if (minOffset_X < 0) {
        minOffset_X = 0;
    }

    if (maxOffset_X > sourceImage.cols) {
        maxOffset_X = sourceImage.cols;
    }

    if (minOffset_Y < 0) {
        minOffset_Y = 0;
    }

    if (maxOffset_Y > sourceImage.rows) {
        maxOffset_Y = sourceImage.rows;
    }

    outputMat = sourceImage(cv::Range(minOffset_Y, maxOffset_Y), cv::Range(minOffset_X, maxOffset_X));

    return outputMat;
}

/// Binarizes a source image the same way ImageMatcher does before contour/area pre-filtering:
/// grayscale -> 5x5 Gaussian blur -> threshold (THRESH_BINARY).
/// @param src source image (BGR, BGRA, or already single-channel)
/// @param threshold < 0 selects automatic Otsu thresholding (current engine behaviour);
///        0..255 uses that fixed manual value
/// @param maxValue cv::threshold maxval (intensity assigned above threshold); applies in both
///        auto and manual modes
/// @param usedThreshold optional out-param receiving the value actually applied (the
///        Otsu-computed value in auto mode). Kept here so the patterns-widget binary preview
///        matches what the matcher would compute.
/// @return the binary image, or an empty cv::Mat if `src` is empty
static cv::Mat binarizeSourceImage(const cv::Mat& src, int threshold = -1,
                                   int maxValue = 255, double* usedThreshold = nullptr) {
    if (src.empty()) return cv::Mat();

    cv::Mat gray;
    if (src.channels() == 3)
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else if (src.channels() == 4)
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    else
        gray = src.clone();

    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);

    cv::Mat binary;
    double applied = 0.0;
    if (threshold < 0) {
        applied = cv::threshold(gray, binary, 0, maxValue,
                                cv::THRESH_BINARY | cv::THRESH_OTSU);
    } else {
        applied = threshold;
        cv::threshold(gray, binary, threshold, maxValue, cv::THRESH_BINARY);
    }
    if (usedThreshold) *usedThreshold = applied;
    return binary;
}

/// Draws the 4 edges of a rotated rectangle (e.g. a picking box) onto `matSrc` as thin
/// anti-aliased lines.
/// @param matSrc image to draw onto (modified in place)
/// @param rectRot rotated rectangle whose 4 corners are connected
/// @param color line color
static void drawPickingBox(cv::Mat& matSrc, cv::RotatedRect rectRot, cv::Scalar color) {
    cv::Point2f vertices2f[4];
    rectRot.points(vertices2f);

    cv::Point vertices[4];
    for(int i = 0; i < 4; ++i){
        vertices[i] = vertices2f[i];
    }

    cv::line(matSrc, vertices[0], vertices[1], color, 1, cv::LineTypes::LINE_AA);
    cv::line(matSrc, vertices[1], vertices[2], color, 1, cv::LineTypes::LINE_AA);
    cv::line(matSrc, vertices[2], vertices[3], color, 1, cv::LineTypes::LINE_AA);
    cv::line(matSrc, vertices[3], vertices[0], color, 1, cv::LineTypes::LINE_AA);
}

};

#endif // VISION_UTILS_H
