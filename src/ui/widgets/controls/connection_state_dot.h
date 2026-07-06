#pragma once

#include <QLabel>

class ConnectionStateDot : public QLabel
{
    Q_OBJECT
public:
    explicit ConnectionStateDot(QWidget *parent = nullptr);
};
