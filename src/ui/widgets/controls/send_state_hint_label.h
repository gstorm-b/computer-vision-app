#pragma once

#include <QLabel>

/// Thin QLabel subclass (Qt Designer promoted widget) used as a send-status
/// hint; QSS colors it via its `sendState` dynamic property ("ready"/"idle"),
/// set by the owning form.
class SendStateHintLabel : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; behavior is unchanged from QLabel.
    explicit SendStateHintLabel(QWidget *parent = nullptr);
};
