#pragma once

#include <QLabel>

class ConnectionStateLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ConnectionStateLabel(QWidget *parent = nullptr);
};
