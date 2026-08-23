#ifndef GRIPPER_PRESET_STORE_H
#define GRIPPER_PRESET_STORE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include "matching/gripper_boxes.h"

/**
 * @file gripper_preset_store.h
 * @brief GripperPreset / GripperPresetStore — named, reusable gripper geometries saved
 *        with the project and offered when a pattern's picking box is authored.
 */

namespace vc::model {

/**
 * @struct GripperPreset
 * @brief One named gripper geometry: a display name plus the jaw-pair values it stands for.
 */
struct GripperPreset {
    QString           name;   ///< Unique, non-empty display name.
    mtc::GripperBoxes boxes;  ///< Jaw size and centre-to-centre distance this preset applies.
};

/**
 * @class GripperPresetStore
 * @brief Ordered collection of uniquely-named GripperPreset entries, owned by a task and
 *        serialised with the project.
 *
 * Presets are a convenience for authoring, not a live binding: applying one COPIES its
 * values into the pattern's own MatchPatternConfig::m_gripperBoxes. Editing or deleting a
 * preset afterwards therefore never mutates patterns already authored from it, which is
 * what keeps a commissioned task stable when the preset list is tidied up later.
 *
 * A preset describes the GRIPPER — jaw size and spacing — so it deliberately carries no
 * jaw angle. The orientation is per-pattern (MatchPatternConfig::m_pickingBoxAngle) and
 * applying a preset must leave it untouched.
 *
 * Name comparison is case-sensitive and exact; the store rejects blank and duplicate
 * names rather than silently de-duplicating, so the caller can report the failure.
 *
 * @note Plain value type — no QObject, no signals. The register widget edits a copy and
 *       hands the result back, so there is no change-notification contract to honour.
 * @see mtc::GripperBoxes, MatchPatternConfig
 */
class GripperPresetStore {
public:
    /// Returns every preset, in insertion order.
    const QVector<GripperPreset>& presets() const { return m_presets; }

    /// Returns how many presets are stored.
    int count() const { return m_presets.size(); }

    /// @return true if a preset with exactly this name exists.
    bool contains(const QString &name) const;

    /**
     * @brief Looks up a preset by exact name.
     * @param[in] name preset name to find
     * @return pointer to the stored preset, or nullptr when no preset has that name.
     *         The pointer is invalidated by any mutating call on this store.
     */
    const GripperPreset* find(const QString &name) const;

    /**
     * @brief Appends a new preset.
     * @param[in] preset preset to store; its name must be non-blank and unused
     * @return false when the name is blank or already taken (nothing is stored)
     */
    bool add(const GripperPreset &preset);

    /**
     * @brief Replaces the geometry of an existing preset, keeping its name and position.
     * @param[in] name  name of the preset to update
     * @param[in] boxes new geometry
     * @return false when no preset has that name
     */
    bool setBoxes(const QString &name, const mtc::GripperBoxes &boxes);

    /**
     * @brief Renames a preset in place, keeping its position in the list.
     * @param[in] from current name
     * @param[in] to   new name; must be non-blank and unused by a different preset
     * @return false when `from` does not exist, or `to` is blank or already taken.
     *         Renaming to the same name succeeds as a no-op.
     */
    bool rename(const QString &from, const QString &to);

    /**
     * @brief Removes the preset with this name.
     * @param[in] name preset to remove
     * @return false when no preset has that name
     */
    bool remove(const QString &name);

    /// Serialises every preset to a JSON array of `{ "name", "w", "h", "distance" }`.
    QJsonArray toJson() const;

    /**
     * @brief Replaces the contents with the presets decoded from `arr`.
     *
     * Entries with a blank or duplicate name are skipped rather than aborting the load, so
     * a hand-edited project file cannot break task loading; a partial list is better than
     * no task.
     * @param[in] arr JSON array previously produced by toJson()
     */
    void fromJson(const QJsonArray &arr);

private:
    /// @return index of the preset named `name`, or -1 when absent.
    int indexOf(const QString &name) const;

    QVector<GripperPreset> m_presets;  ///< Stored presets, in insertion order.
};

} // namespace vc::model
#endif // GRIPPER_PRESET_STORE_H
