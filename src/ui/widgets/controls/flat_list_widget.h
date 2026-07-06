#pragma once

#include <QListWidget>

class FlatListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit FlatListWidget(QWidget *parent = nullptr);
};
