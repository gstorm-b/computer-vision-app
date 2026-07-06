#include "frame_box.h"

FrameBox::FrameBox(QWidget *parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
}
