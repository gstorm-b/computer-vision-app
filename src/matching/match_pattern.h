#ifndef MATCH_PATTERN_H
#define MATCH_PATTERN_H

#include <vector>
#include <opencv2/core.hpp>
#include "match_pattern_config.h"
#include "match_pattern_layer.h"
#include "manager_result.h"

/// Vision/matching module: MatchPattern, a single learned template plus its per-pattern
/// configuration and learned pyramid/edge data.
namespace mtc {

class MatchGroup;
class EdgeMatchConfig;

/// A single learned template.
///
/// Carries a MatchPatternConfig (identity + per-pattern search params) plus the
/// data computed by learnPattern(): image pyramids, edge point arrays, contour
/// masks, etc.  The algorithm (Edge-Based) config is shared at the group level
/// (MatchGroupConfig::typeConfig) and fetched via the parent group.
///
/// To change per-pattern settings after construction, retrieve a copy of the
/// config, modify the relevant fields, then call setConfig().
class MatchPattern {
public:
    /// Default-constructs with an empty config and no parent group.
    MatchPattern();
    /// Constructs with a copy of `config` and a back-pointer to the owning `parent` group
    /// (used to fetch the shared Edge-Based algorithm config).
    MatchPattern(const MatchPatternConfig& config, MatchGroup* parent);

    std::wstring name()   const { return m_config.m_patternName; }  ///< Returns the pattern's display name.
    int          number() const { return m_config.m_patternIndex; } ///< Returns the pattern's index within its group.
    cv::Mat      image()        { return m_config.m_rawImage.clone(); } ///< Returns a clone of the stored raw training image.

    const MatchPatternConfig& config() const { return m_config; } ///< Returns the current per-pattern config by const reference.

    // ── Config update API ─────────────────────────────────────────────────
    /// Replaces the per-pattern config with a copy of `cfg`.
    void setConfig(MatchPatternConfig& cfg);
    /// Returns a copy of the current per-pattern config.
    MatchPatternConfig        patternConfig()    const;
    /// Returns a pointer to the internal config (no copy); valid as long as this
    /// MatchPattern is alive and its config is not reassigned.
    const MatchPatternConfig* patternConfigPtr() const;

    // ── Image API ─────────────────────────────────────────────────────────
    /// Loads the raw training image from `path` (wide-character filesystem path) and
    /// stores it via setRawImage(cv::Mat&); does nothing if `path` is empty.
    void setRawImage(std::wstring path);
    /// Stores `imgPattern` as the raw training image, converting BGR/BGRA input to
    /// grayscale first; marks the pattern as not-yet-learned. Does nothing if empty.
    void setRawImage(cv::Mat& imgPattern);
    /// Returns a clone of the stored raw training image.
    cv::Mat      getRawImage()              const;
    /// Returns the size descriptor of the stored raw training image.
    cv::MatSize  getRawImageSize();
    /// Returns a BGR copy of the working image with the pick position/angle drawn as axes;
    /// returns an empty Mat if no image has been set.
    cv::Mat      getImageWithPickPosition();
    /// Returns the Canny edge image of the working image using automatic median-based
    /// thresholds (kernel size taken from the group's edge config, default 3); returns an
    /// empty Mat if no image has been set.
    cv::Mat      getImageWithCannyThreshold();

    // ── Learn API ─────────────────────────────────────────────────────────
    /// Loads the training image from `path` then learns the pattern; returns false if the
    /// image could not be loaded or learning fails.
    bool learnPattern(std::wstring path);
    /// Sets `imgPattern` as the training image then learns the pattern.
    bool learnPattern(cv::Mat& imgPattern);
    /// Builds the image pyramid and, for each level, the Canny edge map, gradient
    /// magnitude/angle, and resampled contour-derived pattern points used by the
    /// Edge-Based matcher; also selects the largest sub-98%-area contour (via Otsu/fixed
    /// threshold) for the low-workpiece area pre-filter. Requires the parent group's edge
    /// config to be available (only EdgeBased is implemented); returns false otherwise or
    /// if the training image is empty.
    /// @return true on success; sets isPatternLearned() to true
    bool learnPattern();

    // ── State queries ─────────────────────────────────────────────────────
    bool isImageEmpty()     const { return m_config.m_rawImage.empty(); } ///< True when no raw training image has been set.
    bool isPatternLearned() const { return m_hasPatternLearned; } ///< True once learnPattern() has completed successfully.

    // ── Learned-data accessors (used by ImageMatcher) ─────────────────────
    const std::vector<cv::Mat>*         getPyramid()  const { return &m_pyramids; } ///< Returns the image pyramid built by learnPattern().
    const std::vector<PatternLayer>*    getPatterns() const { return &m_patterns; } ///< Returns the per-level learned pattern data built by learnPattern().
    cv::Mat getPatternImage() const { return m_config.m_rawImage.clone(); } ///< Returns a clone of the raw training image (same source as getRawImage()).
    int     getTopLayer()     const { return m_topLayer; } ///< Returns the pyramid depth computed by findTopLayer().
    int     borderColor()     const { return m_borderColor; } ///< Returns the estimated border/background intensity (0 or 255).

    int getModelContoursSelectedIndex()    const { return m_contoursSelectIndex; } ///< Returns the index of the selected low-workpiece contour.
    int getModelContoursSelectedArea()     const { return m_contoursSelectArea; } ///< Returns the area (px^2) of the selected contour.
    int getModelContoursSelectedRectArea() const { return m_contoursSelectRectArea; } ///< Returns the area (px^2) of the selected contour's min-area rotated rect.

    cv::Mat getMaskImage()   const { return m_maskImage.clone(); } ///< Returns a clone of the binary threshold mask computed during learnPattern().
    cv::Mat getPatternMask() const { return m_maskPatternPoint.clone(); } ///< Returns a clone of the mask with the top-layer contour(s) drawn on it.

    void        setImageSaveName(std::string name) { m_iamgeSaveName = name; } ///< Sets the display/save name associated with this pattern's preview image.
    std::string imageSaveName()                    { return m_iamgeSaveName; } ///< Returns the display/save name set via setImageSaveName().

private:
    /// Clears the previously learned per-level pattern data (m_patterns).
    void clearPatternData();
    /// Computes m_minReduceArea from the group's edge config's minReduceLength (default 32
    /// if no edge config is available) and sets m_topLayer to the number of area-halving
    /// (÷4) steps needed for the working image's area to drop below m_minReduceArea.
    void findTopLayer();

    /// Edge algorithm config is shared at the group level — fetches it from the parent
    /// group (nullptr if no parent or the group is not EdgeBased).
    const EdgeMatchConfig* groupEdgeConfig() const;

    friend class MatchGroup;
    /// Replaces the per-pattern config with a copy of `cfg`; always succeeds.
    ManagerResult setConfig(const MatchPatternConfig& cfg);
    /// Sets the pattern's display name; always succeeds.
    ManagerResult setName(const std::wstring& name);
    /// Sets the pattern's index within its group; always succeeds.
    ManagerResult setNumber(int number);
    /// Sets both the stored raw training image and the working image to clones of `image`
    /// (bypasses the grayscale-conversion path used by setRawImage()).
    void          setImage(const cv::Mat& image);

private:
    MatchPatternConfig m_config;              ///< Per-pattern identity, search params, and raw training image.
    MatchGroup*        m_parentGroup{nullptr}; ///< Back-pointer to the owning group; used to fetch the shared Edge-Based config.

    cv::Mat m_image;              ///< Working (grayscale) image set by setRawImage()/setImage(); source for learn/preview operations.
    cv::Mat m_maskImage;          ///< Binary threshold mask computed during learnPattern(), used for the low-workpiece area pre-filter.
    cv::Mat m_maskPatternPoint;   ///< Mask with the top pyramid layer's simplified contour(s) drawn onto it.
    bool    m_hasPatternLearned{false}; ///< True once learnPattern() has completed successfully.
    int     m_minReduceArea{0};   ///< Minimum pyramid-layer area (px^2) computed by findTopLayer().
    int     m_topLayer{0};        ///< Pyramid depth computed by findTopLayer().
    int     m_borderColor{0};     ///< Estimated border/background intensity (0 or 255) from the mean pixel value.

    std::vector<cv::Mat>       m_pyramids; ///< Image pyramid built by learnPattern() (one entry per level, up to m_topLayer).
    std::vector<PatternLayer>  m_patterns; ///< Per-level learned edge template data produced by learnPattern().

    int             m_contoursSelectIndex{0};     ///< Index of the selected low-workpiece contour in the threshold-image contour list.
    int             m_contoursSelectArea{0};       ///< Area (px^2) of the selected contour.
    cv::RotatedRect m_contoursSelectRect;          ///< Minimum-area rotated rect enclosing the selected contour.
    int             m_contoursSelectRectArea{0};   ///< Area (px^2) of m_contoursSelectRect.

    std::string m_iamgeSaveName;  ///< Display/save name associated with this pattern's preview image.
};

} // namespace mtc
#endif // MATCH_PATTERN_H
