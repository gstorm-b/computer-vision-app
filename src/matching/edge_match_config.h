#ifndef EDGE_MATCH_CONFIG_H
#define EDGE_MATCH_CONFIG_H

#include "imatch_type_config.h"

/// Vision/matching module: EdgeMatchConfig, the tunable parameters for gradient-direction
/// edge template matching used by ImageMatcher.
namespace mtc {

/// All tunable parameters for gradient-direction edge template matching (the
/// SIMD-accelerated algorithm in ImageMatcher).
///
/// Covers both the template-learning phase (Canny / pyramid) and the
/// runtime search phase (greediness, sub-pixel, layer control).
///
/// Adding a new EdgeBased setting:
///   1. Add a field here with a sensible default value.
///   2. Add one entry to kEdgeSpecs[] in match_config_property_adapter.cpp.
///   3. Access the field in match_pattern.cpp or image_matcher.cpp.
///   No other files need to change.
class EdgeMatchConfig final : public IMatchTypeConfig {
public:
    /// Default-constructs the config with the default parameter values declared below.
    EdgeMatchConfig() = default;

    /// Returns MatchingType::EdgeBased.
    MatchingType type() const noexcept override { return MatchingType::EdgeBased; }

    /// Returns a heap-allocated deep copy of this config.
    /// @return a new EdgeMatchConfig with the same field values
    std::unique_ptr<IMatchTypeConfig> clone() const override {
        return std::make_unique<EdgeMatchConfig>(*this);
    }

    // ── Canny / Sobel preprocessing ───────────────────────────────────────
    double threshLower  = 100.0;  ///< Canny lower hysteresis threshold
    double threshUpper  = 200.0;  ///< Canny upper hysteresis threshold
    int    kernelSize   = 3;      ///< Sobel / Canny aperture (1, 3, 5, 7)
    int    blurWidth    = 5;      ///< Gaussian pre-blur kernel width (px)
    int    blurHeight   = 5;      ///< Gaussian pre-blur kernel height (px)

    // ── Matching behaviour ────────────────────────────────────────────────
    double greediness        = 0.0;   ///< Early-exit accumulation ratio (0 = off)
    double lowWorkpieceRatio = 1.5;   ///< Ratio used to flag low / partially-visible workpieces

    // ── Pattern learning ──────────────────────────────────────────────────
    int  minReduceLength       = 32;   ///< Pyramid descent stops when side < this (px)
    int  tSamples              = 3;    ///< Contour point spacing during learn
    bool invertBinaryThreshold = true; ///< Invert binary threshold for PCA contour

    // ── Binarization (source pre-filter & PCA contour) ────────────────────
    int  binaryThreshold       = -1;   ///< -1 => auto (Otsu); 0..255 => fixed threshold
    int  binaryMaxValue        = 255;  ///< cv::threshold maxval — intensity assigned above threshold (0..255)

    // ── Fine-search options ───────────────────────────────────────────────
    bool subPixelEstimation = false;   ///< Quadratic sub-pixel position refinement
    bool stopAtLayer1       = false;   ///< Stop pyramid descent at layer 1 (faster)
};

} // namespace mtc
#endif // EDGE_MATCH_CONFIG_H
