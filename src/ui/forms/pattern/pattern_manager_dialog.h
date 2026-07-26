#ifndef PATTERN_MANAGER_DIALOG_H
#define PATTERN_MANAGER_DIALOG_H

#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QValidator>
#include <QMessageBox>
#include <QDebug>

/// Minimum allowed value for a pattern group activation index.
#define PT_GROUP_INDEX_MIN  1
/// Maximum allowed value for a pattern group activation index.
#define PT_GROUP_INDEX_MAX  32

/// Minimum allowed value for a pattern activation index.
#define PT_PATTERN_INDEX_MIN  1
/// Maximum allowed value for a pattern activation index.
#define PT_PATTERN_INDEX_MAX  4

/// Modal dialog for creating/editing a pattern group's name and activation index (1-32),
/// validating both fields before accepting.
class AddGroupDialog : public QDialog {
    Q_OBJECT
public:
    /// Builds the form (name + index fields, OK/Cancel buttons) and wires the OK button to
    /// validateAndAccept().
    /// @param dialog_title window title shown on the dialog
    explicit AddGroupDialog(QString &dialog_title, QWidget *parent = nullptr) : QDialog(parent) {
        this->setWindowTitle(dialog_title);

        QFormLayout *formLayout = new QFormLayout(this);

        groupNameEdit = new QLineEdit(this);
        formLayout->addRow(tr("Group Name:"), groupNameEdit);

        groupIndexEdit = new QLineEdit(this);
        QIntValidator *validator = new QIntValidator(PT_GROUP_INDEX_MIN, PT_GROUP_INDEX_MAX, this);
        groupIndexEdit->setValidator(validator);
        formLayout->addRow(tr("Group activation index:"), groupIndexEdit);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        formLayout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &AddGroupDialog::validateAndAccept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    /// Sets the text of the group-name field.
    inline void setGroupName(QString &name) const {
        groupNameEdit->setText(name);
    }

    /// Returns the current text of the group-name field.
    inline QString getGroupName() const {
        return groupNameEdit->text();
    }

    /// Sets the group-index field to `index` (formatted as a number); no-op if the range check
    /// below fails.
    inline void setGroupIndex(int index) const {
        if ((index < PT_GROUP_INDEX_MIN) && (index >PT_GROUP_INDEX_MAX)) {
            return;
        }
        groupIndexEdit->setText(QString::number(index));
    }

    /// Returns the group-index field's text parsed as an integer.
    inline int getGroupIndex() const {
        return groupIndexEdit->text().toInt();
    }

private slots:
    /// Validates that the name is non-empty and the index is a non-empty value within
    /// [PT_GROUP_INDEX_MIN, PT_GROUP_INDEX_MAX], showing a warning message box and returning
    /// early on failure; calls accept() once both checks pass.
    void validateAndAccept() {
        if (getGroupName().isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Name"), tr("Pattern group name cannot empty."));
            return;
        }

        if (groupIndexEdit->text().isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Index"), tr("Group activation index cannot empty."));
            return;
        }

        int index = groupIndexEdit->text().toInt();
        if (index < PT_GROUP_INDEX_MIN || index > PT_GROUP_INDEX_MAX) {
            QMessageBox::warning(this, tr("Invalid Index"), tr("Group activation index must be between %1 and %2.").
                                                            arg(PT_GROUP_INDEX_MIN).
                                                            arg(PT_GROUP_INDEX_MAX));
            return;
        }
        accept();
    }

private:
    QLineEdit *groupNameEdit;   ///< Input field for the group's display name.
    QLineEdit *groupIndexEdit;  ///< Input field (int-validated) for the group's activation index.
};

/// Modal dialog for creating/editing a pattern's name and activation index (1-4), validating
/// both fields before accepting.
class AddPatternDialog : public QDialog {
    Q_OBJECT
public:
    /// Builds the form (name + index fields, OK/Cancel buttons) and wires the OK button to
    /// validateAndAccept().
    /// @param dialog_title window title shown on the dialog
    explicit AddPatternDialog(QString &dialog_title, QWidget *parent = nullptr) : QDialog(parent) {
        this->setWindowTitle(dialog_title);

        QFormLayout *formLayout = new QFormLayout(this);

        patternNameEdit = new QLineEdit(this);
        formLayout->addRow(tr("Pattern Name:"), patternNameEdit);

        patternIndexEdit = new QLineEdit(this);
        QIntValidator *validator = new QIntValidator(PT_PATTERN_INDEX_MIN, PT_PATTERN_INDEX_MAX, this);
        patternIndexEdit->setValidator(validator);
        formLayout->addRow(tr("Pattern activation index:"), patternIndexEdit);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        formLayout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &AddPatternDialog::validateAndAccept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    /// Sets the text of the pattern-name field.
    inline void setPatternName(QString &name) const {
        patternNameEdit->setText(name);
    }

    /// Returns the current text of the pattern-name field.
    inline QString getPatternName() const {
        return patternNameEdit->text();
    }

    /// Sets the pattern-index field to `index` (formatted as a number); no-op if the range check
    /// below fails.
    inline void setPatternIndex(int index) const {
        if ((index < PT_PATTERN_INDEX_MIN) && (index >PT_PATTERN_INDEX_MAX)) {
            return;
        }
        patternIndexEdit->setText(QString::number(index));
    }

    /// Returns the pattern-index field's text parsed as an integer.
    inline int getPatternIndex() const {
        return patternIndexEdit->text().toInt();
    }

private slots:
    /// Validates that the name is non-empty and the index is a non-empty value within
    /// [PT_PATTERN_INDEX_MIN, PT_PATTERN_INDEX_MAX], showing a warning message box and returning
    /// early on failure; calls accept() once both checks pass.
    void validateAndAccept() {
        if (patternNameEdit->text().isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Name"), tr("Pattern name cannot empty."));
            return;
        }

        if (patternIndexEdit->text().isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Index"), tr("Pattern activation index cannot empty."));
            return;
        }

        int index = patternIndexEdit->text().toInt();
        if (index < PT_PATTERN_INDEX_MIN || index > PT_PATTERN_INDEX_MAX) {
            QMessageBox::warning(this, tr("Invalid Index"), tr("Pattern activation index must be between %1 and %2.").
                                                            arg(PT_PATTERN_INDEX_MIN).
                                                            arg(PT_PATTERN_INDEX_MAX));
            return;
        }
        accept();
    }

private:
    QLineEdit *patternNameEdit;   ///< Input field for the pattern's display name.
    QLineEdit *patternIndexEdit;  ///< Input field (int-validated) for the pattern's activation index.
};

// class CMenu : public QMenu {
//     Q_OBJECT
// public:
//     explicit CMenu(QWidget *parent = nullptr) : QMenu(parent) {
//         this->installEventFilter(this);
//     }

// protected:
//     // void mousePressEvent(QMouseEvent *event) override {
//     //     if (event->button() == Qt::LeftButton) {
//     //         QMenu::mousePressEvent(event);
//     //     } else {
//     //         event->ignore();
//     //     }
//     // }

//     bool eventFilter(QObject *watched, QEvent *event) override {
//         if (watched == this && event->type() == QEvent::MouseButtonPress) {
//             QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
//             if (mouseEvent->button() == Qt::LeftButton) {
//                 return QMenu::eventFilter(watched, event);
//             } else {
//                 return true;
//             }
//         }
//         if (watched == this && event->type() == QEvent::MouseButtonDblClick) {
//             return true;
//         }
//         return QMenu::eventFilter(watched, event);
//     }
// };

#endif // PATTERN_MANAGER_DIALOG_H
