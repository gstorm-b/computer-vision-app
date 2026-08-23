#pragma once

#include <QFrame>

/**
 * @file status_lamp_dot.h
 * @brief StatusLampDot — empty QFrame subclass providing a distinct class name
 *        for QSS selectors and Designer widget promotion.
 */

/**
 * @class StatusLampDot
 * @brief Empty QFrame subclass whose sole purpose is to provide a distinct class
 *        name for QSS selectors and Designer widget promotion; adds no behavior or
 *        members of its own beyond QFrame.
 */
class StatusLampDot : public QFrame
{
    Q_OBJECT
public:
    /// Constructs the dot frame; forwards directly to QFrame with no additional setup.
    explicit StatusLampDot(QWidget *parent = nullptr);
};
