#include "pattern_group_manager.h"
#include "core/setting_keys.h"
#include "vision_utils.h"

#include "match_pattern.h"
#include "match_pattern_config.h"
#include "edge_match_config.h"
#include "matching_types.h"

#include "core/logger/app_logger.h"

namespace mtc {

// ============================================================================
//  JSON helpers — pattern-library serialisation
//
//  Local to this translation unit.  Schema (top-level shape produced by
//  PatternGroupManager::toJson):
//
//    {
//      "groups": [
//        { "name", "number",
//          "matchingType": "Edge-Based" | "Correlation",
//          "typeConfig":   { /* shared per-algorithm fields incl. lowWorkpieceRatio */ },
//          "patterns": [
//            { "name", "number",
//              "minScore", "angle", "toleranceAngle", "maxOverlap",
//              "pickPosition": { "x", "y" },
//              "pickingBoxSize":  { "w", "h" },
//              "pickingBoxDistance", "pickingBoxAngle",
//              "pickingOffset":   { "x", "y", "z" }
//            }, …
//          ]
//        }, …
//      ]
//    }
//
//  Training images (m_rawImage) are NOT in JSON — they travel through the
//  project_images BLOB table.
// ============================================================================

namespace {

// ── MatchingType ↔ string ────────────────────────────────────────────────────
// MatchingType is a plain enum class (no Q_ENUM), so we hand-roll the mapping
// to keep the JSON stable across future enum re-orderings.
/// Converts a MatchingType to its stable JSON string ("Edge-Based",
/// "Correlation") via matchingTypeName().
QString matchingTypeToString(MatchingType t) {
    return QString::fromLatin1(matchingTypeName(t));
}

/// Parses the JSON `"matchingType"` string back to a MatchingType.
/// @return `defaultValue` (EdgeBased unless overridden) if `s` matches
/// neither known literal.
MatchingType matchingTypeFromString(const QString &s,
                                    MatchingType defaultValue = MatchingType::EdgeBased) {
    if (s == QLatin1String("Edge-Based"))  return MatchingType::EdgeBased;
    if (s == QLatin1String("Correlation")) return MatchingType::Correlation;
    return defaultValue;
}

// ── EdgeMatchConfig ↔ JSON ───────────────────────────────────────────────────

/// Serialises every EdgeMatchConfig field to a flat JSON object.
QJsonObject edgeConfigToJson(const EdgeMatchConfig &c) {
    QJsonObject o;
    o["threshLower"]           = c.threshLower;
    o["threshUpper"]           = c.threshUpper;
    o["kernelSize"]            = c.kernelSize;
    o["blurWidth"]             = c.blurWidth;
    o["blurHeight"]            = c.blurHeight;
    o["greediness"]            = c.greediness;
    o["lowWorkpieceRatio"]     = c.lowWorkpieceRatio;
    o["minReduceLength"]       = c.minReduceLength;
    o["tSamples"]              = c.tSamples;
    o["invertBinaryThreshold"] = c.invertBinaryThreshold;
    o["binaryThreshold"]       = c.binaryThreshold;
    o["binaryMaxValue"]        = c.binaryMaxValue;
    o["subPixelEstimation"]    = c.subPixelEstimation;
    o["stopAtLayer1"]          = c.stopAtLayer1;
    return o;
}

/// Reads every EdgeMatchConfig field from `o` back into `c`, leaving each
/// field at its current value when the corresponding JSON key is missing.
void edgeConfigFromJson(const QJsonObject &o, EdgeMatchConfig &c) {
    c.threshLower           = o["threshLower"]          .toDouble(c.threshLower);
    c.threshUpper           = o["threshUpper"]          .toDouble(c.threshUpper);
    c.kernelSize            = o["kernelSize"]           .toInt   (c.kernelSize);
    c.blurWidth             = o["blurWidth"]            .toInt   (c.blurWidth);
    c.blurHeight            = o["blurHeight"]           .toInt   (c.blurHeight);
    c.greediness            = o["greediness"]           .toDouble(c.greediness);
    c.lowWorkpieceRatio     = o["lowWorkpieceRatio"]    .toDouble(c.lowWorkpieceRatio);
    c.minReduceLength       = o["minReduceLength"]      .toInt   (c.minReduceLength);
    c.tSamples              = o["tSamples"]             .toInt   (c.tSamples);
    c.invertBinaryThreshold = o["invertBinaryThreshold"].toBool  (c.invertBinaryThreshold);
    c.binaryThreshold       = o["binaryThreshold"]      .toInt   (c.binaryThreshold);
    c.binaryMaxValue        = o["binaryMaxValue"]       .toInt   (c.binaryMaxValue);
    c.subPixelEstimation    = o["subPixelEstimation"]   .toBool  (c.subPixelEstimation);
    c.stopAtLayer1          = o["stopAtLayer1"]         .toBool  (c.stopAtLayer1);
}

// ── MatchPatternConfig ↔ JSON ────────────────────────────────────────────────

/// Serialises identity, matching, and picking-geometry fields of a
/// MatchPatternConfig to JSON (pickPosition/pickingBoxSize/pickingOffset as
/// nested x/y[/z] objects).
QJsonObject patternConfigToJson(const MatchPatternConfig &c) {
    QJsonObject o;
    o["name"]           = QString::fromStdWString(c.m_patternName);
    o["number"]         = c.m_patternIndex;
    o["minScore"]       = c.m_minScore;
    o["angle"]          = c.m_angle;
    o["toleranceAngle"] = c.m_toleranceAngle;
    o["maxOverlap"]     = c.m_maxOverlap;

    QJsonObject pick;
    pick["x"] = c.m_pickPosition.x;
    pick["y"] = c.m_pickPosition.y;
    o["pickPosition"] = pick;

    o["pickingBoxSize"]     = QJsonObject{{ "w", c.m_pickingBoxSize.width  },
                                          { "h", c.m_pickingBoxSize.height }};
    o["pickingBoxDistance"] = c.m_pickingBoxDistance;
    o["pickingBoxAngle"]    = c.m_pickingBoxAngle;
    o["pickingOffset"]      = QJsonObject{{ "x", c.m_pickingOffset.x },
                                          { "y", c.m_pickingOffset.y },
                                          { "z", c.m_pickingOffset.z }};

    return o;
}

/// Reconstructs a MatchPatternConfig from JSON produced by
/// patternConfigToJson(); missing numeric fields default to 0/0.0.
MatchPatternConfig patternConfigFromJson(const QJsonObject &o) {
    MatchPatternConfig c;
    c.m_patternName    = o["name"].toString().toStdWString();
    c.m_patternIndex   = o["number"]        .toInt   (0);
    c.m_minScore       = o["minScore"]      .toDouble(c.m_minScore);
    c.m_angle          = o["angle"]         .toDouble(c.m_angle);
    c.m_toleranceAngle = o["toleranceAngle"].toDouble(c.m_toleranceAngle);
    c.m_maxOverlap     = o["maxOverlap"]    .toDouble(c.m_maxOverlap);

    const QJsonObject pick = o["pickPosition"].toObject();
    c.m_pickPosition.x = static_cast<float>(pick["x"].toDouble(0.0));
    c.m_pickPosition.y = static_cast<float>(pick["y"].toDouble(0.0));

    const QJsonObject sz = o["pickingBoxSize"].toObject();
    c.m_pickingBoxSize = cv::Size2f(static_cast<float>(sz["w"].toDouble(0.0)),
                                    static_cast<float>(sz["h"].toDouble(0.0)));
    c.m_pickingBoxDistance = o["pickingBoxDistance"].toDouble(0.0);
    c.m_pickingBoxAngle    = o["pickingBoxAngle"]   .toDouble(0.0);

    const QJsonObject off = o["pickingOffset"].toObject();
    c.m_pickingOffset = cv::Point3f(static_cast<float>(off["x"].toDouble(0.0)),
                                    static_cast<float>(off["y"].toDouble(0.0)),
                                    static_cast<float>(off["z"].toDouble(0.0)));

    return c;
}

// ── MatchGroupConfig ↔ JSON ──────────────────────────────────────────────────

/// Serialises `group`'s identity/sort fields, its matching-type name and
/// algorithm config (only for Edge-Based groups, via edgeConfig()), and the
/// JSON-encoded config of every pattern it contains.
QJsonObject groupToJson(const MatchGroup &group) {
    const MatchGroupConfig &g = group.config();
    QJsonObject o;
    o["name"]               = QString::fromStdWString(g.m_groupName);
    o["number"]             = g.m_groupIndex;
    o["sortByAngle"]        = g.m_sortByAngle;
    o["sortConditionAngle"] = g.m_sortConditionAngle;


    o["matchingType"] = matchingTypeToString(g.matchingType());
    if (auto *ecfg = g.edgeConfig())
        o["typeConfig"] = edgeConfigToJson(*ecfg);

    QJsonArray patterns;
    for (const auto &p : group.patterns())
        if (p) patterns.append(patternConfigToJson(p->config()));
    o["patterns"] = patterns;

    return o;
}

/// Reconstructs a MatchGroupConfig (identity, sort fields, matching type and
/// its type-specific config) from JSON produced by groupToJson().
/// @note the `"patterns"` array is not consumed here; PatternGroupManager::
/// fromJson() inserts patterns afterwards via addPattern() so MatchPattern
/// instances are created (and validated) correctly.
MatchGroupConfig groupConfigFromJson(const QJsonObject &o) {
    MatchGroupConfig g;
    g.m_groupName  = o["name"].toString().toStdWString();
    g.m_groupIndex = o["number"].toInt(0);
    g.m_sortByAngle = o["sortByAngle"].toBool(false);
    g.m_sortConditionAngle = o["sortConditionAngle"].toDouble(0.0);

    g.setMatchingType(matchingTypeFromString(o["matchingType"].toString()));
    if (auto *ecfg = g.edgeConfig())
        edgeConfigFromJson(o["typeConfig"].toObject(), *ecfg);

    // m_patterns vector left empty here — patterns are inserted via the
    // manager's addPattern() after the group is added, which creates the
    // MatchPattern instances correctly.
    return g;
}

} // anonymous namespace


/// Constructs an empty manager (no groups) with the given optional QObject
/// parent.
PatternGroupManager::PatternGroupManager(QObject *parent)
    : QObject{parent} {

}

/// Default destructor; m_groups' shared_ptrs release their MatchGroup
/// instances as the list is destroyed.
PatternGroupManager::~PatternGroupManager() {

}

// ── Group queries ─────────────────────────────────────────────────────────────
/// Returns the current list of groups (a copy of the internal list of
/// shared_ptrs, so iterating it is safe even if the manager is mutated
/// concurrently by the caller).
QList<std::shared_ptr<MatchGroup>> PatternGroupManager::groups() const {
    return m_groups;
}

/// Linear search for the group whose name matches `name` exactly.
/// @return the matching group, or nullptr if none exists.
std::shared_ptr<MatchGroup> PatternGroupManager::findGroupByName(const QString &name) const {
    for (std::shared_ptr<MatchGroup> g : m_groups)
        if (QString::fromStdWString(g->name()) == name) return g;
    return nullptr;
}

/// Linear search for the group whose number matches `number`.
/// @return the matching group, or nullptr if none exists.
std::shared_ptr<MatchGroup> PatternGroupManager::findGroupByNumber(int number) const {
    for (std::shared_ptr<MatchGroup> g : m_groups)
        if (g->number() == number) return g;
    return nullptr;
}

/// @return true if a group named `name` exists.
bool PatternGroupManager::containsGroupName(const QString &name) const {
    return findGroupByName(name) != nullptr;
}

/// @return true if a group numbered `number` exists.
bool PatternGroupManager::containsGroupNumber(int number) const {
    return findGroupByNumber(number) != nullptr;
}

/// Returns the number of groups currently held.
int PatternGroupManager::groupCount() const {
    return m_groups.size();
}

// ── Group mutations ───────────────────────────────────────────────────────────

/// Validates `config` (see validateGroupConfig()), then constructs a new
/// MatchGroup, appends it to m_groups, and emits groupAdded().
ManagerResult PatternGroupManager::addGroup(const MatchGroupConfig &config) {
    if (auto r = validateGroupConfig(config); !r)
        return r;

    auto *group = new MatchGroup(config, this);
    // std::shared_ptr<MatchGroup> group = std::make_shared<MatchGroup>(config, this);
    std::shared_ptr<MatchGroup> group_ptr(group);
    m_groups.append(group_ptr);
    emit groupAdded(group_ptr);
    return ManagerResult::success();
}

/// Finds the group named `groupName`, removes it from m_groups, and emits
/// groupRemoved() with a copy of its config as it was before removal.
/// @return failure if no group named `groupName` exists.
ManagerResult PatternGroupManager::removeGroup(const QString &groupName) {
    std::shared_ptr<MatchGroup> group = findGroupByName(groupName);
    if (!group) {
        return ManagerResult::fail(
            tr("Group \"%1\" not found.").arg(groupName).toStdWString());
    }

    const MatchGroupConfig removedCfg = group->config();
    m_groups.removeOne(group);

    emit groupRemoved(removedCfg);
    return ManagerResult::success();
}

/// Resolves `number` to a group name and delegates to removeGroup().
/// @return failure if no group with `number` exists.
ManagerResult PatternGroupManager::removeGroupByNumber(int number) {
    std::shared_ptr<MatchGroup> group = findGroupByNumber(number);
    if (!group) {
        return ManagerResult::fail(
            tr("No group with number %1.").arg(number).toStdWString());
    }
    return removeGroup(QString::fromStdWString(group->name()));
}

/// Finds the group named `currentName`, validates `newConfig` (excluding the
/// group's own current name from the collision check), then replaces its
/// config and emits groupChanged(group, "*") on success.
ManagerResult PatternGroupManager::setGroupConfig(const QString &currentName,
                                                  const MatchGroupConfig &newConfig) {
    std::shared_ptr<MatchGroup> group = findGroupByName(currentName);
    if (!group) {
        return ManagerResult::fail(
            tr("Group \"%1\" not found.").arg(currentName).toStdWString());
    }

    if (auto r = validateGroupConfig(newConfig, currentName); !r)
        return r;

    auto r = group->setConfig(newConfig);
    if (r) emit groupChanged(group, QStringLiteral("*"));
    return r;
}

/// Renames the group `currentName` to `newName`: succeeds as a no-op if the
/// names are equal, fails if `newName` is blank/whitespace-only or already
/// used by another group, otherwise renames and emits
/// groupChanged(group, "name").
ManagerResult PatternGroupManager::renameGroup(const QString &currentName,
                                               const QString &newName) {
    if (currentName == newName) {
        return ManagerResult::success();
    }

    if (newName.trimmed().isEmpty()) {
        return ManagerResult::fail(QStringLiteral("Group name must not be empty.").toStdWString());
    }

    if (containsGroupName(newName)) {
        return ManagerResult::fail(
            QStringLiteral("A group named \"%1\" already exists.").arg(newName).toStdWString());
    }

    std::shared_ptr<MatchGroup> group = findGroupByName(currentName);
    if (!group) {
        return ManagerResult::fail(
            QStringLiteral("Group \"%1\" not found.").arg(currentName).toStdWString());
    }

    auto r = group->setName(newName.toStdWString());
    if (r) emit groupChanged(group, QStringLiteral("name"));
    return r;
}

/// Reassigns the number of group `groupName` to `newNumber`: succeeds as a
/// no-op if unchanged, fails if `newNumber` is already used by another
/// group, otherwise renumbers and emits groupChanged(group, "number").
ManagerResult PatternGroupManager::renumberGroup(const QString &groupName,
                                                 int newNumber) {
    std::shared_ptr<MatchGroup> group = findGroupByName(groupName);
    if (!group) {
        return ManagerResult::fail(
            QStringLiteral("Group \"%1\" not found.").arg(groupName).toStdWString());
    }

    if (group->number() == newNumber) {
        return ManagerResult::success();
    }

    if (containsGroupNumber(newNumber)) {
        return ManagerResult::fail(
            QStringLiteral("Group number %1 already exists.").arg(newNumber).toStdWString());
    }

    auto r = group->setNumber(newNumber);
    if (r) emit groupChanged(group, QStringLiteral("number"));
    return r;
}

// ── Pattern mutations (pass-throughs) ─────────────────────────────────────────

/// Delegates to MatchGroup::addPattern() on group `groupName`, then emits
/// patternAdded() unconditionally (using group->lastPatternAccess(), which
/// is only meaningful when the add actually succeeded).
/// @note patternAdded is emitted even when `result` is a failure; callers
/// must still check the returned ManagerResult.
ManagerResult PatternGroupManager::addPattern(const QString &groupName,
                                              const MatchPatternConfig &config) {
    std::shared_ptr<MatchGroup> group = findGroupByName(groupName);
    if (!group)
        return ManagerResult::fail(
            QStringLiteral("Group \"%1\" not found.").arg(groupName).toStdWString());

    ManagerResult result = group->addPattern(config);
    emit patternAdded(group.get(), group->lastPatternAccess().get());
    // emit patternAdded();
    return result;
}

/// Delegates to MatchGroup::removePattern() on group `groupName`.
/// @note unlike the other pattern mutators, this does not emit
/// patternRemoved (see the signal's declaration in pattern_group_manager.h).
ManagerResult PatternGroupManager::removePattern(const QString &groupName,
                                                 const QString &patternName) {
    std::shared_ptr<MatchGroup> group = findGroupByName(groupName);
    if (!group)
        return ManagerResult::fail(
            QStringLiteral("Group \"%1\" not found.").arg(groupName).toStdWString());

    return group->removePattern(patternName.toStdWString());
}

/// Delegates to MatchGroup::setPatternConfig() on group `groupName`; on
/// success, resolves the pattern by its (possibly new) name from `newConfig`
/// and emits patternChanged(group, pattern, "*").
ManagerResult PatternGroupManager::setPatternConfig(const QString &groupName,
                                                    const QString &currentPatternName,
                                                    const MatchPatternConfig &newConfig) {
    std::shared_ptr<MatchGroup> group = findGroupByName(groupName);
    if (!group)
        return ManagerResult::fail(
            QStringLiteral("Group \"%1\" not found.").arg(groupName).toStdWString());

    auto r = group->setPatternConfig(currentPatternName.toStdWString(), newConfig);
    if (r) {
        // Pattern identity may have changed (rename); resolve by new name.
        if (auto *p = group->findPatternByName(newConfig.m_patternName))
            emit patternChanged(group.get(), p, QStringLiteral("*"));
    }
    return r;
}

/// Delegates to MatchGroup::renamePattern() on group `groupName`; on
/// success, resolves the pattern by its new name and emits
/// patternChanged(group, pattern, "name").
ManagerResult PatternGroupManager::renamePattern(const QString &groupName,
                                                 const QString &currentName,
                                                 const QString &newName)
{
    std::shared_ptr<MatchGroup> group = findGroupByName(groupName);
    if (!group)
        return ManagerResult::fail(
            QStringLiteral("Group \"%1\" not found.").arg(groupName).toStdWString());

    auto r = group->renamePattern(currentName.toStdWString(), newName.toStdWString());
    if (r) {
        if (auto *p = group->findPatternByName(newName.toStdWString()))
            emit patternChanged(group.get(), p, QStringLiteral("name"));
    }
    return r;
}

/// Delegates to MatchGroup::renumberPattern() on group `groupName`; on
/// success, resolves the pattern (name is unchanged by a renumber) and
/// emits patternChanged(group, pattern, "number").
ManagerResult PatternGroupManager::renumberPattern(const QString &groupName,
                                                   const QString &patternName,
                                                   int newNumber)
{
    std::shared_ptr<MatchGroup> group = findGroupByName(groupName);
    if (!group)
        return ManagerResult::fail(
            QStringLiteral("Group \"%1\" not found.").arg(groupName).toStdWString());

    auto r = group->renumberPattern(patternName.toStdWString(), newNumber);
    if (r) {
        if (auto *p = group->findPatternByName(patternName.toStdWString()))
            emit patternChanged(group.get(), p, QStringLiteral("number"));
    }
    return r;
}

/// Delegates to MatchGroup::setPatternImage() on group `groupName`; on
/// success, resolves the pattern by name and emits
/// patternChanged(group, pattern, "image").
ManagerResult PatternGroupManager::setPatternImage(const QString &groupName,
                                                   const QString &patternName,
                                                   const cv::Mat &image)
{
    std::shared_ptr<MatchGroup> group = findGroupByName(groupName);
    if (!group)
        return ManagerResult::fail(
            QStringLiteral("Group \"%1\" not found.").arg(groupName).toStdWString());

    auto r = group->setPatternImage(patternName.toStdWString(), image);
    if (r) {
        if (auto *p = group->findPatternByName(patternName.toStdWString()))
            emit patternChanged(group.get(), p, QStringLiteral("image"));
    }
    return r;
}

// ── Bulk operations ───────────────────────────────────────────────────────────

/// Sorts m_groups in-place by ascending MatchGroup::number() and emits
/// groupsReordered() with the new order.
void PatternGroupManager::sortGroupsByNumber()
{
    std::sort(m_groups.begin(), m_groups.end(),
              [](std::shared_ptr<MatchGroup> a, std::shared_ptr<MatchGroup> b) {
                  return a->number() < b->number();
              });
    emit groupsReordered(m_groups);
}

/// Sorts the patterns of group `groupName` in-place by ascending
/// MatchPattern::number() (reaching into MatchGroup::m_patterns directly via
/// friend access) and emits groupChanged(group, "patternsReordered").
/// @return failure if no group named `groupName` exists.
ManagerResult PatternGroupManager::sortPatternsByNumber(const QString &groupName)
{
    std::shared_ptr<MatchGroup> group = findGroupByName(groupName);
    if (!group)
        return ManagerResult::fail(
            QStringLiteral("Group \"%1\" not found.").arg(groupName).toStdWString());

    auto &patterns = group->m_patterns;
    std::sort(patterns.begin(), patterns.end(),
              [](std::shared_ptr<MatchPattern> a, std::shared_ptr<MatchPattern> b) {
                  return a->number() < b->number();
              });

    // Notify listeners that the group's content changed (order only).
    emit groupChanged(group, QStringLiteral("patternsReordered"));
    return ManagerResult::success();
}

// ── Private ───────────────────────────────────────────────────────────────────

/// Rejects `cfg` if its name is empty, or if any existing group other than
/// `excludeName` already has the same name or the same number.
/// `excludeName` is passed by callers that are reconfiguring/renaming a
/// group in place, so the group does not collide with its own prior state.
ManagerResult PatternGroupManager::validateGroupConfig(
    const MatchGroupConfig &cfg, const QString &excludeName) const
{
    if (cfg.m_groupName.empty())
        return ManagerResult::fail(QStringLiteral("Group name must not be empty.").toStdWString());

    for (std::shared_ptr<MatchGroup> g : m_groups) {
        if (QString::fromStdWString(g->name()) == excludeName) continue;   // skip self
        if (g->name() == cfg.m_groupName)
            return ManagerResult::fail(
                QStringLiteral("A group named \"%1\" already exists.").arg(cfg.m_groupName).toStdWString());
        if (g->number() == cfg.m_groupIndex)
            return ManagerResult::fail(
                QStringLiteral("Group number %1 already exists.").arg(cfg.m_groupIndex).toStdWString());
    }
    return ManagerResult::success();
}

/// Builds the `{ "groups": [...] }` document for the whole library by
/// mapping groupToJson() over every group in m_groups.
QJsonObject PatternGroupManager::toJson() const {
    QJsonArray groupsArr;
    for (const auto &group : m_groups)
        if (group) groupsArr.append(groupToJson(*group));

    QJsonObject o;
    o["groups"] = groupsArr;
    return o;
}

/// Clears every existing group, then rebuilds the library from `obj`
/// (schema produced by toJson()): decodes and addGroup()s each group, then
/// addPattern()s each of its patterns. Failures on individual groups/
/// patterns are logged via LOG_DEV_ERR and skipped rather than aborting the
/// whole load.
/// @return true only if every group and pattern loaded without error.
bool PatternGroupManager::fromJson(const QJsonObject &obj) {
    // Reset to empty state.  No clearAll() exists, so iterate by number;
    // groups() returns a snapshot copy, so removing during iteration is safe.
    const auto existing = m_groups;
    for (const auto &g : existing)
        if (g) removeGroupByNumber(g->number());

    bool allOk = true;
    const QJsonArray groupsArr = obj["groups"].toArray();
    for (const QJsonValue &gv : groupsArr) {
        const QJsonObject gObj  = gv.toObject();
        const MatchGroupConfig gcfg = groupConfigFromJson(gObj);

        if (auto r = addGroup(gcfg); !r) {
            LOG_DEV_ERR << "PatternGroupManager::fromJson – addGroup failed: "
                        << QString::fromStdWString(r.error);
            allOk = false;
            continue;
        }

        const QString groupName = QString::fromStdWString(gcfg.m_groupName);
        for (const QJsonValue &pv : gObj["patterns"].toArray()) {
            const MatchPatternConfig pcfg = patternConfigFromJson(pv.toObject());
            if (auto r = addPattern(groupName, pcfg); !r) {
                LOG_DEV_ERR << "PatternGroupManager::fromJson – addPattern failed: "
                            << QString::fromStdWString(r.error);
                allOk = false;
            }
        }
    }
    return allOk;
}

}
