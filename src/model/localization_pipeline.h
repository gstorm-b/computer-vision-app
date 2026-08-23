#ifndef LOCALIZATION_PIPELINE_H
#define LOCALIZATION_PIPELINE_H

#include <memory>

#include <opencv2/core/mat.hpp>

#include "matching/image_matcher.h"
#include "model/camera_workspace.h"

namespace mtc {
class MatchGroup;
}

/**
 * @file localization_pipeline.h
 * @brief LocalizationPipeline — loads a MatchGroup's learned model into an ImageMatcher and
 *        drives runtime/commission matching runs.
 */
namespace vc::model {

/**
 * @class LocalizationPipeline
 * @brief Wires a MatchGroup's learned patterns into an ImageMatcher and runs workspace-aware
 *        matching for both the runtime cycle and the commission/test flow.
 */
class LocalizationPipeline {
public:
    /// Max objects the runtime cycle searches for (commission searches all: -1).
    static constexpr int kRuntimeMaxObjects = 2;

    /**
     * @brief Loads the group's learned model into the matcher (clone config + learnPattern per
     *        pattern). Expensive — the runtime path calls this only when the active group changes.
     * @param[out] matcher freshly-constructed matcher (empty model) to load into; callers reload
     *             by recreating the matcher on group change, so no model reset is performed here
     * @param[in]  group   match group whose patterns are cloned/learned; a null group fails
     *             immediately
     * @return false if `group` is null, has no loadable patterns, or the resulting model ends
     *         up empty; true otherwise
     */
    bool loadModel(mtc::ImageMatcher &matcher,
                   const std::shared_ptr<mtc::MatchGroup> &group) const;

    /**
     * @brief Runs matching on a matcher whose model is already loaded: applies the workspace
     *        ROI(s), injects the (optional) robot-pickability checker, sets the image, and matches.
     * @param[in,out] matcher matcher with an already-loaded model (see loadModel()); mutated by
     *                running the match
     * @param[in] workspace      working/condition ROI settings applied before matching
     * @param[in] image          source image to match against; an empty image fails immediately
     * @param[in] pickingChecker optional robot-pickability checker forwarded to the matcher
     * @param[in] maxObjects     max objects to find; negative means "find all"
     * @return the matcher's match_result; a default-constructed MatchResult if `image` is empty
     */
    mtc::MatchResult runMatchOn(mtc::ImageMatcher &matcher,
                                const CameraWorkspace &workspace,
                                const cv::Mat &image,
                                const mtc::IRobotPickingChecker *pickingChecker,
                                int maxObjects) const;

    /**
     * @brief Commission/test matching: builds a throwaway matcher per call (not the hot path).
     *        Loads `group`'s model and searches all objects (maxObjects = -1).
     * @param[in] group          match group to load and match against
     * @param[in] workspace      working/condition ROI settings applied before matching
     * @param[in] image          source image to match against
     * @param[in] pickingChecker optional robot-pickability checker forwarded to the matcher
     * @return the match result; a default-constructed MatchResult if loadModel() fails
     */
    mtc::MatchResult runMatchCommision(std::shared_ptr<mtc::MatchGroup> group,
                              const CameraWorkspace &workspace,
                              const cv::Mat &image,
                              const mtc::IRobotPickingChecker *pickingChecker = nullptr) const;
};

} // namespace vc::model

#endif // LOCALIZATION_PIPELINE_H
