#pragma once

#include <map>
#include <optional>
#include <string>

/// Posture/branch classification of a serial arm's joint solution (e.g. shoulder/elbow/wrist
/// sign convention) produced and consumed by PostureResolver implementations.
namespace RobotKinematics {

/// Generic posture/branch descriptor for serial arms. The shoulder/elbow/wrist branch
/// values are conventionally -1 or +1; vendor-specific branches are kept separately.
/// Posture names (lefty/righty, above/below, flip/non-flip) are NOT hard-coded here;
/// they live in preset/posture metadata and are mapped by a PostureResolver (Phase 4).
struct ArmPosture {
    std::optional<int> shoulder;              ///< Shoulder branch sign (-1/+1), unset if not classified.
    std::optional<int> elbow;                 ///< Elbow branch sign (-1/+1), unset if not classified.
    std::optional<int> wrist;                 ///< Wrist branch sign (-1/+1), unset if not classified.
    std::map<std::string, int> vendorSpecific;  ///< Additional vendor-specific branch values keyed by name.

    /// True when none of shoulder/elbow/wrist are set and vendorSpecific is empty.
    bool isEmpty() const
    {
        return !shoulder.has_value() && !elbow.has_value() && !wrist.has_value()
               && vendorSpecific.empty();
    }
};

} // namespace RobotKinematics
