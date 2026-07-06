#pragma once

#include <QLabel>

class StatusTextLabel : public QLabel
{
    Q_OBJECT
public:
    explicit StatusTextLabel(QWidget *parent = nullptr);
};
