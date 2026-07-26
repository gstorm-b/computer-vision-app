#include "model/localization_pipeline.h"

#include "core/logger/app_logger.h"
#include "matching/image_matcher.h"
#include "matching/match_group.h"
#include "matching/match_pattern.h"

/// LocalizationPipeline member definitions: builds a matcher's pattern model from a MatchGroup
/// and runs workspace-aware ImageMatcher matching for the runtime and commission paths.
namespace vc::model {

bool LocalizationPipeline::loadModel(mtc::ImageMatcher &matcher,
                                     const std::shared_ptr<mtc::MatchGroup> &group) const {
    if (!group) {
        LOG_USER_ERR << "Localization matching failed: match group is null.";
        return false;
    }

    // Expects a freshly-constructed matcher (empty model). Callers reload by
    // recreating the matcher on group change, so no model reset is needed here.
    auto *model = matcher.getModel();
    group->cloneConfigTo(*model);

    for (const auto &patternPtr : group->patterns()) {
        if (!patternPtr)
            continue;

        mtc::MatchPatternConfig cfg = patternPtr->config();
        if (cfg.m_rawImage.empty())
            continue;

        auto r = model->addPattern(cfg);
        if (!r)
            continue;

        auto last = model->lastPatternAccess();
        if (!last)
            continue;

        cv::Mat trainImg = cfg.m_rawImage.clone();
        if (!last->learnPattern(trainImg)) {
            LOG_USER_ERR << "Localization matching failed: learnPattern failed for"
                         << QString::fromStdWString(cfg.m_patternName);
        }
    }

    if (model->isEmpty()) {
        LOG_USER_ERR << "Localization matching failed: matching model is empty.";
        return false;
    }
    return true;
}

mtc::MatchResult LocalizationPipeline::runMatchOn(mtc::ImageMatcher &matcher,
                                                  const CameraWorkspace &workspace,
                                                  const cv::Mat &image,
                                                  const mtc::IRobotPickingChecker *pickingChecker,
                                                  int maxObjects) const {
    mtc::MatchResult emptyResult;
    if (image.empty()) {
        LOG_USER_ERR << "Localization matching failed: source image is empty.";
        return emptyResult;
    }

    if (workspace.useWorkspace) {
        matcher.setMatchingROI(workspace.roi.tl(), workspace.roi.br());
    }
    if (workspace.useConditionWorkspace) {
        matcher.setMatchingConditionROI(workspace.conditionRoi.tl(), workspace.conditionRoi.br());
    }
    matcher.setRobotPickingChecker(pickingChecker);
    matcher.setImageSource(image.clone());
    matcher.matching(true, maxObjects, workspace.useWorkspace, workspace.useConditionWorkspace);
    return matcher.match_result;
}

mtc::MatchResult LocalizationPipeline::runMatchCommision(std::shared_ptr<mtc::MatchGroup> group,
                                                         const CameraWorkspace &workspace,
                                                         const cv::Mat &image,
                                                         const mtc::IRobotPickingChecker *pickingChecker) const {
    mtc::ImageMatcher matcher;
    if (!loadModel(matcher, group))
        return mtc::MatchResult{};
    return runMatchOn(matcher, workspace, image, pickingChecker, -1);
}

} // namespace vc::model
