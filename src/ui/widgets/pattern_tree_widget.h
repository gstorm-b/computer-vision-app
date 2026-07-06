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

struct MatchPatternConfig {
    QString name        = QStringLiteral("Pattern");
    int number      = 0;
    double threshScore = 0.80;
    QPixmap thumbnail;          ///< Optional; empty = "No Image" placeholder
};

struct MatchGroupConfig {
    QString                    name    = QStringLiteral("Group");
    int                        number  = 0;
    QList<MatchPatternConfig>  patterns;
};

// ============================================================================
//  PatternGroupItemWidget
//  Displayed for every top-level (group) tree item.
//  Contents: [accent-bar] [name label] [group# label] <stretch>
//            [+ Pattern btn] [✕ Delete btn]
// ============================================================================
class PatternGroupItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PatternGroupItemWidget(const MatchGroupConfig &cfg,
                                    QWidget *parent = nullptr);

    /// Replace all displayed data without rebuilding the widget.
    void             setConfig(const MatchGroupConfig &cfg);
    MatchGroupConfig config() const;

signals:
    /// Fired when user clicks the widget background (not a button).
    void clicked();
    void deleteRequested();
    void addPatternRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void refresh();

    MatchGroupConfig m_config;
    QLabel      *m_nameLabel    = nullptr;
    QLabel      *m_numberLabel  = nullptr;
    QPushButton *m_addPatternBtn = nullptr;
    QPushButton *m_deleteBtn     = nullptr;
};


// ============================================================================
//  PatternItemWidget
//  Displayed for every child (pattern) tree item.
//  Contents: [thumbnail 64×64] [VBox: name / number / thresh] <stretch>
//            [✕ Delete btn]
// ============================================================================
class PatternItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PatternItemWidget(const MatchPatternConfig &cfg,
                               QWidget *parent = nullptr);

    void               setConfig(const MatchPatternConfig &cfg);
    MatchPatternConfig config() const;

signals:
    void clicked();
    void editRequested();
    void deleteRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUi();
    void refresh();

    MatchPatternConfig m_config;
    QLabel      *m_thumbnail   = nullptr;
    QLabel      *m_nameLabel   = nullptr;
    QLabel      *m_numberLabel = nullptr;
    QLabel      *m_threshLabel = nullptr;
    QPushButton *m_editBtn     = nullptr;
    QPushButton *m_deleteBtn   = nullptr;
};


// ============================================================================
//  FooterItemWidget
//  Always pinned at the bottom of the tree.
//  Contents: [＋ Add Group btn] [⇅ Auto Sort btn]
// ============================================================================
class FooterItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FooterItemWidget(QWidget *parent = nullptr);

signals:
    void addGroupRequested();
    void autoSortRequested();

private:
    void setupUi();
    QPushButton *m_addGroupBtn = nullptr;
    QPushButton *m_autoSortBtn = nullptr;
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
    explicit PatternTreeWidget(QWidget *parent = nullptr);

    // ── Data I/O ─────────────────────────────────────────────────────────────
    void                    setGroups(const QList<MatchGroupConfig> &groups);
    QList<MatchGroupConfig> groups() const;

    // ── Programmatic mutations (call from host) ───────────────────────────────
    void addGroup(const MatchGroupConfig &group);
    void removeGroup(int groupIndex);
    void updateGroup(int groupIndex, const MatchGroupConfig &group);

    void addPattern(int groupIndex, const MatchPatternConfig &pattern);
    void removePattern(int groupIndex, int patternIndex);
    void updatePattern(int groupIndex, int patternIndex, const MatchPatternConfig &pattern);

    /// Sort all groups ascending by MatchGroupConfig::number and rebuild.
    void autoSort();

signals:
    // ── Click events ──────────────────────────────────────────────────────────
    void groupClicked  (int groupIndex,   const MatchGroupConfig   &cfg);
    void patternClicked(int groupIndex, int patternIndex,
                        const MatchPatternConfig &cfg);

    // ── User intent (host should respond by calling add/remove methods) ────────
    void addGroupRequested();
    void addPatternRequested(int groupIndex);
    void editPatternRequested(int groupIndex, int patternIndex);
    void deleteGroupRequested(int groupIndex);
    void deletePatternRequested(int groupIndex, int patternIndex);

    // ── Data-change notifications (emitted after internal state is updated) ───
    void groupsChanged(const QList<MatchGroupConfig> &groups);
    void groupChanged(int groupIndex, const MatchGroupConfig &cfg);
    void patternChanged(int groupIndex, int patternIndex,
                        const MatchPatternConfig &cfg);

private:
    // ── Build helpers ─────────────────────────────────────────────────────────
    void setupStyle();
    void rebuildTree();
    void appendGroupItem(const MatchGroupConfig &group, int groupIndex);
    void appendPatternItem(QTreeWidgetItem *parentItem,
                           const MatchPatternConfig &pattern,
                           int groupIndex, int patternIndex);
    void appendFooterItem();
    int mapGroupNumber(int groupIndex);
    int mapPatternNumber(int patternIndex, QList<MatchPatternConfig> &cfgs);

    // ── Utilities ─────────────────────────────────────────────────────────────
    QSet<int> expandedGroupIndices() const;
    void      restoreExpanded(const QSet<int> &indices);

    QList<MatchGroupConfig> m_groups;
    QTreeWidgetItem        *m_footerItem = nullptr;
};

#endif // PATTERN_TREE_WIDGET_H
