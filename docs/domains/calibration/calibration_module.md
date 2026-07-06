# Calibration Module Reference

A C++ module for **planar 2D camera-to-robot calibration** built on top of OpenCV. The module is organised around an abstract `CalibrationBoard` interface that recognises a board on a camera frame and produces a homography (image px ↔ robot XY mm) plus a 3D work-plane fit so that `imageToRobot` can return a true `(x, y, z)` even when the picking surface is slightly tilted.

- **Language**: C++17
- **Build system**: qmake (Qt project files)
- **Dependencies**: OpenCV 4.x (uses the `opencv_world` build), Qt 6 core (only for the demo and tests; the library itself does not need Qt)
- **Scope**: planar 2D pick-and-place — a 2D homography in the board plane plus a separately-fitted 3D plane for Z. Full 3D camera calibration is intentionally out of scope.

---

## 1. File layout

```
calib_board_recognize/
├── calibration/
│   ├── calibration_board.h            // abstract base class + writePrintablePng()
│   ├── calibration_board.cpp
│   ├── fanuc_irvision_board.h         // concrete subclass: Fanuc iRVision dot grid
│   ├── fanuc_irvision_board.cpp
│   ├── calibration_board_factory.h    // factory for allocating boards
│   ├── calibration_board_factory.cpp
│   ├── calibrator.h                   // image px <-> robot mm + work-plane fit + angle conversion
│   └── calibrator.cpp
├── tests/
│   ├── tests.pro
│   └── tst_main.cpp                   // Qt Test runner
├── main.cpp                           // end-to-end demo
├── calib_board_recognize.pro
└── docs/
    ├── calibration_module.md          // this file
    ├── calibration_module.html
    └── session_context.md             // build log of the design conversation
```

The `calibration/` directory is fully self-contained — drop it into any project that links against OpenCV and it compiles.

---

## 2. Architecture

```
                       +----------------------------+
                       |   CalibrationBoard         | <-- abstract base
                       |   (interface)              |
                       +-------------+--------------+
                                     ^
                                     | implements
                       +-------------+--------------+
                       |   FanucIRvisionBoard       | <-- concrete (today)
                       |   - Params, presets        |
                       |   - detect(), generate()   |
                       +----------------------------+

                       +----------------------------+
                       |   CalibrationBoardFactory  | <-- builds boards by
                       |   - create(type)           |     type / preset name,
                       |   - createFromPreset(name) |     returns base ptr
                       |   - createFanucIRvision(p) |
                       +----------------------------+

                       +----------------------------+
                       |   Calibrator               | <-- board-agnostic:
                       |   - homography (image<->XY)|     consumes (imagePts,
                       |   - plane fit (XY -> Z)    |     robotPts3D) of any
                       |   - angle conversions      |     origin
                       +----------------------------+
```

Add a new board family (Halcon, ChArUco, …) by:
1. Subclassing `CalibrationBoard` and implementing the pure virtuals.
2. Adding a new enum value to `CalibrationBoardType`.
3. Adding a branch in `CalibrationBoardFactory::create()` and entries to `createFromPreset()` / `availablePresets()`.

`Calibrator` is board-agnostic — it never imports any subclass header, so it works with any future board family that supplies the same `(imagePoints, robotPoints3D)` correspondences.

---

## 3. Pattern design (FanucIRvisionBoard)

The board is a regular `rows × cols` grid of dots with **4 visual roles**. Both `rows` and `cols` **must be odd**, with `rows ≥ 5` and `cols ≥ 7`.

Let `R = rows`, `C = cols`, `rmid = (R-1)/2`, `cmid = (C-1)/2`.

| Role | Shape | Outer dia. | Inner | Count | Grid positions |
|---|---|---|---|---|---|
| **Normal** | solid dark | `normalDotSizeMm` | – | `R·C − 12` | every other cell |
| **Target** | dark ring + white inner | `normalDotSizeMm` | `innerTargetDotSizeMm` | 8 | 4 corners + 4 edge midpoints |
| **Centre** | dark ring + white inner | `coorDotSizeMm` (larger) | `innerTargetDotSizeMm` | 1 | `(rmid, cmid)` |
| **Coord** | solid dark | `coorDotSizeMm` | – | 3 | `(rmid, cmid+1)`, `(rmid, cmid+2)`, `(rmid−1, cmid)` |

- The 9 ring dots (8 targets + 1 centre) have a white inner — these are the **precise tool-tip targets** when the robot touches the board for calibration.
- The 4 large markers (1 centre ring + 3 coord solids) form a 4-point reference that the detector uses to figure out board orientation **purely from geometry** (no fragile size or hole heuristics).
- The two coord dots to the right of the centre encode the **+X axis** direction; the one above the centre encodes **+Y** (image-up). The 3 solid coord dots + 1 ring centre uniquely break rotational symmetry.

---

## 4. `CalibrationBoard` — abstract base class

Defined in [`calibration/calibration_board.h`](../../../src/calibration/calibration_board.h), namespace `calib`. This is the only header that board-agnostic client code needs to include.

```cpp
class CalibrationBoard {
public:
    virtual ~CalibrationBoard() = default;

    // Synthesis
    virtual cv::Mat generateImage(double pixelsPerMm = 10.0) const = 0;
    bool writePrintableImage(const std::string& path, double dpi = 300.0) const;

    // Detection
    virtual bool detect(const cv::Mat& image,
                        std::vector<cv::Point2f>& imagePoints,
                        std::vector<cv::Point2f>* cornerImagePoints = nullptr,
                        cv::Mat* debugOverlay = nullptr) const = 0;

    // Geometry (board coordinates in mm, z = 0 on a planar board)
    virtual int                      totalDots()             const = 0;
    virtual std::vector<cv::Point3f> objectPoints()          const = 0;
    virtual std::vector<cv::Point2f> objectPointsXY()        const = 0;
    virtual std::vector<cv::Point2f> cornerObjectPointsXY()  const = 0;

    // Identification
    virtual std::string typeName() const;     // "FanucIRvision", ...

    // JSON serialisation (NVI). Emits a self-describing JSON string carrying
    // "type" (from typeName()) + the subclass-specific Params fields written
    // by writeJsonFields(). Use CalibrationBoardFactory::createFromJson() to
    // rebuild a board instance from the resulting string.
    std::string toJson() const;

    // Static helpers
    static cv::Mat pointsToMat(const std::vector<cv::Point2f>& pts);
    static cv::Mat pointsToMat(const std::vector<cv::Point3f>& pts);
    static double  pixelsPerMmFromDpi(double dpi);
    static double  dpiFromPixelsPerMm(double pxPerMm);

protected:
    // Subclass-specific JSON fields. The "type" key is already emitted by toJson().
    virtual void writeJsonFields(cv::FileStorage& fs) const = 0;
};
```

Notes:
- `writePrintableImage` is **non-virtual** and uses the NVI pattern: it calls the virtual `generateImage` at `dpi/25.4` px/mm and pipes the result through the free `writePrintablePng` helper. Every subclass therefore gets a working printable PNG for free.
- `typeName()` returns a stable subclass identifier (`"FanucIRvision"` today). Use it for logging or for guard-clauses when you need to switch on board type.
- `pointsToMat` produces matrices in the format that OpenCV's `cv::calibrateCamera`, `cv::findHomography`, `cv::solvePnP`, etc. accept directly.
- `toJson` / `writeJsonFields` form an NVI pair: subclasses only override the field writer; the type-tagged JSON envelope is built by the base.

---

## 5. `FanucIRvisionBoard` — concrete subclass

Defined in [`calibration/fanuc_irvision_board.h`](../../../src/calibration/fanuc_irvision_board.h), namespace `calib`.

### 5.1 `Params`

```cpp
struct Params {
    int    rows                  = 11;    // must be odd, >= 5
    int    cols                  = 11;    // must be odd, >= 7
    double dotSpacingMm          = 15.0;
    double marginMm              = 15.0;
    double normalDotSizeMm       = 4.0;   // normal dots AND outer of target rings
    double coorDotSizeMm         = 8.0;   // outer of centre ring AND coord solids
    double innerTargetDotSizeMm  = 1.0;   // inner white of target & centre rings

    bool isValid(std::string* errorOut = nullptr) const;
};
```

Validation rules enforced by `Params::isValid()` (and the constructor):

- `rows ≥ 5`, `cols ≥ 7`, both odd.
- `0 < innerTargetDotSizeMm < normalDotSizeMm < coorDotSizeMm < dotSpacingMm`.
- `marginMm ≥ 0`, `dotSpacingMm > 0`.

If invalid, the `FanucIRvisionBoard` constructor throws `std::invalid_argument`.

### 5.2 Built-in presets

Named after the physical iRVision part's dot-pitch. Exact constants are tuned for the boards we have in hand; don't pin tests against specific dimensions, validate structural invariants instead.

```cpp
calib::FanucIRvisionBoard::iRvisionPattern5mm
calib::FanucIRvisionBoard::iRvisionPattern11m
calib::FanucIRvisionBoard::iRvisionPattern15mm
calib::FanucIRvisionBoard::iRvisionPattern22mm
calib::FanucIRvisionBoard::iRvisionPattern30mm
```

Field order in the aggregate literal: `{rows, cols, dotSpacingMm, marginMm, normalDotSizeMm, coorDotSizeMm, innerTargetDotSizeMm}`.

Direct usage (when you need iRVision-specific accessors):

```cpp
calib::FanucIRvisionBoard board(calib::FanucIRvisionBoard::iRvisionPattern15mm);
```

### 5.3 iRVision-specific accessors

In addition to the inherited interface, the subclass exposes:

```cpp
const Params& params() const;

std::vector<int> targetIndices() const;   // 8 target-ring row-major indices
std::vector<int> centerIndex()   const;   // 1 centre-ring index
std::vector<int> coordIndices()  const;   // 3 coord indices [xMid, xTip, yTip]
std::vector<int> markerIndices() const;   // [centre, xMid, xTip, yTip] - 4 orientation refs

// Parse the JSON produced by CalibrationBoard::toJson() into a Params struct.
// Throws std::invalid_argument when the parsed fields fail validation.
static FanucIRvisionBoard::Params paramsFromJson(const std::string& json);
```

Use a `dynamic_cast<const FanucIRvisionBoard*>(...)` if you only hold a `CalibrationBoard*` and need these.

### 5.4 Detection algorithm

1. `binarize()` to threshold the grayscale into a dot mask (`THRESH_BINARY_INV`). The threshold is configurable via `Params::binarizeThreshold` (exposed through `CalibrationBoard::binarizeThreshold()` / `setBinarizeThreshold()`): `-1` => **auto (Otsu)**, `0..255` => **fixed manual** value. `binarize()` is the single shared routine used by both `detect()` and the threshold-tuning dialog, so a tuned preview matches detection exactly. The per-camera value is persisted in the camera config (`CalibBinarizeThreshold`) and tuned through `CalibrationThresholdDialog` (slider + Auto(Otsu) toggle, live binarized preview + detection overlay).
2. `cv::findContours(RETR_CCOMP)` to enumerate top-level circular blobs.
3. **Robustness filters** applied per-contour (this stack matters for real photos, where surrounding bright background can otherwise sneak past circularity):
   - Min contour size and `m00 > 4 px²` (drop noise).
   - Circularity `4πA/p² ≥ 0.55`.
   - **Bounding-box aspect ratio** `max(w,h) / min(w,h) ≤ 1.4` — rejects the giant rectangular background region (image-wide bbox has aspect ≈ image_w/image_h, far from 1) and any non-dot rectangular labels.
4. Sort the surviving blobs by area descending.
5. **Median-area cap** — compute the area at roughly the 75th percentile from the top (well inside the population of normal dots) and drop any blob > 12× that. The 4 orientation markers are ~4× the median, so they survive; the background, if it ever crept past circularity, is ~1000× the median and gets cut.
6. Sanity: the **top 4 areas** must be similar (`blobs[0].area ≤ 3 × blobs[3].area`). These 4 are the orientation markers (1 centre ring + 3 coord solids — all sized `coorDotSize`).
7. Identify roles by pure geometry:
   - Try each of the 4 markers as the "yTip candidate"; the candidate whose remaining triple has the smallest cross-product residual is `yTip`.
   - In the colinear triple `{centre, xMid, xTip}`, the middle point (smallest sum of pairwise distances) is `xMid`.
   - Of the two endpoints, the one closer to `yTip` is `centre`; the other is `xTip`.
8. Build a per-step basis: `vCol = (xTip − centre)/2`, `vRow = centre − yTip`.
9. Project every expected grid cell to image pixels and snap each detected blob to its nearest cell (within `min(|vCol|, |vRow|) × 0.45`).

The algorithm is invariant under rotation, modest perspective and scale: it does **not** rely on the board being axis-aligned in the image, and it does **not** rely on the white inner holes being individually resolvable (so it tolerates aggressive downsampling or thin printed ink).

---

## 6. `CalibrationBoardFactory`

Defined in [`calibration/calibration_board_factory.h`](../../../src/calibration/calibration_board_factory.h), namespace `calib`.

```cpp
enum class CalibrationBoardType {
    FanucIRvision,
    // Halcon,     // future
    // ChArUco,    // future
    // ChessBoard, // future
};

class CalibrationBoardFactory {
public:
    static std::unique_ptr<CalibrationBoard> create(CalibrationBoardType type);

    static std::unique_ptr<CalibrationBoard>
        createFanucIRvision(const FanucIRvisionBoard::Params& params
                                = FanucIRvisionBoard::Params{});

    static std::unique_ptr<CalibrationBoard>
        createFromPreset(const std::string& presetName);

    static std::vector<std::string> availablePresets();

    // Reconstruct a board from the JSON produced by CalibrationBoard::toJson().
    // Dispatches on the "type" key. Returns nullptr if the type is unknown or
    // the parsed Params fail validation.
    static std::unique_ptr<CalibrationBoard>
        createFromJson(const std::string& json);
};
```

All `create*` methods return `std::unique_ptr<CalibrationBoard>` (the abstract base). When type-specific accessors are needed, either construct the subclass directly or `dynamic_cast` from the base pointer.

### 6.1 Recognised preset names

| Name | Backing constant |
|---|---|
| `"iRvision-5mm"`    | `FanucIRvisionBoard::iRvisionPattern5mm`  |
| `"iRvision-11.5mm"` | `FanucIRvisionBoard::iRvisionPattern11m`  |
| `"iRvision-15mm"`   | `FanucIRvisionBoard::iRvisionPattern15mm` |
| `"iRvision-22.5mm"` | `FanucIRvisionBoard::iRvisionPattern22mm` |
| `"iRvision-30mm"`   | `FanucIRvisionBoard::iRvisionPattern30mm` |

`createFromPreset()` returns `nullptr` for any name not in this list.

### 6.2 Usage

```cpp
// 1. Default of a given type.
auto a = calib::CalibrationBoardFactory::create(
            calib::CalibrationBoardType::FanucIRvision);

// 2. Explicit Fanuc iRVision with custom params.
calib::FanucIRvisionBoard::Params p;
p.rows = 9; p.cols = 11; p.dotSpacingMm = 12.0;
auto b = calib::CalibrationBoardFactory::createFanucIRvision(p);

// 3. By preset name (recommended for top-level config files / CLI).
auto c = calib::CalibrationBoardFactory::createFromPreset("iRvision-15mm");

// 4. From a JSON string previously produced by board->toJson().
auto d = calib::CalibrationBoardFactory::createFromJson(savedJson);
```

---

## 7. `Calibrator` class

Defined in [`calibration/calibrator.h`](../../../src/calibration/calibrator.h), namespace `calib`. Holds a planar 2D ↔ 3D calibration:

- a **3×3 homography H** from image px to robot XY mm
- a **3D work plane** through the supplied robot 3D points so that `imageToRobot` can return a true `(x, y, z)` even when the picking surface is slightly tilted
- cached centroids used as anchors for **angle conversions** between the image-y-down frame and the robot top-down frame.

```cpp
class Calibrator {
public:
    Calibrator();
    void clear();

    // Each robot point carries its measured Z so a (possibly tilted) work
    // plane can be fitted during calibrate().
    void addCorrespondences(const std::vector<cv::Point2f>& imagePoints,
                            const std::vector<cv::Point3f>& robotPointsMm);

    bool calibrate(double ransacReprojThreshold = 3.0);
    bool isCalibrated() const;

    // image (px) -> robot (mm). Returned Z lies on the fitted work plane.
    cv::Point3f imageToRobot(const cv::Point2f& imagePx) const;

    // robot (mm) -> image (px). Input Z is ignored; only XY is back-projected.
    cv::Point2f robotToImage(const cv::Point3f& robotMm) const;

    std::vector<cv::Point3f> imageToRobot(const std::vector<cv::Point2f>&) const;
    std::vector<cv::Point2f> robotToImage(const std::vector<cv::Point3f>&) const;

    // Angle conversion. radians=false (default) interprets/returns degrees.
    // image angle convention: CCW from +X, taken in the Y-down image frame.
    // robot angle convention: CCW from +X around +Z (top-down view).
    double rotateImageToRobot(double angleImg, bool radians = false) const;
    double rotateRobotToImage(double angleRob, bool radians = false) const;

    // Apply a planar offset rotated around the +Z axis by `theta`, then add
    // it to `A`. Useful for tool-frame offsets (e.g. "go 10 mm along the
    // tool's local X after the tool has rotated by R degrees around Z").
    //   A      : 3D point in robot mm
    //   offset : 3D offset in mm, interpreted in a frame rotated by `theta`
    //            from the robot world frame around +Z. Offset Z is added as-is.
    //   theta  : rotation angle around +Z. isRad=true (default) means radians.
    //
    // result.x = A.x + cos(theta) * offset.x - sin(theta) * offset.y
    // result.y = A.y + sin(theta) * offset.x + cos(theta) * offset.y
    // result.z = A.z + offset.z
    cv::Point3f translateWithZAxis(cv::Point3f& A,
                                   cv::Point3f& offset,
                                   double theta,
                                   double isRad = true) const;

    // --- Diagnostics ---
    double    reprojectionErrorPx()   const;
    double    reprojectionErrorMm()   const;   // XY residual on robot side
    double    planeFitMaxErrorMm()    const;   // largest robot-Z residual

    cv::Mat   homography()            const;   // image px -> robot mm (XY)
    cv::Mat   homographyInverse()     const;
    cv::Vec4d workPlane()             const;   // a*x + b*y + c*z + d = 0

    size_t    sampleCount() const;
    bool      save(const std::string& filePath) const;
    bool      load(const std::string& filePath);

    // In-memory JSON serialisation of the full instance state (homography,
    // work plane, centroids, raw correspondences, diagnostics). fromJson()
    // restores the calibrator to an "already-calibrated" state WITHOUT
    // re-running the homography fit, so H is preserved bit-for-bit.
    std::string toJson()                       const;
    bool        fromJson(const std::string& json);
};
```

### 7.1 Work-plane fit (Z model)

The 3D robot points supplied to `addCorrespondences` define an implicit work plane `a·x + b·y + c·z + d = 0` (in robot mm). `calibrate()` fits this plane via SVD on the centred coordinates and normalises the result so that `c > 0` (non-vertical plane → `z = −(a·x + b·y + d) / c`). `imageToRobot` then runs the homography to get robot `(x, y)` and reads `z` off the plane.

A perfectly horizontal surface yields `a = b = 0`, `c = 1`, `d = −z_surface`. A 1 mm/100 mm tilt produces small but nonzero `a`/`b`.

`planeFitMaxErrorMm()` reports the worst residual between the fitted plane and the input `robotPoints3D[i].z`. For the typical 4-corner-only calibration the plane is exactly determined (residual ≈ 0); with N > 4 points the value becomes a useful diagnostic.

### 7.2 Angle conversion

`rotateImageToRobot` and `rotateRobotToImage` are **empirical**: they map a unit-direction vector through the homography (with the dataset centroid as anchor) and read the angle of the resulting vector in the other frame. This handles both pure-similarity transforms (typical) and milder perspective without depending on the homography being a strict similarity.

Conventions:
- **Image angle** measured CCW from +image-X. Because image-Y is down, the direction vector at angle θ is `(cos θ, −sin θ)`.
- **Robot angle (R)** measured CCW from +X around +Z (right-hand rule, top-down view). Direction vector: `(cos R, sin R)`.
- The default unit is **degrees**; pass `radians = true` to switch.

Round-trip should be exact to floating-point precision (`< 1e-3°` in tests).

### 7.3 TCP offset compensation: `translateWithZAxis`

```cpp
cv::Point3f translateWithZAxis(cv::Point3f& A,
                               cv::Point3f& offset,
                               double theta,
                               double isRad = true) const;
```

#### Why it exists

Vision calibration alone is not enough for a real pick-and-place line. Even after `imageToRobot` reports a perfectly-projected pick point `A`, the robot will not actually grasp the object at `A` because of **mechanical error in the TCP** (Tool Center Point): the gripper's true contact point drifts a few mm from the nominal TCP due to flange tolerance, tool wear, mounting drift, etc.

This method applies that mechanical offset *after* the rotation of the object, so the corrected target is robot-frame correct for any object orientation.

#### Calibration procedure for the offset

The TCP offset is measured **once, at robot rotation R = 0**, then re-used for every later pick at arbitrary R:

1. Place an object somewhere in the camera view.
2. Run detection / matching → obtain the picking pose; call `imageToRobot` → robot point `A0`. Move the robot there and pick.
3. Place the object back down **with `Rz = 0.0`**.
4. Run detection / matching again → obtain a fresh `A1` = `imageToRobot(matchedPx)`.
5. Move the robot to `A1` and manually nudge XYZ until the gripper centres exactly on the object. Record the corrected position `B1`.
6. The TCP offset (in the robot world frame, valid at R = 0) is:
   ```
   offset = B1 - A1     // both Point3f
   ```

For any future pick with the object rotated by `R` (degrees or radians):

```cpp
cv::Point3f matched = calibrator.imageToRobot({u, v});
double      objR    = calibrator.rotateImageToRobot(objectAngleImgDeg);   // degrees
cv::Point3f goTo    = calibrator.translateWithZAxis(
                          matched, /*offset=*/tcpOffset, objR, /*isRad=*/false);
```

The XY part of the offset gets rotated by `R` around +Z (the gripper has spun by `R`, so the mechanical bias rotates with it). The Z part is added unchanged.

#### Formula

```
goTo.x = A.x + cos(theta) * offset.x − sin(theta) * offset.y
goTo.y = A.y + sin(theta) * offset.x + cos(theta) * offset.y
goTo.z = A.z + offset.z
```

#### Notes

- `isRad` defaults to `true` (theta in **radians**). Pass `false` if you have a degree value coming directly from `rotateImageToRobot()`.
- `A` and `offset` are passed by non-const reference for historical reasons; the function does **not** modify them. If you call it on temporaries, bind them to named locals first.
- The offset is a calibration constant — measure it once, store it next to the homography (e.g. as part of an outer JSON document alongside `calibrator.toJson()`), and re-use across runs.

### 7.5 Real-world calibration flow (4-corner)

```cpp
auto board = calib::CalibrationBoardFactory::createFromPreset("iRvision-15mm");

std::vector<cv::Point2f> allPts, corners;
board->detect(cameraImage, allPts, &corners);  // corners = [TL,TR,BR,BL] image px

// Operator touches each of the 4 corner targets with the robot tool and
// records the corresponding robot (X, Y, Z) in mm. Order MUST match the
// detector output (TL, TR, BR, BL).
std::vector<cv::Point3f> cornersRobot = {
    {-121.738f, 722.750f, 107.544f},
    {  27.895f, 714.734f, 108.544f},
    {  19.769f, 565.180f, 109.250f},
    {-130.012f, 572.863f, 108.451f}
};

calib::Calibrator calibrator;
calibrator.addCorrespondences(corners, cornersRobot);
calibrator.calibrate();

cv::Point2f pixel(123.4f, 567.8f);
cv::Point3f robotXYZ = calibrator.imageToRobot(pixel);   // Z follows the fitted plane

double objectAngleImageDeg = 30.0;
double toolRotationDeg     = calibrator.rotateImageToRobot(objectAngleImageDeg);
```

With exactly 4 correspondences the homography is exactly determined. Adding more (e.g. all `R*C` dots) gives an over-determined least-squares fit and `reprojectionErrorMm()` becomes meaningful.

### 7.6 Persistence

Two pairs share the same on-disk schema (YAML for file I/O, JSON for in-memory strings); they round-trip the full instance state.

| Direction | File-based             | In-memory (string)            |
|-----------|------------------------|-------------------------------|
| Save      | `save(path) → bool`    | `toJson() → std::string`      |
| Load      | `load(path) → bool`    | `fromJson(json) → bool`       |

Both write/read the following fields:

- `calibrated` (0 / 1)
- `homography_image_to_robot` — 3×3 `cv::Mat`
- `work_plane_abcd`           — 4-vector `(a, b, c, d)` for the plane equation
- `image_centroid`, `robot_centroid` — anchors used by angle conversions
- `reprojection_error_px`, `reprojection_error_mm`, `plane_fit_max_error_mm`
- `sample_count`
- `image_points`, `robot_points` — raw correspondences (so a restored instance is bit-identical, not a re-fit)

`fromJson()` / `load()` restore an "already-calibrated" calibrator **without re-running the fit** — `H` is preserved bit-for-bit and all downstream conversions return the same numbers as the original.

```cpp
std::string j = calibrator.toJson();
// ... transmit / persist / cache ...
calib::Calibrator restored;
if (!restored.fromJson(j)) { /* error */ }
assert(restored.imageToRobot(px) == calibrator.imageToRobot(px));   // identical
```

---

## 8. Helper: `writePrintablePng`

```cpp
namespace calib {
bool writePrintablePng(const std::string& path,
                       const cv::Mat& img,
                       double pxPerMm);
}
```

Writes a PNG with an embedded **pHYs** chunk so printers and viewers can reproduce the image at exact physical size. Use this whenever you want to **print the board on paper at 1:1 scale**.

The chunk encodes pixels-per-meter (`round(pxPerMm × 1000)`) in both axes with `unit = 1` (meter, the only PNG-defined unit).

`CalibrationBoard::writePrintableImage` is a convenience wrapper that combines `generateImage(dpi/25.4)` with `writePrintablePng`.

### Printing checklist

1. Call `board->writePrintableImage("board.png", 300.0)` (or 600 DPI for finer ink coverage).
2. Open with **Adobe Reader**, **Foxit**, **GIMP**, **Affinity Photo**, or any viewer that respects pHYs.
3. In the print dialog choose **"Actual size" / "100%"** and disable any "Fit to page" option.
4. The printed board will measure exactly `((cols-1)*spacing + 2*margin) × ((rows-1)*spacing + 2*margin)` mm.

> Note: Windows Photos sometimes ignores pHYs; if the size is off, route through Adobe Reader or export to PDF first.

---

## 9. End-to-end example (excerpt from `main.cpp`)

```cpp
#include "calibration/calibration_board.h"
#include "calibration/calibration_board_factory.h"
#include "calibration/fanuc_irvision_board.h"
#include "calibration/calibrator.h"

// 1. Construct a board via the factory (stays board-agnostic).
std::unique_ptr<calib::CalibrationBoard> board =
    calib::CalibrationBoardFactory::createFromPreset("iRvision-15mm");
if (!board) return 1;

// 2. Render + print at 1:1 mm.
board->writePrintableImage("board_print_300dpi.png", 300.0);

// 3. Detect on a camera frame.
std::vector<cv::Point2f> allPts, corners;
cv::Mat overlay;
if (!board->detect(cameraImage, allPts, &corners, &overlay)) return 2;

// 4. Pair each of the 4 corners with the robot XYZ you measured.
std::vector<cv::Point3f> cornersRobot = { /* (x, y, z) measured by the robot probe */ };

// 5. Calibrate.
calib::Calibrator calibrator;
calibrator.addCorrespondences(corners, cornersRobot);
calibrator.calibrate();
calibrator.save("calibration.yml");

// 6. Use it.
cv::Point3f pickInRobotMm = calibrator.imageToRobot({pixelX, pixelY});
double toolRotationDeg     = calibrator.rotateImageToRobot(angleInImageDeg);

// 7. Spot-check on the board centre marker (cell (rmid, cmid)).
if (const auto* irv = dynamic_cast<const calib::FanucIRvisionBoard*>(board.get())) {
    const int centreIdx        = irv->centerIndex().front();
    const cv::Point3f centreRob = calibrator.imageToRobot(allPts[centreIdx]);
    // drive the robot tool to centreRob and visually confirm it sits on the
    // white inner of the centre ring.
}
```

---

## 10. Building

```pro
# In calib_board_recognize.pro
QT      = core
CONFIG += c++17 cmdline

SOURCES += main.cpp \
           calibration/calibration_board.cpp \
           calibration/fanuc_irvision_board.cpp \
           calibration/calibration_board_factory.cpp \
           calibration/calibrator.cpp
HEADERS += calibration/calibration_board.h \
           calibration/fanuc_irvision_board.h \
           calibration/calibration_board_factory.h \
           calibration/calibrator.h
INCLUDEPATH += $$PWD $$PWD/calibration

# OpenCV (Windows, MSVC 2022, OpenCV 4.11 world build)
win32:CONFIG(release, debug|release): LIBS += -LC:/opencv/build/x64/vc16/lib/ -lopencv_world4110
else:win32:CONFIG(debug,   debug|release): LIBS += -LC:/opencv/build/x64/vc16/lib/ -lopencv_world4110d
else:unix:                                LIBS += -LC:/opencv/build/x64/vc16/lib/ -lopencv_world4110
INCLUDEPATH += C:/opencv/build/include
DEPENDPATH  += C:/opencv/build/include
```

Tests are a sibling sub-project at `tests/tests.pro`. The tests link the same four `.cpp` files via relative paths and add `QT += testlib`.

---

## 11. Operational limits and watch-list

- **Board rotation.** Corner output is labelled by image-space convention. Keep
  the physical board roughly aligned with the camera during calibration; if the
  board is rotated enough that image-space top-left/top-right intuition breaks,
  record robot points in the exact order returned by `detect()`.
- **Inner white targets.** Detection no longer depends on resolving the white
  inner holes, but the operator still uses the white centres as precise robot
  touch targets. Tiny inner diameters on low-resolution/noisy images may make
  overlays less informative even when geometry detection still succeeds.
- **Partial occlusion.** The detector expects the four large orientation markers
  to be visible. If any centre/coord marker is missing, recapture the frame
  instead of trying to calibrate from a partial board.
- **Print/export format.** `writePrintablePng()` embeds exact physical scale
  only for PNG. Other formats need their own DPI metadata handling.
- **Preset validation.** iRVision preset constants mirror the intended physical
  board families, but each shop-floor printed board should still be checked at
  actual size before it becomes a production calibration artefact.

---

## 12. Frequently-asked design questions

**Why an abstract base class?** So the rest of the codebase can hold a `CalibrationBoard*` (or `unique_ptr<CalibrationBoard>`) and never have to know whether the board is Fanuc, Halcon, ChArUco or something new. Adding a board family is a localised change.

**Why a factory?** Construction always returns `unique_ptr<CalibrationBoard>` — the caller can stay base-only. The factory also centralises preset names so configuration files / CLIs can refer to boards by string rather than C++ types.

**Why are `targetIndices()` etc. on the subclass, not the base?** Other board families (e.g. chessboard, ChArUco) do not have the same role structure. Pinning those accessors to `FanucIRvisionBoard` keeps the base interface stable.

**Why does `imageToRobot` return `Point3f` even though we're a 2D-only module?** The camera is 2D, but the robot world is 3D. The picking surface in robot mm is typically not perfectly horizontal — a fitted work plane lets us return a sensible Z so the operator can move the robot to the right height without manual adjustment.

**Why does `robotToImage` ignore the input Z?** The camera projects all points on the plane to the same 2D pixel; once you're moving robot points that lie off the plane, the back-projection is ambiguous. Ignoring Z is the only defensible behaviour for a single-camera planar setup.

**Why are angles converted empirically instead of from a closed-form H decomposition?** The empirical method works for any homography, including mild perspective; it stays equivalent to a closed-form similarity extraction in the no-perspective limit; it is location-independent for any near-similarity transform (the centroid anchor minimises the variance for the general case).

**Why only 4 dot categories instead of 9 markers?** Earlier iterations placed 9 same-size markers at corners + edges + centre. That works but cannot disambiguate 90°/180° rotation without an asymmetric size cue. Using 1 large ring + 3 large solids breaks rotational symmetry naturally while keeping the visual layout symmetric for the operator.

**Why detect orientation from 4 large markers instead of size buckets?** Heuristic size buckets need an accurate `pxPerMm` estimate, which is brittle when the board fills a non-standard fraction of the frame. Identifying the 4 largest blobs and then matching them by geometry is scale-free and orientation-free.

**Why use 4 corner targets for the actual calibration rather than the orientation markers?** The corners span the full board extent → the homography is well-conditioned. The 4 orientation markers are clustered near the centre and would give an ill-conditioned fit.

**Why an odd grid?** Forces a unique centre cell so the centre ring sits exactly on the middle dot, and so the 3 coord dots have integer grid positions adjacent to it.

**Why is the inner white circle the precise tool target?** When the operator manually centres the robot probe over a target ring, the white interior is the only feature visible at very close range — they centre on the white, not on the outer ring.

---

## 13. Tests

`tests/tst_main.cpp` ships three Qt Test classes (31 cases total at the time of writing):

**TestFanucIRvisionBoard** (13 cases)
- `params_evenRowsOrCols_throws`, `params_validOdd_constructsOk`
- `generateImage_hasExpectedSize`, `objectPoints_countAndSpacing`
- `markerIndices_returnsFourOrientationMarkers`, `targetIndices_returnsEightTargets`,
  `coordIndices_returnsThreeCoordDots`, `specialCells_doNotOverlap`,
  `cornerObjectPoints_areBoardCorners`
- `detect_onGeneratedImage_returnsAllDots`, `detect_returnsFourCornersInImageOrder`,
  `detect_orientationOrder_isRowMajor`
- `writePrintableImage_embedsPhysChunk`

**TestCalibrationBoardFactory** (7 cases)
- `create_FanucIRvision_returnsConcrete`, `createFromPreset_knownName_returnsBoard`,
  `createFromPreset_unknownName_returnsNull`, `availablePresets_includesAllIRvisionEntries`,
  `polymorphicUsage_throughBasePointer`
- `createFromPreset_knownName_returnsBoard` validates structural invariants only — it does NOT pin to specific row/col numbers, so preset constants can be tuned without rewriting the test.
- `toJson_roundTrip_viaFactory` — `board.toJson()` → `factory.createFromJson()` → all Params bit-equal.
- `createFromJson_unknownType_returnsNull` — unknown `"type"` field returns nullptr safely.

**TestCalibrator** (9 cases)
- `calibrate_lessThan4Points_returnsFalse`, `calibrate_perfectAffine_lowError`,
  `calibrate_fromFourCornersOnly`, `roundTrip_imageToRobotToImage`
- `imageToRobot_zOnFittedPlane` — fits a known tilted plane (`z = 0.01x − 0.02y + 5`) and verifies that `imageToRobot` returns Z on that exact plane.
- `rotateImageToRobot_roundTripIsExact` — 7 angles round-trip with error < 1e-3°.
- `rotateImageToRobot_pureRotationMatchesAppliedAngle` — for a pure-rotation similarity, the recovered angle equals the applied rotation.
- `saveLoad_homographyAndPlanePreserved` — YAML round-trip preserves H, plane and centroids.
- `toJson_fromJson_preservesFullState` — JSON in-memory round-trip preserves H bit-equal, plane bit-equal, all diagnostics and functional `imageToRobot` / `rotateImageToRobot` outputs.

Run from Qt Creator or the command line via the test subproject
`tests/calibration_test/calibration_test.pro` (qmake + `nmake /nologo`;
target `calibration_test`).

---

## 14. Extending: adding a new board family

To add (say) a Halcon-style chessboard:

1. **Create the subclass** `calibration/halcon_chessboard.h/.cpp` deriving from `CalibrationBoard`. Implement the pure virtuals (`generateImage`, `detect`, `totalDots`, `objectPoints`, `objectPointsXY`, `cornerObjectPointsXY`) and override `typeName()` to return a stable string like `"HalconChessboard"`.

2. **Register with the factory**:
   - Add `Halcon` to `CalibrationBoardType` in `calibration_board_factory.h`.
   - Add a `case CalibrationBoardType::Halcon: return createHalcon();` branch to `create()`.
   - Add a `createHalcon(...)` overload and any preset entries to `createFromPreset()` / `availablePresets()`.

3. **(Optional)** add type-specific tests in `tst_main.cpp`. The polymorphic-usage test in `TestCalibrationBoardFactory` already exercises the abstract interface, so any new board family that passes its own subclass tests will also be drivable through the base interface.

`Calibrator` is board-agnostic and needs no changes — as long as the new board family produces `(imagePoints, robotPoints3D)` correspondences, it plugs in cleanly.

No existing client code needs to change.
