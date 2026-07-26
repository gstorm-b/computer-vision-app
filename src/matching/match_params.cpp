#include "match_params.h"

namespace mtc {

/// Default-constructs with a zero match score and angle; the remaining fields are left
/// uninitialized until populated by the matcher.
MatchParams::MatchParams() {
    _matchScore = 0.0;
    _matchAngle = 0.0;
}

/// Constructs a match result at `ptMinMax` with the given `score` and `angle`; initializes
/// _delete, _newAngle, and _posOnBorder to their defaults (other fields are left
/// uninitialized).
MatchParams::MatchParams(cv::Point2f ptMinMax, double score, double angle) {
    _point = ptMinMax;
    _matchScore = score;
    _matchAngle = angle;

    _delete = false;
    _newAngle = 0.0;

    _posOnBorder = false;
}

/// Default destructor.
MatchParams::~MatchParams() {

}

}
