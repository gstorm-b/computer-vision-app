#pragma once

#include <QFrame>

/// Thin QFrame subclass used as a general-purpose styled panel container;
/// QSS targets it directly and via its `frameBoxVariant` property (e.g.
/// "compact" for a smaller border radius).
class FrameBox : public QFrame
{
    Q_OBJECT
public:
    /// Constructs the frame with no native border (NoFrame shape, Plain
    /// shadow) so its appearance comes entirely from QSS.
    explicit FrameBox(QWidget *parent = nullptr);
};
