#ifndef UTILS_BLOCK_MAX_H
#define UTILS_BLOCK_MAX_H

#include <vector>
#include <opencv2/core.hpp>

/**
 * @file utils_block_max.h
 * @brief MatchBlock/BlockMax — block-partitioned max-value search helper used to enumerate
 *        multiple local maxima in a score/response map.
 */

namespace mtc {

/**
 * @class MatchBlock
 * @brief One rectangular block of a score map, together with its cached maximum
 *        value and the location of that maximum.
 */
class MatchBlock {
public:
    /// Default-constructs a block with an empty rect and _maxValue of 0.0.
    MatchBlock();
    /**
     * @brief Constructs a block covering `rect`, with the given cached max value and location.
     * @param[in] rect     region of the source map this block covers
     * @param[in] val_max  cached maximum value within `rect`
     * @param[in] ptMaxLoc location (in the full source map) of `val_max`
     */
    MatchBlock(cv::Rect rect, double val_max, cv::Point ptMaxLoc);
    /// Destructor (no owned resources to release).
    ~MatchBlock();

public:
    cv::Rect _rect;             ///< Region of the source map this block covers.
    double _maxValue;           ///< Cached maximum value within `_rect`.
    cv::Point _pointMaxLoc;     ///< Location (in the full source map) of `_maxValue`.
};

/**
 * @class BlockMax
 * @brief Partitions a score map into a grid of blocks (sized from the pattern size)
 *        and caches each block's local maximum, so repeated searches for the next-best
 *        match only need to rescan the blocks touched by a previously matched region
 *        (via UpdateMax) instead of the whole map.
 */
class BlockMax {
public:
    /// Default-constructs an empty BlockMax with no blocks and no source map.
    BlockMax();
    /**
     * @brief Partitions `matSrc` into blocks sized 2x `sizePattern` (plus right/bottom residue
     *        blocks when the map doesn't divide evenly) and computes each block's local maximum
     *        via cv::minMaxLoc. Leaves `_vecBlocks` empty if `matSrc` is smaller than one full
     *        block in either dimension.
     * @param[in] matSrc      score/response map to partition
     * @param[in] sizePattern half the block side length; each block is 2x this size
     */
    BlockMax(cv::Mat matSrc, cv::Size sizePattern);
    /// Destructor (no owned resources to release).
    ~BlockMax();

    /**
     * @brief Recomputes the cached maximum (via cv::minMaxLoc) for every block that
     *        intersects `rectIgnore`; used to invalidate blocks after a region has been
     *        consumed/suppressed by a match.
     * @param[in] rectIgnore region whose overlapping blocks need their max recomputed
     */
    void UpdateMax(cv::Rect rectIgnore);
    /**
     * @brief Returns the overall maximum value and its location across all blocks.
     *        Falls back to a direct cv::minMaxLoc over `_matSrc` when there are no blocks.
     * @param[out] dMax     overall maximum value
     * @param[out] ptMaxLoc location of the maximum
     */
    void GetMaxValueLoc(double &dMax, cv::Point& ptMaxLoc);

private:
    std::vector<MatchBlock> _vecBlocks;    ///< Blocks partitioning `_matSrc`, each with its cached local maximum.
    cv::Mat _matSrc;                       ///< Source score/response map being searched.
};

} // namespace mtc

#endif // UTILS_BLOCK_MAX_H
