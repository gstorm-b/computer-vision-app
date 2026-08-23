#pragma once

#include <QListWidget>

/**
 * @file flat_list_widget.h
 * @brief FlatListWidget — thin QListWidget subclass for flat, borderless row-based lists.
 */

/**
 * @class FlatListWidget
 * @brief Thin QListWidget subclass giving flat (borderless, no hover/selection
 *        highlight) row-based lists a distinct class name for QSS selectors; used
 *        as the base for widgets like CameraMappingWidget.
 */
class FlatListWidget : public QListWidget
{
    Q_OBJECT
public:
    /// Constructs the list widget; behavior is unchanged from QListWidget.
    explicit FlatListWidget(QWidget *parent = nullptr);
};
