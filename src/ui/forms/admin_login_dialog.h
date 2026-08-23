#ifndef ADMIN_LOGIN_DIALOG_H
#define ADMIN_LOGIN_DIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class AdminLoginDialog;
}

/**
 * @file admin_login_dialog.h
 * @brief AdminLoginDialog — asks for the administrator password and elevates the process
 *        role on success.
 */

/**
 * @class AdminLoginDialog
 * @brief Modal password prompt that elevates `vc::auth::AccessControl` to Admin.
 *
 * Lives in `src/ui` because both shells need it: the commissioning app for its Access Level
 * menu, and the operator runtime before it will hand off into commissioning. The two shells
 * are peers and may not include each other's headers.
 *
 * The dialog only accepts when the password verifies, so a caller can treat
 * `exec() == Accepted` as "the process is now Admin" without re-checking. A wrong password
 * keeps the dialog open with the field cleared and focused rather than closing and making
 * the user reopen it.
 *
 * @note It reports "incorrect password" and nothing more — never whether a credential is
 *       configured, how long it is, or how many attempts remain. There is no lockout: this
 *       gate exists to stop a mis-tap and casual curiosity on a machine the operator
 *       already has physical access to, and a lockout on such a machine mostly succeeds at
 *       stranding the person who is allowed in.
 */
class AdminLoginDialog : public QDialog {
    Q_OBJECT

public:
    /// Builds the dialog and wires OK/Cancel.
    explicit AdminLoginDialog(QWidget *parent = nullptr);
    ~AdminLoginDialog() override;

    /**
     * @brief Convenience: ensures the process is Admin, prompting only if it is not.
     *
     * The common caller shape — "I need Admin for this action" — expressed once so every
     * caller does not re-derive it and get the already-Admin case subtly different.
     *
     * @param[in] parent parent widget for the dialog
     * @return true if the process is Admin when this returns
     */
    static bool ensureAdmin(QWidget *parent);

private slots:
    /// Verifies the entered password; accepts on success, shows the error and stays open otherwise.
    void onAccept();

private:
    /// Shows `message` in the error label, or hides the label when `message` is empty.
    void showError(const QString &message);

    Ui::AdminLoginDialog *ui;  ///< Generated UI form; owns the prompt, password field, error label and buttons.
};

#endif // ADMIN_LOGIN_DIALOG_H
