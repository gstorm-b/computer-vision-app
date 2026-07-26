#pragma once

#include <QLabel>

/// Thin QLabel subclass (Qt Designer promoted widget) rendered as a pill-shaped
/// on/off indicator; QSS styles it via its `onOffState` dynamic property
/// ("on"/"off"), set by the owning form.
class StatePillLabel : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; behavior is unchanged from QLabel.
    explicit StatePillLabel(QWidget *parent = nullptr);
};
