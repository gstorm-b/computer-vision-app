#ifndef MATCHING_TYPES_H
#define MATCHING_TYPES_H

#include <cstdint>

/// Vision/matching module: MatchingType, the algorithm-family discriminator
/// shared by pattern/group configuration.
namespace mtc {

// ---------------------------------------------------------------------------
// MatchingType — discriminates which algorithm family is in use.
//
// Adding a new algorithm type:
//   1. Add a value here.
//   2. Create a concrete IMatchTypeConfig subclass.
//   3. Add a branch in IMatchTypeConfig::createDefault().
//   4. Add a branch in MatchConfigPropertyAdapter::buildTypeGroup().
// ---------------------------------------------------------------------------
/// Discriminates which matching-algorithm family a group/pattern uses.
enum class MatchingType : uint8_t {
    EdgeBased   = 0,  ///< Gradient-direction edge template matching (SIMD-accelerated)
    Correlation = 1,  ///< Normalised cross-correlation (reserved — not yet implemented)
};

/// Returns the human-readable name of `t` ("Edge-Based", "Correlation", or "Unknown").
inline const char* matchingTypeName(MatchingType t) noexcept {
    switch (t) {
    case MatchingType::EdgeBased:   return "Edge-Based";
    case MatchingType::Correlation: return "Correlation";
    }
    return "Unknown";
}

} // namespace mtc
#endif // MATCHING_TYPES_H
