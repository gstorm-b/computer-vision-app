#include "ui/forms/admin_login_dialog.h"
#include "ui_admin_login_dialog.h"

#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

#include "core/auth/access_control.h"

using vc::auth::AccessControl;

/// Builds the dialog, wires the buttons, and puts the cursor in the password field.
AdminLoginDialog::AdminLoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AdminLoginDialog)
{
    ui->setupUi(this);

    connect(ui->btn_ok,     &QPushButton::clicked, this, &AdminLoginDialog::onAccept);
    connect(ui->btn_cancel, &QPushButton::clicked, this, &AdminLoginDialog::reject);

    // Enter submits. The field is the only input, so making the user reach for the mouse
    // to confirm a password is friction with nothing behind it.
    connect(ui->ledit_password, &QLineEdit::returnPressed, this, &AdminLoginDialog::onAccept);

    showError(QString());
    ui->ledit_password->setFocus();
}

AdminLoginDialog::~AdminLoginDialog()
{
    delete ui;
}

/// Ensures the process is Admin, prompting only when it is not already.
bool AdminLoginDialog::ensureAdmin(QWidget *parent)
{
    AccessControl *access = AccessControl::instance();
    if (access->isAdmin()) {
        return true;
    }

    if (!access->canElevate()) {
        // No credential source. Say so instead of showing a dialog whose only possible
        // outcome is failure — an operator retyping a password against a gate that cannot
        // open is the worst version of this.
        QMessageBox::warning(
            parent,
            tr("Administrator Access"),
            tr("No administrator credential is available on this machine, so access "
               "cannot be granted."));
        return false;
    }

    AdminLoginDialog dialog(parent);
    return dialog.exec() == QDialog::Accepted;
}

/// Verifies the password and accepts only on success.
void AdminLoginDialog::onAccept()
{
    const QString password = ui->ledit_password->text();

    if (password.isEmpty()) {
        showError(tr("Enter the administrator password."));
        return;
    }

    if (!AccessControl::instance()->elevate(password)) {
        // Stay open with the field cleared: a wrong password is usually a typo, and closing
        // the dialog would make the user reopen it to try again.
        showError(tr("Incorrect password."));
        ui->ledit_password->clear();
        ui->ledit_password->setFocus();
        return;
    }

    accept();
}

/// Shows `message` in the error label, hiding the label when there is nothing to say.
void AdminLoginDialog::showError(const QString &message)
{
    ui->lbl_error->setText(message);
    ui->lbl_error->setVisible(!message.isEmpty());
}
