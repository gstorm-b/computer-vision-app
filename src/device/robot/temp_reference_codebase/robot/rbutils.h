#ifndef RBUTILS1_H
#define RBUTILS1_H

#include <QMutex>

#define RB_MUTEX_LOCK_TIME  50

// #define RB_LOCK_DATA           m_data_locker.tryLock(RB_MUTEX_LOCK_TIME)
#define RB_LOCK_DATA           m_data_locker.lock()
#define RB_UNLOCK_DATA         m_data_locker.unlock()

namespace rb {

enum RbMotionType {
    mtLinear = 0,
    mtJoint
};

enum RbDirection {
    dirBackward = 0,
    dirToward
};

static int RbDirectionToInt(RbDirection direction) {
    if (direction == RbDirection::dirToward) {
        return 1;
    } else if (direction == RbDirection::dirBackward) {
        return 0;
    }
    return -1;
}

enum RbAxis {
    axisX = 0,
    axisY,
    axisZ,
    axisRX,
    axisRY,
    axisRZ,
    axisR
};

static int RbAxisToID(RbAxis axis) {
    switch (axis) {
    case RbAxis::axisX:
        return 0;
    case RbAxis::axisY:
        return 1;
    case RbAxis::axisZ:
        return 2;
    case RbAxis::axisRX:
        return 3;
    case RbAxis::axisRY:
        return 4;
    case RbAxis::axisRZ:
        return 5;
    case RbAxis::axisR:
        return 5;
    }
    return -1;
}

class RbAbstractFeedBack {
public:
    virtual ~RbAbstractFeedBack() = default;
};

}

#endif // RBUTILS1_H
