#pragma once

#include <QLabel>

/**
 * @file send_state_hint_label.h
 * @brief SendStateHintLabel — thin QLabel subclass used as a send-status hint,
 *        QSS-styled via a `sendState` dynamic property.
 */

/**
 * @class SendStateHintLabel
 * @brief Thin QLabel subclass (Qt Designer promoted widget) used as a send-status
 *        hint; QSS colors it via its `sendState` dynamic property ("ready"/"idle"),
 *        set by the owning form.
 */
class SendStateHintLabel : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; behavior is unchanged from QLabel.
    explicit SendStateHintLabel(QWidget *parent = nullptr);
};
