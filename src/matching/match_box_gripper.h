#ifndef MATCH_BOX_GRIPPER_H
#define MATCH_BOX_GRIPPER_H

#include <opencv2/opencv.hpp>

/// Vision/matching module: MatchBoxGripper, the picking/collision-box geometry for a
/// gripper.
namespace mtc {

/// Picking/collision-box geometry for a gripper: its footprint size and the distance the
/// box is offset from the pick position.
class MatchBoxGripper {
public:
    /// Default-constructs with a zero-sized box and zero distance.
    MatchBoxGripper() {}

public:
    cv::Size2f m_boxSize;    ///< Width/height of the gripper's collision box.
    double m_boxDistance;    ///< Distance the collision box is offset from the pick position.
};

}

#endif // MATCH_BOX_GRIPPER_H
