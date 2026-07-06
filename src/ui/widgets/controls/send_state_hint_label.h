#pragma once

#include <QLabel>

class SendStateHintLabel : public QLabel
{
    Q_OBJECT
public:
    explicit SendStateHintLabel(QWidget *parent = nullptr);
};
