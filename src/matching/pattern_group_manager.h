#ifndef PATTERN_GROUP_MANAGER_H
#define PATTERN_GROUP_MANAGER_H

#include <QObject>
#include <QList>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "match_group.h"
#include "manager_result.h"

/// Machine-vision pattern-matching model: match groups/patterns
/// (MatchGroup, MatchPattern) and their owning manager (PatternGroupManager).
namespace mtc {

/// Owns the full library of MatchGroup instances (and, transitively, their
/// MatchPattern children) for a task: validates group/pattern mutations,
/// emits Qt change signals for every successful mutation (see
/// design_rules.md 4.1), and (de)serialises the whole library to/from JSON.
class PatternGroupManager : public QObject {
    Q_OBJECT

public:
    /// Constructs an empty manager with no groups.
    explicit PatternGroupManager(QObject *parent = nullptr);
    /// Destroys the manager; each held MatchGroup is released when its
    /// shared_ptr in m_groups goes out of scope.
    ~PatternGroupManager();

    /// Returns the current list of groups, in internal (not necessarily
    /// numeric) order.
    QList<std::shared_ptr<MatchGroup>> groups() const;
    /// Finds the group whose name matches `name` exactly.
    /// @return the matching group, or nullptr if none exists.
    std::shared_ptr<MatchGroup> findGroupByName(const QString &name) const;
    /// Finds the group whose number matches `number`.
    /// @return the matching group, or nullptr if none exists.
    std::shared_ptr<MatchGroup> findGroupByNumber(int number) const;

    /// @return true if a group named `name` exists.
    bool containsGroupName(const QString &name) const;
    /// @return true if a group numbered `number` exists.
    bool containsGroupNumber(int number) const;
    /// Returns the number of groups currently held.
    int groupCount() const;

    /// Create a new group. Fails if name or number already exists.
    ManagerResult addGroup(const MatchGroupConfig &config);

    /// Remove the group with the given name (and all its patterns).
    ManagerResult removeGroup(const QString &groupName);

    /// Convenience: remove by number.
    ManagerResult removeGroupByNumber(int number);

    /// Replace the full config of the group identified by currentName.
    ManagerResult setGroupConfig(const QString &currentName,
                                 const MatchGroupConfig &newConfig);

    /// Renames the group `currentName` to `newName`. No-op success if the
    /// names are identical; fails if `newName` is blank or already used by
    /// another group.
    ManagerResult renameGroup  (const QString &currentName, const QString &newName);
    /// Reassigns the number of group `groupName` to `newNumber`. No-op
    /// success if unchanged; fails if `newNumber` is already used by
    /// another group.
    ManagerResult renumberGroup(const QString &groupName, int newNumber);

    /// Adds a new pattern with `config` to the group `groupName`
    /// (delegates uniqueness validation to MatchGroup::addPattern).
    ManagerResult addPattern   (const QString &groupName, const MatchPatternConfig &config);
    /// Removes the pattern named `patternName` from group `groupName`.
    ManagerResult removePattern(const QString &groupName, const QString &patternName);

    /// Replaces the full config of the pattern currently named
    /// `currentPatternName` inside group `groupName`.
    ManagerResult setPatternConfig  (const QString &groupName,
                                   const QString &currentPatternName,
                                   const MatchPatternConfig &newConfig);
    /// Renames pattern `currentName` to `newName` inside group `groupName`.
    ManagerResult renamePattern     (const QString &groupName,
                                const QString &currentName,
                                const QString &newName);
    /// Reassigns the number of pattern `patternName` inside group
    /// `groupName` to `newNumber`.
    ManagerResult renumberPattern   (const QString &groupName,
                                  const QString &patternName,
                                  int newNumber);
    /// Sets the training image of pattern `patternName` inside group
    /// `groupName`.
    ManagerResult setPatternImage   (const QString &groupName,
                                  const QString &patternName,
                                  const cv::Mat &image);

    // =========================================================================
    //  Bulk operations
    // =========================================================================

    /// Sort all groups in-place by number ascending and re-emit groupsReordered.
    void sortGroupsByNumber();

    /// Sort patterns inside a single group by number ascending.
    ManagerResult sortPatternsByNumber(const QString &groupName);


    // ── Serialisation helpers
    /// Serialises every group (and its patterns) to the
    /// `{ "groups": [ {…, "patterns": […]} ] }` schema documented above
    /// groupToJson() in pattern_group_manager.cpp. Training images are not
    /// included; they travel separately through the project_images BLOB table.
    QJsonObject toJson() const;
    /// Replaces the entire library with the groups/patterns decoded from
    /// `obj` (schema as produced by toJson()). Existing groups are removed
    /// first via removeGroupByNumber().
    /// @return true if every group and pattern loaded without error; on
    /// partial failure the load continues and errors are logged via LOG_DEV_ERR.
    bool fromJson(const QJsonObject &obj);

signals:
    // ── Group-level ──────────────────────────────────────────────────────────
    /// Emitted after a group is successfully added.
    void groupAdded     (std::shared_ptr<MatchGroup> group);
    /// Emitted after a group is removed, carrying a copy of its config as it
    /// was just before removal.
    void groupRemoved   (const MatchGroupConfig &removedConfig);
    /// Emitted after any successful group mutation; `field` names the
    /// changed aspect ("*" for a full config replace, "name", "number", …).
    void groupChanged   (std::shared_ptr<MatchGroup> group, const QString &field);
    /// Emitted after sortGroupsByNumber() with the new group order.
    void groupsReordered(const QList<std::shared_ptr<MatchGroup>> &newOrder);

    // ── Pattern-level (bubbled from PatternGroup) ─────────────────────────────
    /// Emitted after a pattern is successfully added to `group`.
    void patternAdded   (MatchGroup *group, MatchPattern *pattern);
    /// Declared for symmetry with patternAdded/patternChanged, carrying a
    /// copy of the removed pattern's config.
    /// @note removePattern() currently does not emit this signal.
    void patternRemoved (MatchGroup *group, const MatchPatternConfig &removedConfig);
    /// Emitted after any successful pattern mutation; `field` names the
    /// changed aspect ("*", "name", "number", "image", …).
    void patternChanged (MatchGroup *group, MatchPattern *pattern, const QString &field);

private:
    /// Validates `cfg` before it is used to add or replace a group: rejects
    /// an empty name and rejects a name/number collision with any existing
    /// group other than `excludeName` (used when renaming/reconfiguring in
    /// place).
    ManagerResult validateGroupConfig(const MatchGroupConfig &cfg,
                                      const QString &excludeName = {}) const;

private:

    QList<std::shared_ptr<MatchGroup>> m_groups;  ///< All groups owned by this manager.
};

}

#endif // PATTERN_GROUP_MANAGER_H
