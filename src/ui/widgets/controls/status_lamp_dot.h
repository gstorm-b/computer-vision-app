#pragma once

#include <QFrame>

/// Empty QFrame subclass whose sole purpose is to provide a distinct class
/// name for QSS selectors and Designer widget promotion; adds no behavior or
/// members of its own beyond QFrame.
class StatusLampDot : public QFrame
{
    Q_OBJECT
public:
    /// Constructs the dot frame; forwards directly to QFrame with no additional setup.
    explicit StatusLampDot(QWidget *parent = nullptr);
};
