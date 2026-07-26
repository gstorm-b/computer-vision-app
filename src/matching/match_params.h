#ifndef MATCH_PARAMS_H
#define MATCH_PARAMS_H

#include <opencv2/core.hpp>

/// Vision/matching module: MatchParams, the per-candidate result data produced during
/// pattern search.
namespace mtc {

/// Result data for a single match candidate produced during pattern search: its
/// location/score/angle, the search ROI and rotated/bounding geometry, and the
/// sub-pixel refinement fields filled in by the fine-search stage.
class MatchParams {
public:
    /// Default-constructs with a zero match score and angle; the remaining fields are
    /// left uninitialized until populated by the matcher.
    MatchParams();
    /// Constructs a match result at `ptMinMax` with the given `score` and `angle`;
    /// initializes _delete, _newAngle, and _posOnBorder to their defaults (other fields
    /// are left uninitialized).
    MatchParams(cv::Point2f ptMinMax, double score, double angle);
    /// Default destructor.
    ~MatchParams();

public:
    cv::Point2d _point;      ///< Match location in image coordinates.
    double _matchScore;      ///< Match score for this candidate.
    double _matchAngle;      ///< Rotation angle (degrees) at which this candidate was found.

    // Mat _matRotatedSrc;
    cv::Rect _rectRoi;           ///< Search region of interest used to locate this candidate.
    double _angleStart;          ///< Start of the angle range searched for this candidate.
    double _angleEnd;            ///< End of the angle range searched for this candidate.
    cv::RotatedRect _rectR;      ///< Rotated rectangle of the match footprint at _matchAngle.
    cv::Rect _rectBounding;      ///< Axis-aligned bounding rectangle enclosing _rectR.
    bool _delete;                ///< True when this candidate is marked for removal (e.g. during overlap/non-max filtering).

    // for subpixel
    double _vecResult[3][3];     ///< 3x3 neighborhood of scores around the peak, used for sub-pixel interpolation.
    int _maxScoreIndex;           ///< Index of the highest-scoring entry considered for this candidate.
    bool _posOnBorder;            ///< True when the match position lies on the search border (sub-pixel refinement skipped).
    cv::Point2d _pointSubPixel;    ///< Sub-pixel-refined match location.
    double _newAngle;              ///< Refined match angle after sub-pixel correction.
};

}

#endif // MATCH_PARAMS_H
