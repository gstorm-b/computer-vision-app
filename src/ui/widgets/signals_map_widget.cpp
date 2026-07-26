#include "signals_map_widget.h"

#include "core/logger/app_logger.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QSignalBlocker>
#include <QStringList>
#include <QStringListModel>
#include <QStyledItemDelegate>

/// Internal helpers for SignalsMapWidget: table column indices, shared
/// property/style-sheet string literals, and the two private widget/delegate
/// classes used by the "Mapped to" column's tag editor.
namespace {
constexpr int kColSignal   = 0;  ///< Column index of the read-only signal display name.
constexpr int kColType     = 1;  ///< Column index of the read-only "Bool"/"Number" type label.
constexpr int kColMapped   = 2;  ///< Column index of the editable tag (TagLineEdit) cell.
constexpr int kColCount    = 3;  ///< Total number of table columns.

constexpr const char *kPropInternalName = "internalName";  ///< Dynamic property key storing a row's internalName on its tag editor.
constexpr const char *kWarningQss =
    "QLineEdit { background: #fff3cd; color: #856404; }";  ///< Style sheet applied to a tag editor flagged as an orphan.

/// QLineEdit subclass that pops the completer's full list on focus, so the
/// user sees what's available without having to type a character first. We
/// also explicitly call complete() rather than relying on the typing event
/// because completionPrefix() may be the existing text — which would filter
/// the popup to a single match instead of showing the full list.
class TagLineEdit : public QLineEdit {
public:
    using QLineEdit::QLineEdit;
protected:
    /// Forces the completer to show its full candidate list (reset to the
    /// current text as prefix) whenever the editor gains keyboard focus.
    void focusInEvent(QFocusEvent *e) override {
        QLineEdit::focusInEvent(e);
        if (auto *c = completer()) {
            c->setCompletionPrefix(text());
            c->complete();
        }
    }
};

/// Delegate for the completer popup: greys out tags already chosen by other
/// rows and appends "(used by …)". The widget that owns the row is queried
/// live so the suffix updates when the user reassigns elsewhere.
class UsedTagDelegate : public QStyledItemDelegate {
public:
    /// Stores the owning SignalsMapWidget and the table row this delegate's
    /// popup belongs to, so paint() can look up conflicting tag owners.
    UsedTagDelegate(SignalsMapWidget *widget, int row, QObject *parent)
        : QStyledItemDelegate(parent), m_widget(widget), m_row(row) {}

    /// Paints a completer popup entry normally unless its tag text is
    /// currently owned by another row, in which case the text is greyed out
    /// and suffixed with "(used by <owner display name>)".
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QString tag = index.data(Qt::DisplayRole).toString();
        if (m_widget && !tag.isEmpty()) {
            const QString owner = m_widget->tagOwnerDisplay(tag, m_row);
            if (!owner.isEmpty()) {
                opt.palette.setColor(QPalette::Text,            QColor(140, 140, 140));
                opt.palette.setColor(QPalette::HighlightedText, QColor(140, 140, 140));
                opt.text = QStringLiteral("%1   (used by %2)").arg(tag, owner);
            }
        }
        QStyledItemDelegate::paint(painter, opt, index);
    }

private:
    QPointer<SignalsMapWidget> m_widget;  ///< Owning widget, queried live for the current tag owner.
    int                        m_row;     ///< Table row (in m_widget) this delegate's popup belongs to.
};
} // namespace

/// Constructs the table with no rows and applies the 3-column header layout
/// via initHeader().
SignalsMapWidget::SignalsMapWidget(QWidget *parent)
    : QTableWidget(parent) {
    initHeader();
}

/// Sets the 3-column header (Signal / Type / Mapped to), hides the vertical
/// header, sets per-column resize modes (Signal and Mapped-to stretch, Type
/// sized to contents), and configures whole-row selection with no direct
/// cell editing.
void SignalsMapWidget::initHeader() {
    QTableWidget::setColumnCount(kColCount);
    setHorizontalHeaderLabels(QStringList{
        tr("Signal"), tr("Type"), tr("Mapped to")
    });
    verticalHeader()->setVisible(false);
    horizontalHeader()->setSectionResizeMode(kColSignal, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(kColType,   QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(kColMapped, QHeaderView::Stretch);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
}

/// Returns the row index whose internalName matches, or -1 if not found.
int SignalsMapWidget::rowIndexOf(const QString &internalName) const {
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].internalName == internalName) return i;
    }
    return -1;
}

/// Returns the selectable tag list for `t` (m_boolTags for Type::Bool,
/// m_numberTags otherwise).
QStringList SignalsMapWidget::tagsFor(Type t) const {
    return t == Type::Bool ? m_boolTags : m_numberTags;
}

/// Rebuilds `row`'s tag editor: refreshes the completer's candidate model
/// from the row's current type, sets the displayed text without re-triggering
/// editingFinished (via QSignalBlocker), and updates orphan-warning styling.
void SignalsMapWidget::populateEditor(int row) {
    if (row < 0 || row >= m_rows.size()) return;
    auto *edit = qobject_cast<QLineEdit *>(cellWidget(row, kColMapped));
    if (!edit) return;

    const RowState &st = m_rows[row];
    const QStringList tags = tagsFor(st.type);

    if (auto *c = edit->completer()) {
        if (auto *m = qobject_cast<QStringListModel *>(c->model())) {
            m->setStringList(tags);
        }
    }

    {
        QSignalBlocker block(edit);
        edit->setText(st.currentTag);
    }
    const bool orphan = !st.currentTag.isEmpty() && !tags.contains(st.currentTag);
    applyWarning(row, orphan);
}

/// Toggles the orphan-warning styling (yellow background + tooltip) on
/// `row`'s tag editor and records the flag in m_rows.
void SignalsMapWidget::applyWarning(int row, bool on) {
    if (row < 0 || row >= m_rows.size()) return;
    m_rows[row].isWarning = on;
    auto *edit = qobject_cast<QLineEdit *>(cellWidget(row, kColMapped));
    if (!edit) return;
    edit->setStyleSheet(on ? QString::fromLatin1(kWarningQss) : QString());
    edit->setToolTip(
        on ? tr("Tag is not in the current comm device's available list. "
                "Pick a valid one or it will be cleared on validation.")
           : QString());
}

/// Appends a new row for `internalName` at the end of the table by
/// delegating to insertRowAt().
void SignalsMapWidget::appendRow(const QString &internalName,
                                 const QString &displayName,
                                 Type type) {
    insertRowAt(m_rows.size(), internalName, displayName, type);
}

/// Inserts a new row for `internalName` at `row` (clamped to the valid
/// range): creates the read-only Signal/Type cells, builds a TagLineEdit with
/// a case-insensitive contains-filter QCompleter (using UsedTagDelegate for
/// its popup), and populates it from the current per-type tag list.
/// @note No-op (with a logged warning) if a row for internalName already exists.
void SignalsMapWidget::insertRowAt(int row,
                                   const QString &internalName,
                                   const QString &displayName,
                                   Type type) {
    if (rowIndexOf(internalName) >= 0) {
        LOG_USER_WARN << "SignalsMapWidget: duplicate internalName" << internalName;
        return;
    }
    row = qBound(0, row, m_rows.size());

    RowState st{internalName, displayName, type, QString(), false};
    m_rows.insert(row, st);

    QTableWidget::insertRow(row);

    auto *cellSignal = new QTableWidgetItem(displayName);
    cellSignal->setFlags(cellSignal->flags() & ~Qt::ItemIsEditable);
    cellSignal->setData(Qt::UserRole, internalName);
    setItem(row, kColSignal, cellSignal);

    auto *cellType = new QTableWidgetItem(type == Type::Bool ? tr("Bool")
                                                             : tr("Number"));
    cellType->setFlags(cellType->flags() & ~Qt::ItemIsEditable);
    cellType->setTextAlignment(Qt::AlignCenter);
    setItem(row, kColType, cellType);

    auto *edit = new TagLineEdit(this);
    edit->setProperty(kPropInternalName, internalName);
    edit->setClearButtonEnabled(true);

    auto *model = new QStringListModel(tagsFor(type), edit);
    auto *completer = new QCompleter(model, edit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->popup()->setItemDelegate(new UsedTagDelegate(this, row, completer));
    edit->setCompleter(completer);

    setCellWidget(row, kColMapped, edit);

    connect(edit, &QLineEdit::editingFinished,
            this, &SignalsMapWidget::onLineEditingFinished);

    populateEditor(row);
}

/// Removes the row identified by `internalName` (and its backing RowState),
/// if one exists.
void SignalsMapWidget::removeRow(const QString &internalName) {
    int row = rowIndexOf(internalName);
    if (row < 0) return;
    m_rows.removeAt(row);
    QTableWidget::removeRow(row);
}

/// Removes every row (and backing RowState) from the table.
void SignalsMapWidget::clearRows() {
    m_rows.clear();
    setRowCount(0);
}

/// Sets the tag text for the row identified by `internalName`, repopulates
/// its editor, and refreshes orphan/conflict styling across all rows.
void SignalsMapWidget::setRowValue(const QString &internalName,
                                   const QString &tag) {
    int row = rowIndexOf(internalName);
    if (row < 0) return;
    m_rows[row].currentTag = tag;
    populateEditor(row);
    refreshEditorDelegates();
}

/// Returns the currently selected tag for the row identified by
/// `internalName`, or an empty string if the row does not exist.
QString SignalsMapWidget::rowValue(const QString &internalName) const {
    int row = rowIndexOf(internalName);
    return row < 0 ? QString() : m_rows[row].currentTag;
}

/// Replaces the selectable tag list for Number-type rows and repopulates
/// every Number row's editor (which recomputes each row's orphan state).
void SignalsMapWidget::setNumberTags(const QStringList &tags) {
    m_numberTags = tags;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].type == Type::Number) populateEditor(i);
    }
    refreshEditorDelegates();
}

/// Replaces the selectable tag list for Bool-type rows and repopulates every
/// Bool row's editor (which recomputes each row's orphan state).
void SignalsMapWidget::setBoolTags(const QStringList &tags) {
    m_boolTags = tags;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].type == Type::Bool) populateEditor(i);
    }
    refreshEditorDelegates();
}

/// Clears the tag of every warning-flagged (orphaned) row to "" and emits
/// signalMappingChanged() for each one cleared.
/// @return internalNames of every row whose current tag is empty after this
/// call (freshly cleared or already empty beforehand).
QStringList SignalsMapWidget::checkEmpty() {
    QStringList emptyNames;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].isWarning) {
            m_rows[i].currentTag.clear();
            populateEditor(i);
            emit signalMappingChanged(m_rows[i].internalName, QString());
        }
        if (m_rows[i].currentTag.isEmpty()) {
            emptyNames << m_rows[i].internalName;
        }
    }
    return emptyNames;
}

/// Handles a tag editor's editingFinished: if the new tag is already owned by
/// another row, prompts a Yes/No reassignment dialog (reverting the text on
/// "No", or clearing/warning the losing row on "Yes"); otherwise commits the
/// new tag, updates orphan-warning state, and emits signalMappingChanged().
/// @note No-op while m_suppressEdit guards a programmatic text revert.
void SignalsMapWidget::onLineEditingFinished() {
    if (m_suppressEdit) return;
    auto *edit = qobject_cast<QLineEdit *>(sender());
    if (!edit) return;
    const QString internalName = edit->property(kPropInternalName).toString();
    int row = rowIndexOf(internalName);
    if (row < 0) return;

    const QString newTag = edit->text().trimmed();
    const QString oldTag = m_rows[row].currentTag;
    if (newTag == oldTag) return;

    // Conflict check applies only to non-empty selections.
    if (!newTag.isEmpty()) {
        const QString ownerDisplay = tagOwnerDisplay(newTag, row);
        if (!ownerDisplay.isEmpty()) {
            QMessageBox::StandardButton btn = QMessageBox::question(
                this, tr("Reassign tag"),
                tr("Tag \"%1\" is currently mapped to \"%2\".\n"
                   "Reassign it to \"%3\"?")
                    .arg(newTag, ownerDisplay, m_rows[row].displayName),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (btn != QMessageBox::Yes) {
                m_suppressEdit = true;
                edit->setText(oldTag);
                m_suppressEdit = false;
                return;
            }
            for (int j = 0; j < m_rows.size(); ++j) {
                if (j == row) continue;
                if (m_rows[j].currentTag == newTag) {
                    m_rows[j].currentTag.clear();
                    populateEditor(j);
                    applyWarning(j, true);
                    emit signalMappingChanged(m_rows[j].internalName, QString());
                }
            }
        }
    }

    m_rows[row].currentTag = newTag;
    // Free-typed values that aren't in the current list become orphan-warning;
    // checkEmpty() will purge them at validation time.
    const QStringList tags = tagsFor(m_rows[row].type);
    const bool orphan = !newTag.isEmpty() && !tags.contains(newTag);
    applyWarning(row, orphan);

    refreshEditorDelegates();
    emit signalMappingChanged(internalName, newTag);
}

/// Returns the displayName of the row (other than `excludeRow`) that
/// currently has `tag` selected, or an empty string if no other row owns it.
QString SignalsMapWidget::tagOwnerDisplay(const QString &tag,
                                          int excludeRow) const {
    if (tag.isEmpty()) return {};
    for (int i = 0; i < m_rows.size(); ++i) {
        if (i == excludeRow) continue;
        if (m_rows[i].currentTag == tag) return m_rows[i].displayName;
    }
    return {};
}

/// Forces every row's completer popup to repaint, so UsedTagDelegate picks up
/// any change in which rows currently own which tags.
void SignalsMapWidget::refreshEditorDelegates() {
    for (int i = 0; i < m_rows.size(); ++i) {
        auto *edit = qobject_cast<QLineEdit *>(cellWidget(i, kColMapped));
        if (!edit) continue;
        if (auto *c = edit->completer()) {
            if (c->popup()) c->popup()->update();
        }
    }
}
