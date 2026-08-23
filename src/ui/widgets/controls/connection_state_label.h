#pragma once

#include <QLabel>

/**
 * @file connection_state_label.h
 * @brief ConnectionStateLabel — thin QLabel subclass for the textual connection-state
 *        readout, QSS-styled via a `connectionState` dynamic property.
 */

/**
 * @class ConnectionStateLabel
 * @brief Thin QLabel subclass (Qt Designer promoted widget) used for the textual
 *        connection-state readout (e.g. "DISCONNECTED"); QSS colors it via its
 *        `connectionState` dynamic property ("connected"/"connecting"/"disconnected"),
 *        set by the owning form.
 */
class ConnectionStateLabel : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; behavior is unchanged from QLabel.
    explicit ConnectionStateLabel(QWidget *parent = nullptr);
};
