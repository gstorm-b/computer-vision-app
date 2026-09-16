# Later TODO list

Outstanding items that were flagged but intentionally deferred to keep PRs
focused. Each entry records WHAT, WHERE, WHY-deferred, and a rough hint on
how to pick it up.

## Triage 2026-09-16 (Phase 10 start)

Phase 9 closed on 2026-09-16 with carried items, and Phase 10 redesigns the runtime core behind the
current controller; its charter and open policy questions (PQ-n) live in `temp_docs/` until
Checkpoint 0 and then under `docs/plan/phase_10/`. Every disposition below is relative to Phase 10:
CLOSED-ALREADY repeats the item's own status, KEEP is open and unaffected by the redesign, ABSORB
names the work package (WP-xx) or policy question that will resolve or re-decide it, MERGE names the
open item that already carries it, NEEDS-OWNER wants a ruling first. **Item bodies below are
untouched**: this table is the only artefact, and nothing was closed, reworded, renumbered or moved.
Rows follow file order, which is not numeric (53 precedes 52, and 50 follows 52).

| # | Title | Existing status | Disposition | Target | Note |
|---|---|---|---|---|---|
| 1 | `SignalsMapWidget::checkEmpty()` caller not wired | CLOSED 2026-09-09 (Phase 9 / C4) | CLOSED-ALREADY | — | Owner-run read of the save dialog in both themes and Japanese is still listed inside |
| 2 | Shared QSS design tokens for themed widgets | DONE 2026-06-24, follow-ups Done 2026-06-24 | CLOSED-ALREADY | — | Resolver and sweep shipped; only the approved `#7a1010` exception remains |
| 3 | `CalibrationBoardDialog` preset-only selection | Deferred (open) | KEEP | — | Custom board authoring is a deferred integration (`AGENT.md`) |
| 4 | `EditableComboWidget::eventFilter` popup-only design | Deferred (open) | KEEP | — | UI note, no defect |
| 5 | `getCurrentMapping()` duplicate camera ids | RESOLVED 2026-06-27 | CLOSED-ALREADY | — | |
| 6 | `RobotDevice` vendor API surface undefined | Deferred (open) | KEEP | — | Waits on a real vendor protocol |
| 7 | `AddDeviceWizard` has no Robot card | Deferred (open) | KEEP | — | Follows item 6 |
| 8 | `RobotRunner` has no runtime wiring | Deferred (open) | KEEP | — | Deferred integration (`AGENT.md`) |
| 9 | `VisionSerial` declared, not implemented | Deferred (open) | KEEP | — | Deferred integration (`AGENT.md`) |
| 10 | `VisionOutputDeviceWidget` is TCP-only | Completed Phase 1; residual waits on item 9 | MERGE | item 9 | The residual (serial widget plus factory branch) is part of delivering item 9 |
| 11 | `VisionOutputRunner` transport-specific signals | No issue today (watch) | KEEP | — | Contingent on a sub-type needing it; runners are kept as they are by the redesign |
| 13 | Widget `static_cast` to concrete device | RESOLVED 2026-06-24 | CLOSED-ALREADY | — | |
| 14 | `cbxVisionType` shown with single sub-type | Deferred (open) | KEEP | — | Cosmetic |
| 15 | `subDeviceTypeLists[PLC]` carries McFrame strings | Deferred (open) | KEEP | — | Waits on a second PLC vendor family |
| 20 | Wizard stack page still named `pgMc` | Deferred (open) | KEEP | — | Cosmetic |
| 21 | `matchingRunner` has no explicit teardown | RESOLVED 2026-06-23 | CLOSED-ALREADY | — | |
| 22 | Code review findings, qt-cpp-review 2026-05-30 | CLOSED 2026-06-24 (batch) | CLOSED-ALREADY | — | 22.20 is intentionally deferred inside the closed batch |
| 23 | UI conformance migration to ui_design_rules | PARTIALLY RESOLVED 2026-06-24 | KEEP | — | Its two listed blockers are reported closed in item 2 and `technical_debt_and_next_steps.md`; the status line looks stale; UI slice outside Phase 10 |
| 24 | `svgIcon()` not theme-aware | RESOLVED 2026-06-27 | CLOSED-ALREADY | — | |
| 25 | `LocalizationDashboardWidget` wiring to refactored `.ui` | RESOLVED 2026-05-31; lamp half CLOSED 2026-09-14 | CLOSED-ALREADY | — | The remaining dark/light review pass is the Phase 4 on-hold UI pass tracked in `technical_debt_and_next_steps.md` |
| 26 | Localization runtime production follow-ups | CLOSED 2026-09-14 (Phase 9 / Z3) | CLOSED-ALREADY | — | |
| 27 | RobotKinematics deploy done; customer install pending | RESOLVED for build-folder runs; install open | KEEP | — | The customer-install half is on hold with Phase 4 |
| 28 | VisionOutput result payload format `%08.2f` | RESOLVED 2026-07-01 | CLOSED-ALREADY | — | |
| 29 | Dead `MatchedObject::checkCollisionObject` | Open cleanup | KEEP | — | Matching module, no behaviour risk |
| 30 | `MatchBoxGripper` dead code | Open cleanup | KEEP | — | Matching module, no behaviour risk |
| 31 | `pattern_group_manager.h` pulls QtWidgets into matching | Open, architectural | KEEP | — | Layering debt outside the core redesign |
| 32 | Flaky `test_disconnect_notice_on_graceful_close` | CLOSED 2026-09-09 (Phase 9 / D2) | CLOSED-ALREADY | — | The original wrong diagnosis is kept below the close for traceability |
| 33 | Device kinematic check ignores RX/RY | RESOLVED 2026-07-29 | CLOSED-ALREADY | — | |
| 34 | `MatchPattern::getImageWithPickPosition()` has no callers | Open, deliberately not deleted | KEEP | — | |
| 35 | `RuntimeShellWindow` UI built in code | CLOSED 2026-08-23 (Phase 7 / C2) | CLOSED-ALREADY | — | |
| 36 | Two shells keep separate settings files | CLOSED 2026-08-23 (Phase 7 / A3) | CLOSED-ALREADY | — | |
| 37 | `app/mainwindow.cpp` builds dock layout in code | Open | KEEP | — | UI cleanup slice |
| 38 | No-task project lands runtime on blank page | Open | KEEP | — | Runtime shell UX, not the core |
| 39 | Flaky `test_both_shells_take_the_same_instance_key` | Open (flaky) | KEEP | — | Test hygiene |
| 40 | Three dead controls in commissioning shell | Open | KEEP | — | UI cleanup slice |
| 41 | Editor-to-runtime hand-off does not raise window | OPEN, still reproducing | KEEP | — | Shell defect, not the core |
| 42 | Add Device wizard shows raw JSON token | Open | KEEP | — | UI defect |
| 43 | Virtual PLC inputs cannot be driven | RESOLVED 2026-09-07 | CLOSED-ALREADY | — | |
| 44 | `build_docs.bat` tool defaults point nowhere | Open; owner decision listed | KEEP | — | Not blocking (charter §1.1); the tool-root decision is the owner's and unrelated to the core |
| 45 | `AGENTS.md` cards give Doxygen `\ref` warnings | Open, cosmetic | KEEP | — | |
| 46 | `McProtocolConfig` copies share context config | Open | KEEP | — | Device-layer defect, wants its own verification pass |
| 47 | `MCRequest::isValid()` never checked | Open | KEEP | — | Device-layer limit decision, wants its own verification pass |
| 48 | `addPropertyToBrowser` copied in three widgets | Open | KEEP | — | UI de-duplication |
| 49 | Basler continuous shot, `startAutoContinuousShot()` pair | Deferred (open) | KEEP | — | Camera feature with no request behind it |
| 53 | `clearRoleContext()` blanket-disconnects dual-role runner | CLOSED 2026-09-09 (Phase 9 / A3), no test | CLOSED-ALREADY | — | Closed by reasoning; the role-health rows of WP-32 are where a test for this shape would land |
| 52 | Modbus 32-bit word order not settable | Open | KEEP | — | Device config option, not the core |
| 50 | `PlcMitsuDeviceWizard` is dead code | Open | KEEP | — | Cleanup |
| 51 | `IDevice::errorOccurred` never emitted | Open; runner half RESOLVED 2026-09-08 | ABSORB | WP-21 | The to-be table must say whether device errors are a core event (then devices emit) or the channel is deleted; no PQ yet |
| 54 | Held `bExecuteTrigger` fires at runtime start | Open, owner-gated (Phase G) | ABSORB | PQ-1 | Phase G design reused; implemented on the new core as WP-50 |
| 55 | `setup()` misreports unregistered camera as calibration | CLOSED 2026-09-09 (C2 + C7) | CLOSED-ALREADY | — | |
| 56 | MC 1C/3C hardware coverage and bench numbers | Partly closed by E1; 1C/3C and bench open | KEEP | — | Owner-run hardware verification, outside the core |
| 57 | Robot pick check active on dual-role Modbus | CLOSED 2026-09-14 | CLOSED-ALREADY | — | |
| 58 | Startup neither validates nor announces selection | Part A CLOSED 2026-09-09; Part B open | ABSORB | PQ-2, PQ-3 | Never-written-register fault to PQ-3, late PLC re-read to PQ-2; the first-poll half rides with item 54 |
| 59 | Housekeeping from the item-58 investigation | CLOSED 2026-09-09 | CLOSED-ALREADY | — | The device-level MC test gap is carried by item 56 |
| 60 | Active-index echo rejected on Modbus master | Open; fixed by C3, which landed 2026-09-09 | KEEP | — | Status line is stale: the controller now publishes `nActiveCameraStatus` / `nActivePatternGroupStatus` and no longer echoes; needs a closing note, not a triage row |
| 61 | Dashboard never re-reads task config | CLOSED 2026-09-11 (Phase 9 / F2) | CLOSED-ALREADY | — | The residual camera-lamp refusal-path gap is unfiled; the Selection / outputSnapshot design (WP-33, WP-34) covers it |
| 62 | Stale root `main.moc` deletes tests | Open, root cause found | KEEP | — | Build hygiene; normal debug loop |
| 63 | Fault code 400 covers four content faults | Open, deliberate (C7) | ABSORB | PQ-15 | Fault codes are contract; decide before the Phase 10 manual |
| 64 | Dual-role Modbus publish collides with poll | Open, field-observed 2026-09-14 | ABSORB | PQ-10 | The retry policy is the core decision; the transport half stays a normal debug loop |
| 65 | `app/` no longer resolves in build | RESOLVED 2026-09-14 | CLOSED-ALREADY | — | |
| 66 | Two editors for the robot pick check | Open, filed 2026-09-14 | KEEP | — | Config/UI ownership question, not a core state; no PQ exists |
| 67 | Recovery policies settable only in tests | Open, filed 2026-09-14 | ABSORB | PQ-14 | The policy struct lands in WP-32 |
| 68 | Two-position cap hard-coded; PLC debug print | Open, owner to confirm | ABSORB | PQ-8 | The debug-print half is a trivial cleanup independent of the ruling |
| 69 | PLC write ack: refused write unverified on hardware | Open for the E4 hardware observation only | KEEP | — | Owner-run, not blocking (charter §1.1) |
| 70 | Vision-output client stuck Recovering after heartbeat return | Open; investigation paused 2026-09-10 | ABSORB | PQ-7 | Core half: what reconnect means on `RoleStatus(LostConnected)` (S-06 row, WP-32); the client redial bug stays a device debug loop |

Counts: CLOSED-ALREADY 21 · KEEP 35 · ABSORB 8 · MERGE 1 · NEEDS-OWNER 0 (65 rows).

Post-triage housekeeping (PM, 2026-09-16, after the table above was frozen): items **60** and **23**
received closing notes (their KEEP rows stand as the triage-time reading); items **71** (A1's M-only
hardware check, carried out of Phase 9) and **72** (refused index updates no camera visual, residual
of item 61) were filed below.

---

## 1. `SignalsMapWidget::checkEmpty()` — caller not wired

**CLOSED (2026-09-09) — Phase 9 Task C4.** Not by wiring `checkEmpty()`: that method was
**deleted**. It did two jobs in one call — purge every orphan, *then* report what it had
purged — so the caller learned what was destroyed only after it was destroyed, and there was
no way to ask the question without answering it destructively. That shape is why no caller
was ever chosen, and choosing a trigger for it would have shipped the problem.

Replaced by two methods that each do one thing (`src/ui/widgets/signals_map_widget.h`):

| Method | Does |
|---|---|
| `QStringList orphanRowNames() const` | reports, changes nothing |
| `void clearRowTags(const QStringList &)` | purges exactly what it is told, nothing else |

**Trigger chosen: project Save**, via `LocalizationSettingWidget::confirmOrphanedSignalsBeforeSave()`
→ `LocalizationTaskWidget` → `MainWindow::saveToFile()`, which aborts the save if any task
cancels. The dialog offers *Clear and save* / *Save as-is* / *Go back* (default **Go back**), and
asks a second time before clearing a **required** signal, naming the count and the tags about to
be lost. Commission start is covered separately by the runtime gate, below.

**Tests:** `test_orphan_row_names_reports_orphans_without_clearing_them` (the report/purge split —
the whole point of the new shape), plus the runtime half that made the trigger worth having:
`test_an_orphan_tag_fails_setup_on_required_and_optional_signals_alike`,
`test_a_bit_signal_mapped_to_a_word_tag_is_an_orphan_for_that_signal`,
`test_each_required_signal_unmapped_fails_setup_naming_it`.

**Field evidence:** the gate refused a real orphan on the owner's cell — *"Signal \"Camera
selection\" (nActiveCamera) is mapped to tag IR01000, which the device 05 does not provide as a
register."* → Faulted (`app_log_2026-09-08.txt:1552-1553`). The owner confirmed the save dialog
appears and all three buttons work (2026-09-08). **Still owner-run:** reading the dialog in both
themes and in Japanese — its strings entered the `.ts` in the 2026-09-09 sweep and are
`unfinished`, so it renders in English until a translator fills them in.

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

**CLOSED (2026-09-16, closing note added at the Phase 10 triage).** The two blockers named at the
end of this item — `DevicesMonitorWidget` / `DeviceRowDelegate` staying dark in light mode, and
`SystemLogForm` lacking dark/light styling — are recorded as done on 2026-06-24 in item 2 and in
`technical_debt_and_next_steps.md` ("Finish the remaining UI token closeout"). Nothing else in the
item is open; the PARTIALLY RESOLVED line below predates those closures and is kept for traceability.

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

**Connection-lamp half CLOSED 2026-09-14 (Phase 9 / F2), both checks owner-confirmed.**
2026-09-11: a role-binding change reaches the dashboard without reopening the window (item 61's
half). 2026-09-14: pulling the cable turned the lamp **red and then back to green** on reconnect —
the liveness half this entry was actually about. `app_log_2026-09-14.txt` shows the underlying
transition at `:1105` (`Ready -> Recovering`, `role=primary_plc deviceId=05 status=LostConnected`)
and the reconnect at `:1113` five seconds later, so the lamp was following real status changes and
not a stale seed. The three
connection lamps were dead for the life of every runtime, and the "RESOLVED" above did not
cover it. The widget is constructed at `runtime_shell_window.cpp:472`, **nine lines before**
`beginRuntime()` at `:481`, so `wireConnectionLamp()` resolved `runnerFor()` to null on all
three roles, set "—", and never retried. `initWidget()` now re-wires on
`ITask::runtimeStarted` and on `TaskRunner::phaseChanged` (which also covers the return to
Idle). **Re-wiring alone is not enough** — see item 61, fixed in the same edit. This item
cannot close until the owner confirms the lamps live on a cell; see Checkpoint F.

---

## 26. Localization runtime production follow-ups after first implementation pass

**CLOSED 2026-09-14 — Phase 9 / Z3, per plan decision D6.** The umbrella's open bullets were folded
into the Phase 9 tasks that did the work, and each closed there with evidence:

| Bullet | Closed by | Evidence |
|---|---|---|
| Latency measurement (named below as a remaining gate) | **F3** | `CycleTimings` on every `CycleResult`; 85 cycles on the dual-role Modbus cell 2026-09-14, median cycle 304 ms with matching 79 % of it. The threading-revisit criterion was evaluated: no revisit. `test_successful_cycle_stamps_every_stage_monotonically`, `test_faulted_cycle_carries_only_the_stages_it_reached` |
| "Add focused runtime/controller tests for any remaining unverified edge cases" | **B1 / B2**, then every later Phase 9 task | B1 pinned the runner → controller liveness forwards; B2 added the `TaskLocalization`-level fixture that reaches `beginRuntime()`. The contract suite grew from 105 to 159 cases over the phase |
| "Run an operator UI verification pass…" | **F2** (lamps), **F3(b)** (cycle-time KPI), owner runs at Checkpoints A, C-1 and F | Owner-confirmed: lamps follow a binding change without reopening the window (2026-09-11) and a cable pull red → green (2026-09-14); the fault panel, result table and task log were exercised on every owner run. The formal release-gate UI pass stays on hold with Phase 4 — see `technical_debt_and_next_steps.md` |

The PLC-write bullet was conditional — *"only when new tag families or PLC vendors are added"* — and
was met when they were: Phase 8 added Modbus, and Phase 9 / E1–E4 added write-completion coverage for
MC, Modbus and the virtual PLC (item 69).

The original status is kept below as written.

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

**CLOSED (2026-09-09) — Phase 9 Task D2, on the second diagnosis.** The flake is gone in both
duplicated copies: the 20-run bar reads **0 / 20** for `vision_output_device_test` and **0 / 20**
for `vision_tcpip_client_device_test`, and removing the fix returns them to **13 / 20** and
**16 / 20**.

**The cause was never the close *kind*.** Closing a socket whose **OS** receive buffer still holds
unread inbound bytes makes the stack send **RST** rather than FIN, and an RST tells the peer to
discard *its* receive buffer — including the disconnect notice it had not read yet. The heartbeat
socket is precisely the one holding unread bytes, because the peer acks every probe.

The fix is `VisionTcpipDeviceBase::drainBeforeClose()`, called from `detachMainSocket()` and
`detachHeartbeatSocket()` before `abort()`. Bounded at 50 one-millisecond read attempts, exiting on
the first that finds nothing, so the usual cost is a single attempt.

**Everything below this line is the ORIGINAL diagnosis and it is WRONG.** It is kept because the
sequence of refutations is the useful part, and because the same reasoning would otherwise be
re-derived by the next reader. The measurements, in order:

| Build | Failures / 20 |
|---|---|
| Original `abort()` | 10 |
| Graceful close (`disconnectFromHost()` + bounded wait) — the fix this entry originally prescribed | 7 |
| Graceful close + drain **Qt's** buffer | 13 |
| Peer draining on `disconnected` as well as `readyRead` | 5 |
| **No teardown at all** | **0** |
| `QThread::msleep(50)`, then teardown | 14 |
| **Drain the OS receive buffer, then teardown** | **0** |

Three things each kill part of the old story:

1. **With `abort()` the peer reports `RemoteHostClosedError`, not `ConnectionResetError`** — and
   the same with the graceful close. The abortive close was not producing the RST this entry blamed.
2. **Suppressing the teardown entirely fixes it, `msleep(50)` does not.** So the bytes always left
   the process, and the loss is caused by the close itself — not by timing, and not by the write.
3. **`bytesAvailable()` reports Qt's buffer, not the OS's.** Qt only moves bytes across on a read
   notification, so a socket whose thread has not returned to its event loop reports `0 available`
   while the OS buffer is full. That is why "available = 0" read as proof there was nothing unread,
   and why an earlier `readAll()` drain changed nothing. `waitForReadyRead()` is what forces the
   transfer — and that one call is the difference between 13 / 20 and 0 / 20.

*The lesson worth keeping: the original entry reasoned from RST semantics to a fix without ever
measuring whether an RST was occurring. It read as authoritative for six weeks.*

---

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

**Work when picked up.** ~~Replace the abortive close on the graceful path with a
graceful one: `disconnectFromHost()` and wait for `disconnected` (bounded), so the
FIN is ordered *after* the notice in the stream instead of racing it. Keep
`abort()` for the lost-connection path, where there is nothing to deliver. The
test needs no change once the ordering is correct.~~

---

### ⚠️ 2026-09-09 — the RST diagnosis above is WRONG. Built, measured, reverted.

Phase 9 Task **D2** implemented exactly the work described above: a `SocketClose::{Graceful,
Abortive}` mode on both detach helpers, `disconnectFromHost()` + `waitForDisconnected(300)` on
the graceful path, `abort()` kept for `declareLostConnection()`, with the timing budget stated
(150 ms notice flush + 2×300 ms ≤ 750 ms, inside `disconnectAndWait()`'s 3000 ms guard).

**It did not work, and the measurements say the premise is false.** All on the server suite,
`test_disconnect_notice_on_graceful_close` run in isolation, 20 consecutive runs per row:

| Build | Failures |
|---|---|
| Graceful close (the proposed fix) | **7 / 20** |
| Graceful close + drain the receive buffer before closing | **13 / 20** |
| Forced back to the original `abort()`, same binary | **10 / 20** |
| Peer additionally draining on `disconnected` as well as `readyRead` | **5 / 20** |

All four are the same rate within the noise this flake is already known for. The change was
**reverted in full**; only Task D1 landed from Phase D.

**What the instrumentation actually showed.** Server side, logged inside `sendDisconnectNotice()`
and the close, comparing a passing and a failing run of the same binary:

```text
DIAG notice flush=1 waited=0 toWrite=0 state=3        <- identical in both
DIAG closeSocket heartbeat pre-state=3 toWrite=0 available=0
DIAG closeSocket heartbeat post-state=0 toWrite=0     <- clean FIN, no timeout logged, ever
```

Peer side, on a failing run:

```text
DIAG hbRx=[] state=0 err=1 errStr=The remote host closed the connection probes=1
```

Three things follow, and each kills part of the old diagnosis:

1. **`err=1` is `RemoteHostClosedError` — a clean FIN — and it is the SAME with `abort()`.**
   If the abortive close were producing the RST the diagnosis blames, the peer would report
   `ConnectionResetError`. It never does. *The abort was not resetting the connection.*
2. **`available=0` on the server before the close.** There is no unread inbound data, so the
   "closing a socket with unread data forces RST" variant is out too.
3. **`hbRx` is empty and `probes=1`.** The peer parsed exactly `connection_check.` and nothing
   else — with framing intact and nothing left over. The notice's 11 bytes never arrived at the
   peer at all. Draining on `disconnected` does not recover them, so they were not buffered and
   lost to read/close ordering either.

So: the server writes the notice, `flush()` reports success, `bytesToWrite()` is 0, the socket
closes cleanly — and about a third of the time the bytes are still never delivered. The loss is
below Qt, in the write-then-close window on Windows loopback, and **no change on the send side
that still closes immediately afterwards will fix it.**

**Where to look next** (untested — hypotheses, labelled as such):

- Do not close the heartbeat socket immediately after the notice. Wait, bounded, for the peer to
  close its end (a peer that understood the notice will), and only then tear down. This changes
  the protocol's shutdown handshake, not just the socket call, and needs the owner's agreement
  because a peer that never closes costs the full bound on every disconnect.
- Or accept that the notice is best-effort — which is what the code comment already claims — and
  **change the test to match the contract** rather than asserting a delivery the protocol does not
  promise. If this is the answer, item 32 closes as "test asserts more than the protocol
  guarantees" and the two duplicated tests are rewritten.

**Do not re-attempt the graceful-close fix without first reproducing the table above.** It is
built, it is measured, and it does not move the number.

**The test exists in duplicate** — `tests/vision_output_device_test/main.cpp:217` and
`tests/vision_tcpip_client_device_test/main.cpp:231`. This entry originally named only the
second. Both fail, at similar rates, from the same shared base-class path.

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

**Status (2026-09-07): RESOLVED.** `VirtualPlcDevice` implements the new
`vc::device::IPlcInputSimulator` capability; `PlcRunner::requestInjectInputValue()` carries a value
onto the device thread and `supportsInputSimulation()` answers **per device**, so no real PLC ever
advertises drivable inputs; `VirtualPlcInputPanel` is added to the virtual device page by
`VirtualDeviceWidget` when — and only when — the runner says the device supports it.

Both decisions this item flagged were taken as recommended: **separate stores** for injected inputs
and recorded writes, and a **hook on the generic widget** rather than a second widget class. The
UI is in the commissioning shell only, by the owner's decision — `ncr_runtime.exe` has no device
pages, and the operator shell is not a place to forge PLC inputs.

Verified by six contract cases (95 → 101), each negative-checked. The one that matters is
`test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs`: it drives the
controller through the **real delivery path** — device → runner thread boundary → `valueChanged` →
controller — rather than calling `handlePlcValues()` directly the way every other case in that
suite does. Full contract and traps:
[`docs/domains/virtual_devices/virtual_devices.md`](../domains/virtual_devices/virtual_devices.md)
→ "Driving a virtual PLC's inputs".

The original entry follows, unchanged.

---

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

---

## 44. `build_docs.bat` tool defaults point at a path that does not exist

**Found:** 2026-08-23, rebuilding the Doxygen reference for the Phase 7 close-out.
**Severity:** Low — loud failure, trivial workaround. **Effort:** minutes.

Commit `51469ecf` changed the three tool defaults in
[`docs/doxygen/build_docs.bat`](../doxygen/build_docs.bat) from `C:\build_packages\...` to
`C:\BAO\...`. On this machine `C:\BAO` does not exist and `C:\build_packages` does, so the script
exits 1 at its first guard:

```
[build_docs] DOXYGEN_EXE not found: C:\BAO\doxygen-1.17.0-win64\doxygen.exe
```

The Phase 7 rebuild ran by setting `DOXYGEN_EXE`, `GRAPHVIZ_DOT_DIR` and `PLANTUML_JAR` explicitly
— the documented override path, and the reason this is Low rather than a blocker.

Left alone deliberately: the change was the project owner's own, and which of the two roots is
canonical is their call, not something to guess at. It may be correct for a machine this repo is
also used on.

**Decide one of:**
- `C:\build_packages` is canonical → restore the defaults.
- `C:\BAO` is canonical → the tools need to move/symlink there on this machine.
- Neither → drop the hard-coded defaults and make the three variables required, which is closest
  to `AGENT.md`'s "local machine paths come from environment variables" rule.

The guard behaviour itself is right and should stay: it fails loudly instead of emitting a
partial reference.

**Update 2026-09-14 (Phase 9 / Z2).** The tools are now at **neither** root on this machine:
`C:\build_packages\doxygen-1.17.0-win64\doxygen.exe`, `…\Graphviz-15.1.0-win64\bin\dot.exe` and
`…\plantuml\plantuml-java8-SNAPSHOT.jar` all fail `Test-Path` (Java is present). The Z2 documentation
pass therefore could not rebuild the reference, and `docs/generated/doxygen/` still shows the pre-Z2
comments and diagrams. The "Found on this machine under" column of `documentation_build.md` is stale
for the same reason. The decision above is unchanged; it now also has to say where the tools come
from.

---

## 45. `AGENTS.md` scope cards produce unresolvable `\ref` warnings in the Doxygen build

**Found:** 2026-08-23, same rebuild. **Severity:** Low — cosmetic. **Effort:** minutes.

The Doxyfile's `FILE_PATTERNS` includes `*.md` with `RECURSIVE = YES`, so all 17 module
`AGENTS.md` scope cards are parsed as Doxygen pages and appear in the generated reference. That is
useful and worth keeping — a reader lands on the module's own boundary rules.

The side effect is that their relative Markdown links to files *outside* the Doxygen input
(`docs/domains/...`) cannot be resolved, giving 3 warnings of the form:

```
app/AGENTS.md:20: warning: unable to resolve reference to
  '.../docs/domains/runtime_app/runtime_shell.md' for \ref command
```

Pre-existing — not introduced by Phase 7; adding `runtime_app/` to `INPUT` merely added one more.

**Options,** cheapest first: leave it (the links work for anyone reading the repo, which is who
`AGENTS.md` is for); or make the offending links absolute URLs so Doxygen stops treating them as
`\ref`; or add `docs/domains` to `INPUT` so the targets resolve — the largest change, and it pulls
the whole domain-doc tree into the API reference.

---

## 46. `McProtocolConfig` copies share their context and message-interface config

**Found:** 2026-08-25, during Phase 8 / A1 while establishing what a new context's `clone()`
has to deep-copy.

`McProtocolConfig` holds `std::shared_ptr<McContext> m_context`, and `McContext` holds
`std::shared_ptr<McMsgItfConfig> m_msg_cfg`. Neither class declares a copy constructor, so the
compiler-generated ones copy the *pointers*:

- `McProtocolDevice::mcProtocolConfig()` returns `m_config` **by value**
  ([mc_protocol_device.cpp:203-205](../../src/device/plc/mc_protocol_device.cpp#L203-L205)),
  and the returned "copy" shares the live device's context object.
- `McProtocolConfig::clone()` does deep-copy the context (`setContext()` -> `ctx->clone()`),
  but every concrete `clone()` is `new Context_McXX(*this)` -- the implicit copy again -- so the
  cloned context still shares the original's `McMsgItfConfig`.

`MitsubishiMcDeviceWidget` keeps a `McProtocolConfig m_config` it calls a working copy and edits
through the property browser. It is not a copy: edits to context fields land straight on the
device's live configuration, before `saveConfig()` is called and regardless of whether it ever is.

**Why it has not been noticed:** the widget calls `saveConfig()` on nearly every edit path, so the
value the user typed does end up where they expect. What is missing is the *isolation* -- there is
no state in which the widget holds an unsaved edit, and `setDeviceConfig()`'s "no-op while
connected" guard can be bypassed by editing a field, since the write already happened through the
shared pointer.

**Scope of a fix:** give `McContext` and `McProtocolConfig` copy constructors that deep-copy
(`m_msg_cfg` cloned, not shared), which needs a `clone()` on `McMsgItfConfig`. Small and
contained, but it changes when edits reach a connected device, so it wants its own verification
pass rather than riding along with a feature task.

**Phase 8 note:** the new 1C/3C contexts (task A2) deep-copy their own message-interface config,
so they do not add to this. That makes 3E the odd one out until this is fixed -- deliberate, and
recorded here so the inconsistency is not read as an oversight in the new code.

---

## 47. `MCRequest::isValid()` is never checked, so its 128-device cap is dead

**Found:** 2026-08-25, during Phase 8 / A5 while checking what device counts the new
computer-link codecs must survive.

`MCRequest`'s constructors set `is_config = false` when the amount is outside 1..128
([mc_request.h:94-106](../../src/device/plc/mc_request.h#L94-L106)), and `isValid()` reports it.
Nothing in the polling path ever calls it: `McProtocolDevice::update_m_map()` /
`update_d_map()` build a request straight from an optimized range and push it onto the polling
queue, and `Frame3E` uses `m_amount` without asking.

Meanwhile the UI lets the range go much higher -- `McContext`'s `amountMAddress` /
`amountDAddress` carry `Q_CLASSINFO(..._max, "1024")`.

**Why nothing has broken.** A 3E batch read of 200 points is legal on the wire, so the invalid
flag is inert for the only frame that has shipped. The cap and the UI limit have simply never
had to agree.

**Why it matters now.** The computer-link frames have a real field width: the 1C device-count
field is two hexadecimal characters, so 255 points is the hard maximum, and `Frame1C` refuses
anything larger with `RequestFrameError`. A project configured with more than 255 M devices
would therefore poll fine on 3E and fail every request on 1C -- with the failure appearing at
send time, not at configuration time, which is the wrong end to discover it.

**Scope of a fix:** decide which limit is real (per frame, most likely), enforce it where the
range is configured rather than where the frame is built, and either honour `isValid()` in the
polling path or delete it. Small, but it changes what an existing project is allowed to hold, so
it wants its own verification pass rather than riding along with a feature task.

---

## 48. `addPropertyToBrowser` now exists in three device widgets

Added while implementing Phase 8 / B6 (`ModbusDeviceWidget`).

The ~50-line function that mirrors one `QMetaProperty` into a `QtVariantProperty` — enum keys
translated in the enum's scope, display name from `<prop>_name`, `minimum`/`maximum` from
`<prop>_min`/`<prop>_max`, disabled when not writable — is now written out three times:

- `src/ui/forms/camera/basler_camera_widget.cpp`
- `src/ui/forms/plc/mitsubishi_mc_device_widget.cpp`
- `src/ui/forms/plc/modbus_device_widget.cpp`

**Why it was copied rather than extracted.** Extracting it means editing two commissioned
widgets during a task that is about adding a third. That is the wrong trade to make inside a
feature task, and the two existing copies have shipped.

**Why it matters.** This is exactly the shape Phase 7 found in the property browsers: one block
copied five times, where every copy translated the enum keys beside the label and none translated
the label itself. Not five oversights — one bug, five times. A third copy raises the odds that a
future fix lands in two places out of three, and the widget that misses it will look correct
because the labels still appear, just untranslated.

**Scope of a fix:** move it to `src/ui/widgets/property_browser/` beside `PropertyBrowserWidget`
(which already owns the manager and factory these callers reach for), have all three widgets call
it, and delete the copies. Mechanical, but it touches three device panels, so it wants its own
verification pass — each widget's property browser opened and an enum, an int with a range, and a
read-only property confirmed on screen.

---

## 49. Continuous shot for the Basler camera, and the unimplemented `startAutoContinuousShot()` pair

**What.** Two leftovers from the JAI continuous-shot work (Phase C-2,
`docs/history/plan/phase_8_implementation_plan.md`):

1. `BaslerGigECamera::startContinuousShot()` / `stopContinuousShot()` still
   `return false;` / `{}`. Its widget has the same "Continuous shot" button,
   wired to nothing.
2. `startAutoContinuousShot()` / `stopAutoContinousShot()` are unimplemented on
   **every** camera and called by nobody. `VirtualCameraDevice` still answers
   `true` without doing anything.

**Where.** `src/device/camera/camera_basler_gige.{h,cpp}`,
`src/ui/forms/camera/basler_camera_widget.cpp` (the `connect()` for
`btn_auto_shot` is commented out), `src/device/camera/camera_device.h`,
`src/device/virtual/virtual_camera_device.h`.

**Why deferred.** (1) is real work with no request behind it: the owner asked for
continuous shot on the JAI camera, and doing the Basler one at the same time would
have meant rewriting the panel of a camera already running in production, in the
same change that introduced the feature. (2) has no defined meaning distinct from
the `startContinuousShot()` pair — inventing one would be a second abstraction
before the first has a user.

**How to pick up.** For (1) the device layer is now a worked example: queued
self-posting pump (never a loop), frames on `continuousFrameReady()` and not
`grabFinished()`, state reported after every start/stop request. Pylon's
`StartGrabbing(GrabStrategy_LatestImageOnly)` replaces the eBUS pipeline, and the
bandwidth cap matters there too. The runner and the widget pattern need no changes.
For (2), the honest options are to give it a meaning (hardware-triggered
free-running, most likely) **or** delete the four methods; do not leave
`VirtualCameraDevice` claiming success for something that does nothing.

**Added 2026-09-03 — (3) `btn_baklight_toggle` is dead in the Basler widget too.**
Declared at `src/ui/forms/camera/basler_camera_widget.ui:174`, enabled, and its
`connect()` commented out at `basler_camera_widget.cpp:243-244` — right beside the
commented-out `btn_auto_shot` from (1). The owner found the JAI copy of this button
on 2026-09-03 while accepting Checkpoint C.

**Updated 2026-09-04 — C9 has landed, so this is now cheap.** The whole middle layer
exists: `DeviceCommandKind::CameraBacklightOn`/`Off`, `CameraRunner::requestBacklight()`,
`CameraDevice::backlightStateChanged()` and the `setBacklightOverride()` virtual.
`BaslerGigECamera` answers `hasIOPort()` **true**, so it passes the runner's refusal
check and falls through to `CameraDevice::setBacklightOverride()`'s base
implementation, which logs *"Backlight override is not implemented by this camera"*
and fails the command. That is deliberate and loud — the two answers contradict each
other and only the subclass can settle it — but it means **the Basler path is now
wired-and-failing rather than dead**, which is a different and more visible state
than before. Picking it up is: override `setBacklightOverride()` on
`BaslerGigECamera` the way the JAI one does (route the auto sequence through a
single `setAutoBacklightState()` first — that indirection, not the flag, is what
makes the override impossible for a future grab path to forget), uncomment the
widget's `connect()`, and rename `btn_baklight_toggle` to `btn_backlight_toggle` as
the JAI form now has it.

**Also from C9: the sibling action buttons guard instead of disabling.**
`btn_trigger` and `btn_auto_shot` in both camera widgets stay enabled while the
camera is disconnected and return early in their slots, so a click does nothing
visible. That is the same "looks live, is not" shape C9 existed to fix; the
backlight button is now genuinely disabled via `applyConnectionVisual()`, and the
siblings were left alone as out of scope. Make them consistent — preferably by
disabling, not by adding more silent guards.


---

## 53. `clearRoleContext()` blanket-disconnects a runner that may hold two roles

**CLOSED (2026-09-09) — Phase 9 Task A3, with NO test, deliberately.** The fix is in:
`RoleRecoveryContext` carries `statusConnection` / `errorConnection` and `clearRoleContext()`
drops exactly those two instead of every connection from that runner.

**There is no test and there will not be one, and that is the honest closure.** The trace below
(added 2026-09-08) establishes that no path in the current codebase reaches
`clearRoleContext()` with a populated fixed-role context: both fixed roles are bound in exactly
one place, `bindFixedRoleRunners()`, and `resetRuntimeBindings()` has already emptied them and
already dropped `m_plcValueConnection` before the blanket form could fire. A test written against
this would pass identically on the fixed and the unfixed build.

A planned fixture (**O-1**) was **withdrawn** rather than written, because making it "fail" would
have required breaking two passing tests to construct a state the product cannot enter. A test
that cannot distinguish the fix from its absence is not evidence, and recording one as evidence is
worse than recording none.

**Closed as hardening, not as a defect repair.** It removes a hazard and stops `setup()`'s line
ordering from being load-bearing. **Re-open** the moment a second bind site for `primary_plc` or
`vision_output` exists — Phase E's write acknowledgement and Phase F1's re-binding are the
candidates — because the hazard becomes real and testable then. That, not a bench run, is the
trigger.

**Status:** open, latent — found 2026-09-04 while tracing the combined-write defect.
Not reachable today; filed because the configuration that makes it reachable is
the one now running in the field.

`clearRoleContext()` (`src/model/localization_runtime_controller.cpp:709`, was cited as
`:568-580` when filed) tears down connections with

```cpp
disconnect(context.runner, nullptr, this, nullptr);
```

That removes **every** signal from that runner to the controller, not just the ones
`bindRoleContext()` made. One `ModbusTcpServerDevice` can legitimately fill both the
`primary_plc` and `vision_output` roles — that is the capability `PlcRunner` gained on
2026-09-01 (`src/runtime/plc_runner.h:70-76`), and the owner runs exactly that
configuration. Rebinding **either** role would therefore also drop
`m_plcValueConnection`, and the runtime would stop receiving all PLC signals with
nothing failing and nothing logged.

The camera path has the same shape: `bindActiveCameraRole()` → `bindRoleContext()` →
`clearRoleContext()` would drop `m_cameraGrabConnection`/`m_cameraCommandConnection`
if it ran mid-cycle. The `CycleState::Running` guard at the top of
`setActiveCameraNumber()` (`localization_runtime_controller.cpp:156`) is what prevents
it today.

**Why it is latent, not live.** Role binding happens in `setup()`, and a camera change
is refused while a cycle runs. So no current path rebinds a role whose runner also
carries a live connection this controller still needs.

**Status 2026-09-08: FIXED (Phase 9 Task A3), and unreachable — no test, automated or
manual, can distinguish the fixed build from the unfixed one.** The fix landed:
`RoleRecoveryContext` stores `statusConnection` / `errorConnection` and
`clearRoleContext()` (`localization_runtime_controller.cpp:756-767`) drops exactly those.
The reachability claim above was then traced properly, and it is stronger than "latent":

- `clearRoleContext()` has three call sites — `resetRuntimeBindings()` (`:635-637`),
  `bindRoleContext()` (`:693`), and `bindActiveCameraRole()` (`:670`).
- The `PrimaryPlc` and `VisionOutput` contexts are **bound in exactly one place**,
  `bindFixedRoleRunners()`, reached only from `setup()` at `:441` — which runs
  `resetRuntimeBindings()` first at `:399`, so both contexts are already empty and
  `clearRoleContext()` returns at `:759` without disconnecting anything.
- The only site that reaches `clearRoleContext()` with a **populated** context outside
  that reset is the Camera role, via `bindActiveCameraRole()`. A camera runner is a
  different `QObject` from the PLC runner that carries `m_plcValueConnection`, so the
  blanket form had nothing of the value stream to take.
- Inside `resetRuntimeBindings()` itself, `disconnect(m_plcValueConnection)` at `:626`
  runs **before** the three `clearRoleContext()` calls at `:635-637`. The value connection
  is already gone when the blanket form would have fired, and `setup()` remakes it.

So the dual-role Modbus binding does **not** make this reachable, contrary to what this
item claimed when filed. The fix is correct hardening — it removes a hazard and makes
`setup()`'s line ordering stop being load-bearing — but it is **fixed-by-reasoning, not
fixed-by-evidence**, and an owner-run check on the dual-role cell would pass either way.
A check that cannot fail is not evidence; do not record one as if it were.

**What the owner run on the dual-role cell is actually worth.** It is a *regression*
check on Task A3, not proof of item 53: after a camera change on the dual-role Modbus
binding, `bExecuteTrigger` still starts a cycle. It confirms the change broke nothing.
Recorded that way in Phase 9 Checkpoint A.

**What would turn this into evidence.** Only a second bind site for a fixed role — a path
that rebinds `primary_plc` or `vision_output` while the runtime is live (Phase E's write
acknowledgement and Phase F1's re-binding are the candidates). Re-open this item if such a
path is added, because the hazard becomes real the moment one exists. That, not a bench
test, is the trigger.


---

## 52. Modbus result layout has no settable 32-bit word order

**Status:** open — the unbuilt half of Open Question 2 in
`docs/history/plan/phase_8_implementation_plan.md`. Raised 2026-09-03 as a suspected
defect, resolved 2026-09-04 as a missing feature.

`ModbusResultLayout` packs each 32-bit axis value **high word first**
(`modbus_result_layout.cpp:40-55` and the caller at `:80-81`), hard-coded. A grep for
`wordOrder|byteOrder|endian|swap` across `src/device/plc/modbus` returns nothing.

**This is correct today and must not be changed.** The Modbus master on this path is
the **robot**, commissioned against exactly this order and verified working end to end
on 2026-09-04. Flipping it would break a running machine to satisfy a document.

**What is missing is the option.** The owner's recorded answer asked for an arg to set
the order, and it was never built. It matters for the next master, not this one — and
notably a **Mitsubishi** master would need the opposite: `MCRequest` packs 32-bit
values low word first (`mc_request.h:150-151`, and the comment says so), so the two
protocol families in this codebase already disagree about word order while only one of
them can express it.

**Why it is backlog rather than done.** Adding a parameter with exactly one caller and
no second implementation to shape it is the abstraction this project's rules warn
against — the shape would be guessed, not observed. Build it when a second master
appears; that master defines what the parameter needs to be.

**How to pick up.** Add the order to `ModbusConfig` (it is a Q_GADGET, so the widget
and JSON come for free), thread it into `encodeAxis()`/`decodeAxis()` and the payload
loop, and default it to the current high-word-first so existing projects reload
unchanged. `modbus_device_test` should then cover both orders round-tripping.


---

## 50. `PlcMitsuDeviceWizard` is dead code, compiled into both binaries

**Status:** open — found 2026-09-03 by the sweep prompted by the backlight-button
defect (Phase 8 Checkpoint C).

The whole class is unreachable, not just parts of it. `src/ui/forms/plc/plc_mitsu_device_wizard.{h,cpp,ui}`
is compiled and linked via `src/ui/ui.pri:29` (SOURCES), `:112` (HEADERS) and
`:196` (FORMS), but a grep for `PlcMitsuDeviceWizard` across `src/`, `app/` and
`runtime_app/` finds no construction anywhere — only its own files and translation
catalogue entries. Its harvest method `getWizardJson()` is commented out at
`plc_mitsu_device_wizard.cpp:18-20` and `.h:36`, and the `.ui` has an empty
`<connections/>` block, so there is no auto-connect escape hatch either.

**What replaced it.** `AddDeviceWizard`'s `pgMc` page —
`src/ui/forms/add_device_wizard.ui:473-493`, wired at
`add_device_wizard.cpp:293-311` and `:322-357`. That is the live Mitsubishi path.

**Why it matters, mildly.** Two data-entry fields on the dead form
(`ledit_ip_address` at `.ui:94`, `spb_port` at `:87`) are read by nothing. That
sounds alarming and is not: no operator can reach the form, so nothing is silently
lost. The real costs are ordinary — build time, translation strings in the `.ts`
for UI nobody sees, and a file that reads like the current Mitsubishi wizard to
anyone who opens it looking for one.

**How to pick up.** Delete the three files and their three `ui.pri` entries, then
re-run `update_translations.ps1` and expect its strings to vanish (they are the
"newly vanished" the sweep script normally warns about — here that is the desired
outcome, so record the count deliberately rather than letting it look like a
regression). Same category as item 30 (`MatchBoxGripper`).


---

## 51. `IDevice::errorOccurred` is never emitted by any device

**Status:** open — found 2026-09-03 by the same sweep. **Half resolved:** the runner forward
(Phase 9 / B1, below). The device half — no device emits the signal — still waits on the decision
under "How to pick up".

The signal is declared at `src/device/idevice.h:265`, documented as *"Emitted by
subclasses to report a device-level error"*, and the whole delivery path exists:
`CameraRunner` (`camera_runner.h:302-303`) and `PlcRunner` (`plc_runner.h:178-179`)
queue-connect the device's signal to their own, and
`LocalizationRuntimeController` (`localization_runtime_controller.cpp:543,551,559`)
routes it into `reportRoleError()` (`:1543-1557`). **No device subclass ever emits
it.** The only traffic on the channel is what the runners synthesise themselves —
`connectionFailed` re-emission and PLC write failures.

**The real consequence is narrower than it first looks**, which is why this is
backlog and not a defect. Device-internal failures do reach the operator: every one
of them logs `LOG_USER_ERR` (75 call sites under `src/device`), and grab failures
additionally fail the command (`camera_runner.h:394-403`) and abort the cycle with
`LocalizationFaultCode::CameraGrabTimeout`. What they do *not* get is the
**task-log** ERROR entry that `reportRoleError()` writes — so connection failures
and PLC write failures appear in a task's own log while a refused GenICam write does
not, and the two look equally serious in the app log. It is an inconsistency in
where errors surface, not a silence.

**Second, smaller item in the same area — RESOLVED 2026-09-08 (Phase 9 Task B1).**
`VisionOutputRunner::wireSignals()` omitted the `errorOccurred` forward that the other
two runners have. That is now added, with a comment at the line naming this item and
stating that nothing emits the signal yet, so its presence is never mistaken for
coverage. `test_every_runner_family_forwards_device_errors_to_the_controller` pins all
three families in one place and was red before the fix, so the next runner family
cannot omit it quietly.

**This does not close item 51**, and the forward is still dormant: the trap it removed
was that two of three runners would have delivered a device error and the third would
have dropped it silently. The decision below is untouched.

**How to pick up.** Decide the rule before emitting anything: either device-level
errors are worth a task-log entry (then emit from the devices — the forwards are now
all in place), or they are not (then delete the signal and its four connects rather
than leaving a wired channel nothing drives). Do not do half — a partially-emitted
signal is worse than either end state, because it makes the task log look complete
when it is not.

## 54. A held `bExecuteTrigger` fires one cycle when the runtime starts

**Status:** open — owner-gated as Phase 9 / **Phase G**, not started as of 2026-09-14. Phase 9 / Z2
held the Trigger section of `plc_signal_contract.md` back with a pointer here rather than assert either
behaviour. The title is itself an unverified claim: Phase G's first task exists to measure whether a
held trigger really fires on each PLC family before anything is changed.

Found during the Phase 8 / F5 audit of `LocalizationRuntimeController` against
`docs/domains/task_localization/plc_signal_contract.md`. Not a defect against the
letter of the contract, which is why it is here and not fixed: it is a judgment
call that belongs to the owner.

`setup()` forces `m_lastExecuteTrigger = false`
(`src/model/localization_runtime_controller.cpp`, in the state reset near the top).
If the robot is *holding* `bExecuteTrigger` high at the moment the runtime starts,
the first polled value delivered to `handlePlcValues()` reads as a rising edge
(`trigger && !m_lastExecuteTrigger`) and, once the roles connect and the task
re-arms, starts a cycle. The master never produced an edge.

The contract's Trigger section says a cycle starts only when "previous trigger
state was false, new trigger state is true". Initialising the previous state to
false satisfies that literally, so both readings are defensible:

- **Treat it as intended.** A held-high trigger is a pending request the cell
  wants serviced, and dropping it on a restart would silently lose work.
- **Treat it as a hazard.** Restarting the runtime while the robot holds the bit
  fires an unrequested cycle, and the robot cannot tell that cycle apart from one
  it asked for.

**How to pick up. This is a DOCUMENTATION item. Do not implement the fix this
entry originally proposed.**

> ⛔ **The originally proposed fix was wrong and would break every production
> session.** It said: *"seed `m_lastExecuteTrigger` from the first value observed
> rather than from a constant, so the first sample only establishes a baseline."*
> Corrected 2026-09-07 after a 25-agent survey flagged it.
>
> There is no harmless "first sample" to baseline against. The device layer
> already suppresses the connect-time snapshot — `valueChanged` is
> change-detected against a shadow seeded at connect, so **the first
> `bExecuteTrigger` the controller ever receives is already a genuine 0→1 written
> by the master.** Swallowing it to establish a baseline would consume the
> robot's first real trigger of every session: the cycle never starts, and the
> robot waits forever on a `bMatchingFinished` that cannot come.
>
> The entry was written convincingly enough to be implemented as-is. That is
> exactly why it is flagged here rather than quietly reworded.

Decide which reading the cell wants, then write it into the contract's Trigger
section either way, because today the document does not say. If a code change is
ever wanted, it needs a different mechanism than baseline-seeding and its own
analysis.

Note the related asymmetry while you are there: `m_lastErrorReset` is *not* reset
by `setup()` at all, so the two edge-detected inputs are initialised by different
rules for no stated reason. That part still stands.

## 55. `setup()` reports an unregistered active camera as a calibration problem

**CLOSED (2026-09-09) — Phase 9 Task C2, with the fault code finished by C7.**

`validateCameraNumber()` / `validatePatternGroupNumber()` now live on the controller and are
called by `setup()` **and** by both setters — one validator, three call sites, instead of gates
that existed only inside the setters. They return a three-way verdict
(`Accepted` / `OutOfRange` / `NotRegistered`), so the message names which of the two it was.

**Correction 3 of the audit is settled.** `MatchGroup`'s range is `[1, 32]`
(`match_group.cpp:12-13`), so group **0 is out of range**, matching what
`plc_signal_contract.md` already stated. No range change was needed; the contract was right and
the code simply never consulted it at startup.

**The fault code was the other half of this item, and C2 left it wrong.** An unregistered camera
published `CameraLost` (100) — the same wrong signpost as the calibration message, in the field
the PLC actually branches on. **Task C7** moved it to `CameraNotRegistered` (103).

**Tests:** `test_setup_refuses_an_unregistered_active_camera_distinctly_from_calibration` (the
exact confusion this item is named for), `test_setup_refuses_an_out_of_range_active_camera_with_a_range_message`,
`test_setup_refuses_an_out_of_range_active_pattern_group_with_a_range_message`,
`test_an_index_that_names_nothing_reports_not_registered_not_lost` (C7, the code).

**Field evidence** (`app_log_2026-09-08.txt`): a real `IR00000=2` write produced
*"Ready -> Faulted (No camera is registered for number 2.)"* (`:1450-1451`) — registration, not
calibration — and a group write produced *"Pattern group number 0 is outside the valid range
1..32."* (`:1495`), quoting `MatchGroup`'s own bounds. Both re-armed with no operator action
(`:1456`, `:1498`).

*(Retitled 2026-09-07. The old title said "does not range-check the active camera
/ pattern group number", which was too narrow - see point 2 below.)*

Same audit as item 54, and lower severity, because the bad case does fail - it
just fails with the wrong explanation.

> ⚠️ **This item is NOT the "task is Ready with index 0 at startup" defect.** That
> is **item 58**. Item 55's case ends **Faulted**; the owner observed **Ready**.
> They share a root - `setup()` applies no index gate - but closing this item
> would not change that behaviour at all. Verified by an adversarial pass on
> 2026-09-07, which also confirmed the mechanism below is correct and corrected
> the three points that follow.

**Corrections from that pass:**

1. **The central trace is confirmed**, including at runtime: a probe test with
   `context.activeCameraNumber = 0` returned `valid=false` with exactly one error,
   *"Active camera calibration is invalid."*, and no camera connect. `-1` returned
   `valid=true`. `bindActiveCameraRole(0)` calls `clearRoleContext()` **silently -
   no log, no error**, which is what lets the misleading message be the only
   signal.
2. **Scope is wider than "range".** `activeCameraNumber = 99` behaves identically
   to `0`: adopted verbatim, same lone calibration message. The gap is **any
   non-negative number naming no registered camera**, not specifically an
   out-of-range one.
3. **The pattern-group half behaves differently and was asserted without
   evidence.** Group 0 fails `validateActivePatternGroup()` with *"Active pattern
   group is missing."* - not misleading in the same way. Also **unresolved**:
   whether 0 is even out of range for a group. `MatchGroup::setIndexRange()`
   rejects only `min < 0`, while `plc_signal_contract.md` states 1..32. Settle
   that before writing a range check against it.
4. **The "reachable mainly by a hand-built context" note attributes the guarantee
   to the wrong place.** `TaskDeviceBindings::setCameraNumberMap()` performs **no**
   range check; the invariant is held by `fromJson` on load and by the `num > 0`
   filter in `camera_mapping_widget.cpp:465`.

`setup()` adopts `context.activeCameraNumber` and
`context.activePatternGroupNumber` with no range check; only a negative value is
special (it means "first available"). Both PLC-driven setters now check the range
first and say so plainly, but the setup path does not. A context arriving with
camera number 0 binds no camera role, then fails
`validateActiveCameraCalibration()` and reports **"Active camera calibration is
invalid"** - which sends whoever reads it to the calibration data rather than to
the number.

In practice the saved project's bindings are already validated on load
(`TaskDeviceBinding::fromJson` rejects a binding outside 1..16), so this is
reachable mainly by a caller building a `RuntimeContext` by hand. Worth closing
for the message alone.

**How to pick up.** Range-check both numbers in `setup()` using the same bounds
the setters use (`TaskDeviceBinding::kMinCameraNumber`/`kMaxCameraNumber` and
`mtc::MatchGroup::validateIndexRange()`), and append an error naming the number.
Keep the negative sentinel working: `-1` means "first available" and must stay
that way.

## 56. MC 1C/3C: read/write command coverage on the real PLC, and the benchmark numbers

Carried out of **Checkpoint A** when Phase 8 closed (2026-09-07), at the owner's
direction. Not a known defect - an untested surface, and the checkpoint text
explains why that distinction matters here.

**Phase 9 update (2026-09-09, recorded 2026-09-14).** Two parts of this are no longer absent:

- **Write completion on the real C24 is proven.** The owner pulled the cable mid-write and got a
  failure completion rather than silence (Phase 9 / E1 owner-run).
- **A device-level MC harness exists.** E1's `McProtocolDevice::createMsgInterface()` seam and
  `mc_frame_test`'s `FakeMcPort` compile and drive the whole device, so the helper-only proofs for
  59.3 / 59.4 can become device-level tests. None has been written.

What remains is the rest of this entry as written: the 1C/3C read/write command set on a real module,
and `tools/mc_protocol_bench` numbers for all three frames.

**What is proven.** 3E still works with no regression, and 1C and 3C both connect
to a real C24 module and read. Owner-confirmed 2026-09-03. The regression risk the
checkpoint was built around is retired: the C-frames did not break the shipping 3E
path.

**What is not.** The remaining read/write commands on 1C and 3C. Owner: *"chua test
het cac lenh doc ghi ma protocol co"*. **Write commands matter most** and are the
ones a bench run defaults to off.

This is exactly the class `mc_frame_test` cannot reach. That suite proves a codec is
byte-correct against reference frames; it cannot prove a real C24 accepts the frame.
Sum-check, station number, PC number and access-route handling can all differ **per
command**, so a codec that passes every reference case can still be refused on the
wire for one command and not another. Phase 7 closed with five defects that compiled
clean and passed the suite. The C-frames are that same class of work.

**Also carried:** benchmark numbers for all three frames. `tools/mc_protocol_bench`
(Phase 8 Task A8) is built and smoke-tested but **has never been pointed at the
PLC**, so there is no measured latency figure for 3E, 1C or 3C.

**How to pick up.** With the C24 module available, run each read and each write
command the protocol defines, on 1C and on 3C, with 3E as the control. Then run
`tools/mc_protocol_bench` against the same PLC for all three frames and record the
numbers. Add a `mc_frame_test` case for any frame a real module refuses, using the
bytes the module rejected rather than a self-generated expectation.

## 57. Robot pick check verified active on the dual-role Modbus binding

**CLOSED 2026-09-14 — as a missing feature, not as a verification.** Filed as "verify the check is
active", it could never pass as filed: the runtime took the settings from the output device, a Modbus
device carries none, so there was nothing active to observe. Phase 9 / F1 built the missing piece — the
pick check became a task setting, `TaskLocalizeConfig::robotCheckConfig()` — and the owner then
observed it reject unreachable poses on the dual-role Modbus binding. Evidence at the end of this entry.

Carried out of **Checkpoint B** when Phase 8 closed (2026-09-07). The owner
confirmed Modbus works on both the server and the client role and approved the
result layout; this one item was not part of that confirmation, and it needs a
different kind of evidence than "the cell ran".

**The hazard** is Phase 8 Task B1's: `buildRuntimeContext()` reads
`robotCheckConfig` by casting the bound device's config to `VisionOutputDeviceCfg`.
That cast fails for a Modbus device and the kinematic-check settings **silently
default**.

**"The robot picked correctly" is not evidence**, which is the whole reason this
stayed open through a successful real-robot run on 2026-09-03. A defaulted-off check
produces a *working* robot: the cell runs, parts get picked, and nothing looks wrong
until the one cycle the check was supposed to stop.

**How to pick up.** Observe the check *doing something* on the dual-role Modbus
binding specifically - one run with a deliberately unreachable pick that the check
must reject, or the check's own settings read back out of the live task. Either
proves the cast produced real settings rather than defaults.

> **The HAZARD is removed as of 2026-09-10 (Phase 9 / F1). The item stays OPEN, because what it
> asks for is an observation nobody has made yet.**
>
> The cast this entry describes was already replaced by `IResultOutputDevice` in Phase 8 / B1,
> which fixed *which families could answer* but not the deeper problem: a device with nothing
> commissioned answers "disabled", and that is indistinguishable from "this cell does not want the
> check". F1 moved the setting onto the task — `TaskLocalizeConfig::robotCheckConfig()`,
> schema v3 → v4 — so `buildRuntimeContext()` no longer asks a device at all. The setting is
> edited from **Settings tab → Robot pick check → Set…**, which is also new: before F1 it lived
> only on the vision-output device panels, where a dual-role Modbus cell had no way to reach it.
>
> F1 also added the guard this entry's failure mode needed: an **enabled** check whose preset does
> not resolve now **refuses the runtime and names the preset**, instead of building a checker that
> fails closed and reads on the dashboard as *"nothing is pickable today"*.
>
> **Still not evidence, and that is the point of leaving this open.** *"The robot picked
> correctly"* was never evidence, and neither is *"the tests are green"*. What closes this is the
> owner observing the check **reject a deliberately unreachable pick** on the dual-role Modbus
> binding, commissioned from the task settings. Recorded at Checkpoint F.
>
> **Owner run 2026-09-11 — confirmed, but not on the binding this item names.** Unreachable poses
> were skipped with the check commissioned from the task settings. Every runtime after the F1 build
> ran on `PLC_Mitsu_01` (MC) + `VisionOut_01` (app logs 09-10 from 11:21, 09-11 from 16:48); no
> Modbus device carried `vision_output` in any of them. So the task → checker path is proven on
> hardware, and since `buildRuntimeContext()` no longer asks any device, the binding family can no
> longer change what the checker receives. The PLC-carries-output case is pinned by
> `test_runtime_reads_the_pick_check_from_the_task_when_a_plc_carries_the_output_role`. What stays
> unobserved is literally this item's case; it is kept open for that, and closing it on the
> residual risk is the owner's call.

**CLOSED 2026-09-14 — the observation this item was filed for has now been made.** The owner
re-ran with a **Modbus TCP client carrying both roles at once** and confirmed unreachable poses
are skipped. `app_log_2026-09-14.txt` corroborates the binding independently: at `:208-209` the
runtime requests `role= primary_plc deviceId= 05` and `role= vision_output deviceId= 05` — the
same device (`Modbus_Client_1`, `192.168.1.59:801`) — and 85 cycles then ran against it
(08:48:35–08:51:57). That is exactly the configuration this entry describes, with the check
commissioned from the **task** settings rather than a device panel.

Evidence chain, for the record: the hazard was removed by Phase 9 / F1 (the pick check moved to
`TaskLocalizeConfig::robotCheckConfig()`, schema v3 → v4), the PLC-carries-output path is pinned
by `test_runtime_reads_the_pick_check_from_the_task_when_a_plc_carries_the_output_role`, and the
behaviour is now observed on real hardware on the exact binding. *"The robot picked correctly"*
was never evidence — a rejected unreachable pose is.

> ⚠️ **That same run exposed a separate defect on this binding** — the result publish colliding
> with the poll and aborting the cycle. It does not reopen this item; see the dual-role Modbus
> collision entry at the end of this file.

## 58. Startup neither validates nor announces the active camera / pattern group

**PART A CLOSED (2026-09-09) — Phase 9 Tasks C2 + C3 + C6. Part B partly delivered; see below.**

The reported behaviour is gone, confirmed by the owner on the cell **2026-09-09**: a runtime
started with the master holding camera 0 and pattern 0 **faults instead of going Ready**, and
correcting both registers returns it to Ready with **no restart and no operator action**.

| Piece | Task | What it does |
|---|---|---|
| Startup validates | **C2** | `setup()` runs the same validator the setters run |
| Startup announces | **C3** | new `nActiveCameraStatus` / `nActivePatternGroupStatus`; the echo onto the master's **command** registers is deleted, not moved |
| Startup **reads** the live selection | **C6** | `PlcValueMap::valueForTag()` + a bounded wait on `PlcRunner::pollingUpdate`, so `setup()` sees what the master is actually holding |

**The design caution in "How to pick up" was followed, not overridden.** Part A's second bullet
suggested publishing onto `nActiveCamera` / `nActivePatternGroup`. That would have been wrong, and
the field proved it before C3 shipped: on a Modbus **client** binding the controller's echo was
rejected outright — a client cannot write an input register (item 60). C3 added separate status
signals instead. `main.cpp:3069`, which asserted the omission, was updated deliberately.

**Tests:** `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready`,
`test_a_runtime_started_with_the_master_holding_a_valid_number_adopts_it`,
`test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal`,
`test_setup_announces_the_active_selection_on_status_signals`,
`test_status_signals_with_no_tag_emit_but_do_not_write`,
`test_build_runtime_context_leaves_the_selection_for_setup_to_resolve`,
`test_setup_falls_back_to_the_project_default_when_the_index_signal_is_unmapped`.

> ⚠️ **The first cut of C6 refused the bad index correctly and then could never re-arm** — it
> routed the refusal through `SetupResult::errors`, which leaves `m_valid` false, and
> `markRuntimeReady()` gates on `m_valid`. The owner found it on the cell; the task's own two
> tests had asserted the wrong contract (permanent fault) and went red at the fix. The lesson is
> recorded in the plan at Task C6: a 0 **written at runtime** is a recoverable fault, a 0 **read at
> setup** was made a terminal one, and "does not reach Ready" was tested while "recovers" was not.

**Part B — what is delivered and what is still open.** B asked whether the runtime should learn
the true power-up state at all. **C6 answered yes and built the mechanism**: the `pollingUpdate`
full-snapshot hook now has a real consumer, and `PlcValueMap` gained the name→value accessor B
identified as the missing piece. What B raised and **remains open** is narrower:

- Whether a **mapped-but-never-written** register should be a distinct commissioning fault. Today
  an absent tag and an unreadable snapshot both fall back to the project default with a USER-level
  warning; they are not separated from a register the master genuinely set.
- The **MC** and **Modbus client** first-poll adoption (`mc_protocol_device.cpp:424`, `:476`;
  `modbus_tcp_client_device.cpp:275`), which swallows even a *non-zero* power-up value into the
  shadow. C6 reads the snapshot rather than the change stream, so the index path is no longer
  exposed to this — but every **other** signal still is. This is the half that carries into
  Phase G / item 54.

  > ⚠️ **That sentence was written before it was true, and the gap cost a field defect.** "C6 reads
  > the snapshot, so the index path is safe" quietly assumed the snapshot was trustworthy. On MC it
  > was not: it was published one response early, and the very first one carried nothing but the
  > zero-fill — **item 59.5**, owner-reported 2026-09-09, fixed by Phase 9 Task **E5**. The claim
  > holds now; it did not when it was filed. And "source-established, not observed on hardware" was
  > carrying more weight than it looked: what had not been observed was not a minor variant of this
  > paragraph, it was the thing that broke the cell.

**Owner-reported 2026-09-07, on real hardware:** *"khi runtime khoi dong mac du
camera number va pattern number deu la 0 nhung task van ready."* Scheduled by the
owner into the **Phase 9** plan; this entry exists to record the mechanism
accurately, not to prescribe the fix.

**Do not read item 55 as covering this.** That item is about a `RuntimeContext`
arriving with camera 0, and its case ends **Faulted**. This one ends **Ready**.
They share a root - `setup()` applies no index gate - but they are different
defects, and closing 55 would not change the reported behaviour at all.

### Mechanism (verified by a 13-agent adversarial pass, 2026-09-07)

Two independent facts, both required:

**1. The startup path bypasses both setters entirely.** `beginRuntime()`
(`task_localization.cpp:172`) -> `setupTask()` -> `setupRuntimeController()` ->
`controller->setup(context)`. Nothing on that path calls
`setActiveCameraNumber()` or `setActivePatternGroupNumber()`. The context is
filled **app-side from the project bindings, never from the device**:
`context.activeCameraNumber = cameraDeviceIds.firstKey()` and
`activePatternGroupNumber = patternGroups.firstKey()`
(`task_localization.cpp:724-738`). `setup()` then assigns both as raw members
(`localization_runtime_controller.cpp:423-431`), binds the camera role directly
via `bindActiveCameraRole()`, clears both rejection latches (`:376-377`), and
calls `markRuntimeReady()`.

Every range and registration gate - `kMinCameraNumber`/`kMaxCameraNumber`
(`:170`), the runner lookup (`:192`), `MatchGroup::validateIndexRange()` (`:277`)
- lives **only inside the setters**. The setters have exactly two production
entry points: `handlePlcValues()` (change-driven) and the queued UI wrappers,
whose only callers are **dead** (see item 59). So an index that merely *sits* in
a register is never inspected; only one that *arrives as a change* is.

**2. A register that is 0 from power-up and never written is never delivered.**
`ModbusRegisterMap::takeChangedValues()` skips on `!differs`
(`modbus_register_map.cpp:229-245`); `configure()` seeds every address to 0 and
`deviceConnect()` adopts that all-zero space as the shadow
(`modbus_tcp_server_device.cpp:335`). 0-vs-0 is not a diff, and the server has no
poll timer.

> **Do not over-generalise this to "zeros are never published".** A master
> writing 0 **over** a non-zero value *is* a change and *is* delivered - the log
> shows `HR00000=0` published and the task faulting 6 ms later with *"Camera
> number 0 is outside the valid range 1..16."* (`app_log_2026-09-07.txt`,
> 14:23:15.770 -> .776). `VirtualPlcDevice::injectInputValue()` publishes
> unconditionally with no shadow at all. The load-bearing case is narrow: **0
> from power-up, never written.** An adversarial verifier refuted a broader
> phrasing of this claim, correctly.

**Runtime proof.** In `app_log_2026-09-07.txt`: at 13:34:20 the master left
`HR00000=0` and wrote nothing more; `beginRuntime` ran at 13:37:57 and the task
logged `RuntimeStarting -> Ready (Runtime ready.)` at 13:37:59 with no fault; the
master wrote `HR00000=1` only 18 s later. At the 13:29:40 process start, **a full
successful matching cycle ran at 13:30:19-13:30:21 while the selection register
still read 0.** Every Ready transition in that log carries setup's own reason
`"Runtime ready."`, never a setter's `"Active camera changed. Runtime ready."`.

**Aggravating factor - the runtime never announces its own choice.**
`nActiveCamera` has exactly one publish site
(`localization_runtime_controller.cpp:228`, inside the camera setter) and
`nActivePatternGroup` one (`:309`, further gated on `valid`).
`publishInitialReadyOutputs()` (`:837-850`) publishes ten signals and neither
index. So the register keeps its power-up value and the signal-monitor row keeps
the literal `"0"` it was constructed with. **"Nobody wrote anything" and "0 was
commanded" are indistinguishable on that row** - which is exactly what the owner
saw. (The two dashboard labels do start at an em-dash, so they distinguish it.)

### Why it matters

A commissioning engineer sees `bTaskReady`, `bCameraValid` and `bPatternValid`
all true while the selection registers read 0, and **the runtime will accept and
execute a trigger in that state**. The recipe actually used is `firstKey()` of the
binding map - whichever camera and group happen to sort first. If the master's
intended selection is anything else, the part is inspected with the wrong camera
or pattern and **nothing anywhere reports a mismatch**.

Self-correction depends entirely on the master eventually writing a value that
*differs* from what the device last saw. A master that writes its selection once
at power-up - before the vision app starts, or with a value equal to the shadow -
never triggers a setter, and the mismatch is permanent for the session.

### How to pick up

Two separable pieces; **A alone removes the operational risk.**

**A. Make startup validate and announce its own selection.**
- `localization_runtime_controller.cpp:423-431` - route the startup selection
  through the same gates the setters use. Extracting the range/registration check
  into a shared validator that `setup()` also calls is the obvious shape. Keep
  `-1` = "first available" working; note `buildRuntimeContext()` already resolves
  that itself, so setup's own fallback is dead for the only production caller.
- `localization_runtime_controller.cpp:837-850` - publish `nActiveCamera` and
  `nActivePatternGroup` on the ready path. **Design caution:** these are
  master-written *command* registers, and the comment at `:299-303` records a
  deliberate decision not to echo a value onto them. A task that echoes at
  startup can fight a master about to write its own; a separate status register
  may be the right shape. **That decision belongs in the Phase 9 plan, not in the
  fix.**
- `tests/architecture_contract_test/main.cpp:3069` currently **asserts the
  omission** (`nActiveCamera` never emitted after `setup()`). It will fail and
  must be updated deliberately, not patched around.

**B. Decide whether the runtime should learn the true power-up state at all.**
The full snapshot already exists - `pollingUpdate(m_map.clone())`
(`modbus_tcp_server_device.cpp:469`, `mc_protocol_device.cpp:479`) - but the
controller connects only `valueChanged`, and `pollingUpdate` has no consumer
outside two device widgets. That is the hook if a mapped-but-unwritten register is
to become a commissioning fault. Weigh it against the Modbus **client** and **MC**
first-poll behaviour (`modbus_tcp_client_device.cpp:275`;
`mc_protocol_device.cpp:424`, `:476`), which silently adopt the first poll into
the shadow - MC swallows even a *non-zero* power-up value. That is a second,
independent way a startup value goes unseen. **Source-established, not observed on
hardware.**

**Docs already corrected.** `plc_signal_contract.md` claimed a power-up 0 "will
hold the task in fault - which is intended". That was false; the paragraph now
says what the code does and points here. Restore the original wording only when
the code actually does it.

## 59. Housekeeping surfaced by the item-58 investigation

**CLOSED (2026-09-09) — all four resolved.** 59.4 was found while planning Phase 9 and is filed
here for the first time (the plan referred to it as "59.3b"); it turned out to be the most severe
item in this group, not housekeeping at all.

| # | Closed by | Tests |
|---|---|---|
| 59.1 dead slots | **C5** — all four deleted | *see note* |
| 59.2 `activeCameraWorkspace` never populated at startup | **A2** | `test_setup_applies_the_active_camera_workspace_before_the_first_cycle`, `test_a_camera_with_no_workspace_keeps_the_default` |
| 59.3 lockstep `std::map` walk | **A1** | `test_a_shadow_larger_than_the_live_map_still_compares_by_address`, `test_an_address_new_to_the_shadow_is_reported_without_a_previous_value` |
| 59.4 MC drops every M change when no D range is configured | **A1**, same edit | `test_m_changes_survive_a_station_with_no_d_ranges`, `test_d_changes_survive_a_station_with_no_m_ranges` |

**59.1 — the evidence is a deletion, so say what it is.** No test asserts the absence of a dead
slot. What is checkable: the four identifiers are gone from `src/`, and the 2026-09-09 translation
sweep moved **exactly four** strings to `vanished` — *"Cannot change camera, not found camera
number %1"*, *"Cannot change camera, device %1 with id %2 isn't camera type"*, *"Task %1 change
camera number failed, value %2"*, *"Task %1 change pattern number failed, value %2"* — one per
deleted function, and nothing else. Their Japanese is retained; `-no-obsolete` was not used.
`task_localization.h` carries a comment at the deletion site so the removal reads as deliberate.

**59.2 was not housekeeping either.** With `activeCameraWorkspace` unset for the whole session,
`useConditionWorkspace` stayed false, `outSideConditionRoiCheck()` was never called, and every
object kept `m_isOutsideConditionRoi == false` — so objects the commissioning engineer had fenced
out were converted to robot coordinates and **sent as pick targets**, on the first cycle of every
session, with nothing logged and every lamp green. `setup()` now calls
`applyActiveCameraWorkspace()` directly. Field-confirmed: `app_log_2026-09-08.txt:1432` shows
`workspace: crop=on condition=on conditionRoi=(571.034,302.069 546.207x459.31)` at startup, and the
owner confirmed on the cell that an object outside the condition ROI is skipped.

**59.4 (new).** `McProtocolDevice::check_device_changed()` returned at `mc_protocol_device.cpp:721-723`
— **before** the `emit valueChanged` — whenever the station had no D ranges configured. On an
M-only PLC that drops every `bExecuteTrigger` while the device widget looks perfectly alive.
`deviceMChanged` still fired, which is what made it invisible. A1 extracted the diff into the
header-only `src/device/plc/mc_device_map_diff.h` (iterating **by key**, which is what fixes 59.3
in the same edit) and made `check_device_changed()` emit once, unconditionally, after both maps.

> **Still unverified on hardware.** `mc_frame_test` does not compile `mc_protocol_device.cpp` —
> a device-level test would need an `McMsgInterface` seam that does not exist — so 59.3 and 59.4
> are proven at the **helper** level, not at the device level. Item 56 (MC 1C/3C coverage) carries
> the remaining gap.
>
> **Update 2026-09-09:** the seam now exists (Phase 9 / E1's `createMsgInterface()`), and
> `mc_frame_test` compiles the device. A device-level test for 59.3/59.4 is therefore possible
> today; it has not been written. **Part of item 56 is closed** by the same phase: the owner
> confirmed on the real C24 that pulling the cable mid-write produces a failure completion rather
> than silence (Phase 9 / E1 owner-run). Write-command coverage on real hardware is no longer
> absent — what remains of 56 is the 1C/3C frames, which are still bench-only.

Found 2026-09-07 while tracing the startup index path. None is the item-58 defect;
all were noticed on the way and are recorded rather than fixed, per the rule
against mixing unrelated work into a change.

1. **Dead slots.** `TaskLocalization::onSignalChangeCameraNumber()` and
   `onSignalChangePatternNumber()` (`task_localization.cpp:471-497`, declared
   `task_localization.h:201`, `:205`) have **no `connect()` site anywhere** in
   `src\` or `runtime_app\`. They are the queued UI path into the active-index
   setters. Wire them or delete them - a dead slot that looks like a live entry
   point is how item 58's "the setters have two entry points" reads as safer than
   it is. *(Searched by identifier; a string-based or `QMetaObject`-resolved
   connection would not have been caught.)*
2. **`m_context.activeCameraWorkspace` is never populated at startup.** It is
   assigned only inside `setActiveCameraNumber()`
   (`localization_runtime_controller.cpp:252-254`) and `buildRuntimeContext()`
   does not fill it, so it stays at its default through a normal runtime start.
   Consequences not traced - do that before deciding whether it matters.
3. **`McProtocolDevice::check_device_changed()` walks two `std::map`s in
   lockstep** assuming identical key sets, while `update_last_*_map()` only adds
   and never erases. If the shadow outgrows the live map the iterators misalign.
   Latent - no reaching case was constructed.
4. **The same function discards every M change when the station has no D ranges
   configured.** *(Filed 2026-09-09; found 2026-09-08 while planning Phase 9, where
   it was referred to as "59.3b". Not latent — live on any M-only PLC.)* The early
   return at `mc_protocol_device.cpp:721-723` sits **before** the single
   `emit valueChanged` at `:743-745`, so an M-only station publishes nothing at all
   while `deviceMChanged` at `:714` keeps firing and the device widget keeps looking
   alive. On such a station every `bExecuteTrigger` is dropped and the task simply
   never runs a cycle, with no fault and no log line.
5. **The polling snapshot was published one response too early.** *(Filed 2026-09-09,
   owner-reported on an MC cell; **FIXED the same day by Phase 9 Task E5**.)*
   `polling_query()` emitted `pollingUpdate()` at the moment the round's **last**
   request was *selected* — before `request_handle()` sent it, let alone parsed its
   reply. Every snapshot was therefore one response short, and the **first** one
   carried the zero-fill `update_d_map()` had just written. Task C6 reads that snapshot
   to resolve the startup selection, so `nActiveCamera` came back as **0** and the
   runtime faulted with `CameraNotRegistered` on a project whose camera 1 was bound and
   registered — then re-armed itself a moment later, when the real value finally
   arrived as a change. `is_first_time_polling` was mis-placed identically and moved
   with it: on an **M-only** station (see 59.4) that flag's early clear left the *whole*
   first poll un-suppressed, so a `bExecuteTrigger` held high at power-up would have
   started a cycle nobody triggered. Both now clear at the tail of `response_handle()`,
   giving MC the contract both Modbus families already keep — **a snapshot means what
   was just read.**

---

## 60. The active-index echo is rejected outright on a Modbus master

**CLOSED (2026-09-16, closing note added at the Phase 10 triage).** The fix this item was
waiting for landed with Phase 9 Task C3 on 2026-09-09: both command-register echoes were deleted
and the adopted selection is published on `nActiveCameraStatus` / `nActivePatternGroupStatus`
instead (decision record `DR-0001`, tests named in item 58). The status line below was written
before C3 landed and is kept for traceability. The one loose end it mentions — a device error on a
dual-role binding being reported once per role — is cosmetic and is not tracked separately.

**Status:** open, **field-confirmed 2026-09-08**. Fixed by Phase 9 Task C3, which
already exists and already says to delete both echoes; this item records that the
defect is no longer theoretical and raises its severity.

`setActiveCameraNumber()` publishes the accepted number straight back onto the tag it
was read from (`localization_runtime_controller.cpp:228`), and
`setActivePatternGroupNumber()` does the same at `:308`. `LocalizationSignalMapper`
holds one **undirected** map (`m_signalNameToTag`), so a signal the master owns as a
command register is writable by the runtime with no direction check anywhere in the
path.

On a Modbus **client** (master) binding whose command registers sit in the input-register
area, the write cannot succeed — a master may not write input registers — so every
accepted selection change produces a device-level rejection and a `runtimeError`:

```text
build\bin\release\logs\app_log_2026-09-08.txt:739-752
[11:12:19.411] Modbus values changed. deviceId= 04 count= 1 values= IR00000=1
[11:12:19.483] Modbus values changed. deviceId= 05 count= 1 values= IR00000=1
[11:12:19.485][ERR]  Modbus word write rejected: a master cannot write input registers.
                     deviceId= 05 tag= IR00000
[11:12:19.488][WARN] primary_plc   runtime error: PLC word write failed: IR00000
[11:12:19.488][WARN] vision_output runtime error: PLC word write failed: IR00000
```

and identically for `IR00001` (the pattern group) at `:748-752`.

**The client is where it is loud, not where it is worst.** The same echo on a **server**
binding writes the same value back into a register the master owns, silently and
successfully. That is a write race, not a no-op: a master that changes the command
register while the runtime is echoing the previous value has its command overwritten by
the vision system. The rejection on the master binding is the benign version of this bug.

**Noticed alongside, not separately filed:** the error is reported **twice**, once per
role, because one device fills both `primary_plc` and `vision_output` and each role
context forwards the same device error. Cosmetic, but it doubles the noise in exactly the
configuration that is hardest to read.

**How to pick up.** Phase 9 Task C3 — publish `nActiveCameraStatus` /
`nActivePatternGroupStatus` instead, delete both echoes. See the migration note there: a
commissioned master reading the command registers back must be moved onto the status tags
first.

---

## 61. The dashboard never re-reads the task config, so its bindings go stale

**Status:** **CLOSED 2026-09-11 — Phase 9 / F2, owner-confirmed on the cell.** With the dashboard
already open, changing the vision-output and primary-PLC bindings in the Setting tab updated the
dashboard without reopening the window — the check that distinguishes this cause from item 25's.
Field-confirmed 2026-09-08.
Phase 9 Task F2 covers a *different* cause of the same symptom and would **not** have fixed
this one on its own — see below; both were fixed in the same edit.

> **What landed.** `initWidget()` now connects `ITask::configChanged` to a handler that
> re-reads `taskLocalizeConfig()` into `m_config` and runs the existing
> `pushSignalTagsFromConfig()` + `updateTaskContext()` + `rebuildConnectionWiring()` trio —
> exactly the "how to pick up" below. It is idempotent: `rebuildConnectionWiring()` already
> drops `m_connectionConns` first.
>
> **This cannot close on a test.** There is no widget-level test framework in this project, so
> the only check that distinguishes the two causes is the owner one: with the dashboard already
> open, change the primary-PLC or vision-output binding in the Setting tab and return **without
> closing the window** — the device label and the lamp must both follow. The re-wiring half
> passes that while still showing the wrong device, which is why it is the discriminating check.
> Recorded at Checkpoint F.
>
> **Not fixed here, and still open:** the camera lamp's refusal-path gap noted below —
> `setActiveCameraNumber()` returns at `:189` and `:215` before publishing the index, so a
> refused selection updates no camera visual. That is a separate defect in a different file and
> was left alone rather than folded in.

`LocalizationDashboardWidget` takes a **copy** of the config at construction
(`localization_dashboard_widget.cpp:180`) and refreshes it in exactly one place: the
`ITask::devicesChanged` handler at `:209-215`. But `devicesChanged` is emitted only when a
device is assigned to or unassigned from the task (`itask.h:158`, `:168`) — **not** when
the role bindings change. Editing "primary PLC" or "vision output" in the Setting tab goes
`localization_setting_widget.cpp:202/210` → `:545 setTaskLocalizeConfig()` →
`task_localization.cpp:101` → `ITask::setTaskConfig()` → **`emit configChanged()`**
(`itask.h:210`) — and **nothing in the dashboard is connected to `configChanged`**.

In the editor shell the dashboard is built once and cached
(`localization_task_widget.cpp:619-625`), so the stale copy survives for the life of the
window. Everything read through `m_config.d->m_deviceBindings` is then wrong:

- `updateTaskContext()` (`:421-422`) — the PLC and vision-output **device name labels**.
- `rebuildConnectionWiring()` (`:290-292`) — which device each **connection lamp** watches.
- `resolveActiveCameraDeviceId()` (`:365-373`) — the camera lamp's device, via a stale
  `cameraNumberMap()`.

Observed on 2026-09-08: with a Modbus client bound as PLC and output, the link came up
(`app_log_2026-09-08.txt:705` *"Modbus client connected. deviceId= 05"*) and the dashboard
showed neither device. The camera lamp escapes only by accident — `nActiveCamera`'s
handler calls `rebuildConnectionWiring()` (`:489-495`) — and loses even that on the
refusal paths, which return before publishing the index (`:189`, `:215`), so a refused
camera selection updates no camera visual at all.

**Why Task F2 is not enough.** F2 attributes the dead lamps to construction order — the
runtime-shell dashboard is built before `beginRuntime()`, so `runnerFor(id)` returns null
once and is never retried. That is real and separate. But re-wiring on `runtimeStarted` /
`phaseChanged` still reads the same stale `m_config`, so a role bound after the dashboard
was first shown stays invisible. **Both causes must be fixed for either symptom to go
away.** F2 has been amended to carry this.

**How to pick up.** Connect `ITask::configChanged` to a handler that re-reads
`taskLocalizeConfig()` and then runs the existing `pushSignalTagsFromConfig()` +
`updateTaskContext()` + `rebuildConnectionWiring()` trio — the same three the
`devicesChanged` handler already runs. Keep it idempotent; `rebuildConnectionWiring()`
already drops `m_connectionConns` first.

---

## 62. A stale root `main.moc` silently deletes tests from the two vision suites

**Status:** open, **root cause found and proven 2026-09-08**. Worked around in the local
build dir; the durable fix is not yet made.

**Symptom.** `tests/vision_output_device_test/main.cpp` defines 7 test functions and
`tests/vision_tcpip_client_device_test/main.cpp` defines 8. The built binaries registered
**6** and **7**. `test_disconnect_notice_on_graceful_close`
(`vision_output_device_test/main.cpp:217`, `vision_tcpip_client_device_test/main.cpp:231`)
was missing from both. It did not fail and it was not skipped — QTest reported
`0 skipped, 0 blacklisted`, so **the suite looked cleanly green while carrying a hole**.
Confirm the shape of any suite with `<exe> -functions` before trusting a total.

**Mechanism — proven, and it is self-perpetuating.** Each `main.cpp` ends with
`#include "main.moc"`, and qmake **resolves that include against the include path when it
generates the Makefile**. Both build roots still held a `main.moc` dated **2026-06-03**,
left behind when these two suites were built with output at the build root; qmake now
emits `release\main.moc`. Because the build-dir root is on the search path, qmake's
dependency scanner found the *physical June file* and wrote **that** path into the
Makefile. Comparing the generated Makefiles shows it exactly:

```text
mc_frame_test\Makefile.Release:3157      release\main.moc \      ← correct
vision_output_device_test\...:2209       main.moc \              ← the stale root file
vision_output_device_test\...:11893      main.moc \
```

The first of those is inside the prerequisites of the `release\main.moc` rule itself — the
rule that *produces* the moc was told to depend on the stale copy of its own output.

So the file's mere existence makes qmake depend on it, which keeps it required, which
keeps it present. At compile time the same include order (`-I<srcdir> -I. … -Irelease`)
makes the compiler read it too, and the metaobject ends up listing the June set.

**Why the obvious repairs fail.** Regenerating the moc does not help — `nmake -f
Makefile.Release compiler_moc_source_make_all` writes `release\main.moc` and the root copy
still wins. Deleting the root copy *without* re-running qmake fails differently: the
existing Makefile still names the bare path and nmake stops with `U1073: don't know how to
make 'main.moc'`. Both were tried; both are dead ends.

**The fix — delete first, then re-run qmake. Order is the whole trick.**

```powershell
# per affected build dir, e.g. build\tests\vision_output_device_test
Remove-Item .\main.moc                       # the stale root copy
qmake <path-to>\<suite>.pro                  # regenerates deps against release\main.moc
Remove-Item .\release\main.obj               # no moc dependency existed, so force it once
nmake -f Makefile.Release
```

**Applied and verified 2026-09-08 in both build dirs.** After the delete + qmake, every
`main.moc` reference in `Makefile.Release` reads `release\main.moc` and the bare-path
prerequisites are gone. Forced full recompiles then produced binaries registering **7** and
**8** test functions, matching the sources, and the root `main.moc` does not come back.

This is a **build-directory** repair, not a source change: the two `.pro` files are
structurally identical to `mc_frame_test.pro`, which was never affected because its build
root never held a `main.moc`. Nothing in `tests/` needs editing. A clean build dir would
have had the same effect; the sequence above avoids a full rebuild of RobotKinematics.

---

## 63. Fault code 400 now claims "not registered" for four content-invalid pattern faults

**Status:** open, **introduced deliberately by Phase 9 Task C7 (2026-09-09)** and recorded
in the same change rather than left to be discovered.

**What happened.** C7 renamed `PatternInvalid` → `PatternNotRegistered` (value 400
unchanged) so the pattern half matched the new `CameraNotRegistered = 103`. For the
*selection* faults that is exactly right. But 400 is also published by four paths where the
group **is** registered and its content is the problem:

| Site | Condition |
| --- | --- |
| `localization_runtime_controller.cpp` `setActivePatternGroupNumber()` | group in range and present, but `validateActivePatternGroup()` finds no usable train image |
| `setup()`, after `groupCheck.accepted()` | same content check, at startup |
| `startCycle()` pre-flight | same check, one cycle later |
| `runMatching()` | the active pattern-group snapshot is unavailable |

**Why it matters.** This is the *same* defect C7 exists to remove, left standing on the
other index. An operator reading `PatternNotRegistered` goes looking for a group that was
never registered, finds it present, and has been sent to the wrong screen — precisely the
complaint item 55 fixed for the camera message and C7 fixed for the camera code.

**The fix, when the owner allocates the value.** Split the two meanings:

- `PatternNotRegistered = 400` — out of range, or no group for that number. Unchanged value,
  so no deployed PLC program is disturbed.
- `PatternInvalid = 402` — the group exists but is unusable (no train image, snapshot
  missing). New value; the four sites above move to it.

402 is free and inside the documented 400s band (pattern/calibration). `401` is
`CalibrationInvalid`.

**Not done in C7 on purpose.** Adding a numeric code to the PLC contract is an integration
change, not a rename: `plc_signal_contract.md` says *"Do not renumber existing codes. Add
new codes in a documented range and update tests"*, and any master already branching on 400
for a content fault would need to learn 402. That is the owner's call, not a side effect of
a rename they asked for.

**Verification when done.** Extend
`test_an_index_that_names_nothing_reports_not_registered_not_lost` with a fifth block: a
registered group whose patterns carry no train image must publish 402, and the negative
check is that reverting it to 400 makes exactly that block fail.

**What it was hiding.** With the test restored, item 32's flake is real. Measured
2026-09-08 across ten runs of each suite:

| suite | total | `test_disconnect_notice_on_graceful_close` |
|---|---|---|
| `vision_output_device_test` | 9 | **4 / 10 fail** (2/6, then 2/4) |
| `vision_tcpip_client_device_test` | 10 | **6 / 10 fail** (5/6, then 1/4) |

⚠️ **The rate is not stable between batches** — the client suite went 5-in-6 and then
1-in-4 with no change in between. Treat these as "fails often, timing-dependent", not as a
percentage. A run of 20 is the minimum that means anything, and **a green batch of 4 proves
nothing**; Task D2's verification already asks for 20 and should not be shortened.

Item 32 names only the client suite, and no batch has yet shown the server copy failing
more than the client, so Phase 9 Task D2 should still drive from the client.

**Scope — every suite was audited, only these two are affected.** Source `void test_` count
vs what the binary registers, 2026-09-08:

| suite | source | binary | total | verdict |
|---|---|---|---|---|
| `architecture_contract_test` | 103 | 103 | 105 | clean |
| `mc_frame_test` | 42 | 42 | 44 | clean |
| `modbus_device_test` | 20 | 20 | 22 | clean |
| `vision_output_device_test` | 7 | **6** → 7 after fix | 9 | **was holed** — shadowing moc |
| `vision_tcpip_client_device_test` | 8 | **7** → 8 after fix | 10 | **was holed** — shadowing moc |
| `jai_camera_hardware_test` | 19 | **17** | 21 | **holed — different cause, see below** |

The three clean suites keep their `main.moc` only under `release\`; only the two vision
build dirs carried a root-level copy, which is what made them the ones to rot.

**`jai_camera_hardware_test` is a second, unrelated cause: an ordinary stale build.** Its
`main.cpp` was edited **2026-09-04**; its `release\main.moc` and its executable are both
from **2026-09-02**. No shadowing is involved — there is no root `main.moc` — the suite
simply was not rebuilt after `test_backlight_override_drives_the_lamp_and_reports_it` and
`test_backlight_override_survives_a_grab` were added. `Compare-Object` against `-functions`
named both directly.

Left as found rather than rebuilt blind: it is a hardware suite that needs the bench camera
and `NCR_JAI_TEST_IP`, and rebuilding it proves nothing without one. **Rebuild it before
quoting any number from it.** The point for this item is that two *different* faults
produce the identical invisible symptom, which is the argument for the guard below.

**Status of the fix.** The two affected build dirs are repaired and verified. **No source
or `.pro` change is needed or wanted** — nothing was wrong with them.

**The guard is in place (2026-09-08).**
[`docs/rules/build_and_verification.md`](../rules/build_and_verification.md) gained
*"Confirm A Suite's Shape Before Trusting Its Total"*: the `-functions` vs `void test_`
comparison, both causes, and a dated shape table for all six suites. That doc also now
documents `NCR_JAI_TEST_IP`, which had never been written down anywhere.

**Why this item stays open.** The guard is a written step, not an enforced one — nothing
fails if it is skipped. The natural home for enforcement is the architecture contract test,
which already checks things a build cannot (the `.qrc` split, the display-name markers), but
it cannot read another suite's binary. Options, none obviously right:

- have each test `.pro` emit its expected function count and assert it at `initTestCase` —
  self-checking, but every suite pays for a fault two suites had;
- a small script over all suites, run as part of the release routine — cheap, but one more
  thing to remember;
- accept the written step and rely on the shape table drifting visibly.

Closing this needs a decision, not more investigation. Until then the written step stands,
and `jai_camera_hardware_test` is a live example of the failure it catches.

---

## 64. A dual-role Modbus client aborts the cycle when the result publish collides with the poll

**Status:** open, **field-observed 2026-09-14**, on the exact binding item 57 was just closed on.
Twice in 85 cycles (~2.4%), each time needing a `bErrorReset` to clear.

**What happens.** With a Modbus TCP client carrying **both** `primary_plc` and `vision_output`
(device 05 in `app_log_2026-09-14.txt`), the runtime's result publish is dispatched while a *poll*
transaction is still inside its nested wait. `sendVisionResult()`
(`modbus_tcp_client_device.cpp:836`) runs its first write — step 1, the count clear at `:893` —
which hits the in-flight guard at `:390` and is refused:

```
:857  Modbus request failed. deviceId= 05 request= WRITE HR[0..0] count=1 values=[0]
      reason= A Modbus transaction is already in flight.
:858  Modbus write collided with a transaction already in flight; not counted against the
      link retry budget.
:859  Modbus result publish failed. deviceId= 05
:861  Task state transition: RunningCycle -> Recovering (A Modbus transaction is already in flight.)
```

(the second occurrence is `:962-965`.)

**Phase 8 / F2a is working correctly here** — `noteTransactionFailure()` (`:494-503`) exempts the
collision from the link retry budget, so the client does **not** tear its own link down. The link
stays up. What is missing is any recovery for the *publish itself*: `sendVisionResult()` returns
false, and the controller aborts the whole cycle.

**Why the existing mitigations do not cover this path.** `writeDigitalIoByName()` /
`writeWordIoByName()` **park** on `m_inTransaction` (`:734-737`) and replay via
`drainPendingIoWrites()`. `sendVisionResult()` deliberately cannot: Phase 8 rejected parking for it
because its four sequential `transact()` calls must not be interleaved — a parked tag write draining
between the count write and the payload write is exactly the half-written block the function is
arranged to prevent. It does stop the poll timer (`:864-867`) and set `m_suppressPendingDrain`
(`:880`), but **both guard the outbound direction only**: they stop anything interleaving *once the
publish has started*. Neither helps when a poll is **already** in flight as the publish arrives.
Stopping the timer is too late by then.

**Options, none obviously right — this needs a decision, not a quick patch:**
- Retry the publish once the current transaction unwinds. Must stay bounded, and the cycle's send
  completion must still resolve **exactly once** (the E1/E3 contract).
- Give the publish priority: refuse to *start* a poll while a publish is pending.
- Treat the collision as retryable at the controller. Note E4's write-retry budget covers tracked
  handshake **tag** writes; the result send is not one of them.

> ⚠️ **Do not "fix" this by making the failure non-fatal.** A result that never reached the
> registers must never be reported as sent — that is the silent-wrong-result class this whole phase
> exists to remove.

**Verification when done.** `modbus_device_test` cannot currently reach this: every write test calls
the device directly on its own thread, so a queued burst never forms (Phase 8 recorded the same
gap). A new case has to post the publish from another thread while a poll `transact()` is in its
nested wait.

---

## 65. `app/` no longer resolves, so a from-scratch build silently produces a shell with no sources

**Status:** **RESOLVED 2026-09-14 — owner decision: `components/app/` is the shell's home, so the
wiring was repointed rather than the junction recreated.** Found the same day; it is why
`architecture_contract_test` briefly read **152 passed / 7 failed**. Not caused by Phase 9 / F.

> **What changed.** The five functional references — `ncr_picking.pro:19`,
> `runtime_app/runtime_app.pri:42`, `translations/ncr_translations.pro:69` and `:96`,
> `scripts/update_translations.ps1:42` — plus the Doxygen `INPUT` in `docs/doxygen/Doxyfile`. The
> contract test's seven path expectations, deliberately and together with the layout change. The
> authoritative docs that describe the current layout: `AGENT.md` (module map and layering rule),
> both shell scope cards, the four `src/*/AGENTS.md` "must not include" lines, `docs/README.md`,
> `docs/rules/{build_and_verification,documentation_build,design_rules}.md`,
> `docs/domains/runtime_app/runtime_shell.md`, `uml/README.md`, `docs/doxygen/pages/mainpage.dox`
> and `docs/doxygen/build_docs.bat`. `components/app/AGENTS.md`'s relative link to
> `runtime_shell.md` had been broken by the extra directory level and is fixed. `docs/history/` and
> `docs/backlog/` are records and were left as written.
>
> **The part a plain rename would have got silently wrong.** `test_module_include_layering_contract`
> classifies an include by its **first path segment**. `runtime_app.pri` puts the repository root on
> `INCLUDEPATH`, so after the move the runtime shell reaches the editor as
> `"components/app/mainwindow.h"` — first segment `components`, which is no module, so the check
> skipped it. Renaming the directory in the test alone would have left **the peer rule blind while
> still green**. The test now maps a shell's directory back to its module name
> (`components/app/...` → `app`). **Negative-checked both ways:** a probe
> `#include "components/app/mainwindow.h"` placed in `runtime_app/src/runtime_layout_controller.cpp`
> fails the contract with the mapping (*"runtime_app must not include app"*) and **passes without
> it**. Probe removed, mapping restored.
>
> **Verification.** Fresh `qmake` of `ncr_picking_all.pro` in `build\all\Release`: no `Cannot read`,
> and `Makefile.ncr_picking.Release` lists `..\..\..\components\app\main.cpp` with no root-`app/`
> reference left. nmake then judged the editor's objects up to date — the sources had moved but not
> changed — so `components/app/main.cpp` and `mainwindow.cpp` had their **mtimes bumped, content
> untouched**, to force a real compile from the new path: both compiled and **both shells relinked**
> (11:59). Contract test **159 / 0**, shape 157 == 157. Translation sweep through the repointed
> script: `Updating 'components/app/translations/ncr_picking_ja_JP.ts'`, **1324 source texts — the
> same total as 2026-09-10 — vanished 33 → 33**. That is the discriminating number: had
> `ncr_translations.pro` failed to include `app.pri`, `MainWindow`'s strings would have gone vanished.
> A second umbrella build carried the regenerated `.qm` into both shells (12:01); a re-run found
> nothing left to build and no zero-byte objects.
>
> The warning further down — *"Do not repoint the test at components/app to get green"* — was about
> doing it **to get green**. Done as part of an owner-decided layout change, with the peer-rule hole
> closed and negative-checked, it is the contract being updated, not bypassed.

**Measured, not inferred:**

| Check | Result |
|---|---|
| `<root>\app` exists | **False** |
| `<root>\components\app\app.pri` exists | True |
| `ncr_picking.pro:19` | `include(app/app.pri)` |
| fresh `qmake ncr_picking.pro` (inside vcvars64) | `Cannot read C:/DGB/Project/ncr_picking/app/app.pri: No such file or directory` — **and exits 0** |
| generated `Makefile.Release` | no `main.cpp`, no `mainwindow`, no `components/app` |
| anything outside `build/` referencing `components/app` | **nothing** |

**Two more dangling paths on the same cause:** `runtime_app/runtime_app.pri:42`
(`$$PWD/../app/translations/ncr_picking_ja_JP.ts`) and `scripts/update_translations.ps1:42`
(`app\translations\ncr_picking_ja_JP.ts`) — the shared `.ts` the whole translation pipeline runs on.

**Why nobody noticed.** qmake's `include()` of a missing file warns and continues, so the failure is
silent and the exit code is success. The 2026-09-14 binaries (08:42) were produced by `nmake` from
Makefiles dated **2026-09-10 10:22** — generated while `app/` still resolved — relinking
pre-existing objects, so the build appeared healthy.

**When it broke, and the likely mechanism.** `app/` resolved as recently as **2026-09-10 09:12**:
the Phase F translation sweep wrote through `app\translations\...` and the file it produced is
`components\app\translations\ncr_picking_ja_JP.ts`, whose siblings retain mtimes back to 08-24. That
is consistent with `app/` having been a **directory junction** onto `components\app` that has since
been removed, rather than a genuine move — but the filesystem can no longer distinguish the two, so
**the owner should confirm which it was** before anything is repointed.

**The seven failing tests are not stale — they are the guard that caught this**
(`test_both_shells_take_the_same_instance_key`, `..._report_one_shared_version`,
`test_action_signals_are_never_blocked_in_the_shells`, `test_runtime_shell_is_a_peer_of_the_app_shell`,
`test_translations_are_updated_from_one_project_that_sees_every_source`,
`test_both_shells_load_translations_through_the_shared_helper`,
`test_module_include_layering_contract`). **Do not repoint the test at `components/app` to get green**
— it encodes the layout contract and is doing its job.

**The fix is a layout decision, and it is the owner's:** either recreate the `app` junction, or
repoint `ncr_picking.pro:19`, `runtime_app.pri:42`, `scripts/update_translations.ps1:42` **and** the
contract test's expectations at `components/app`, together, in one change.

**Worth considering while here:** a missing `include()` that still exits 0 can hide any future move
the same way. A one-line `!exists(...): error(...)` guard in the shell `.pro`s would convert this
class of fault from silent to loud.

---

## 66. Two editors for the robot pick check: the task's, and each vision-output device's

**Status:** open, **filed 2026-09-14 (Phase 9 / Z2)** — deferred by design in F1 (plan open
question O-2), not a regression.

Since Phase 9 / F1 the localization runtime's pickability gate reads
`TaskLocalizeConfig::robotCheckConfig()`, edited from the task's Settings tab (**Robot pick check →
Set…**). `RobotKinematicCheckWidget` is still embedded in `VisionTcpipDeviceWidget` and
`VisionTcpipClientDeviceWidget` too, editing `VisionOutputDeviceCfg::m_kinematicCheck` — which now
drives **only** the device-side advisory check, `VisionTcpipDeviceBase::runKinematicCheck()` (log +
`kinematicCheckResult`, never blocks the payload).

**Why it matters.** An engineer who opens the device panel sees an "enabled" pick check with a
preset and a pick path, and has every reason to believe that is what gates the robot. It is not, and
the two copies can disagree silently.

**Options, none chosen:** remove the device-widget editors and the advisory check with them; keep
the advisory check but feed it the task's settings; or keep both and label the device copy as
advisory. `IResultOutputDevice::robotKinematicCheckConfig()` — still implemented by every
result-output device, no longer called by the task — goes or stays with that decision. Removing a
method with live implementors and a persisted JSON key is a deprecation with a migration question,
which is why F1 did not fold it in.

---

## 67. Recovery policies cannot be set outside a test

**Status:** open, **filed 2026-09-14 (Phase 9 / Z1–Z2)**; persistence deferred by plan decision D5.

`LocalizationRuntimeController::setRecoveryPolicies()` has **no production caller** — its only
caller is `tests/architecture_contract_test/main.cpp`, which shortens the retry interval. Every
shipped runtime therefore runs on `defaultCameraRecoveryPolicy()` / `defaultPlcRecoveryPolicy()` /
`defaultVisionOutputRecoveryPolicy()`: retry every 5000 ms, `LostConnected` and `ConnectFailed` both
recoverable. There is nowhere to change that per cell.

**When it is picked up.** The injection point is `TaskLocalization::setupRuntimeController()`,
immediately before `controller->setup(context)` and inside the same thread hop. Policies are copied
into each role's recovery context when the role is bound (`bindRoleContext()`), so a call made after
`setup()` changes nothing until the next bind. Persisting the values means a `TaskLocalizeConfig`
field and a schema bump.

Z1 removed `LocalizationRecoveryPolicy::connectTimeoutMs`, which nothing read. Do not bring it back
as part of this: how long one connect attempt may take belongs to the runner and the device.

---

## 68. At most two positions per cycle, hard-coded — and a debug print on the PLC path

**Status:** open, **found 2026-09-14 during the Phase 9 / Z2 documentation pass.** Not changed,
because it may be deliberate — **owner to confirm.**

**1. The cap.** `LocalizationRuntimeController::buildVisionOutputPositions()`
(`localization_runtime_controller.cpp:1725-1730`) appends a position only while
`positions.size() < 2`. Every further object that passed the collision, condition-workspace and pick
checks is marked `Skipped` — **with no reason in the row**, unlike every other skip, which names its
cause. No contract document mentions a limit, the number is not a named constant, and nothing
explains it. On a cell with three pickable parts the operator sees the third one "Skipped" and cannot
tell why. `runtime_controller_api.md` → "Output Coordinate Contract" now states it, with a pointer
here.

If it is intentional — a robot program that accepts two targets, say — it wants a named constant or a
setting, a reason string in the row, and a line in
[pick_geometry_and_output_contract.md](../domains/task_localization/pick_geometry_and_output_contract.md).
If it is a bench leftover, it should go. The commented-out status logic just above it (`:1714-1722`)
can go with whichever decision is made.

**2. A debug print in `handlePlcValues()`.** `localization_runtime_controller.cpp:938-939`: a
`/// temp debug` marker followed by `qDebug() << "Handle plc value: camera number:" …`, on every
`nActiveCamera` event. Harmless, but it is marked temporary and it prints outside the logger.

---

## 69. PLC write acknowledgement (D3): built and bench-proven — a refused write is unverified on hardware

**Status:** open **only for the E4 hardware observation.** Filed 2026-09-14 by Phase 9 / Z3: the work
was planned inside Phase 9 without a backlog entry, and this records what shipped and what is not yet
proven.

**What shipped (Phase 9 / E1–E5).**

| Task | Result |
|---|---|
| **E1** | `McProtocolDevice` resolves every write **exactly once**, including every abandon path — teardown, retry exhaustion, re-initialisation. A `createMsgInterface()` seam lets `mc_frame_test` drive the whole device through a fake port |
| **E2** | The Modbus client and server and `VirtualPlcDevice` resolve their writes too, including writes parked behind a transaction already in flight |
| **E3** | `IPlcIoWriter` tracked writes carry an id; `PlcRunner::writeFinished(id, ok, message)` reports every outcome |
| **E4** | `LocalizationRuntimeController` retries the five handshake outputs (3 attempts in total, 40 ms apart) and aborts the cycle with **`301 PlcWriteFailed`**. Advisory outputs, writes to a disconnected role and the abort's own publishes are not tracked. Contract: `plc_signal_contract.md` → "Handshake Writes Are Acknowledged" |
| **E5** | The MC `pollingUpdate` snapshot is published only after a complete read pass |

**Owner-confirmed on hardware (2026-09-09):** E5 — no startup fault on the MC binding; E1 — a cable
pulled mid-write on the C24 produces a failure completion.

**Not confirmed: E4 — a real PLC refusing a write on a healthy link.** The owner has no way to make a
healthy PLC refuse on demand. Two recipes need no PLC cooperation; neither has been tried:

| Family | Recipe | Why the write fails while the link stays up |
|---|---|---|
| Modbus client | Map a handshake signal (e.g. `bMatchingFinished`) to a **discrete-input** tag | `writeDigitalIoByName()` refuses locally — a master cannot write discrete inputs |
| MC | Map a handshake signal to a device address the PLC does not have | The C24 answers with a non-zero end code; the write resolves failed and the budget runs out |

> ⚠️ Check first whether the Phase 9 / C4 signal-map gate refuses the Modbus recipe at setup. The gate
> asks whether the device *provides* the tag, and a discrete input is provided, so it should pass and
> fail at write time — which is what makes the recipe work. If the gate refuses it, only the MC recipe
> remains.

The 2026-09-14 Modbus run logged `PLC write failed: Modbus client is not connected` during a cable
pull. That is **not** this observation: the role was disconnected, which is `300 PlcLost`'s path by
design and deliberately untracked.

**Related but separate:** item 64 — on a dual-role Modbus client the *result publish* can collide
with the poll and abort the cycle. That path is `sendVisionResult()`, not a handshake write.

---

## 70. A vision-output client stays Recovering when only its heartbeat link comes back

**Status:** open — **field-reported 2026-09-09; investigation paused by the owner 2026-09-10.** Filed by
Phase 9 / Z3 so the evidence is not lost.

**Symptom (owner).** A heartbeat timeout on the vision output puts the task in Recovering. After the
cable is reconnected the receiving side shows a connection and its heartbeat counter climbs — but the
task never leaves Recovering until the runtime is stopped.

**The device was the CLIENT.** `app_log_2026-09-09.txt:6136` — `VisionTcpipClientDevice dialing
192.168.0.10 main port: 5000 heartbeat port: 5001`, device `03 VisionOut_01`, the binding for the
whole failing session (loaded at `:6116`, next reload not until `:6238`). **This corrects the first
round of the investigation**, which reasoned from the *server* device, `VisionTcpipDevice` — present
elsewhere in that day's log, on 127.0.0.1. Its "listening ⇒ Connected" predicate and the idea that
"the peer never dialled the main port" do not apply to a client.

**The failing sequence** (`app_log_2026-09-09.txt`):

```
:6209-6211  15:58:10  main link up + heartbeat link up; Recovering -> Ready   (a recovery that works)
:6212       16:02:39  VisionTcpip lost connection: Heartbeat reply timeout (3000 ms)
:6214       16:02:39  Ready -> Recovering (role=vision_output status=LostConnected)
:6216       16:02:44  VisionTcpipDeviceBase already active VisionOut_01      (recovery attempt 1)
:6217       16:03:46  VisionTcpip heartbeat link up from 192.168.0.10        (heartbeat ONLY)
:6218       16:23:52  heartbeat reply timeout again
:6221       16:23:57  VisionTcpipDeviceBase already active VisionOut_01      (recovery attempt 2)
:6222       16:24:15  VisionTcpip heartbeat link up from 192.168.0.10        (heartbeat ONLY again)
:6223       16:25:49  Recovering -> Stopping (endCommission)
```

**What that points at — a hypothesis from the log, not yet confirmed in the code.**

1. The client reports `Connected` only when **both** links are up and `Connecting` otherwise
   (`VisionTcpipClientDevice::publishCurrentConnectStatus()`, Phase 9 / D1). With only the heartbeat
   link back, the role never reports healthy, so the controller correctly never re-arms. **The
   controller is probably not the defect.**
2. Every recovery reconnect is a **no-op**: `deviceConnect()` returns at
   `vision_tcpip_device_base.cpp:52` ("already active") because the device still counts itself active
   after the heartbeat timeout. The recovery policy's retries issue no new dial.
3. Something re-dials the **heartbeat** socket — it came back 62 s and 18 s after each attempt — but
   not the **main** socket. Why is the question.

Contrast `:6279-6285`, the same device half an hour later: a heartbeat timeout, then **both** links up
at 16:37:37. The main link does sometimes return; what differs is not yet known.

**Why the first round went slowly.** `setConnectionStatus()` logs nothing, so the device's own status
transitions are invisible in the app log; and `appendTaskLog()` writes only to the task-log panel, so
the absence of a "recovered" line in the app log proved nothing — an inference that was made, and
withdrawn, in that round.

**When it is picked up.** Read the client's redial path for the main socket after a heartbeat-timeout
teardown, and why `deviceConnect()` stays "already active" through it. One DEV log line per
connect-status change would make the next reproduction conclusive.

---

## 71. Phase 9 / A1's M-only MC trigger delivery is unverified on hardware

**Status:** open — filed 2026-09-16 at the Phase 9 closeout as a carried item. Not a defect: an
**unverified claim**. Non-blocking for Phase 10.

**What.** Phase 9 Task A1 fixed `McProtocolDevice::check_device_changed()` discarding every M
change when no D range is configured (item 59.3b), by extracting the shadow-map diff into the
header-only helper in `src/device/plc/mc_device_map_diff.h`. The fix is proven by unit cases in
`mc_frame_test` against that helper only. The field half — a `bExecuteTrigger` change on a real
M-only MC PLC (`amountDAddress = 0`) reaching the task — was never run, because no M-only station
was identified (Phase 9 open question O-3). The plan ships A1 on unit evidence and forbids recording
it as hardware-verified.

**Where.** `src/device/plc/mc_device_map_diff.h`, `src/device/plc/mc_protocol_device.cpp`; Phase 9
plan Task A1 and the Checkpoint Z OWNER-RUN table.

**Why deferred.** Needs hardware that is not on the bench.

**How to pick up.** When an M-only MC station exists: commission a project with only M tags mapped,
start the runtime, pulse the trigger, and confirm the cycle runs (app log `Ready -> RunningCycle`,
and the `RT state=` trace once Phase 10 / WP-10 has landed). Record the result here and close.

---

## 72. A refused index selection updates no camera visual on the dashboard (residual of item 61)

**Status:** open — filed 2026-09-16 by the Phase 10 triage (WP-03) from item 61's "not fixed here,
and still open" note. Operator-visibility defect, not a control defect: the fault panel and the PLC
outputs are correct; only the camera lamp is stale.

**What.** On the refusal paths of `setActiveCameraNumber()` (out-of-range or unregistered number,
and the non-numeric case via `reportSignalTypeMismatch()`) the controller returns before publishing
any index, so `LocalizationDashboardWidget` never re-resolves the device behind the camera lamp; the
lamp keeps showing the previously bound camera while the task is `Faulted` with `103`. The mechanism
is as item 61 recorded it on 2026-09-08 and has not been re-confirmed against the post-C3 code
(`applySignalToDashboard()` now matches both the command and the status name); confirm before fixing.

**Where.** `src/model/localization_runtime_controller.cpp` (`setActiveCameraNumber()` refusal
branches, `reportSignalTypeMismatch()`); `src/ui/forms/task/localization_dashboard_widget.cpp`
(`applySignalToDashboard()`, `resolveActiveCameraDeviceId()`).

**Why deferred.** Item 61 fixed the binding-staleness cause only. The Phase 10 to-be design
(`Selection` state plus `outputSnapshot()`, WP-33 / WP-34) publishes a snapshot on every selection
event including a refusal, which covers this by construction — but that lands at Stage 4, and the
shipped code still has the gap.

**How to pick up.** Either verify it closed at Stage 4 with scenario S-03 (the camera lamp must
reflect a refused selection), or fix earlier with a dedicated dashboard event on the refusal branch.
Owner-run either way: write camera 0 while Ready, confirm the lamp shows the refused state, then
write a valid number and confirm it recovers.
