# Phase 5 Implementation Plan — Gripper Presets, 6-Axis Picking Offset, Absolute Path Axes

**Date:** 2026-07-29
**Status:** ALL PHASES CODE COMPLETE — A ✅, B ✅, C ✅ (confirmed on hardware),
D1–D5 ✅, F1–F5 ✅ (2026-07-29/30), E1 ✅ (2026-07-30). Phase F was opened from the
user's post-test adjustments and partially reverses B1/D1/D2/D4; Phase E ran last so it
documents the amended shape rather than the superseded one.

**What remains is verification the user owns**, not implementation. Checkpoint F is the
gate; within it the first item — loading a v1 project and confirming every jaw angle
survives — is the only one that can silently lose already-commissioned data.
**Source request:** [../request/phase_5_request.md](../request/phase_5_request.md)

> ⚠️ **The wire format has changed on the vision side.** Every position is now emitted as
> 6 fields (`x,y,z,rx,ry,rz`) instead of 4. The old `r` is emitted as `rz`. A robot
> program that still reads 4 fields per position will **misalign and read wrong
> coordinates** — it loops `pos_count` × 4 over a flat field list, so it does not merely
> ignore the extras. The robot side is user-owned (Q1); Checkpoint A was confirmed against
> the live robot on 2026-07-29.

> **Location note.** This file sits under `docs/history/`, which `AGENT.md` declares
> traceability-only. This document is an exception: it is the CURRENT plan for Phase 5,
> not history. If a future agent reads `AGENT.md` first and skips `docs/history/`, it
> will miss this plan. Consider moving this folder to `docs/plan/` and linking it from
> `docs/README.md` under "Active Planning And Backlog".

## Overview

Phase 5 reshapes how a pattern describes its gripper geometry and how a pick pose is
offset before it reaches the robot:

1. **Gripper geometry becomes a reusable value type.** The three loose picking-box
   fields on `MatchPatternConfig` move into `mtc::GripperBoxes`, which can be saved as a
   named preset and reused across patterns.
2. **The picking offset becomes 6-axis.** A pattern gains `m_pickingRotationOffset`
   (RX/RY/RZ, degrees) applied in the TOOL frame, alongside the existing XYZ
   `m_pickingOffset`.
3. **The output protocol becomes 6-axis.** `VisionOutputPosition` grows `rx`, `ry`, `rz`
   and the wire frame changes from `"N,x,y,z,r,…;"` to `"N,x,y,z,r,rx,ry,rz,…;"`.
4. **The advisory pickability check uses the same 6-axis offset**, so
   `ImageMatcher::robotPossiblePickingCheck` evaluates the pose the robot will actually
   be commanded to.
5. **Pick-path waypoints gain per-axis absolute/relative semantics** — six booleans on
   `PickPathPoint` switching an axis from "offset from pick pose" to "absolute value".

The collision check also becomes optional per pattern via `m_usePickingBox`.

## Resolved Decisions

| # | Decision | Consequence |
|---|---|---|
| **D1** | **Output is 6 axes: `x, y, z, rx, ry, rz`.** The 4-axis contract's `r` and the new `rz` are the same axis — the rotation about Z — so `r` is dropped and `rz` carries it, now including the pattern's Z rotation offset. `rx`/`ry` are genuinely new. | Protocol change. Touches the wire format, both vision-output test suites, `design_rules.md` §13.4, `uml/02_device_families.puml`, and the Nachi reference client. This is the highest-risk item in Phase 5 and is sequenced first. **Revised 2026-07-29** after the user's Checkpoint C hardware test: the plan had briefly carried `r` and `rz` as separate fields (7 total), which is redundant for a 6-DOF pose. Corrected to 6. |
| **D2** | **Runtime struct renames to `CollisionGeometry`.** `MatchedObject::GripperBox` → `CollisionGeometry`; the config type keeps the requested name `GripperBoxes`. | Removes the `CollisionBoxes`/`GripperBoxes` near-collision. `Geometry` = computed at match time, `Boxes` = configured template. |
| **D3** | **Presets are project-scoped.** Stored in the project file next to the pattern library. | Presets travel with the project; they do not persist across projects on one machine. |
| **D4** | **Back-compatible read + version key.** Legacy flat pattern keys are accepted on read; a `version` key is added to the pattern blob. | Deliberate, user-approved exception to the `AGENT.md` no-compat-shims rule, because real tested picking geometry exists in saved projects. |

## Amendments — post-test adjustments (2026-07-29)

Added to the request after the user tested Phases A–D. See
[../request/phase_5_request.md](../request/phase_5_request.md) → "Điều chỉnh yêu cầu".
These are tracked as **Phase F** below.

| # | Amendment | Consequence |
|---|---|---|
| **A1** | **`m_pickingBoxAngle` is per-pattern, not part of the shared preset.** Each pattern has its own jaw angle; putting it in `GripperBoxes` was an error in the original request. | **Partially reverses work already shipped this phase.** `GripperBoxes` drops `angle` (keeping `size` + `distance`), `MatchPatternConfig` gains `m_pickingBoxAngle` back as its own member, and the pattern JSON changes shape a second time. Fallout reaches B1, D1, D2, D4 and D5. |
| **A2** | Both wizards set the **picking angle alongside the picking position**, on the pick step. The picking angle is `MatchPatternConfig::m_angle` — a **different field** from the jaw angle in A1. | The pick step gains a control for `m_angle`, which today is reachable only from the property browser. The box step **keeps** its own `m_pickingBoxAngle` control; the two are unrelated and both remain editable. |
| **A3** | Both wizards gain a **new step after the picking-box step** for the 6-axis offset (X, Y, Z, RX, RY, RZ). | `m_pickingOffset` and `m_pickingRotationOffset` become authorable at creation time instead of property-browser-only. AddPatternWizard goes 5 → 6 steps; its step rail, navigation and validation are all count-driven. |
| **A4** | Both wizards must **not close on a stray Enter or Esc**. | Guard both key paths so a mis-keystroke cannot discard a half-finished pattern. |

**Schema impact of A1.** The pattern JSON will have carried three shapes:

| Version | Gripper geometry on disk |
|---|---|
| v0 (pre-Phase-5) | flat `pickingBoxSize` / `pickingBoxDistance` / `pickingBoxAngle` |
| v1 (current) | nested `gripperBoxes { w, h, distance, angle }` |
| v2 (after F1) | nested `gripperBoxes { w, h, distance }` + flat `pickingBoxAngle` |

Conveniently v2 reuses v0's key name for the angle, so one read rule covers all three:
take the angle from `gripperBoxes.angle` when present (v1), otherwise from the top-level
`pickingBoxAngle` (v0 **and** v2). `kSchemaVersion` must still go to 2 — a v1-era build
reading a v2 document would silently see `angle = 0` and ignore the top-level key, which
is exactly the silent-loss case the version guard exists to prevent.

**D5 — Rotation convention (assumed, not asked).** RX/RY/RZ are degrees composed via the
existing `RobotKinematics::Pose::fromXYZRPY_mm_deg` roll/pitch/yaw convention, matching
how `RobotKinematicPickingChecker::isPickable` already builds waypoint offsets. Flag if
the robot expects a different Euler order.

## Architecture Decisions

- **`GripperBoxes` lives in `src/matching/`, level 1.** Consumed by `matching` (geometry)
  and `ui` (editor + presets). `ui` may include `matching`; the reverse is forbidden by
  the include-layering contract.
- **The preset registry is a separate type from the geometry.** `GripperBoxes` stays a
  plain value type; the named-preset collection is a distinct owner so `matching` does
  not grow a registry concern it never reads at runtime.
- **Presets are copied on apply, not referenced.** A pattern stores resolved values, so
  deleting or editing a preset never silently mutates existing patterns.
- **The protocol change lands first, carrying zeros.** Task A2 changes the wire format
  with `rx=ry=rz=0.00`, before any code can produce a non-zero rotation. The payload is
  trivially predictable, so the robot-side integration is validated in isolation — the
  riskiest coupling fails fast instead of surfacing mixed with pose-math bugs.
- **Rename is isolated from schema work.** A pure rename with zero behaviour change means
  a later build failure is attributable to real work, not rename fallout.
- **No new abstraction for "offset providers".** Per `AGENT.md`, the 6-axis offset is
  passed as a plain value until a second concrete implementation proves a shape is needed.

## Current State (verified against source)

| Entity | Location | Today |
|---|---|---|
| `MatchedObject::GripperBox` | [match_object.h:32](../../../src/matching/match_object.h#L32) | jaw geometry struct, member `m_gripperBox` |
| `m_pickingBoxSize/Distance/Angle` | [match_pattern_config.h:67-69](../../../src/matching/match_pattern_config.h#L67-L69) | three loose fields |
| `m_pickingOffset` | [match_pattern_config.h:72](../../../src/matching/match_pattern_config.h#L72) | `cv::Point3f`, XYZ only |
| Pattern JSON | [pattern_group_manager.cpp:123-129](../../../src/matching/pattern_group_manager.cpp#L123-L129) | flat `pickingBoxSize`/`Distance`/`Angle` keys, **no `version` key** |
| Property rows | [localization_patterns_widget.cpp:272-331](../../../src/ui/forms/task/localization_patterns_widget.cpp#L272-L331) | 7 `PropSpec` entries |
| Wizard Step 4 | [add_pattern_wizard.h:111-118](../../../src/ui/forms/pattern/add_pattern_wizard.h#L111-L118) | `pickBoxW/H/Dist/Angle()` |
| `VisionOutputPosition` | [vision_output_request.h:22](../../../src/device/output_device/vision_output_request.h#L22) | 4 axes; `toString()` emits `"%08.2f"` ×4 |
| Result frame | [vision_output_request.h:47](../../../src/device/output_device/vision_output_request.h#L47) | `"{N},{pos},{pos},…;"`, each pos `x,y,z,r` |
| Nachi parser | [task1_main_channel.prg:213-222](../../../tests/nachi_client/task1_main_channel.prg#L213-L222) | **stride 4**: `GR[GR_POS_BASE + i*4 + 0..3]`, `GR_POS_BASE=30`, `MAX_POS=8` |
| `robotPossiblePickingCheck` | [image_matcher.cpp:1257](../../../src/matching/image_matcher.cpp#L1257) | takes only `const MatchedObject&` |
| `isPickable` | [robot_kinematic_picking_checker.cpp:189](../../../src/model/robot_kinematic_picking_checker.cpp#L189) | composes `pickPose * offset` in TOOL frame |
| `PickPathPoint` | [vision_output_config.h:63](../../../src/device/output_device/vision_output_config.h#L63) | 6 doubles + 3 posture labels |
| World point | [localization_runtime_controller.cpp:797-803](../../../src/model/localization_runtime_controller.cpp#L797-L803) | `translateWithZAxis` — Z-rotation only |

Three facts worth flagging up front:

- **The Nachi parser hard-codes stride 4.** `GR[GR_POS_BASE + i * 4 + n]` and the clear
  loop `FOR i = 0 TO MAX_POS * 4 - 1`. Six axes means stride 6 and `GR[30..77]` instead
  of `GR[30..61]`. `GB_*`/`GI_*` are separate register banks, so no collision there —
  but the robot's GR register count must be ≥ 78. See Q1.
- **Pattern JSON is unversioned.** `TaskLocalizeConfig` carries `version`/`kSchemaVersion`,
  but the pattern library is serialised separately under the `patternManager` key by
  `TaskLocalization::toJson()`. D4 adds one.
- **`ItemGripperBox` is unrelated.** [item_gripper_box.h](../../../src/ui/widgets/image_widget/item_gripper_box.h)
  is an independent `QGraphicsItem`. The rename must not touch it — a blind
  search-and-replace on "GripperBox" will.

## Dependency Graph

```
A1 rename CollisionGeometry ─── (independent)

A2 VisionOutputPosition 6 axes ──┐  (independent, zeros)
                                 │
B1 GripperBoxes value type ──┬── B2 rotation offset + usePickingBox
                             │        │
                             │        ├── C1 buildVisionOutputPositions ── (needs A2)
                             │        └── C2 robotPossiblePickingCheck
                             │                  └── C3 PickPathPoint absolute axes
                             │
                             └── D1 preset store
                                     ├── D2 Register Widget
                                     │      ├── D3 "Gripper" button
                                     │      └── D4 wizard preset picker
                                     └── D5 property-widget suggestions
```

---

## Phase A — Independent Foundations

### Task A1: Rename `MatchedObject::GripperBox` to `CollisionGeometry` — ✅ DONE (2026-07-29)

**Description:** Pure rename of the nested struct, its member, and its accessor. No
logic changes.

**Acceptance criteria:**
- [x] `GripperBox` → `CollisionGeometry`; `m_gripperBox` → `m_collisionGeometry`; `gripperBox()` → `collisionGeometry()`.
- [x] `ItemGripperBox` and `item_gripper_box.*` are untouched.
- [x] Matching output on a sample image is unchanged — *unchanged by construction (rename only, no logic touched); not separately re-run on a sample image.*

**Verification:**
- [x] Root app builds: Release build via `qmake` + `nmake /nologo -f Makefile.Release` in `build\Release` — `=== BUILD OK ===`, zero errors, `ncr_picking.exe` relinked 2026-07-29 09:58.
- [x] `grep -rn "GripperBox" src/` returns only `ItemGripperBox` hits (16 lines, all in `item_gripper_box.*`).

**Outcome notes:**
- Two method names were renamed alongside the struct to keep the grep criterion clean:
  `computeGripperBox()` → `computeCollisionGeometry()`, `drawGripperBoxToImage()` →
  `drawCollisionGeometryToImage()`.
- `ItemGripperBox` was confirmed untouched — it is an unrelated `QGraphicsItem`.
- Build environment note: the shell had none of the expected env vars set. Values used
  came from the machine's actual toolchain (Qt 6.8.3, VS2022 Community,
  `opencv_world4110`, `PylonBase_v10`), confirmed against the DLL already deployed next
  to the previous build.

**Dependencies:** None
**Files:** `match_object.h`, `image_matcher.cpp`, `vision_result_adapter.cpp`
**Scope:** S

### Task A2: Expand `VisionOutputPosition` to 6 axes (values still zero) — ✅ DONE (2026-07-29)

**Description:** Add `rx`, `ry`, `rz` to the struct and to `toString()`, changing the
wire frame to `"N,x,y,z,r,rx,ry,rz,…;"`. Nothing produces non-zero rotation yet, so every
emitted position ends `…,00000.00,00000.00,00000.00`.

**Scope change (Q1 decision):** the Nachi robot program is **out of scope**. `.prg` files
are not touched; the user updates the robot side. The GR-register capacity question is
therefore also the user's to resolve.

**Acceptance criteria:**
- [x] `toString()` emits 6 `%08.2f` fields per position, comma-separated, same zero-padding. *(Field count was corrected twice: the plan first said "8", which conflated the per-frame detected-count field with the per-position axes; it then carried 7 axes until the Checkpoint C hardware test showed `r` and `rz` are the same axis. Final: 6.)*
- [x] Both vision-output test suites pass with updated expected payloads (5 assertions changed).
- [x] `design_rules.md` §13.4 and `uml/02_device_families.puml` describe the 6-axis frame.
- [~] ~~Nachi parser stride 6~~ — dropped per Q1; user-owned.
- [~] ~~`MAX_POS` fits the GR register file~~ — dropped per Q1; user-owned.

**Verification:**
- [x] `tests/vision_output_device_test`: **9 passed, 0 failed** (806 ms).
- [x] `tests/vision_tcpip_client_device_test`: **10 passed, 0 failed** (1489 ms).
- [x] Root app rebuilds clean after the struct widened — `=== BUILD OK ===`, exe relinked 2026-07-29 10:16.
- [ ] Manual: real robot receives a frame and parses positions with the trailing zeros — **user-owned, pending** (robot program not yet updated).

**Outcome notes:**
- Aggregate initialisers like `{1.0, 2.0, 3.0, 4.0}` still compile; `rx`/`ry`/`rz` fall
  back to their default member initialisers, so no call site needed changing.
- **Pre-existing breakage fixed in passing:** `tests/vision_tcpip_client_device_test.pro`
  still referenced pre-restructure paths (`src/logger/app_logger.*`,
  `src/qgadget_marco.h` — note the typo, `src/utils/meta_utils.h`) and would not build
  from the CLI at all. Corrected to `src/core/...` to match the sibling suite's `.pro`.
  Unrelated to Phase 5, but it blocked this task's verification.
- The doc-comment on `VisionOutputPosition` now warns that widening the struct changes
  the wire frame and the robot parser must move in step.

**Dependencies:** None
**Files:** `vision_output_request.h`, `tests/vision_output_device_test/main.cpp`, `tests/vision_tcpip_client_device_test/main.cpp`, `tests/vision_tcpip_client_device_test/vision_tcpip_client_device_test.pro`, `design_rules.md`, `uml/02_device_families.puml`
**Scope:** M

### Checkpoint A — ✅ PASSED (2026-07-29), one user-owned item outstanding
- [x] Root app builds; both vision-output suites pass (9 + 10, zero failures).
- [x] `architecture_contract_test` passes: **41 passed, 0 failed** (1321 ms), built beside its own `.pro` under `tests/architecture_contract_test/build/msvc_release`.
- [ ] **Robot-side integration with the zero-rotation frame — user-owned (Q1).** Phase C
      will start producing non-zero `rx`/`ry`/`rz`, so the robot program should be updated
      before then; until it is, the emitted trailing zeros keep the old behaviour intact
      on the vision side but the robot is reading 7 fields where it expects 4.

**Build recipe used (all three suites):** `vcvars64.bat`, then `qmake … -spec win32-msvc
CONFIG+=release`, then `nmake /nologo -f Makefile.Release compiler_moc_source_make_all`
(tests only), then `nmake /nologo -f Makefile.Release`. Run test exes with
`QT_QPA_PLATFORM=minimal` and `-o results.txt,txt` — `app_logger` swallows stdout, so the
`-o` file is the only reliable verdict source.

---

## Phase B — Pattern Schema

### Task B1: Introduce `mtc::GripperBoxes`; move the three fields into it — ✅ DONE (2026-07-29), ⚠️ AMENDED BY F1

> **Superseded in part.** Amendment A1 moves `angle` back out of `GripperBoxes` onto the
> pattern. The type, the nested JSON object and the version key all stand; only the angle
> field moves. Read this task together with F1.

**Description:** New value type holding `size`, `distance`, `angle`. `MatchPatternConfig`
replaces its three loose fields with one `GripperBoxes m_gripperBoxes`. JSON gains a
nested `gripperBoxes` object plus a `version` key; legacy flat keys are accepted on read
(D4).

**Acceptance criteria:**
- [x] `MatchPatternConfig` exposes `m_gripperBoxes`; the three old members are gone — repo-wide grep for `m_pickingBoxSize|m_pickingBoxDistance|m_pickingBoxAngle` returns nothing.
- [x] Copy ctor and `operator=` carry the new member.
- [x] JSON writes nested `gripperBoxes` + `version`; `PatternGroupManager::fromJson()` refuses a document whose `version` exceeds `kSchemaVersion` (= 1) with a `LOG_USER_ERR`, matching `TaskLocalizeConfig`.
- [x] Property rows, both wizards, and `ImageMatcher::matching()` read through the new type.
- [ ] A pre-Phase-5 project restores identical geometry from the flat keys — **pending: needs a real project loaded in the app** (see the verification hook below).

**Verification:**
- [x] Root app builds clean after the type change.
- [ ] Save/reload round-trip preserves every pattern's geometry — **pending the same app run**.

**Outcome notes:**
- `GripperBoxes` is a plain value type in `src/matching/gripper_boxes.h` (level 1); JSON
  stays in `pattern_group_manager.cpp` per design_rules §5.2, so the type carries no Qt.
- `kSchemaVersion = 1` added to `PatternGroupManager`. v0 = a document with no `version`
  key. The schema-history block above `patternConfigToJson()` documents both shapes and
  records the D4 exception so a later reader does not "clean up" the legacy branch.
- Registered `gripper_boxes.h` in `matching.pri` **and** in the architecture contract
  test's `.pro` header list.
- **Verification approach changed at the user's request.** A synthetic round-trip harness
  was built first; the user preferred verifying against real project files instead. A
  temporary `phase5VerifyPatternSchema()` now runs at the end of
  `MainWindow::onOpenProject()` and logs, per localization task: every pattern's loaded
  gripper geometry (proving the v0 flat-key read), then a `toJson()`→`fromJson()`
  round-trip comparison. It is one contiguous, clearly-marked block in `mainwindow.cpp`
  plus a single call site — **the user removes it once Phase B is confirmed.**

**Dependencies:** None
**Files:** new `src/matching/gripper_boxes.h`, `matching.pri`, `match_pattern_config.h/.cpp`, `pattern_group_manager.h/.cpp`, `image_matcher.cpp`, `localization_patterns_widget.cpp`, `architecture_contract_test.pro`, `app/mainwindow.cpp` (temporary hook)
**Scope:** M

### Task B2: Add `m_pickingRotationOffset` and `m_usePickingBox` — ✅ DONE (2026-07-29)

**Description:** Add the RX/RY/RZ rotation offset (`cv::Point3f`, degrees) and the
per-pattern collision toggle. Wire `m_usePickingBox` into `ImageMatcher::matching()` so a
disabled pattern skips the collision test.

**Acceptance criteria:**
- [x] Both fields serialise, deserialise, and survive copy/assign.
- [x] `m_usePickingBox == false` ⇒ `checkCollisionObject2` is not called; the object keeps its default `Outside`/no-collision state and is never rejected for collision.
- [x] Default `m_usePickingBox == true`, and `fromJson` defaults it to `true` when the key is absent, so v0 documents keep today's behaviour.
- [x] Property browser exposes 3 rotation rows + 1 bool row.
- [ ] Toggle-off behaviour confirmed on a real matching run — **pending user check**.

**Verification:**
- [x] Root app builds clean.
- [x] `architecture_contract_test`: **41 passed, 0 failed** after Phase B.
- [ ] Toggle off on a pattern known to collide; it becomes pickable — **pending user check**.

**Outcome notes:**
- The collision *geometry* is still computed when the toggle is off, so the result overlay
  keeps drawing the jaws; only the verdict is opt-out. The reasoning is in a comment at
  the call site in `image_matcher.cpp`.
- `pickingRotationOffset` serialises as `{ "rx", "ry", "rz" }` and defaults to zero on v0
  documents, which is a behavioural no-op — nothing consumes it until Phase C.

**Dependencies:** B1
**Files:** `match_pattern_config.h/.cpp`, `pattern_group_manager.cpp`, `image_matcher.cpp`, `localization_patterns_widget.cpp`
**Scope:** M

### Checkpoint B — partially verified (2026-07-29)
- [x] Contract test passes (41/41); root app builds clean.
- [ ] **Pre-Phase-5 project loads with identical geometry — awaiting a real project load.**
      Open an existing project in the app and read the `[Phase5]` lines in the System Log:
      each pattern's `size/distance/angle` must match what was configured before, and every
      task must report `round-trip OK`.

**Debt found while implementing Phase B** (logged in `later_todo_list.md`, not fixed here):
`MatchBoxGripper` is dead code, and `pattern_group_manager.h` pulls `QMessageBox` into the
non-UI matching module.

---

## Phase C — Pose Math

### Task C1: Apply the rotation offset in `buildVisionOutputPositions` — ✅ DONE (2026-07-29)

**Description:** Replace the Z-only `translateWithZAxis` call with a full TOOL-frame
composition honouring RX/RY/RZ, and populate the `rx`/`ry`/`rz` fields added in A2.

**Acceptance criteria:**
- [x] Zero rotation offset ⇒ world X/Y/Z/R identical to today — **guaranteed by construction**, see below.
- [x] Non-zero RX/RY/RZ shifts the world point per the D5 convention and fills the new output axes.
- [x] The result-table row and the sent position stay consistent — both read the same `row.world`.

**Verification:**
- [x] Root app builds; contract test 41/41.
- [ ] Simulated cycle comparing the result table against hand-computed values — **pending user check on real hardware**.

**Outcome notes:**
- Implemented as `world = pickPose * offsetPose` using `RobotKinematics::Pose`, the same
  type and convention `isPickable` already used. Reusing the library rather than
  hand-rolling RPY maths is what removes the "frame/convention mismatch" risk.
- **Zero-offset equivalence is algebraic, not empirical.** `pickPose` has RPY(0,0,r), so
  `(pickPose * offsetPose).translation = p + Rz(r) · o`, and `Rz` rotates x/y while
  passing z through — exactly what `Calibrator::translateWithZAxis(A, offset, r, false)`
  computed. Identical for every existing pattern.
- **Wire semantics — confirmed on hardware 2026-07-29 and corrected.** `x/y/z` carry the
  composed translation. Orientation is `rx`/`ry` from the pattern's rotation offset and
  `rz = worldYaw + offset.z`: the pick yaw and the pattern's Z offset both act about Z, so
  they add into one axis. There is no separate `r` — the earlier 7-field split kept `r`
  and `rz` apart, which is redundant for a 6-DOF pose and was the plan's error, not the
  code's. A pattern with no rotation offset emits `rx = ry = 0` and `rz` equal to the
  pre-Phase-5 `r`, so existing behaviour is preserved exactly.
- `Calibrator::translateWithZAxis()` is now unused by this path but left untouched
  (other callers may exist; out of scope).

**Dependencies:** A2, B2
**Files:** `localization_runtime_controller.cpp`, `match_object.h`, `image_matcher.cpp`
**Scope:** M

### Task C2: Pass the 6-axis pattern offset into `robotPossiblePickingCheck` — ✅ DONE (2026-07-29)

**Description:** Give the advisory check the pattern's XYZ + RX/RY/RZ offset so its
verdict reflects the pose actually commanded, not the bare match centre.

**Acceptance criteria:**
- [x] `robotPossiblePickingCheck(obj, pickingOffset, pickingRotationOffset)` takes the offset explicitly (as the request asked) and hands it to the checker.
- [x] Zero offset ⇒ same verdict as before: the composed `basePose` equals `pickPose` when the offset is identity.
- [ ] A non-zero RX/RY that pushes the pose out of reach flips the verdict — **pending user check**.

**Verification:**
- [x] Root app builds; contract test 41/41.
- [ ] Matching run comparing possible-pick counts at zero vs non-zero offset — **pending user check**.

**Outcome notes:**
- The offset travels as six new fields on `WorldPickPose` and is composed inside
  `RobotKinematicPickingChecker` (which already links RobotKinematics). This keeps the
  level-1 `matching` module free of pose maths and of the robot library, which is the
  whole point of the `IRobotPickingChecker` port.
- Before this task the advisory check evaluated the **bare match centre** — it ignored
  `m_pickingOffset` entirely, even the XYZ part that already existed. So this also fixes
  a pre-Phase-5 inaccuracy, not just the new rotation.
- The contract test only uses `IRobotPickingChecker` as a queued-signal parameter type, so
  the widened struct needed no test changes.

**Dependencies:** B2
**Files:** `image_matcher.h/.cpp`, `match_object.h`, `robot_picking_checker.h`, `robot_kinematic_picking_checker.cpp`
**Scope:** M

### Task C3: `PickPathPoint` per-axis absolute flags — ✅ DONE (2026-07-29)

**Description:** Add `absX`, `absY`, `absZ`, `absRoll`, `absPitch`, `absYaw`. In
`isPickable`, compose the TOOL-frame offset as today, then override each flagged axis
with its absolute value. Mixed absolute/relative cannot be expressed as a single matrix
product — decompose the composed pose to XYZ+RPY, patch per axis, rebuild.

**Acceptance criteria:**
- [x] All six flags serialise/deserialise; absent keys default `false`, so pre-Phase-5 configs stay all-relative.
- [x] All flags false ⇒ verdict identical to pre-change: `Waypoint::allRelative()` short-circuits the decompose/patch/rebuild entirely, leaving the exact pose the product produced.
- [x] The check widget lets the operator set each flag per waypoint.
- [ ] `absZ = true, dz = 500` evaluated at Z = 500 regardless of pick height — **pending user check**.

**Verification:**
- [x] Root app builds; contract test 41/41.
- [ ] Manual: configure an absolute-Z approach waypoint and confirm the reported pose — **pending user check**.

**Outcome notes:**
- Decomposition uses `canonicalEulerAngles(2,1,0)` (intrinsic ZYX = `[yaw, pitch, roll]`)
  and rebuilds via `fromXYZRPY_mm_deg`, matching Q3's confirmed roll/pitch/yaw convention.
- Absolute values are interpreted in the robot **base** frame per Q2.
- Pick-path table went from 10 to 16 columns: each axis is now followed by its own
  "Abs …" checkbox, so the toggle sits next to the value it governs. Column indices moved
  behind two named constants (`kPathFirstAxisCol`, `kPathFirstPostureCol`) instead of the
  magic numbers the read/write paths used before.

**Dependencies:** C2
**Files:** `vision_output_config.h`, `robot_kinematic_picking_checker.h/.cpp`, `robot_kinematic_check_widget.cpp`
**Scope:** M

### Checkpoint C — ✅ PASSED (2026-07-29, confirmed on hardware)
- [x] Zero-offset regression holds on all three paths, each by construction rather than by
      sampling: C1 algebraically reduces to the old `Rz` translate, C2's composition is
      identity at zero offset, C3 skips its patch step when no axis is absolute.
- [x] Root app builds; `architecture_contract_test` 41/41; `vision_output_device_test` 9/9.
- [x] **Reviewed with the user on real hardware.** The review caught the frame-convention
      error the checkpoint existed to catch — but in the *plan*, not the code: `r` and `rz`
      were the same axis all along, so the output needed 6 fields, not 7. Cleanup applied
      2026-07-29; the field is gone from `VisionOutputPosition` and every consumer.

**Follow-ups opened by this phase** (logged in `later_todo_list.md`, not fixed here):
- Item 32 — `test_disconnect_notice_on_graceful_close` is flaky. Traced during Phase C to
  the product side: the graceful close `abort()`s the heartbeat socket right after writing
  the notice, and the resulting RST can discard the peer's unread buffer. Pre-existing;
  nothing in Phase 5 touches that path.
- Item 33 — ✅ **closed 2026-07-29.** `VisionTcpipDeviceBase::runKinematicCheck()` had
  hard-coded a top-down orientation and ignored `rx`/`ry`, a gap Phase 5 itself opened. Now
  builds `fromXYZRPY_mm_deg(x, y, z, 180 + rx, ry, rz)`. Exact, not approximate: rotations
  about a shared axis commute, so folding the top-down flip into the roll term gives the
  same pose as post-multiplying it, and at `rx = ry = 0` it reduces to the old form.

---

## Phase D — Preset Registry and UI

### Task D1: `GripperBoxes` preset store — ✅ DONE (2026-07-29), ⚠️ AMENDED BY F2

> **Superseded in part.** A preset no longer carries an angle (amendment A1); see F2.

**Acceptance criteria:**
- [x] Presets persist in the project file across save/reload (D3) — serialised by `TaskLocalization::toJson()` under `"gripperPresets"`; absent key loads as an empty store, so pre-Phase-5 projects are unaffected.
- [x] Names unique; blank and duplicate names are rejected by the store (`add()`/`rename()` return false) so the caller can report the failure rather than silently de-duplicating.
- [x] Deleting a preset does not mutate patterns already using its values — presets are copied on apply, never referenced.
- [ ] User-visible rejection messages — **pending D2**, which owns the UI that reports them.

**Verification:**
- [x] Root app builds; `architecture_contract_test` 41/41.
- [ ] Round-trip across a real save/reload — **pending D2**, since nothing can create a preset until the register widget exists.

**Outcome notes:**
- Lives in `model`, not `matching`: `matching` never reads presets at runtime, and keeping
  the registry out of it preserves the "level-1 module owns matching only" boundary.
  `mtc::GripperBoxes` itself stays in `matching` because `MatchPatternConfig` embeds it.
- Deliberately a plain value type — no `QObject`, no signals. The register widget will edit
  a copy and hand it back via `setGripperPresets()`, so there is no change-notification
  contract to maintain.
- `fromJson()` skips blank/duplicate entries instead of failing the load: a hand-edited
  project file degrades to a shorter preset list rather than breaking the whole task.

**Dependencies:** B1
**Files:** new `src/model/gripper_preset_store.h/.cpp`, `model.pri`, `task_localization.h/.cpp`, `architecture_contract_test.pro`
**Scope:** M

### Task D2: Gripper Type Register Widget — ✅ DONE (2026-07-29), ⚠️ AMENDED BY F2

> **Superseded in part.** The Angle column goes away (amendment A1); see F2.

**Description:** Table dialog for preset CRUD — name, width, height, distance, angle.
Structure in `.ui`, behaviour in `.cpp`, styling in `.qss` with theme tokens.

**Acceptance criteria:**
- [x] Add/remove rows; every cell editable (name as a text item, the four axes as spin boxes).
- [x] Invalid values rejected with a visible message — blank and duplicate names are caught on Save, the offending row is selected, and the reason appears in an inline label.
- [x] No inline `setStyleSheet`; the error/note distinction is a `messageKind` property styled from the global sheets using tokens.
- [ ] Visual check in both themes — **pending user check**.

**Outcome notes:**
- A `QDialog` parented to `LocalizationPatternsWidget`, so it inherits that widget's
  stylesheet. That is why it needs **no new `.qss` pair and no `.qrc` entries** — one less
  file pair to keep in sync across theme changes.
- Validation reports inline rather than through a message box: a rejected row keeps the
  user in the table instead of interrupting the edit.
- `onSave()` rebuilds a fresh store and lets `GripperPresetStore::add()` decide, rather
  than re-implementing the uniqueness rule in the widget. One rule, one place.

**Dependencies:** D1
**Files:** new `src/ui/forms/pattern/gripper_register_dialog.{ui,h,cpp}`, `ui.pri`, `dark.qss`, `light.qss`
**Scope:** M

### Task D3: "Gripper" button in the pattern library — ✅ DONE (2026-07-29)

**Acceptance criteria:**
- [x] Button on the same row as Add Group / Auto Sort — added to `FooterItemWidget` with the same `patternAction` property, height and cursor.
- [x] Matches their styling: `add`/`sort` have no dedicated QSS rules (only `delete`/`edit` do), so the new button picks up the same default `QPushButton` look automatically.
- [x] Click opens the register dialog modally; accepting stores the edited set on the task, cancelling changes nothing.

**Dependencies:** D2
**Files:** `pattern_tree_widget.h/.cpp`, `localization_patterns_widget.h/.cpp`
**Scope:** S

### Task D4: Wizard preset picker at the picking-box step — ✅ DONE (2026-07-29), ⚠️ AMENDED BY F2/F3/F4

> **Superseded in part.** A preset now fills three values instead of four (A1), the angle
> moves to the pick step (A2), and a new offset step follows this one (A3).

**Acceptance criteria:**
- [x] Step 4 offers existing presets or Custom; an empty store leaves the selector showing Custom only.
- [x] Selecting a preset fills the four spin boxes, which drive the canvas preview through the existing `onBoxChanged()` path.
- [x] Editing after selection silently reverts the selector to Custom — covered for both spin-box edits and canvas drags.

**Outcome notes:**
- Presets arrive through `setGripperPresets()` called before `exec()`, rather than a new
  constructor parameter — the wizard has existing callers and this keeps them untouched.
- The wizard still returns whatever the spin boxes hold. A preset is a starting value, never
  a reference, so nothing downstream has to know a preset was involved.

**Dependencies:** D2
**Files:** `add_pattern_wizard.h/.cpp`, `localization_patterns_widget.cpp`
**Scope:** M

### Task D5: Preset suggestions in the property widget — ✅ DONE (2026-07-29)

**Acceptance criteria:**
- [x] A preset can be applied to the bound pattern from the property panel via an "Apply Gripper Preset" button that pops up the registered presets.
- [x] Applying commits through `commitWorkingPatternConfig()` — the same path every other property edit uses — then rebuilds the browser so the spin rows show the new values.
- [x] Empty store and "no pattern selected" both report why instead of doing nothing silently.

**Outcome notes:**
- Implemented as a button + menu rather than a `PropSpec` row: the spec table supports
  Double/Int/Bool only, and widening it for one list-valued field would have been a bigger
  change than the feature warrants.

**Dependencies:** D1
**Files:** `localization_patterns_widget.h/.cpp`
**Scope:** S

### Checkpoint D — code complete (2026-07-29), manual pass outstanding
- [x] Root app builds; `architecture_contract_test` 41/41.
- [ ] Full flow: register preset → create pattern with it → run matching → correct boxes drawn — **pending user check**.
- [ ] Both themes verified — **pending user check**.
- [ ] This also closes the two D1 criteria that needed a UI to exercise: user-visible
      rejection messages, and a preset round-trip across a real save/reload.

---

## Phase F — Post-test Adjustments

Sequenced before Phase E so the docs describe the final shape. F1 lands first because
every other task in this phase reads the field it moves.

### Task F1: Move the jaw angle out of `GripperBoxes` and back onto the pattern — ✅ DONE (2026-07-29)

**Description:** `GripperBoxes` keeps `size` and `distance` only. `MatchPatternConfig`
regains `m_pickingBoxAngle`. Pattern JSON goes to v2 per the schema table above, with one
read rule spanning v0/v1/v2.

**Acceptance criteria:**
- [x] `mtc::GripperBoxes` has `size` and `distance`; no `angle`.
- [x] `MatchPatternConfig::m_pickingBoxAngle` exists, is copied by the copy ctor/assignment, and drives `computeCollisionGeometry()`.
- [x] `kSchemaVersion` = 2; a v3+ document is still refused.
- [x] A v0 project loads its angle from the flat key; a **v1 project loads its angle from `gripperBoxes.angle`**.
- [x] Writes emit the v2 shape only.

**Verification:**
- [x] Root app builds (`=== BUILD OK ===`); `architecture_contract_test` **41 passed, 0 failed** (1342 ms).
- [ ] Load a v1 project saved during Phase A–D testing and confirm each pattern's jaw angle survives — **user-owned, pending**.

**Outcome notes:**
- The v0/v1/v2 read collapsed to one branch plus one conditional, exactly as the schema
  table predicted: `gripperBoxes.angle` when that nested key is present (v1 only),
  otherwise the flat `pickingBoxAngle` — which v0 and v2 share.
- The temporary `phase5VerifyPatternSchema()` hook in `mainwindow.cpp` was updated to read
  the new field, so the `[Phase5]` log lines still prove the v1 → v2 migration on a real
  project load. Still the user's to delete.

**Dependencies:** None (first in phase)
**Files:** `gripper_boxes.h`, `match_pattern_config.h/.cpp`, `pattern_group_manager.h/.cpp`, `image_matcher.cpp`, `app/mainwindow.cpp` (temporary hook)
**Scope:** M

### Task F2: Follow the angle move through the preset UI — ✅ DONE (2026-07-29)

**Description:** Presets no longer carry an angle, so the register dialog, the wizard
preset picker and the property browser all shed it.

**Acceptance criteria:**
- [x] `GripperPreset` JSON has no `angle`; a stored `angle` key from the current build is ignored on read rather than failing.
- [x] Register dialog drops its Angle column (4 columns: name, width, height, distance).
- [x] Wizard preset selection fills width/height/distance only and leaves the pattern's own jaw angle alone.
- [x] Property browser still exposes the jaw angle, now bound to `m_pickingBoxAngle`.
- [x] Applying a preset from the property panel does not overwrite the pattern's jaw angle — true **by construction**: the assignment is `m_gripperBoxes = preset->boxes`, and the angle is no longer in that type.
- [x] **EditPatternWizard gains the same preset picker as AddPatternWizard** (Q5), seeded from the task's store and defaulting to Custom for an existing pattern.

**Outcome notes:**
- The "revert to Custom on manual edit" watch list now excludes the angle spin box in both
  wizards. A preset supplies no angle, so changing it cannot invalidate the selection —
  reverting there would have been a lie about what the preset covers.
- The register dialog needed no `.ui` change: the table's column count and headers are set
  from the `.cpp`, so dropping a column is one constant and one header list.

**Dependencies:** F1
**Files:** `gripper_preset_store.h/.cpp`, `gripper_register_dialog.cpp`, `add_pattern_wizard.cpp`, `edit_pattern_wizard.h/.cpp`, `localization_patterns_widget.cpp`
**Scope:** M

### Task F3: Set the picking angle (`m_angle`) on the pick step (both wizards) — ✅ DONE (2026-07-29)

**Description:** The pick step authors the pick position **and** `MatchPatternConfig::m_angle`
together. This is a different field from the jaw angle F1 moves — see the behaviour note
below, which is what makes `m_angle` the right home for "picking angle".

**What `m_angle` actually does (verified in source, 2026-07-29):**
- It is used in exactly one place: `mo.computePointAngle(cfg->m_angle)` at
  [image_matcher.cpp:772](../../../src/matching/image_matcher.cpp#L772), giving
  `point_angle = matched_Angle + m_angle`. So it is a constant added to the reported angle
  of every match from that pattern — precisely "the angle at which this part is picked".
- It does **not** steer the search. The angle sweep is built from `m_toleranceAngle` around
  **zero** ([image_matcher.cpp:535-541](../../../src/matching/image_matcher.cpp#L535-L541)),
  not around `m_angle`. Exposing it in the wizard therefore cannot narrow or shift matching.

**Two existing descriptions are wrong and mislead about this — fix them in this task:**
- The property-browser tooltip calls it *"Search center angle in degrees"*. It is not a
  search parameter at all.
- `match_pattern_config.h` documents `m_toleranceAngle` as tolerance *"around m_angle"*.
  The sweep is around zero.

**Acceptance criteria:**
- [x] AddPatternWizard and EditPatternWizard expose an angle control on the pick step, bound to `m_angle`.
- [x] The value round-trips: wizard → `m_angle` → reopening the edit wizard shows it.
- [x] The box step still offers `m_pickingBoxAngle`; the two controls are distinct and both work.
- [x] The pick-step canvas indicates the chosen angle, and the angle can be set **directly on the image** (see the gizmo revision below).
- [x] The misleading descriptions above are corrected.

**Verification:**
- [x] Root app builds; contract test 41/41.
- [ ] Set a non-zero picking angle, run a match, and confirm every reported angle shifts by exactly that amount while the detected count is unchanged (proving it did not alter the search) — **user-owned, pending**.

**Outcome notes:**
- **Three descriptions were wrong, not two.** The property browser's *tolerance* row also
  claimed the deviation was "around the center angle". All three now state the real
  behaviour, and the two spin rows explicitly say they are independent of each other.
- The property row is relabelled from "Angle (°)" to "Picking Angle (°)" so the browser and
  the wizard name the same field the same way.
- The orientation marker is a new `AddPatternImageCanvas::setPickAngle()`. Drawn in widget
  space at a fixed length: it conveys a direction, not a distance, so it must stay legible
  at any zoom.

**Gizmo revision — requested by the user after review (2026-07-30).** The first cut was a
single display-only arrow. Two shortcomings, both fair:

1. **One arrow does not show the frame.** A lone arrow says where X points but nothing
   about Y, so the operator cannot tell which way round the part is actually gripped. It is
  now an **X/Y arrow pair** — X red along the angle, Y green 90° clockwise from it. `+90`,
  not `-90`: the canvas works in image coordinates where Y grows downward, so this renders
  as the familiar X-right/Y-down frame and matches the convention the matcher reports
  angles in.
2. **Typing a number is not how you set an orientation.** The gizmo now carries a **drag
   knob** past the X tip, so the angle is set on the image. `pickAngleChanged(double)` was
   added and both wizards echo it into their angle spin box (signal-blocked, so the spin
   box does not push the value back mid-drag). `setPickAngle()` deliberately does **not**
   emit — it is the host-driven direction.

Two details worth keeping:
- **The knob is hit-tested before the pick point.** In Pick mode a left-click places the
  pick, so without priority the click that grabs the knob would first teleport the pick
  under the cursor — the opposite of what grabbing a rotation handle should do.
- **The gizmo is drawn in Pick and Finish, never in Box.** The jaw pair already owns a
  rotation handle in Box mode, and two rotation gizmos around one pick point leave the user
  guessing which angle a drag is changing.

**Gizmo revision 2 — second review pass (2026-07-30).** Three further fixes, all usability:

3. **Axes now run both ways through the pick point.** Each is a full line spanning the axis
   length either side, not a half-arrow. The negative half is drawn thinner and translucent
   so the positive direction is still unambiguous — otherwise a symmetric cross says nothing
   about which way X points. The **pick ring is now drawn after the gizmo** in both Pick and
   Finish modes, so the centre marker covers the crossing instead of the two lines knotting
   over it. The layering is load-bearing, so `drawPickOrientation()` documents that it must
   be called before the ring.
4. **The knob and its angle readout moved off the axes**, onto the X/Y bisector
   (`m_pickAngle + 45°`), at a radius just inside the axis length so the gizmo's envelope
   does not grow. Before this the knob, its connector and the readout all sat on the X axis,
   on top of the arrow and the "X" letter.
5. **The pick point is now draggable, not click-only.** Press places it and stays armed, so
   the same gesture can be dragged. Click-only meant converging on a point by re-clicking.

Two consequences worth recording:
- **The knob's drag math needed a correction after all.** With the knob on the bisector, the
  cursor's bearing is `m_pickAngle + 45°`, so the drag subtracts `kPickHandleBearing`. The
  first revision had it on the X axis and therefore needed none; that note is now obsolete
  and the code comment says which correction applies and why.
- **The clamp rule now lives in one place.** `setPickClamped()` replaced three copies of the
  same crop-or-image bounds logic (Pick press, the new Pick drag, Box-mode `BH_Pick`). Two
  copies were tolerable; adding a third for the drag path was not.

**Files (revisions):** `pattern_canvas.h/.cpp`, `add_pattern_wizard.cpp`, `edit_pattern_wizard.cpp`

**Dependencies:** F1
**Files:** `add_pattern_wizard.h/.cpp`, `edit_pattern_wizard.h/.cpp`, `pattern_canvas.h/.cpp`, `localization_patterns_widget.cpp`, `match_pattern_config.h`
**Scope:** M

### Task F4: New offset step after the picking-box step (both wizards) — ✅ DONE (2026-07-29)

**Description:** A step for `m_pickingOffset` (X/Y/Z) and `m_pickingRotationOffset`
(RX/RY/RZ), so a pattern can be authored complete instead of needing a property-browser
visit afterwards.

**Acceptance criteria:**
- [x] AddPatternWizard has 6 steps; the rail, navigation, per-step validation and header subtitles all account for the new one.
- [x] EditPatternWizard offers the same fields and seeds them from the existing pattern (and gains its own step, going 4 → 5).
- [x] Values reach `m_pickingOffset` / `m_pickingRotationOffset` on accept.
- [x] Leaving the step untouched yields zeros, i.e. today's behaviour.

**Verification:**
- [x] Root app builds; contract test 41/41.
- [ ] Author a pattern with a non-zero offset and confirm it lands in the config — **user-owned, pending**.

**Outcome notes:**
- Stayed inside the wizard files, so no split was needed.
- The hand-counted step numbers were the real risk here, not the new page. Both wizards now
  derive the rail, the navigation bounds, the "N of M" subtitle and the Apply-button switch
  from a single `STEP_COUNT` plus named `STEP_*` indices. Before this, four separate places
  in each file had the count written in — which is exactly how an inserted step goes wrong.
- The offset step has no canvas: the offset is applied in the TOOL frame *after* the 2D
  match is transformed to world coordinates, so there is nothing meaningful to draw over the
  image. The page explains that rather than showing a misleading overlay.
- EditPatternWizard's diff view gained rows for the picking angle and both offsets, and the
  jaw row is relabelled "JAW OFFSET" so it cannot be confused with the new "PICK OFFSET".

**Dependencies:** F1
**Files:** `add_pattern_wizard.h/.cpp`, `edit_pattern_wizard.h/.cpp`, `localization_patterns_widget.cpp`
**Scope:** L

### Task F5: Block accidental Enter/Esc close (both wizards) — ✅ DONE (2026-07-29)

**Description:** A stray Return or Escape must not discard a part-authored pattern.

**Acceptance criteria:**
- [x] Enter/Return does not accept or advance the dialog from any step.
- [x] Escape does not reject it; closing stays deliberate (the header close button / Cancel).
- [x] Text and spin-box editing still behave normally — the guard must not swallow keys the focused editor needs.

**Outcome notes:**
- Implemented as `keyPressEvent()` on each dialog, intercepting only `Key_Return`,
  `Key_Enter` and `Key_Escape` and forwarding everything else to `QDialog`. A dialog-level
  override is the right level: `QDialog` is where the Return→accept / Escape→reject mapping
  lives, and a focused editor gets first refusal on the key before the dialog ever sees it,
  so ordinary typing is untouched.

**Dependencies:** None
**Files:** `add_pattern_wizard.h/.cpp`, `edit_pattern_wizard.h/.cpp`
**Scope:** S

### Task F5b: Focus the Next button when either wizard opens — ✅ DONE (2026-07-31)

**Why this is reopened.** The user tested F5 and found Enter *and Space* still close both
wizards on step 1. F5's dialog-level `keyPressEvent` was not wrong, it was
**unreachable**: a focused `QPushButton` consumes Space and Return itself, so the event
never propagates up to the dialog. Cancel happened to hold initial focus, so the first
keystroke hit the one button that discards the work.

**Acceptance criteria:**
- [x] Both wizards focus the **Next** button when shown, on every step, not just the first.
- [x] Cancel and Back cannot be triggered by Return: `setAutoDefault(false)` on them, so a
      stray Enter can never reach a destructive action even if focus moves.
- [x] Escape still does nothing (unchanged from F5).

**Outcome notes:** the focus call went into `goToStep()`, which already runs on every step
change *and* on the constructor's initial `goToStep(0)` — one line, one place, no separate
`showEvent` override to keep in step with it.

**Note on what "blocked" now means.** With Next focused, Enter *advances* the wizard. That
is deliberate and matches the request — the requirement is that a stray key must not
**close** the dialog, not that Enter be globally inert. On the final step Next reads
"Apply", so Enter there commits; that is the one place Enter still closes the dialog, and
it is the step whose whole purpose is confirmation.

**Dependencies:** F5
**Files:** `add_pattern_wizard.cpp`, `edit_pattern_wizard.cpp`
**Scope:** S

### Task F6: Remove the "Apply Gripper Preset" button from the property panel — ✅ DONE (2026-07-31), reverts D5

**Description:** The user judged the property-panel preset action unnecessary. Presets are
already reachable where they are actually chosen: the register dialog and the box step of
both wizards.

**Acceptance criteria:**
- [x] The button, its slot, and its member are gone; no dead helper is left behind.
- [x] The preset store, the register dialog and the wizard pickers are untouched.

**Dependencies:** None
**Files:** `localization_patterns_widget.h/.cpp`
**Scope:** S

### Checkpoint F — code complete (2026-07-29), manual pass outstanding
- [x] Root app builds (`=== BUILD OK ===`); `architecture_contract_test` **41 passed, 0 failed**.
- [ ] A v1 project from Phase A–D testing loads with every jaw angle intact — **the one
      item worth checking first**, since it is the only F-phase change that can silently
      lose already-commissioned data. Open a project saved during Phase A–D testing and
      read the `[Phase5]` lines: each pattern's angle must match what was configured.
- [ ] Author a pattern end-to-end in both wizards — position + picking angle, preset box,
      offsets — and confirm every value lands in the config.
- [ ] Enter and Escape do nothing on every step of both wizards.
- [ ] The pick-step gizmo: drag the knob and confirm the spin box follows, that grabbing the
      knob does not move the pick point, and that the X/Y arrows read correctly at several
      zoom levels.
- [ ] Both themes verified (carried over from Checkpoint D).

---

## Phase G — Supplementary: pattern thumbnail viewer

Added to the request after F was tested (see
[../request/phase_5_request.md](../request/phase_5_request.md) → "Yêu cầu bổ sung").
Phase E already shipped; its docs get a follow-up edit at the end of this phase rather
than being re-run.

### Task G1: Replace the pattern thumbnail with a dedicated read-only view — ✅ DONE (2026-07-31)

**Description:** The selected-pattern thumbnail becomes a purpose-built `QGraphicsView`
that renders the pattern image plus the pick point, picking orientation and picking box —
drawn exactly as the wizard canvas draws them — with pan, zoom and reset-transform, and no
item interaction at all.

**Current state (verified in source):**
- `LocalizationPatternsWidget` owns a bare `QGraphicsScene` + one `QGraphicsPixmapItem`,
  shown in a stock `QGraphicsView` from the `.ui`
  ([localization_patterns_widget.cpp:442-448](../../../src/ui/forms/task/localization_patterns_widget.cpp#L442-L448)).
- The pixmap comes from `MatchPattern::getImageWithPickPosition()`, which **burns the pick
  axes into a `cv::Mat`** with `vsu::drawAxes2Img`
  ([match_pattern.cpp:232-242](../../../src/matching/match_pattern.cpp#L232-L242)).
- There is no zoom, no pan, and the **picking box is not drawn at all**.
- `fitInView()` is called on every refresh, so any view transform is discarded.

**Two things this exposes, worth stating before the work starts:**
1. **Drawing overlays belongs in `ui`, not `matching`.** `getImageWithPickPosition()` is
   the matching module rendering UI decoration into a pixel buffer. Moving the overlay to
   the view removes that, and the burned-in axes also cannot honour zoom, so they had to go
   regardless.
2. **The overlay must not be copy-pasted from the canvas.** "Drawn the same way" is a
   requirement that decays silently if there are two implementations — the gizmo has
   already been revised twice in this phase, and a second copy would have missed both
   revisions. The painting is extracted into one shared helper that the canvas and the
   thumbnail both call.

**Acceptance criteria:**
- [x] A new view class renders the pattern image with pick point, orientation gizmo and
      both jaw boxes, visually identical to the wizard canvas.
- [x] Pan (drag), zoom (wheel, cursor-anchored) and reset-transform all work.
- [x] **Nothing is interactive**: read-only is enforced by construction — one pixmap item
      with no interaction flags and `setAcceptedMouseButtons(Qt::NoButton)`, plus
      `ScrollHandDrag` — so there is no code path that could mutate a pattern.
- [x] The overlay painting has exactly **one** implementation, shared with
      `AddPatternImageCanvas`.
- [x] Gizmo elements keep a fixed on-screen size at every zoom level, as on the canvas.
- [x] Selecting a different pattern refreshes the content; the caption still shows name,
      pixel size and min score.
- [x] `MatchPattern::getImageWithPickPosition()` is no longer used by this path — grep
      confirms **zero remaining callers**; logged as `later_todo_list.md` item 34 rather
      than deleted, because removing it also makes `vsu::drawAxes2Img` a deletion candidate
      and that is the user's call, not a side effect of a UI task.

**Verification:**
- [x] Root app builds; `architecture_contract_test` **41 passed, 0 failed** (1441 ms).
      `ui.pri` gained both new files; the contract test's `.pro` needed no change (it does
      not enumerate `ui` headers). Confirmed the new sources really compiled by checking
      for `pattern_thumbnail_view.obj` / `pick_overlay_painter.obj` — a stale Makefile
      would otherwise have produced a green build that never saw them.
- [ ] Manual: select a pattern with a non-zero picking angle and a configured box, then
      zoom and pan — the boxes must track the image while the gizmo stays a constant
      on-screen size, and no element may respond to clicks or drags. **Pending user check.**

**Outcome notes:**
- `pick_overlay` (`src/ui/widgets/pick_overlay_painter.h/.cpp`) is the shared painter; the
  canvas's `drawPickOrientation()` is now a thin forward to it, and
  `pickRotationHandle()` derives the hit target from `pick_overlay::knobOffset()` so the
  grab point cannot drift from where the knob is painted.
- The view is promoted in the `.ui` (`<customwidget>`), matching how `PatternTreeWidget`
  and `ImageViewOnly` are already wired, so it owns its scene and the host widget lost its
  `m_thumbScene`/`m_thumbPixmap` members entirely.
- `FullViewportUpdate` is required, not cosmetic: the overlay is painted with the world
  transform reset, so it lives outside scene coordinates and a partial repaint would clip
  it against an exposed rect computed in scene space.
- The jaw pair is suppressed when width or height is zero — an unset box would otherwise
  render as two labelled dots and imply geometry the pattern does not have.

**Dependencies:** F1–F6 (reads `m_gripperBoxes`, `m_pickingBoxAngle`, `m_angle`)
**Files:** new thumbnail view + shared overlay painter under `src/ui/`, `ui.pri`,
`localization_patterns_widget.h/.cpp/.ui`, `pattern_canvas.cpp`,
`architecture_contract_test.pro`
**Scope:** M

### Task G2: Fold the thumbnail viewer into the Phase E docs — ✅ DONE (2026-07-31)

**Acceptance criteria:**
- [x] `pick_geometry_and_output_contract.md` §8 covers the thumbnail view alongside the
      wizards, including that the overlay painter is shared.
- [x] `uml/06_ui_widgets.puml` shows the new view — `PatternThumbnailView` and the
      `pick_overlay` namespace, plus `GripperRegisterDialog` and the canvas ownership
      edges, which the diagram had been missing since Phase D.

**Verification note:** same limit as E1 — no `java`/`plantuml.jar` on this machine, so the
diagram was checked structurally only (balanced tags, all seven note targets resolve, no
duplicate declarations), not rendered.

**Dependencies:** G1
**Scope:** S

---

## Phase E — Documentation

### Task E1: Update docs and UML — ✅ DONE (2026-07-30)

**Acceptance criteria:**
- [x] `uml/` reflects `GripperBoxes`, `CollisionGeometry`, the 6-axis output, and the new schema.
- [x] `docs/domains/task_localization/` documents the 6-axis offset, the 6-axis frame *(the criterion said "8-field" — a leftover from the plan's own miscount; each position is 6 axes)*, and absolute-axis semantics.
- [x] Doc comments follow `docs/rules/doc_comment_style.md`.
- [x] The D4 back-compat exception is recorded where a future agent will find it.

**Outcome notes:**
- New doc: **[../../domains/task_localization/pick_geometry_and_output_contract.md](../../domains/task_localization/pick_geometry_and_output_contract.md)**.
  The domain folder had *no* coverage of pick geometry at all — a grep for
  `picking|offset|pickBox` across it returned nothing — so this is a new document
  rather than an edit. It covers the pattern fields, presets, the schema versions, the
  world-pose composition, the wire format, absolute-axis semantics, the advisory check
  and the authoring UI, and leads with the failure modes that are silent.
- Linked from the domain `README.md` (reading order + source-file table + Important
  Contracts) and from `docs/README.md` → Current Domain Specs.
- **The D4 exception is recorded in `AGENT.md` directly under the rule that forbids
  it.** That is the one place a future agent reads before deciding to "clean up" a
  compat branch; a note only in the domain doc would be found *after* the deletion.
- UML updated: `05_matching_calibration.puml` (`GripperBoxes`, `CollisionGeometry`,
  the full `MatchPatternConfig`, `GripperPreset`/`GripperPresetStore`, plus notes on the
  schema history and the three similarly-named types), `04_localization_task.puml`
  (6-axis `VisionOutputPosition`, the world-pose composition, preset ownership),
  `09_robot_kinematics.puml`.
- **`09_robot_kinematics.puml` was stale in a way nothing else caught:** it still
  documented the pick pose as `fromXYZRPY_mm_deg(x,y,z,180,0,r)`, the form backlog
  item 33 replaced. Corrected, with the reason the folded-in 180 flip is exact.
- **Verification limit, stated plainly:** the `.puml` files were **not** render-tested —
  no `java` and no `plantuml.jar` on this machine. Checked structurally instead:
  balanced `@startuml`/`@enduml`, every `note … of X` target declared, no duplicate
  declarations. A render is still worth doing before relying on the diagrams.
- One doc-comment fix: the `m_pickingBoxAngle` `///<` line had reached 258 characters,
  over 1.5x anything else in that header. Converted to a `///` block above the member
  per `doc_comment_style.md` §7.
- `docs/generated/doxygen/` was deliberately left alone — generated output, regenerated
  from source, not hand-edited.

**Dependencies:** A1–F5
**Scope:** S

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Robot GR register file too small for `MAX_POS × 6` | **High** — silent truncation or robot fault | Resolve Q1 before Task A2; reduce `MAX_POS` if needed. |
| Six-axis frame breaks the live robot program | **High** | A2 lands with zeros first and is validated on the robot before any pose math exists. |
| Frame/convention mismatch on RX/RY/RZ (D5) | **High** — plausible-looking but wrong pick poses | Zero-offset regression guard in every Phase C task; hand-verify one known pose. |
| Mixed absolute/relative pose composition | **High** | Decompose→patch→rebuild explicitly (C3); never a single matrix product. |
| Existing saved projects lose picking geometry | Medium | Back-compatible read (D4), verified against a real pre-Phase-5 project. |
| Older app build reads a Phase 5 project | Medium | `version` key added in B1; newer documents refused with a logged error. |
| Blind search-and-replace hits `ItemGripperBox` | Low | Called out in A1 acceptance criteria. |

## Open Questions

**Q1 — Nachi GR register capacity.** *(blocks Task A2)*
Six axes × `MAX_POS = 8` needs `GR[30..77]`. The current layout uses `GR[30..61]`. Does
the target robot have ≥ 78 GR registers free, or should `MAX_POS` drop? Note the runtime
currently caps sends at 2 positions
([localization_runtime_controller.cpp:821](../../../src/model/localization_runtime_controller.cpp#L821)),
so 8 is headroom rather than a live requirement.

   - Decision: Ignore Nachi robot program (.prg file), not touch .prg file, user will handle it.

**Q2 — Absolute-axis frame.** *(blocks Task C3)*
A `PickPathPoint` axis flagged absolute — is its value in the robot **base** frame
(assumed), the tool frame, or the workspace frame?

   - `PickPathPoint` is absolute value in the robot base frame

**Q3 — Euler order (D5).** Confirm RX/RY/RZ compose as roll/pitch/yaw in the existing
`fromXYZRPY_mm_deg` convention, matching the robot's own convention.
   - RX/RY/RZ compose as roll/pitch/yaw ✅ (answered 2026-07-29)

**Q4 — Is the pick step's "picking angle" the same value as the box angle?** *(blocked F3)*
   - **Answered 2026-07-29: no.** The picking angle is `MatchPatternConfig::m_angle`, which
     is unrelated to `m_pickingBoxAngle`. The plan's earlier assumption (one value moved
     between steps) was wrong and has been corrected — the box step keeps its jaw-angle
     control, and the pick step gains a separate control for `m_angle`. See F3.

**Q5 — Should EditPatternWizard also get the preset picker?**
   - **Answered 2026-07-29: yes.** Folded into F2.

**Q6 — Presets after the angle move.**
   - **Answered 2026-07-29:** a preset is width/height/distance and defines the gripper
     geometry. The feature stands as-is.

   - RX/RY/RZ compose as roll/pitch/yaw
