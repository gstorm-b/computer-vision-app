#ifndef IMAGE_MATCHER_H
#define IMAGE_MATCHER_H

#include "utils_block_max.h"
#include "match_params.h"
#include "match_object.h"
#include "match_group.h"
#include "edge_match_config.h"
#include "robot_picking_checker.h"

#define SOURCE_THERSHOLD_TOLERANCE  (double)0.3

/// Vision/matching module: the ImageMatcher template-matching engine and its
/// MatchResult output type.
namespace mtc {

/// Snapshot of one ImageMatcher::matching() run: the matched objects, the
/// annotated output image, timing, and picking-availability stats.
struct MatchResult {
    std::vector<MatchedObject> Objects;   ///< Matched objects, sorted by pick priority (see sortMatchedObjects).
    cv::Point2f cropOffsetPoint;           ///< Origin of the matching crop in the original source image (ROI_tl if usingRoi, else (0,0)).
    double      ExecutionTime{0.0};        ///< Wall-clock duration of the matching() call, in milliseconds.
    cv::Mat     Image;                     ///< Annotated copy of the (possibly cropped) source image with matched masks and gripper boxes drawn in.
    int         imageCols{0};              ///< Column count of Image / the matching region.
    int         imageRows{0};              ///< Row count of Image / the matching region.
    bool        isFoundMatchObject{false}; ///< True when at least one object was matched.
    int         totalPossiblePicking{0};   ///< Count of matched objects that are collision-free, inside the condition ROI, and robot-pickable.
    bool        isAreaLessThanLimits{false}; ///< True when total non-noise contour area is below (max model contour area x lowWorkpieceRatio) — a low-material/empty-tray signal.
};

/// Edge-based template matching engine: runs an OpenCV pyramid, coarse-to-fine,
/// rotation-searching shape/edge match for every registered pattern against a
/// source image, then filters, sorts, and annotates the results.
///
/// Algorithm config lives in the group's MatchGroupConfig::typeConfig
/// (EdgeMatchConfig), shared by every pattern in the group.  Runtime search
/// parameters (max_pos_num, max_overlap) are kept here as they govern the
/// search over the whole image, not a single pattern.
class ImageMatcher {
public:
    /// Default-constructs an empty matcher (no source image, empty model group).
    ImageMatcher();

    /// Returns the model group holding this matcher's registered patterns and
    /// shared edge-match config. Same underlying object as getModel().
    MatchGroup* getPatternGroup();
    /// Returns the model group holding this matcher's registered patterns and
    /// shared edge-match config. Same underlying object as getPatternGroup().
    MatchGroup* getModel() { return &m_model_src; }

    /// Returns how many patterns are registered in the model group.
    int  getTemplateSourceSize();
    /// Loads the source image to match against from disk (cv::imread), replacing
    /// any previously set image.
    void setImageSource(std::string path);
    /// Sets the source image to match against, cloning `img` so the matcher owns
    /// an independent copy.
    void setImageSource(cv::Mat img);
    /// Sets the rectangle the source image is cropped to before searching, used
    /// only when the `usingRoi` flag is passed to matching().
    void setMatchingROI(cv::Point tl, cv::Point br);
    /// Sets the rectangle used to classify matched objects as inside/outside the
    /// pick condition ROI (independent of the matching crop ROI), used only when
    /// the `usingConditionRoi` flag is passed to matching().
    void setMatchingConditionROI(cv::Point tl, cv::Point br);

    /// Replaces the source image (clone of `image`), then runs the same pipeline
    /// as the other matching() overload.
    /// @param boundingBoxChecking accepted but not read by the current implementation
    /// @param objectsNum stop scanning further contour candidates once this many
    ///        objects have been matched and at least one is possible-to-pick;
    ///        a negative value disables the early stop
    /// @param usingRoi crop the source image to the matching ROI before searching
    /// @param usingConditionRoi classify matched objects against the condition ROI
    void matching(cv::Mat& image, bool boundingBoxChecking, int objectsNum, bool usingRoi, bool usingConditionRoi);
    /// Core matching pipeline: grayscales + thresholds + finds contours in the
    /// current source image, shortlists contours whose area falls in each
    /// pattern's expected range, runs MatchEdge per candidate/pattern, resolves
    /// picking/collision geometry and robot-pickability, sorts, and rewrites
    /// match_result (including the annotated match_result.Image).
    /// @param boundingBoxChecking accepted but not read by the current implementation
    /// @param objectsNum stop scanning further contour candidates once this many
    ///        objects have been matched and at least one is possible-to-pick;
    ///        a negative value disables the early stop
    /// @param usingRoi crop the source image to the matching ROI before searching
    /// @param usingConditionRoi classify matched objects against the condition ROI
    void matching(bool boundingBoxChecking, int objectsNum, bool usingRoi,  bool usingConditionRoi);

    /// Sets the source image by cloning `img`; reference-parameter counterpart to
    /// setImageSource(cv::Mat).
    void setMatchSourceImage(cv::Mat& img);
    /// Runs the coarse-to-fine pyramid, rotation-searching edge match of one
    /// pattern against `imgEdge`: coarse search at the top pyramid layer, then
    /// descends the pyramid refining position/angle (with optional sub-pixel
    /// estimation at layer 0), filters by score and by rotated-rect overlap, and
    /// builds one MatchedObject per surviving candidate.
    /// @param imgEdge image region to search (already cropped to the candidate area)
    /// @param pattern model pattern to match; must be learned (isPatternLearned())
    /// @param matchObjs output; filled with one MatchedObject per accepted match
    ///        (points/angle/score set; parent set to `pattern`, not yet translated
    ///        to full-image coordinates)
    /// @return true if at least one match scored above the pattern's minimum
    ///         score; false if the pattern is unlearned, the images are
    ///         incompatible in size, the group's algorithm config is not
    ///         EdgeBased, or no candidate reached the score threshold
    bool MatchEdge(cv::Mat& imgEdge, MatchPattern* pattern,
                   std::vector<MatchedObject>& matchObjs);

    /// Sort matched objects in place by pick priority, then by score or angle.
    ///
    /// Primary key — pick priority (best first):
    ///   1. no collision AND inside the condition ROI
    ///   2. collision   AND inside the condition ROI
    ///   3. the rest (outside the condition ROI)
    ///
    /// Secondary key — applied within each priority group:
    ///   - highest matched_Score first (default), or
    ///   - if the group config's m_sortByAngle is set, smallest angular error of
    ///     point_angle vs m_sortConditionAngle first (score is ignored).
    void sortMatchedObjects(std::vector<MatchedObject>& objects);

    /// Inject the robot-pickability checker (dependency inversion; non-owning).
    /// Pass nullptr to disable. The owner builds the concrete adapter from the
    /// calibration + robot config and keeps it alive while this matcher is used.
    void setRobotPickingChecker(const IRobotPickingChecker* checker) {
        m_pickingChecker = checker;
    }

    /// Whether the robot can actually pick this matched object. Sequential steps:
    ///   1. convert the object's image position/angle to a world pick pose,
    ///   2. solveAll IK to confirm the pose is reachable,
    ///   3. (only if the robot config enabled it) simplified-mesh self-collision.
    /// Returns true when no checker is injected (the check is advisory: an
    /// unconfigured robot does not gate matching).
    ///
    /// Frame note: the image coordinates handed to the checker are
    /// point_Center + match_result.cropOffsetPoint, i.e. the matcher's own input
    /// frame. If the host pre-crops the image before matching, it must inject a
    /// checker whose calibration is consistent with that frame (or set the
    /// matching ROI here instead of pre-cropping).
    bool robotPossiblePickingCheck(const MatchedObject& obj) const;

public:
    int    max_pos_num = 70;  ///< Max candidate positions kept per rotation angle in the coarse (top pyramid layer) search, before overlap filtering.
    double max_overlap = 0.2; ///< Max allowed rotated-rect overlap ratio between two candidates before the lower-scoring one is discarded.
    MatchResult match_result; ///< Output of the most recent matching() call.

private:
    /// One image contour that survived the coarse area filter and is a
    /// candidate to be edge-matched against one or more patterns.
    struct PossibleObject {
        int           conIndex;         ///< Index of the source contour (into srcContours) this candidate came from.
        cv::Rect      conBoundingRect;   ///< Padded bounding rect (+/- SMALL_CROP_OFFSET, clamped to the image) used to crop the source image for per-pattern matching.
        cv::RotatedRect conMinRectArea;  ///< Minimum-area rotated rect of the contour.
        std::vector<int> modelCheckList; ///< Indexes into the group's pattern list whose contour-area range matches this contour's area.
    };

    // EdgeBased matching steps — cfg provides per-pattern algorithm settings
    /// Non-SIMD edge-based template match: for every position, accumulates the
    /// normalized dot product between the pattern's gradient directions and the
    /// source image's Sobel gradient at that position, with an early-exit once
    /// the running partial score can no longer reach minScore.
    /// @param matSrc source pyramid-layer image to search (already rotated)
    /// @param model pattern providing the per-layer gradient template (getPatterns())
    /// @param matResult output score map (CV_32FC1), sized (rows-tmplRows+1) x (cols-tmplCols+1)
    /// @param layer pyramid layer to match at
    /// @param minScore minimum score used both for early-exit and to accept a result
    /// @param cfg edge-match config (used for cfg.greediness)
    /// @return false if the template does not fit in matSrc or the pattern has no points at this layer
    bool MatchEdgePattern(cv::Mat& matSrc, MatchPattern* model,
                          cv::Mat& matResult, int layer, double minScore,
                          const EdgeMatchConfig& cfg);
    /// SIMD (AVX2) counterpart to MatchEdgePattern: same normalized
    /// gradient-dot-product score, computed via edge_template_match_simd_omp over
    /// 32-byte-aligned scratch buffers instead of the scalar early-exit loop.
    /// @return false if the template does not fit in matSrc or the pattern has no points at this layer
    bool MatchEdgePattern_SIMD(cv::Mat& matSrc, MatchPattern* model,
                               cv::Mat& matResult, int layer, double minScore);

    /// Computes the padded canvas size needed to hold sizeDst after
    /// warpAffine-rotating it by rAngle around its own center without clipping,
    /// snapping to exact swapped/unchanged dimensions at multiples of 90 degrees.
    cv::Size  GetBestRotationSize(cv::Size sizeSrc, cv::Size sizeDst, double rAngle);
    /// Rotates point `ptInput` by `angle` (radians) around `ptOrg`, in the
    /// flipped (image, Y-down) coordinate convention used by this class's
    /// pyramid matching.
    cv::Point2f ptRotatePt2f(cv::Point2f ptInput, cv::Point2f ptOrg, double angle);
    /// Finds the next-best match location in matResult after suppressing
    /// (setting to -1) the region around ptMaxLoc sized by (1 - maxOverlap), so a
    /// previously reported peak cannot be picked twice.
    /// @param matResult score map, mutated in place (suppressed region filled with -1)
    /// @param ptMaxLoc location of the previous peak to suppress around
    /// @param sizeTemplate template size, scaled by (1 - maxOverlap) to size the suppressed rectangle
    /// @param maxValue output; the next maximum score found
    /// @param maxOverlap suppression ratio: 0 suppresses just the template footprint, closer to 1 suppresses more
    /// @return location of the next maximum
    cv::Point GetNextMaxLoc(cv::Mat& matResult, cv::Point ptMaxLoc,
                            cv::Size sizeTemplate, double& maxValue, double maxOverlap);
    /// Same suppression logic as the other overload, but also updates
    /// `blockMax`'s block-max acceleration structure over the suppressed region
    /// (used when the coarse search is running with BlockMax enabled).
    cv::Point GetNextMaxLoc(cv::Mat& matResult, cv::Point ptMaxLoc,
                            cv::Size sizeTemplate, double& maxValue,
                            double maxOverlap, BlockMax& blockMax);
    /// Crops and rotates a (size + 6)-padded region of matSrc, centered at ptLT
    /// rotated by dAngle around matSrc's center, into matROI — used to search
    /// only the neighbourhood of a coarse match at a finer pyramid layer.
    void GetRotatedROI(cv::Mat& matSrc, cv::Size size, cv::Point2f ptLT,
                       double dAngle, cv::Mat& matROI);
    /// Refines a coarse match to sub-pixel/sub-degree precision by fitting a
    /// quadratic surface (least squares over a fixed 3x3x3 neighbourhood in x, y,
    /// and angle around iMaxScoreIndex) and solving for the surface's stationary point.
    /// @param vec candidate list; entry iMaxScoreIndex and its two angle-neighbours supply the 27 fitted samples
    /// @param dNewX output; refined X offset from the peak
    /// @param dNewY output; refined Y offset from the peak
    /// @param dNewAngle output; refined angle offset (degrees) from the peak
    /// @param dAngleStep angular spacing (degrees) between the neighbouring angle samples
    /// @param iMaxScoreIndex index of the peak candidate in vec
    /// @return always true (the fit is not checked for a singular system)
    bool SubPixEsimation(std::vector<MatchParams>* vec, double* dNewX,
                         double* dNewY, double* dNewAngle,
                         double dAngleStep, int iMaxScoreIndex);
    /// Sorts vec by score (descending) and truncates it at the first entry
    /// scoring below dScore, discarding the low-scoring tail.
    void FilterWithScore(std::vector<MatchParams>* vec, double dScore);
    /// Removes overlapping candidates: for every pair whose rotated rects
    /// intersect, drops the lower-scoring one (both if one fully contains the
    /// other, or if the intersection area exceeds dMaxOverLap fraction of rect
    /// i's area).
    /// @param vec candidate list, mutated in place: entries are marked _delete then erased
    /// @param iMethod comparison method; TM_SQDIFF keeps the lower score, any other value (as used here, TM_CCOEFF_NORMED) keeps the higher score
    /// @param dMaxOverLap max allowed overlap-area ratio before the lower-ranked candidate is dropped
    /// @param del_indexes optional; if non-null, receives the pre-erase indexes of every candidate marked for deletion
    void FilterWithRotatedRect(std::vector<MatchParams>* vec, int iMethod,
                               double dMaxOverLap,
                               std::vector<int>* del_indexes = nullptr);
    /// Sorts polygon vertices vecSort into angular order around their centroid
    /// (used to make an intersection polygon's vertices consistent before
    /// computing its area).
    void SortPtWithCenter(std::vector<cv::Point2f>& vecSort);
    /// Debug/visualization helper: draws each matched object's oriented bounding
    /// box (blue) plus its center and top-left corner (red/green dots) onto drawImage.
    void DrawMatchResult(cv::Mat& drawImage,
                         std::vector<MatchedObject>& matchedResults);
    /// Returns whether pt lies within the axis-aligned box spanned by tl and br
    /// (corners may be given in either order).
    bool pointInBox(const cv::Point2f& tl, const cv::Point2f& br, const cv::Point2f& pt);

private:
    cv::Mat      m_img_source;       ///< Current source image to match against (set via setImageSource/setMatchSourceImage/matching(image, ...)).
    MatchGroup   m_model_src;        ///< Owns the registered patterns plus the shared MatchGroupConfig (EdgeMatchConfig) used by MatchEdge.
    cv::Point2f  ROI_tl;             ///< Top-left of the matching crop ROI (used when usingRoi is true).
    cv::Point2f  ROI_br;             ///< Bottom-right of the matching crop ROI.
    cv::Point2f  Condition_ROI_tl;   ///< Top-left of the pick-condition ROI (used when usingConditionRoi is true).
    cv::Point2f  Condition_ROI_br;   ///< Bottom-right of the pick-condition ROI.

    std::vector<MatchParams> m_final_overlap_result; ///< Raw per-pattern match candidates (post-overlap-filter) from the most recent MatchEdge call.

    /// Non-owning robot-pickability checker (nullptr => check disabled).
    const IRobotPickingChecker* m_pickingChecker{nullptr};

    const double lowerThreshRatio = 1.0 - SOURCE_THERSHOLD_TOLERANCE; ///< Lower bound of the contour-area/model-area ratio window used to shortlist candidate contours for a pattern.
    const double upperThreshRatio = 4.0 + SOURCE_THERSHOLD_TOLERANCE; ///< Upper bound of that contour-area/model-area ratio window.
};

} // namespace mtc
#endif // IMAGE_MATCHER_H
