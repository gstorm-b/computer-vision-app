#ifndef SIGNALS_MAP_WIDGET_H
#define SIGNALS_MAP_WIDGET_H

#include <QTableWidget>
#include <QStringList>

class QLineEdit;
class QStyledItemDelegate;

/// 3-column table that maps "task signals" (logical names declared by a task,
/// e.g. "Camera selection", "Pattern selection") to "communication tags"
/// (PLC addresses or named tags, e.g. "M100", "D200") provided by the assigned
/// communication device.
///
///   Signal    |  Type           |  Mapped to
///   --------- + --------------- + ------------------
///   `<display>` | Number | Bool   | `<combobox tag>`
///
/// @par Internal vs display name
/// Every row carries an internalName (the field key in the owning config,
/// e.g. "nActiveCamera") that the owner uses to write back into the model,
/// AND a displayName (the user-facing label) shown in column 1. The change
/// signal carries the internalName so the owner can route it without parsing
/// human text.
///
/// @par Tag list ownership
/// The widget never queries devices itself. The owning page is expected to
/// push the per-type tag lists via setBoolTags() / setNumberTags() whenever
/// the assigned comm device (or its configuration) changes. The combobox is
/// non-editable: users can only pick from the supplied list.
///
/// @par Orphan handling
/// When a tag list is replaced, any currently-selected tag that is not in
/// the new list is kept visible (so the user sees what was there) but the
/// row is flagged as a warning (background tint + tooltip). The orphan is
/// not auto-cleared. checkEmpty() is the explicit point where orphans get
/// purged to "" so the caller can report which signals need re-mapping.
///
/// @par Conflict handling
/// Tags already chosen by other rows are rendered in the dropdown with a
/// grey colour and a "(used by …)" suffix. The user can still pick them;
/// doing so opens a Yes/No confirmation. On Yes, the previous owning row
/// is cleared to "" and flagged as warning; on No, the picked combobox
/// reverts to its previous value.
class SignalsMapWidget : public QTableWidget
{
    Q_OBJECT

public:
    /// Signal value kind: numeric (word) or boolean (bit); drives which tag
    /// list (m_numberTags / m_boolTags) a row's combobox is populated from.
    enum class Type { Number, Bool };
    Q_ENUM(Type)

    /// Constructs the widget and sets up the 3-column header (Signal / Type /
    /// Mapped to) with no rows.
    explicit SignalsMapWidget(QWidget *parent = nullptr);

    /// Appends a new row for `internalName` at the end of the table.
    /// @note internalName is the unique row key; this is a no-op (with a
    /// logged warning) if a row for internalName already exists.
    void appendRow(const QString &internalName,
                   const QString &displayName,
                   Type type);
    /// Inserts a new row for `internalName` at `row` (clamped to the valid
    /// range), with its tag editor populated from the current per-type tag
    /// list.
    /// @note internalName is the unique row key; this is a no-op (with a
    /// logged warning) if a row for internalName already exists.
    void insertRowAt(int row,
                     const QString &internalName,
                     const QString &displayName,
                     Type type);
    /// Removes the row identified by `internalName`, if one exists.
    void removeRow(const QString &internalName);
    /// Removes every row from the table.
    void clearRows();

    /// Sets the tag text for the row identified by `internalName` and
    /// refreshes its warning state (orphan if the tag is not in the current
    /// per-type list).
    void setRowValue(const QString &internalName, const QString &tag);
    /// Returns the currently selected tag for the row identified by
    /// `internalName`, or an empty string if the row does not exist.
    QString rowValue(const QString &internalName) const;

    /// Replaces the selectable tag list for Number-type rows and re-populates
    /// every Number row's editor from it.
    /// @note Rows whose current tag is not in `tags` become warning-flagged
    /// (orphaned) but keep their displayed text.
    void setNumberTags(const QStringList &tags);
    /// Replaces the selectable tag list for Bool-type rows and re-populates
    /// every Bool row's editor from it.
    /// @note Rows whose current tag is not in `tags` become warning-flagged
    /// (orphaned) but keep their displayed text.
    void setBoolTags(const QStringList &tags);

    /// Clears the tag of every warning-flagged (orphaned) row to "" and emits
    /// signalMappingChanged() for each one cleared.
    /// @return internalNames of every row whose current tag is now empty
    /// (freshly cleared or already empty beforehand).
    QStringList checkEmpty();

    /// Returns the displayName of the row (other than `excludeRow`) that
    /// currently has `tag` selected; used by the combobox popup's item
    /// delegate to render the "(used by …)" suffix.
    /// @return Empty string if no other row owns `tag`.
    QString tagOwnerDisplay(const QString &tag, int excludeRow) const;

signals:
    /// Emitted after the user successfully changes a row's tag (via
    /// onLineEditingFinished(), after any reassignment conflict is resolved).
    void signalMappingChanged(const QString &internalName, const QString &tag);

private slots:
    /// Handles QLineEdit::editingFinished for a row's tag editor: detects
    /// conflicts with tags already owned by another row (prompting a Yes/No
    /// reassignment dialog), reverts on "No", clears the losing row on "Yes",
    /// updates orphan-warning state, and emits signalMappingChanged().
    /// @note No-op while m_suppressEdit guards a programmatic text revert.
    void onLineEditingFinished();

private:
    /// Per-row state backing one line of the table: identity, display text,
    /// value kind, current tag selection, and orphan-warning flag.
    struct RowState {
        QString internalName;   ///< Unique row key used by the owning config.
        QString displayName;    ///< User-facing label shown in column 1.
        Type    type;           ///< Number or Bool; selects the tag list.
        QString currentTag;     ///< Currently selected/entered tag text.
        bool    isWarning{false}; ///< True when currentTag is an orphan (not in the current tag list).
    };

    /// Sets the table's column count/labels, header resize modes, and
    /// selection/edit-trigger behaviour (whole-row select, no direct editing).
    void initHeader();
    /// Returns the row index whose internalName matches, or -1 if not found.
    int  rowIndexOf(const QString &internalName) const;
    /// Rebuilds `row`'s tag editor from its current state: refreshes the
    /// completer's candidate list for its type, sets the displayed text
    /// (without re-triggering editingFinished), and updates its orphan-
    /// warning styling.
    void populateEditor(int row);
    /// Toggles the warning styling (background tint + tooltip) on `row`'s tag
    /// editor and records the flag in m_rows.
    void applyWarning(int row, bool on);
    /// Returns the tag list for `t` (m_boolTags for Type::Bool, m_numberTags
    /// otherwise).
    QStringList tagsFor(Type t) const;
    /// Recomputes the completer popup paint state of every row's editor
    /// (tags used by other rows are greyed, etc.).
    void refreshEditorDelegates();

    QList<RowState> m_rows;        ///< One entry per table row, in display order.
    QStringList     m_numberTags;  ///< Selectable tags for Number-type rows.
    QStringList     m_boolTags;    ///< Selectable tags for Bool-type rows.

    bool m_suppressEdit{false}; ///< Guards re-entrant editingFinished when we revert a QLineEdit value.
};

#endif // SIGNALS_MAP_WIDGET_H
