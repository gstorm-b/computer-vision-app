#pragma once

#include <QLabel>

/// Thin QLabel subclass (Qt Designer promoted widget) used for the textual
/// connection-state readout (e.g. "DISCONNECTED"); QSS colors it via its
/// `connectionState` dynamic property ("connected"/"connecting"/"disconnected"),
/// set by the owning form.
class ConnectionStateLabel : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; behavior is unchanged from QLabel.
    explicit ConnectionStateLabel(QWidget *parent = nullptr);
};
