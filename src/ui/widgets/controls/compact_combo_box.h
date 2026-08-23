#pragma once

#include <QComboBox>

/**
 * @file compact_combo_box.h
 * @brief CompactComboBox — thin QComboBox subclass giving compact combo boxes a
 *        distinct class name for QSS selectors.
 */

/**
 * @class CompactComboBox
 * @brief Thin QComboBox subclass (Qt Designer promoted widget) giving compact combo
 *        boxes a distinct class name for QSS selectors, e.g. `CompactComboBox::drop-down`
 *        to hide the drop-down border.
 */
class CompactComboBox : public QComboBox
{
    Q_OBJECT
public:
    /// Constructs the combo box; behavior is unchanged from QComboBox.
    explicit CompactComboBox(QWidget *parent = nullptr);
};
