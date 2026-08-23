#pragma once
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @file status_lamp.h
 * @brief StatusLamp — read-only, severity-coloured state indicator for operator dashboards.
 */

/**
 * @class StatusLamp
 * @brief Read-only status lamp widget: stacks an uppercase name label above a
 *        severity-coloured state label inside a QFrame, styled via the `status`
 *        dynamic property (see StatusLamp::Status) so QSS can target it directly.
 *
 * Thin QFrame subclass. The stable class name allows QSS selectors like
 * `StatusLamp[status="ok"] { border-left-color: #40c870; }` to apply uniformly
 * to all lamp instances without per-objectName rules.
 *
 * Layout:
 * @code
 *   ┌──────────────┐
 *   │  TASK        │  <- lampPart="name"  (small, muted, uppercase)
 *   │  Ready       │  <- lampPart="state" (larger, bold, severity-coloured)
 *   └──────────────┘
 * @endcode
 *
 * Usage (from host cpp):
 * @code
 *   ui->lamp_task->setLampName("Task");
 *   ui->lamp_task->setStatus(StatusLamp::Status::Ok, tr("Ready"));
 * @endcode
 */
class StatusLamp : public QFrame
{
    Q_OBJECT
public:
    /**
     * @enum Status
     * @brief Severity levels for the lamp; mapped to the QSS `status` property
     *        string ("ok"/"warning"/"error"/"off") by statusProperty().
     */
    enum class Status { Ok, Warning, Error, Off };
    Q_ENUM(Status)

    /**
     * @brief Constructs the lamp with the name/state labels stacked in a
     *        QVBoxLayout and the status initialized to Off.
     * @param[in] parent Owning widget; standard Qt parent/child ownership.
     */
    explicit StatusLamp(QWidget *parent = nullptr);

    /// Set the fixed name displayed above the state text (shown in UPPER CASE).
    void setLampName(const QString &name);

    /**
     * @brief Update the status property (triggers QSS repolish) and state text.
     * @param[in] status    New severity level; written to the `status` QSS property.
     * @param[in] stateText New text for the state label; leave empty to clear it.
     */
    void setStatus(Status status, const QString &stateText = QString());

    /// Returns the current severity status.
    Status  status()    const { return m_status; }
    /// Returns the text currently shown in the name label (already upper-cased).
    QString lampName()  const { return m_nameLabel->text(); }
    /// Returns the text currently shown in the state label.
    QString stateText() const { return m_stateLabel->text(); }

private:
    Status  m_status    = Status::Off;   ///< Current severity level; default Off.
    QLabel *m_nameLabel  = nullptr;       ///< Small muted label showing the uppercased lamp name.
    QLabel *m_stateLabel = nullptr;       ///< Larger bold label showing the severity-coloured state text.

    /// Maps `s` to the QSS `status` property string ("ok"/"warning"/"error"/"off").
    static const char *statusProperty(Status s);
};
