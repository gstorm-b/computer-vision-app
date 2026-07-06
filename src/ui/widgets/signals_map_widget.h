#ifndef SIGNALS_MAP_WIDGET_H
#define SIGNALS_MAP_WIDGET_H

#include <QTableWidget>
#include <QStringList>

class QLineEdit;
class QStyledItemDelegate;

// =====================================================================
// SignalsMapWidget
// =====================================================================
//
// 3-column table that maps "task signals" (logical names declared by a task,
// e.g. "Camera selection", "Pattern selection") to "communication tags"
// (PLC addresses or named tags, e.g. "M100", "D200") provided by the assigned
// communication device.
//
//   Signal    |  Type           |  Mapped to
//   --------- + --------------- + ------------------
//   <display> | Number | Bool   | <combobox tag>
//
// Internal vs display name
// ────────────────────────
// Every row carries an internalName (the field key in the owning config,
// e.g. "nActiveCamera") that the owner uses to write back into the model,
// AND a displayName (the user-facing label) shown in column 1. The change
// signal carries the internalName so the owner can route it without parsing
// human text.
//
// Tag list ownership
// ──────────────────
// The widget never queries devices itself. The owning page is expected to
// push the per-type tag lists via setBoolTags() / setNumberTags() whenever
// the assigned comm device (or its configuration) changes. The combobox is
// non-editable: users can only pick from the supplied list.
//
// Orphan handling
// ───────────────
// When a tag list is replaced, any currently-selected tag that is not in
// the new list is kept visible (so the user sees what was there) but the
// row is flagged as a warning (background tint + tooltip). The orphan is
// not auto-cleared. checkEmpty() is the explicit point where orphans get
// purged to "" so the caller can report which signals need re-mapping.
//
// Conflict handling
// ─────────────────
// Tags already chosen by other rows are rendered in the dropdown with a
// grey colour and a "(used by …)" suffix. The user can still pick them;
// doing so opens a Yes/No confirmation. On Yes, the previous owning row
// is cleared to "" and flagged as warning; on No, the picked combobox
// reverts to its previous value.
//
class SignalsMapWidget : public QTableWidget
{
    Q_OBJECT

public:
    enum class Type { Number, Bool };
    Q_ENUM(Type)

    explicit SignalsMapWidget(QWidget *parent = nullptr);

    // Row management. The internalName is the unique row key; insert/append
    // will be no-ops if internalName already exists (and log a warning).
    void appendRow(const QString &internalName,
                   const QString &displayName,
                   Type type);
    void insertRowAt(int row,
                     const QString &internalName,
                     const QString &displayName,
                     Type type);
    void removeRow(const QString &internalName);
    void clearRows();

    // Set / get the chosen tag for a row, by internalName.
    void setRowValue(const QString &internalName, const QString &tag);
    QString rowValue(const QString &internalName) const;

    // Replace the per-type tag list. Rows whose current value is not in the
    // matching list become warning-flagged but keep their displayed text.
    void setNumberTags(const QStringList &tags);
    void setBoolTags(const QStringList &tags);

    // Destructive: any warning-flagged (orphaned) tags are cleared to "".
    // Returns the internalNames of rows that are now empty.
    QStringList checkEmpty();

    // Helper exposed for the combobox view's item delegate. Returns the
    // displayName of the row that currently owns `tag`, excluding `excludeRow`.
    // Empty string if no other row owns it.
    QString tagOwnerDisplay(const QString &tag, int excludeRow) const;

signals:
    // Emitted after the user successfully changes a row's tag.
    void signalMappingChanged(const QString &internalName, const QString &tag);

private slots:
    void onLineEditingFinished();

private:
    struct RowState {
        QString internalName;
        QString displayName;
        Type    type;
        QString currentTag;
        bool    isWarning{false};
    };

    void initHeader();
    int  rowIndexOf(const QString &internalName) const;
    void populateEditor(int row);
    void applyWarning(int row, bool on);
    QStringList tagsFor(Type t) const;
    // Recompute the completer popup paint state of every row's editor
    // (tags used by other rows are greyed, etc.).
    void refreshEditorDelegates();

    QList<RowState> m_rows;
    QStringList     m_numberTags;
    QStringList     m_boolTags;

    // Guard re-entrant editingFinished when we revert a QLineEdit value.
    bool m_suppressEdit{false};
};

#endif // SIGNALS_MAP_WIDGET_H
