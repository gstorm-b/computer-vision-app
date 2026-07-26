#pragma once

#include <QLabel>

/// Thin QLabel subclass (Qt Designer promoted widget) used as a small
/// connection-status dot; QSS colors it via its `connectionState` dynamic
/// property ("connected"/"connecting"/"disconnected"), set by the owning form.
class ConnectionStateDot : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; behavior is unchanged from QLabel.
    explicit ConnectionStateDot(QWidget *parent = nullptr);
};
