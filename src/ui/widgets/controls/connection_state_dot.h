#pragma once

#include <QLabel>

/**
 * @file connection_state_dot.h
 * @brief ConnectionStateDot — thin QLabel subclass used as a small connection-status
 *        dot, QSS-styled via a `connectionState` dynamic property.
 */

/**
 * @class ConnectionStateDot
 * @brief Thin QLabel subclass (Qt Designer promoted widget) used as a small
 *        connection-status dot; QSS colors it via its `connectionState` dynamic
 *        property ("connected"/"connecting"/"disconnected"), set by the owning form.
 */
class ConnectionStateDot : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; behavior is unchanged from QLabel.
    explicit ConnectionStateDot(QWidget *parent = nullptr);
};
