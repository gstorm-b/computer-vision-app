# Later TODO list

Outstanding items that were flagged but intentionally deferred to keep PRs
focused. Each entry records WHAT, WHERE, WHY-deferred, and a rough hint on
how to pick it up.

---

## 1. `SignalsMapWidget::checkEmpty()` — caller not wired

**What.** `SignalsMapWidget::checkEmpty()` is destructive: it purges orphan
(warning-flagged) tags to `""` and returns the list of `internalName`s now
empty. Currently nothing in the codebase calls it.

**Where.** `src/ui/widgets/signals_map_widget.{h,cpp}`, called from a not-yet-
chosen point in `src/ui/forms/task/localization_setting_widget.cpp`.

**Why deferred.** Caller policy is a UX decision: validate on Save? On Apply?
On commission start? Calling it eagerly inside `loadConfigToTask()` would
surface a confirm-style dialog on every field change, which is the wrong
moment. Owner needs an explicit validate trigger first.

**How to pick up.** Decide validate trigger (suggest: project Save and/or
commission start). Wire it to call `ui->listView_signals_map->checkEmpty()`
and surface the returned `QStringList` to the user (block save with a
dialog listing the missing mappings, or just log + allow).

---

## 2. Shared QSS design tokens for themed widgets

**What.** Three QSS files now hand-roll the same colour palette by hex
literal (`#2b8ce8`, `rgba(43, 140, 232, 28)`, etc.):
- `resrc/styles/add_device_wizard_{dark,light}.qss`
- `resrc/styles/camera_mapping_widget_{dark,light}.qss`

Any future themed widget added by following rule 15.1 will copy the same
constants again.

**Why deferred.** Refactoring all three files in one go widens the change
beyond the immediate task. Qt doesn't natively support QSS variables, so
the refactor needs a deliberate strategy (a small `.qss` snippet that
gets `@`-included by a generator, or a `resrc/styles/_tokens.qss` plus a
build-time concat step, or just a documented palette file).

**How to pick up.** Pick the cheapest strategy (probably: a single
`design_tokens.md` doc listing every token and which `.qss` files use it,
so renames stay tractable until we have enough widgets to justify a
generator).

**Update (2026-05-30).** The canonical token table is now **fully finalized**
in [ui_theme_tokens.md](../rules/ui_theme_tokens.md), covering all six original groups:

- §5.1 Background (bg.*) — 5 tokens
- §5.2 Border (border.*) — 4 tokens
- §5.3 Text (text.*) — 5 tokens
- §5.4 Accent + Selection (accent.*, selection.*) — 6 tokens
- §5.5 State + State surface (state.*) — 8 tokens  ← **confirmed, no longer proposed**
- Panel accent (`panel.accent.*`) — 4 tokens

Active theme: **Hybrid — Graphite Vision background + Navy Ops Orange accent**.

**Update (2026-06-24): DONE — runtime token resolver + migration sweep.**
Chosen mechanism: a **runtime resolver** (not a build-time generator). `.qss`
files reference tokens as `@{group.token}`; `ThemeManager::resolveTokens()`
substitutes each from the canonical `tokenTable()` in
[theme_manager.cpp](../../src/core/utils/theme_manager.cpp), mirroring
[ui_theme_tokens.md](../rules/ui_theme_tokens.md), when the sheet loads. Every loader
routes through it: global `apply()`, the `IDeviceWidget` /
`ITaskWidget` base `reloadStyleSheet()`, and the three widgets with their own
reload (`AddDeviceWizard`, `CameraMappingWidget`, `SignalsMonitorWidget`).

Migrated all 18 sheets — 1054 `@{token}` references. The sweep is **provably
non-visual-changing**: a verification pass parsed `tokenTable()` and re-resolved
every migrated file, matching the pre-migration backup byte-for-byte (modulo hex
case and rgba whitespace, both visually irrelevant). The app builds and links
with the resolver wired in.

**Update (2026-06-24, handoff token sweep applied): PARTIALLY RESOLVED.** The
follow-up colourway pass from
[resrc/styles/THEME_PALETTE_DESIGN_BRIEF.md](../../resrc/styles/THEME_PALETTE_DESIGN_BRIEF.md)
is now implemented: `ThemeManager::tokenTable()` gained the `device.*`,
`state.*.bright`, `state.error.deep`, and `overlay.*` families, and the active
QSS sheets were swept onto those placeholders. The old device-role leftovers
(`camera`/`plc`/`output`/`default`), status-lamp bright stops, and ADS
hover/pressed overlays are no longer hard-coded in the themed sheets. The only
approved raw exception left in the sweep scope is `#7a1010`, matching the
handoff rationale.

**Done (2026-06-24).** The remaining themed surfaces from
`ui_token_handoff_rework_request.md` are now closed:
`DevicesMonitorWidget`/`DeviceRowDelegate` use token-backed theme-aware
painting and stylesheet reload, and `SystemLogForm` now has explicit
dark/light QSS plus full visible-entry re-render on theme switch so existing
log lines stay readable after toggling the active theme.

---

## 3. `CalibrationBoardDialog` — preset-only selection

**What.** The dialog at `src/ui/widgets/calibration/calibration_board_dialog.{h,cpp}`
only lets the user choose a named preset from
`CalibrationBoardFactory::availablePresets()`. There is no way to author
a custom `FanucIRvisionBoard::Params` (custom rows/cols/spacing/margins).

**Why deferred.** Current shop-floor flow uses one of the five iRVision
presets verbatim. Adding a full Params editor (and the validation around
`FanucIRvisionBoard::Params::isValid`) before there is demand would be
premature.

**How to pick up.** Add a second tab / collapsible group in the dialog
that exposes raw `Params` fields. On accept, build via
`CalibrationBoardFactory::createFanucIRvision(params)` instead of
`createFromPreset(name)`. Persistence needs a new key beyond
`DEVICE_JSK_CALIB_BOARD_PRESET` — probably the board's `toJson()` blob.

---

## 4. `EditableComboWidget::eventFilter` — combobox-is-popup-only design

**What.** `EditableComboWidget` keeps its `QComboBox` page hidden even
when it is the current widget: the `FocusIn` handler immediately switches
back to the label. The popup is the only visual the user ever sees.

**Status.** Intentional. Documented inline now as
`// Combobox is purely a popup host — its visual rep stays hidden.`

**Why noted here anyway.** A future contributor may flag it as a bug and
"fix" it. If we ever want the combobox to actually render inline (e.g.
for accessibility / keyboard-only navigation), the fix is to remove the
`FocusIn` branch and tune the stacked-widget transitions. Not needed
today.

---

## 5. `getCurrentMapping()` does not detect duplicate camera ids across rows

**Status (2026-06-27): RESOLVED.** `CameraMappingWidget` now performs a
duplicate-camera sanity pass after rebuild/sort and marks any later duplicate
row with a warning state.

**What.** `CameraMappingWidget::provideNameOptions()` filters out cameras
already used by other rows when *that* row is being edited, so a duplicate
selection cannot happen via the UI. But if a caller injects a mapping
through `setCurrentMapping()` that already has two rows pointing to the
same id (corrupt save file, manual edit), the widget renders both rows
without warning.

**What was done.**
- Added `applyDuplicateWarnings()` in
  `src/ui/widgets/camera_mapping_widget.cpp`, called from `onDataChanged()`.
- Later rows whose `nameWidget->userData()` repeats an earlier camera id now
  get `duplicateCamera=true` on the row and `mappingWarning="duplicate"` on the
  visible camera label.
- Added matching warning selectors to
  `resrc/styles/camera_mapping_widget_{dark,light}.qss`.

**Current behavior.** UI editing still prevents duplicates proactively, and
corrupt / injected mappings loaded through `setCurrentMapping()` now surface a
warning tint + tooltip on the later duplicate row until the user resolves it.

---

## 6. `RobotDevice` — vendor API surface not yet defined

**What.** `src/device/robot/robot_device.h` currently only declares the
family-level dispatch (`RobotType` + `robotType()` pure virtual) plus the
mandatory `IDevice` overrides. There is no shared abstraction for motion,
teach-pendant, IO, frame transforms, etc. The two concrete subclasses
(`KawasakiRobotDevice`, `NachiRobotDevice`) return stubs for every
required override (connect/disconnect/isConnected return false; bits and
words lists are empty; `pushRequest` returns false).

**Why deferred.** Per Rule 12.5, the abstract is promoted only when a
real implementation reveals what the shared surface should look like.
Designing motion APIs against zero vendor experience would lock us into
the wrong abstraction.

**How to pick up.** When the first vendor integration starts (likely
Kawasaki), implement that vendor's real protocol in
`kawasaki_robot_device.cpp` first. Once a second vendor (Nachi or Huayan)
is being implemented, extract the shared methods into `RobotDevice` as
pure virtuals and migrate the existing impl. Don't try to design the
abstraction before the second vendor exists.

---

## 7. `AddDeviceWizard` — no Robot card

**What.** The wizard at `src/ui/forms/add_device_wizard.{h,cpp,ui}` currently
has cards for Camera, MC and VisionOutput only. Robot devices cannot be
created through the UI, even though `DeviceFactory::createRobotDevice`
and `DeviceManager::getSubDeviceTypeList(Robot)` are both wired.

**Why deferred.** The Robot framework was added without UI in scope (user
asked for the device-side framework only). Adding a card requires:
designing the icon + colour token (analog to `deviceColor="camera"`),
authoring matching dark/light QSS, registering the card in `initCards()`,
and a sub-type combobox feeding from `getSubDeviceTypeList(Robot)`.

**How to pick up.** Mirror the MC card. Pick a colour key (e.g.
`deviceColor="robot"` with an orange-ish accent distinct from
visionOutput). Add the card frame + sub-type combo to `add_device_wizard.ui`,
register it via `bind()` in `initCards()`, and append a matching
`QPushButton#adwAddBtn[deviceColor="robot"]` block to both QSS files.

---

## 8. `RobotRunner` — no runtime wiring

**What.** Camera, MC and VisionOutput each have a `*Runner` class in
`src/runtime/` that mediates between the GUI thread and the device's
own thread, and `TaskRunner::registerDevice` knows how to spin one up.
There is no `RobotRunner` and no dispatch case for `DeviceType::Robot`
in the task runner.

**Why deferred.** Without a real vendor protocol there is nothing to
route across thread boundaries. The Robot stubs all return synchronously.

**How to pick up.** When the first vendor implementation needs blocking
I/O (TCP socket to teach pendant, serial line, etc.), create
`src/runtime/robot_runner.h` modelled on `camera_runner.h`. Add a
dispatch case in `TaskRunner::registerDevice` and any task that consumes
a robot will need an `assignedDevicesOfType(Robot)` helper similar to
how `localization_setting_widget.cpp` currently uses camera/comm devices.

---

## 9. `VisionSerial` — sub-type declared, no implementation

**What.** `VisionOutputType::VisionSerial` is registered in
`src/device/output_device/vision_output_config.h` (the enum) and in the
`VisionOutputTypeToString/FromString` helpers in
`vision_output_device.h`, but:
- `DeviceFactory::createVisionOutputDevice` returns nullptr for that case.
- `AddDeviceWizard::buildDeviceJson` skips it (no concrete config to build).
- `DeviceManager::subDeviceTypeLists[VisionOutput]` only contains
  `VisionTCPIP`, so the wizard combobox never offers it to the user.

**Why deferred.** Same as Robot vendors — no real serial-protocol spec
yet. Designing the abstraction with zero impl would lock the wrong shape.

**How to pick up.** Add `vision_serial_config.h` + `vision_serial_device.{h,cpp}`
mirroring the TCP pair, wire a `createVisionSerial` factory branch,
register `VisionTypeToString(VisionSerial)` in
`DeviceManager::subDeviceTypeLists`, and add a wizard branch in
`buildDeviceJson`. The new widget (see #10) will also be needed.

---

## 10. `VisionOutputDeviceWidget` is TCP-only

**Status.** Completed in Phase 1. The TCP-specific widget is now
`VisionTcpipDeviceWidget`, and `DeviceWidgetFactory` owns subtype dispatch.

**Remaining future work.** When `VisionSerialDevice` lands, create
`VisionSerialDeviceWidget` and add a `VisionSerial` branch to
`DeviceWidgetFactory`.

---

## 11. `VisionOutputRunner` may need transport-specific signals

**What.** `src/runtime/vision_output_runner.h` currently exposes only
`requestConnect/Disconnect` and forwards `connectStatusChanged` /
`errorOccurred` — all transport-agnostic surface from `IDevice`. It
holds a `VisionOutputDevice*` (the abstract base), so it works for any
concrete sub-type.

**Status.** No issue today; works for TCP and will work for serial.

**Why noted here anyway.** If a sub-type ever needs to surface
transport-specific signals through the runner (e.g. serial framing
errors, TCP heartbeat-lost statistics), the runner has to be split or
templatised. Until then, leave alone.

---

## 13. Widget `static_cast` to concrete device is unsafe

**Status (2026-06-24): RESOLVED.** `BaslerCameraWidget` and
`VisionTcpipDeviceWidget` now use `qobject_cast` at the widget boundary. Wrong
subtypes log an error and disable the widget instead of dereferencing an invalid
concrete pointer. The full Debug app build passed after the change.

**What.** Both `BaslerCameraWidget::initCameraWiget` and
`VisionTcpipDeviceWidget::initWidget` use `static_cast` to downcast
from `IDevice*` to the concrete device type
(`BaslerGigECamera*` / `VisionTcpipDevice*`). If a future caller passes
in a device of a different sub-type (e.g. a Realsense camera reaches
the Basler widget by mistake) the cast silently produces a wrong-typed
pointer and the next member access is UB.

**Why deferred.** No misroute has been observed in practice. Both widgets
are constructed in `localization_task_widget.cpp` from a switch on
`device->deviceType()` + sub-type — the typing is enforced at the call
site, just not at the widget boundary.

**How to pick up.** Replace `static_cast` with `qobject_cast` plus a
nullptr check that logs `LOG_DEV_ERR` and disables the widget surface
(no crash, just no-op). Touchpoints:
- `src/ui/forms/camera/basler_camera_widget.cpp` ~ line 225.
- `src/ui/forms/vision_output/vision_tcpip_device_widget.cpp` ~ line 139.

---

## 14. `cbxVisionType` shown even with a single sub-type

**What.** `AddDeviceWizard` `pgVisionOutput` page renders
`cbxVisionType` combobox unconditionally, even though
`DeviceManager::subDeviceTypeLists[VisionOutput]` only contains
`VisionTCPIP` right now. Same pattern as `cbxCameraType` (single entry
`BaslerGigE`).

**Status.** Intentional consistency with Camera — leaves room for
forward-compat when the second sub-type is added without UI churn.

**Why noted.** A reviewer may flag it as "useless control"; this entry
documents that the control is kept on purpose, and should be made
conditional only if the project decides single-sub-type cards should
hide their combo (would also apply to `cbxCameraType`).

---

## 15. `subDeviceTypeLists[PLC]` carries McFrame strings, not `PlcType`

**What.** After the PLC sub-type promotion, `DeviceManager::subDeviceTypeLists`
maps `DeviceType::PLC` to the Mitsubishi MC frame-type list
(`["Frame_3E"]`), not to the family-level `PlcType` list (which would be
`["MitsubishiMc"]`). The AddDeviceWizard relies on this to populate
`cbxMcFrameType`, which is a protocol-level setting specific to
Mitsubishi MC — wrong abstraction level.

**Where.** `src/device/device_manager.cpp` near the constructor's
`subDeviceTypeLists.insert(DeviceType::PLC, mc_type_strlist)` line
(commented inline already).

**Why deferred.** No second PLC vendor exists yet; the abuse is
invisible to users until Omron FINS or Siemens S7 arrives and needs its
own protocol-level options (different frame types, different addressing).

**How to pick up.** When vendor #2 lands:
1. Change `subDeviceTypeLists[PLC]` to actually carry `PlcType` strings.
2. Introduce a parallel per-vendor protocol-option source (e.g.
   `DeviceManager::getProtocolOptions(PlcType)` returning a struct of
   per-vendor combo contents).
3. AddDeviceWizard reads `subDeviceTypeLists[PLC]` to fill a new
   `cbxPlcType` combo; the existing `cbxMcFrameType` / `cbxMcCode`
   become conditional on `cbxPlcType == MitsubishiMc`.

---

## 20. AddDeviceWizard stack page still named `pgMc`

**What.** The PLC card was renamed (`adwCard_mc → adwCard_plc`,
`deviceColor="mc" → "plc"`), but the underlying stack page inside
`pgMcLayout` / the page object `pgMc` and its children
(`cbxMcFrameType`, `cbxMcCode`, `lblMcFrame`, `lblMcCode`) still use
the `Mc` prefix.

**Where.** `src/ui/forms/add_device_wizard.ui` around the visionOutput-page
neighbourhood.

**Status.** Cosmetic only. The widgets belong to the Mitsubishi MC
sub-type's protocol options, so the `Mc` prefix is technically correct
at that level — the inconsistency is that the outer card was promoted
to the family name while the inner controls stayed protocol-named.

**Why deferred.** Pure cosmetic; renaming requires editing the .ui
plus every `ui->cbxMcFrameType` access in `add_device_wizard.cpp`
without any functional gain until #15 actually splits the wizard
into a 2-level (PlcType → protocol options) flow.

**How to pick up.** Rename together with the work in #15 — when the
inner controls become conditional on a `cbxPlcType` selection, give
them the per-vendor names at the same time
(`pgMitsubishi`, `cbxMitsubishiFrameType`, …).

---

## 21. `TaskLocalization::matchingRunner` has no explicit teardown

**Status (2026-06-23): RESOLVED.** `TaskLocalization` now declares and
implements `~TaskLocalization()`. The destructor destroys the runtime
controller, quits `matchingRunner`, waits up to 3000 ms, logs a warning if the
thread does not stop in time, deletes the thread, and clears the pointer.

**Where.** `src/model/task_localization.cpp` constructor/destructor and
`src/model/task_localization.h` member `QThread *matchingRunner`.

**Residual verification.** Covered by code inspection in the Phase 0 audit.
Keep lifecycle shutdown in the Phase 1/2 build and runtime verification pass.

---

## 22. Code review findings — qt-cpp-review (2026-05-30)

Batch of findings from a structured `qt-cpp-review` over the outstanding
device-binding / vision-widget / task-localization changes. Logged here per
Rule 11.2 (flag, don't silently fix). Items already tracked elsewhere are NOT
repeated; for example, the `matchingRunner` teardown is #21.

**Tech-lead note.** 22.1–22.2 are correctness bugs, not genuine "defer to
keep the PR focused" items — they are parked here only because the owner asked
to flag-not-fix this round. Items fixed by Phase 2 have been removed from this
batch. The remaining items should be scheduled before the next
commission run, ahead of the cleanup items lower down.

**CLOSED (2026-06-24).** All of batch 22 is resolved or intentionally deferred.
Every numbered item (22.1–22.24; 22.7 and 22.15 never existed) carries a Status
line: RESOLVED, RESOLVED (moot/already-fixed), or — for 22.20 only — DEFERRED
with rationale (premature optimization). Verified across the architecture
contract suite (38 passed) and a full Debug app build.

### Critical — schedule before next commission run

**22.1 — `std::shared_ptr::reset(raw)` creates a second owning control block.**

**Status (2026-06-24): RESOLVED.** Code audit confirms the double-ownership is
gone: there is no `.reset(raw)` call anywhere in `src/`, and the
`m_nextConnectCamera` / `m_selectedCamera` members no longer exist. The device
path now follows the recommended fix — `buildRuntimeContext()` derives the typed
camera via `std::dynamic_pointer_cast<device::CameraDevice>(device)`
([task_localization.cpp](../../src/model/task_localization.cpp), `buildRuntimeContext`),
so every owner shares the single control block held by `DeviceManager`. The
former line references (`:261/:266/:267/:320`) now point at unrelated code.

- Where. [task_localization.cpp:261](../../src/model/task_localization.cpp) (and
  :266, :267, :320). `m_nextConnectCamera.reset(camera)` / `m_selectedCamera.reset(camera)`
  receive a raw `CameraDevice*` from `qobject_cast<CameraDevice*>(device.get())`,
  where `device` is a `std::shared_ptr<IDevice>` owned by `DeviceManager`.
- Why it matters. `reset(raw)` builds a brand-new control block over an object
  already owned elsewhere → double-free / dangling when either owner drops.
- How to fix. Keep the original `shared_ptr<IDevice>` and use
  `std::static_pointer_cast`/`dynamic_pointer_cast`, assigning with `=` so all
  owners share one control block. Never `reset()` with a raw pointer pulled from
  another `shared_ptr`.

**22.2 — Cross-thread queued matching signals carry unregistered metatypes.**

**Status (2026-06-24): RESOLVED.** `TaskLocalization` now registers
`mtc::MatchResult`, `cv::Mat`, `std::shared_ptr<mtc::MatchGroup>`,
`std::shared_ptr<mtc::IRobotPickingChecker>`, and `CameraWorkspace`.
`CameraWorkspace` is also declared with `Q_DECLARE_METATYPE`. The architecture
contract test now includes
`test_runtime_matching_payload_metatypes_support_queued_connection`, which
passes `runtimeMatchingRequested(...)` through a `Qt::QueuedConnection` and
verifies that `CameraWorkspace` and `cv::Mat` are delivered.

- Where. [task_localization.h](../../src/model/task_localization.h) +
  [task_localization.cpp](../../src/model/task_localization.cpp) and
  [localization_runtime_controller.h](../../src/model/localization_runtime_controller.h).
  `startCommissionMatchingRequest(std::shared_ptr<mtc::MatchGroup>, cv::Mat,
  CameraWorkspace)`, `commissionMatchingFinished(mtc::MatchResult)`, and
  `runtimeMatchingRequested(...)` cross thread boundaries (worker on
  `matchingRunner`) -> `Qt::QueuedConnection`.
- Why it matters. Queued connections marshal each argument through the metatype
  system. `mtc::MatchResult`, `cv::Mat`, and `std::shared_ptr<mtc::MatchGroup>`
  are not registered, so Qt logs "Cannot queue arguments of type 'cv::Mat'" and
  silently drops the call — commission matching never runs.
- Verification. Phase 1 architecture contract suite rebuilt and ran with exit
  code `0` on 2026-06-24.

### High — real bugs, scoped fixes

**22.3 — `removeWidget()` during forward index iteration skips pages.**

**Status (2026-06-24): RESOLVED.** `onTaskDevicesChanged()` now iterates the
content stack from the top down (`idx = count()-1 … 0`), so `removeWidget()`
shifting higher indices can no longer skip the page that slides into a vacated
slot. Compiles in the full Debug app build.

- Where. [localization_task_widget.cpp:470](../../src/ui/forms/task/localization_task_widget.cpp),
  `onTaskDevicesChanged()`. Loop iterates `content_stack` by ascending index and
  removes inside the loop; `removeWidget` shifts later indices down while `idx`
  still increments, so a page right after a removed one is never visited. Two
  consecutive removed device pages leave a stale `IDeviceWidget` behind.
- How to fix. Collect widgets to remove in one pass, delete in a second; or
  iterate `idx = count()-1 .. 0`.

**22.4 — `m_devicePages` cache evicted for still-assigned devices.**

**Status (2026-06-24): RESOLVED.** Both `removePropertyBrowserWidget(...)` and
`m_devicePages.remove(deviceId)` now sit inside the
`if (!deviceIds.contains(deviceId))` guard in `onTaskDevicesChanged()`, so live
device pages are no longer torn down/evicted. Compiles in the full Debug app
build (`localization_task_widget.cpp`).

- Where. [localization_task_widget.cpp:479](../../src/ui/forms/task/localization_task_widget.cpp).
  `removePropertyBrowserWidget(...)` and `m_devicePages.remove(deviceId)` sit
  outside the `if (!deviceIds.contains(deviceId))` guard, so they run for every
  page found — including live ones. Cache and stack desync; a later
  `showDeviceConfigPage` misses the cache and builds a duplicate page, orphaning
  the original.
- How to fix. Move both calls inside the `if (!deviceIds.contains(deviceId))`
  block.

**22.5 — `IDeviceWidget` constructor drops its `parent` argument.**

**Status (2026-06-24): RESOLVED.** The constructor forwards `parent` to
`QWidget(parent)`. This pass also verified the base widget contract during the
full Debug app build.

- Where. [device_widget.h:12](../../src/ui/forms/device_widget.h). `IDeviceWidget(QWidget *parent = nullptr) {}`
  has an empty init list, so `QWidget` is default-constructed and `parent` is
  discarded. Subclasses forward `parent` expecting parent-child ownership.
- Note. Base class is outside the reviewed changeset but every device widget
  depends on it.
- How to fix. `IDeviceWidget(QWidget *parent = nullptr) : QWidget(parent) {}`.

**22.6 — Shared `MatchGroup` read on worker thread while GUI can mutate it.**

**Status (2026-06-24): RESOLVED.** Both matching paths now hand the worker an
isolated deep copy built on the GUI thread via
`TaskLocalization::snapshotPatternGroup()` (own `MatchGroupConfig` with cloned
`typeConfig`, plus an own pattern vector whose configs carry `m_rawImage`):
- Runtime: `buildRuntimeContext()` already snapshotted into
  `context.patternGroups`; it now calls the shared helper.
- Commission: `startCommissionMatching()` previously emitted the **live**
  `PatternGroupManager` group (obtained in
  `LocalizationPatternsWidget::runMatchingTest`) directly to the matching thread.
  It now snapshots before `emit`, so the worker never touches the live group.

Verified: architecture contract suite rebuilt and ran on 2026-06-24 — `Totals:
38 passed, 0 failed` (incl. `test_runtime_matching_payload_metatypes_support_
queued_connection` and `test_localization_runtime_trigger_cycle_uses_matching_
worker_contract`).

- Where. [task_localization.cpp:382-404](../../src/model/task_localization.cpp). The
  worker lambda iterates `group->patterns()` / reads `config()` on the matching
  thread, holding the same `shared_ptr<MatchGroup>` that `PatternGroupManager`
  (GUI thread) keeps mutating (`addPattern`/`removePattern`/`setPatternImage`).
  `MatchGroup` is non-QObject with no locking (design_rules §4.2) — the container
  read races the GUI-thread append/erase.
- How to fix. Snapshot a deep copy of the needed config + cloned train images on
  the GUI thread before emitting, and hand only the copy to the worker; or
  serialize all `MatchGroup` access with a mutex; or block pattern editing while a
  commission match is in flight.

### Medium — hardening / robustness

**22.8 — `fromJson` performs no schema/version validation.**

**Status (2026-06-24): RESOLVED.** `TaskLocalizeConfig::toJson()` now writes
`version` (`kSchemaVersion = 1`); `fromJson()` reads it and refuses any document
whose version is newer than supported (a missing key is the legacy baseline,
version 0, and is accepted). Verified by the architecture contract suite (38
passed) and the full Debug app build.

- Where. [task_localization_config.h:89](../../src/model/task_localization_config.h),
  [task_localization.cpp:116](../../src/model/task_localization.cpp). Only gate is
  `obj.empty()`; fields read with defaults, so a future/foreign document is
  silently accepted with partial-default state. `toJson()` writes no version key.
- How to fix. Add a `version` int to `toJson()` and validate/migrate it in
  `fromJson()`; treat missing as the legacy baseline.

**22.9 — No range validation on imported binding data.**

**Status (2026-06-24): RESOLVED.** `TaskDeviceBinding::fromJson()` now rejects
out-of-range camera numbers (CameraNumber bindings must be
`kMinCameraNumber..kMaxCameraNumber` = 1..16) and over-long device ids
(`kMaxDeviceIdLength` = 64). Invalid bindings are dropped via the existing
bool-return convention in `TaskDeviceBindings::fromJson()`. Verified by the
architecture contract suite (38 passed) and the full Debug app build.

- Where. [task_device_binding.h:65](../../src/model/task_device_binding.h).
  `cameraNumber = obj["cameraNumber"].toInt(0)` accepts any int including
  negatives, while the task enforces 1..16 elsewhere (`limit_num_camera`). A
  malformed/hostile file can inject out-of-range numbers.
- How to fix. Validate `cameraNumber` against the legal range (and cap device-id
  string length) in `fromJson`; drop/clamp invalid entries via the existing
  `bool`-return convention.

**22.10 — `IDeviceWidget` polymorphic base lacks virtual dtor / `Q_DISABLE_COPY_MOVE`.**

**Status (2026-06-24): RESOLVED.** `IDeviceWidget` now declares
`~IDeviceWidget() override = default;` and `Q_DISABLE_COPY_MOVE(IDeviceWidget)`.
The full Debug app build passed after moc regeneration.

- Where. [device_widget.h:8](../../src/ui/forms/device_widget.h). Declares pure virtuals
  but no explicit virtual destructor and no `Q_DISABLE_COPY_MOVE`.
- How to fix. Add `Q_DISABLE_COPY_MOVE(IDeviceWidget)` and
  `~IDeviceWidget() override = default;`. (Base class — coordinate with 22.5.)

**22.11 — `m_output_device` may be dereferenced uninitialized.**

**Status (2026-06-24): RESOLVED.** `m_output_device` is now initialized to
`nullptr`; `initWidget()` assigns it only after a successful `qobject_cast`, and
`saveConfig()` returns early when the typed device is unavailable.

- Where. [vision_tcpip_device_widget.h:54](../../src/ui/forms/vision_output/vision_tcpip_device_widget.h).
  No in-class initializer; assigned only inside `if (m_device)` in `initWidget()`,
  but `saveConfig()` derefs unconditionally. The factory currently guards device
  null, so the bad path is not reachable today — but the invariant is implicit.
- How to fix. Initialize `m_output_device{nullptr}` and null-check before deref,
  or assert the device invariant at construction. Related to #13 (unsafe casts in
  the same widget).

**22.12 — `taskRunner()` dereferenced without null check in runner helpers.**

**Status (2026-06-24): RESOLVED (already fixed).** `cameraRunner()` /
`plcRunner()` already begin with `if (!taskRunner()) return nullptr;`
([task_localization.cpp](../../src/model/task_localization.cpp)); the doc line refs
were stale. Verified by the architecture contract suite (38 passed).

- Where. [task_localization.cpp:85-95](../../src/model/task_localization.cpp).
  `cameraRunner()` / `plcRunner()` call `taskRunner()->runnerFor(...)` with no
  guard, while the task widget treats `taskRunner()` as possibly null
  ([localization_task_widget.cpp:781](../../src/ui/forms/task/localization_task_widget.cpp)).
- How to fix. Add a null guard returning nullptr for consistency.

**22.13 — Reconnect `SingleShotConnection` re-arms against the wrong camera.**

**Status (2026-06-24): RESOLVED (moot).** The whole mechanism this described is
gone: `waitReconnectCameraHandle`, `m_selectedCamera` and `m_nextConnectCamera`
no longer exist anywhere in `src/` (removed alongside 22.1). Recovery reconnects
are now coordinated by `LocalizationRuntimeController` through runners — nothing
left to fix.

- Where. [task_localization.cpp:325](../../src/model/task_localization.cpp).
  `waitReconnectCameraHandle` re-connects to `m_selectedCamera` for non-terminal
  statuses, but the device being awaited is `m_nextConnectCamera`. Combined with
  22.1, it can wire the wait onto a soon-to-dangle sender.
- How to fix. Confirm which device the wait targets, disconnect prior connections
  before re-arming, and avoid re-arming on a sender whose `shared_ptr` may reset.

### Low — cleanup / quality

**22.14 — Dead member variables.**

**Status (2026-06-24): RESOLVED.** `m_currentCamNumber` / `m_curentPatternNumber`
were already gone; the remaining declaration-only dead members
`m_lastMatchResult` and `m_lastVisionOutput` (no read/write sites anywhere) were
removed from `task_localization.h`. Verified by the architecture contract suite
(38 passed).

- Where. [task_localization.h:121-132](../../src/model/task_localization.h).
  `m_currentCamNumber`, `m_curentPatternNumber` (also a typo), `m_lastMatchResult`,
  and `m_lastVisionOutput` have no read/write sites.
- How to fix. Remove them, or gate behind the feature when it lands; fix the typo
  if kept.

**22.16 — `switch` over `ConnectStatus` uses `default:`, hiding new cases.**

**Status (2026-06-24): RESOLVED.** The two originally-referenced sites were
already refactored away; the remaining `ConnectStatus` switches were made
exhaustive (no `default:`; every value enumerated so -Wswitch / C4062 flags a
new value): `basler_camera_widget`, `mitsubishi_mc_device_widget`,
`localization_dashboard_widget::applyConnectStatusToLamp`, the `dotStateFor`
lambda in `localization_task_widget`, and `localization_recovery_policy` (which
was also silently missing `Connecting`). `connectStatusName` was already
exhaustive. Verified by contract suite (38 passed) + full Debug app build.

- Where. [task_localization.cpp:347](../../src/model/task_localization.cpp),
  [vision_tcpip_device_widget.cpp:224](../../src/ui/forms/vision_output/vision_tcpip_device_widget.cpp).
- How to fix. Enumerate every `ConnectStatus` value explicitly (no-ops with
  `break;`) and drop `default:` so `-Wswitch` flags additions.

**22.17 — Duplicated meta-property lookup/dispatch logic.**

**Status (2026-06-24): RESOLVED.** Added a shared `vc::gadget_meta` helper
(`displayName` / `writeProperty` / `readProperty`) to `src/core/qgadget_macro.h` —
the header that already owns the `"<prop>_name"` Q_CLASSINFO convention.
`localization_setting_widget` (`displayNameOf` / `writeConfigField` /
`readConfigField`) and `vision_tcpip_device_widget`'s property dispatch now both
route through it. Verified by contract suite (38 passed) + full Debug app build.

- Where. [localization_setting_widget.cpp:62](../../src/ui/forms/task/localization_setting_widget.cpp)
  (`readConfigField`/`writeConfigField`) and
  [vision_tcpip_device_widget.cpp:166](../../src/ui/forms/vision_output/vision_tcpip_device_widget.cpp)
  re-implement the "indexOfProperty → write/readOnGadget" pattern and class-info
  `_name` resolution independently.
- How to fix. Extract a shared `gadget_meta` helper and call from both.

**22.18 — Public `const` data members used as limits.**

**Status (2026-06-24): RESOLVED.** `limit_comm_device` /
`limit_vision_output_device` / `limit_num_camera` are now
`static constexpr int kLimitCommDevice` / `kLimitVisionOutputDevice` /
`kLimitNumCamera`; the three usages in `task_localization.cpp` were updated.
Verified by the architecture contract suite (38 passed).

- Where. [task_localization.h:105-107](../../src/model/task_localization.h).
  `limit_comm_device`, `limit_vision_output_device`, `limit_num_camera` are
  public non-static snake_case const members.
- How to fix. Make them `static constexpr int` with a consistent name scheme
  (e.g. `kLimitNumCamera`).

**22.19 — Fixed page-index constants assume a click order that isn't enforced.**

**Status (2026-06-24): RESOLVED.** `showDashboardPage()` and `showSettingsPage()`
now navigate with `setCurrentWidget(m_…Page)` instead of
`setCurrentIndex(k…Page)`, matching the patterns/device pages — so a page's real
stack index (pages are inserted lazily in click order) no longer has to equal
its preferred slot. Compiles in the full Debug app build.

- Where. [localization_task_widget.cpp:616](../../src/ui/forms/task/localization_task_widget.cpp).
  `kDashboardPage/kSettingsPage/kPatternsPage` are used both as `insertWidget`
  positions and `setCurrentIndex` targets, but pages are created lazily in user
  order, mixed with `setCurrentWidget(...)` navigation elsewhere.
- How to fix. Navigate by widget pointer consistently
  (`setCurrentWidget(m_settingPage)`), or build all fixed pages once up front.

**22.20 — `cameraNumberMap()` rebuilds a `QMap` by linear scan on a hot path.**

**Status (2026-06-24): DEFERRED — no action (intentional).** This item's own note
says "Fine at ≤16 cameras; revisit only if profiling shows it matters." Adding a
cache now is premature optimization with no measured need, so it is deliberately
left as-is. Re-open only if profiling shows `cameraDeviceId()` on the
camera-switch path is a real hotspot.

- Where. [task_device_binding.h:103](../../src/model/task_device_binding.h).
  `cameraDeviceId()` is called from `setCameraNumber()` on every camera-switch
  signal; each call scans the `QList` and allocates a fresh map. Fine at ≤16
  cameras; revisit only if binding counts grow.
- How to fix. Cache a `QHash<int,QString>` invalidated on `setCameraNumberMap()`
  if profiling shows it matters.

**22.21 — `saveConfig()` ignores the persistence outcome.**

**Status (2026-06-24): RESOLVED.** `VisionTcpipDevice::setVisionTcpipConfig()`
now returns `bool` (false when the link is live and the config is locked); the
widget's `saveConfig()` surfaces a user-facing `LOG_USER_WARN` on false instead
of silently dropping the edit. Verified by contract suite (38 passed) + full
Debug app build.

- Where. [vision_tcpip_device_widget.cpp:208](../../src/ui/forms/vision_output/vision_tcpip_device_widget.cpp).
  `setVisionTcpipConfig(m_config)` result is discarded; called after every edit
  with no success/failure feedback.
- How to fix. If the setter can fail or persists to disk, return a status and
  surface failures (log + visual), matching the `changeDeviceName` pattern.

**22.22 — Rename failure is reverted but not surfaced to the user.**

**Status (2026-06-24): RESOLVED.** A user-facing `LOG_USER_WARN` ("name already
in use") is now logged before the field is reverted, in all four device widgets
that share the rename idiom (basler, mitsubishi, vision_tcpip, vision_tcpip
client). Verified by the full Debug app build.

- Where. [vision_tcpip_device_widget.cpp:190](../../src/ui/forms/vision_output/vision_tcpip_device_widget.cpp).
  When `changeDeviceName` returns false the field is reset with no user-visible
  reason. Same silent idiom as the basler / mc_protocol widgets — uniform but
  uniformly silent on a user-facing failure.
- How to fix. On false, add a user-level log/toast explaining the rejection in
  addition to reverting the field (applies to the sibling widgets too).

### Medium — found during the 2026-06-24 shared_ptr ownership audit

These are lifetime-extension leaks (not double-free) — a `shared_ptr` captured by
value into a Qt connection whose lifetime is bound to a long-lived receiver, so
the owned object survives its logical removal. Found while auditing 22.1.

**22.23 — `DeviceManager::reserveDevice` leaks the device past `releaseDevice`.**

**Status (2026-06-24): RESOLVED.** The `configChanged` lambda now captures the
device **id** (`const QString deviceId = device->id()`) instead of the
`shared_ptr`, so the connection no longer extends the device's lifetime — it
auto-disconnects when the device (the sender) is destroyed by `releaseDevice()`.
Verified by the architecture contract suite (38 passed; `device_manager.cpp`
recompiled) and the full Debug app build.

**Follow-up (2026-06-24): RESOLVED.** The `reserveDevice`/`commitDevice`
inconsistency the fix note called out is now fixed too. The connect was extracted
into `DeviceManager::connectConfigChanged()` and is called by **both** creation
paths — `reserveDevice` (project load) and `commitDevice` (Add-Device wizard).
Previously, devices added via the wizard never emitted `deviceModified` on a
config edit, so `Project::projectModificationOccurred()` did not fire and the
edit was not reflected in the project's modified state; project-loaded devices
behaved correctly. Now both paths mark the project modified consistently.
Verified by the architecture contract suite (38 passed).

- Where. [device_manager.cpp:126](../../src/device/device_manager.cpp).
  `connect(device.get(), &IDevice::configChanged, this, [this, device]{ ... })`
  captures the `device` `shared_ptr` by value, and the connection is bound to
  `this` (the `DeviceManager`). After `releaseDevice()` removes the device from
  `deviceInstances`, the lambda still holds a strong ref, so the device is never
  destroyed until the `DeviceManager` itself dies — it keeps living on its thread
  and still emits `deviceModified` for a removed id. Note `commitDevice` does
  *not* make this connection, so the two creation paths behave inconsistently.
- How to fix. Capture the device **id** (a `QString`) instead of the `shared_ptr`,
  or capture a `std::weak_ptr`/`QPointer` and bail when expired; disconnect on
  `releaseDevice`. Reconcile the connect between `reserveDevice`/`commitDevice`.

**22.24 — `Project::addTask` leaks the task past removal.**

**Status (2026-06-24): RESOLVED.** The `configChanged` lambda now captures the
task **id** (`const QString taskId = task_ptr->id()`) instead of the
`shared_ptr`, so the connection no longer keeps the task alive past
`removeTask()`. Verified by the architecture contract suite (38 passed;
`project.cpp` recompiled) and the full Debug app build.

- Where. [project.cpp:75](../../src/model/project.cpp). Same pattern:
  `connect(task_ptr.get(), &ITask::configChanged, this, [this, task_ptr]{ ... })`
  captures the `task_ptr` `shared_ptr` by value into a connection bound to the
  `Project`, so a removed task is kept alive until the `Project` is destroyed.
- How to fix. Capture the task **id** instead of the `shared_ptr` (or a
  `weak_ptr`), and disconnect when the task is removed.

---

## 23. UI conformance migration to ui_design_rules.md

**Status (2026-06-24): PARTIALLY RESOLVED.** (2026-05-30: theme-reload contract
done; hex-token migration was pending. 2026-06-24: runtime resolver shipped,
and the follow-up device/status/overlay handoff sweep was applied; see #2.)

**Completed in this pass:**
- `IDeviceWidget` and `ITaskWidget` now both provide `virtual reloadStyleSheet()`
  and `setupThemeReload(darkPath, lightPath)`. Subclasses call `setupThemeReload`
  once from their constructor; the base handles the initial load and the
  `ThemeManager::themeChanged` subscription.
- Fixed `IDeviceWidget` constructor bug: `parent` was not forwarded to `QWidget`.
- All four subclasses that had per-form QSS now use `setupThemeReload` instead of
  duplicating the reload/connect boilerplate:
  `MitsubishiMcDeviceWidget`, `LocalizationTaskWidget`, `LocalizationPatternsWidget`,
  `VisionTcpipDeviceWidget`.
- Created missing QSS pairs that were referenced in code but absent or not
  registered: `localization_patterns_widget_{dark,light}.qss` (new files),
  `localization_task_widget_{dark,light}.qss` (existed on disk, now registered).
- Fixed `VisionTcpipDeviceWidget::updateConnectionVisual()`: removed three
  inline `setStyleSheet()` calls; connection state now driven by
  `setProperty("connectionState", ...)` + repolish, styled in the new
  per-form QSS pair via attribute selectors (ui_design_rules §3.6, §4.5).
- Removed unused `#include "form/pattern/pattern_theme.h"` from
  `vision_tcpip_device_widget.cpp`.
- All six new/fixed QSS pairs registered in `resrc.qrc`.

**Update (2026-06-24): token mechanism + §5-value migration DONE (see #2).**
- **Hardcoded hex to tokens.** ~~Reconcile per-form + global sheets onto §5.~~
  The token mechanism now exists (runtime `@{token}` resolver, #2) and all
  §5-token colours across the 18 sheets are migrated (1054 references), verified
  non-visual-changing. The mechanism is no longer blocked.
- **Accent-tinted panels.** ~~Replace blue-tinted literals `#2a3a52`/`#1a2540`/
  `#111f30`.~~ Confirmed already absent from the QSS files (this note was stale);
  the `panel.accent.*` tokens are in place and tokenized.

**Closeout note (2026-06-24).** The colourway pass tracked here is done:
the device-role palette, status-lamp bright stops, and overlay helpers are now
formal tokens, and the corresponding QSS selectors use them. The only raw
colour intentionally left in place in the sweep scope is `#7a1010`, the
handoff-approved single-use delete-button shadow.

Final UI conformance closeout remains blocked by:
- `DevicesMonitorWidget` / `DeviceRowDelegate` staying dark inside
  `MitsubishiMcDeviceWidget` in light mode.
- `SystemLogForm` lacking designed dark/light styling.

Track implementation requirements in
[ui_token_handoff_rework_request.md](../history/handoffs/ui_token_handoff_rework_request.md).

---

## 24. `svgIcon()` is not theme-aware — Rule 4.4 violation (project-wide)

**Status (2026-06-27): RESOLVED.** The icon-loading convention was normalized
around a theme-aware `svgIcon()` helper backed by `ThemeManager::themedIcon()`.

**What was done:**
- `windows_helper.h:85` no longer returns a plain single-path `QIcon`; it now
  creates a theme-aware SVG icon engine that resolves `foo.svg` vs
  `foo_dark.svg` against the active `ThemeManager` style at render time.
- Existing `setIcon(svgIcon(...))` call sites now consult the active theme
  without needing per-call-site rewrites.
- Direct pixmap consumers that store icon pixels in `QLabel` were refreshed on
  `themeChanged`:
  - `src/ui/forms/add_device_wizard.cpp` card icons
  - `src/ui/widgets/controls/device_nav_item_widget.cpp` nav-item icon label
- `docs/rules/ui_design_rules.md` Rule 4.4 was updated to match the implemented
  convention: use `svgIcon(basePath)` as the normal entrypoint, fall back to the
  base asset when no `_dark` sibling exists, and explicitly refresh stored
  pixmaps on theme changes.

**Asset audit outcome:**
- Several icons already ship with `_dark.svg` light-theme variants under
  `resrc/icon/` (`dashboard`, `new_file`, `plc_icon`, `plus_square`, `reload`,
  `robot_movement`, `setting`, `vision_target`, ...).
- Remaining icons without `_dark` siblings are currently treated as
  theme-neutral and continue to reuse their base asset in both themes until a
  future art pass proves they need dedicated light-theme artwork.

---

## 25. `LocalizationDashboardWidget` backend wiring to the refactored `.ui`

**Status (2026-05-31): RESOLVED.** `localization_dashboard_widget.{h,cpp}` were
adapted to the refactored `.ui`. All pre-refactor `objectName` references were
migrated and the five behavioural changes below were implemented. The widget now
matches the operator-runtime mockup (`docs/artifacts/ui/task_localization_dashboard_mockup.html`)
and the implementation plan (`docs/domains/task_localization/task_localization_implementation_plan.md`).

**What was done:**
- objectName migration applied (`gv_match_view`, `wg_signal_monitor`,
  `lbl_val_vision_device`, `lbl_val_plc_device`, `lbl_val_camera`,
  `lbl_val_pattern_group`, `tbl_result`, `lbl_kpi_*_val`, `log_task_view`).
- `updateTaskStateLabel()` now drives the **Cycle** lamp from `TaskState`
  (Faulted→Error, RunningCycle/Recovering→Warning, Ready→Ok, else Off).
- New `applySignalToDashboard(name, value)` routes live signals to the
  Task/Camera/Pattern lamps and context value labels.
- New `setFaultState(active, code)` drives `frame_fault[active]` (setProperty +
  repolish, Rule 4.5), the fault value labels, and the Task lamp Error state;
  fault-code text uses `localizationFaultCodeName()`.
- `appendTaskLog()` now calls `log_task_view->appendEvent(TaskEvent)` with
  `severityToLevel()` mapping the runtime severity string → `TaskEventLevel`.
- Context labels write **value-only** (`lbl_val_*`); captions stay static in `.ui`.
- Both custom widgets (`StatusLamp`, `TaskEventLogWidget`) are promoted in the
  `.ui` and registered in `ncr_picking.pro`. Styling lives in global
  `dark.qss`/`light.qss` (no per-form pair, no `resrc.qrc` change).

**Residual dependency update (2026-05-31):** `TaskLocalization` now exposes the
`signalChanged(QString, QVariant)` path through `ITask`, and runtime output/input
updates are forwarded by `LocalizationRuntimeController`. Dashboard v1 is still
intentionally read-only, so monitor row writes remain disabled.

**Remaining verification (when the project builds end-to-end):** run the §9
review checklist in both dark and light; confirm lamps, fault panel, KPIs,
result table, and operator log update on a runtime cycle, and that the dashboard
exposes no manual write / trigger / start-stop controls (read-only v1).

---

## 26. Localization runtime production follow-ups after first implementation pass

**Status (2026-06-24): PARTIALLY VERIFIED.** The first implementation pass
builds and covers the main contract shape, and the 2026-06-24 hardening pass
added focused tests for invalid setup faults, PLC output writes, invalid PLC tag
rejection, and camera loss during `RunningCycle`. The three runtime threading /
active-camera follow-ups below are now resolved (2026-06-24); the remaining
gates to production-complete are the operator UI pass and latency measurement.

**Remaining work.**
- **Done (2026-06-24):** ~~Move the `LocalizationRuntimeController` object into
  `TaskRunner::m_runtimeThread` or explicitly document the supported coordinator
  thread.~~ The controller is moved onto `TaskRunner::runtimeThread()` in
  `beginRuntime`, and all task-to-controller calls are queued (`setup` blocking,
  the rest non-blocking). The coordinator-thread model is now the recorded
  supported design — see `phase2_phase3_runtime_hardening.md` →
  "Runtime Threading Model Decision (2026-06-24)".
- **Done (2026-06-24):** ~~Move runtime matching off the controller call
  stack.~~ Matching runs on the `matchingRunner` thread via
  `runtimeMatchingRequested` → `m_matchingWorker`, with the result posted back
  through a queued `onRuntimeMatchingFinished`. It no longer runs synchronously
  on the controller call stack.
- **Done (2026-06-24):** ~~Rework runtime active-camera switching so it is fully
  runner-based.~~ The `TaskLocalization::setCameraNumber()` path no longer makes
  direct `CameraDevice` connect/disconnect calls; it routes through
  `queueSetActiveCameraNumber` →
  `LocalizationRuntimeController::setActiveCameraNumber`, which disconnects the
  previous camera via `previousRunner->requestDisconnect()` and connects the new
  one via `newRunner->requestConnect()` (queued `CameraRunner` commands). Fixed a
  latent null-deref/stale-workspace defect on this path during the review.
  Verified by the architecture contract suite (38 passed).
- Add focused runtime/controller tests for any remaining unverified edge cases.
  Covered by 2026-06-24 contract tests: trigger edge behavior,
  held-trigger suppression, trigger reset, grab timeout fault `102`,
  VisionOutput send failure fault `201`, invalid pattern fault `400`, invalid
  calibration fault `401`, and camera-loss handling during `RunningCycle`.
- Extend PLC write behavior tests only when new tag families or PLC vendors are
  added. Covered by 2026-06-24 contract tests for valid `M` bit tags, valid `D`
  word tags, and invalid tag rejection through `PlcRunner`.
- Run an operator UI verification pass against a real or simulated runtime
  cycle to confirm dashboard lamps, fault panel, KPIs, result table, and
  task-local log updates.

**Verification already done in the first pass.**
- `architecture_contract_test` built and ran with exit code `0`.
- Full Debug app build completed successfully.

**Additional verification (2026-06-24).**
- `architecture_contract_test` rebuilt with qmake + `nmake /nologo`.
- `architecture_contract_test.exe -silent` exited with code `0`.
- Full Debug app build completed successfully after the Phase 2/3 edits.

---

## 27. RobotKinematics — build-folder deploy done; customer install pending

**Status (2026-06-22): RESOLVED for build-folder runs.** The `RobotKinematics`
component replaced the old `rkin` module and is wired into `ncr_picking.pro` via
`components/RobotKinematics/robotkinematics.pri`, including the Coal mesh-collision
backend. The `.pri` now post-link-copies everything the app needs next to the
binary:

- **Mesh-collision runtime DLLs.** `coal.dll`, `assimp-vc143-mt.dll`,
  `boost_serialization-vc143-mt-x64-1_87.dll`, and
  `boost_filesystem-vc143-mt-x64-1_87.dll` from
  `3rdparty/{coal,assimp,boost}`, only if not already present (`if not exist ...
  copy`). Toggle: `CONFIG -= robotkinematics_copy_dlls`.
- **Mesh assets.** The whole `components/RobotKinematics/presets/Nachi/MZ04`
  tree, including the `simplified/` mesh profile, is copied to
  `<target>/robot_assets/Nachi/MZ04` via `xcopy /D /E`. Toggle:
  `CONFIG -= robotkinematics_copy_assets`.

`runKinematicCheck()` resolves the profile from `<appdir>/robot_assets/Nachi/MZ04`
first, then falls back to the source-tree upward walk.

**Still open — customer install packaging.**
1. **Verify clean target runtime.** Phase 1 found that `coal.dll` dynamically
   depends on Boost serialization/filesystem DLLs, and
   `robotkinematics.pri` now copies them for build-folder runs. Confirm on a
   clean target machine that no further transitive runtime DLLs are missing.
2. **Installer layout.** When an install/packaging step exists, ship
   `robot_assets/` (and the DLLs) next to the binary through the installer rather
   than relying on the build-time `QMAKE_POST_LINK`. Embedding the STL meshes as
   Qt resources is possible but `MeshCollisionProfileJsonLoader` reads from the
   filesystem, so a temp-extract shim would be needed.

Detailed release payload and clean-machine checks are now tracked in
[customer_installer_packaging.md](../product/customer_installer_packaging.md).

**Why deferred.** No installer/packaging step exists yet; the build-folder copy
covers dev and local QA. Revisit when packaging is defined.

**Build note.** The whole `RobotKinematics` source set now compiles into the app;
the heavy lift is environment-dependent (the prebuilt `3rdparty/{coal,boost,
assimp}` trees must be present and ABI-compatible with the MSVC 2022 / Qt 6.8.2
kit used in Phase 1). Validate with a local build.

---

## 28. VisionOutput result payload format — `%08.2f` (RESOLVED 2026-07-01)

**Status: RESOLVED.** The wire format was formalized to
`QString::asprintf("%08.2f", ...)` → fixed-width, zero-padded, 2 decimals
(e.g. `1.0` → `00001.00`). The owner confirmed `%08.2f` is the intended
contract (not the old `.arg(x,0,'f',3)` → `1.000`).

**What was done.**
- [vision_output_request.h](../../src/device/output_device/vision_output_request.h) —
  removed the dead commented-out `%.3f` block; updated the `toString()` /
  `buildPayload()` docstrings to describe and exemplify `%08.2f`.
- Updated the five stale assertions to the new format:
  `tests/vision_output_device_test/main.cpp` (`test_request_payload_format`,
  `test_client_attach_heartbeat_and_io`, `test_reject_duplicate_main_client`)
  and `tests/vision_tcpip_client_device_test/main.cpp`
  (`test_request_payload_format`, `test_connected_heartbeat_and_result`).
- Documented the field format in
  [vision_tcpip_protocol.h](../../src/device/output_device/vision_tcpip_protocol.h),
  [design_rules.md](../rules/design_rules.md) §13.4,
  [uml/02_device_families.puml](../../uml/02_device_families.puml), and the Nachi
  reference (`tests/nachi_client/README.md`, `task1_main_channel.prg`). The Nachi
  parser needs no logic change — `VAL("00001.00")` already yields `1.0`.

**Note.** Both vision-output test `.pro` files were also stale: they predated
the RobotKinematics integration into `vision_tcpip_device_base.cpp` and did not
`include(robotkinematics.pri)`, so they no longer compiled from the CLI. That
wiring was added alongside this change so the suites build and run again.

## 29. Dead `MatchedObject::checkCollisionObject` after the mask-based rewrite

**Status: open cleanup, no behaviour risk.**

The mask-based gripper collision check
(`MatchedObject::checkCollisionObject2`) was confirmed complete on 2026-07-28
after real picking runs with no observed failures, and it is the only path
`ImageMatcher::matching()` calls
([image_matcher.cpp:364](../../src/matching/image_matcher.cpp#L364)).

The superseded point-in-polygon implementation
([match_object.h:146](../../src/matching/match_object.h#L146)) is still in the
header and now has no caller anywhere in `src/` — only comments reference it.
Keeping it invites someone to call the version with the known false negative: a
jaw lying wholly inside a large part registers no collision, because no contour
vertex falls inside the jaw.

**Work when picked up.**

- Remove `checkCollisionObject` and its `goto`-based loop.
- Update the two comments that name it: the "Unlike checkCollisionObject ..."
  paragraph above `checkCollisionObject2`, and the `hasCollision()` brief that
  says "the last collision check (checkCollisionObject or
  checkCollisionObject2)".
- Confirm no test or tool references it before deleting.

## 30. `MatchBoxGripper` is dead code superseded by `GripperBoxes`

**Status: open cleanup, no behaviour risk.** Found during Phase 5 Task B1
(2026-07-29).

[match_box_gripper.h](../../src/matching/match_box_gripper.h) declares
`mtc::MatchBoxGripper` (`m_boxSize`, `m_boxDistance`) and nothing constructs or
references it anywhere in `src/`, `app/`, `tests/`, or `tools/` — the only hits
are its own definition plus the file-list entries in `src/matching/matching.pri`
and `tests/architecture_contract_test/architecture_contract_test.pro`.

Phase 5 introduced `mtc::GripperBoxes`
([gripper_boxes.h](../../src/matching/gripper_boxes.h)) for exactly this concept,
with the `angle` field `MatchBoxGripper` lacks. Leaving both invites someone to
pick the dead one.

**Work when picked up.** Delete the header and its two file-list entries.

## 31. `pattern_group_manager.h` pulls QtWidgets into the matching module

**Status: open, architectural.** Found during Phase 5 Task B1 (2026-07-29).

[pattern_group_manager.h](../../src/matching/pattern_group_manager.h) includes
`<QMessageBox>`, so the level-1 `matching` module — which the module map says
owns the matching engine and must not include UI — drags in QtWidgets. Any
non-GUI consumer (a headless test, a CLI tool) must add `QT += widgets` purely to
compile a data class; this was hit while building a standalone check for the
Phase 5 schema work.

The include-layering contract test does not catch it because it validates
project-header layering, not Qt module usage.

**Work when picked up.** Confirm whether `QMessageBox` is actually used in the
header; if it is only needed in the `.cpp` (or not at all), move or drop the
include. If a manager genuinely needs to raise a dialog, that decision belongs in
the UI layer instead — return a `ManagerResult` and let the caller present it,
which is the pattern the rest of the class already follows.

## 32. Flaky `test_disconnect_notice_on_graceful_close`

**Status: open, intermittent — diagnosed to the product side, not the test.**
Observed during Phase 5 Task C3 verification (2026-07-29).

`VisionTcpipClientDeviceTest::test_disconnect_notice_on_graceful_close`
([main.cpp:230](../../tests/vision_tcpip_client_device_test/main.cpp#L230)) fails
intermittently — roughly **2 runs in 3** on the machine it was measured on, though
the rate moves with load. The assertion that fails is

```
QVERIFY(waitFor([&]() { return peer.gotDisconnectNotice(); }));
```

right after `device.deviceDisconnect()`. It also reproduces when that test is run
**in isolation** (`... test_disconnect_notice_on_graceful_close`), so it is not
interference between tests. Timing is bimodal: a passing run finishes in ~58 ms, a
failing one burns the full `waitFor` timeout (~2 s). The notice either lands
immediately or never.

Not caused by Phase 5: nothing in Phases A–C touches the heartbeat or
graceful-close path.

**Diagnosis.** Device logs are byte-identical between failing and passing runs —
`"VisionTcpip sent disconnect notice"` is emitted every time
([vision_tcpip_device_base.cpp:522](../../src/device/output_device/vision_tcpip_device_base.cpp#L522)).
So the send side succeeds and the payload is lost in transit.

`sendDisconnectNotice()` writes the notice, calls `flush()`, then
`waitForBytesWritten(150)`. That only proves the bytes left the *local* send
buffer — it says nothing about the peer having read them. `deviceDisconnect()`
then calls `detachHeartbeatSocket()->abort()`. An abortive close sends a TCP RST,
and an RST causes the receiver to **discard whatever is still sitting unread in
its receive buffer**. So whether the notice survives is a pure race between the
peer's event loop and the RST. The comment above `kDisconnectFlushMs` shows the
author knew about the discard risk and added the bounded wait as mitigation, but
that mitigation cannot close the hole — no local-side wait can.

**Why it matters beyond the test.** A real remote intermittently misses the
graceful-disconnect notice and has to fall back on heartbeat timeout, which is
slower and looks like a link failure rather than a planned shutdown.

**Work when picked up.** Replace the abortive close on the graceful path with a
graceful one: `disconnectFromHost()` and wait for `disconnected` (bounded), so the
FIN is ordered *after* the notice in the stream instead of racing it. Keep
`abort()` for the lost-connection path, where there is nothing to deliver. The
test needs no change once the ordering is correct.

## 33. Device-level kinematic check ignores the RX/RY axes (RESOLVED 2026-07-29)

**Status: RESOLVED.** Fixed in the same session it was found, before the rotation
offsets could be used in production.

`runKinematicCheck()` now builds
`fromXYZRPY_mm_deg(p.x, p.y, p.z, 180.0 + p.rx, p.ry, p.rz)`.

The emitted `rx`/`ry`/`rz` are the fixed-axis RPY of the orientation the runtime
actually commands: `LocalizationRuntimeController` composes `pick * offset`, and
since `fromXYZRPY` builds `Rz(yaw)·Ry(pitch)·Rx(roll)` with the pick contributing
only a Z rotation, that product is exactly `Rz(rz)·Ry(ry)·Rx(rx)`. The check adds
the top-down tool flip on top as before; rotations about a shared axis commute, so

```
Rz(rz)·Ry(ry)·Rx(rx) · Rx(180)  ==  Rz(rz)·Ry(ry)·Rx(180 + rx)
```

which is why folding the flip into the roll term is exact rather than an
approximation. With `rx = ry = 0` it reduces to the previous `180.0, 0.0, rz`
form, so the 4-axis behaviour is bit-for-bit unchanged.

Verified: root app builds, `architecture_contract_test` 41/41.

---

**Original report** (kept for traceability):

**Status: open, correctness gap opened by Phase 5.** Found 2026-07-29.

`VisionTcpipDeviceBase::runKinematicCheck()`
([vision_tcpip_device_base.cpp:252](../../src/device/output_device/vision_tcpip_device_base.cpp#L252))
builds the pose it tests as

```cpp
RobotKinematics::Pose::fromXYZRPY_mm_deg(p.x, p.y, p.z, 180.0, 0.0, p.rz);
```

The hard-coded `180.0, 0.0` was correct under the 4-axis contract, where every
outgoing pose was a top-down pick and the only free rotation was about Z. Phase 5
made `VisionOutputPosition` six-axis, so `p.rx`/`p.ry` can now be non-zero — and
this check silently drops them. It therefore validates reachability for a
different orientation than the one actually sent, and can pass a pose the robot
cannot reach.

Note this is a **separate, older path** from
`RobotKinematicPickingChecker::isPickable`, which was updated in Phase 5 Task C2
and does honour the full offset. This one is the vision-output device's own
advisory check on outgoing positions.

**Work when picked up.** Decide how `rx`/`ry` compose with the top-down base
orientation — they are not simply additive in fixed-axis RPY, so this needs the
same `Pose` product the runtime uses rather than arithmetic on the angles. The
cleanest fix is probably to have the runtime hand the device the already-composed
orientation instead of re-deriving it here.

---

## 34. `MatchPattern::getImageWithPickPosition()` has no callers left

**Status:** open — found during Phase 5 Task G1 (2026-07-31), deliberately not deleted.

Its only caller was `LocalizationPatternsWidget::updatePatternThumb()`, which used it
to get a `cv::Mat` with the pick axes already drawn in. G1 replaced the thumbnail with
`PatternThumbnailView`, which paints the overlay itself, so the method is now unused:

```
src\matching\match_pattern.h:73    declaration
src\matching\match_pattern.cpp:232 definition
```

**Why it was left in place.** Removing it also makes `vsu::drawAxes2Img` a deletion
candidate, and that helper may have other users; unpicking that was out of scope for a
UI task. Deleting production code is the user's call, not a side effect.

**Worth noting when picked up.** This method was the `matching` module rendering UI
decoration into a pixel buffer — a layering smell independent of whether anything calls
it. Burning marks into the image also cannot honour a zoomable view, which is why the
thumbnail had to stop using it rather than merely preferring not to.

---

## 35. `RuntimeShellWindow` builds its UI in code, with no `.ui` file

**Status:** ✅ **CLOSED 2026-08-23 by Phase 7 / C2.** `runtime_app/ui/runtime_shell_window.ui`
now carries both pages, the menubar and every action; the `.cpp` only wires them. The ADS
dock manager stays in code because it is not a Designer widget — it is constructed into the
form's `wg_dock` host, the same pattern `app/mainwindow.cpp` uses.

**The rule is now mechanical rather than a reading exercise.**
`test_runtime_shell_is_structured_and_form_driven` fails on `new QVBoxLayout`,
`QStackedWidget`, `QToolBar`, `QMenuBar`, `QStatusBar` and friends anywhere under
`runtime_app/`, which is how this went unnoticed for a whole phase in the first place. The
test was verified able to fail by temporarily injecting exactly such a line.

Original text follows.

---

**Status:** open — found 2026-08-23 while verifying the restored `runtime_app/` for
Phase 6 / E7. Deliberately not fixed there.

`runtime_app/runtime_shell_window.cpp` constructs the whole widget tree in C++:
`new QStackedWidget`, `new QVBoxLayout`, `new QLabel`, `new QPushButton`,
`new QToolBar`. There is no `runtime_shell_window.ui`, `runtime_app/runtime_app.pri`
has no `FORMS` entry, and the `.cpp` includes no generated `ui_*.h`.

That contradicts two things:

- `docs/rules/ui_design_rules.md` **Rule 1.1** — "Layout primitives are authored in
  `.ui`" — and its own checklist item, "Structure is in `.ui`; no layout primitives
  `new`-ed in cpp";
- the Phase 6 plan's own Task D4, which lists
  `runtime_app/runtime_shell_window.h/.cpp/.ui` among the files to create.

**Why it was left.** It is not damage from the folder loss — the shipped implementation
never had a `.ui`. Converting it is a self-contained UI task with its own visual
verification, and folding it into the E7 build work would have mixed an untested UI
rewrite into a build-system change. Note that `m_dockManager->setStyleSheet(QString())`
in the same file is **not** part of this: `app/mainwindow.cpp:320` does the same thing
for the same reason (disable the ADS internal stylesheet so the global QSS owns
`ads--` rules), so it is established precedent, not a violation.

**How to pick up.** Author `runtime_shell_window.ui` with both pages, give every styled
widget an `objectName`, add it to `FORMS` in `runtime_app/runtime_app.pri`, and reduce
the `.cpp` to wiring. Then re-run the Checkpoint D manual startup checks — the three
project-select messages are the part most easily broken by a layout port.

Alternatively the owner may decide a shell window is a sanctioned exception to Rule 1.1,
in which case record that exception in `runtime_app/AGENTS.md` so the next agent does
not "fix" it.

---

## 36. The two shells keep separate settings files

**Status:** ✅ **CLOSED 2026-08-23 by Phase 7 / A3.** `AppSettings::filePath()` is now
product-scoped — `GenericDataLocation` + a compile-time product folder — instead of being
derived from `QCoreApplication::applicationName()`. On Windows that resolves to the same
`%APPDATA%\NCRN Pick\settings.dat` the editor always used, so nothing needed migrating on
the editor side, and `legacyFilePath()` reads a shell's old application-scoped file once as
a fallback so settings written before the change are not lost.

The invariant is asserted directly rather than by inspection:
`test_settings_path_is_product_scoped_not_application_scoped` changes the application name
and requires the path **not** to move. That test was verified able to fail by temporarily
restoring the old `AppDataLocation` expression.

The `SingleInstanceGuard` warning below still stands and became more relevant, not less:
Phase 7 / B1 deliberately moves both shells onto **one** instance key, which is the change
this note said to check for. Original text follows.

---

**Status:** open — found 2026-08-23 during Phase 6 / E7b verification.

`AppSettings::filePath()` is `QStandardPaths::AppDataLocation + "/settings.dat"`, and
`AppDataLocation` includes `QCoreApplication::applicationName()`. The shells set
different names — `"NCRN Pick"` in `app/main.cpp`, `"NCRN Pick Runtime"` in
`runtime_app/main.cpp` — so they read and write **different** files:

```
%APPDATA%\NCRN Pick\settings.dat            ncr_picking.exe
%APPDATA%\NCRN Pick Runtime\settings.dat    ncr_runtime.exe
```

For `lastRuntimeProjectPath` that is correct and probably intended — only the runtime
shell uses it. For `theme` and `language` it is a behaviour gap: an operator who sets
Japanese in the commissioning app gets an English runtime, and the runtime shell has **no
UI to change either setting**, so there is no way to fix it from the runtime side.

Whether that is a defect depends on a decision nobody has made yet: are the two shells one
product with one set of preferences, or two applications that happen to ship together?

**How to pick up.** If shared: set the same `applicationName` (or an explicit
`organizationName` + a fixed settings path) in both shells, and check what else keys off
`applicationName` — `SingleInstanceGuard` deliberately uses its own per-shell key
(`"ncr_picking"` / `"ncr_runtime"`) and must keep it, or the two shells would start
blocking each other, which is explicitly not wanted (see
`docs/domains/runtime_app/runtime_shell.md` → "One Process Per Shell"). If separate: say
so in `runtime_shell.md` and give the runtime shell a language selector, because "no way
to change it" is only acceptable while nobody needs to.


---

## 37. `app/mainwindow.cpp` constructs the dock host's layout in code

**Status:** open — noticed 2026-08-23 while making the Rule 1.1 check mechanical for the
runtime shell (Phase 7 / C2).

```
app/mainwindow.cpp:300    QVBoxLayout *layout = new QVBoxLayout(ui->wg_dock);
```

`docs/rules/ui_design_rules.md` Rule 1.1 puts layout primitives in the `.ui`. The runtime
shell's equivalent is now declared in `runtime_shell_window.ui` and the contract test
enforces it for `runtime_app/` — but the check is **deliberately scoped to that shell**,
because widening it would have failed on this line.

**Why it was left.** It is a one-line change in `mainwindow.ui` plus one line removed from
the `.cpp`, but it touches the commissioning window's dock host, which the project owner had
just finished verifying by hand. Mixing an unrequested UI change into a build/structure phase
is how a verified window stops being verified.

**How to pick up.** Declare a `QVBoxLayout` on `wg_dock` in `mainwindow.ui` with zero
margins and spacing, delete the four lines in `createMainContents()`, then widen the contract
test's `layoutPrimitive` scan from `runtime_app/` to both shells so it cannot come back.
Re-check that the dock area still fills the window and the system-log dock still opens.


---

## 38. A project with no localization tasks lands the runtime on a blank page

**Status:** open — noticed 2026-08-23 while fixing the runtime dock layout (Phase 7 / C).

`RuntimeShellWindow::startProject()` switches to `page_runtime` unconditionally once the
project loads. If the project has **no localization task**, `startTaskRuntimes()` creates no
docks, `applyCurrentLayout()` returns early on the empty list, and the operator is left
looking at an empty dock manager. The status bar says `0 task(s) running`, which is the only
clue on screen.

**Why it matters.** This shell owns the screen on a field machine and the whole design rule
for it is "never a blank window" — `onCloseProject()` and all three startup failure modes
already route back to the project-select page for exactly that reason. This path was missed.

**How to pick up.** In `startProject()`, if `shown == 0` after `startTaskRuntimes()`, stay on
(or return to) the project-select page with a reason naming the case: the project loaded but
has nothing to run. Distinguish it from "the file would not load" — the actions differ
(open a different project vs. commission this one). Add it to the startup table in
`docs/domains/runtime_app/runtime_shell.md`, which currently lists three failure modes.


---

## 39. `test_both_shells_take_the_same_instance_key` is flaky on back-to-back runs

**Status:** open — measured 2026-08-23 during Phase 7 / C verification.

The test acquires the **real** product instance key (`ShellHandoff::kInstanceKey`) with a
`SingleInstanceGuard`. Run the suite once and it passes; run it three times in immediate
succession and `editorLikeGuard.tryAcquire()` returns `false` on the later runs. Isolated
re-runs were 58/58 every time.

```text
run A (cmd /c, immediate)  -> 57 passed, 1 failed  (tryAcquire returned FALSE)
run B (cmd /c, immediate)  -> 57 passed, 1 failed
run C (cmd /c, immediate)  -> 57 passed, 1 failed
run alone                  -> 58 passed, 0 failed
```

**Likely cause.** `QLockFile` decides a lock is stale by checking whether the recorded PID
is still alive **and** whether the recorded application name matches. Consecutive runs of
the same executable share an application name, and Windows reuses PIDs quickly — so a lock
left a moment ago can look live to the next run. The lock file was absent from `%TEMP%`
whenever it was inspected afterwards, so nothing is leaking permanently.

**Why it matters.** It costs whoever hits it a wrong conclusion: the failure names the
shared-instance-key contract, which reads like a real regression in the hand-off design.

**How to pick up.** Give the test its own key derived from the shared constant rather than
the constant itself (e.g. `kInstanceKey + "_test"`), and assert separately — by reading
both `main.cpp` files, which this test already does — that the two shells use the shared
constant verbatim. That keeps the contract and removes the collision with any real shell
and with the previous run. Worth doing before anyone wires the suite into CI, where runs
are back-to-back by construction.


---

## 40. Three dead controls in the commissioning shell's UI

**Status:** open — found 2026-08-23 by a sweep prompted by the `btn_browse` defect the owner
reported in the runtime shell (Phase 7 / C). The runtime shell's own control was fixed; these
three are in `app/` and were left alone because that window had just been verified by hand.

| Control | Where | What the operator sees |
|---|---|---|
| `m_actCaptureImage` | `app/mainwindow.cpp:254-255` | A toolbar button **with an icon** on the task toolbar that does nothing. Created and given an icon, never `connect()`ed — its four siblings are wired at `:244-247`. This is the only one that is both visible and clickable. |
| `menuHelp` | `app/mainwindow.ui:68`, added to the bar at `:106` | A top-level **Help** menu that opens an empty popup. Zero `<addaction>` children, never referenced in the `.cpp`. |
| `actionRecent_Job` | `app/mainwindow.ui:151` | Nothing — it is in no `<addaction>` anywhere, so it is unreachable. Dead weight in the form only. |

**Why it matters.** A control that is visible and does nothing is worse than a missing one:
the operator concludes the software is broken rather than that the feature is absent. The
capture-image button is the sharp end of this.

**How to pick up.** `actionRecent_Job` is a safe delete from the `.ui`. `menuHelp` needs a
product decision — populate it (About, version, log folder) or remove it. `m_actCaptureImage`
needs the same: decide what "Capture Image" does from the commissioning window, or take the
button off the toolbar. Do not leave it clickable and inert.

**Related.** The same sweep confirmed everything else in both shells is wired, including the
Theme/Language/tile actions that carry their behaviour through a `QActionGroup` rather than a
direct `connect()` — those look dead to a naive grep and are not.


---

## 41. Editor → runtime hand-off still does not bring the runtime window to the front

**Status:** OPEN, still reproducing — reported again by the owner 2026-08-23 **after** the
blind-gap fix below landed. Runtime → editor works reliably; editor → runtime does not.

### What was fixed, and why it was not enough

`RuntimeShellWindow`'s constructor used to load the project and open every device before
`presentShellWindow()` was reached, so the process ran for seconds with **no window at all**.
That is now split: `beginStartup()` does the loading, posted from `main()` after
`presentShellWindow()`. Measured on the `--handoff` path:

| | Foreground claim reached at |
|---|---|
| Before | +3795 ms after process start |
| After | **+197 ms** |

`presentShellWindow()` also used to defer `activateWindow()` a full event-loop turn behind
`show()`; it now claims immediately **and** re-asserts on the deferred turn.

**Both changes are real improvements and should stay.** Neither fixed the reported symptom, so
the blind gap was at most a contributing factor and is not the root cause. Do not re-do this
work.

### What has been ruled out

- **The grant does not expire with time.** `AllowSetForegroundWindow()` has no wall-clock
  expiry. It is revoked by exactly two things: the next user input not directed at the granted
  process, and another `AllowSetForegroundWindow()` naming a different PID.
- **It survives the granting process exiting.** The reliable runtime → editor direction proves
  this in this codebase — the incoming editor's granter is already dead when it asks.
- **Nothing steals the grant inside the runtime before the claim.** No top-level window or
  modal dialog is created before `presentShellWindow()`: startup failures route to an
  in-window page via `showProjectSelect()`, docks are parented to the hidden main window, and
  `RuntimeLayoutController::applyLayout()` only calls `addDockWidget()`.
- **`QLockFile`'s poll backoff is not the cause.** Its jitter is ~100–300 ms in the realistic
  case, and it is *larger* in the direction that works (the runtime's teardown is the slow
  one), so the mechanism predicts the asymmetry inverted.

### Where to look next, in order

1. **Instrument it — this is the blocker.** The failure is currently undiagnosable from logs:
   - `::AllowSetForegroundWindow()`'s `BOOL` return is **discarded** at
     `src/core/utils/shell_handoff.cpp:115`. It fails silently if the caller is not the
     foreground process at that instant, and the outgoing editor reaches that line only after
     `maybeSave()` (a possible modal save prompt) and `onCloseProject()`. Log the return.
   - The incoming side logs *"foreground claimed"* unconditionally at
     `src/ui/forms/shell_startup.cpp:112`; `activateWindow()` returns void. Follow it with a
     `GetForegroundWindow() == (HWND)window->winId()` check and log the actual outcome.

   Until those two land, "sometimes it works" cannot be separated into "never granted" vs
   "granted then revoked" vs "granted, claimed, refused".
2. **Check the editor's release path for a foreground loss** between the confirm dialog
   (`src/ui/forms/shell_startup.cpp:120`) and `launchSibling()` (`:157`) — `releaseResources()`
   at `:142` runs in between and, for the editor, includes `maybeSave()` and the destruction of
   every `TaskRunner` (up to 3 s of `wait()` each).
3. **Consider the standard fallbacks** only after 1 and 2 give evidence: re-asserting the claim
   on the window's first `showEvent`/`WM_ACTIVATE`, or `FlashWindowEx` as an honest last resort
   (a taskbar flash the operator can act on beats a window that never surfaces). The
   `AttachThreadInput` trick works but is a hack and should be the last option considered.

### Why it matters

The operator switches applications and appears to get nothing, then has to hunt for the
window. On a field machine where the runtime owns the screen, that reads as the machine
failing to start.


---

## 42. The Add Device wizard shows the raw JSON token instead of the display name

**Status:** open — noticed 2026-08-23 while registering the virtual device sub-types
(Phase 7 / D2).

`DeviceRegistry::displayNamesFor()` returns `entry.subTypeValue` — the **JSON token** — and
the wizard puts that straight into its combo (`add_device_wizard.cpp`), then writes
`currentText()` back as the token. Meanwhile `DeviceRegistryEntry::displayName` exists,
carries the readable label ("Basler GigE", "Virtual Camera"), and **nothing reads it** —
`device_registry.h:29` documents it as write-only.

**Why it matters.** It fuses two things with opposite lifetimes:

| | Lifetime |
|---|---|
| The token | **Frozen forever** — it is persisted into customer project files |
| The operator-facing label | Should be changeable any time, and translatable |

Today an operator picking a virtual camera sees the bare word `Virtual`. Making that clearer
— "Virtual Camera (no hardware)" — is exactly the kind of R8 wording that should be free to
improve, and right now it cannot be touched without changing what gets written into every
project file from then on.

**How to pick up.** Populate the combos with `displayName` and carry the token in
`QComboBox::itemData`, then change `buildDeviceJson()` from `currentText()` to
`currentData()`. Add a registry lookup that returns entries (not just token strings) so the
wizard can read both fields. Do it alongside the risk-R8 markers so the wording lands in one
place — the tokens themselves stay exactly as they are.

**Related.** `displayNamesFor()`'s name already lies about what it returns; renaming it to
`subTypeTokensFor()` while here would stop the next person making the same assumption.


---

## 43. Virtual PLC values cannot be driven, so the runtime stops at Ready

**Status:** open — found 2026-08-23 by the project owner completing Checkpoint D. **This is the
single thing standing between "a hardware-free project reaches Ready" and "a hardware-free
project runs a cycle."**

`VirtualPlcDevice` records what the runtime **writes** (`digitalWrites`, `wordWrites`) and
advertises a tag space the signal-map editor can bind to. What it has no way to do is drive a
value **in**.

Every runtime state past Ready begins with the PLC changing an input:

| To reach | The PLC must set |
|---|---|
| RunningCycle | `bExecuteTrigger` |
| Faulted → Ready | `bErrorReset` |
| A camera / pattern-group switch | `nActiveCamera`, `nActivePatternGroup` |

With no way to produce those, a hardware-free project can be built, opened, configured and taken
to Ready — and then nothing. Every state transition the task state machine has is unreachable,
including the fault paths that matter most.

**How to pick up.** `VirtualPlcDevice` already inherits the signal that delivers values:
`PlcDevice::valueChanged(QMap<QString, QVariant>)`, which the runtime consumes through
`PlcRunner`. Add a public setter — `setTagValue(const QString &tag, const QVariant &value)` —
that records the value and emits `valueChanged` with it, then surface it in the device panel.

Two things to decide when doing it:

1. **Where the UI lives.** `VirtualDeviceWidget` is generic on purpose — its rows come from
   `Q_PROPERTY` metadata and it knows nothing about any family. A tag-poke table is
   family-specific, so either it gets a family hook or the virtual PLC gets its own panel. The
   generic widget is worth keeping; a small "extra content" slot it can fill is likely cheaper
   than a second widget.
2. **Whether written values read back.** A real PLC's `M10` reads back what the PLC holds, not
   what this software last wrote. Deciding that the virtual PLC echoes its own writes makes
   handshake signals self-completing and would hide a mapping mistake where the runtime writes
   and reads the same tag by accident. Recommend keeping the injected (input) values and the
   recorded (output) writes as **separate stores**, which is also what the real hardware does.

**Related.** [`docs/domains/virtual_devices/virtual_devices.md`](../domains/virtual_devices/virtual_devices.md)
"Known gap", and the PLC signal contract in
[`docs/domains/task_localization/plc_signal_contract.md`](../domains/task_localization/plc_signal_contract.md).
