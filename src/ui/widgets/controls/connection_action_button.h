#pragma once

#include <QPushButton>

/**
 * @file connection_action_button.h
 * @brief ConnectionActionButton — thin QPushButton subclass for the connect/
 *        disconnect action button, QSS-styled via a `connectionState` dynamic property.
 */

/**
 * @class ConnectionActionButton
 * @brief Thin QPushButton subclass (Qt Designer promoted widget) for the connect/
 *        disconnect action button; QSS colors it via its `connectionState` dynamic
 *        property ("disconnected"/"connected"/"connecting"), set by the owning form.
 */
class ConnectionActionButton : public QPushButton
{
    Q_OBJECT
public:
    /// Constructs the button; behavior is unchanged from QPushButton.
    explicit ConnectionActionButton(QWidget *parent = nullptr);
};
