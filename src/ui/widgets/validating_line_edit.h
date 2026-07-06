#ifndef VALIDATING_LINE_EDIT_H
#define VALIDATING_LINE_EDIT_H

#include <QLineEdit>
#include <QCompleter>
#include <QStringListModel>
#include <QDebug>

class ValidatingLineEdit : public QLineEdit {
    Q_OBJECT

public:
    explicit ValidatingLineEdit(QWidget *parent = nullptr)
        : QLineEdit(parent), m_completer(nullptr)
    {
        connect(this, &QLineEdit::editingFinished,
                this, &ValidatingLineEdit::validateInput);
    }

    void setCompleter(QCompleter *completer) {
        m_completer = completer;
        QLineEdit::setCompleter(completer);
    }

private slots:
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
    QCompleter *m_completer;
};

#endif // VALIDATING_LINE_EDIT_H
