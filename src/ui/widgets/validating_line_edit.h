#ifndef VALIDATING_LINE_EDIT_H
#define VALIDATING_LINE_EDIT_H

#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>
#include <QDebug>

/// QLineEdit that only accepts text matching an entry in its completer's
/// string-list model: when editing finishes, any text not present in that
/// list is silently cleared.
class ValidatingLineEdit : public QLineEdit {
    Q_OBJECT

public:
    /// Constructs the edit with no completer set and connects
    /// editingFinished to validateInput().
    explicit ValidatingLineEdit(QWidget *parent = nullptr)
        : QLineEdit(parent), m_completer(nullptr)
    {
        connect(this, &QLineEdit::editingFinished,
                this, &ValidatingLineEdit::validateInput);
    }

    /// Sets the completer used both for input suggestions and, via its
    /// QStringListModel, as the allowed-value whitelist checked by
    /// validateInput().
    void setCompleter(QCompleter *completer) {
        m_completer = completer;
        QLineEdit::setCompleter(completer);
    }

private slots:
    /// Clears the current text if it does not exactly match an entry in
    /// m_completer's QStringListModel; no-op if no completer is set or its
    /// model isn't a QStringListModel.
    void validateInput() {
        if (!m_completer) return;

        QString currentText = text();
        auto *model = qobject_cast<QStringListModel*>(m_completer->model());
        if (model) {
            if (!model->stringList().contains(currentText)) {
                clear();
            }
        }
    }

private:
    QCompleter *m_completer;  ///< Completer whose string-list model is also used as the allowed-value whitelist.
};

#endif // VALIDATING_LINE_EDIT_H
