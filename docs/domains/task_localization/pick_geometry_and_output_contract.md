# Pick Geometry And Vision Output Contract

How a pattern describes where and how a part is gripped, how that becomes a robot
pose, and what goes on the wire. Written for developers changing any part of that
chain; the failure modes here are silent, so read the warnings.

Introduced by Phase 5 (2026-07-29/30). See
[../../history/plan/phase_5_implementation_plan.md](../../history/plan/phase_5_implementation_plan.md)
for the task-by-task record.

## 1. What a pattern carries

`mtc::MatchPatternConfig`
([match_pattern_config.h](../../../src/matching/match_pattern_config.h)) holds the
per-pattern pick description:

| Field | Meaning |
|---|---|
| `m_pickPosition` | Pick point, in pattern-local image pixels. |
| `m_angle` | **Picking angle**, degrees. See the warning below. |
| `m_gripperBoxes` | Jaw size + centre-to-centre spacing (`mtc::GripperBoxes`). Describes the **gripper**. |
| `m_pickingBoxAngle` | Jaw-pair orientation, degrees. Per-**pattern**, not part of the gripper. |
| `m_usePickingBox` | When false, the collision check is skipped and the pattern is never rejected for collision. |
| `m_pickingOffset` | Pick offset X/Y/Z, millimetres. |
| `m_pickingRotationOffset` | Pick rotation offset RX/RY/RZ, degrees, TOOL frame. |

### ⚠ `m_angle` is not a search parameter

`m_angle` is added to the reported angle of every match from that pattern —
`point_angle = matched_angle + m_angle`, the only place it is used
([image_matcher.cpp](../../../src/matching/image_matcher.cpp)). It expresses the
orientation at which the part is gripped.

It does **not** steer matching. The angular search sweep is built from
`m_toleranceAngle` around **zero**, not around `m_angle`. Three descriptions in the
codebase claimed otherwise and were corrected in Phase 5 (task F3); if you find a
fourth, it is wrong too.

### ⚠ Two different angles

`m_angle` (picking angle) and `m_pickingBoxAngle` (jaw orientation) are unrelated and
both independently editable. The original Phase 5 request conflated them. A change
that "unifies" them will break either the reported pick angle or the collision
geometry.

### ⚠ Three similarly-named types

| Type | Module | What it is |
|---|---|---|
| `mtc::GripperBoxes` | matching | **Configured** jaw size + spacing. Shareable as a preset. |
| `mtc::MatchedObject::CollisionGeometry` | matching | **Computed** per match: two oriented rects + corners. |
| `ItemGripperBox` | ui | Unrelated `QGraphicsItem`. |

A search-and-replace on "GripperBox" hits the third one. Don't.

## 2. Gripper presets

`vc::model::GripperPresetStore`
([gripper_preset_store.h](../../../src/model/gripper_preset_store.h)) is a named,
project-scoped collection of `GripperBoxes` values, owned by `TaskLocalization` and
saved under its `gripperPresets` key.

- A preset carries **size and distance only** — no angle. The orientation is
  per-pattern, so a preset must not reinstate one.
- Applying a preset **copies** its values. Nothing holds a reference, so editing or
  deleting a preset later cannot mutate a pattern already authored from it. That is
  what keeps a commissioned task stable when the list is tidied up.
- It lives in `model`, not `matching`: `matching` never reads presets at runtime.
- Blank and duplicate names are rejected by `add()`/`rename()` rather than silently
  de-duplicated, so the caller reports the failure. `fromJson()` is the exception —
  it skips bad entries so a hand-edited project file degrades to a shorter list
  instead of failing the whole task load.

Authoring surfaces: the register dialog (Gripper button under the pattern library),
the preset picker on the box step of both wizards, and an Apply button in the
property panel.

## 3. Pattern JSON schema versions

`PatternGroupManager::kSchemaVersion` is **2**. A document declaring a higher version
is refused with a logged error rather than loaded with partial defaults. A document
with no `version` key is the pre-versioning baseline (v0).

| Version | Gripper geometry on disk |
|---|---|
| v0 (pre-Phase-5) | flat `pickingBoxSize` / `pickingBoxDistance` / `pickingBoxAngle` |
| v1 (mid-Phase-5) | nested `gripperBoxes { w, h, distance, angle }` |
| v2 (current) | nested `gripperBoxes { w, h, distance }` + flat `pickingBoxAngle` |

One read rule covers all three: take the angle from `gripperBoxes.angle` when that
nested key is present (v1 only), otherwise from the flat `pickingBoxAngle` key —
which v0 and v2 happen to share. Writes always emit v2.

### The back-compatible read is deliberate

`AGENT.md` forbids compatibility shims. **This read is an explicitly agreed
exception**, because real commissioned picking geometry exists in saved projects and
losing it means re-teaching parts on the line. Do not "clean up" the legacy branches
in `patternConfigFromJson()`.

The version bump to 2 matters even though v2 reuses a v0 key name: a v1-era build
reading a v2 document would find no nested angle, silently load `0`, and ignore the
flat key that actually holds it. That silent-loss case is exactly what the version
guard exists to prevent.

## 4. From match to world pose

`LocalizationRuntimeController::buildVisionOutputPositions()`
([localization_runtime_controller.cpp](../../../src/model/localization_runtime_controller.cpp))
composes the commanded pose with `RobotKinematics::Pose`:

```
pickPose   = fromXYZRPY_mm_deg(pickPoint.x, pickPoint.y, pickPoint.z, 0, 0, worldYaw)
offsetPose = fromXYZRPY_mm_deg(offset.x, offset.y, offset.z, rot.x, rot.y, rot.z)
world      = pickPose * offsetPose
```

- `worldYaw = -calibrator.rotateImageToRobot(point_angle)`.
- The offset is applied in the **TOOL** frame — hence the post-multiply.
- Rotation composes as roll (RX) / pitch (RY) / yaw (RZ), the existing
  `fromXYZRPY_mm_deg` convention, confirmed against the robot.

Output axes:

| Axis | Source |
|---|---|
| `x`, `y`, `z` | the composed translation |
| `rx`, `ry` | the pattern's rotation offset, directly |
| `rz` | `worldYaw + offset.z` — pick yaw and the pattern's Z offset both act about Z, so they add |

**Zero-offset behaviour is preserved exactly, and provably.** With RPY(0, 0, r),
`(pickPose * offsetPose).translation = p + Rz(r)·o`, which is what the previous
`Calibrator::translateWithZAxis(A, offset, r, false)` computed. The equivalence is
algebraic, not established by sampling, so it holds for every existing pattern.

## 5. Wire format — 6 axes per position

`vc::device::VisionOutputPosition`
([vision_output_request.h](../../../src/device/output_device/vision_output_request.h))
emits six `%08.2f` fields per position. Frame:

```
"{detected},{x,y,z,rx,ry,rz},{x,y,z,rx,ry,rz},…;"
```

### ⚠ This was 4 axes before Phase 5, and the change is not backward-tolerant

The old `r` and the new `rz` are the **same axis** — rotation about Z. `r` was
dropped; `rz` carries it, now including the pattern's Z rotation offset. `rx`/`ry`
are genuinely new.

A robot program still reading 4 fields per position does **not** merely ignore the
extras: it strides a flat field list (`GR[base + i*4 + n]`), so it **misaligns and
reads wrong coordinates**. Six axes means stride 6 and a correspondingly larger
register window.

The robot program (`.prg`) is **user-owned and out of scope** for this codebase by
explicit decision; `tests/nachi_client/` is reference material only. Framing rules
are in [../../rules/design_rules.md](../../rules/design_rules.md) §13.4.

## 6. Pick-path waypoints: per-axis absolute semantics

`vc::device::PickPathPoint`
([vision_output_config.h](../../../src/device/output_device/vision_output_config.h))
carries six doubles plus six booleans: `absX`, `absY`, `absZ`, `absRoll`,
`absPitch`, `absYaw`.

- Flag **false** (default): the value is an offset from the pick pose, in the TOOL
  frame — the pre-Phase-5 behaviour.
- Flag **true**: the value is absolute, in the robot **base** frame.
- Absent keys default to `false`, so pre-Phase-5 configs stay all-relative.

Mixed absolute/relative cannot be expressed as a single matrix product.
`RobotKinematicPickingChecker::isPickable()` therefore composes the TOOL-frame offset
as usual, then **decomposes** the result to XYZ+RPY, patches each flagged axis, and
**rebuilds** the pose. Decomposition uses `canonicalEulerAngles(2,1,0)` (intrinsic
ZYX = `[yaw, pitch, roll]`) to match `fromXYZRPY_mm_deg`.

`Waypoint::allRelative()` short-circuits the whole decompose/patch/rebuild when no
axis is flagged, so the all-relative path returns bit-identical results to before.

## 7. Advisory pickability check

`ImageMatcher::robotPossiblePickingCheck(obj, pickingOffset, pickingRotationOffset)`
receives the pattern's 6-axis offset so its verdict reflects the pose the robot will
actually be commanded, not the bare match centre.

Before Phase 5 it evaluated the bare centre and ignored `m_pickingOffset` entirely —
including the XYZ part that already existed. So this also fixed a pre-existing
inaccuracy.

The offset travels as fields on `WorldPickPose` and is composed inside
`RobotKinematicPickingChecker`, which already links RobotKinematics. That keeps the
level-1 `matching` module free of pose maths and of the robot library — the point of
the `IRobotPickingChecker` port.

The check is **advisory**: it logs and emits, never blocks the payload.

## 8. Authoring UI

Both wizards ([add_pattern_wizard.h](../../../src/ui/forms/pattern/add_pattern_wizard.h),
[edit_pattern_wizard.h](../../../src/ui/forms/pattern/edit_pattern_wizard.h)) author
the full pick description: AddPatternWizard has 6 steps, EditPatternWizard 5.

- The **pick step** sets the pick point and the picking angle. Its canvas draws an
  X/Y orientation gizmo whose knob can be dragged to set the angle, and the pick
  point itself is draggable.
- The **box step** sets jaw size, distance and the jaw angle, with a preset picker.
- The **offset step** sets all six offset axes. It has no canvas: the offset is
  applied in the TOOL frame *after* the 2D match has been transformed to world
  coordinates, so there is nothing meaningful to draw over the image.
- Enter and Escape are deliberately inert in both wizards, so a stray keystroke
  cannot discard or prematurely commit a part-authored pattern.

Step counts, rails, subtitles and navigation bounds derive from one `STEP_COUNT`
constant plus named `STEP_*` indices per wizard. Inserting a step means adding its
title, subline and page — nothing counts steps by hand.

Gizmo geometry is sized in **widget** pixels, not image pixels: it communicates a
direction, so it must read the same at every zoom level.

### The overlay has exactly one implementation

The pick marker, the orientation gizmo and the jaw pair are painted by `pick_overlay`
([pick_overlay_painter.h](../../../src/ui/widgets/pick_overlay_painter.h)), shared by the
wizard canvas and the read-only pattern thumbnail.

Every function there takes **widget (viewport) pixels**. Callers do their own
image-to-widget mapping, so the painter knows nothing about zoom, pan, scene transforms or
crop rectangles. Sizes that must stay constant on screen are baked in as widget-pixel
constants; sizes that must track the image — the jaw boxes — are passed in pre-scaled.

Keep it that way. "Drawn the same way in both places" is a requirement that decays
silently the moment there are two implementations: this gizmo was revised twice during
Phase 5, and a second copy would have missed both revisions without anything failing.

### The pattern thumbnail is a viewer, not an editor

`PatternThumbnailView`
([pattern_thumbnail_view.h](../../../src/ui/widgets/pattern_thumbnail_view.h)) shows the
selected pattern with its pick geometry and supports pan, zoom and reset — and nothing
else. Read-only is enforced **by construction**, not by disabling behaviours one at a
time: the scene holds a single pixmap item with no interaction flags and no accepted mouse
buttons, and the view runs in `ScrollHandDrag`, so no code path can mutate a pattern.

The overlay is drawn in `drawForeground()` with the world transform reset, which is what
lets the gizmo hold a constant on-screen size while the jaw boxes stay locked to the image.
The view uses `FullViewportUpdate` for the same reason: the overlay is painted outside the
scene's coordinate system, so a partial repaint would clip it against the wrong rectangle.

Before this, the thumbnail displayed `MatchPattern::getImageWithPickPosition()`, which
burned the axes into a `cv::Mat` inside the **matching** module. That is UI drawing in a
non-UI module, and burned-in marks cannot hold a fixed on-screen size once the view can
zoom — so the change was a correctness requirement, not a preference. That method now has
no callers; see `later_todo_list.md` item 34.
