#include "image_matcher.h"

#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include <iostream>

#include <chrono>
#include <immintrin.h>   // AVX2
#include <omp.h>
#include <cmath>
#include <malloc.h>
#include <algorithm>
#include <vector>

#include "vision_utils.h"

#define VISION_TOLERANCE    0.0000001
#define D2R                 (CV_PI / 180.0)
#define R2D                 (180.0 / CV_PI)
#define MATCH_CANDIDATE_NUM 5
#define SMALL_CROP_OFFSET   20

using namespace std;
using namespace cv;

/// Vision/matching module: template-matching engine (ImageMatcher), pattern
/// group/config model, and the geometry/SIMD helpers it depends on.
namespace mtc {

// ── SIMD helpers ─────────────────────────────────────────────────────────────

/// Horizontally sums the four 32-bit lanes of an SSE2 vector into a scalar.
/// @param V vector of four int32 values to reduce
/// @return sum of all four lanes
inline int _mm_hsum_epi32(__m128i V) {
    __m128i T = _mm_add_epi32(V, _mm_srli_si128(V, 8));
    T = _mm_add_epi32(T, _mm_srli_si128(T, 4));
    return _mm_cvtsi128_si32(T);
}

/// SSE2 dot-product of two equal-length byte buffers (kernel vs. convolution
/// window): unpacks each 16-byte block to 16-bit lanes, multiply-adds in
/// 128-bit chunks, then finishes any remainder (< 16 bytes) with a scalar loop.
/// @param pCharKernel first byte buffer (e.g. template pixels)
/// @param pCharConv second byte buffer (e.g. source window pixels)
/// @param iLength number of bytes to accumulate over
/// @return sum of elementwise products of the two buffers
inline int IM_Conv_SIMD(unsigned char* pCharKernel, unsigned char* pCharConv, int iLength) {
    const int iBlockSize = 16, Block = iLength / iBlockSize;
    __m128i SumV = _mm_setzero_si128();
    __m128i Zero = _mm_setzero_si128();
    for (int Y = 0; Y < Block * iBlockSize; Y += iBlockSize) {
        __m128i SrcK = _mm_loadu_si128((__m128i*)(pCharKernel + Y));
        __m128i SrcC = _mm_loadu_si128((__m128i*)(pCharConv  + Y));
        __m128i SrcK_L = _mm_unpacklo_epi8(SrcK, Zero);
        __m128i SrcK_H = _mm_unpackhi_epi8(SrcK, Zero);
        __m128i SrcC_L = _mm_unpacklo_epi8(SrcC, Zero);
        __m128i SrcC_H = _mm_unpackhi_epi8(SrcC, Zero);
        __m128i SumT = _mm_add_epi32(_mm_madd_epi16(SrcK_L, SrcC_L),
                                      _mm_madd_epi16(SrcK_H, SrcC_H));
        SumV = _mm_add_epi32(SumV, SumT);
    }
    int Sum = _mm_hsum_epi32(SumV);
    for (int Y = Block * iBlockSize; Y < iLength; Y++)
        Sum += pCharKernel[Y] * pCharConv[Y];
    return Sum;
}

// ── Comparators ──────────────────────────────────────────────────────────────

/// Sort predicate ordering MatchParams by descending match score (best first).
static bool compareScoreBig2Small(const MatchParams& lhs, const MatchParams& rhs) {
    return lhs._matchScore > rhs._matchScore;
}

/// Sort predicate ordering (point, angle) pairs by ascending angle; used to
/// walk a rotated-rect intersection polygon's vertices in angular order
/// around the shape's center (see SortPtWithCenter).
static bool comparePtWithAngle(const pair<Point2f, double> lhs,
                                const pair<Point2f, double> rhs) {
    return lhs.second < rhs.second;
}

// ── ImageMatcher ──────────────────────────────────────────────────────────────

/// Default-constructs the matcher with an empty source image and pattern group.
ImageMatcher::ImageMatcher() {}

/// Returns the pattern group holding this matcher's learned patterns/config.
MatchGroup* ImageMatcher::getPatternGroup() { return &m_model_src; }

/// @return number of learned patterns currently stored in the pattern group.
int ImageMatcher::getTemplateSourceSize() {
    return static_cast<int>(m_model_src.patternCount());
}

/// Loads the matching source image from disk via cv::imread, replacing any
/// previously set source (silently becomes empty if the path fails to load).
void ImageMatcher::setImageSource(std::string path) {
    m_img_source = cv::imread(path);
}

/// Sets the matching source image, cloning `img` so the matcher owns an
/// independent copy of the pixel data.
void ImageMatcher::setImageSource(cv::Mat img) {
    m_img_source = img.clone();
}

/// Sets the region of interest that matching() restricts contour search to
/// when `usingRoi` is true.
/// @param tl top-left corner of the ROI, in source-image pixels
/// @param br bottom-right corner of the ROI, in source-image pixels
void ImageMatcher::setMatchingROI(cv::Point tl, cv::Point br) {
    ROI_tl = tl;
    ROI_br = br;
}

/// Sets the condition ROI used to flag matched objects that fall (partly or
/// fully) outside it as not possible-to-pick.
/// @param tl top-left corner of the condition ROI, in source-image pixels
/// @param br bottom-right corner of the condition ROI, in source-image pixels
void ImageMatcher::setMatchingConditionROI(cv::Point tl, cv::Point br) {
    Condition_ROI_tl = tl;
    Condition_ROI_br = br;
}

// ── Border clear helper ───────────────────────────────────────────────────────

/// Paints a solid white frame of `offset` pixels around the image's four
/// edges in place, so contours touching the border (partial objects) are cut
/// off before findContours runs.
/// @param image single-channel image to modify in place
/// @param offset border thickness in pixels on each side
static inline void clearImageBorder(Mat& image, int offset) {
    const int cols = image.cols, rows = image.rows;
    for (int y = 0; y < offset; ++y)
        for (int x = 0; x < cols; ++x) {
            image.at<uchar>(y, x)             = 255;
            image.at<uchar>(rows - 1 - y, x)  = 255;
        }
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < offset; ++x) {
            image.at<uchar>(y, x)             = 255;
            image.at<uchar>(y, cols - 1 - x)  = 255;
        }
}

// ── Public matching entry points ──────────────────────────────────────────────

/// Debug utility that renders a contour list on a blank white canvas (red,
/// 2px) and writes it to disk; used ad hoc from matching() when the commented
/// debug call is enabled.
/// @param srcContours contours to draw, in canvas pixel coordinates
/// @param outPath file path the rendered image is written to via cv::imwrite
/// @param canvasSize size of the blank canvas the contours are drawn onto
void saveContourImage(const vector<vector<Point>>& srcContours,
                      const string& outPath,
                      cv::Size canvasSize) {

    Mat contourImg = Mat::zeros(canvasSize, CV_8UC3);
    contourImg.setTo(Scalar(255, 255, 255));
    drawContours(contourImg, srcContours, -1, Scalar(0, 0, 255), 2);
    imwrite(outPath, contourImg);
}

/// Convenience overload: sets `image` as the source (cloned) then runs the
/// full matching() pipeline below.
/// @param image new source image, cloned before matching
void ImageMatcher::matching(Mat& image, bool boundingBoxChecking,
                             int objectsNum, bool usingRoi, bool usingConditionRoi) {
    m_img_source = image.clone();
    matching(boundingBoxChecking, objectsNum, usingRoi, usingConditionRoi);
}

/// Runs the full matching pipeline against the current source image and
/// populates match_result: grayscale + threshold pre-processing, contour
/// extraction (optionally cropped to the matching ROI), per-contour area
/// filtering against each pattern's expected contour area, edge-based
/// matching (MatchEdge) for each candidate object/pattern pair, collision and
/// condition-ROI checks, robot-pickability checks, result sorting, and
/// annotated result-image rendering. No-ops (leaves match_result untouched)
/// if the source image is empty; clears match_result.Objects and returns
/// early if the pattern group has no learned patterns.
void ImageMatcher::matching(bool boundingBoxChecking, int objectsNum, bool usingRoi, bool usingConditionRoi) {
    if (m_img_source.empty()) return;

    match_result.isFoundMatchObject   = false;
    match_result.totalPossiblePicking = 0;
    match_result.cropOffsetPoint = usingRoi ? ROI_tl : cv::Point2f(0.0, 0.0);

    if (m_model_src.isEmpty()) {
        match_result.Objects.clear();
        match_result.ExecutionTime        = 0;
        match_result.isAreaLessThanLimits = false;
        m_img_source.copyTo(match_result.Image);
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    // find max model contour area for material-quantity check
    int maxModelSize     = 0;
    int maxModelRectArea = 0;
    std::vector<std::shared_ptr<MatchPattern>> patterns = m_model_src.patterns();
    for (auto& sp : patterns) {
        MatchPattern* model = sp.get();
        if (maxModelSize     < model->getModelContoursSelectedArea())
            maxModelSize     = model->getModelContoursSelectedArea();
        if (maxModelRectArea < model->getModelContoursSelectedRectArea())
            maxModelRectArea = model->getModelContoursSelectedRectArea();
    }

    /// Pre-processing
    Mat imageGray, imageThresh, temp_src_image;
    if (m_img_source.channels() == 3)
        cv::cvtColor(m_img_source, imageGray, cv::COLOR_BGR2GRAY);
    else if (m_img_source.channels() == 4)
        cv::cvtColor(m_img_source, imageGray, cv::COLOR_BGRA2GRAY);
    else
        imageGray = m_img_source.clone();

    // const MatchGroupConfig &groupConfig = m_model_src.config();
    const EdgeMatchConfig* groupEcfg = m_model_src.config().edgeConfig();

    GaussianBlur(imageGray, imageGray, cv::Size(groupEcfg->blurWidth, groupEcfg->blurHeight), 0);
    if(groupEcfg->binaryThreshold >= 0) {
        threshold(imageGray, imageThresh, groupEcfg->binaryThreshold, groupEcfg->binaryMaxValue,
                  groupEcfg->invertBinaryThreshold ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY_INV);
    } else {
        threshold(imageGray, imageThresh, 0, 255,
                  groupEcfg->invertBinaryThreshold ? cv::THRESH_BINARY_INV + cv::THRESH_OTSU
                                                   : cv::THRESH_BINARY + cv::THRESH_OTSU);
    }

    vector<vector<Point>> srcContours;
    vector<Vec4i>         srcHierarchy;
    const int border_offset = 2;
    cv::Size img_size;
    cv::Point2f cropConditionRoiTl;
    cv::Point2f cropConditionRoiBr;

    if (usingRoi) {
        temp_src_image = m_img_source(cv::Range(ROI_tl.y, ROI_br.y),
                                      cv::Range(ROI_tl.x, ROI_br.x)).clone();
        cv::Mat roi_mat = imageThresh(cv::Range(ROI_tl.y, ROI_br.y),
                                      cv::Range(ROI_tl.x, ROI_br.x)).clone();
        clearImageBorder(roi_mat, border_offset);
        // cv::imwrite("Test.bmp", roi_mat);
        findContours(roi_mat, srcContours, srcHierarchy,
                     cv::RETR_TREE, cv::CHAIN_APPROX_NONE);
        cropConditionRoiTl.x = Condition_ROI_tl.x - ROI_tl.x;
        cropConditionRoiTl.y = Condition_ROI_tl.y - ROI_tl.y;
        cropConditionRoiBr.x = Condition_ROI_br.x - ROI_tl.x;
        cropConditionRoiBr.y = Condition_ROI_br.y - ROI_tl.y;
        img_size = cv::Size(roi_mat.cols, roi_mat.rows);
    } else {
        m_img_source.copyTo(temp_src_image);
        clearImageBorder(imageThresh, border_offset);
        findContours(imageThresh, srcContours, srcHierarchy,
                     cv::RETR_TREE, cv::CHAIN_APPROX_NONE);
        img_size = cv::Size(imageThresh.cols, imageThresh.rows);
    }

    // debug
    //saveContourImage(srcContours, "contours.png", img_size);

    int sumArea       = 0;
    int temp_img_cols = static_cast<int>(temp_src_image.cols * 0.8);
    int temp_img_rows = static_cast<int>(temp_src_image.rows * 0.8);
    cv::Rect imageRect(0, 0, temp_src_image.cols, temp_src_image.rows);
    const int maxNoiseArea = static_cast<int>(
        temp_src_image.cols * temp_src_image.rows * 0.5);

    vector<PossibleObject> objects;
    vector<int>            possibleCollisionContourIndex;

    for (int ci = 0; ci < static_cast<int>(srcContours.size()); ++ci) {
        PossibleObject tempObj;
        int area         = static_cast<int>(contourArea(srcContours[ci]));
        cv::RotatedRect rect    = cv::minAreaRect(srcContours[ci]);
        cv::Rect        bndBox  = cv::boundingRect(srcContours[ci]);
        int rectArea = static_cast<int>(rect.size.height * rect.size.width);

        possibleCollisionContourIndex.push_back(ci);

        if (rectArea >= maxNoiseArea) continue;

        bool isNoise = (rect.size.height >= temp_img_rows)
                    || (rect.size.width  >= temp_img_cols)
                    || (bndBox.height    >= temp_img_rows)
                    || (bndBox.width     >= temp_img_cols);

        if (!isNoise) sumArea += area;

        for (int mi = 0; mi < static_cast<int>(patterns.size()); ++mi) {
            MatchPattern* model = patterns.at(mi).get();
            int lowerArea = static_cast<int>(model->getModelContoursSelectedArea() * lowerThreshRatio);
            int upperArea = static_cast<int>(model->getModelContoursSelectedArea() * upperThreshRatio);
            if (area <= lowerArea || area >= upperArea) continue;
            tempObj.modelCheckList.push_back(mi);
        }

        if (!tempObj.modelCheckList.empty()) {
            tempObj.conIndex = ci;
            cv::Rect tmp = cv::boundingRect(srcContours[ci]);
            tempObj.conBoundingRect = cv::Rect(
                tmp.x - SMALL_CROP_OFFSET,
                tmp.y - SMALL_CROP_OFFSET,
                tmp.width  + 2 * SMALL_CROP_OFFSET,
                tmp.height + 2 * SMALL_CROP_OFFSET) & imageRect;
            tempObj.conMinRectArea = cv::minAreaRect(srcContours[ci]);
            objects.push_back(tempObj);
        }
    }

    match_result.Objects.clear();

    // Filled-contour footprint mask, built once for the whole frame (it is
    // identical for every matched object) and shared by every
    // checkCollisionObject2 call. It lives in the same pixel frame as the
    // translated gripper points. Contours are filled solid; nested holes are
    // filled too, which keeps the collision test conservative.
    cv::Mat collisionContourMask = cv::Mat::zeros(img_size, CV_8UC1);
    cv::drawContours(collisionContourMask, srcContours, -1, cv::Scalar(255), cv::FILLED);

    for (int oi = 0; oi < static_cast<int>(objects.size()); ++oi) {
        cv::Mat temp_crop = temp_src_image(objects[oi].conBoundingRect);
        m_final_overlap_result.clear();
        std::vector<MatchedObject> area_obj;

        for (int mi = 0; mi < static_cast<int>(objects[oi].modelCheckList.size()); ++mi) {
            std::vector<MatchedObject> tmp_obj;
            int modelIdx = objects[oi].modelCheckList[mi];
            MatchPattern* pattern = patterns.at(modelIdx).get();
            this->MatchEdge(temp_crop, pattern, tmp_obj);
            for (auto& o : tmp_obj)
                area_obj.push_back(o);
        }

        std::vector<int> del_indexes;
        FilterWithRotatedRect(&m_final_overlap_result, cv::TM_CCOEFF_NORMED,
                              max_overlap, &del_indexes);
        std::sort(del_indexes.begin(), del_indexes.end());
        for (auto it = del_indexes.rbegin(); it != del_indexes.rend(); ++it) {
            if (*it >= 0 && *it < static_cast<int>(area_obj.size()))
                area_obj.erase(area_obj.begin() + *it);
        }

        for (auto& obj : area_obj) {
            if (obj.parent() == nullptr) continue;
            obj.translateByTopLeft(objects[oi].conBoundingRect.tl());

            // Picking / collision-box geometry is per-pattern — read it from the
            // matched object's parent pattern config, not the shared group config.
            const MatchPatternConfig* pcfg = obj.parent()->patternConfigPtr();
            obj.pickingBox = cv::RotatedRect(obj.point_Center,
                                              pcfg->m_pickingBoxSize,
                                              obj.point_angle);
            obj.pattern_name  = pcfg->m_patternName;
            obj.pattern_index = pcfg->m_patternIndex;
            obj.computeGripperBox(pcfg->m_pickingBoxSize,
                                  pcfg->m_pickingBoxDistance,
                                  pcfg->m_pickingBoxAngle);
            obj.checkCollisionObject2(collisionContourMask);
            if (usingConditionRoi) {
                obj.outSideConditionRoiCheck(cropConditionRoiTl, cropConditionRoiBr);
            }
            obj.setPossibleToPick(robotPossiblePickingCheck(obj));
            match_result.Objects.push_back(obj);
            if ((!obj.hasCollision()) && (!obj.isOutsideConditionRoi()) && ((obj.isPossibleToPick())))
                match_result.totalPossiblePicking++;
        }

        if (objectsNum < 0) continue;
        if (static_cast<int>(match_result.Objects.size()) > objectsNum
            && match_result.totalPossiblePicking > 0)
            break;
    }

    std::chrono::time_point stop_pt = std::chrono::high_resolution_clock::now();
    double time_count = std::chrono::duration<double, std::milli>(stop_pt - start).count();

    match_result.ExecutionTime   = time_count;
    match_result.imageCols       = temp_src_image.cols;
    match_result.imageRows       = temp_src_image.rows;

    // Order objects so the best pick candidates come first (priority, then score
    // or angle). The downstream consumer reports + sends them in this order.
    sortMatchedObjects(match_result.Objects);

    match_result.isFoundMatchObject = !match_result.Objects.empty();
    const double lowWorkpieceRatio = groupEcfg ? groupEcfg->lowWorkpieceRatio : 1.5;
    match_result.isAreaLessThanLimits =
        (sumArea < static_cast<int>(maxModelSize * lowWorkpieceRatio));

    if (temp_src_image.channels() == 1)
        cv::cvtColor(temp_src_image, match_result.Image, cv::COLOR_GRAY2BGR);
    else if (m_img_source.channels() == 4)
        cv::cvtColor(temp_src_image, match_result.Image, cv::COLOR_BGRA2BGR);
    else
        match_result.Image = temp_src_image.clone();

    for (auto& obj : match_result.Objects) {
        const MatchPattern* parent = obj.parent();
        if (parent) obj.drawMaskPatternIntoImage(match_result.Image);

        vsu::drawAxes2Img(match_result.Image, obj.point_Center, obj.point_angle,
                          false, 20, 0.4, DEFAULT_COLOR_BLUE, DEFAULT_COLOR_RED);

        if (obj.hasCollision()) {
            obj.drawGripperBoxToImage(match_result.Image, DEFAULT_COLOR_RED);
        } else if (!obj.isPossibleToPick() || obj.isOutsideConditionRoi()){
            obj.drawGripperBoxToImage(match_result.Image, DEFAULT_COLOR_YELLOW);
        } else {
            obj.drawGripperBoxToImage(match_result.Image, DEFAULT_COLOR_GREEN);
        }
        obj.setParent(nullptr);
    }
}

/// Anonymous-namespace helpers for sortMatchedObjects() below.
namespace {

/// Pick-priority bucket for sorting: lower value is picked first.
///   0  no collision and inside the condition ROI
///   1  collision but inside the condition ROI
///   2  the rest (outside the condition ROI, regardless of collision)
/// @return priority bucket (0, 1, or 2) for `obj`
int pickPriority(const MatchedObject& obj) {
    if (obj.isOutsideConditionRoi()) return 2;
    if (obj.hasCollision())          return 1;
    return 0;
}

/// Absolute circular angular error (degrees) between an object angle and the
/// target condition angle, wrapped into [-180, 180] so 179 vs -179 is 2, not 358.
/// @param angle measured angle, in degrees
/// @param target reference angle to compare against, in degrees
/// @return non-negative angular difference in degrees, at most 180
double angleErrorDeg(double angle, double target) {
    double diff = angle - target;
    while (diff > 180.0)  diff -= 360.0;
    while (diff < -180.0) diff += 360.0;
    return std::fabs(diff);
}

} // namespace

/// Stable-sorts `objects` in place, best pick candidates first: primarily by
/// pickPriority() (no-collision-in-ROI, then collision-in-ROI, then outside
/// ROI), and within each priority bucket by either angular error to the
/// group's configured target angle (if m_sortByAngle is set) or by descending
/// match score.
/// @param objects matched objects to reorder in place
void ImageMatcher::sortMatchedObjects(std::vector<MatchedObject>& objects) {
    const MatchGroupConfig& cfg = m_model_src.config();
    const bool   sortByAngle = cfg.m_sortByAngle;
    const double targetAngle = cfg.m_sortConditionAngle;

    std::stable_sort(objects.begin(), objects.end(),
        [sortByAngle, targetAngle](const MatchedObject& a, const MatchedObject& b) {
            // 1) Pick priority — better pick group first.
            const int pa = pickPriority(a);
            const int pb = pickPriority(b);
            if (pa != pb) return pa < pb;

            // 2) Within a group: angle error (if enabled) or score.
            if (sortByAngle) {
                return angleErrorDeg(a.point_angle, targetAngle)
                     < angleErrorDeg(b.point_angle, targetAngle);   // smallest error first
            }
            return a.matched_Score > b.matched_Score;                // highest score first
        });
}

/// Sets the matching source image, cloning `img` so the matcher owns an
/// independent copy of the pixel data.
void ImageMatcher::setMatchSourceImage(cv::Mat& img) {
    m_img_source = img.clone();
}

// ── MatchEdge ─────────────────────────────────────────────────────────────────

/// Runs edge-based template matching of `edgePattern` against `img_edge`:
/// builds an image pyramid, searches the coarsest layer over the pattern's
/// tolerance angle range (SIMD or scalar edge scoring depending on
/// NOT_USE_SIMD), descends the pyramid refining position/angle per layer
/// (with optional sub-pixel estimation at layer 0), filters by score and
/// mutual overlap, then builds one MatchedObject per surviving candidate
/// (corners, center, score, angle) and appends the raw MatchParams to
/// m_final_overlap_result for later overlap filtering across patterns.
bool ImageMatcher::MatchEdge(cv::Mat& img_edge, MatchPattern* edgePattern,
                              std::vector<MatchedObject>& match_objs) {
    if (!edgePattern || !edgePattern->isPatternLearned()) return false;

    cv::Mat img_dst = edgePattern->getPatternImage();
    if (img_edge.empty() || img_dst.empty()) return false;
    if (img_dst.cols > img_edge.cols && img_dst.rows > img_edge.rows) return false;
    if (img_dst.size().area() > img_edge.size().area()) return false;

    const MatchPatternConfig* cfg = edgePattern->patternConfigPtr();
    // Edge algorithm config is shared at the group level (m_model_src), not on
    // the individual pattern.  Per-pattern search params still come from cfg.
    const EdgeMatchConfig* ecfg = m_model_src.config().edgeConfig();
    if (!ecfg) return false;  // only EdgeBased supported here

    // ── Build source pyramid ──────────────────────────────────────────────

    cv::Mat img_gray;
    if (img_edge.channels() == 3)
        cv::cvtColor(img_edge, img_gray, cv::COLOR_BGR2GRAY);
    else if (img_edge.channels() == 4)
        cv::cvtColor(img_edge, img_gray, cv::COLOR_BGRA2GRAY);
    else
        img_gray = img_edge.clone();

    const int top_layer = edgePattern->getTopLayer();
    vector<Mat> vecSrcPyr;
    cv::buildPyramid(img_gray, vecSrcPyr, top_layer);

    // ── Prepare angle list ────────────────────────────────────────────────

    const vector<cv::Mat>* tmpl_pyr = edgePattern->getPyramid();
    double angleStep = atan(2.0 / max(tmpl_pyr->at(top_layer).cols,
                                       tmpl_pyr->at(top_layer).rows)) * R2D;
    vector<double> vecAngles;
    if (cfg->m_toleranceAngle < VISION_TOLERANCE) {
        vecAngles.push_back(0.0);
    } else {
        for (double a = 0; a < cfg->m_toleranceAngle + angleStep; a += angleStep)
            vecAngles.push_back(a);
        for (double a = -angleStep; a > -(cfg->m_toleranceAngle) - angleStep; a -= angleStep)
            vecAngles.push_back(a);
    }

    const int   top_srcW  = vecSrcPyr[top_layer].cols;
    const int   top_srcH  = vecSrcPyr[top_layer].rows;
    Point2f top_center((top_srcW - 1) / 2.0f, (top_srcH - 1) / 2.0f);

    // reduce min score per lower pyramid layer
    vector<double> vecLayerScores(top_layer + 1, cfg->m_minScore);
    for (int li = 1; li <= top_layer; ++li)
        vecLayerScores[li] = vecLayerScores[li - 1] * 0.8;

    cv::Size top_patSize = tmpl_pyr->at(top_layer).size();
    bool calMaxByBlock = ((vecSrcPyr[top_layer].size().area() / top_patSize.area()) > 500)
                         && (max_pos_num > 10);

    vector<MatchParams> vecMatchParams;

    // ── Smallest-layer matching ───────────────────────────────────────────

    for (int idx = 0; idx < static_cast<int>(vecAngles.size()); ++idx) {
        Mat matRotatedSrc, matR = cv::getRotationMatrix2D(top_center, vecAngles[idx], 1.0);
        Mat matResult;
        Point ptMaxLoc;
        double value, maxValue;
        Size sizeBest = GetBestRotationSize(vecSrcPyr[top_layer].size(),
                                            tmpl_pyr->at(top_layer).size(),
                                            vecAngles[idx]);
        float fTx = (sizeBest.width  - 1) / 2.0f - top_center.x;
        float fTy = (sizeBest.height - 1) / 2.0f - top_center.y;
        matR.at<double>(0, 2) += fTx;
        matR.at<double>(1, 2) += fTy;
        warpAffine(vecSrcPyr[top_layer], matRotatedSrc, matR, sizeBest,
                   INTER_LINEAR, BORDER_CONSTANT, Scalar(edgePattern->borderColor()));

        bool isValid = false;
#ifdef NOT_USE_SIMD
        isValid = MatchEdgePattern(matRotatedSrc, edgePattern, matResult,
                                   top_layer, vecLayerScores[top_layer], *ecfg);
#else
        isValid = MatchEdgePattern_SIMD(matRotatedSrc, edgePattern, matResult,
                                        top_layer, vecLayerScores[top_layer]);
#endif
        if (!isValid) continue;

        if (calMaxByBlock) {
            BlockMax blockMax(matResult, tmpl_pyr->at(top_layer).size());
            blockMax.GetMaxValueLoc(maxValue, ptMaxLoc);
            if (maxValue < vecLayerScores[top_layer]) continue;
            vecMatchParams.push_back(MatchParams(Point2f(ptMaxLoc.x - fTx,
                                                          ptMaxLoc.y - fTy),
                                                  maxValue, vecAngles[idx]));
            for (int j = 0; j < max_pos_num + MATCH_CANDIDATE_NUM - 1; ++j) {
                ptMaxLoc = GetNextMaxLoc(matResult, ptMaxLoc,
                                         tmpl_pyr->at(top_layer).size(),
                                         value, max_overlap, blockMax);
                if (value < vecLayerScores[top_layer]) break;
                vecMatchParams.push_back(MatchParams(
                    Point2f(ptMaxLoc.x - fTx, ptMaxLoc.y - fTy), value, vecAngles[idx]));
            }
        } else {
            cv::minMaxLoc(matResult, 0, &maxValue, 0, &ptMaxLoc);
            if (maxValue < vecLayerScores[top_layer]) continue;
            vecMatchParams.push_back(MatchParams(Point2f(ptMaxLoc.x - fTx,
                                                          ptMaxLoc.y - fTy),
                                                  maxValue, vecAngles[idx]));
            for (int j = 0; j < max_pos_num + MATCH_CANDIDATE_NUM - 1; ++j) {
                ptMaxLoc = GetNextMaxLoc(matResult, ptMaxLoc,
                                         tmpl_pyr->at(top_layer).size(),
                                         value, max_overlap);
                if (value < vecLayerScores[top_layer]) break;
                vecMatchParams.push_back(MatchParams(
                    Point2f(ptMaxLoc.x - fTx, ptMaxLoc.y - fTy), value, vecAngles[idx]));
            }
        }
    }
    sort(vecMatchParams.begin(), vecMatchParams.end(), compareScoreBig2Small);

    // ── Fine matching (pyramid descent) ───────────────────────────────────

    const int stop_layer = ecfg->stopAtLayer1 ? 1 : 0;
    vector<MatchParams> vecAllResult;

    for (int i = 0; i < static_cast<int>(vecMatchParams.size()); ++i) {
        double rAngle = -vecMatchParams[i]._matchAngle * D2R;
        Point2f ptLT  = ptRotatePt2f(vecMatchParams[i]._point, top_center, rAngle);

        double _angleStep = atan(2.0 / max(top_srcW, top_srcH)) * R2D;
        vecMatchParams[i]._angleStart = vecMatchParams[i]._matchAngle - _angleStep;
        vecMatchParams[i]._angleEnd   = vecMatchParams[i]._matchAngle + _angleStep;

        if (top_layer <= stop_layer) {
            vecMatchParams[i]._point = Point2d(ptLT * (top_layer == 0 ? 1 : 2));
            vecAllResult.push_back(vecMatchParams[i]);
        } else {
            for (int iLayer = top_layer - 1; iLayer >= stop_layer; --iLayer) {
                _angleStep = atan(2.0 / max(tmpl_pyr->at(iLayer).cols,
                                             tmpl_pyr->at(iLayer).rows)) * R2D;
                vector<double> layerAngles;
                double dMatchedAngle = vecMatchParams[i]._matchAngle;
                if (cfg->m_toleranceAngle < VISION_TOLERANCE) {
                    layerAngles.push_back(0.0);
                } else {
                    for (int k = -1; k <= 1; ++k)
                        layerAngles.push_back(dMatchedAngle + _angleStep * k);
                }

                Point2f ptSrcCenter((vecSrcPyr[iLayer].cols - 1) / 2.0f,
                                    (vecSrcPyr[iLayer].rows - 1) / 2.0f);
                const int iSize = static_cast<int>(layerAngles.size());
                vector<MatchParams> vecNew(iSize);
                int    iMaxIdx  = 0;
                double dBigVal  = -1;

                for (int j = 0; j < iSize; ++j) {
                    Mat matResult, matRotatedSrc;
                    double dMaxVal = 0;
                    Point  ptMax;
                    GetRotatedROI(vecSrcPyr[iLayer], tmpl_pyr->at(iLayer).size(),
                                  ptLT * 2, layerAngles[j], matRotatedSrc);
#ifdef NOT_USE_SIMD
                    MatchEdgePattern(matRotatedSrc, edgePattern, matResult,
                                     iLayer, vecLayerScores[iLayer], *ecfg);
#else
                    MatchEdgePattern_SIMD(matRotatedSrc, edgePattern, matResult,
                                          iLayer, vecLayerScores[iLayer]);
#endif
                    minMaxLoc(matResult, 0, &dMaxVal, 0, &ptMax);
                    vecNew[j] = MatchParams(ptMax, dMaxVal, layerAngles[j]);
                    if (vecNew[j]._matchScore > dBigVal) {
                        iMaxIdx = j;
                        dBigVal = vecNew[j]._matchScore;
                    }
                    if (ptMax.x == 0 || ptMax.y == 0
                        || ptMax.x == matResult.cols - 1
                        || ptMax.y == matResult.rows - 1)
                        vecNew[j]._posOnBorder = true;
                    if (!vecNew[j]._posOnBorder) {
                        for (int y = -1; y <= 1; ++y)
                            for (int x = -1; x <= 1; ++x)
                                vecNew[j]._vecResult[x + 1][y + 1] =
                                    matResult.at<float>(ptMax + Point(x, y));
                    }
                }

                if (vecNew[iMaxIdx]._matchScore < vecLayerScores[iLayer]) break;

                // sub-pixel refinement
                if (ecfg->subPixelEstimation && iLayer == 0
                    && !vecNew[iMaxIdx]._posOnBorder
                    && iMaxIdx != 0 && iMaxIdx != 2) {
                    double dNewX = 0, dNewY = 0, dNewAngle = 0;
                    SubPixEsimation(&vecNew, &dNewX, &dNewY, &dNewAngle,
                                    _angleStep, iMaxIdx);
                    vecNew[iMaxIdx]._point      = Point2d(dNewX, dNewY);
                    vecNew[iMaxIdx]._matchAngle = dNewAngle;
                }

                double dNewAngle = vecNew[iMaxIdx]._matchAngle;
                Point2f ptPad = ptRotatePt2f(ptLT * 2, ptSrcCenter,
                                              dNewAngle * D2R) - Point2f(3, 3);
                Point2f pt(vecNew[iMaxIdx]._point.x + ptPad.x,
                           vecNew[iMaxIdx]._point.y + ptPad.y);
                pt = ptRotatePt2f(pt, ptSrcCenter, -dNewAngle * D2R);

                if (iLayer == stop_layer) {
                    vecNew[iMaxIdx]._point = pt * (stop_layer == 0 ? 1 : 2);
                    vecAllResult.push_back(vecNew[iMaxIdx]);
                } else {
                    vecMatchParams[i]._matchAngle = dNewAngle;
                    vecMatchParams[i]._angleStart = dNewAngle - _angleStep / 2;
                    vecMatchParams[i]._angleEnd   = dNewAngle + _angleStep / 2;
                    ptLT = pt;
                }
            }
        }
    }

    FilterWithScore(&vecAllResult, cfg->m_minScore);

    // ── Overlap filter ────────────────────────────────────────────────────

    const int iDstW = tmpl_pyr->at(stop_layer).cols * (stop_layer == 0 ? 1 : 2);
    const int iDstH = tmpl_pyr->at(stop_layer).rows * (stop_layer == 0 ? 1 : 2);

    for (int i = 0; i < static_cast<int>(vecAllResult.size()); ++i) {
        Point2f ptLT2, ptRT, ptRB, ptLB;
        double dRA = -vecAllResult[i]._matchAngle * D2R;
        ptLT2 = vecAllResult[i]._point;
        ptRT  = Point2f(ptLT2.x + iDstW * (float)cos(dRA),
                        ptLT2.y - iDstW * (float)sin(dRA));
        ptLB  = Point2f(ptLT2.x + iDstH * (float)sin(dRA),
                        ptLT2.y + iDstH * (float)cos(dRA));
        ptRB  = Point2f(ptRT.x  + iDstH * (float)sin(dRA),
                        ptRT.y  + iDstH * (float)cos(dRA));
        vecAllResult[i]._rectR = RotatedRect(ptLT2, ptRT, ptRB);
    }
    FilterWithRotatedRect(&vecAllResult, cv::TM_CCOEFF_NORMED, max_overlap);

    sort(vecAllResult.begin(), vecAllResult.end(), compareScoreBig2Small);

    // ── Build output ──────────────────────────────────────────────────────

    match_objs.clear();
    const int numMatched = static_cast<int>(vecAllResult.size());
    if (numMatched == 0) return false;

    const int iW = tmpl_pyr->at(0).cols;
    const int iH = tmpl_pyr->at(0).rows;

    for (int idx = 0; idx < numMatched; ++idx) {
        MatchedObject mo;
        double dRA = -vecAllResult[idx]._matchAngle * D2R;

        mo.point_LT = vecAllResult[idx]._point;
        mo.point_RT = Point2f(mo.point_LT.x + iW * cos(dRA),
                              mo.point_LT.y - iW * sin(dRA));
        mo.point_LB = Point2f(mo.point_LT.x + iH * sin(dRA),
                              mo.point_LT.y + iH * cos(dRA));
        mo.point_RB = Point2f(mo.point_RT.x + iH * sin(dRA),
                              mo.point_RT.y + iH * cos(dRA));

        Point2f tspt = cfg->m_pickPosition;
        mo.point_Center = Point2f(
            mo.point_LT.x + (tspt.x * cos(dRA) + tspt.y * sin(dRA)),
            mo.point_LT.y + (tspt.y * cos(dRA) - tspt.x * sin(dRA)));
        mo.point_offset = cfg->m_pickingOffset;

        mo.matched_Score = vecAllResult[idx]._matchScore;
        mo.computeMatchAngle(vecAllResult[idx]._matchAngle);
        mo.computePointAngle(cfg->m_angle);
        mo.setParent(edgePattern);

        match_objs.push_back(mo);
        m_final_overlap_result.push_back(vecAllResult[idx]);
    }
    return true;
}

// ── MatchEdgePattern (non-SIMD fallback) ──────────────────────────────────────

/// Scalar (non-SIMD) edge-based template match at one pyramid layer: computes
/// Sobel gradients of `matSrc`, then for every valid window position
/// accumulates a normalized-gradient-dot-product score over the pattern's
/// edge points (Steger-style greedy early-out via normGreediness/normMinScore
/// bounds), writing scores at or above `minScore` into `matResult`.
bool ImageMatcher::MatchEdgePattern(Mat& matSrc, MatchPattern* model,
                                     Mat& matResult, int layer, double minScore,
                                     const EdgeMatchConfig& cfg) {
    const vector<PatternLayer>* patterns = model->getPatterns();
    int result_row = matSrc.rows - patterns->at(layer).image.rows + 1;
    int result_col = matSrc.cols - patterns->at(layer).image.cols + 1;
    if (result_col <= 0 || result_row <= 0) return false;

    Mat imgDest = matSrc.clone();
    Mat gx, gy, magnitude, angle;
    Sobel(imgDest, gx, CV_64F, 1, 0, 3);
    Sobel(imgDest, gy, CV_64F, 0, 1, 3);
    cartToPolar(gx, gy, magnitude, angle);

    const vector<PatternPoint>& modelPattern = patterns->at(layer).patternPoints;
    const long noOfCoords = static_cast<long>(modelPattern.size());
    if (noOfCoords <= 0) return false;

    const float normMinScore  = static_cast<float>(minScore) / noOfCoords;
    const float normGreediness = static_cast<float>(
        (1.0 - cfg.greediness * minScore) / (1.0 - cfg.greediness)) / noOfCoords;

    matResult.create(result_row, result_col, CV_32FC1);
    matResult.setTo(0.0f);

    for (int rowIdx = 0; rowIdx < result_row; ++rowIdx) {
        for (int colIdx = 0; colIdx < result_col; ++colIdx) {
            float partialSum = 0.0f;
            float partialScore = 0.0f;
            for (int count = 0; count < noOfCoords; ++count) {
                const PatternPoint& pt = modelPattern[count];
                int cx = colIdx + pt.Coordinates.x;
                int cy = rowIdx + pt.Coordinates.y;

                float iTx = pt.Derivative.x;
                float iTy = pt.Derivative.y;
                float iSx = static_cast<float>(gx.ptr<float>(cy)[cx]);
                float iSy = static_cast<float>(gy.ptr<float>(cy)[cx]);

                if ((iSx != 0 || iSy != 0) && (iTx != 0 || iTy != 0)) {
                    float mag = static_cast<float>(magnitude.ptr<float>(cy)[cx]);
                    if (mag == 0)
                        partialSum += (iSx * iTx + iSy * iTy);
                    else
                        partialSum += (iSx * iTx + iSy * iTy) * (1.0f / (mag * pt.Magnitude));
                }

                int sumCoords  = count + 1;
                partialScore   = partialSum / sumCoords;
                float brkLow   = (static_cast<float>(minScore) - 1.0f) + normGreediness * sumCoords;
                float brkUp    = normMinScore * sumCoords;
                if (partialScore < std::min(brkLow, brkUp)) break;
            }
            if (partialScore >= static_cast<float>(minScore))
                matResult.ptr<float>(rowIdx)[colIdx] = partialScore;
        }
    }
    return true;
}

// ── MatchEdgePattern_SIMD ─────────────────────────────────────────────────────

/// AVX2 dot-product-of-normalized-gradients score over `length` edge points:
/// for each point, (Sx*Tx + Sy*Ty) / max(SMag*TMag, eps), summed. Processes
/// 8-wide float blocks with FMA and falls back to a scalar loop for the tail.
/// @param iSx source gradient X component per point (32-byte aligned buffer)
/// @param iSy source gradient Y component per point (32-byte aligned buffer)
/// @param iTx template gradient X component per point (32-byte aligned buffer)
/// @param iTy template gradient Y component per point (32-byte aligned buffer)
/// @param SMag source gradient magnitude per point (32-byte aligned buffer)
/// @param TMag template gradient magnitude per point (32-byte aligned buffer)
/// @param length number of edge points to accumulate over
/// @return summed normalized-gradient dot product over all `length` points
inline float edge_template_match_simd_omp(const float* iSx, const float* iSy,
                                           const float* iTx, const float* iTy,
                                           const float* SMag, const float* TMag,
                                           int length) {
    float total_sum = 0.0f;
    __m256 sum_vec  = _mm256_setzero_ps();
    __m256 epsilon  = _mm256_set1_ps(1e-20f);

    for (int i = 0; i <= length - 8; i += 8) {
        __m256 Sx   = _mm256_load_ps(iSx  + i);
        __m256 Sy   = _mm256_load_ps(iSy  + i);
        __m256 Tx   = _mm256_load_ps(iTx  + i);
        __m256 Ty   = _mm256_load_ps(iTy  + i);
        __m256 smag = _mm256_load_ps(SMag + i);
        __m256 tmag = _mm256_load_ps(TMag + i);

        __m256 dot   = _mm256_fmadd_ps(Sx, Tx, _mm256_mul_ps(Sy, Ty));
        __m256 denom = _mm256_max_ps(_mm256_mul_ps(smag, tmag), epsilon);
        sum_vec = _mm256_add_ps(sum_vec, _mm256_div_ps(dot, denom));
    }

    for (int i = (length & ~7); i < length; ++i) {
        float denom = SMag[i] * TMag[i] + 1e-8f;
        total_sum  += (iSx[i] * iTx[i] + iSy[i] * iTy[i]) / denom;
    }

    __m128 lo = _mm256_castps256_ps128(sum_vec);
    __m128 hi = _mm256_extractf128_ps(sum_vec, 1);
    __m128 s  = _mm_hadd_ps(_mm_add_ps(lo, hi), _mm_add_ps(lo, hi));
    s = _mm_hadd_ps(s, s);
    return total_sum + _mm_cvtss_f32(s);
}

/// SIMD (AVX2) edge-based template match at one pyramid layer: computes
/// Sobel gradients + magnitude of `matSrc`, then for every valid window
/// position gathers per-point source gradients into aligned scratch buffers
/// and scores the window via edge_template_match_simd_omp() against the
/// pattern's precomputed gradient buffers, writing the average score at or
/// above `minScore` into `matResult`. Unlike MatchEdgePattern(), this has no
/// early-out and always scores every point.
/// @param matSrc source window to search, gradients computed in place
/// @param model pattern providing the precomputed edge-point gradient
///        buffers (pGx/pGy/pMag) for `layer`
/// @param matResult output score map (CV_32FC1), sized to the valid search
///        window; zero-initialized, only entries scoring >= minScore are set
/// @param layer pyramid layer index selecting which pattern points/buffers to use
/// @param minScore minimum average score a position must reach to be kept
bool ImageMatcher::MatchEdgePattern_SIMD(Mat& matSrc, MatchPattern* model,
                                          Mat& matResult, int layer, double minScore) {
    const vector<PatternLayer>* patterns = model->getPatterns();
    int result_row = matSrc.rows - patterns->at(layer).image.rows + 1;
    int result_col = matSrc.cols - patterns->at(layer).image.cols + 1;
    if (result_col <= 0 || result_row <= 0) return false;

    Mat imgDest = matSrc.clone();
    Mat gx, gy, magnitude;
    Sobel(imgDest, gx, CV_32F, 1, 0, 3);
    Sobel(imgDest, gy, CV_32F, 0, 1, 3);
    cv::magnitude(gx, gy, magnitude);

    const vector<PatternPoint>& modelPattern = patterns->at(layer).patternPoints;
    const long noOfCoords = static_cast<long>(modelPattern.size());
    if (noOfCoords <= 0) return false;

    matResult.create(result_row, result_col, CV_32FC1);
    matResult.setTo(0.0f);

    float* bufSx  = static_cast<float*>(_mm_malloc(noOfCoords * sizeof(float), 32));
    float* bufSy  = static_cast<float*>(_mm_malloc(noOfCoords * sizeof(float), 32));
    float* bufSMag= static_cast<float*>(_mm_malloc(noOfCoords * sizeof(float), 32));
    float* bufTx  = patterns->at(layer).pGx;
    float* bufTy  = patterns->at(layer).pGy;
    float* bufTMag= patterns->at(layer).pMag;

    for (int rowIdx = 0; rowIdx < result_row; ++rowIdx) {
        for (int colIdx = 0; colIdx < result_col; ++colIdx) {
            for (int cnt = 0; cnt < noOfCoords; ++cnt) {
                const Point& coord = modelPattern[cnt].Coordinates;
                int cx = colIdx + coord.x;
                int cy = rowIdx + coord.y;
                bufSx[cnt]   = gx.ptr<float>(cy)[cx];
                bufSy[cnt]   = gy.ptr<float>(cy)[cx];
                bufSMag[cnt] = magnitude.ptr<float>(cy)[cx];
            }
            float score = edge_template_match_simd_omp(bufSx, bufSy, bufTx, bufTy,
                                                        bufSMag, bufTMag,
                                                        static_cast<int>(noOfCoords))
                          / noOfCoords;
            if (score >= static_cast<float>(minScore))
                matResult.ptr<float>(rowIdx)[colIdx] = score;
        }
    }
    _mm_free(bufSx);
    _mm_free(bufSy);
    _mm_free(bufSMag);
    return true;
}

// ── Geometry helpers ──────────────────────────────────────────────────────────

/// Computes the bounding size needed to hold `sizeSrc` after rotating it by
/// `rAngle` degrees about its center, so a warpAffine destination can contain
/// the whole rotated source without clipping. Special-cases 0/90/180/270
/// degrees (axis-aligned, no growth) and otherwise derives the size from the
/// rotated corner extents, falling back to a tighter axis-aligned bound when
/// that estimate looks inconsistent with `sizeDst`'s aspect.
/// @param sizeSrc size of the image being rotated
/// @param sizeDst size of the template being searched for (used to sanity-check the estimate)
/// @param rAngle rotation angle in degrees
/// @return size (width, height) large enough to contain the rotated `sizeSrc`
Size ImageMatcher::GetBestRotationSize(Size sizeSrc, Size sizeDst, double rAngle) {
    double rad = rAngle * D2R;
    Point2f c((sizeSrc.width - 1) / 2.0f, (sizeSrc.height - 1) / 2.0f);
    Point2f ltR = ptRotatePt2f(Point2f(0, 0),                        c, rad);
    Point2f lbR = ptRotatePt2f(Point2f(0, (float)(sizeSrc.height-1)),c, rad);
    Point2f rbR = ptRotatePt2f(Point2f((float)(sizeSrc.width-1),(float)(sizeSrc.height-1)),c,rad);
    Point2f rtR = ptRotatePt2f(Point2f((float)(sizeSrc.width-1),0),  c, rad);

    float topY    = max({ltR.y, lbR.y, rbR.y, rtR.y});
    float bottomY = min({ltR.y, lbR.y, rbR.y, rtR.y});
    float rightX  = max({ltR.x, lbR.x, rbR.x, rtR.x});
    float leftX   = min({ltR.x, lbR.x, rbR.x, rtR.x});

    if (rAngle > 360) rAngle -= 360;
    else if (rAngle < 0) rAngle += 360;

    if (fabs(fabs(rAngle) - 90) < VISION_TOLERANCE || fabs(fabs(rAngle) - 270) < VISION_TOLERANCE)
        return Size(sizeSrc.height, sizeSrc.width);
    if (fabs(rAngle) < VISION_TOLERANCE || fabs(fabs(rAngle) - 180) < VISION_TOLERANCE)
        return sizeSrc;

    double a = rAngle;
    if      (a > 90  && a < 180) a -= 90;
    else if (a > 180 && a < 270) a -= 180;
    else if (a > 270 && a < 360) a -= 270;

    float fH1 = (float)(sizeDst.width  * sin(a * D2R) * cos(a * D2R));
    float fH2 = (float)(sizeDst.height * sin(a * D2R) * cos(a * D2R));
    int halfH = (int)ceil(topY   - c.y - fH1);
    int halfW = (int)ceil(rightX - c.x - fH2);
    Size ret(halfW * 2, halfH * 2);

    bool wrongSize = (sizeDst.width  < ret.width  && sizeDst.height > ret.height)
                  || (sizeDst.width  > ret.width  && sizeDst.height < ret.height)
                  || sizeDst.area() > ret.area();
    if (wrongSize)
        ret = Size((int)(rightX - leftX + 0.5f), (int)(topY - bottomY + 0.5f));
    return ret;
}

/// Rotates point `in` about center `org` by `angle` radians, using a
/// mathematical (y-up) rotation convention: the y-down image coordinate is
/// mirrored about `org.y` before applying the standard rotation formula, then
/// mirrored back, so the rotation direction matches the algorithm's angle
/// convention rather than raw image-pixel rotation.
/// @param in point to rotate, in image pixel coordinates
/// @param org rotation center, in image pixel coordinates
/// @param angle rotation angle in radians
/// @return rotated point, in image pixel coordinates
Point2f ImageMatcher::ptRotatePt2f(Point2f in, Point2f org, double angle) {
    double h  = org.y * 2;
    double y1 = h - in.y;
    double y2 = h - org.y;
    double x  = (in.x - org.x) * cos(angle) - (y1 - org.y) * sin(angle) + org.x;
    double y  = (in.x - org.x) * sin(angle) + (y1 - org.y) * cos(angle) + y2;
    return Point2f((float)x, (float)(-(y - h)));
}

/// Suppresses the region around the previous best match (`ptMaxLoc`) in
/// `matResult` by filling a rectangle sized from `sizeTemplate` and
/// `maxOverlap` with -1, then finds and returns the next-best score location.
Point ImageMatcher::GetNextMaxLoc(Mat& matResult, Point ptMaxLoc,
                                   Size sizeTemplate, double& maxValue, double maxOverlap) {
    int sx = (int)(ptMaxLoc.x - sizeTemplate.width  * (1 - maxOverlap));
    int sy = (int)(ptMaxLoc.y - sizeTemplate.height * (1 - maxOverlap));
    cv::rectangle(matResult,
                  Rect(sx, sy,
                       (int)(2 * sizeTemplate.width  * (1 - maxOverlap)),
                       (int)(2 * sizeTemplate.height * (1 - maxOverlap))),
                  Scalar(-1), cv::FILLED);
    Point ptNew;
    cv::minMaxLoc(matResult, 0, &maxValue, 0, &ptNew);
    return ptNew;
}

/// Overload used with the block-max acceleration structure (see BlockMax):
/// suppresses the region around `ptMaxLoc` in `matResult` and in `blockMax`'s
/// per-block cache, then returns the next-best location from `blockMax`
/// instead of a full re-scan of `matResult`.
/// @param matResult score map to search; modified in place (region suppressed)
/// @param ptMaxLoc location of the previous best match to suppress around
/// @param sizeTemplate size of the template, used to size the suppression rectangle
/// @param maxValue output; set to the next-best score found
/// @param maxOverlap allowed overlap fraction (0..1) that shrinks the suppressed area
/// @param blockMax block-max cache kept in sync with `matResult`; updated in place
/// @return location of the next-best score, from `blockMax`
Point ImageMatcher::GetNextMaxLoc(Mat& matResult, Point ptMaxLoc,
                                   Size sizeTemplate, double& maxValue,
                                   double maxOverlap, BlockMax& blockMax) {
    int sx = (int)(ptMaxLoc.x - sizeTemplate.width  * (1 - maxOverlap));
    int sy = (int)(ptMaxLoc.y - sizeTemplate.height * (1 - maxOverlap));
    Rect ignore(sx, sy,
                (int)(2 * sizeTemplate.width  * (1 - maxOverlap)),
                (int)(2 * sizeTemplate.height * (1 - maxOverlap)));
    cv::rectangle(matResult, ignore, Scalar(-1), cv::FILLED);
    blockMax.UpdateMax(ignore);
    Point ptRet;
    blockMax.GetMaxValueLoc(maxValue, ptRet);
    return ptRet;
}

/// Extracts a padded, de-rotated region of interest from `matSrc` centered at
/// `ptLT`: rotates `ptLT` into the source's rotated frame, then warps `matSrc`
/// with a rotation matrix translated so that point lands at a fixed 3px
/// margin inside the output, producing an axis-aligned crop of `size` plus
/// 6px padding suitable for re-matching at a finer pyramid layer.
/// @param matSrc source image to extract from
/// @param size nominal size of the region to extract (padded by 6px on each axis)
/// @param ptLT reference point (in `matSrc`'s rotated coordinate frame) to center the extraction on
/// @param dAngle rotation angle in degrees applied while extracting
/// @param matROI output; overwritten with the extracted, padded region
void ImageMatcher::GetRotatedROI(Mat& matSrc, Size size, Point2f ptLT,
                                  double dAngle, Mat& matROI) {
    double rad = dAngle * D2R;
    Point2f c((matSrc.cols - 1) / 2.0f, (matSrc.rows - 1) / 2.0f);
    Point2f ptLT_rot = ptRotatePt2f(ptLT, c, rad);
    Size szPad(size.width + 6, size.height + 6);
    Mat rMat = getRotationMatrix2D(c, dAngle, 1);
    rMat.at<double>(0, 2) -= ptLT_rot.x - 3;
    rMat.at<double>(1, 2) -= ptLT_rot.y - 3;
    warpAffine(matSrc, matROI, rMat, szPad);
}

// ── Sub-pixel estimation ──────────────────────────────────────────────────────

/// Refines the best match's (x, y, angle) to sub-pixel/sub-degree precision by
/// fitting a 2nd-order polynomial surface to the 3x3x3 (x, y, angle)
/// neighborhood of cached scores around `(*vec)[iMaxIdx]` (least squares via
/// the normal equations) and solving for the surface's stationary point.
bool ImageMatcher::SubPixEsimation(vector<MatchParams>* vec, double* dNewX,
                                    double* dNewY, double* dNewAngle,
                                    double dAngleStep, int iMaxIdx) {
    Mat matA(27, 10, CV_64F);
    Mat matS(27, 1,  CV_64F);

    double dx0 = (*vec)[iMaxIdx]._point.x;
    double dy0 = (*vec)[iMaxIdx]._point.y;
    double dt0 = (*vec)[iMaxIdx]._matchAngle;
    int row = 0;
    for (int theta = 0; theta <= 2; ++theta) {
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                double dX = dx0 + x;
                double dY = dy0 + y;
                double dT = (dt0 + (theta - 1) * dAngleStep) * D2R;
                matA.at<double>(row, 0) = dX * dX;
                matA.at<double>(row, 1) = dY * dY;
                matA.at<double>(row, 2) = dT * dT;
                matA.at<double>(row, 3) = dX * dY;
                matA.at<double>(row, 4) = dX * dT;
                matA.at<double>(row, 5) = dY * dT;
                matA.at<double>(row, 6) = dX;
                matA.at<double>(row, 7) = dY;
                matA.at<double>(row, 8) = dT;
                matA.at<double>(row, 9) = 1.0;
                matS.at<double>(row, 0) = (*vec)[iMaxIdx + (theta - 1)]._vecResult[x + 1][y + 1];
                ++row;
            }
        }
    }
    Mat matZ = (matA.t() * matA).inv() * matA.t() * matS;
    Mat matZ_t;
    transpose(matZ, matZ_t);
    double* dZ = matZ_t.ptr<double>(0);
    Mat matK1 = (Mat_<double>(3, 3) << 2*dZ[0], dZ[3], dZ[4],
                                        dZ[3], 2*dZ[1], dZ[5],
                                        dZ[4], dZ[5], 2*dZ[2]);
    Mat matK2 = (Mat_<double>(3, 1) << -dZ[6], -dZ[7], -dZ[8]);
    Mat delta = matK1.inv() * matK2;
    *dNewX     = delta.at<double>(0, 0);
    *dNewY     = delta.at<double>(1, 0);
    *dNewAngle = delta.at<double>(2, 0) * R2D;
    return true;
}

// ── Score / overlap filters ───────────────────────────────────────────────────

/// Sorts `vec` by descending score, then truncates it at the first entry
/// scoring below `dScore` (all entries after are discarded).
/// @param vec match candidates to sort and filter in place
/// @param dScore minimum score a candidate must reach to be kept
void ImageMatcher::FilterWithScore(vector<MatchParams>* vec, double dScore) {
    sort(vec->begin(), vec->end(), compareScoreBig2Small);
    int iDel = static_cast<int>(vec->size()) + 1;
    for (int i = 0; i < static_cast<int>(vec->size()); ++i) {
        if ((*vec)[i]._matchScore < dScore) { iDel = i; break; }
    }
    if (iDel <= static_cast<int>(vec->size()))
        vec->erase(vec->begin() + iDel, vec->end());
}

/// Removes mutually-overlapping candidates from `vec`: for every pair whose
/// rotated rectangles (`_rectR`) intersect, computes the intersection
/// polygon area against candidate i's rect area and, if fully overlapping or
/// the overlap ratio exceeds `dMaxOverLap`, marks the worse-scoring one
/// deleted (worse meaning lower score, or higher score when `iMethod` is
/// cv::TM_SQDIFF). Deleted entries are erased from `vec` at the end.
void ImageMatcher::FilterWithRotatedRect(vector<MatchParams>* vec, int iMethod,
                                          double dMaxOverLap,
                                          std::vector<int>* del_indexes) {
    const int iSize = static_cast<int>(vec->size());
    for (int i = 0; i < iSize - 1; ++i) {
        if (vec->at(i)._delete) continue;
        for (int j = i + 1; j < iSize; ++j) {
            if (vec->at(j)._delete) continue;
            vector<Point2f> inter;
            int iType = rotatedRectangleIntersection(vec->at(i)._rectR,
                                                      vec->at(j)._rectR, inter);
            if (iType == cv::INTERSECT_NONE) continue;
            int iDel;
            if (iType == cv::INTERSECT_FULL) {
                iDel = (iMethod == cv::TM_SQDIFF)
                     ? (vec->at(i)._matchScore <= vec->at(j)._matchScore ? j : i)
                     : (vec->at(i)._matchScore >= vec->at(j)._matchScore ? j : i);
                vec->at(iDel)._delete = true;
                if (del_indexes) del_indexes->push_back(iDel);
            } else if (static_cast<int>(inter.size()) >= 3) {
                SortPtWithCenter(inter);
                double ratio = contourArea(inter) / vec->at(i)._rectR.size.area();
                if (ratio > dMaxOverLap) {
                    iDel = (iMethod == cv::TM_SQDIFF)
                         ? (vec->at(i)._matchScore <= vec->at(j)._matchScore ? j : i)
                         : (vec->at(i)._matchScore >= vec->at(j)._matchScore ? j : i);
                    vec->at(iDel)._delete = true;
                    if (del_indexes) del_indexes->push_back(iDel);
                }
            }
        }
    }
    for (auto it = vec->begin(); it != vec->end();)
        it = (*it)._delete ? vec->erase(it) : ++it;
}

/// Reorders `vecSort` into angular order (ascending) around the points'
/// centroid, so a polygon's vertices (e.g. a rotated-rect intersection) can
/// be traced consistently for area/contour computations.
/// @param vecSort points to reorder in place
void ImageMatcher::SortPtWithCenter(vector<Point2f>& vecSort) {
    const int n = static_cast<int>(vecSort.size());
    Point2f center;
    for (auto& p : vecSort) center += p;
    center /= n;

    vector<pair<Point2f, double>> va(n);
    for (int i = 0; i < n; ++i) {
        va[i].first = vecSort[i];
        Point2f v(vecSort[i].x - center.x, vecSort[i].y - center.y);
        float norm = v.x * v.x + v.y * v.y;
        double angle = (v.y < 0) ?  acos(v.x / norm) * R2D
                     : (v.y > 0) ? (360.0 - acos(v.x / norm) * R2D)
                     : (v.x - center.x > 0 ? 0.0 : 180.0);
        va[i].second = angle;
    }
    sort(va.begin(), va.end(), comparePtWithAngle);
    for (int i = 0; i < n; ++i) vecSort[i] = va[i].first;
}

/// Debug/visualization helper: draws each matched object's bounding
/// quadrilateral (blue) plus its center (red dot) and top-left corner (green
/// dot) onto `drawImage`.
/// @param drawImage image to draw onto in place
/// @param matchedResults objects whose geometry is drawn
void ImageMatcher::DrawMatchResult(Mat& drawImage,
                                    vector<MatchedObject>& matchedResults) {
    cv::Scalar box_color(255, 0, 0);
    for (auto& obj : matchedResults) {
        cv::line(drawImage, obj.point_LT, obj.point_RT, box_color, 2, cv::LINE_AA);
        cv::line(drawImage, obj.point_RT, obj.point_RB, box_color, 2, cv::LINE_AA);
        cv::line(drawImage, obj.point_RB, obj.point_LB, box_color, 2, cv::LINE_AA);
        cv::line(drawImage, obj.point_LB, obj.point_LT, box_color, 2, cv::LINE_AA);
        cv::circle(drawImage, obj.point_Center, 2, cv::Scalar(0, 0, 255), 2);
        cv::circle(drawImage, obj.point_LT,     2, cv::Scalar(0, 255, 0), 2);
    }
}

/// Tests whether `pt` falls within the axis-aligned box spanned by `tl`/`br`
/// (order-independent: each corner pair is min/max-normalized first).
/// @return true if `pt` lies within the box (inclusive of its edges)
bool ImageMatcher::pointInBox(const cv::Point2f& tl, const cv::Point2f& br, const cv::Point2f& pt) {
    float left   = std::min(tl.x, br.x);
    float right  = std::max(tl.x, br.x);
    float top    = std::min(tl.y, br.y);
    float bottom = std::max(tl.y, br.y);

    return (pt.x >= left && pt.x <= right && pt.y >= top && pt.y <= bottom);
}

/// Checks whether the injected robot-pickability checker (m_pickingChecker)
/// considers `obj` reachable and (if configured) collision-free: converts the
/// object's image position/angle plus the current match_result's crop offset
/// to a world pick pose, then asks the checker if that pose is reachable
/// (and, if enabled, collision-checked).
/// @param obj matched object whose image position/angle is evaluated
/// @return true if no checker is injected (advisory-only, does not gate
///         matching), or the checker's own reachability/collision verdict
bool ImageMatcher::robotPossiblePickingCheck(const MatchedObject& obj) const {
    // No robot checker configured: the check is advisory, do not gate matching.
    if (!m_pickingChecker) {
        return true;
    }

    // Step 1 — image position/angle -> world pick pose. point_Center is in the
    // matcher's input frame; cropOffsetPoint maps it back to that input's full
    // frame (see the header's frame note).
    const double imgX = obj.point_Center.x + match_result.cropOffsetPoint.x;
    const double imgY = obj.point_Center.y + match_result.cropOffsetPoint.y;
    WorldPickPose pose;
    if (!m_pickingChecker->imageToWorld(imgX, imgY, obj.point_angle, pose))
        return false;

    // Step 2 (+3) — reachability via solveAll IK; the adapter additionally runs
    // the simplified-mesh self-collision check when the robot config enabled it.
    return m_pickingChecker->isPickable(pose, m_pickingChecker->collisionCheckEnabled());
}

} // namespace mtc
