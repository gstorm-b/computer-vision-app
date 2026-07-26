#pragma once

#include <QLabel>

/// Empty QLabel subclass whose sole purpose is to provide a distinct class
/// name for QSS styling of status text; adds no behavior or members of its
/// own beyond QLabel.
class StatusTextLabel : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; forwards directly to QLabel with no additional setup.
    explicit StatusTextLabel(QWidget *parent = nullptr);
};
