#ifndef PATTERN_TREE_WIDGET_H
#define PATTERN_TREE_WIDGET_H

#include <QTreeWidget>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QMouseEvent>
#include <QSet>

// #include "MatchGroupConfig.h"
// #include "MatchPatternConfig.h"

/// A single match pattern's editable data: display name, identifying number,
/// match threshold score, and an optional preview thumbnail.
struct MatchPatternConfig {
    QString name        = QStringLiteral("Pattern");   ///< Display name shown in the tree.
    int number      = 0;   ///< Pattern number/identifier used to map tree rows back to config entries.
    double threshScore = 0.80;   ///< Match threshold score (0.0-1.0) required to accept a match.
    QPixmap thumbnail;          ///< Optional; empty = "No Image" placeholder
};

/// A named group of MatchPatternConfig entries, displayed as a top-level tree item
/// with its patterns nested underneath.
struct MatchGroupConfig {
    QString                    name    = QStringLiteral("Group");   ///< Display name shown in the tree.
    int                        number  = 0;   ///< Group number/identifier used to map tree rows back to config entries.
    QList<MatchPatternConfig>  patterns;   ///< Patterns belonging to this group, in display order.
};

// ============================================================================
//  PatternGroupItemWidget
//  Displayed for every top-level (group) tree item.
//  Contents: [accent-bar] [name label] [group# label] <stretch>
//            [+ Pattern btn] [✕ Delete btn]
// ============================================================================
/// Row widget for a top-level (group) tree item: accent bar, name label, group
/// number badge, an "Add Pattern" button, and a delete button.
class PatternGroupItemWidget : public QWidget
{
    Q_OBJECT

public:
    /// Builds the row UI and populates it from `cfg`.
    explicit PatternGroupItemWidget(const MatchGroupConfig &cfg,
                                    QWidget *parent = nullptr);

    /// Replace all displayed data without rebuilding the widget.
    void             setConfig(const MatchGroupConfig &cfg);
    /// Returns the currently displayed group configuration.
    MatchGroupConfig config() const;

signals:
    /// Fired when user clicks the widget background (not a button).
    void clicked();
    /// Fired when the delete ("x") button is clicked.
    void deleteRequested();
    /// Fired when the "+ Pattern" button is clicked.
    void addPatternRequested();

protected:
    /// Emits clicked() unless the press landed on a child button (in which case
    /// the button's own click handling takes over and the event is ignored here).
    void mousePressEvent(QMouseEvent *event) override;

private:
    /// Builds the row layout (accent bar, labels, buttons) and wires button signals.
    void setupUi();
    /// Updates the name/number labels from m_config.
    void refresh();

    MatchGroupConfig m_config;
    QLabel      *m_nameLabel    = nullptr;   ///< Displays MatchGroupConfig::name.
    QLabel      *m_numberLabel  = nullptr;   ///< Displays MatchGroupConfig::number as "# N".
    QPushButton *m_addPatternBtn = nullptr;   ///< Emits addPatternRequested() when clicked.
    QPushButton *m_deleteBtn     = nullptr;   ///< Emits deleteRequested() when clicked.
};


// ============================================================================
//  PatternItemWidget
//  Displayed for every child (pattern) tree item.
//  Contents: [thumbnail 64×64] [VBox: name / number / thresh] <stretch>
//            [✕ Delete btn]
// ============================================================================
/// Row widget for a child (pattern) tree item: thumbnail, name/number/threshold
/// labels, an edit button, and a delete button.
class PatternItemWidget : public QWidget
{
    Q_OBJECT

public:
    /// Builds the row UI and populates it from `cfg`.
    explicit PatternItemWidget(const MatchPatternConfig &cfg,
                               QWidget *parent = nullptr);

    /// Replace all displayed data (name/number/threshold/thumbnail) without rebuilding the widget.
    void               setConfig(const MatchPatternConfig &cfg);
    /// Returns the currently displayed pattern configuration.
    MatchPatternConfig config() const;

signals:
    /// Fired when user clicks the widget background (not a button).
    void clicked();
    /// Fired when the edit ("pencil") button is clicked.
    void editRequested();
    /// Fired when the delete ("x") button is clicked.
    void deleteRequested();

protected:
    /// Emits clicked() unless the press landed on a child button.
    void mousePressEvent(QMouseEvent *event) override;

private:
    /// Builds the row layout (thumbnail, info column, buttons) and wires button signals.
    void setupUi();
    /// Updates the thumbnail and name/number/threshold labels from m_config;
    /// shows a "No Image" placeholder when the thumbnail is null.
    void refresh();

    MatchPatternConfig m_config;
    QLabel      *m_thumbnail   = nullptr;   ///< Shows MatchPatternConfig::thumbnail, or a "No Image" placeholder text.
    QLabel      *m_nameLabel   = nullptr;   ///< Displays MatchPatternConfig::name.
    QLabel      *m_numberLabel = nullptr;   ///< Displays MatchPatternConfig::number as "Pattern #N".
    QLabel      *m_threshLabel = nullptr;   ///< Displays MatchPatternConfig::threshScore as "Thresh N.NNN".
    QPushButton *m_editBtn     = nullptr;   ///< Emits editRequested() when clicked.
    QPushButton *m_deleteBtn   = nullptr;   ///< Emits deleteRequested() when clicked.
};


// ============================================================================
//  FooterItemWidget
//  Always pinned at the bottom of the tree.
//  Contents: [＋ Add Group btn] [⇅ Auto Sort btn]
// ============================================================================
/// Row widget pinned as the tree's last (non-selectable) item: an "Add Group"
/// button and an "Auto Sort" button.
class FooterItemWidget : public QWidget
{
    Q_OBJECT

public:
    /// Builds the row UI (buttons only, no config to display).
    explicit FooterItemWidget(QWidget *parent = nullptr);

signals:
    /// Fired when the "+ Add Group" button is clicked.
    void addGroupRequested();
    /// Fired when the "Auto Sort" button is clicked.
    void autoSortRequested();

private:
    /// Builds the row layout (two buttons) and wires their click signals.
    void setupUi();
    QPushButton *m_addGroupBtn = nullptr;   ///< Emits addGroupRequested() when clicked.
    QPushButton *m_autoSortBtn = nullptr;   ///< Emits autoSortRequested() when clicked.
};


// ============================================================================
//  PatternTreeWidget
//  Main widget.  Inherit from QTreeWidget — simpler than QTreeView + model
//  because every item has a distinct custom delegate widget.
//
//  Layout:
//    ┌─ Group 0  (PatternGroupItemWidget)
//    │   ├── Pattern 0  (PatternItemWidget)
//    │   └── Pattern 1  (PatternItemWidget)
//    ├─ Group 1  ...
//    └─ [Footer]  (FooterItemWidget)  — non-selectable
//
//  All signals that need a response from the host application are listed in
//  the "Host → Widget" and "Widget → Host" sections below.
// ============================================================================
class PatternTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    /// Applies the tree's static styling and appends the pinned footer row;
    /// the tree is empty of groups until setGroups()/addGroup() is called.
    explicit PatternTreeWidget(QWidget *parent = nullptr);

    // ── Data I/O ─────────────────────────────────────────────────────────────
    /// Replaces all groups with `groups`, rebuilds the tree, and expands every group.
    void                    setGroups(const QList<MatchGroupConfig> &groups);
    /// Returns the current groups (and their nested patterns) in display order.
    QList<MatchGroupConfig> groups() const;

    // ── Programmatic mutations (call from host) ───────────────────────────────
    /// Appends `group` as a new top-level tree item (before the footer) and
    /// emits groupsChanged().
    void addGroup(const MatchGroupConfig &group);
    /// Removes the group at `groupIndex` (mapped via mapGroupNumber()) and
    /// defers the tree rebuild to the next event loop iteration so it is safe
    /// to call from a signal handler still on the call stack.
    /// @param groupIndex group number to remove (not a raw list index)
    void removeGroup(int groupIndex);
    /// Replaces the config for the group at `groupIndex` (mapped via
    /// mapGroupNumber()), updates its row widget in place, and emits
    /// groupChanged()/groupsChanged().
    /// @param groupIndex group number to update (not a raw list index)
    void updateGroup(int groupIndex, const MatchGroupConfig &group);

    /// Appends `pattern` to the group at `groupIndex` (mapped via
    /// mapGroupNumber()), inserts its row widget, expands the group, and emits
    /// groupChanged()/groupsChanged().
    /// @param groupIndex group number to append to (not a raw list index)
    void addPattern(int groupIndex, const MatchPatternConfig &pattern);
    /// Removes the pattern at `patternIndex` from the group at `groupIndex`
    /// (mapped via mapGroupNumber()) and defers the tree rebuild to the next
    /// event loop iteration.
    /// @param groupIndex group number containing the pattern (not a raw list index)
    void removePattern(int groupIndex, int patternIndex);
    /// Replaces the pattern at `patternIndex` within the group at `groupIndex`
    /// (mapped via mapGroupNumber()), updates its row widget in place, and
    /// emits patternChanged()/groupsChanged().
    /// @param groupIndex group number containing the pattern (not a raw list index)
    void updatePattern(int groupIndex, int patternIndex, const MatchPatternConfig &pattern);

    /// Sort all groups ascending by MatchGroupConfig::number and rebuild.
    void autoSort();

signals:
    // ── Click events ──────────────────────────────────────────────────────────
    /// Emitted when a group row is clicked (background, not a button).
    void groupClicked  (int groupIndex,   const MatchGroupConfig   &cfg);
    /// Emitted when a pattern row is clicked (background, not a button).
    void patternClicked(int groupIndex, int patternIndex,
                        const MatchPatternConfig &cfg);

    // ── User intent (host should respond by calling add/remove methods) ────────
    /// Emitted when the footer's "Add Group" button is clicked.
    void addGroupRequested();
    /// Emitted when a group's "+ Pattern" button is clicked.
    void addPatternRequested(int groupIndex);
    /// Emitted when a pattern's edit button is clicked.
    void editPatternRequested(int groupIndex, int patternIndex);
    /// Emitted when a group's delete button is clicked.
    void deleteGroupRequested(int groupIndex);
    /// Emitted when a pattern's delete button is clicked.
    void deletePatternRequested(int groupIndex, int patternIndex);

    // ── Data-change notifications (emitted after internal state is updated) ───
    /// Emitted after any mutation, with the full up-to-date group list.
    void groupsChanged(const QList<MatchGroupConfig> &groups);
    /// Emitted after a single group's data changes.
    void groupChanged(int groupIndex, const MatchGroupConfig &cfg);
    /// Emitted after a single pattern's data changes.
    void patternChanged(int groupIndex, int patternIndex,
                        const MatchPatternConfig &cfg);

private:
    // ── Build helpers ─────────────────────────────────────────────────────────
    /// Configures the tree's static QTreeWidget properties (no header, root
    /// decoration, animation, single selection, scroll behavior); visual
    /// styling itself lives in dark.qss/light.qss.
    void setupStyle();
    /// Clears and rebuilds the entire tree from m_groups, preserving which
    /// groups were expanded beforehand.
    void rebuildTree();
    /// Creates the top-level tree item and PatternGroupItemWidget for `group`,
    /// inserts it before the footer, wires its signals, and appends its patterns.
    void appendGroupItem(const MatchGroupConfig &group, int groupIndex);
    /// Creates a child tree item and PatternItemWidget for `pattern` under
    /// `parentItem` and wires its signals.
    void appendPatternItem(QTreeWidgetItem *parentItem,
                           const MatchPatternConfig &pattern,
                           int groupIndex, int patternIndex);
    /// (Re)creates the pinned footer row (FooterItemWidget) as the tree's last item.
    void appendFooterItem();
    /// Maps a group's MatchGroupConfig::number to its current index in m_groups.
    /// @return the matching index, or -1 if no group has that number
    int mapGroupNumber(int groupIndex);
    /// Maps a pattern's MatchPatternConfig::number to its index within `cfgs`.
    /// @return the matching index, or -1 if no pattern has that number
    int mapPatternNumber(int patternIndex, QList<MatchPatternConfig> &cfgs);

    // ── Utilities ─────────────────────────────────────────────────────────────
    /// Returns the indices (within m_groups) of all currently expanded group rows.
    QSet<int> expandedGroupIndices() const;
    /// Re-expands the group rows at `indices` after a rebuild.
    void      restoreExpanded(const QSet<int> &indices);

    QList<MatchGroupConfig> m_groups;
    QTreeWidgetItem        *m_footerItem = nullptr;   ///< The pinned, non-selectable footer tree item.
};

#endif // PATTERN_TREE_WIDGET_H
