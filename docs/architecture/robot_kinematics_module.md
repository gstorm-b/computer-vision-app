# Robot Kinematics Component (`RobotKinematics`)

> Reference for the reusable robot kinematics component under
> [components/RobotKinematics/](../../components/RobotKinematics/). Read this before
> touching FK/IK code or the Vision Output reachability/collision check.
>
> **History.** This replaces the former standalone `rkin` module
> (`robot_kinematics/`, `namespace rkin`). That folder has been removed; the new
> component is the single source of truth. Anything still importing
> `kinematics_types.h` / `ikinematic_solver.h` / `kinematic_factory.h` /
> `presets/robot_presets.h` is stale.

## Purpose

A C++/Eigen backend library for industrial robot kinematics: forward
kinematics, inverse kinematics, joint-limit validation, user/tool frames,
robot presets, reusable solver interfaces, primitive self-collision, and an
optional accurate **mesh** collision backend (Coal). It has no core UI and is
independent of the `vc::device::robot` communication family.

In this project it backs the Vision Output "is this pick TCP pose reachable /
within joint limits / not singular — and does the arm self-collide?" check
(Phase 2).

## Layout

- **Component.** [components/RobotKinematics/](../../components/RobotKinematics/) —
  `include/RobotKinematics/**` (public headers, `namespace RobotKinematics`) and
  `src/**` (Qt Core only, no widgets). Standalone build:
  `RobotKinematics.pro` (subdirs) + Qt Test runner under `tests/`.
- **Integration into the app.** Pulled in as source via
  `include(components/RobotKinematics/robotkinematics.pri)` in
  [ncr_picking.pro](../../ncr_picking.pro). The `.pri`:
  - adds `INCLUDEPATH += include` and the header-only Eigen at
    `3rdparty/eigen` (included as `<Eigen/...>`);
  - enables the Coal mesh-collision backend by setting
    `CONFIG += robotkinematics_mesh_collision`, `MESH_COLLISION_BACKEND = coal`,
    and `COAL_ROOT/BOOST_ROOT/ASSIMP_ROOT` to the repo-root `3rdparty/{coal,
    boost,assimp}` install trees, then including the component's own
    `mesh_collision_backend.pri` (which defines
    `ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND` and links `-lcoal`);
  - lists every library source + the conditional `CoalMeshCollisionBackend.cpp`.
- **Third-party.** All of the component's third-party install trees were
  relocated to the repo-root [3rdparty/](../../3rdparty/): `eigen`, `coal`,
  `boost`, `assimp`, `fcl`, `libccd`, `jrl-cmakemodules`.

## Units

The public API is **SI**: meters and radians internally. Convenience
constructors/accessors bridge to the project's mm/deg world:

- `Pose::fromXYZRPY_mm_deg(x,y,z, roll,pitch,yaw)` and `translation_m()`
  (use `units::toMm()` to display), `units::toDeg()`.
- `JointVector::fromDegrees({...})` / `toDegrees()` for revolute joints.

The RPY convention is fixed-axis roll(X)/pitch(Y)/yaw(Z), composed as
`R = Rz(yaw)·Ry(pitch)·Rx(roll)`.

## Public API (what the project uses)

- **Model.** `SerialRobotConfig` (links, joints, frames, `tools`,
  `defaultToolId`, posture/solver metadata). A `Tool` carries `id`, `name`, and
  a `flangeToTcp` `Pose` — the TCP lives in the config, not on the solver.
- **FK.** `ForwardKinematics` (static): `flangePose(config, joints)`,
  `toolPose(config, joints, flangeToTcp)`, `movableJointCount(config)`.
- **IK.** `SerialRobotKinematics(config).solve(IKRequest)` / `solveAll(...)`.
  `IKRequest{ targetPose (TCP pose), tool, seedJoint, options }`;
  `IKResult{ status, solutions[], ok(), best() }`. The solver is a **hybrid**:
  the analytic spherical-wrist plugin when it supports the model, otherwise an
  adaptive damped-least-squares numerical solver.
- **Status.** `KinematicsStatus` (Ok / TargetUnreachable / JointLimitViolation /
  Singularity / NoConvergedSolution / ToolNotFound / …). No built-in
  `toString`; the project maps it to a label locally.
- **Presets.** `Presets::nachiMZ04D()` (the one built-in preset relevant here),
  `Presets::virtual6DofTestArm()`, and `PresetJsonLoader::loadFile()` for JSON
  presets. **Kawasaki RS007N and Nachi MZ07 do not exist in this component** —
  they were dropped with the old `rkin` module.
- **Collision.** `MeshCollisionProfileJsonLoader::loadFile()` →
  `MeshCollisionProfile`; `CollisionBackends::checkMesh(config, profile,
  MeshCollisionCheckRequest{ joints })` → `CollisionCheckResult{ hasCollision,
  pairs[] }`. Primitive (no external libs) is also available via
  `checkPrimitive(...)`.

## Phase 2 — Vision Output integration (built)

- **Config** — `vc::device::RobotKinematicCheckConfig`
  ([vision_output_config.h](../../src/device/output_device/vision_output_config.h)):
  `enabled`, `collisionCheckEnabled`, `presetName`, `tcpName`, TCP offset
  (mm/deg). Plain value type (no RobotKinematics dependency in the header) on
  the shared `VisionOutputDeviceCfg` base, so both the TCP server and client
  transports carry it; serialized under the `RobotKinematicCheck` JSON key.
- **Widget** — `RobotKinematicCheckWidget`
  ([src/ui/forms/vision_output/](../../src/ui/forms/vision_output/robot_kinematic_check_widget.cpp)):
  enable checkbox, **self-collision (mesh) checkbox**, preset selector (only
  **"Nachi MZ04D"**), and TCP offset fields. Embedded in both
  `VisionTcpipDeviceWidget` and `VisionTcpipClientDeviceWidget`. Also hosts a
  collapsible **FK / IK tester** (a scratchpad, not saved): FK uses
  `ForwardKinematics::toolPose`, IK uses `SerialRobotKinematics::solve`, with
  the TCP appended to the config as the default tool. When the self-collision
  checkbox is on, each FK/IK result also runs a self-collision check on the
  resulting joints using the **simplified** (voxel) Nachi MZ04D mesh profile,
  appending the outcome to the tester status line. The widget also hosts a
  **pick-path table** (`tbl_pick_path`): each row is a 6-axis offset + a posture
  branch per axis (shoulder/elbow/wrist), the posture combos populated from the
  selected preset's `posture.labels`. Switching preset prompts for confirmation
  and clears the path (labels are preset-specific). Stored on the config as
  `pickPath` (posture as preset label strings).
- **Check** — `VisionTcpipDeviceBase::runKinematicCheck()`
  ([vision_tcpip_device_base.cpp](../../src/device/output_device/vision_tcpip_device_base.cpp))
  runs on the result send path (`pushRequest`, worker thread): for each outgoing
  pick `(x,y,z,r)` it builds the top-down pick pose
  `Pose::fromXYZRPY_mm_deg(x,y,z, 180,0, r)` at the configured TCP, runs IK on a
  `SerialRobotKinematics` built from the preset, logs unreachable poses; and when
  `collisionCheckEnabled` is set, runs `CollisionBackends::checkMesh` on each
  reachable solution's joints using the **simplified** (voxel) Nachi MZ04D mesh
  profile (same profile as the widget tester) and logs self-collisions.
  It emits `kinematicCheckResult(total, reachable)`. **Advisory only** — never
  blocks or alters the sent payload.

### Matcher robot-pickability port (dependency inversion)

The pure-vision `ImageMatcher` (`namespace mtc`, `src/matching/`) can ask "can the
robot actually pick this object?" **without** depending on RobotKinematics / calib
/ vc. It does so through a port:

- **Port** — [src/matching/robot_picking_checker.h](../../src/matching/robot_picking_checker.h):
  `mtc::IRobotPickingChecker` + the neutral POD `mtc::WorldPickPose` (x/y/z mm,
  r deg). `ImageMatcher` includes only this header.
- **Method** — `bool ImageMatcher::robotPossiblePickingCheck(const MatchedObject&)`
  ([image_matcher.cpp](../../src/matching/image_matcher.cpp)): step 1 image→world
  (`point_Center + cropOffsetPoint`, `point_angle`), step 2 reachability, step 3
  collision (only when the config enabled it). Returns `true` when no checker is
  injected (advisory — an unconfigured robot does not gate matching). Injected via
  `setRobotPickingChecker(const mtc::IRobotPickingChecker*)` (non-owning).
- **Adapter** — [src/model/robot_kinematic_picking_checker.h](../../src/model/robot_kinematic_picking_checker.h)
  (`vc::model::RobotKinematicPickingChecker`): the single bridge that pulls in
  `calib::Calibrator` (image→world: `imageToRobot` + `rotateImageToRobot`), the
  RobotKinematics component (`solveAll` IK + simplified-mesh `checkMesh`), and the
  `vc::device::RobotKinematicCheckConfig` (preset/TCP/collision toggle + pick-path).
  `imageToWorld` runs the calibration; `isPickable(pose, withCollision)` checks the
  whole **picking path** — the pick pose plus each configured `PickPathPoint`
  (6-axis TOOL-frame offset, `waypoint = pickPose * offset`). Every waypoint must
  have a `solveAll` solution on its required posture branch (shoulder/elbow/wrist
  labels resolved to branch signs via the preset's `posture.labels`) and, when the
  collision check is on, that branch's solution must be collision-free. An empty
  path degrades to the single pick pose.
- **Wiring (done).** `TaskLocalization::buildRuntimeContext()` snapshots the
  assigned vision device's `RobotKinematicCheckConfig` (incl. the pick-path) into
  `LocalizationRuntimeController::RuntimeContext.robotCheckConfig`. The controller
  builds the `RobotKinematicPickingChecker` **once at runtime setup** and rebuilds
  it only on active-camera change (`rebuildPickingChecker` — the calibrator differs
  per camera), never per cycle; it is passed each cycle as a cached
  `std::shared_ptr<mtc::IRobotPickingChecker>` (registered metatype) across the
  matching-worker thread. The runtime `ImageMatcher` is **persistent**
  (`TaskLocalization::m_runtimeMatcher`, built once on the matching thread; its
  learned model reloaded via `LocalizationPipeline::loadModel` only when the active
  pattern group changes, then `runMatchOn` per cycle). `ImageMatcher::matching()`
  runs `robotPossiblePickingCheck` per object (sets
  `MatchedObject::setPossibleToPick`, feeds `MatchResult::totalPossiblePicking`).
  Disabled check or uncalibrated camera => null checker => not gated. The
  commission/test path keeps a throwaway matcher and is not gated.
- **Frame caveat.** The image coords handed to the checker are in the matcher's
  own input frame (`point_Center + cropOffsetPoint`). If the host pre-crops the
  frame before matching, it must inject a checker whose calibration matches that
  frame (or set the matcher ROI instead of pre-cropping).
- **Known overlap.** The adapter duplicates the build-config / solveAll /
  collision logic also present in the device and widget (third copy). Candidate to
  consolidate into one shared RobotKinematics facade later (flagged, not done).

### Assumptions / limits (Phase 2)

- Pick orientation uses the **top-down** convention (tool Z down, yaw = r).
  Cells with an angled approach need a different convention — surface it.
- The check is **advisory** (logs + signal); it does not drop unreachable poses.
- The robot config is rebuilt per result-send batch (no shared mutable solver
  across threads). The mesh-collision profile (8 STL meshes) is loaded once and
  cached; `checkMesh` then runs per reachable pose.
- IK reference user frames must currently be fixed to the base link, and
  numerical `solveAll` is not mathematically exhaustive (component caveats).

### Deployment — handled by `robotkinematics.pri` post-link copy

After the app links, the `.pri` copies the mesh backend's runtime artefacts into
the target (binary) directory, so the app runs straight from the build folder:

- **Mesh-collision runtime DLLs.** `coal.dll`, `assimp-vc143-mt.dll`,
  `boost_serialization-vc143-mt-x64-1_87.dll`, and
  `boost_filesystem-vc143-mt-x64-1_87.dll` from
  `3rdparty/{coal,assimp,boost}`, per-file "if not exist … copy". Toggle:
  `CONFIG -= robotkinematics_copy_dlls`.
- **Mesh assets.** The whole `components/RobotKinematics/presets/Nachi/MZ04`
  tree → `<target>/robot_assets/Nachi/MZ04` via `xcopy /D /E` (recursive,
  missing/newer only). Recursive so both the original-resolution meshes (device
  runtime check) and the `simplified/` voxel meshes (widget tester check) are
  deployed. Toggle: `CONFIG -= robotkinematics_copy_assets`.

`runKinematicCheck()` resolves the profile from `<appdir>/robot_assets/Nachi/MZ04`
first, then falls back to the source tree (upward walk) for dev runs from source.
A customer install still needs to ship `robot_assets/` next to the binary
(later_todo_list #27).

## Cross-references

- UML: [uml/09_robot_kinematics.puml](../../uml/09_robot_kinematics.puml).
- Component docs: [components/RobotKinematics/README.md](../../components/RobotKinematics/README.md),
  `components/RobotKinematics/docs/robot_kinematics_spec.md`.
- Request: [request-prompt-description/robot_kinematic_module_request.md](../history/request_prompts/robot_kinematic_module_request.md).
