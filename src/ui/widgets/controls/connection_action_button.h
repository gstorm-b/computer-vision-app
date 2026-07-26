#pragma once

#include <QPushButton>

/// Thin QPushButton subclass (Qt Designer promoted widget) for the connect/
/// disconnect action button; QSS colors it via its `connectionState` dynamic
/// property ("disconnected"/"connected"/"connecting"), set by the owning form.
class ConnectionActionButton : public QPushButton
{
    Q_OBJECT
public:
    /// Constructs the button; behavior is unchanged from QPushButton.
    explicit ConnectionActionButton(QWidget *parent = nullptr);
};
