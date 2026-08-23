#pragma once

#include <QLabel>

/**
 * @file state_pill_label.h
 * @brief StatePillLabel — thin QLabel subclass rendered as a pill-shaped on/off indicator.
 */

/**
 * @class StatePillLabel
 * @brief Thin QLabel subclass (Qt Designer promoted widget) rendered as a pill-shaped
 *        on/off indicator; QSS styles it via its `onOffState` dynamic property
 *        ("on"/"off"), set by the owning form.
 */
class StatePillLabel : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; behavior is unchanged from QLabel.
    explicit StatePillLabel(QWidget *parent = nullptr);
};
