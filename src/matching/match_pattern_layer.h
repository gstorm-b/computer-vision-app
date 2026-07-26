#ifndef MATCH_PATTERN_LAYER_H
#define MATCH_PATTERN_LAYER_H

#include <vector>
#include <opencv2/core.hpp>

/// Vision/matching module: PatternPoint and PatternLayer, the learned
/// per-point edge data and template layer produced when training a pattern.
namespace mtc {

/// Per-edge-point sample extracted from a pattern's edges: pixel coordinate,
/// gradient direction (Derivative) and gradient magnitude.
struct PatternPoint {
    cv::Point Coordinates;      ///< Pixel coordinate of the edge point, in the pattern image.
    cv::Point2f Derivative;     ///< Gradient direction (unit-ish vector) at this point.
    float Magnitude;            ///< Gradient magnitude at this point.
};

/// Learned template data for one trained pattern image: the extracted edge
/// points/contours plus the raw SIMD-aligned gradient buffers (pGx/pGy/pMag)
/// used by the Edge-Based matching algorithm. Owns those raw buffers, so
/// custom copy/move members are provided to manage them explicitly.
class PatternLayer {
public:
    /// Default-constructs an empty layer with no allocated gradient buffers.
    PatternLayer() :
        pGx(nullptr),
        pGy(nullptr),
        pMag(nullptr){

    }

    /// Destructor; releases the SIMD-aligned gradient buffers via freeMemory().
    ~PatternLayer() {
        this->freeMemory();
    }

    /// Frees any existing gradient buffers, then allocates 32-byte-aligned
    /// storage for `num` points in pGx/pGy/pMag.
    /// @param num number of points to allocate storage for
    void allocateMemory(size_t num) {
        freeMemory();
        pGx = (float*)_mm_malloc(num * sizeof(float), 32);
        pGy = (float*)_mm_malloc(num * sizeof(float), 32);
        pMag = (float*)_mm_malloc(num * sizeof(float), 32);
    }

    /// Releases the SIMD-aligned pGx/pGy/pMag buffers (if allocated) and resets them to nullptr.
    void freeMemory() {
        if (pGx) {
            _mm_free(pGx);
            pGx = nullptr;
        }
        if (pGy) {
            _mm_free(pGy);
            pGy = nullptr;
        }

        if (pMag) {
            _mm_free(pMag);
            pMag = nullptr;
        }
    }

    /// Copy constructor; deep-copies the image/Magnitude/angle mats and the
    /// patternPoints/contours/hierarchies vectors, then reallocates and copies
    /// the raw gradient buffers to match `other`'s point count.
    PatternLayer(const PatternLayer& other) {
        image = other.image.clone();
        patternPoints = other.patternPoints;
        contours = other.contours;
        hierarchies = other.hierarchies;
        Magnitude = other.Magnitude.clone();
        angle = other.angle.clone();

        size_t numOfPts = patternPoints.size();
        allocateMemory(numOfPts);
        std::memcpy(pGx, other.pGx, numOfPts * sizeof(float));
        std::memcpy(pGy, other.pGy, numOfPts * sizeof(float));
        std::memcpy(pMag, other.pMag, numOfPts * sizeof(float));
    }

    /// Copy assignment; deep-copies the image/Magnitude/angle mats and the
    /// patternPoints/contours/hierarchies vectors, then reallocates and copies
    /// the raw gradient buffers to match `other`'s point count.
    PatternLayer& operator=(const PatternLayer& other) {
        if (this != &other) {
            image = other.image.clone();
            patternPoints = other.patternPoints;
            contours = other.contours;
            hierarchies = other.hierarchies;
            Magnitude = other.Magnitude.clone();
            angle = other.angle.clone();

            size_t numOfPts = patternPoints.size();
            allocateMemory(numOfPts);
            std::memcpy(pGx, other.pGx, numOfPts * sizeof(float));
            std::memcpy(pGy, other.pGy, numOfPts * sizeof(float));
            std::memcpy(pMag, other.pMag, numOfPts * sizeof(float));
        }
        return *this;
    }

    /// Move constructor; takes ownership of `other`'s mats and raw gradient
    /// buffers, leaving `other`'s pGx/pGy/pMag pointers null.
    PatternLayer(PatternLayer&& other) noexcept
        : image(other.image),
        patternPoints(other.patternPoints),
        hierarchies(other.hierarchies),
        Magnitude(other.Magnitude),
        angle(other.angle),
        pGx(other.pGx),
        pGy(other.pGy),
        pMag(other.pMag) {

        other.pGx = nullptr;
        other.pGy = nullptr;
        other.pMag = nullptr;
    }

    /// Move assignment; releases this instance's gradient buffers, then takes
    /// ownership of `other`'s mats and raw buffers, leaving `other`'s
    /// pGx/pGy/pMag pointers null.
    PatternLayer& operator=(PatternLayer&& other) noexcept {
        if (this != &other) {
            image = other.image;
            patternPoints = other.patternPoints;
            contours = other.contours;
            hierarchies = other.hierarchies;
            Magnitude = other.Magnitude;
            angle = other.angle;

            freeMemory();
            pGx = other.pGx;
            pGy = other.pGy;
            pMag = other.pMag;
            other.pGx = nullptr;
            other.pGy = nullptr;
            other.pMag = nullptr;
        }
        return *this;
    }

public:
    cv::Mat image;                                     ///< Training image for this pattern layer.
    std::vector<PatternPoint> patternPoints;           ///< Edge points (coordinate + derivative + magnitude) extracted from the pattern.
    std::vector<std::vector<cv::Point>> contours;      ///< Contours extracted from the pattern's edges.
    std::vector<cv::Vec4i> hierarchies;                ///< Contour hierarchy information corresponding to `contours`.
    cv::Mat Magnitude;                                 ///< Per-pixel gradient magnitude map of the pattern image.
    cv::Mat angle;                                     ///< Per-pixel gradient angle map of the pattern image.
    float *pGx{nullptr};                                ///< Owned, SIMD-aligned array of per-point gradient X components (see allocateMemory/freeMemory).
    float *pGy{nullptr};                                ///< Owned, SIMD-aligned array of per-point gradient Y components (see allocateMemory/freeMemory).
    float *pMag{nullptr};                               ///< Owned, SIMD-aligned array of per-point gradient magnitudes (see allocateMemory/freeMemory).
};

} // namespace mtc

#endif // MATCH_PATTERN_LAYER_H
