#pragma once

#include <QLabel>

/**
 * @file type_chip_label.h
 * @brief TypeChipLabel — empty QLabel subclass providing a distinct class name
 *        for QSS styling of "type chip" badges.
 */

/**
 * @class TypeChipLabel
 * @brief Empty QLabel subclass whose sole purpose is to provide a distinct class
 *        name for QSS styling of "type chip" badges; adds no behavior or members
 *        of its own beyond QLabel.
 */
class TypeChipLabel : public QLabel
{
    Q_OBJECT
public:
    /// Constructs the label; forwards directly to QLabel with no additional setup.
    explicit TypeChipLabel(QWidget *parent = nullptr);
};
