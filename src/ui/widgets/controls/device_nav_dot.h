#pragma once

#include <QFrame>

/**
 * @file device_nav_dot.h
 * @brief DeviceNavDot — thin QFrame subclass for the small connect-status indicator
 *        dot inside DeviceNavItemWidget.
 */

/**
 * @class DeviceNavDot
 * @brief Thin QFrame subclass giving the small connect-status indicator dot inside
 *        DeviceNavItemWidget a distinct class name for QSS selectors, driven by its
 *        `lampState` dynamic property ("on"/"warn"/"off").
 */
class DeviceNavDot : public QFrame
{
    Q_OBJECT
public:
    /// Constructs the frame; behavior is unchanged from QFrame.
    explicit DeviceNavDot(QWidget *parent = nullptr);
};
