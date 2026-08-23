#pragma once

#include <QFrame>

/**
 * @file frame_box.h
 * @brief FrameBox — thin QFrame subclass used as a general-purpose styled panel container.
 */

/**
 * @class FrameBox
 * @brief Thin QFrame subclass used as a general-purpose styled panel container;
 *        QSS targets it directly and via its `frameBoxVariant` property (e.g.
 *        "compact" for a smaller border radius).
 */
class FrameBox : public QFrame
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the frame with no native border (NoFrame shape, Plain
     *        shadow) so its appearance comes entirely from QSS.
     * @param[in] parent Owning widget; standard Qt parent/child ownership.
     */
    explicit FrameBox(QWidget *parent = nullptr);
};
