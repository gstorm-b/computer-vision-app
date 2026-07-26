#include "frame_box.h"

/// Constructs the frame with no native border (NoFrame shape, Plain shadow)
/// so its appearance comes entirely from QSS.
FrameBox::FrameBox(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
}
