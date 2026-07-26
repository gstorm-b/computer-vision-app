#ifndef MATCH_GROUP_H
#define MATCH_GROUP_H

#include "match_pattern.h"
#include "manager_result.h"
#include "edge_match_config.h"

#include <memory>

/// Vision/matching module: MatchGroupConfig and MatchGroup, the per-group
/// pattern container and its shared algorithm config.
namespace mtc {

class PatternGroupManager;

/// MatchGroupConfig — per-group configuration.
///
/// Holds group identity and the algorithm-specific config (`typeConfig`,
/// default EdgeMatchConfig) that is SHARED by every pattern in the group.
/// Typed access helpers (edgeConfig / configAs<T>) return nullptr when the
/// current matching type does not match.
///
/// Picking / collision-box geometry is per-pattern — it lives on
/// MatchPatternConfig, not here (collision is an Edge-Based, per-pattern
/// concern).
///
/// Copy semantics: deep-copies `typeConfig` via clone() so each group owns an
/// independent copy of its algorithm config.
class MatchGroupConfig {
public:
    /// Default-constructs group identity fields and a default EdgeMatchConfig typeConfig.
    MatchGroupConfig();
    /// Deep-copies `other`, cloning `typeConfig` so this instance owns an independent algorithm config.
    MatchGroupConfig(const MatchGroupConfig &other);
    /// Deep-copies `other`, cloning `typeConfig` so this instance owns an independent algorithm config.
    MatchGroupConfig &operator=(const MatchGroupConfig &other);
    MatchGroupConfig(MatchGroupConfig &&)            = default;
    MatchGroupConfig &operator=(MatchGroupConfig &&) = default;

    std::wstring m_groupName;              ///< Display name of the group.
    int m_groupIndex{0};                   ///< Numeric identifier of the group.
    bool m_sortByAngle{false};              ///< When true, ImageMatcher::sortMatchedObjects orders matches within a pick-priority bucket by smallest angular error to m_sortConditionAngle instead of by score.
    double m_sortConditionAngle{0.0};       ///< Target angle (degrees) used for the angle-based sort when m_sortByAngle is set.

    /// Algorithm-specific config, shared by all patterns in the group.
    /// Default-constructed as EdgeMatchConfig. Replace via setMatchingType().
    std::shared_ptr<IMatchTypeConfig> typeConfig;

    /// Serializable pattern-config list carried on the group config (copied
    /// verbatim by the copy ctor/assignment). MatchGroup's live patterns are
    /// stored separately as shared_ptr<MatchPattern>; see MatchGroup::patterns().
    std::vector<MatchPatternConfig> m_patterns;

    // ── Typed helpers ─────────────────────────────────────────────────────
    /// Returns the current algorithm type, or EdgeBased if typeConfig is null.
    MatchingType matchingType() const noexcept {
        return typeConfig ? typeConfig->type() : MatchingType::EdgeBased;
    }

    /// Switches to a different algorithm type; resets typeConfig to a fresh
    /// default instance for `t` (no-op if already of that type).
    void setMatchingType(MatchingType t) {
        if (!typeConfig || typeConfig->type() != t)
            typeConfig = IMatchTypeConfig::createDefault(t);
    }

    /// Convenience cast — returns nullptr if current type != EdgeBased.
    EdgeMatchConfig*       edgeConfig()       noexcept { return dynamic_cast<EdgeMatchConfig*>(typeConfig.get()); }
    /// Convenience cast — returns nullptr if current type != EdgeBased.
    const EdgeMatchConfig* edgeConfig() const noexcept { return dynamic_cast<const EdgeMatchConfig*>(typeConfig.get()); }

    /// Generic typed cast for future algorithm types; returns nullptr if typeConfig is not (or not currently) a T.
    template<typename T>       T* configAs()       noexcept { return dynamic_cast<T*>(typeConfig.get()); }
    /// Generic typed cast for future algorithm types; returns nullptr if typeConfig is not (or not currently) a T.
    template<typename T> const T* configAs() const noexcept { return dynamic_cast<const T*>(typeConfig.get()); }
};

/// Owns one named/numbered group of patterns plus their shared MatchGroupConfig:
/// pattern CRUD (add/remove/rename/renumber/setImage), lookup, and static
/// group/pattern index-range validation. Instances not created via
/// PatternGroupManager have no manager and cannot use setConfig/setName/setNumber.
class MatchGroup {
public:
    /// Default-constructs an empty, unnamed group with no manager.
    MatchGroup();
    /// Constructs an empty group with the given name/index and no manager.
    MatchGroup(std::wstring name, int index);
    ~MatchGroup();

    /// Copies this group's config (identity + shared algorithm config) onto `group`.
    void cloneConfigTo(MatchGroup &group);
    /// Returns the group's current config (identity + shared algorithm config).
    const MatchGroupConfig &config() const { return m_config; }
    /// Returns the group's display name.
    const std::wstring &name() const { return m_config.m_groupName; }
    /// Returns the group's numeric index.
    int number() const { return m_config.m_groupIndex; }

    /// Returns the live pattern list (shared ownership; a copy of the internal vector of shared_ptr).
    std::vector<std::shared_ptr<MatchPattern>> patterns() const;
    /// Returns the pattern with the given name, or nullptr if none matches.
    MatchPattern* findPatternByName(const std::wstring &name) const;
    /// Returns the pattern with the given number, or nullptr if none matches.
    MatchPattern* findPatternByNumber(int number) const;

    /// Returns whether a pattern with this name exists in the group.
    bool containsPatternName(const std::wstring &name) const;
    /// Returns whether a pattern with this number exists in the group.
    bool containsPatternNumber(int number) const;
    /// Returns the number of patterns currently in the group.
    int patternCount() const;
    /// Returns whether the group has no patterns.
    bool isEmpty() const;

    // ── Pattern mutations ─────────────────────────────────────────────────────
    /// Create a new Pattern with the given config.
    /// Fails if name or number already exists in this group.
    ManagerResult addPattern(const MatchPatternConfig &config);

    /// Remove by name.
    ManagerResult removePattern(const std::wstring &patternName);

    /// Convenience: remove by number.
    ManagerResult removePatternByNumber(int number);

    /// Returns the pattern most recently added via addPattern(), or nullptr if none has been added.
    std::shared_ptr<MatchPattern> lastPatternAccess() const;

    /// Replace the entire config of a pattern identified by its current name.
    /// Validates that the new name/number do not collide with siblings.
    ManagerResult setPatternConfig(const std::wstring &currentName,
                                   const MatchPatternConfig &newConfig);

    /// Targeted field setters — avoids shipping the whole config just to rename.
    /// Renames a pattern from `currentName` to `newName` (no-op/success if unchanged).
    /// @note As implemented the guard is `!containsPatternName(newName)`: it fails
    ///       (with an "already exists" message) when `newName` is NOT already in
    ///       use, and proceeds — even into a colliding name — when it IS already
    ///       in use. The check's polarity does not match its failure message.
    ManagerResult renamePattern  (const std::wstring &currentName, const std::wstring &newName);
    /// Changes a pattern's number; fails if `newNumber` is already used by another pattern in the group.
    ManagerResult renumberPattern(const std::wstring &patternName, int newNumber);
    /// Replaces a pattern's reference/template image.
    ManagerResult setPatternImage(const std::wstring &patternName, const cv::Mat &image);

    /// Returns whether `index` falls within [m_min_index_range, m_max_index_range].
    static bool validateIndexRange(int index);
    /// Sets the valid group-index range to [min, max]. Rejects (returns false,
    /// leaves the range unchanged) if min < 0 or max <= min.
    static bool setIndexRange(int min, int max);

    /// Returns the current maximum valid group index.
    static int getMaxGroupRange();
    /// Returns the current minimum valid group index.
    static int getMinGroupRange();
    /// Returns the maximum valid pattern index (fixed; not adjustable via a setter).
    static int getMaxPatternRange();
    /// Returns the minimum valid pattern index (fixed; not adjustable via a setter).
    static int getMinPatternRange();

    static int m_min_index_range;    ///< Minimum valid group index, adjustable via setIndexRange().
    static int m_max_index_range;    ///< Maximum valid group index, adjustable via setIndexRange().
    static int m_min_pattern_range;  ///< Minimum valid pattern index (fixed at construction time).
    static int m_max_pattern_range;  ///< Maximum valid pattern index (fixed at construction time).

private:
    friend class PatternGroupManager;
    /// Constructs a group owned by `parent`, copying `config`. Only PatternGroupManager may call this.
    explicit MatchGroup(const MatchGroupConfig &config,
                          PatternGroupManager *parent);

    /// Called only by PatternGroupManager after uniqueness check at manager level.
    ManagerResult setConfig(const MatchGroupConfig &cfg);
    /// Called only by PatternGroupManager after uniqueness check at manager level. No-op if `name` is unchanged.
    ManagerResult setName(const std::wstring &name);
    /// Called only by PatternGroupManager after uniqueness check at manager level. No-op if `number` is unchanged.
    ManagerResult setNumber(int number);

    // ── Helpers ───────────────────────────────────────────────────────────────
    /// Validates a candidate pattern config against the group's existing patterns:
    /// fails if the name is empty, or if another pattern (other than `excludeName`)
    /// already has the same name or number.
    /// @param cfg candidate pattern config to validate
    /// @param excludeName pattern name to skip during the collision check (used when validating a rename/edit of that same pattern)
    ManagerResult validatePatternConfig(const MatchPatternConfig &cfg,
                                        const std::wstring &excludeName = {}) const;

private:
    PatternGroupManager *m_manager;      ///< Non-owning back-reference to the owning manager (nullptr if not manager-created).
    MatchGroupConfig m_config;           ///< This group's identity and shared algorithm config.
    std::vector<std::shared_ptr<MatchPattern>> m_patterns; ///< Live patterns registered in this group.
    std::shared_ptr<MatchPattern> m_lastPatternAccess;      ///< Pattern most recently added via addPattern().
};

}

#endif // MATCH_GROUP_H
