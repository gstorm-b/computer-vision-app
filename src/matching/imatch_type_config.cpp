#include "imatch_type_config.h"
#include "edge_match_config.h"

namespace mtc {

/// Constructs a default-initialised config for `t`; currently only EdgeBased is
/// implemented, Correlation and any unhandled type return nullptr.
std::unique_ptr<IMatchTypeConfig> IMatchTypeConfig::createDefault(MatchingType t) {
    switch (t) {
    case MatchingType::EdgeBased:
        return std::make_unique<EdgeMatchConfig>();
    case MatchingType::Correlation:
        return nullptr;  // not yet implemented
    }
    return nullptr;
}

} // namespace mtc
