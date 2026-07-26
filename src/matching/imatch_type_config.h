#ifndef IMATCH_TYPE_CONFIG_H
#define IMATCH_TYPE_CONFIG_H

#include "matching_types.h"
#include <memory>

/// Vision/matching module: IMatchTypeConfig, the abstract base for per-algorithm
/// runtime/learn parameter configs.
namespace mtc {

/// Abstract base for per-algorithm runtime/learn parameters.
///
/// Each algorithm family (EdgeBased, Correlation, …) provides exactly one
/// concrete subclass.  The interface is kept Qt-free so the core matching
/// library has no UI dependency.  Property-browser integration lives entirely
/// in MatchConfigPropertyAdapter.
class IMatchTypeConfig {
public:
    virtual ~IMatchTypeConfig() = default;

    /// Returns the matching algorithm family this config belongs to.
    virtual MatchingType type() const noexcept = 0;

    /// Deep-copy factory — used when MatchPatternConfig is copied by value.
    /// @return a new heap-allocated copy of this config
    virtual std::unique_ptr<IMatchTypeConfig> clone() const = 0;

    /// Factory: constructs a default-initialised config for a given type.
    /// @return the default config for `t`, or nullptr for types not yet implemented
    static std::unique_ptr<IMatchTypeConfig> createDefault(MatchingType t);
};

} // namespace mtc
#endif // IMATCH_TYPE_CONFIG_H
