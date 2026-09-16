# Phase 9 Implementation Plan — Localization Runtime Backlog

**Date:** 2026-09-08 (issued) · 2026-09-16 (closed)
**Status:** CLOSED WITH CARRIED ITEMS — 2026-09-16. Issued as "DRAFT — not started" on 2026-09-08,
ran 2026-09-08 → 2026-09-14, and closed on 2026-09-16 with every unfinished item carried to a named
destination (a Phase 10 work package or a backlog item) — see "Checkpoint Z — Phase 9 complete" for
the carry table.
**Source request:** [`../request/phase_9_request.md`](../request/phase_9_request.md)
**Owner decisions:** D1–D6, locked 2026-09-07; D7–D8 added 2026-09-08. Planned against, not re-litigated. All eight are recorded as `DR-0001`…`DR-0008` in `docs/decisions/` (imported 2026-09-16).
**Backlog:** [`later_todo_list.md`](../../backlog/later_todo_list.md) items 1, 25, 26, 32, 51, 53,
54, 55, 57, 58, 59 (+ 59.3b, found for this plan and not yet filed).

> **Location note.** Same exception as Phases 5–8: this file lives under `docs/history/`, which
> `AGENT.md` declares traceability-only, but it is the CURRENT plan for Phase 9. The standing
> recommendation to move `docs/history/plan/` to `docs/plan/` is now five phases old.
> Since the phase closed on 2026-09-16 this file's place under `docs/history/plan/` is correct — it
> is a closed plan kept for traceability — and active plans live in `docs/plan/` (created by
> Phase 10 / WP-01).

### Baselines, re-measured 2026-09-08 — not carried over from Phase 8

| Suite | Baseline | Evidence |
|---|---|---|
| `architecture_contract_test` | **101 passed / 0 failed** | `build\architecture_contract_test\release\results.txt` (2026-09-07 15:47); 99 `test_*` functions + `initTestCase`/`cleanupTestCase` |
| display-name total | **122** | `tests/architecture_contract_test/main.cpp:4210` |
| `mc_frame_test` | 38 (36 fns + 2) | counted 2026-09-08 |
| `modbus_device_test` | 22 (20 fns + 2) | counted 2026-09-08 |
| `vision_output_device_test` | 9 (7 fns + 2) — 8 pass + **the same flake**, 4/10 runs | counted from source, **run-verified 2026-09-08** |
| `vision_tcpip_client_device_test` | **10** (8 fns + 2) — 9 pass + 1 known flake, item 32, **6/10 runs** | counted from source, **run-verified 2026-09-08** |

> ⚠️ **These two counts are counts of the SOURCE, and until 2026-09-08 the binaries did not match
> them.** Both suites shipped a build in which `test_disconnect_notice_on_graceful_close` was absent
> from the metaobject — reported as `0 skipped, 0 blacklisted`, so the suites ran 8 and 9 and looked
> green. A stale root `main.moc` shadowed the generated one and, worse, qmake had written that stale
> path into the Makefile's dependencies. **Fixed and verified in both build dirs** (delete the root
> copy, re-run qmake, force one recompile — **backlog 62**); no source or `.pro` change was needed.
>
> **Verify with `<exe> -functions`, not with the total.** The flake rates above were measured only
> after the fix, and they **swing between batches** — the client suite gave 5-in-6 and then 1-in-4
> with nothing changed. A short green batch means nothing for this case.

> ⚠️ Phase 8 closed recording the contract suite at **95**. It is **101** today. A task that
> reports 96 has not passed; it has run the wrong binary.

> ⚠️ **Test counts in the task bodies below are ADDITIVE targets, not a verified arithmetic chain.**
> Read `**A → B**` as *"this task adds B−A cases"*. The running totals were computed against a draft
> ordering that has since changed — **Task C1 was deleted and Task C6 added on 2026-09-08** — and one
> stretch of them is visibly inconsistent (Phase D opens lower than Phase C closes). They have
> deliberately **not** been re-threaded into a fresh chain, because a hand-maintained cumulative
> count across twenty tasks is false precision that drifts the moment anything is reordered or
> dropped.
>
> **What a checkpoint actually requires:** `0 failed`, and the observed total equal to the baseline
> plus what everything landed so far actually added — **recorded as observed**, not matched against a
> number written earlier. If an observed total disagrees with a written one, the written one is
> wrong: say so in the checkpoint and move on. Never add a filler test to reach a number.

### Revision 2026-09-08 — corrections applied to the draft

The first draft of this plan was produced by a design pass and then reviewed. These are the
substantive corrections; they are recorded rather than silently folded in, because two of them are
the kind of error that would have cost a day to find during implementation.

| What | Correction |
|---|---|
| **Scope** | The owner's `docs/history/request/phase_9_*.md` files are **tentative notes, not a spec** (*"mình chỉ dự tính"*). The draft treated them as the phase definition. Scope is the fifteen verified backlog items, per the owner: *"giữ plan của bạn"*. |
| **Task C1 — deleted** | It would have made an invalid index not-a-fault. **D7**: the Phase 8 F1/F4/F5 behaviour stands. |
| **Task C6 — added** | **D8**: `beginRuntime()` connects PLC/output first and `setup()` reads the live selection from the PLC. This is item 58's root fix and the phase's main deliverable. |
| **`reportIndexRejection()` does not exist** | The draft's C1 and C2 both leaned on it. The real edit sites are three duplicated inline blocks. C2 must **create** the helper. Verified 2026-09-08. |
| **Line citations for both setters were ~16 lines stale** | `setActiveCameraNumber()` starts at **`:154`** (not `:170`/`:178`); `setActivePatternGroupNumber()` at **`:265`** (not `:277`). Corrected in C2. Treat every other cite in this plan as needing the same check before it is relied on. |
| **C3 would have silently killed the camera lamp** | `applySignalToDashboard()` (`localization_dashboard_widget.cpp:489`) matches the literal name `"nActiveCamera"` and re-resolves the lamp's device on change. Deleting the echo without adding the status names there breaks it for every selection the PLC did not write. Criterion and split point corrected. |
| **A review finding that was itself wrong** | The review placed the blanket disconnect at `:717`. It is at **`:718`**, as the draft said. Both the draft and its reviewer were checked against source; neither was taken on trust. |

**Sizing corrections.** The draft under-sized seven tasks. **This table is authoritative** — the
`**Size:**` line inside each task body still carries the draft's value, and where the two disagree
this table wins. It is kept in one place on purpose: seven scattered edits drift, one table does not.

| Task | Draft | Actual | Why |
|---|---|---|---|
| **C2** | S | **M** | Must create the validator across three inline blocks, and C6 now depends on its verdict type |
| **C4** | M | **L** | Runtime gate + a new save-time confirmation dialog (new UI, strings, translation sweep, two themes) + the `.vproj` audit + seven tests. **Split at the runtime/UI seam.** |
| **C6** | — | **L** | Accessor across four device implementations + a `beginRuntime()` lifecycle change. Split as stated in the task. |
| **E1** | M | **L** | Needs an `McMsgInterface` injection seam that does not exist; unverifiable without it |
| **F1** | M | **L** | Config + schema bump + persistence + rewritten capability contract + rehosting a 517-line widget with its `.ui` and translation sweep |
| **G2** | M | **L** | New virtual on `PlcValueMap` × four implementations + arm state machine + wire-contract rewrite. **C6 part (a) pays for the accessor**, which drops G2 to M if C6 lands first. |
| **A1** | S | **M** | New header + `.pri` registration + tests, and the helper owns shadow mutation |
| **Z2** | S | **M** | Six documents with "every cited line number confirmed" as a criterion — the review found roughly a dozen stale cites in this plan alone |

**Scope reductions — build the smaller thing.** Four places in the draft proposed more structure
than the work needs, against the project's "no broad abstractions before a second implementation"
rule:

- **A1:** drop the `template <typename T> diffDeviceMap(…, perAddressCallback)`. Two call sites in
  one file. **Two explicit non-template functions** do the same job.
- **E3:** drop the `DeviceCommandKind` PLC additions. One consumer. Use the draft's own fallback —
  **`writeFinished(id, ok, message)` on `PlcRunner`** — as the design, not the escape hatch.
- **B2(b):** do not re-run a full trigger→grab→match→send cycle one layer above
  `test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs`. The untested thing
  is the forward at `task_localization.cpp:593-606` — **one injected signal**, not a cycle.
- **F3:** two timestamps and one log line answer the latency question. The five boundaries,
  partial-fill semantics and dashboard KPI are ahead of demand.

---

## Overview and goal

Phase 9 has no new device sub-type and no new protocol. It is fourteen defects, one missing
feature and three request items, and the goal is one sentence: **stop the localization runtime
from doing the wrong thing quietly.**

Every defect the owner reported in Phase F, and item 58 after it, has the same shape — the build
is clean, the suite is green, the dashboard is alive, and the machine is doing something nobody
asked for. Three of the defects in this phase (**59.2**, **59.3b**, the vision-output connect
wedge) are exactly that shape again. None of them was found by a test; all were found by reading
the source or by the owner on the cell.

So the phase is ordered by **how soon a defect hurts a real cell, and how invisible it is while it
does** — with one deliberate exception, stated below. Everything painted on screen is marked
**OWNER-RUN**, because there is no widget-level test framework in this project and there will not
be one this phase.

### The ordering, and the three places the alternatives disagree

1. **The silent wrong pick goes first.** `m_context.activeCameraWorkspace` is written in exactly
   one place in the whole tree — `setActiveCameraNumber()` (`localization_runtime_controller.cpp:252-254`)
   — so on a normal start it stays default for the whole session: condition-ROI filtering is off
   and fenced-out objects ship as pick targets on cycle 1. The fix is two lines. It is task **A2**,
   not a task in the middle of a refactor branch, because it is the only defect in this phase that
   makes the cell pick the wrong thing.
2. **`clearRoleContext()` (item 53, LOW) lands third, not last.** Its blanket
   `disconnect(runner, nullptr, this, nullptr)` at `:718` is survivable today only because
   `setup()` re-makes `m_plcValueConnection` at `:412`, ten lines before `bindFixedRoleRunners()`
   at `:422` — and Phase C rewrites exactly that window. Severity ranks the value of a fix; it does
   not rank the order when one item is the precondition for the *safety* of the others. A2 is
   sequenced ahead of it anyway, with an explicit criterion that A2 must not move the connection
   assignment.
3. **Item 8's PLC-write policy is fifth, behind a stop rule.** It is the only open-ended item in
   the phase (write correlation across a round-robin polling state machine), and none of the
   earlier fixes gets harder by waiting for it. Interface churn is cheap to absorb late;
   a mis-picked part is not.

**Where the phase does *not* sort by harm:** the reproduction and harness work in Phase B sits
ahead of Phase C. Both of item 25's defects, and item 58, survived because the
controller → task → widget hop has no test at all. Fixing them on top of an untested hop buys a
second chance to be wrong silently.

---

## Decisions taken with the project owner (locked 2026-09-07)

Recorded verbatim, with the rationale and where each lands. **Not re-litigated in this plan.**

| # | Decision (verbatim) | Rationale | Lands in |
|---|---|---|---|
| **D1** | *Item 58, announcing the active selection:* add **separate status signals** (`nActiveCameraStatus` / `nActivePatternGroupStatus`), and make them **optional tags exactly like every other output signal**. If the operator leaves the tag empty in the signal-map UI, the runtime performs **no PLC write** — it only keeps the value internally and emits `signalChanged()` for the UI/dashboard. **Never** echo onto the master's command registers (`nActiveCamera` / `nActivePatternGroup`). | `publishNumberSignal()` (`:820-832`) already implements the rule — emit always, write only when a tag is mapped — so no special-casing is needed. A task that writes back onto the master's own command register can fight a master about to write its own value; the comment at `:299-303` records that. | **C3** |
| **D2** | *Item 57, robot pick-check config:* move the settings to `TaskLocalizeConfig` and read them **family-independently**, ignoring `IResultOutputDevice::robotKinematicCheckConfig()` for this purpose. A **null checker with `enabled=true` is a hard setup error**. | The Modbus devices hold `m_kinematicCheck` with a setter called only from tests, no `toJson`/`fromJson` and no widget — so the check is structurally inert on the dual-role binding. The comment at `device_capabilities.h:152-159` currently blesses the silent disable and must be revised. | **F1** |
| **D3** | *Item 8, contract write failure:* **bounded retry then `abortCycle`** with a new fault code, for the **handshake signals only** (`bTaskReady`, `bMatchingFinished`, `bTaskFault`, `nFaultCode`, `nDetectedNumber`). Log-only for the rest. **Hard precondition:** on MC, `writeDigitalIoByName`/`writeWordIoByName` return true for **queued**, not written (`mc_protocol_device.cpp:240`, `:263`). A real completion signal must exist there first or this policy is unimplementable. | A retry policy over a primitive that cannot report failure would log "retrying" and "giving up" about writes that were never observed — worse than today's honest silence. | **E1–E4**, behind the stop rule |
| **D4** | *Item 1, signal map gate:* required subset `bExecuteTrigger`, `bTaskReady`, `bMatchingFinished`, `bTaskFault`, `nFaultCode` → **hard setup error** if unmapped. All others → **visible warning**. **Orphan tags** (non-empty but absent from the bound device's tag list) → **hard error in every case**. | An unmapped `bTaskFault` means the PLC never learns a cycle failed and keeps consuming results. An orphan tag is worse than a blank one: `reportRoleError()` deduplicates by message (`:1752-1755`), so a permanently failing write logs once and is silent forever. | **C4** |
| **D5** | Delete `LocalizationRecoveryPolicy::connectTimeoutMs` (dead). Defer persisting policy values. | Its only reader is an assertion at `tests/architecture_contract_test/main.cpp:2195`; `setRecoveryPolicies()`'s only caller is `:2257`. The product ships on defaults. | **Z1** |
| **D6** | Close umbrella item 26, folding its bullets into item 25, the latency task, and item 8. | An umbrella that outlives its children is where work is forgotten twice. | **Z3** |

**One numbering correction, so the closeout targets the right entries.** D3 calls the write-failure
work "item 8". In `later_todo_list.md`, **item 8 is `RobotRunner` — no runtime wiring**, unrelated;
there is no backlog entry for PLC write acknowledgement at all. D6's third fold-target is therefore
not item 8 either: item 26's three remaining bullets are the operator-UI pass (→ **F2**), latency
measurement (→ **F3**) and remaining runtime edge-case tests (→ **B1/B2**). **Z3 files the
write-failure item as a new entry** rather than closing an unrelated one. D4's "item 1" *does*
match (`SignalsMapWidget::checkEmpty()` — caller not wired).

---

## Two decisions taken after the draft, 2026-09-08

> **The `docs/history/request/phase_9_request.md` and `phase_10_request.md` documents are the
> owner's own tentative notes, not a specification.** The owner said so explicitly: *"mình chỉ dự
> tính"*. An earlier revision of this plan treated `phase_9_request.md` as the phase definition and
> built two contingent tasks around it. That was wrong, and both are resolved below. Do not re-derive
> scope from those files.

**D7 — An invalid camera/pattern index REMAINS a task fault.** Owner, 2026-09-08: *"vẫn giữ nguyên
hành vi khi index invalid thì xem là task fault."*

The draft carried a contingent task (C1) to reverse this: stop publishing `bTaskFault`/`nFaultCode`
for an index rejection on the grounds that `bCameraValid`/`bPatternValid` already report it. **That
task is deleted.** Phase 8's F1/F4/F5 behaviour stands unchanged — an out-of-range, unregistered or
non-numeric index publishes `bTaskFault=true` + `nFaultCode` (100/400), enters `CycleState::Faulted`,
sets the per-signal rejection latch, and recovers only on a valid write to that same signal. Nothing
in Phase 9 touches that contract, and no task may weaken it as a side effect.

This also settles the sub-question the draft's critique raised: `reportSignalTypeMismatch()`
(`:914`) — a number signal mapped onto a coil — keeps faulting too. The runtime is now consistent
across all three failure modes rather than split between two rules.

**D8 — `beginRuntime()` connects the PLC and output roles first, and setup reads the live selection
from the PLC.** Owner, 2026-09-08, on whether to do the work their notes had left commented out:
*"Có — đây chính là gốc của bug index 0."*

This is the **root fix for item 58**, and it changes the shape of Phase C. Today `setup()` takes both
indices from `buildRuntimeContext()`, which fills them from `cameraDeviceIds.firstKey()` /
`patternGroups.firstKey()` (`task_localization.cpp:724-737`) — the project's bindings, never the
PLC. The two index signals are **inputs owned by the PLC role**, so resolving them before that role
is connected is resolving them from the wrong source by construction.

It lands as **C6**, and it is the reason **C2 is worth building**: with the live read in place, C2's
setup-side validators stop being defence-in-depth against a context the app cannot produce and start
gating real master-written values. C6 also builds the `PlcValueMap` accessor that **G2 needs**, so
the two owner-gated items share one piece of infrastructure.

---

## The held-trigger item, and what it actually costs

**Item 54 — "a held `bExecuteTrigger` at runtime start."** Still **OWNER-GATED**; the owner has not
ruled on it, and the note in their tentative request is not a ruling. Backlog item 54 is
DOCUMENTATION-ONLY as it stands, and **the fix it originally proposed is a trap** — seeding
`m_lastExecuteTrigger` from the first observed value would swallow the first genuine 0→1 of every
session and hang the robot waiting on a `bMatchingFinished` that cannot come.

If the owner does want a behaviour change, the plan must say what it costs:

- **The described hazard is not currently reachable on any real PLC family.** MC suppresses the
  entire first polling round and adopts the polled values into its shadow maps
  (`mc_protocol_device.cpp:424`, `:476`, gates at `:710`/`:733`); `ModbusRegisterMap` drops its
  baseline the same way (`modbus_register_map.h:161`, `:215-224`). A trigger already high before
  the runtime existed is therefore **never delivered as a change**, so no rising edge occurs and no
  cycle starts. What actually happens is that the task goes **Ready with nothing said about it**.
- Delivering the requested behaviour therefore needs the runtime to learn the **actual** power-up
  level — a family-independent snapshot read. `PlcRunner::pollingUpdate` already exists
  (`plc_runner.h:155`, connected `:224-225`) and every PLC family emits a full map
  (`mc_protocol_device`, both Modbus devices), but `PlcValueMap` (`plc_device.h:114-119`) is an
  empty polymorphic base with only `clone()` — there is no name→value accessor. That accessor is
  the real work.
- **D8 pays for that accessor.** C6 builds it for the index read, so if the owner later opens item
  54, G2's cost drops to the arm-state machine and its contract text. This is the one place in the
  phase where an owner-gated item gets cheaper by waiting.

Planned as **G1/G2**, **owner-gated**: G1 records what the runtime does today (XS, no behaviour
change, worth having either way); G2 implements the request. If the owner keeps 54
documentation-only, G2 is dropped and its documentation half moves into **Z2**.

---

## Current state — verified against source, 2026-09-08

Every row was read, not carried from the backlog.

| Fact | Evidence | Consequence |
|---|---|---|
| MC discards **every** M change when no D range is configured | `check_device_changed()` returns at `mc_protocol_device.cpp:721-723`, **before** the `emit valueChanged` at `:743-745`. `deviceMChanged` at `:714` still fires | **A1.** On an M-only PLC every `bExecuteTrigger` is dropped while the device widget looks perfectly alive |
| The same function walks two `std::map`s in lockstep | `:706-718`, `:729-741`; `update_last_*_map()` (`:750-772`) only ever **adds** | **A1**, same edit (59.3) |
| `mc_frame_test` does **not** compile `mc_protocol_device.cpp` | `tests/mc_frame_test/mc_frame_test.pro` lists `mc_device_map.cpp`, the three frame codecs and `app_logger.cpp` | A1's helper must be **header-only**; a device-level test would need a `McMsgInterface` seam that does not exist |
| `activeCameraWorkspace` is written in exactly one place | `localization_runtime_controller.cpp:252-254`, inside `setActiveCameraNumber()`'s `if (newRunner)` branch. `buildRuntimeContext()` never fills it; `setup()` never derives it | **A2.** Read at `:1057` (crop offset) and `:1592` (matcher payload) — condition-ROI filtering is off for the whole session |
| The contract fixture declares **no** camera workspaces | `LocalizationRuntimeFixture::config()` (`main.cpp:302-320`) sets 14 tags and nothing else | A2 causes **no churn** in existing cycle tests; `cameraWorkspace()` returns the same default it does today |
| `clearRoleContext()` blanket-disconnects | `:709-721`. Safe today only by line order: `m_plcValueConnection` remade at `:412`, `bindFixedRoleRunners()` at `:422` | **A3** |
| `setup()` assigns both active indices as raw members | `:423-431`. Every range/registration gate lives only in the setters (`:170`, `:192`, `:277`) | **C2** |
| `buildRuntimeContext()` hard-codes `firstKey()` for both indices | `task_localization.cpp:724-726`, `:735-737` | **The gates can never fire from the only production caller** — `firstKey()` of a binding map is always registered. C2's validators are defence-in-depth; C2's *log line* is what the cell gets |
| Neither index is announced on the ready path | `publishInitialReadyOutputs()` `:837-850` publishes ten signals and neither index | **C3.** "Nobody wrote anything" and "0 was commanded" are indistinguishable on the register and on the monitor row |
| The runtime **does** echo onto the command registers today | `publishNumberSignal("nActiveCamera", …)` at `:228`; `publishNumberSignal("nActivePatternGroup", …)` at `:309` | **C3** removes both, per D1. No test asserts a write to `D100`/`D101`, and `main.cpp:2876` reads the *input* echo from `handlePlcValues` (`:508`), so it stays green |
| `main.cpp:3069` is **not** a `setup()` assertion | It is inside `test_localization_runtime_rejects_camera_change_while_running` (`:3036`): trigger a cycle, call `setActiveCameraNumber(2)` **while Running**, assert nothing was published. `lastSignalValue()` (`:395-404`) returns an invalid `QVariant` for an absent signal, so `.toInt()==0` means "not emitted" | **Do not touch it.** Under D1 it stays green untouched. An earlier draft prescribed rewriting it; that would blunt a live mid-cycle-rejection guard |
| `m_lastErrorReset` is never reset by `setup()` | `:373` resets `m_lastExecuteTrigger`; `m_lastErrorReset` appears only at `:577-578` | **C4** folds in the one-line fix; recorded in item 54 as *"that part still stands"* |
| The four active-index entry points are dead | `onSignalChangeCameraNumber` / `onSignalChangePatternNumber` (`task_localization.cpp:471-497`) have no `connect()` site, **and** `setCameraNumber()` (`:426`) / `setPatternNumber()` (`:458`) have no caller outside those two slots — verified across `src\`, `app\`, `runtime_app\`, `tests\` | **C5** deletes all four. Deleting only the slots leaves two public methods reachable by name |
| All 14 signals are mappable and validated by name | `kSignalFields[]` `localization_signal_mapper.cpp:15-30`; every PLC device implements `IPlcTagProvider` (`device_capabilities.h:44-50`); `IDeviceRunner::device()` is public (`idevice_runner.h:78`) | **C4**'s orphan check needs no new interface and no new context field |
| The gate does not break the suite or the commissioned projects | Fixture maps `M10..M19` / `D100..D103`; `VirtualPlcDevice` offers 128 M + 128 D (`virtual_plc_config.h:85-90`). `build\Huayan.vproj` and `build\virtual.vproj` map **all 14** signals; `Release\Project files\4E_Line.vproj` maps none **and has no device bindings**, so it already fails setup today | **C4** ships with the audit re-run as an acceptance criterion |
| `VisionTcpipDeviceBase::deviceConnect()` returns true silently when already active | `:33-40`; `syncRuntimeState()` (`:610-618`) touches no `ConnectStatus` | **D1.** `VisionOutputRunner::m_busy` is cleared **only** by `onConnectStatusChanged` (`:104-107`) **or** `onConnectionFailed` (`:111-114`) — the early return emits neither, so the wedge is permanent and every scheduled reconnect becomes a no-op |
| The server never re-announces `Connected` | `VisionTcpipDevice::startTransport()` publishes it once (`vision_tcpip_device.cpp:113`), when the listeners come up — **with no client attached**. `attachMainSocket()`/`attachHeartbeatSocket()` (`:306-345`) publish nothing | **D1.** "Connected" already means "listening" for the server; the fix republishes that same predicate, it does not redefine it |
| The graceful close races its own notice | `sendDisconnectNotice()` (`:514-531`) writes + `waitForBytesWritten(150)`, then `detachHeartbeatSocket()` (`:365-375`) **aborts** — RST discards the peer's unread buffer. `detachMainSocket()` (`:351-360`) aborts the RESULT socket unflushed | **D2.** The flaky test exists **in duplicate**: `vision_output_device_test/main.cpp:217` and `vision_tcpip_client_device_test/main.cpp:231`. Item 32 names only the second |
| No device anywhere emits `IDevice::errorOccurred` | Zero `emit errorOccurred` under `src/device/`; the only producer is `PlcRunner` itself (`plc_runner.h:193`, `:200`) | **B1** adds the missing `VisionOutputRunner` forward and marks it **dormant** at the line, so its presence is not read as coverage |
| "true" does not mean "written" on two families | MC `:240`, `:263` return `pushRequest()` = queued. `ModbusTcpClientDevice::writeDigitalIoByName` returns true for a write parked in `m_pendingIoWrites` (`:648-651`) — deliberate, and documented at `:640-647` | **E1 + E2.** D3's precondition is not MC-only |
| `McProtocolDevice::requestFinished(McResult)` is declared and never emitted | `mc_protocol_device.h:175`; no emit site in the repository | **E1** — the seam already exists as a dead signal |
| Robot pick check is inert on a Modbus binding | `modbus_tcp_{client,server}_device.h` hold `m_kinematicCheck` + a setter called only from `tests/modbus_device_test/main.cpp:1126-1127`; no `toJson`/`fromJson`, no widget. `RobotKinematicCheckWidget` (517 lines, model-free) is hosted only by the two vision-output widgets | **F1.** Item 57 is filed as a *verification* item and is a **missing-feature** item |
| A wrong preset fails **closed**, a missing checker fails **open** | `m_robotValid = buildRobotConfig(...)` (`robot_kinematic_picking_checker.cpp:149`) is private with no accessor; `isPickable()` (`:195`) returns false for everything when invalid. `rebuildPickingChecker()` (`:991-1004`) leaves the checker **null** when disabled or uncalibrated | **F1**'s guard needs an `isReady()` accessor; the uncalibrated branch is already caught by `validateActiveCameraCalibration()` |
| Dashboard lamps are wired before runners exist | `runtime_shell_window.cpp:472` constructs the dashboard; `beginRuntime()` is nine lines later at `:481`. `wireConnectionLamp()` (`:299-330`) resolves `runnerFor(id)`, gets null, sets "—", never retries | **F2** |
| `bErrorReset` has no monitor row | `kSignalRows[]` (`localization_dashboard_widget.cpp:64-80`) lists 13 signals; `typeOf()` (`:98-103`) falls back to `Number` and the monitor logs a USER-level warn per acknowledge | **C3** (one table edit, done with the two new rows) |
| `checkEmpty()` **clears** orphan tags | `signals_map_widget.cpp:279-292` blanks every warning-flagged row and returns the now-empty names | **C4** must classify required vs optional **before** purging, or the commissioning flow converts an orphaned `bTaskReady` into an unmapped required signal and bricks the next start |
| `kSchemaVersion` is 2 | `task_localization_config.h:150` | **C3** takes it to 3 and **F1** to 4. See the rollback note in Risks |
| A32's recorded status is false | `architecture_improvement_todo.md` A32 claims *"retry scheduling and fault escalation"*; escalation was removed when reconnect became unbounded | **Z2** |

---

## Rules that apply to every task in this phase

Stated once rather than repeated in twenty acceptance lists.

- **Sizes** are XS/S/M/L. **Anything L carries a written split point** — a real halving, not a scope
  fence. No task touches more than ~5 source files; the test file is named but does not count.
- **Every new or changed test is negative-checked**: reintroduce the defect, confirm the suite goes
  red **on the named cases**, confirm the named cases that must **stay green** do,
  restore, re-run green. Each task states its own injection and both lists. *A negative check where
  everything goes red is not a check* — F4's first negative check in Phase 8 passed for the wrong
  reason (`phase_8_implementation_plan.md:2027-2031`), which is why both halves are required.
- **No backwards-compatibility shims** (`AGENT.md`). New JSON keys are added; no existing key
  changes meaning.
- **A new config property with a display name carries its `kDisplayNameSources[]` entry in the same
  edit**, context spelled exactly as `staticMetaObject.className()`, and `QCOMPARE(totalNames, …)`
  (`main.cpp:4210`) raised in the same commit. A missed marker is a label that works in English and
  can never be translated, with no build error.
- **No new hard-coded machine paths.**
- **Nothing is marked verified without build/test/manual evidence.** Items that can only be
  confirmed by a person are marked **OWNER-RUN** — there is no widget-level test framework in this
  project and this phase does not build one.
- **`qmake` before `nmake`, always**, and check for 0-byte `.obj` files after an interrupted build
  (`MEMORY.md`): a 0-byte object links clean and behaves as if the translation unit were empty.
- PowerShell 5.1: `&&` is a parse error. Commands below are one per line.

### Verification commands (used verbatim; referenced by name below)

**"umbrella build"**
```powershell
cd C:\DGB\Project\ncr_picking\build\all\Release
qmake -o Makefile ..\..\..\ncr_picking_all.pro -spec win32-msvc "CONFIG+=release" "CONFIG+=multicore"
nmake /nologo
```

**"contract test"** — expected result stated per task as an absolute count against the 101 baseline.
```powershell
cd C:\DGB\Project\ncr_picking\build\architecture_contract_test
qmake -o Makefile ..\..\tests\architecture_contract_test\architecture_contract_test.pro -spec win32-msvc "CONFIG+=release"
nmake /nologo -f Makefile.Release compiler_moc_source_make_all
nmake /nologo
$env:QT_QPA_PLATFORM = 'minimal'
cd release
.\architecture_contract_test.exe -o results.txt,txt
Select-String -Path results.txt -Pattern 'FAIL!|Totals'
```

**"device suite `<name>`"** — `mc_frame_test`, `modbus_device_test`, `vision_output_device_test`,
`vision_tcpip_client_device_test`. All four `#include "main.moc"`, so the moc target runs first.
```powershell
cd C:\DGB\Project\ncr_picking\build\tests\<name>
qmake -o Makefile ..\..\..\tests\<name>\<name>.pro -spec win32-msvc "CONFIG+=release"
nmake /nologo -f Makefile.Release compiler_moc_source_make_all
nmake /nologo
$env:QT_QPA_PLATFORM = 'minimal'
cd release
.\<name>.exe -o results.txt,txt
Select-String -Path results.txt -Pattern 'FAIL!|Totals'
```

**"translation sweep"**
```powershell
cd C:\DGB\Project\ncr_picking
.\scripts\update_translations.ps1
```
Read `finished` / `unfinished` / `vanished` separately, never as one total. **0 newly vanished.**

> Close both shells before running the contract suite: `test_both_shells_take_the_same_instance_key`
> takes the real product lock and is flaky on back-to-back runs (`build_and_verification.md:405-414`).

---

## Known test breakage — expected, sanctioned, and never suppressed

Nothing else in the suite may change. If a number moves that is not on this list, **stop and find
out why**; do not re-baseline.

| # | Test | Task | What changes, and why |
|---|---|---|---|
| 3 | `QCOMPARE(totalNames, 122)` (`main.cpp:4210`) | **C3** → 124, **F1** → re-checked | Two new display names in C3. F1 adds a plain member with accessors, **not** a `Q_PROPERTY`, so F1 asserts the total is **unchanged** — an accidental `P_PROPERTY_*` then fails the build |
| 4 | `test_localization_recovery_policy_defaults_match_runtime_spec` (`main.cpp:2195`) | **Z1** | One assertion removed with the field. The case itself stays and still asserts the surviving defaults; the case **count does not drop** |
| 5 | `test_disconnect_notice_on_graceful_close`, both copies | **D2** | Goes from ~2-in-3 failing (`later_todo_list.md:1119`) to 20 consecutive green runs in **both** suites |

> **Rows 1 and 2 were deleted on 2026-09-08.** They listed
> `test_localization_runtime_setup_faults_on_invalid_pattern_group` and the camera/latch cases at
> `main.cpp:2655-2740` as sanctioned breakage under the since-deleted Task C1. Under **D7** the
> Phase 8 fault behaviour stands, so **none of those tests changes.** They are now the strongest
> guard the phase has: if C2's extraction is behaviour-neutral, they stay green untouched, and if
> any of them needs an edit, the extraction was wrong. Row numbering is left with the gap.

**Explicitly not breakage, and not to be "fixed":**
- **Every Phase 8 index-rejection and rejection-latch case** (`main.cpp:2655-2740` and siblings) —
  D7 keeps that contract intact. An edit to any of them is a defect in this phase's work, not a
  sanctioned update.
- `main.cpp:3069` — a mid-cycle rejection guard, green before and after C3 (see Current State).
- `main.cpp:2876` — reads the input echo from `handlePlcValues`, not the removed output publish.
- Every existing cycle test — A2 changes no matcher input for them, because the fixture config
  declares no camera workspaces.

---

## Phase A — the two silent wrong-result defects

### Task A1: Reproduce, then fix, the MC value-publish drop (59.3b + 59.3)

**Description.** Extract the M/D shadow-map diff out of `McProtocolDevice::check_device_changed()`
into a header-only, device-free helper — `src/device/plc/mc_device_map_diff.h`,
`template <typename T> QMap<QString,QVariant> diffDeviceMap(live, shadow, prefix, perAddressCallback)`
— that iterates **by key**, not by lockstep iterator pair. `check_device_changed()` then calls it
twice and emits `valueChanged` **once, unconditionally**, after both; the early return at `:721-723`
disappears and the per-address emissions move into the callback so their behaviour is unchanged.

The helper must be header-only: `mc_frame_test.pro` compiles four `src/` files and
`mc_protocol_device.cpp` is not one of them, and the device's transport (`std::unique_ptr<McMsgInterface>`,
constructed internally) has no injection point. **Write the failing test first, against the helper.**

**Acceptance criteria**
- [ ] An M-only device map (empty D map, exactly what `update_d_map()` produces for a context with
      no D ranges, `:661-664`) yields the M changes in the returned map. Today's shape returns nothing.
- [ ] A shadow with **more** keys than the live map returns correct results and reads past neither
      end. `update_last_*_map()` can only add, so this state is unrepairable once entered — the
      helper must not care.
- [ ] A shadow with **fewer** keys reports the missing addresses as changes and seeds them.
- [ ] `is_first_time_polling` suppression is preserved exactly: no changes reported on the first
      round, shadows still updated. **This is deliberately not "fixed" here** — it is the mechanism
      Q2 depends on and it belongs to G1/G2.
- [ ] `deviceMChanged` / `deviceDChanged` still fire once per changed address, same
      `(address, oldValue, newValue)` arguments, in address order.
- [ ] `valueChanged` is emitted exactly once per polling round, and only when the accumulated map
      is non-empty.
- [ ] The three C-frame codecs are untouched.

**Verification**
- [ ] Umbrella build clean.
- [ ] Device suite `mc_frame_test`: **38 → 43+ passed / 0 failed**.
- [ ] **Negative check.** Restore `if (device_map_d.empty()) return;` → the M-only case goes red and
      the D-only case stays green. Restore the lockstep `++it_new, ++it_last` loop → the two
      size-mismatch cases go red and the equal-size case stays green. Revert both, re-run green.
- [ ] **OWNER-RUN, deferred to Checkpoint Z, and optional:** on a real M-only MC PLC
      (`amountDAddress = 0`), a `bExecuteTrigger` change reaches the task. **If no such station
      exists, say so** — this task then ships on unit evidence and must not be recorded as
      hardware-verified.

**Recorded coverage gap, not coverage:** nothing in this repository has ever executed
`check_device_changed()`, `response_handle()` or any other `McProtocolDevice` method. The honest
proof available at S size is a pure function with a real test. A fake `McMsgInterface` is the only
way to test the transport state machine; it is an M-to-L task of its own and is **out of scope**.

**Dependencies:** none — this task shares no file with any other in the phase, so it may run in
parallel with Phase A/B if a second pair of hands exists (it writes to a different test binary,
which is what makes it genuinely parallel).
**Files:** `src/device/plc/mc_device_map_diff.h` (new), `src/device/plc/mc_protocol_device.cpp`,
`src/device/device.pri`, `tests/mc_frame_test/mc_frame_test.pro`, `tests/mc_frame_test/main.cpp`
**Size:** S

---

### Task A2: Populate the active camera workspace at setup (59.2)

**Description.** In `setup()`, immediately after `bindActiveCameraRole()` (`:427`), resolve the
active camera's workspace from `m_config.cameraWorkspace(runner->device()->id())` into both
`m_context.activeCameraWorkspace` and `m_activeCameraWorkspace` — the same two assignments the
setter makes at `:252-254` — through **one** helper both paths call, so they cannot drift. Guard
against a null runner exactly as `:248-255` does.

**Why this is first in the phase.** `LocalizationPipeline::runMatchOn()` passes
`workspace.useConditionWorkspace` straight into `ImageMatcher::matching()`; with it false,
`outSideConditionRoiCheck()` is never called, so **every** object keeps
`m_isOutsideConditionRoi == false` and `buildVisionOutputPositions()` (`:1106`) admits it. With
`useWorkspace` false the matcher also searches the whole frame instead of the commissioned ROI.
Objects the commissioning engineer fenced out are converted to robot coordinates and sent as pick
targets, on the first cycle of every session, with nothing logged and every lamp green.

**Acceptance criteria**
- [ ] After `setup()` on a config carrying a workspace for the active camera,
      `runtimeMatchingRequested` carries that workspace — asserted on the signal's `CameraWorkspace`
      argument, which every existing fixture discards.
- [ ] `buildVisionOutputPositions()` applies `matchResult.cropOffsetPoint` on the first cycle after
      startup, not only after a camera change.
- [ ] A camera number with no runner leaves both members default and dereferences nothing.
- [ ] The two members are assigned together, in the helper; neither `setup()` nor the setter
      assigns one without the other.
- [ ] The condition-ROI filter itself is **unchanged**. This task supplies correct input; it does
      not touch the filter.
- [ ] **This task must not move the `m_plcValueConnection` assignment at `:412` or
      `bindFixedRoleRunners()` at `:422`.** A3 has not landed yet, so that line order is still
      load-bearing.

**Verification**
- [ ] Umbrella build clean.
- [ ] Contract test **101 → 103**: `test_setup_applies_the_active_camera_workspace_before_the_first_cycle`
      (a config with a condition ROI that excludes a known match; one cycle straight after `setup()`
      with **no** camera-change command; the excluded object is not in the sent positions), and
      `test_a_camera_with_no_workspace_keeps_the_default_and_warns_nothing`.
- [ ] **Negative check.** Delete the new `setup()`-side assignment → the startup case goes red and
      the after-camera-change behaviour stays green. That asymmetry **is** the defect. Restore.
- [ ] **No churn is expected in existing cycle tests** — the fixture config declares no camera
      workspaces, so `cameraWorkspace()` returns the same default it does today. If any existing
      number moves anyway, it is **read and re-justified in the plan, never re-baselined**.
- [ ] **OWNER-RUN, Checkpoint A:** on a project with a commissioned condition ROI, start the runtime
      and trigger once **without** commanding a camera change; an object outside the fence is
      reported skipped, not sent.

**Dependencies:** none
**Files:** `src/model/localization_runtime_controller.{h,cpp}`, `tests/architecture_contract_test/main.cpp`
**Size:** S

---

### Task A3: `clearRoleContext()` disconnects only what it wired (item 53)

**Description.** Replace the blanket `disconnect(context.runner, nullptr, this, nullptr)` at `:718`
with disconnection of exactly the two connections `bindRoleContext()` made for that role
(`:679-704`), by storing their `QMetaObject::Connection` handles in `RoleRecoveryContext`. For a
device serving both `primary_plc` and `vision_output` — the dual-role Modbus binding Phase 8/B1 made
possible — clearing either role currently drops `m_plcValueConnection`, and with it every
`bExecuteTrigger`, `bErrorReset` and index write, **with `bTaskReady` still true**. This lands before
every Phase C task that rewrites the `:412`–`:431` window.

**Acceptance criteria**
- [ ] `RoleRecoveryContext` carries `statusConnection` and `errorConnection`; `clearRoleContext()`
      disconnects only those and removes the context. No `disconnect(obj, nullptr, this, nullptr)`
      remains in the file.
- [ ] `resetRuntimeBindings()` (`:593-610`) is unchanged: its explicit `disconnect(m_plcValueConnection)`
      at `:598` stays the one place the value stream is dropped deliberately.
- [ ] The `Qt::UniqueConnection` flags stay; binding the same runner twice still yields one
      connection each, and two `setup()` calls in a row leave exactly one live connection per role.
- [ ] Rebinding the Camera role (`setActiveCameraNumber(2)`) leaves `m_plcValueConnection` intact —
      a PLC input injected afterwards still reaches `handlePlcValues()`.
- [ ] A comment records **why** the blanket form was wrong, naming the dual-role case, so nobody
      simplifies it back.

**Verification**
- [ ] Umbrella build clean; contract test **103 → 105**:
      `test_a_camera_rebind_does_not_drop_the_plc_input_stream` and
      `test_the_plc_value_stream_survives_binding_the_fixed_roles`.
- [x] **Negative check run 2026-09-08, and the plan's prediction was WRONG.** Both parts were
      executed. (b) blanket `disconnect` alone, line order intact → both cases **green**, as
      predicted. (a) blanket `disconnect` **and** the `m_plcValueConnection` assignment moved below
      `bindFixedRoleRunners()` → both cases **green as well**. The plan expected red.

      > **Neither test pins item 53, and the tick above must not be read as coverage.**
      > `clearRoleContext()` only ever runs with a populated context on the **Camera** role (via
      > `bindActiveCameraRole`), and a camera runner is a different `QObject` from the PLC runner
      > that carries `m_plcValueConnection` — so the blanket form has nothing of the value stream
      > to take. Every other path reaches it through `resetRuntimeBindings()`, which has already
      > emptied `m_recoveryContexts`, so it returns before disconnecting anything.
      >
      > The fix is defensive and reasoned; it is **not test-proven**. The two cases are kept as
      > value-stream regression guards and are labelled in the source as not covering item 53.
- [x] **Reachability traced 2026-09-08, and it closes O-1 rather than answering it.** The first
      reading — "reachable only with one device on both roles" — was still too generous. The two
      fixed roles have exactly **one** bind site (`bindFixedRoleRunners()`, reached only from
      `setup():441`, which runs `resetRuntimeBindings():399` first, so both contexts are already
      empty and `clearRoleContext()` returns at `:759`) and **one** clear site
      (`resetRuntimeBindings():635-637`, where `disconnect(m_plcValueConnection)` at `:626` has
      already dropped the value stream two lines earlier). The dual-role Modbus binding therefore
      does **not** make item 53 reachable either.

      > **Consequence for the phase: O-1 is withdrawn, not deferred.** Giving `VirtualPlcDevice`
      > `IResultOutputDevice` would have bought a fixture for a defect that has no live path, at
      > the cost of deliberately breaking two passing tests. Item 53 is recorded as
      > **fixed-by-reasoning**, with the re-open trigger written into the backlog: a *second* bind
      > site for a fixed role — which Phase E and Task F1 may both introduce. That is the moment
      > the hazard becomes real, and it is a code-review trigger, not a bench test.
- [ ] Device suite `modbus_device_test`: 22 / 0, unchanged.
- [x] **OWNER-RUN, Checkpoint A — satisfied by the 2026-09-08 field run, evidence from the log.**
      On the dual-role Modbus **server** binding (`primary_plc` and `vision_output` both device
      `01`, `app_log_2026-09-08.txt:80-81`), the camera changed at `:571-581`
      (`HR00000` 5 → 1, *"Faulted -> Ready (Active camera changed.)"*), and the next PLC input
      arrived and was acted on seven seconds later at `:592-596` (`COIL00000=true` →
      *"Ready -> RunningCycle"*). The value stream survived the rebind.

      > Recorded as a **regression check on A3**, not as evidence for item 53 — per the
      > reachability finding above, this sequence passes on the unfixed build too.

**Dependencies:** A2 (same file; A2 first, by the harm rule)
**Files:** `src/model/localization_runtime_controller.{h,cpp}`, `tests/architecture_contract_test/main.cpp`
**Size:** S

---

### Checkpoint A — a correctly commissioned cell no longer mis-picks silently

- [x] Umbrella build clean; both shells relink (`ncr_picking.exe` / `ncr_runtime.exe`, 2026-09-08 10:23).
- [x] All suites **re-run, none assumed**, 2026-09-08. Contract **105 / 0**;
      `mc_frame_test` **44 / 0**; `modbus_device_test` **22 / 0**;
      `vision_output_device_test` **9** with a flake; `vision_tcpip_client_device_test` **10**
      with the same flake. The counts match this checkpoint's prediction.

      > **Reaching those two numbers took a build fix, and the first reading was wrong.** The two
      > vision suites initially ran **8 / 0** and **9 / 0** — one short each, and with no flake, which
      > looked like good news. It was not: `test_disconnect_notice_on_graceful_close` was **absent
      > from both binaries**. `-functions` listed 6 and 7 where the sources define 7 and 8, and the
      > run reported `0 skipped, 0 blacklisted` — the test was not failing, it did not exist.
      >
      > Cause: a stale `main.moc` at each build-dir **root**, dated 2026-06-03, predating the test.
      > `main.cpp` does `#include "main.moc"`, and qmake resolved that include against the physical
      > stale file — writing the bare path into the Makefile's dependencies, which kept the file
      > required, which kept it shadowing. Self-perpetuating. **Fixed:** delete the root copy, then
      > re-run qmake (that order), then force one recompile. Verified in both dirs; no source or
      > `.pro` change was needed. Full detail in **backlog 62**.
      >
      > With the test restored, both binaries register it and it fails as item 32 describes — over
      > ten runs each, `vision_output_device_test` **4** and `vision_tcpip_client_device_test`
      > **6**, with the rate swinging between batches. **Task D2 should drive from the client**,
      > and should not read a short green batch as progress.
      >
      > Phase A changed neither suite. Nothing in Phase A's result depends on this; it is recorded
      > here because this checkpoint claims the suites were re-run, and an honest re-run had to
      > account for a test that silently was not there.
- [x] Each negative check recorded individually, including A3's part (b), which stayed green.
      A3's part (a) **also** stayed green against the plan's prediction — recorded in full under
      Task A3 rather than smoothed over here.
- [x] **OWNER-RUN, 2026-09-08:** one cycle with an object outside the condition ROI — **skipped,
      not sent**. Confirmed by the owner on the dual-role Modbus cell. This is the check A2 exists
      for, and it is the one that had no automated equivalent.
- [x] **OWNER-RUN:** PLC inputs survive a camera change on the dual-role Modbus binding (A3) —
      satisfied from the field log; see Task A3 for the cited sequence and for why it is a
      regression check rather than evidence for item 53.
- [x] *(Resolved 2026-09-08 — no ruling outstanding here. **D7** keeps an invalid index a fault and
      **D8** adds Task C6. Phase C proceeds as written.)*

**Checkpoint A CLOSED 2026-09-08.**

> **Three defects were found by the same field run and none of them is Phase A's.** All three are
> recorded rather than fixed here, per the rule against widening a phase mid-flight. Two land on
> tasks that already exist and are now field-confirmed rather than inferred — **C3** (the
> active-index echo, backlog 60) and **F2** (the dead dashboard lamps, backlog 61). The third is a
> **second, independent cause** behind F2's symptom that F2 as written would not have fixed; F2 has
> been amended. Phase B is unaffected and remains next.

> What this checkpoint does **not** retire: a cell configured wrongly (C4), a cell whose master
> commands a camera other than `firstKey()` (C2/C3), a degraded PLC link (Phase E), or a
> vision-output link that blipped (Phase D). A green checkpoint here is not "the runtime is safe".

---

## Phase B — the harness the defects walked through

### Task B1: Pin the runner → controller liveness forwards, and add the missing one

**Description.** Add `PlcLost` and `VisionOutputLost` cases to the existing controller fixture
(`main.cpp:238-393`), mirroring `test_localization_runtime_camera_loss_faults_running_cycle`
(`:3195`) — no test pins those two forwards today. Add the missing `errorOccurred` forward in
`VisionOutputRunner::wireSignals()` (`vision_output_runner.h:79-92`); `PlcRunner` has it at `:228-229`.

**Be honest about that one line.** No device in `src/device/` emits `IDevice::errorOccurred` — the
only producer is `PlcRunner` itself. The forward is correct and costs one line, but it is **dormant**
until item 51's other half lands. Say so at the line, so its presence is never read as coverage.

**Acceptance criteria**
- [ ] Losing the PLC during `Running` aborts the cycle with `PlcLost` (300) and publishes
      `bTaskFault`/`nFaultCode`; losing the vision-output device aborts with `VisionOutputLost` (200).
- [ ] Both roles, lost **outside** a cycle, withdraw `bTaskReady` without raising `bTaskFault`, and
      re-arm on reconnect — the documented unbounded-retry contract (`plc_signal_contract.md:303-325`),
      which no test currently states.
- [ ] `VisionOutputRunner` forwards `errorOccurred`, with a comment naming item 51 and stating that
      nothing emits it yet.
- [ ] One test asserts the forward exists for all three runner families in one place, so the next
      runner cannot omit it quietly.
- [ ] Every new case asserts **behaviour**, not that `connect()` was called.

**Verification**
- [x] Umbrella build clean (2026-09-08 14:19, both shells relinked); contract test **105 → 110**,
      0 failed. The five cases are the two in-cycle losses, the two out-of-cycle
      withdraw-and-re-arm cases, and the cross-family forward case.
- [x] **Negative check (a) — the `errorOccurred` forward.** Observed as the ordinary TDD red step
      rather than staged afterwards: before the one-line connect existed the run was **109 passed /
      1 failed**, the single failure being
      `test_every_runner_family_forwards_device_errors_to_the_controller`, with the four loss cases
      green. Adding the line took it to 110 / 0.
- [x] **Negative check (b) — `handleRoleStatusChanged` ignoring `RunnerRole::PrimaryPlc`.** Applied
      as an early `return` at the top of the function → **108 passed / 2 failed**, and the two
      failures were exactly `..._plc_loss_faults_running_cycle` and
      `..._plc_loss_outside_a_cycle_withdraws_ready_without_fault`. The camera case and both
      vision-output cases stayed green, so the four new cases discriminate by role rather than
      passing on any status change. Restored; re-verified 110 / 0.

> **PLAN DEVIATION — the Files list below was short, and the task could not be done as written.**
> The two loss cases need a hardware-free way to drive a device's `ConnectStatus`, and
> `forceConnectionStatus()` existed on **`VirtualCameraDevice` only**. Calling
> `setConnectionStatus()` from the test thread was rejected as the alternative: the devices live on
> their runners' worker threads, so that emits `connectStatusChanged` off-thread and produces an
> ordering the hardware cannot — which is the documented reason the camera's version is a queued
> wrapper in the first place. The same wrapper was therefore added to `VirtualPlcDevice` and
> `VirtualVisionOutputDevice`, mirroring the camera's implementation and its comment. Test-harness
> capability on virtual devices only; no product behaviour changed.

**Dependencies:** Checkpoint A
**Files:** `src/runtime/vision_output_runner.h`, `tests/architecture_contract_test/main.cpp`,
**plus (deviation above)** `src/device/virtual/virtual_plc_device.{h,cpp}`,
`src/device/virtual/virtual_vision_output_device.{h,cpp}`
**Size:** S

---

### Task B2: A `TaskLocalization`-level fixture — the hop that has never been tested

**Description.** Build `TaskLocalizationRuntimeFixture` in the contract test: a `Project` +
`DeviceManager` holding a `VirtualPlcDevice`, two `VirtualCameraDevice`s and a
`VirtualVisionOutputDevice`, a `TaskLocalization` bound to them, taken through `beginRuntime()` and
driven **only** by `PlcRunner::requestInjectInputValue()` — the shape
`test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs` (`:2822`) already
proves at controller level, lifted one layer up. Assert the forwards at `task_localization.cpp:593-606`.

**This is the missing test both of item 25's defects and item 58 walked through**, and it is the
harness F2 asserts against.

**Split point (this task is at the S/M boundary and gates F2).**
*(a)* construct the project + devices + task, `beginRuntime()`, assert `runtimeStarted()` fires
**after** the runners are registered and attached, and that a bindings-incomplete project leaves the
task `Faulted` with `isValid()` false (the `:194-198` branch, untested today).
*(b)* the full trigger → grab → match → send → trigger-reset cycle through the real `matchingRunner`
worker, plus the `signalChanged` / `cycleResultUpdated` / `taskLogAppended` forwards.
**If (b) overruns, land (a) and carry (b)** — and record it, because F2's only automatable assertion
lives in (a), and without (b) the hop assertions do not exist and F2 becomes fully OWNER-RUN.

**Acceptance criteria**
- [ ] *(a)* `runtimeStarted()` is observed after the runners exist; the incomplete-bindings project
      ends `Faulted` and `isValid()` false.
- [ ] *(b)* A full cycle runs through `TaskLocalization` with matching served by the real
      `matchingRunner` path — no GUI, no hardware.
- [ ] *(b)* Every signal the controller publishes arrives on `ITask::signalChanged` with the same
      name and value (the hop at `:593-594`); `taskLogAppended` delivers the cycle's INFO summary;
      `taskStateChanged` reaches `Ready → RunningCycle → Ready`.
- [ ] The fixture tears down cleanly under `QT_QPA_PLATFORM=minimal` with no leaked threads
      (`endRuntime()` then `stopAll()`).

**Verification**
- [x] Contract test **110 → 112 (a) → 114 (b)**, 0 failed. Both halves landed; nothing was carried.
      Run **three times back-to-back** with both shells closed: **114 / 0 each time**, no new flake.
- [x] **Negative check — and the contrast is the finding, not a formality.** Commenting out the
      `signalChanged` connect at `task_localization.cpp:593-594` gave **112 passed / 2 failed**: the
      only failures were the two new hop cases, and **every one of the 112 controller-level cases
      stayed green**. That is the gap B2 exists to close, measured rather than argued — the suite as
      it stood before this task could not see a severed forward at all. Restored; re-verified 114 / 0.

> **Two things B2(b) deliberately does not assert.** *(1) How many objects the matcher finds.* The
> pattern is a synthetic 8×8 field and the frame comes from a virtual camera; what the real matcher
> makes of that is not a contract, and pinning it would turn matcher tuning into a failure in a test
> about signal plumbing. The cycle completing and re-arming is the contract. *(2) The runner is
> reached through `TaskRunner::runnerFor()`, not through a probe.* `TaskLocalization::plcRunner()` is
> private; widening it with `using` would have tested an access path no caller has. The fixture
> resolves it the way a widget must.

> **One trap worth the line it costs.** `DeviceManager::reserveDevice()` validates id format and
> **silently refuses** anything that is not the product's own form (`"01"`, `"02"`, …). The first
> version of this fixture used `"plc1"`/`"cam1"`, every device was rejected, and the symptom was a
> runtime that entered *"Commission phase ( 0 devices)"* and simply never emitted `runtimeStarted()`.
> Nothing in the failure text pointed at the ids.

**Dependencies:** B1 (shares fixture edits)
**Files:** `tests/architecture_contract_test/main.cpp`, possibly
`tests/architecture_contract_test/architecture_contract_test.pro`
**Size:** M *(split as stated)*

---

### Checkpoint B — the phase can prove its own work

- [x] Umbrella build clean (2026-09-08 14:19); both shells relinked. Contract test **114 / 0**,
      three consecutive runs. B2(b) was **not** carried.
- [x] All four device suites re-run at baseline, **with a shape check on each** (backlog 62):
      `mc_frame_test` 42 fns / **44 passed**; `modbus_device_test` 20 / **22**;
      `vision_output_device_test` 7 / **9**; `vision_tcpip_client_device_test` 8 / **10**; all
      0 failed. Every registered-function count matches its source, so no suite is running short.
      The item-32 flake did not fire in this pass — that is luck, not a result; see backlog 62.
- [x] Every negative check recorded individually: B1(a) as the TDD red step (109/1, the single
      expected failure), B1(b) role-discriminating (108/2, exactly the two PLC cases), and B2's
      stay-green contrast (112/2, with all controller-level cases green).

**Checkpoint B CLOSED 2026-09-08.**

> **What this phase actually bought.** Five roles-and-liveness cases that did not exist, one real
> missing forward found and fixed, and — the point of B2 — a measured demonstration that the suite
> could not see a severed controller→task forward. F2's automatable half now exists, so F2 is no
> longer fully owner-run. Phase C is next and is unaffected by anything here.

---

## Phase C — the startup selection contract

> **One branch, strictly sequential, never parallelised.** Every task here edits `setup()`
> (`:365-472`), its bind helpers, or `buildRuntimeContext()`. Two agents on this region conflict on
> every merge and, worse, each reasons about a `setup()` the other has already changed.

> **Task C1 was deleted on 2026-09-08 (see D7).** It would have made an invalid index not-a-fault.
> The owner ruled the opposite: the Phase 8 behaviour stands. Task numbering is left with the gap
> rather than renumbered, so that references to C2–C6 written before this revision still resolve.

> ### OWNER FIELD RUN of C2/C3/C4, 2026-09-08 15:29–15:38 — passed, and found two defects
>
> Run on the real cell (Basler camera 02 + Modbus client 05) against the 15:10 shells. **The three
> tasks did what they were built to do**, all from `app_log_2026-09-08.txt`:
>
> | Evidence | Line |
> |---|---|
> | Startup summary, with the A2 workspace visibly live: `workspace: crop=on condition=on conditionRoi=(571.034,302.069 546.207x459.31)` | `:1432` |
> | **C2** registration message, distinct from calibration — from a real `IR00000=2` write: *"Ready -> Faulted (No camera is registered for number 2.)"* | `:1450-1451` |
> | **C2** range message with `MatchGroup`'s own bounds: *"Pattern group number 0 is outside the valid range 1..32."* | `:1495` |
> | Both re-armed with no operator action | `:1456`, `:1498` |
> | **C3** the two status signals warn when unmapped and do **not** fault; runtime went Ready with only one of them mapped | `:1430-1431`, `:1565-1566` |
> | **C4** the gate refused a real orphan: *"Signal \"Camera selection\" (nActiveCamera) is mapped to tag IR01000, which the device 05 does not provide as a register."* → Faulted | `:1552-1553` |
>
> That last line is C4's OWNER-RUN criterion met on hardware. **Two defects came with it, both in
> code written the same day, and both are now fixed and negative-checked.**
>
> **Defect 1 — the refusal message never reached the operator.** `setup()` logged every error at
> `LOG_DEV_ERR`. On the operator's screen the transition read only *"Runtime start aborted:
> setupTask failed"*, which names nothing. C4's own criterion says the runtime must refuse *"with a
> message naming the tag"* — it did, to the wrong audience. A refusal nobody can read is barely
> better than the silent start the gate replaced. Errors now go to `LOG_USER_ERR` **and** the task
> log at ERROR, so they reach the dashboard. Pinned by
> `test_setup_errors_reach_the_operator_log_naming_the_signal`; reverting to `LOG_DEV_ERR` → 130/1.
>
> **Defect 2 — the startup summary said "commanded" when nothing commanded anything.**
> `buildRuntimeContext()` pre-resolved both indices to `firstKey()` (`task_localization.cpp:654`,
> `:665`) before `setup()` could tell a project default from a commanded value, so
> `m_activeCameraFromProjectDefault` never became true and the summary's provenance was wrong on
> **every** production run. Provenance is the only thing separating *"the task is on camera 1"* from
> *"the master and the task agree"*, so a label that is always wrong is worse than no label. The
> sentinel is now left for `setup()` to resolve — it applies the identical `firstKey()` fallback, so
> the selected value is unchanged and only the provenance survives. **This is also the precondition
> C6 needs**: a value read from the live PLC cannot beat a default that has already been written in.
>
> > **The first version of that test passed its own negative check** — it asserted
> > `summary.contains("project default")`, and the summary carries a source for *each* index, so
> > restoring the camera half alone left the pattern-group half satisfying it. Re-cut as the
> > **absence** of `"commanded"`, it goes red on exactly that reintroduction (130/1). Recorded
> > because a test that cannot fail is the failure mode this project keeps finding.
>
> Contract test **129 → 131**. Shells rebuilt 2026-09-08 15:47 with both fixes.
>
> The summary also now appends to the **operator's task log**, not only the app log: which camera
> and group the runtime settled on, and whether anyone chose them, is what an operator checks when a
> cell picks from the wrong place.

> ### Phase C status, 2026-09-08 — PARTIALLY LANDED
>
> | Task | State | Contract test |
> |---|---|---|
> | **C2** validators + startup summary | ✅ landed, negative-checked, **field-run** | 114 → 118 |
> | **C3** status signals, echo deleted | ✅ landed, negative-checked (a,b,c), **field-run** | 118 → 122 |
> | **C4** signal-map gate + `m_lastErrorReset` | ✅ **both halves**, negative-checked (a,b,c + the purge trap), **field-run incl. its OWNER-RUN criterion** | 122 → 129, then → **133** |
> | **C5** delete the dead entry points | ✅ landed | unchanged at 122 |
> | Field-found fixes (see above) | ✅ landed, negative-checked | 129 → 131 |
> | **C6** live PLC read + ordering | ✅ **both halves**, negative-checked (a,b) | 133 → **142** |
>
> Umbrella build clean, both shells relinked 2026-09-08 16:50. Contract test **142 / 0**, twice.
> Device suites at baseline with shape checks: `mc_frame_test` 42/44, `modbus_device_test` 20/22,
> `vision_output_device_test` 7/9, `vision_tcpip_client_device_test` 8/10 (the item-32 flake fired
> in the client suite on this pass, as it does).
>
> **Every Phase C task has landed.** What Checkpoint C still needs is owner-run: the 2026-09-07
> symptom re-checked on the cell with the master's registers at 0, and the save dialog read in both
> themes and in Japanese. The symptom itself is now pinned hardware-free by
> `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready`.
>
> **C4's commissioning half, 2026-09-08.** `checkEmpty()` replaced by `orphanRowNames()` +
> `clearRowTags()`; save-time confirmation in `LocalizationSettingWidget`, forwarded by
> `LocalizationTaskWidget`, called from `MainWindow::saveToFile()` before anything is written.
> `SignalsMapWidget` is now compiled into the contract test (one SOURCES + one HEADERS line; it has
> no `.ui` and no project dependency beyond `app_logger`), which is what made the two safety cases
> possible at all. **Negative check:** restoring the blind clear inside `orphanRowNames()` turned
> **both** new cases red (131 / 2) and nothing else. Its own dialog remains OWNER-RUN — there is no
> widget test framework here and this is not claimed otherwise.
> Device suites at baseline with shape checks: `mc_frame_test` 42 fns / 44, `modbus_device_test`
> 20 / 22, `vision_output_device_test` 7 / 9, `vision_tcpip_client_device_test` 8 / 10 — the item-32
> flake fired in both vision suites on this pass, which is the known behaviour, not a regression.
>
> **What is NOT done, and why — neither is blocked, both are simply the large half.**
>
> 1. **C4's commissioning half.** Wiring `SignalsMapWidget::checkEmpty()` into the setting widget at
>    project save, with the confirmation dialog that refuses to purge a required row silently. It is
>    UI work whose only verification is OWNER-RUN (no widget test framework exists here), and it is
>    independent of the runtime gate that already landed.
> 2. **C6 in full.** Stopped deliberately at the design boundary rather than started and left half
>    done — the task itself says *"do not attempt in one pass"*, and its (b) half changes
>    `beginRuntime()`'s lifecycle.
>
> **C6(a) is smaller than the plan states, and its shape changed on inspection.** The plan says the
> accessor *"touches four device implementations"*. There are **two** concrete `PlcValueMap`s —
> `McDeviceMap` (`mc_device_map.h:85`) and `ModbusRegisterMap` (`modbus_register_map.h:147`).
> `VirtualPlcDevice` has **no value map at all**; it publishes through `valueChanged` only. So C6(a)
> is two implementations *plus a new requirement the plan did not carry*: the virtual PLC needs a
> snapshot source before C6's own hardware-free verification
> (*"the virtual PLC holds a selection before setup() runs"*) can exist. Size that in before
> starting.

### Task C2: One index validator, used by the setters **and** by `setup()`, plus the startup summary (item 55)

**Description.** The range + registration checks are currently **three duplicated inline blocks**,
not a function: `setActiveCameraNumber()` (starts `:154`) carries the out-of-range block at
`:170-189` and the not-registered block at `:191-214`, and `setActivePatternGroupNumber()` (starts
`:265`) carries the range block at `:269-288`. There is **no** `reportIndexRejection()` — an earlier
revision of this plan named one; it does not exist. Creating the helper is part of this task, not a
precondition of it.

Extract them into two private validators returning a small result (`accepted` / `outOfRange` /
`notRegistered`, plus the message). Both setters call them; `setup()` calls them too, before
assigning the members. A failure at setup produces a message that **names the number and the legal
range** — item 55 is that today an unregistered camera surfaces as *"Active camera calibration is
invalid."*, sending a commissioning engineer to the wrong screen.

**Preserve D7 exactly.** The extraction must be behaviour-neutral at the setter level: the fault
publish, the fault code, the `CycleState::Faulted` transition and the per-signal rejection latch all
stay. This task changes *where the check lives*, never *what a rejection does*.

Add the **startup summary**: one `LOG_USER_INFO` on the ready path naming the active camera number,
its device id, the active pattern group, the workspace state (on/off + ROI, from A2), and **where
each came from** — `"commanded"` vs `"project default (firstKey)"`.

**What this buys, honestly — and it changed on 2026-09-08.** Until D8, `buildRuntimeContext()` set
both indices from `cameraDeviceIds.firstKey()` / `patternGroups.firstKey()`
(`task_localization.cpp:724-737`), and `firstKey()` of a binding map is always registered — so the
setup-side validators could never fire from the only production caller, and the real deliverable was
the summary line. **C6 changes that**: once `setup()` resolves the selection from the live PLC, the
validators gate a master-written value that genuinely can be 0, 99, or unregistered. C2 is therefore
the precondition C6 needs, not defence-in-depth. Build it first and land C6 on top.

**Acceptance criteria**
- [ ] A context with `activeCameraNumber = 0` fails setup with an error naming `0` and the range
      `1..16`, **not** with a calibration message; an in-range but unregistered number fails naming
      registration, distinctly.
- [ ] The same for pattern groups, against `MatchGroup::validateIndexRange()`.
- [ ] Ranges still come from `TaskDeviceBinding::kMinCameraNumber`/`kMaxCameraNumber` and
      `MatchGroup`, never from a constant copied into the controller.
- [ ] `-1` still means "first available" and still resolves through `:424-431`. The code notes that
      `buildRuntimeContext()` already resolves it, so this fallback is dead for the production
      caller — recorded, not deleted, because the contract tests construct contexts directly.
- [ ] The summary line is emitted exactly once per `beginRuntime()`, and says "project default"
      when the number was not commanded.
- [ ] **D7 preserved.** A setter-level rejection still publishes `bTaskFault=true` + `nFaultCode`,
      still enters `Faulted`, still latches. Asserted by the *existing* Phase 8 cases staying green
      with no edit — if any of them needs changing, the extraction was not behaviour-neutral.

**Verification**
- [x] Contract test **114 → 118**, 0 failed (the plan's "101 → 105" was written against the draft
      baseline; the additive +4 is what holds). Four cases: camera out of range, camera in range but
      unregistered, pattern group out of range, and the `-1` default still resolving.
- [x] **Negative check — and it is exactly item 55.** Forcing both verdicts to `Accepted` in
      `setup()` gave **114 passed / 4 failed**: the three new setup cases plus the pattern-group
      case below, while **every setter-level case stayed green**, `test_localization_zero_active_index_is_refused_by_the_range_check`
      included. The gates existed at the setter and `setup()` never used them. Restored; 118 / 0.
- [x] Startup summary observed in the suite's own log:
      `Localization runtime selection. camera= 1 ( commanded ) cameraDeviceId= cam1 patternGroup= 1 …`

> **⚠️ ONE EXISTING CASE WAS EDITED, against this task's own "no edit" rule. Recorded rather than
> quietly done.** `test_localization_runtime_setup_faults_on_invalid_pattern_group` sets
> `activePatternGroupNumber = 99` and asserted the message *"Active pattern group is missing."*.
> 99 is outside `MatchGroup`'s 1..32, so with the validator in place it is now reported as out of
> range instead.
>
> **D7 is not what changed.** The three publishes (`bPatternValid` false, `bTaskFault` true,
> `nFaultCode` 400), the invalid `SetupResult` and the `Faulted` transition are all untouched, and
> that case still asserts every one of them. Only the diagnostic wording moved — which is the
> deliverable of this task, not a side effect: 99 is not a *missing* group, it is a number that can
> never name one, and "missing" sent a commissioning engineer looking for a group nobody ever
> defined. The genuinely-missing case (an in-range number with nothing bound to it) still runs
> `validateActivePatternGroup()` and still reports "missing".
>
> The "no edit" rule caught a real change and forced this judgement, which is what it is for.

> **Implementation note — the sentinel nearly broke a good message.** The validators run only when
> the number actually resolved. A still-negative value after the `firstKey()` fallback means the map
> was **empty**, which `setup()` already reports as a missing role binding; complaining that `-1` is
> out of range on top of that buries the real message under a derived one. The first cut did exactly
> that and turned the case above red for the wrong reason.

**Dependencies:** Checkpoint B
**Files:** `src/model/localization_runtime_controller.{h,cpp}`, `tests/architecture_contract_test/main.cpp`
**Size:** M *(raised from S on 2026-09-08: the helper must be created across three duplicated inline
blocks, not merely called, and C6 now depends on its verdict type)*

---

### Task C3: Status signals for the active selection, and no more command-register echo (D1 / item 58)

**Description.** Per D1, add `nActiveCameraStatus` and `nActivePatternGroupStatus` as **optional**
output signals carrying the index the runtime actually adopted: `TaskLocalizeConfigPrivate` members,
`P_PROPERTY_STRING_READWRITE` with display names **"Active camera (status)"** /
**"Active pattern group (status)"**, `kDisplayNameSources[]` entries **in the same edit**,
`toJson`/`fromJson` keys, `kSchemaVersion` 2 → 3, `kSignalFields[]` entries
(`localization_signal_mapper.cpp:15-30`), and publish calls in `publishInitialReadyOutputs()` and on
every **accepted** change in both setters. `publishNumberSignal()` already emits `signalChanged`
always and writes only when a tag is mapped, so **no special-casing**.

**Delete the two existing echoes** — `publishNumberSignal("nActiveCamera", …)` at `:228` and
`publishNumberSignal("nActivePatternGroup", …)` at `:309`. D1's "never echo onto the master's
command registers" cannot hold while those remain, and the acceptance criterion below cannot pass.
**This is a PLC-visible change** and carries a migration step (below).

> **Field-confirmed 2026-09-08 — this is no longer a design argument (backlog 60).** On a Modbus
> **client** binding whose command registers sit in the input-register area, the echo cannot even
> be attempted: a master may not write input registers, so every accepted selection change is
> rejected by the device and raised as a `runtimeError`.
>
> ```text
> app_log_2026-09-08.txt:739-752
> [11:12:19.483] Modbus values changed. deviceId= 05 count= 1 values= IR00000=1
> [11:12:19.485][ERR]  Modbus word write rejected: a master cannot write input registers.
>                      deviceId= 05 tag= IR00000
> [11:12:19.488][WARN] primary_plc   runtime error: PLC word write failed: IR00000
> [11:12:19.488][WARN] vision_output runtime error: PLC word write failed: IR00000
> ```
>
> Identically for `IR00001` at `:748-752`. The root is that `LocalizationSignalMapper` holds one
> **undirected** map, so nothing in the path distinguishes a command register from an output.
> **The client is where it is loud, not where it is worst:** on a server binding the same echo
> succeeds silently into a register the master owns, which is a write race against the master's
> own command. Deleting the echoes fixes both.
>
> The doubled `runtimeError` — one per role, because one device fills both — is recorded in
> backlog 60 and is not this task's to fix.

Add the three missing monitor rows while the same table is open: both status signals and
**`bErrorReset`**, whose absence from `kSignalRows[]` makes `typeOf()` fall back to `Number` and the
monitor log a USER-level warn on every acknowledge (item 25's second half).

**Migration, stated because a commissioned master may read those registers back.** In
`build\Huayan.vproj` the command registers are `HR00000`/`HR00001` — master-owned holding registers,
which is exactly why the echo is wrong. The order on a live cell is: map the two status tags → move
any master readback logic onto them → then upgrade. Z2 documents it; the OWNER-RUN item below
confirms it before the phase closes. Keeping the echo "for one release" is a
backwards-compatibility shim and `AGENT.md` forbids it.

**Acceptance criteria**
- [ ] With both status tags mapped, `setup()` publishes the resolved camera and group numbers to the
      PLC, and `nActiveCamera` / `nActivePatternGroup` are **never written** — asserted against
      `VirtualPlcDevice::wordWrites`, not against the `signalChanged` spy (`handlePlcValues` emits
      `signalChanged` for the *input* at `:508`, so a spy cannot tell a write from an echo).
- [ ] With both status tags **empty**, `signalChanged` fires for both and **no PLC write occurs at all**.
- [ ] Every accepted selection change republishes the status; a **refused** one does not.
- [ ] Both are published **before** `markRuntimeReady()`, so a dashboard reading the ready state
      already has the indices.
- [ ] `kSchemaVersion` 2 → 3 with a history line: *"3 — adds the two active-selection status tags. A
      v2 document loads with both unbound, which is a supported configuration."* A v3 document is
      refused by a v2-era build, which is the point of the bump.
- [ ] `kSignalRows[]` in `localization_dashboard_widget.cpp` (`:64-80`) **and** in
      `localization_setting_widget.cpp` gain both status rows as `Number`; the dashboard also gains
      `bErrorReset` as `Bool`. After this task no signal the controller publishes is missing from
      the monitor schema.
- [ ] **`applySignalToDashboard()` must handle the new names, or the camera lamp silently stops
      following the task.** `localization_dashboard_widget.cpp:489` matches the literal string
      `"nActiveCamera"` and, on a change, calls `rebuildConnectionWiring()` to re-resolve which
      camera device the lamp watches (`:492-495`); `:497` does the same for
      `nActivePatternGroup`. Add `nActiveCameraStatus` / `nActivePatternGroupStatus` to those
      branches.

      > The regression is narrower than it first looks, and the plan states the real shape so
      > nobody "fixes" the wrong half: `handlePlcValues()` emits `signalChanged` for the **input**
      > `nActiveCamera` (`:508`), so a master-written change still reaches the dashboard after the
      > echo is deleted. What breaks is the case where the runtime adopts a selection the PLC did
      > **not** write — the startup resolution C6 introduces, and any UI-driven change. Following
      > the status signal is the more correct behaviour anyway: the lamp should watch the camera
      > the task is actually using, not the one it was told to use.
- [ ] `QCOMPARE(totalNames, 122)` → **124**, in this commit.

**Verification**
- [x] Contract test **118 → 122**, 0 failed, with all four named cases. Display-name marker case
      green at **124**.
- [x] Nothing in the suite asserted a write to `D100`/`D101`, as predicted — the echo removal broke
      no existing case. The fixture config gained `D104`/`D105` as the status tags.
- [x] **Negative check (b) — the defect itself.** Re-adding `publishNumberSignal("nActiveCamera", …)`
      in the setter gave **121 / 1**: only `test_the_runtime_never_writes_the_command_registers`
      failed. Restored.
- [x] **Negative check (c).** Dropping the `"Active pattern group (status)"` marker gave **121 / 1**:
      only `test_every_display_name_has_a_translation_marker`. Restored.
- [x] **Negative check (a) — THE PLAN'S PREDICTION WAS WRONG, and the code is right.** Removing the
      status publishes gave **119 / 3**, not the predicted "announce red, optional-tag green":
      `test_status_signals_with_no_tag_emit_but_do_not_write` went red too, and it should have. That
      case asserts the status value still **reaches the UI** when no tag is mapped, so deleting the
      publish outright must break it — the plan's phrasing assumed only the PLC-write half would be
      removed. `test_a_refused_selection_is_not_announced` also went red because it establishes the
      announced value before refusing one. The discriminating half held: every case that does not
      touch the status signals stayed green. Restored; re-verified 122 / 0.

> **IMPLEMENTATION DEVIATION — the status is published from TWO sites, not one.** The plan put the
> publishes in `publishInitialReadyOutputs()` only, but that runs solely from `markRuntimeReady()`,
> which does not fire when `setup()` is valid and a device is still connecting. The acceptance
> criterion says **`setup()`** publishes the resolved selection, so it is published there too, on
> the valid path. A runtime that has chosen but is not yet ready has still chosen, and the master is
> entitled to read it. This is what made negative check (a) behave differently from the prediction.

> **`applySignalToDashboard()` matches BOTH names for each index** (`nActiveCamera` **or**
> `nActiveCameraStatus`), rather than moving to the status name alone. `handlePlcValues()` still
> emits `signalChanged` for the input, so a master-written change must keep moving the lamp
> immediately; the status name is what carries a selection the runtime adopted *without* being told.
> Matching one and not the other is the camera-lamp regression this task's own criteria warn about.
- [ ] **OWNER-RUN, Checkpoint C-1:** with both status registers mapped, the master reads back the
      camera and group the task actually selected at startup; the **command registers are untouched**
      (watch them from the PLC side); the dashboard shows both rows plus `bErrorReset`, with no
      USER-level warn on acknowledge.
- [ ] **OWNER-RUN, Checkpoint C-1:** with both tags left empty, the dashboard still shows the values
      and the PLC sees no write.

**Split point (six files — over the guideline).** *(a)* config + mapper + controller publish + echo
removal + tests; *(b)* the two `kSignalRows` tables **and** the `applySignalToDashboard()` branches.
**Never split the other way** — a row in the map UI for a signal that is never published is a control
that silently does nothing, and half (a) without half (b) is exactly the camera-lamp regression
described above.

**Dependencies:** C2
**Files:** `src/model/task_localization_config.h`, `src/model/localization_signal_mapper.cpp`,
`src/model/localization_runtime_controller.cpp`, `src/ui/forms/task/localization_dashboard_widget.cpp`,
`src/ui/forms/task/localization_setting_widget.cpp`, `tests/architecture_contract_test/main.cpp`
**Size:** M *(split as stated)*

---

### Checkpoint C-1 — startup announces its selection

- [x] Umbrella build clean; both shells relink (2026-09-09 08:48, after C7); contract test
      **144 passed / 0 failed**. The `125 / 0` target above was written as an additive estimate and
      is superseded — see the note under Baselines. **Display names:** the literal total `124` is
      not re-counted here and is not what guards anything; `test_every_display_name_has_a_translation_marker`
      compares the property set and the `kDisplayNameSources[]` markers **in both directions** and
      checks each marker's context, and it is green in the run above. C3's two new status-signal
      display names carry their markers.
- [x] **Translation sweep run 2026-09-09** — full result and the reading of the "0 newly vanished"
      criterion are recorded at Checkpoint C. C7 itself adds no strings: fault-code names are
      `QStringLiteral` identifiers by design (`localizationFaultCodeName()`), deliberately not
      `tr()`, so the dashboard shows the stable code name in every language.
- [x] Every negative check recorded, including C2's stay-green contrast and C3's `:3069`/`:2876` check.
- [x] **OWNER-RUN, one session on the cell — confirmed 2026-09-09.** The status registers report the
      live selection; the command registers are never written; the app log's startup summary names
      the camera, the group, the workspace and where each came from. An invalid index still faults
      per **D7**, so C2's extraction changed the message and the fault code, not the contract.

**Checkpoint C-1 CLOSED 2026-09-09.**

> **What this checkpoint claims, precisely.** It does **not** claim the runtime refuses to run on
> `firstKey()`. `buildRuntimeContext()` still supplies `firstKey()` for both indices, so a master
> that leaves 0 in its command register at power-up still gets a Ready task on the first bound
> camera. What has changed is that the selection is now **stated** — on two PLC registers, on the
> dashboard, and in the log — so a mismatch against the master's intent is visible to the operator
> and to the PLC program. Making the runtime *read* its selection from the PLC is the commented-out
> half of the request (see Out of scope).

---

### Task C4: The signal-map gate (D4 / item 1), and the `m_lastErrorReset` asymmetry

**Description.** Two halves, because the operator reaches the runtime by two routes.

*Runtime half* — in `setup()`, after `m_signalMapper.configure()` (`:371`) and before the role
connect requests (`:458-460`):
- each of the five required signals (`bExecuteTrigger`, `bTaskReady`, `bMatchingFinished`,
  `bTaskFault`, `nFaultCode`) with an empty tag appends a `SetupResult::errors` entry naming the
  signal's display name;
- every other signal with an empty tag emits one `WARN` task-log line and one `LOG_USER_WARN`;
- an **orphan** — a non-empty tag absent from the bound PLC device's tag list, read through
  `dynamic_cast<IPlcTagProvider *>(primaryPlcRunner()->device())` — is a hard error **in every
  case**, checked **per kind** (a bit signal against `availableDigitalIoNames()`, a word signal
  against `availableWordIoNames()`), never against the union;
- duplicate tags across two signals are raised into `result.errors`; `LocalizationSignalMapper::configure()`
  already detects them but only at `LOG_DEV_ERR` (`:59-66`), and two signals on one tag is a wiring
  fault, not a developer note;
- a PLC device implementing neither provider produces **one** DEV-level note and skips orphan
  checking — "no tag list" is not "the tag is wrong".

*Commissioning half* — wire `SignalsMapWidget::checkEmpty()` (`signals_map_widget.h:121`, still
called by nothing) into the localization setting widget at **project save**. `checkEmpty()` **clears**
every warning-flagged row (`signals_map_widget.cpp:279-292`), so it must not run blind: the dialog
classifies the returned names as required or optional and **refuses to purge a required row without
an explicit confirmation naming the consequence** — otherwise the save silently converts an orphaned
`bTaskReady` into an unmapped required signal and bricks the next runtime start.

*One-line fold-in.* `setup()` resets `m_lastExecuteTrigger` at `:373` and never resets
`m_lastErrorReset`. Two edge-detected inputs initialised by different rules for no stated reason,
recorded in item 54 as *"that part still stands"*. Reset it here, in the setup() edit this phase is
already making.

**Acceptance criteria**
- [ ] Each of the five required signals, unmapped in turn, fails setup with a message naming it —
      **five separate assertions**, not one.
- [ ] An unmapped optional signal (e.g. `bMatchingLowArea`) leaves setup **valid** and produces
      exactly one WARN. `bErrorReset` unmapped is explicitly supported
      (`task_localization_config.h:95-99`) and warns only.
- [ ] `M9999` — absent from `VirtualPlcDevice::availableDigitalIoNames()` — fails setup, on a
      required **and** on an optional signal, with a message naming the signal, the tag and the device.
- [ ] A bit signal mapped to a word tag is an orphan for that signal; the check is per-kind.
- [ ] The gate reads the same signal list the mapper uses — one list, not a second copy that drifts
      when a signal is added.
- [x] Saving a project whose map has orphan rows prompts; a required orphan cannot be purged
      silently; the purge writes `""` back into the config only for rows the operator confirmed.

> **DEVIATION — `checkEmpty()` was deleted, not wired.** The plan says to wire it. It cannot be
> wired safely: it clears every orphaned row and *then* returns their names, so no caller can
> classify them before the damage is done — which is precisely why "it must not run blind" had to be
> written into this task in the first place. Keeping a blind-clear entry point beside a safe one is
> the same trap in a smaller box (the argument C5 used), so it is replaced by two primitives:
>
> - `SignalsMapWidget::orphanRowNames() const` — reports, changes nothing;
> - `SignalsMapWidget::clearRowTags(names)` — purges exactly what it is told, consulting no
>   warning state, so a row the operator chose to keep is kept.
>
> Backlog item 1 asked for "wire it or delete it"; this is the third answer the item did not
> consider, and it is the only one that is safe.
>
> **The classification uses `LocalizationRuntimeController::requiredSignalNames()`**, promoted to
> public for exactly this. One definition: a second copy in the widget would drift, and the drift
> would let the editor purge a signal the runtime still demands.
>
> **Save path:** `MainWindow::saveToFile()` asks every open `LocalizationTaskWidget` first, and any
> of them may cancel the save. A task whose Settings page was never opened returns true — there is
> no widget holding a tag list to compare against, and inventing one at save time would ask the
> operator about a state they never saw. The runtime gate still refuses such a project at startup.
>
> **Three buttons, not two:** *Clear and save* / *Save as-is* / *Go back*, defaulting to Go back.
> "Save as-is" exists because an orphan can be legitimate — the tag may be right for a device about
> to be reconnected — and the runtime will still refuse to start until it is fixed, which is the
> honest outcome rather than a silent one. Clearing a **required** orphan asks a second time,
> naming the count and the tags about to be lost.
- [ ] `m_lastErrorReset` is reset by `setup()` alongside `m_lastExecuteTrigger`.
- [x] **The installed base is audited before the gate is enabled.** Re-run 2026-09-08 across **all
      14** `.vproj` files under `build\`, `Release\Project files\` and `Release\NCR PICKING ver1.2\config\`
      — more than the pre-audit covered.

      | Project | Five required signals | Device bindings |
      |---|---|---|
      | `20260820`, `nachi_20260701`, `untitled` (both copies) | all mapped (`M7016`/`M7001`/`M7005`/`M7008`/`D8001`) | bound |
      | `Huayan`, `Huayan_1` (both copies) | all mapped (Modbus COIL/DI/HR/IR) | bound |
      | `virtual` | all mapped (`M0`/`M18`/`M19`/`M23`/`D17`) | bound |
      | `4E_Line` (both copies), `virtual_2` | **none mapped** | **none** |

      **12 of 14 pass the required-signal half.** The two that fail have no device bindings either,
      so they already fail setup today with *"Missing primary_plc role runner."* — the gate adds
      messages but breaks nothing that works. `virtual_2.vproj` is new since the pre-audit (the
      owner's 2026-09-08 test project) and falls in the same category.

> ⚠️ **The ORPHAN half of the audit is NOT closed, and cannot be closed offline.** All four PLC
> families implement `IPlcTagProvider` (`mc_protocol_device.h:27`,
> `modbus_tcp_client_device.h:45`, `modbus_tcp_server_device.h:41`, `virtual_plc_device.h:34`), so
> the orphan check is live for every device — and whether a project passes it depends on that
> device's **configured ranges**, which live in the same `.vproj` and can only be resolved by
> loading it.
>
> This matters, because the owner's own 2026-09-08 log shows a project that would now be refused:
> at `app_log_2026-09-08.txt:88-103` a Modbus device rejects `M7001`…`M7008` and `D8000`/`D8001` as
> *"not a bit tag"* / *"not a register tag"* — MC-style tags left mapped onto a Modbus device. That
> project starts today and fails **every** write; with the gate it fails setup instead, naming the
> tag. **That is the improvement item 1 exists for, and it is still a behaviour change for the
> installed base.** It belongs in the OWNER-RUN item below, not in a tick here.

**Verification**
- [x] Contract test **122 → 129**, 0 failed (runtime half). Seven cases: the five required signals
      in one case with five separate assertions, an unmapped optional, an unmapped `bErrorReset`,
      orphans on required **and** optional, the per-kind orphan, duplicate tags, and the
      `m_lastErrorReset` reset.
- [x] **The existing suite did not move** — 122 / 0 before the new cases were added, with the gate
      already live. The fixture's `M10..M19` / `D100..D105` all exist on a `VirtualPlcDevice`
      offering 128 M and 128 D.
- [x] **Negative check (a).** `bMatchingLowArea` forced into the required set → **128 / 1**, only
      `test_an_unmapped_optional_signal_is_valid_and_warns`. Restored.
- [x] **Negative check (b).** Orphan condition disabled → **127 / 2**, exactly the two orphan cases;
      every unmapped case stayed green. Restored.
- [x] **Negative check (c).** Orphans compared against the union of both tag lists → **128 / 1**,
      only `test_a_bit_signal_mapped_to_a_word_tag_is_an_orphan_for_that_signal`. Restored.

> **ORDERING TRAP, and it cost a red run.** `validateSignalMap()` must be called **after**
> `bindFixedRoleRunners()`. `primaryPlcRunner()` resolves through `m_recoveryContexts`, which only
> that call populates — placed where the plan's prose suggested (right after
> `m_signalMapper.configure()`), the gate found no tag provider, **silently skipped every orphan
> check, and passed**. A gate that cannot see the device is worse than no gate, because it reports
> success. It still runs well before any role is asked to connect, which is the property that
> mattered.
- [x] **OWNER-RUN, runtime half — done 2026-09-08 15:36 on the cell.** A signal bound outside the
      device's span refused the runtime naming the tag: *"Signal \"Camera selection\"
      (nActiveCamera) is mapped to tag IR01000, which the device 05 does not provide as a
      register."* (`app_log_2026-09-08.txt:1552-1553`). The follow-on defect — that the message
      reached only the developer log — was found by this run and fixed; see the field-run block at
      the top of Phase C.
- [x] **OWNER-RUN, save dialog — done 2026-09-08.** The dialog appears on a project with orphan
      rows and all three buttons behave (*Clear and save* / *Save as-is* / *Go back*).
- [ ] **OWNER-RUN, remaining:** the save-time dialog read in both themes and in Japanese.

**Dependencies:** C3
**Files:** `src/model/localization_runtime_controller.{h,cpp}`,
`src/ui/forms/task/localization_setting_widget.cpp`, `tests/architecture_contract_test/main.cpp`
**Size:** M

---

### Task C5: Delete the four dead active-index entry points (59.1)

**Description.** `TaskLocalization::onSignalChangeCameraNumber()` / `onSignalChangePatternNumber()`
(`task_localization.cpp:471-497`, declared `task_localization.h:201`, `:205`) have no `connect()`
site — **and** `setCameraNumber()` (`:426`) / `setPatternNumber()` (`:458`), which they call, have no
caller anywhere else. Delete all four, with their declarations and doc comments. Deleting only the
slots leaves two public methods reachable by name, which is the same trap in a smaller box.

The live paths are `handlePlcValues()` (PLC) and `queueSetActive*()` (manual). A third that nothing
calls is what made item 58's *"the setters have two entry points"* read as safer than it was.

**Acceptance criteria**
- [ ] All four members and their declarations are gone.
- [ ] `cameraNumberChanged` / `patternNumberChanged` (`localization_runtime_controller.h:223-225`)
      keep whatever consumers they have, or are removed with them if they have none — **checked, not
      assumed**.
- [ ] Both shells relink; nothing resolved any of the four by name (`QMetaObject::invokeMethod` with
      a string, a `.ui` auto-connection).

**Verification**
- [x] Contract test **unchanged at 122 / 0** across the deletion; umbrella build clean and both
      shells relinked (2026-09-08 14:58).
- [x] **`cameraNumberChanged` / `patternNumberChanged` were removed too** — checked, not assumed.
      They were emitted from `handlePlcValues()` and connected by **nothing** across `src/`, `app/`,
      `runtime_app/` and `tests/` (only generated `moc_*` matched). Both indices already reach every
      consumer through `signalChanged()`, and since C3 the adopted value also arrives on the two
      status signals.
- [x] Evidence is the link plus a string search, recorded in the plan:
      ```powershell
      cd C:\DGB\Project\ncr_picking
      Select-String -Path src\*,app\*,runtime_app\*,tests\* -Include *.cpp,*.h,*.ui -Recurse `
        -Pattern 'onSignalChangeCameraNumber|onSignalChangePatternNumber|setCameraNumber\(|setPatternNumber\('
      ```
      Expect no hit outside `build\` and generated `moc_*`. *(Run 2026-09-08: no hit anywhere except
      the definitions themselves and the unrelated `setCameraNumberMap`.)*
- [ ] No negative check applies — this is a deletion. The search result **is** the evidence.

**Dependencies:** C4
**Files:** `src/model/task_localization.{h,cpp}`
**Size:** XS

---

### Task C6: Connect the PLC and output roles first, then resolve the selection from the live PLC (D8 / item 58 root fix)

**Description.** This is the task the whole phase points at. Today the two index signals — which are
**inputs owned by the PLC role** — are resolved from the project's bindings before the PLC role is
even connected. `buildRuntimeContext()` fills them from `cameraDeviceIds.firstKey()` /
`patternGroups.firstKey()` (`task_localization.cpp:724-737`), and `setup()` adopts them verbatim.
Resolving a PLC-owned input from a source that is not the PLC is wrong by construction, and it is
why a task reaches Ready with the master's registers at 0 and runs a cycle on whichever camera sorts
first.

Two parts, in this order:

**(a) Ordering.** `beginRuntime()` connects the **primary PLC and vision-output roles first** and
waits for them to reach a healthy status before the camera and pattern selection is resolved. Today
`setup()` fires `requestRoleConnectNow()` at all three roles together and then evaluates readiness.
The camera role's binding *depends on* a value only the PLC can supply, so it cannot be bound first.

**(b) Live read.** Give `PlcValueMap` (`plc_device.h:114-119`) a name→value accessor — it is an
empty polymorphic base with only `clone()` today, which is the real work in this task — and have
`setup()` resolve the active indices from the PLC's own snapshot via `PlcRunner::pollingUpdate`
(`plc_runner.h:155`). Feed the result through **C2's validators**. Fall back to the project's
`firstKey()` **only when the signal is unmapped**, and say which source was used in C2's summary
line.

**What must not change.** D7: a value read from the PLC that is out of range, unregistered or
non-numeric faults exactly as a written one does. C6 changes *where the startup value comes from*,
never *what an invalid value means*.

> ⚠️ **THE FIRST IMPLEMENTATION BROKE EXACTLY THIS, and the field found it (2026-09-08 17:03-17:12).**
> A refused startup index was reported through `SetupResult::errors`, which leaves `m_valid` false —
> and `markRuntimeReady()` gates on `m_valid` (`:1161`). So the runtime could **never** re-arm. The
> owner started with camera 0 and pattern 0, then corrected both: the camera reconnected
> (`app_log_2026-09-08.txt:1958-1968`), the pattern went valid (`:1972-1973`), `bErrorReset` was
> pulsed (`:1974-1975`), and **no `Faulted -> Ready` transition ever appeared** — `bTaskReady`
> stayed off until the task was stopped at `:1984`.
>
> A written 0 faults **recoverably**; a read 0 faulted **terminally**. Those are not the same
> meaning, so the paragraph above was violated by its own implementation.
>
> **Fix:** a refused startup index sets the per-signal latch, publishes the fault signals, enters
> `Faulted` and emits `runtimeFault` — but does **not** enter `result.errors`. The runtime stays
> valid and the next good write re-arms it, exactly as at runtime. The refused number is also not
> adopted, matching `setActiveCameraNumber()`, so a usable camera stays bound and the role can
> connect. `SetupResult::errors` keeps its meaning: *this configuration cannot run*, which an index
> the master can change at any moment is not.
>
> **Two of this task's own tests asserted the wrong contract** (`!setup.valid` and a terminal
> `Faulted`) and went red against the fix, correctly. They are replaced by
> `test_a_startup_index_the_plc_refuses_is_recoverable_not_terminal` and an extended end-to-end
> case that now asserts the **recovery** — the half that was missing and that the field had to
> supply. Negative check: restoring the `result.errors.append()` turns both red (140 / 2).

**Acceptance criteria**
- [ ] `beginRuntime()` does not resolve the camera/pattern selection until the primary PLC role
      reports a healthy status, or until a bounded wait expires — **the wait is bounded and its
      expiry is reported**, never silent, or a dead PLC becomes a hang instead of a fault.
- [ ] With `nActiveCamera` mapped and the master holding **0** at startup, the task does **not**
      reach Ready: it faults per D7, naming the number and the range. This is the owner-reported
      symptom, and it is the acceptance test for the phase.
- [ ] With `nActiveCamera` mapped and holding a **valid** number that differs from `firstKey()`, the
      runtime starts on the **commanded** camera, and C2's summary line says `"commanded"`.
- [ ] With `nActiveCamera` **unmapped**, behaviour is unchanged from today — project default, summary
      line says `"project default (firstKey)"`, no fault. A cell that does not switch cameras must
      not be broken by this task.
- [ ] The accessor is family-independent: one code path serves MC, both Modbus devices and the
      virtual PLC. No `dynamic_cast` to a concrete device in the controller.
- [ ] A PLC that supplies no snapshot at all (accessor returns nothing for the tag) is treated as
      unmapped, not as 0. **This distinction is the entire defect** — do not collapse it.

**Verification**
- [x] Umbrella build clean, both shells relinked 2026-09-08 16:50; contract test **136 → 142**,
      0 failed, twice. C6(a) added 3 cases, C6(b) 4 at setup level and 2 end to end.
- [x] The hardware-free path proves it — as **new** cases rather than an extension of
      `test_a_hardware_free_runtime_runs_a_cycle_driven_only_by_injected_plc_inputs`, which drives
      the controller directly and so cannot exercise `beginRuntime()`'s ordering at all. The new
      pair parks a value in the virtual PLC **before** `beginCommission()`, the way a master holds
      a register across a restart:
      `test_a_runtime_started_with_the_master_holding_zero_does_not_reach_ready` and
      `test_a_runtime_started_with_the_master_holding_a_valid_number_adopts_it`. Backlog 43's
      injection is what made this possible without the cell.
- [x] **Negative check (a).** Making `commandedIndexFromPlc()` always answer "nothing" gave
      **138 / 2**: the commanded-camera and holding-0 cases, exactly as predicted, with the
      unmapped and no-snapshot cases green. Restored.
- [x] **Negative check (b) — far larger than predicted, and that is the finding.** Treating "no
      snapshot" as 0 gave **97 / 43**. The plan expected one case. Almost every existing case
      builds a context with `nActiveCamera` mapped and no snapshot, so collapsing "not read" into
      "holds 0" selects camera 0 across the whole suite. **The distinction is load-bearing far
      beyond the case written to pin it** — which is the strongest evidence available that it is
      the right distinction. `test_a_plc_with_no_snapshot_is_not_read_as_holding_zero` was among
      the 43, so the specific prediction held too. Restored; 142 / 0.
- [ ] **OWNER-RUN, Checkpoint C:** the exact sequence from the 2026-09-07 report — start the runtime
      with the master's registers at 0 and confirm the task does **not** go Ready and does **not**
      accept a trigger; then write a valid number and confirm it starts on that camera.

**Split point (over the file guideline).** *(a)* the `PlcValueMap` accessor across its four
implementations, with its own tests and no controller change — independently useful, and it is what
G2 needs; *(b)* the `beginRuntime()` ordering and the `setup()` read. Land (a) first: it is the
piece that can be verified in isolation.

> **C6(a) was TWO implementations, not four — and it carried a requirement the plan did not.**
> `McDeviceMap` and `ModbusRegisterMap` are the only concrete `PlcValueMap`s.
> **`VirtualPlcDevice` had none at all**, so the one PLC that needs no hardware was also the only
> one that could not be asked what it holds — which would have put this defect permanently out of
> reach of an automated test. `VirtualPlcValueMap` was added, and the device now publishes a
> snapshot on `deviceConnect()` as well as on every injection, because a real PLC starts polling
> when it connects.
>
> That last line is not cosmetic: without it every task-level test paid the full
> `kPlcSnapshotWaitMs` bound and the suite ran **16.7 s instead of 8.7 s** — the bounded wait
> working exactly as designed, against a device that had nothing to say.

> **How the snapshot reaches `setup()`: through `RuntimeContext`, not a controller subscription.**
> `setup()` stays a pure function of its context, which is what the contract tests depend on — they
> build contexts directly. `TaskLocalization` owns the subscription and the wait.

> **The bounded wait is a nested `QEventLoop` on the GUI thread, inside a lifecycle call, and that
> is a deliberate trade.** Resolving the selection *after* `setup()` would avoid it entirely — but
> the task would reach Ready on the project default first and only then fault, and "does not reach
> Ready" is the acceptance criterion this task exists for. The bound is
> `kPlcSnapshotWaitMs = 2000`; expiry falls back to the project default **and says so at USER
> level**, because a PLC that never answers must degrade, not hang.

**Dependencies:** C2 (validators), C3 (status signals, so the adopted value is reportable), C5
**Files:** `src/device/plc/plc_device.h`, `src/device/plc/mc_protocol_device.{h,cpp}`,
`src/device/plc/modbus/modbus_register_map.{h,cpp}`, `src/device/virtual/virtual_plc_device.{h,cpp}`,
`src/model/task_localization.cpp`, `src/model/localization_runtime_controller.cpp`,
`tests/architecture_contract_test/main.cpp`
**Size:** **L** *(the accessor touches four device implementations and `beginRuntime()`'s ordering
is a lifecycle change — split as stated, do not attempt in one pass)*

---

### Task C7: The fault code for "this number names nothing" (owner-requested, 2026-09-09)

**Why.** C2–C6 fixed the *message* an unusable index produces, and left the *code* wrong. Every
camera-selection refusal published `nFaultCode = 100` — `CameraLost`. A master branching on 100
sends someone to check cabling and power supply for a camera that was never connected because it
was never bound. That is the same wrong signpost item 55 removed from the message text, still
standing in the one field the PLC program actually branches on. The message is for the person
reading the log; the code is for the program. Fixing only the first is half a fix.

The owner declared `CameraNotRegistered = 103` and asked that the index-invalid paths use it, and
that `PatternInvalid` be renamed `PatternNotRegistered` to match.

**What changed.**

- `LocalizationFaultCode::CameraNotRegistered = 103` — documented, and **added to the
  `localizationFaultCodeName()` switch**, which the declaration had missed. The switch has no
  `default`, so the omission compiled clean and the dashboard's fault panel
  (`localization_dashboard_widget.cpp` `faultCodeText()`) would have shown the operator the literal
  word `Unknown` for every 103. A UI defect, not only a logging one.
- `PatternInvalid` → `PatternNotRegistered`. **Value 400 unchanged**, so no deployed PLC program
  needs an edit for the pattern half. Name only.
- Four publish sites moved from `CameraLost` to `CameraNotRegistered`:
  `setActiveCameraNumber()`'s refusal, `setup()`'s context-supplied number, the
  `m_startupSelectionFault` block from C6, and `reportSignalTypeMismatch()`.

> **`reportSignalTypeMismatch()` was not in the owner's ask, and was changed anyway — stated here
> rather than done quietly.** A mis-bound tag is not an out-of-range index; it carries no commanded
> number at all. But the two paths were built deliberately identical (the comment at that site says
> "Faulted like a bad index, and for the same reason"), and leaving one at 100 while the other moved
> to 103 would have made that comment a lie and split one operator-visible behaviour into two codes.
> The outcome is the same in both cases — no camera is selected — so they stay aligned.

> **⚠️ The rename makes 400's name narrower than four of its uses.** `PatternNotRegistered` is
> published for a group that *is* registered but holds no usable train image: the content check in
> `setActivePatternGroupNumber()`, the one in `setup()`, `startCycle()`'s pre-flight, and the
> missing-snapshot abort. For those, "not registered" is as misleading as `CameraLost` was for an
> unbound camera — the operator goes looking for a missing group registration and finds the group
> present. **This is the same class of defect this task exists to fix, left standing on the pattern
> side.** The clean shape is a second code (`PatternInvalid = 402`, content-invalid) alongside
> `PatternNotRegistered = 400` (selection-invalid), which needs a value the owner has not allocated.
> Recorded as backlog item 63; not done here, because adding a numeric code to the PLC contract
> without the owner's decision is not a rename.

**What must not change.** Value 400. The published *behaviour* of every one of these paths — which
signals go false, the `Faulted` transition, the recoverability C6 established. C7 changes one
integer per site and nothing else.

**Tests** (`tests/architecture_contract_test/main.cpp`):

- `test_localization_fault_code_values_are_stable` — extended with `CameraNotRegistered == 103` and
  the renamed `PatternNotRegistered == 400`.
- `test_every_localization_fault_code_has_a_name` — **new**. Iterates every declared code and
  asserts the name is not `"Unknown"`. This is the test whose absence let the missing switch case
  through; the "not Unknown" check alone could be satisfied by a case returning the wrong string, so
  it also spot-checks the two names this task changed.
- `test_an_index_that_names_nothing_reports_not_registered_not_lost` — **new**. Four blocks, one per
  publish site, each asserting the published `nFaultCode`, because a partial change across four
  statements is precisely the regression this guards. Site (3) is the field case: the master holding
  0 at startup.

**Verification (measured 2026-09-09):**

- Suite shape checked before trusting the total: `-functions` 142, `void test_` in source 142.
- **144 passed / 0 failed** (142 cases + `initTestCase`/`cleanupTestCase`).
- Negative check (a): `setActiveCameraNumber()`'s refusal restored to `CameraLost` → **143 / 1**,
  exactly `test_an_index_that_names_nothing_reports_not_registered_not_lost`. Restored.
- Negative check (b): `CameraNotRegistered`'s case deleted from `localizationFaultCodeName()` →
  **143 / 1**, exactly `test_every_localization_fault_code_has_a_name`, failing with *"fault code
  103 has no name mapping"*. Restored.

**Docs updated:** `plc_signal_contract.md` (fault table, the `nFaultCode = 100 (camera)` line in the
refusal block, two `PatternInvalid` references), `task_localization.md` (fault table),
`runtime_controller_api.md`, `maintenance_and_extension.md` (coverage list, plus a warning on step 2
of "Adding A Fault Code" — the step this change found had been skipped).
`docs/domains/task_localization/task_localization_implementation_plan.md` was **left alone**: it is
a completed plan that defers to `task_localization.md` for the contract, and editing it would be
rewriting a record rather than a document.

**Also removed:** a comment block duplicated verbatim in `setup()` (the "Announce the resolved
selection here too" paragraph, pasted twice during C3/C6). Noted rather than left, because it was
introduced by this phase.

**Dependencies:** C2, C6
**Files:** `src/model/localization_fault_code.h`, `src/model/localization_runtime_controller.cpp`,
`tests/architecture_contract_test/main.cpp`, four docs under `docs/domains/task_localization/`
**Size:** **S**

---

### Checkpoint C — the selection contract is closed ✅ **CLOSED 2026-09-09**

- [x] Contract test **144 passed / 0 failed** (2026-09-09, after C7), suite shape verified
      `-functions` 142 == `void test_` 142. Umbrella build clean; **both shells relinked 2026-09-09
      08:48** carrying C7.
- [x] **All four device suites at baseline** (2026-09-09, shape verified before each total):

      | Suite | `-functions` vs source | Observed | Written baseline |
      |---|---|---|---|
      | `mc_frame_test` | 42 == 42 | **44 / 0** | 38 — **the written one is wrong**; A1 added 6 |
      | `modbus_device_test` | 20 == 20 | **22 / 0** | 22 ✓ |
      | `vision_output_device_test` | 7 == 7 | **9 / 0** | 9 ✓ |
      | `vision_tcpip_client_device_test` | 8 == 8 | **10 / 0** | 10 ✓ |

      The known flake (**item 32**, product-side, pre-existing) was then characterised over five
      further runs of each vision suite: **1/5 and 1/5**, and in every failing run the only failure
      was `test_disconnect_notice_on_graceful_close`. One clean pass is not evidence the flake is
      gone — it is not, and it is not C7's.
- [x] **Translation sweep run** 2026-09-09 (`scripts\update_translations.ps1`, backup taken).
      `finished 864→860, unfinished 446→458, vanished 29→33`. The criterion reads *0 newly
      vanished* and this run produced **4** — inspected rather than waved through, and all four are
      correct: they are the log strings of the four dead entry points **C5 deliberately deleted**,
      confirmed absent from `src/` by grep. That is not the failure this criterion guards against
      (a shell-driven lupdate marking hundreds vanished at once); their Japanese is retained and
      returns if the strings ever do. The 12 newly `unfinished` are exactly C4's 10 dialog strings
      plus C3's 2 status display names — the latter proving the `kDisplayNameSources[]` markers
      resolve under the right context. `.qm` re-released 09:03:36 and both shells relinked after it,
      so the new file is embedded, not merely written.
- [x] Every Phase C negative check recorded individually, with its stay-green half — C2, C3(a)(b),
      C4, C5, C6(a)(b), the C6-regression check, C7(a)(b). Three of them contradicted the plan's own
      prediction and the plan was corrected, not the observation.
- [x] The `.vproj` audit result from C4 recorded — in Task C4 itself, where the table lives:
      **12 of 14** projects pass the required-signal half; the 2 that fail have no device bindings
      and already fail setup today. The **orphan** half is explicitly *not* closable offline (it
      depends on each device's configured ranges, readable only by loading the project) and is
      carried by the OWNER-RUN item below, not by this tick.
- [x] **The owner's 2026-09-07 symptom is gone** — confirmed by the owner on the cell 2026-09-09:
      started with the master holding camera 0 and pattern 0, the task faults instead of going Ready;
      correcting **both** registers returns it to Ready with no restart and no operator action. This
      is the single outcome Phase C exists for, and it took the C6 regression fix to actually get
      there — the first cut refused the bad index correctly and then could never re-arm.
- [x] **OWNER-RUN — confirmed 2026-09-09.** A deliberately broken signal map is refused with a
      message naming the signal and the tag; the save-time dialog behaves in both themes and in
      Japanese; acknowledge produces no warn line. *(The dialog's three buttons were separately
      confirmed 2026-09-08. Its 10 strings entered the `.ts` as `unfinished` in the same day's
      sweep, so it renders in English until a translator fills them in — the dialog's **behaviour**
      is what this item gates, and that is confirmed.)*
- [x] **Backlog closed 2026-09-09, each with the task and the test that closed it:**

      | Item | Task | Evidence |
      |---|---|---|
      | **1** `checkEmpty()` unwired | C4 | `checkEmpty()` **deleted**, split into `orphanRowNames()`/`clearRowTags()`; trigger = project Save. `test_orphan_row_names_reports_orphans_without_clearing_them` + 3 gate tests; field log `:1552-1553` |
      | **53** blanket disconnect | A3 | **NO test, stated as such.** Unreachable in the current codebase; the planned O-1 fixture was withdrawn rather than written. Re-open when a second fixed-role bind site exists |
      | **55** unregistered camera read as calibration | C2 (+ C7 for the code) | `test_setup_refuses_an_unregistered_active_camera_distinctly_from_calibration` + 2; field log `:1450-1451`, `:1495` |
      | **58 part A** | C2+C3+C6 | 7 tests listed in the item; **owner-confirmed on the cell 2026-09-09** |
      | **59.1** dead slots | C5 | deletion — evidenced by grep + exactly 4 strings vanishing in the sweep, one per deleted function |
      | **59.2** workspace unset at startup | A2 | `test_setup_applies_the_active_camera_workspace_before_the_first_cycle` + 1; field log `:1432` |
      | **59.3** lockstep map walk | A1 | `test_a_shadow_larger_than_the_live_map_still_compares_by_address` + 1 |
      | **59.3b** → filed as **59.4** | A1 | `test_m_changes_survive_a_station_with_no_d_ranges` + 1. It was never housekeeping: on an M-only PLC every `bExecuteTrigger` was dropped |

      **Item 58 part B stays open**, with its remaining scope stated precisely in the item: C6 built
      the snapshot mechanism B asked for, so what remains is (i) whether a mapped-but-never-written
      register is its own commissioning fault, and (ii) MC / Modbus-client first-poll adoption,
      which still affects every signal other than the two indices.
      **New item 63** (pattern content-invalid needs its own code) opened by C7.
      **59.3 and 59.4 are proven at the helper level only** — `mc_frame_test` does not compile
      `mc_protocol_device.cpp`; item 56 carries that gap.

---

## Phase D — vision-output liveness

### Task D1: A wedged connect must be visible

**Description.** `VisionTcpipDeviceBase::deviceConnect()` returns `true` at `:39` when already
active, emitting no status. `VisionOutputRunner::m_busy` is cleared only by `onConnectStatusChanged`
or `onConnectionFailed` — the early return emits neither — so the runner is wedged permanently and
every later `requestConnect()`, including every retry `scheduleRoleReconnect()` schedules, does
nothing. The task parks in `Recovering` with `bTaskReady=false` **and** `bTaskFault=false`, and the
documented "watch `bTaskReady`" escape hatch is the operator's only signal.

Two edits, one predicate. Add a `publishCurrentConnectStatus()` helper per subclass whose rule is
**exactly the rule `startTransport()` already applies** — for the server, listening ⇒ `Connected`
(`vision_tcpip_device.cpp:113` publishes it with no client attached, so this redefines nothing); for
the client, both links up ⇒ `Connected`, else `Connecting` (`vision_tcpip_client_device.cpp:256`).
Call it on `deviceConnect()`'s early return, and beside the existing `syncRuntimeState()` in
`attachMainSocket()`/`attachHeartbeatSocket()` — which is what makes the server re-announce
`Connected` after `declareLostConnection()` left it at `LostConnected` with its listeners still open.

**The client is the control, not a second patient.** `VisionTcpipClientDevice` already redials
(`:118-126`) and republishes; its suite must be verifiably unchanged.

**Acceptance criteria**
- [ ] A second `deviceConnect()` on an already-active device emits `connectStatusChanged` carrying
      the status `startTransport()` would report for the same state — never an optimistic constant.
- [ ] `VisionOutputRunner::requestConnect()` twice in a row is not wedged: the second call is either
      dispatched or `m_busy` is cleared by the status that comes back. Asserted at **runner** level,
      because that is where the damage is.
- [ ] A client re-attaching after a heartbeat loss drives the server back to `Connected` and the
      controller's `handleRoleStatusChanged` re-arms the runtime.
- [ ] Repeated `requestConnect()` on a connected role does not spam the status stream — publish, do
      not re-announce an unchanged status.
- [ ] Nothing is published from inside `deviceConnect()`'s `QMutexLocker` scope in a way that can
      re-enter the device.
- [ ] `PlcRunner`'s and `CameraRunner`'s `m_busy` are audited for the same shape and the finding
      recorded either way.

**Verification**
- [ ] Umbrella build clean.
- [ ] Device suite `vision_output_device_test`: **9 → 11** —
      `test_connect_on_an_active_device_still_publishes_a_status`,
      `test_server_republishes_connected_when_a_client_reattaches`.
- [ ] Contract test **132 → 133**: `test_vision_output_role_recovers_after_a_link_loss`, driving the
      controller's recovery path end to end and asserting `bTaskReady` returns true.
- [ ] Device suite `vision_tcpip_client_device_test`: **10**, unchanged — the control.
- [ ] **Negative check.** Restore the silent early return → the runner-wedge case and the contract
      case go red **while the client suite stays green**. Remove the attach-time publish → the
      re-attach case goes red. Restore both.
- [ ] **OWNER-RUN:** pull the vision-output cable for longer than the heartbeat timeout, restore it,
      and confirm the task returns to Ready **without restarting the application**.

**Dependencies:** Checkpoint C
**Files:** `src/device/output_device/vision_tcpip_device_base.{h,cpp}`,
`src/device/output_device/vision_tcpip_device.cpp`, `tests/vision_output_device_test/main.cpp`,
`tests/architecture_contract_test/main.cpp`
**Size:** S

#### ✅ D1 LANDED 2026-09-09

`publishCurrentConnectStatus()` is a pure virtual on the base, implemented by both subclasses,
each repeating **its own** `startTransport()` predicate: server ⇒ listening means `Connected`
(and `ConnectFailed`, named, for the unreachable active-but-not-listening state); client ⇒ both
links up means `Connected`, else `Connecting`. Called from `deviceConnect()`'s already-active
path **outside the `QMutexLocker` scope** (the early return was restructured so the two existing
paths keep byte-identical behaviour), and beside `syncRuntimeState()` in `attachMainSocket()` /
`attachHeartbeatSocket()`.

> **The audit criterion produced the strongest evidence in the task, and it inverted the framing.**
> `PlcRunner` and `CameraRunner` are **not** exposed to this shape — because every other device
> family already does exactly what D1 adds. `McProtocolDevice::deviceConnect()` (`:97-99`),
> `ModbusTcpClientDevice` (`:191-193`), `ModbusTcpServerDevice` (`:279-281`) and `JaiGigECamera`
> (`:637-639`) all re-publish `Connected` on the redundant-connect path, and the MC one carries
> the reason in a comment: *"Re-publish Connected so a runner that issued the redundant request
> still sees a status event and clears its busy flag."* The three virtual devices publish
> unconditionally; `BaslerGigECamera` has no early return at all. **The vision TCP/IP family was
> the single gap in seven implementations**, and someone had already written down why it matters.

> **A fragility worth knowing.** The fix depends on `IDevice::setConnectionStatus()` emitting
> unconditionally — its de-duplication `if` is **commented out** at `idevice.h:135`. Re-enabling
> it would silently restore the wedge for the already-`Connected` case (recompute agrees with the
> stored value ⇒ no emit ⇒ `m_busy` stays true). The field case still recovers either way, because
> there the recompute moves `LostConnected` → `Connected`. Left as found — un-commenting it would
> change every device family — and `test_vision_output_runner_is_not_wedged_by_a_redundant_connect`
> is what would catch it.

**Tests** — the plan asked for two device cases and one contract case, and that is what landed,
though the contract case is **named for criterion 2 rather than the plan's suggested
`test_vision_output_role_recovers_after_a_link_loss`**: the wedge is the damage, "asserted at
runner level" is the criterion, and the name should say which.

- `test_connect_on_an_active_device_still_publishes_a_status` — spy attached *after* the first
  connect so it records the redundant call alone; asserts exactly one emission carrying
  `Connected` with no client attached.
- `test_server_republishes_connected_when_a_client_reattaches` — drives a real heartbeat loss
  with a non-acking peer, then reattaches.
- `test_vision_output_runner_is_not_wedged_by_a_redundant_connect` (contract) — a real
  `VisionTcpipDevice` behind a `VisionOutputRunner`, because the virtual device publishes
  unconditionally and can never reproduce this. `m_busy` is private, so the assertion is the
  observable consequence: three `requestConnect()` calls, three statuses, then a disconnect.

**Verification (measured 2026-09-09):**

- `vision_output_device_test` **11 passed / 0 failed** (`-functions` 9, was 7 — the plan's
  "9 → 11" total is exactly right).
- `architecture_contract_test` **145 passed / 0 failed**, shape `-functions` 143 == source 143.
- `vision_tcpip_client_device_test` **10 / 0** — the control, unchanged.
- **Negative check (a)** — silent early return restored: contract **144 / 1** (exactly the runner
  case), `vision_output_device_test` **9 / 2** (the new case, plus the item-32 flake firing
  coincidentally), **client suite 10 / 0**. The control staying green is the half that matters.
- **Negative check (b)** — attach-time publish removed:
  `test_server_republishes_connected_when_a_client_reattaches` red, exactly as predicted.
- Both restored; no scaffolding left.

---

### Task D2: The graceful close is a close race, not a test bug (item 32)

**Description.** On the graceful path, replace the abortive close with `disconnectFromHost()` plus a
bounded wait for `disconnected`, so the FIN is ordered *after* the notice in the stream instead of
racing it: `sendDisconnectNotice()` (`:514-531`) already flushes with `waitForBytesWritten(150)`, and
then `detachHeartbeatSocket()` (`:365-375`) **aborts**, and an RST discards what the peer has not yet
read. `detachMainSocket()` (`:351-360`) gets the same treatment — it aborts the RESULT socket
unflushed, and a result written and then RST-discarded is worse than the lost notice, because the
robot acts on it. Keep `abort()` on the lost-connection path (`declareLostConnection()` `:582-596`),
where there is nothing to deliver.

**The test exists in duplicate** — `vision_output_device_test/main.cpp:217` and
`vision_tcpip_client_device_test/main.cpp:231`. Item 32 names only the second. The fix is in the
shared base, so one code path serves both; both must go green, and the backlog entry is amended to
name both files before it is closed.

> ⚠️ **Before touching the product code, confirm the test is in the binary.** Until 2026-09-08 it
> was in **neither** — a stale root `main.moc` shadowed the generated one and both suites ran a
> short set with `0 skipped, 0 blacklisted` (**backlog 62**). Both build dirs are now repaired and
> verified, and the repair is durable — it survives qmake, because qmake was the thing writing the
> bad dependency. Still start this task with `<exe> -functions` in both build dirs: if the case is
> not listed, fix the build first or every measurement below is meaningless.

> **Drive from the client suite** — it is the one item 32 names, and no batch has shown the server
> copy failing more often. But **do not plan around a rate**: measured over ten runs each,
> `vision_tcpip_client_device_test` failed 6 and `vision_output_device_test` 4, and the client
> swung from 5-in-6 to 1-in-4 between batches with nothing changed. This is a timing race whose
> frequency depends on machine load. The 20-run bar below is the minimum that carries information;
> **do not shorten it**, and treat a green batch of 4 or 6 as no evidence at all.

**Acceptance criteria**
- [ ] The graceful path flushes and waits, bounded, before teardown; the bound is a named constant
      with a stated reason, and expiry is **logged**, not silently ignored.
- [ ] The bounded wait cannot hang the device thread, and fits inside `disconnectAndWait()`'s outer
      3 s bound (`idevice_runner.h:159-190`). **State the arithmetic.**
- [ ] A lost-connection teardown still sends no notice —
      `vision_output_device_test`'s `test_heartbeat_timeout_declares_lost` keeps asserting
      `!peer.gotDisconnectNotice()`.
- [ ] A result payload in flight is flushed before the main socket closes, or the failure is
      reported — never silently discarded.

**Verification**
- [ ] Umbrella build clean; both suites at **11** and **10**, 0 failed.
- [ ] 20 consecutive runs of the named case in **both** suites, all green:
      ```powershell
      cd C:\DGB\Project\ncr_picking\build\tests\vision_output_device_test\release
      $env:QT_QPA_PLATFORM = 'minimal'
      1..20 | ForEach-Object { .\vision_output_device_test.exe test_disconnect_notice_on_graceful_close -o "r$_.txt,txt" }
      Select-String -Path r*.txt -Pattern 'FAIL!|Totals'
      ```
      Repeat in `build\tests\vision_tcpip_client_device_test\release`.
- [ ] **Negative check.** Restore `abort()` on the graceful path → the flake returns within 20 runs
      in at least one suite; **record the observed rate**. The pre-fix baseline is now measured
      rather than estimated: **5 / 6 on the client suite, 2 / 6 on the server suite** (2026-09-08,
      six consecutive runs each, after the build was corrected). If it does **not** return, the
      diagnosis is wrong and this task reopens rather than closing on a green run. Restore.

**Dependencies:** D1 (same file)
**Files:** `src/device/output_device/vision_tcpip_device_base.cpp`,
`tests/vision_output_device_test/main.cpp`, `tests/vision_tcpip_client_device_test/main.cpp`
**Size:** S

#### ❌ D2 BUILT, MEASURED, REVERTED 2026-09-09 — the diagnosis is wrong

**The task's own stop rule fired, and it fired harder than it was written to.** The negative check
says: *"Restore `abort()` on the graceful path → the flake returns within 20 runs... If it does
**not** return, the diagnosis is wrong and this task reopens rather than closing on a green run."*
What happened is the stronger version — **the flake never left**.

The work was done as specified: `SocketClose::{Graceful,Abortive}` on both detach helpers,
`disconnectFromHost()` + `waitForDisconnected(kGracefulCloseMs = 300)` on the graceful path,
`abort()` kept for `declareLostConnection()` alone, expiry logged, timing arithmetic stated
(150 + 2×300 = 750 ms inside `disconnectAndWait()`'s 3000 ms guard). Then the 20-run bar:

| Build | Failures / 20 |
|---|---|
| Graceful close — the specified fix | **7** |
| Graceful close + drain the receive buffer first | **13** |
| Same binary forced back to `abort()` | **10** |
| Peer draining on `disconnected` too | **5** |

All the same rate within this flake's known noise. **Reverted in full.** The full instrumentation
and the three refuted hypotheses are written into **backlog item 32**, which previously carried
the same RST diagnosis this plan repeated.

The single fact that settles it: with `abort()` the peer reports **`RemoteHostClosedError`**, a
clean FIN — not `ConnectionResetError`. *The abortive close was never producing the RST both this
plan and item 32 blamed.* Server-side counters are byte-identical between passing and failing runs
(`flush=1`, `toWrite=0`, `available=0`, clean `disconnectFromHost()`, no timeout ever logged), and
the peer's receive buffer is **empty** with the probe correctly parsed — so the notice's 11 bytes
never arrive at all, rather than arriving and being discarded.

**Item 32 stays open with a corrected diagnosis and two labelled-as-untested candidates:** wait
(bounded) for the peer to close its end before tearing down — a shutdown-handshake change needing
the owner's agreement — or accept that the notice is best-effort, as the code comment already
claims, and rewrite both duplicated tests to assert what the protocol actually promises.

**Nothing about the pending-write concern (criterion 4) was proven either**, so it was not kept:
shipping an unproven behaviour change into a device teardown path to satisfy an argument is
exactly what this project's discipline forbids. If it is worth having, it is worth its own task
with its own test.

#### ✅ D2 REOPENED AND CLOSED 2026-09-09 — the real cause found by one more experiment

The revert stands: **the close *kind* was never the problem.** What was missing was the
experiment that isolates it. One run settled it — suppress the teardown entirely and the notice
arrives **0 / 20**. So the bytes always left the process; the close destroyed them.

| Build | Failures / 20 |
|---|---|
| Original `abort()` | 10 |
| D2's graceful close | 7 |
| Graceful + drain **Qt's** buffer | 13 |
| Peer draining on `disconnected` too | 5 |
| **No teardown at all** | **0** |
| `QThread::msleep(50)`, then teardown | 14 |
| **Drain the OS receive buffer, then teardown** | **0** |

**Mechanism.** Closing a socket whose **OS** receive buffer still holds unread inbound bytes makes
the stack send **RST** instead of FIN, and an RST tells the peer to discard *its* receive buffer —
including the notice it had not read yet. The heartbeat socket is exactly the one holding unread
bytes: the peer acks every probe.

> **The measurement that had misled me was mine, and it was measuring the wrong buffer.**
> `bytesAvailable()` reports **Qt's** buffer, not the OS's — Qt only moves bytes across on a read
> notification, so a socket whose thread has not returned to its event loop reports `0 available`
> while the OS buffer is full. That is why "available = 0" looked like proof there was no unread
> data, and why the earlier `readAll()` drain did nothing. `waitForReadyRead()` is what forces the
> transfer. The `msleep(50)` row is the control: wall-clock time alone does not help, because
> nothing reads during it.

**The fix** is `drainBeforeClose()`, called from `detachMainSocket()` and `detachHeartbeatSocket()`
before `abort()`. Bounded by `kDrainBeforeCloseAttempts = 50` one-millisecond attempts, exiting on
the first that finds nothing — so the usual cost is one attempt and the pathological case is ~50 ms
per socket, ~250 ms for a whole graceful teardown, inside `disconnectAndWait()`'s 3000 ms guard.

`abort()` is **kept deliberately**: once the buffer is empty it produces an ordinary FIN, and unlike
`disconnectFromHost()` it cannot block. D2's graceful close is *not* reinstated — it did not move
the number, and its one independent argument (flushing a pending write) remains unproven.

**Verification (measured 2026-09-09):** the 20-run bar D2 specified, met for the first time —
`vision_output_device_test` **0 / 20**, `vision_tcpip_client_device_test` **0 / 20**.
Full suites: **11 / 0** and **10 / 0**; contract test **151 / 0**; `mc_frame_test` **54 / 0**;
`modbus_device_test` **25 / 0**. Umbrella clean, both shells relinked 15:21.

**Negative check.** `drainBeforeClose()` made a no-op → **13 / 20** and **16 / 20**. The flake
returns harder than the pre-fix baseline, which is the rate this defect always had. Restored.

---

### Checkpoint D — all four device suites green with no known flake ✅ **MET 2026-09-09**

*Recorded as a miss first, and left that way for the rest of Phase E — the headline (**"the first
time since Phase 5 with no known flake"**) was written assuming D2 would close item 32, and D2's
diagnosis was refuted. It was met only after D2 reopened and the real cause was found.*

- [x] Umbrella build clean; both shells relink (**2026-09-09 15:21**). Contract test **151 / 0**
      (shape 149 == 149); `mc_frame_test` **54 / 0**; `modbus_device_test` **25 / 0**;
      `vision_output_device_test` **11 / 0**; `vision_tcpip_client_device_test` **10 / 0**.
      The written target of `133 / 0` is one of this plan's additive estimates, superseded by the
      observed totals after Phases D and E.
- [x] **No known flake — for the first time since Phase 5.** Item 32 is closed by
      `drainBeforeClose()` (see D2 above): the 20-run bar it specified now reads **0 / 20** in
      **both** duplicated copies, and removing the fix returns them to **13 / 20** and **16 / 20**.
- [ ] **OWNER-RUN:** vision-output cable pull and restore recovers without an application restart.
      This is D1's field check — it exercises the wedge D1 removed, which no automated test can
      reach from outside a real link loss.

      > **Observed FAILING on 2026-09-09; recorded here 2026-09-14.** On the cell, a heartbeat timeout
      > put the task in Recovering, and after the cable was restored only the **heartbeat** link came
      > back. The device was a `VisionTcpipClientDevice`, whose status predicate requires both links,
      > so it could not report healthy, and the task stayed Recovering until the runtime was stopped.
      > At 15:58:10 the same day the same device did recover normally, when both links returned. Filed
      > as **backlog 70** with the log sequence; investigation paused by the owner 2026-09-10. This line
      > stays open.

**Phase D outcome: D1 landed and is proven; D2 was reverted, reopened, correctly diagnosed and
closed.** The only thing left in Phase D is the owner-run cable pull.

---

## Phase E — PLC write acknowledgement (D3)

> **Stop rule, stated before the work starts.** D3 is unimplementable until a write can report
> completion. If **E1** is not landed and green by Checkpoint D + 1, Phase E stops there, E2–E4 carry
> to the next phase, and the phase closes without the write policy — **and Phase F starts anyway**
> (nothing in F depends on E). Half of D3 — a retry policy over a completion signal that does not
> exist — would report success for writes that never reached the wire, which is worse than today's
> honest silence.

### Task E1: MC write completion — the precondition

**Description.** Give each queued `MCRequest` an optional correlation id, assigned at `pushRequest()`
and returned to the caller. `McProtocolDevice::requestFinished(McResult)` is **declared at
`mc_protocol_device.h:175` and never emitted** — the seam already exists as a dead signal. Emit it
exactly once per correlated ad-hoc write, from every terminal path: `response_handle()`'s
`ResponseOk` / `ResponseError` / `ResponseInvalid` (`:561-581`), the frame-build failure and send
failure in `request_handle()`, retry exhaustion in `retry_request_handle()`, and every path that
abandons a queue — `setDeviceLostConnect()` (`:776-785`), the queue clear at `:431`, and
`deviceDisconnect()`.

**A queued write must resolve exactly once, always.** A request that vanishes is worse than one that
fails: the caller waits forever.

**Split point (this task is at the M/L boundary).** *(a)* correlation id + `McResult` id + resolve on
the ack/nak path at `response_handle()`; *(b)* resolve-on-abandon across the retry, disconnect and
queue-clear paths. Land (a) then (b); **(b) is what makes the contract true**, so if only (a) lands,
E3/E4 do **not** start.

**Thread-safety hazard to name in the code, not only in the test:** `pushRequest()` takes `m_mutex`,
but `onSetCommActiveDevice()` (`:322-329`) deliberately pushes to `m_request_queue` **without** it,
documented at `:320-321` as safe only because it shares the polling call path. A pending-write map
touched by submission and by resolution crosses exactly that boundary.

**Acceptance criteria**
- [ ] A write that receives an ACK resolves once with `ok=true`; a NAK resolves once with `ok=false`
      and the frame's mapped error description.
- [ ] A write abandoned by retry exhaustion, by disconnect, or by the queue clear resolves once with
      `ok=false` and a reason naming which.
- [ ] `setDeviceLostConnect()` fails **every** outstanding queued request, not only the one in flight.
- [ ] The heartbeat M-bit toggle queued in `onSetCommActiveDevice()` carries no id and produces no
      completion traffic; polling requests likewise.
- [ ] No path resolves a write twice and none leaves one unresolved — asserted by counting
      resolutions against submissions in the test.
- [ ] Polling, retry, timeout, change detection and the comm-active heartbeat are behaviourally
      unchanged; the three C-frame codecs are untouched.

**Verification**
- [ ] Umbrella build clean.
- [ ] Device suite `mc_frame_test`, or a new `mc_device_test` if a transport stub is needed —
      **decide in the task and record the choice**, since `mc_frame_test.pro` does not compile
      `mc_protocol_device.cpp` and the device owns its transport with no injection point. If a stub
      is needed, creating that seam is part of this task and pushes it to the split.
- [ ] **Negative check.** Drop the resolve-on-disconnect path → the unresolved-write case goes red
      **as a timeout, not as a wrong value** — proving the test detects silence, which is the whole
      failure mode. Emit twice from `response_handle()` → the resolves-once case goes red. Restore both.
- [ ] **OWNER-RUN, Checkpoint E:** against the real C24, write a bit and confirm a single completion
      with the right outcome; pull the cable mid-write and confirm a failure completion rather than
      silence. **This also closes part of backlog 56** (write-command coverage on the real PLC),
      carried out of Phase 8 — say so in the closeout.

**Dependencies:** Checkpoint D
**Files:** `src/device/plc/mc_request.h`, `src/device/plc/mc_protocol_device.{h,cpp}`, the chosen
test `.pro` + `main.cpp`
**Size:** M *(split as stated)*

#### ✅ E1 LANDED 2026-09-09 (a and b)

**Test-suite decision, recorded as the task required: `mc_frame_test` is extended, no new
`mc_device_test`.** The stub forces a *seam*, not a second suite — and `mc_frame_test.pro` already
listed every header the device needs, because both transports (`mc_msg_tcp_client.h`,
`mc_msg_serial_port.h`) and the config are header-only. Adding the device cost **one line** of
SOURCES. A separate suite would have duplicated the whole codec dependency set to gain nothing.

**The seam is one virtual.** `initialize_mc_device()`'s transport switch moved into a protected
`virtual std::unique_ptr<McMsgInterface> createMsgInterface(McMsgItfType)`; the test subclasses it
and returns a stub that records frames and stages replies. Everything else in the harness is
shipped code — the real 3E codec, the real queue, the real polling state machine. A stub codec
would have proved nothing about completions raised on the frames the product actually sends.

> **No pending-write map, and that is the point.** The task named the hazard: `pushRequest()`
> holds `m_mutex` while `onSetCommActiveDevice()` deliberately does not, documented as safe only
> because it shares the polling call path — and a map keyed by id, written at submission and read
> at resolution, crosses exactly that boundary. Putting the correlation id and the resolved flag
> **on the request object** removes the boundary instead of guarding it: the id travels with the
> request through the queue, and whichever path the request dies on reads it from there.
> `resolveRequest()` is then the single funnel, so "exactly once" is one check in one place rather
> than a rule seven call sites are trusted to remember.

**Terminal paths wired:** `response_handle()` ResponseOk / ResponseError / ResponseInvalid (the
NAK carries the frame's own mapped end-code description, not a generic "failed"); `request_handle()`
frame-build failure and send failure; `retry_request_handle()` exhaustion, resolved there rather
than by the sweep so the reason names retry exhaustion — that write *was* sent, repeatedly;
`teardownConnection()`, which is the one place both `deviceDisconnect()` and
`setDeviceLostConnect()` reach the queue through; and `initialize_mc_device()`'s queue clear, which
would otherwise silently drop anything pushed while disconnected.

> **`Q_DECLARE_METATYPE(McResult)` was missing.** `requestFinished(McResult)` was declared and
> never emitted, so the omission cost nothing and nobody noticed. E1 makes it live, and without
> the registration a queued connection — the only kind `PlcRunner` makes — drops the argument
> silently. Added.

**Verification (measured 2026-09-09):** `mc_frame_test` **51 passed / 0 failed** (shape
`-functions` 49 == source 49), from a baseline of 44. Seven new cases: ack, NAK, polling-and-
heartbeat-are-silent, the untracked push still reports nothing, disconnect-abandon, lost-link
fails *every* queued write, and the counting form (resolutions == submissions, no id twice).

- **Negative check (a)** — resolve-on-disconnect dropped: **50 / 1**, exactly
  `test_a_write_abandoned_by_disconnect_resolves_once_as_failed`, and it fails on
  `finished.count() == 0` — **silence detected, not a wrong value**, which is what the task asked
  the check to prove. Suite runtime jumped to 7.5 s on the QTRY timeout. Restored.
- **Negative check (b)** — a second `emit requestFinished` bypassing the guard: **49 / 2**, one
  more than predicted. The counting case failed on *"no id may resolve twice"*. Restored.

**Harness bug worth recording, because it wasted a build.** The first cut parked polling with a
60 s refresh interval to keep the round-robin quiet — but the timer is the ONLY thing that
dispatches anything, ad-hoc writes included, so four cases sat in QTRY timeouts waiting for a
write that was never sent (34.7 s suite, 4 red). The fix is to subscribe **no** M/D ranges instead:
the polling queue is then empty, `polling_query()` returns immediately, the comm-active toggle
(queued only on a completed round) never fires, and the only traffic is the write under test.

---

### Task E2: Modbus and virtual writes resolve too

**Description.** `ModbusTcpClientDevice::writeDigitalIoByName` returns `true` for a write parked in
`m_pendingIoWrites` (`:648-651`) — deliberate and correct (the comment at `:640-647` explains the
publish burst it fixes), but it means `true` does not mean "written" here either. Give the deferred
path the same completion contract E1 defines, so one controller policy serves both PLC families.
`ModbusTcpServerDevice` writes synchronously into its own space and resolves immediately;
`VirtualPlcDevice` resolves immediately with the value it recorded — the harness must not be the one
family that lies.

**Acceptance criteria**
- [ ] A deferred client write resolves once when `drainPendingIoWrites()` runs it, with the real outcome.
- [ ] A deferred write dropped because the link died resolves once with `ok=false`.
- [ ] Server and virtual writes resolve immediately, with the outcome their `bool` return already carries.
- [ ] Nothing resolves twice.

**Verification**
- [ ] Umbrella build clean; device suite `modbus_device_test`: **22 → 25**.
- [ ] Contract test unchanged at **133**.
- [ ] **Negative check.** Make the deferred path resolve at submission instead of at drain → the
      deferred-outcome case goes red and the immediate cases stay green. Restore.

**Dependencies:** E1
**Files:** `src/device/plc/modbus/modbus_tcp_client_device.{h,cpp}`,
`src/device/plc/modbus/modbus_tcp_server_device.h`, `src/device/virtual/virtual_plc_device.{h,cpp}`,
`tests/modbus_device_test/main.cpp`
**Size:** S

#### ✅ E2 LANDED 2026-09-09

**The contract is declared once, on `PlcDevice`, not three times.** `ioWriteFinished(quint64 id,
bool ok, QString message)` plus `writeDigitalIoTracked()` / `writeWordIoTracked()` — one shape all
four families speak, which is what "so one controller policy serves both PLC families" asks for.

> **Deviation, stated rather than made quietly:** the plan assigns the interface work to E3 and
> `plc_device.h` is not in E2's file list. Declaring three bespoke completion signals in E2 and
> unifying them in E3 would have been churn for its own sake. The default implementation on
> `PlcDevice` performs the write through `IPlcIoWriter` and resolves immediately, which is
> **already correct** for the two synchronous families — `ModbusTcpServerDevice` writes into its
> own register space, `VirtualPlcDevice` into a map — so neither has to restate anything, and a
> device implementing no writer at all resolves as *failed* rather than silently. E3 is left with
> what it is actually about: the runner signal and the command-kind decision.

**No existing signature changed.** The `IPlcIoWriter` bool methods keep their meaning and their
callers; `resolveIoWrite(0, …)` is a no-op, so an untracked write behaves exactly as before. The
Modbus client threads the id through its deferred path with a member consumed at the top of each
write method — captured into a local **first thing**, before `transact()`'s nested loop can
re-enter and clobber it. Parked writes carry the id through the park and are resolved by the
**replay**; the overflow drop and `teardown()` fail theirs rather than dropping them.

MC now emits both: `requestFinished(McResult)` keeps the MC-specific detail, `ioWriteFinished`
carries the family-neutral form, from one id space (`nextIoWriteId()`).

**Verification:** `modbus_device_test` **25 passed / 0 failed** — exactly the 22 → 25 the task
predicted. `mc_frame_test` 51 / 0. Contract test **145 / 0, unchanged**, as required.

> ⚠️ **The negative check failed to discriminate, twice, and the test was re-cut — twice.**
> Injecting the bug (resolve the deferred write at submission instead of at the replay) left the
> suite **fully green at 25 / 0**. The first version asserted only that every id resolved once with
> `ok=true`, which is true either way. The second added "at least one completion reports failure"
> and was *still* green: the write holding the wire fails on its own when the slave dies, so it
> satisfied the check without saying anything about the parked ones.
>
> The version that discriminates kills the slave **while the burst is parked** and asserts on
> `ids[1..]` specifically — the writes that were parked, excluding the one that held the wire.
> With the bug installed: **24 / 1**, failing on
> *"a write parked when the link died must report the failure of its REPLAY"*. Restored, green.
>
> This is the third time in this phase a test written here could not fail. The pattern each time
> is the same: the assertion was true of both the fixed and the broken build, and only asserting
> an outcome **the broken build cannot produce** fixes it.

---

### Checkpoint E-1 — the precondition is met, or Phase E stops ✅ **GATE OPEN 2026-09-09**

- [x] Umbrella build clean, both shells relink. `mc_frame_test` **51 / 0** (shape 49 == 49, from a
      baseline of 44); `modbus_device_test` **25 / 0** (from 22); contract test **145 / 0,
      unchanged**.
- [x] **The gate is OPEN.** E1(b) delivered resolve-exactly-once on every abandon path, and the
      proof is the negative check that matters: dropping the resolve-on-disconnect path turns the
      abandon case red **on `count() == 0`** — the test detects *silence*, which is the failure
      mode the whole precondition exists to remove. Every acceptance criterion of E1 and E2 is met:

      | Criterion | Where |
      |---|---|
      | ACK resolves once, `ok=true` | `test_a_tracked_write_that_is_acked_resolves_once_as_ok` |
      | NAK resolves once with the frame's mapped error | `test_a_tracked_write_the_plc_refuses_resolves_once_as_failed_with_the_end_code` |
      | Retry exhaustion / disconnect / queue-clear each name their reason | `retry_request_handle()`, `teardownConnection()`, `initialize_mc_device()`; two cases |
      | `setDeviceLostConnect()` fails **every** outstanding, not only the in-flight one | `test_a_lost_link_fails_every_queued_write_not_just_the_one_in_flight` |
      | Heartbeat + polling carry no id and produce no completion traffic | `test_polling_and_the_comm_active_heartbeat_produce_no_completions` |
      | Resolutions == submissions, no id twice | `test_resolutions_match_submissions_exactly` |
      | Deferred Modbus write resolves with the **replay's** outcome | `test_a_deferred_client_write_resolves_with_the_outcome_of_the_replay` |
      | Synchronous families resolve immediately with the real outcome | `test_a_server_tracked_write_resolves_immediately_with_the_real_outcome` |
      | Polling, retry, timeout, change detection, heartbeat unchanged; codecs untouched | the 42 pre-existing `mc_frame_test` cases still green |

- [ ] **OWNER-RUN, carried to Checkpoint E:** against the real C24, write a bit and confirm a
      single completion with the right outcome; pull the cable mid-write and confirm a failure
      completion rather than silence. **This also closes part of backlog 56.** Not reachable from a
      stub: the harness proves the state machine resolves, not that the C24 answers as modelled.

---

### Task E3: `IPlcIoWriter` and `PlcRunner` carry the result

**Description.** Plumb the completion up: the `IPlcIoWriter` write methods return (or take) a
submission id, and `PlcRunner::requestWriteDigitalIo`/`requestWriteWordIo` (`plc_runner.h:50-58`,
today `void`) return that id and emit a completion on the runtime thread.

**Decision to take in this task, with a recommendation.** Reuse the existing
`DeviceCommand`/`DeviceCommandResult` model (`device_command.h:25-35` has no PLC kinds) by adding
`PlcWriteDigital` / `PlcWriteWord`, rather than inventing a second result shape — A30 exists to stop
that drift and `CameraRunner` is the precedent. **If** adopting it drags in `DeviceCommandQueue`'s
timeout machinery and pushes past M, fall back to a narrow
`writeFinished(QString id, bool ok, QString message)` on `PlcRunner`, **and record why**, because the
next runner will follow whichever shape ships.

**Split point.** *(a)* the `IPlcIoWriter` signature change across the four implementations;
*(b)* the runner signal + the command-kind decision. Land in that order.

**Acceptance criteria**
- [ ] Both write requests return an id; the completion carries it, the outcome, and a reason on failure.
- [ ] Every write terminates in exactly one completion, including: device does not implement
      `IPlcIoWriter` (resolves as **failed**, never silent — the existing `errorOccurred` at
      `plc_runner.h:193`, `:200` is preserved or subsumed, never dropped), tag rejected
      synchronously, device thread torn down mid-flight.
- [ ] Correct for all four families: MC (asynchronous, via E1), Modbus client (deferred), Modbus
      server (synchronous), virtual (synchronous).
- [ ] If the `DeviceCommand` route is taken, `deviceCommandKindToString()` and the enum stay
      exhaustive (`device_command.h:64-87`) — a missing arm is a compiler warning, not a runtime
      "Unknown".
- [ ] Callers that ignore the id compile and behave identically.

**Verification**
- [ ] Umbrella build clean; both shells relink; contract test **133 → 136**, including a case
      parameterised over virtual / MC / Modbus-server.
- [ ] `modbus_device_test` 25 / 0; `mc_frame_test` green.
- [ ] **Negative check.** Drop the failure resolution for a device with no writer → that case goes
      red as a **timeout**. Make the Modbus server skip its completion → the family case goes red
      **for that family only**, proving the parameterisation distinguishes them. Restore both.

**Dependencies:** Checkpoint E-1
**Files:** `src/device/device_capabilities.h`, `src/runtime/plc_runner.h`,
`src/runtime/device_command.h`, the four `IPlcIoWriter` implementations,
`tests/architecture_contract_test/main.cpp` *(over the ~5 guideline — this is why the split above is
mandatory, not optional)*
**Size:** M *(split as stated)*

#### ✅ E3 LANDED 2026-09-09

**Decision, with the reason the task demanded: the narrow signal, not a `DeviceCommand` kind.**
`DeviceCommandQueue` is a one-at-a-time FIFO whose policies are `RejectWhenBusy`,
`QueueWhenBusy` (capped at `maxPending = 16`) and `QueueFull`. PLC handshake writes arrive in
**bursts** — `publishInitialReadyOutputs()` sends ten — and the Modbus client's entire
deferred-write parking exists because refusing a burst *"silently dropped nine writes out of ten
… and the runtime's outputs simply never reached the PLC"*. Routing writes through that queue
would rebuild the defect E2 just finished paying for. So:
`PlcRunner::writeFinished(quint64 id, bool ok, QString message)`.

*(The task suggested a `QString` id; `quint64` is what the device layer already uses and a string
would mean converting at both ends for nothing.)*

**The id is TAKEN by the device, not returned — a threading decision, not a style one.** E2 shipped
the returning form; E3 inverted it. `PlcRunner` must hand its caller an id synchronously and only
then queue the write onto the device thread, so a device that minted its own id would force a
device-id → runner-id map written on one thread and read on another. One id space owned by the
caller removes the map instead of guarding it. `pushTrackedRequest()` takes an optional id for the
same reason, defaulting to self-allocation for direct callers.

**`errorOccurred()` is preserved, not subsumed.** The device panels and the recovery policy both
listen to it; the forward emits it *and* `writeFinished`, so a failed write stays visible to every
consumer that has not been taught the new signal.

**Verification:** contract test **147 passed / 0 failed** (shape 145 == 145, from 143). Two cases,
not the three the task estimated — the parameterised family case covers virtual and Modbus server
in one, and the MC family is covered at device level by `mc_frame_test`'s seven E1 cases rather
than duplicating its transport stub here.

- **Negative check (a)** — no-writer resolution dropped: **146 / 1**, on `finished.count() == 0`.
  A timeout, which is what the task asked the check to prove.
- **Negative check (b)** — Modbus server's completion suppressed: **146 / 1**, the family case.
  **The first run of this check exposed a flaw in the test:** `QTRY_COMPARE` failed with
  *"Actual (finished.count()): 0"* and nothing else, so the parameterisation did **not**
  distinguish the families — a virtual-family break would have looked identical. Re-cut with
  `QTest::qWaitFor` + a labelled `QVERIFY2`, the same check now fails with
  *"modbus-server: both writes must resolve; got 0 of 2"*. Restored, green.

---

### Task E4: The controller policy (D3)

**Description.** In `publishBoolSignal()` / `publishNumberSignal()` (`:801-832`), track completions
for the **five handshake signals only** — `bTaskReady`, `bMatchingFinished`, `bTaskFault`,
`nFaultCode`, `nDetectedNumber`. On failure, retry a bounded number of times; on exhaustion,
`abortCycle(LocalizationFaultCode::PlcWriteFailed, …)`. Every other signal is log-only, exactly as
today. Add `PlcWriteFailed = 301` to `localization_fault_code.h` (the 300s are PLC per the header's
own grouping at `:19`; 300 is `PlcLost`) with its `localizationFaultCodeName()` arm.

Those five are the signals the PLC's own logic waits on: a lost `bMatchingFinished` hangs the PLC
forever, and a lost `bTaskFault` is worse — the PLC believes the cycle succeeded and picks on results
that were never valid. Retrying all fourteen would turn a degraded link into a write storm.

**Acceptance criteria**
- [ ] A failing `bMatchingFinished` write, retried to exhaustion, aborts the cycle with **301** and a
      task-log ERROR naming the signal and the tag; a write that succeeds on retry 2 produces no
      fault and no abort.
- [ ] A failing `bMatchingLowArea` write logs and does **not** retry and does **not** abort — and its
      log is **not** deduplicated into silence (`reportRoleError()` `:1752-1755` dedupes by message;
      either bypass it for this class or make it count-and-report, following the
      `m_consecutiveAutoRecoveries` pattern already in the file).
- [ ] **The recursion trap is closed in code, not only in the test:** the abort path publishes
      `bTaskFault` and `nFaultCode` — two of the five tracked signals — over the link that just
      failed. Those publishes must not re-enter the retry machinery. Escalation is attempted once,
      then the runtime falls back to the task log and `runtimeFault()`, which reach the operator
      without the PLC.
- [ ] Writes issued while the PLC role is disconnected are **not** counted against the budget — a
      disconnected role is the recovery policy's business, and double-faulting it would raise 301 on
      every cable pull.
- [ ] The retry budget and interval are named constants beside `kFaultAutoRecoverMs`, each with its
      reason; a retry in flight is cancelled with the cycle.
- [ ] `test_localization_fault_code_values_are_stable` (`main.cpp:804`) updated for 301; **no
      existing code renumbered**.

**Verification**
- [ ] Umbrella build clean; contract test **136 → 141**, using a `VirtualPlcDevice` mode that fails a
      named tag's write: retry-then-succeed, retry-then-abort, advisory-logs-only,
      dead-PLC-no-recursion, fault-code-stability.
- [ ] **Negative check.** Extend the policy to every signal → the advisory case goes red and the
      handshake cases stay green. Remove the recursion guard → the dead-PLC case must **fail**, not
      hang; assert a bounded abort count so it does. Restore both.
- [ ] **OWNER-RUN, Checkpoint E:** with the PLC link pulled mid-cycle, the task aborts with 301
      rather than reporting a completed cycle whose outputs never arrived, and recovers when the link
      returns.

**Dependencies:** E3
**Files:** `src/model/localization_runtime_controller.{h,cpp}`, `src/model/localization_fault_code.h`,
`src/device/virtual/virtual_plc_device.{h,cpp}`,
`docs/domains/task_localization/plc_signal_contract.md`, `tests/architecture_contract_test/main.cpp`
**Size:** M

#### ✅ E4 LANDED 2026-09-09

`PlcWriteFailed = 301` with its name arm and both fault tables. Five signals tracked
(`isHandshakeSignal()`), `kPlcWriteRetryBudget = 3` and `kPlcWriteRetryDelayMs = 40` beside
`kFaultAutoRecoverMs`, each with its reason in the header — three attempts at 40 ms bound the whole
escalation at roughly 120 ms, inside the 2000 ms auto-recover window that follows.

**The recursion trap is closed in code, and the guard is one flag in one place.**
`m_plcWriteEscalating` is set across `abortCycle()`, so the `bTaskFault`/`nFaultCode` the abort
publishes over the just-failed link are **not** tracked. Escalation is attempted once; the task log
and `runtimeFault()` then carry the news through the two channels that do not need the PLC.

**A disconnected role is not counted**, checked in `trackHandshakeWrite()` against the role's own
`connectStatus()`: counting a cable pull against this budget would raise 301 on top of the
`PlcLost` the recovery path already reports — two faults for one cause, the second of them wrong.
`clearPendingWrites()` runs at the top of `abortCycle()` and in `clearRoleContext()` for the PLC
role, so a retry in flight dies with its cycle rather than firing a stale handshake value into the
next one.

**Hardware-free failure injection:** `VirtualPlcDevice::failWritesForTag` — a **count** per tag,
not a flag, so a test can say "fail twice, then succeed" and drive the budget from both sides.
`-1` refuses forever. Losing the *connection* already had `forceConnectionStatus()`; losing a
*write* on a healthy link had nothing, which is why D3 had no test surface before.

> **No test-only publish hook was added.** The four cases drive the real ready path —
> `markRuntimeReady()` → `publishInitialReadyOutputs()` writes `bTaskReady`, `nDetectedNumber` and
> `nFaultCode`, three of the five, and by then the PLC role is Connected, which is the condition
> tracking requires. A `publishSignalForTest()` was written first and deleted: a hook that only
> tests use proves the hook works.

**Verification:** contract test **151 passed / 0 failed** (shape 149 == 149, from 147). Four new
behavioural cases; the fifth criterion — fault-code stability — folded into the existing
`test_localization_fault_code_values_are_stable` and `test_every_localization_fault_code_has_a_name`
rather than a new case, which is why the count is +4 and not the estimated +5.

- **Negative check (a)** — policy extended to every signal: **150 / 1**, exactly
  `test_an_advisory_write_failure_does_not_retry_and_does_not_abort`, with every handshake case
  staying green. Restored.
- **Negative check (b)** — recursion guard removed: **150 / 1**, and it **fails rather than
  hangs**, which is what the task specifically asked the bounded assertion to guarantee:
  *"escalation must not re-enter itself; got 9 faults"* against a bound of 3. Restored.

> ⚠️ **A latent harness defect from E1 surfaced here as a crash, and it is worth recording.**
> After E4 landed, `mc_frame_test` crashed in `test_the_untracked_push_still_reports_nothing` with
> an access violation inside `QIODevice::write` from a queued meta-call. Cause: `FakeMcPort`'s
> deferred reply used `QTimer::singleShot(0, lambda)` with **no context object**, so the lambda
> outlived the port — `deviceDisconnect()` destroys the transport, the queued call fires
> afterwards, and it writes into a destroyed `QBuffer`. It was wrong from the moment E1 wrote it;
> E2/E3's extra signal traffic changed the timing enough to lose the race. Fixed by binding the
> call to the port's own `QBuffer` member, which cancels it on destruction. Verified over three
> consecutive runs, 51 / 0 each.

---

### Task E5: The MC snapshot must mean "what was just read" (owner-reported 2026-09-09)

**Owner-reported, on the cell, with the PLC role bound to an MC-protocol device:** the runtime
faults at startup with **`CameraNotRegistered`** because the active camera number reads **0**,
although the project has camera 1 bound and registered — then re-arms itself a moment later.

**Mechanism.** `McProtocolDevice::polling_query()` emits the snapshot at the moment it *selects*
the last request of a round (`mc_protocol_device.cpp:627`), **before** `request_handle()` sends it
and long before its response is parsed:

```cpp
m_current_request = m_polling_request_queue.at(m_update_command_index++);
if (m_update_command_index >= m_polling_request_queue.count()) {
    ...
    emit pollingUpdate(m_device_map.clone());   // ← here
    onSetCommActiveDevice();
}
...
request_handle();                                // ← the last request is only sent now
```

`optimizeDeviceMap()` builds the queue M-ranges-then-D-ranges, so the last request is normally a D
range. Two consequences:

- Every MC snapshot is one response short — the last range carries the **previous** round's values.
- The **first** snapshot carries the zero-fill `update_d_map()` wrote (`device_map_d[addr] = 0`),
  because no D response has been parsed at all.

`nActiveCamera` is a D tag, so `PlcValueMap::valueForTag()` **finds** it and answers **0**. That is
not "unreadable" — it is exactly the "the master is holding 0" case C6 was built to catch, so
`validateCameraNumber(0)` refuses it and the runtime faults. One round later the real value arrives
as a change (the shadow is zero-filled too, so 0→1 *is* a change), `setActiveCameraNumber(1)` runs
and the C6 fix re-arms it. Hence "faults, then clears itself in an instant".

> **Why Modbus never showed this, and why that matters.** Both Modbus families publish only after a
> completed read pass — the client in `publishPollResults()` at the end of `onPollTimeout()`, the
> server because it owns its register space. C6 was designed, tested and field-confirmed entirely on
> Modbus. MC is the one family that never ran this path, and **item 58 part B named MC's first-poll
> behaviour as source-established but unobserved.** It has now been observed.

**The fix: one emit site, one contract.** Move the emit to the end of `response_handle()`, where the
round's last response has been parsed:

```cpp
if (m_data_update_state == QueryContinue) {
    polling_query();
} else if (m_data_update_state == QueryFinished) {
    emit pollingUpdate(m_device_map.clone());
}
```

Safe because `m_data_update_state` is assigned on **every** dispatch in `polling_query()` — the
ad-hoc branch always sets `QueryContinue`, the polling branch sets `QueryContinue` or
`QueryFinished` — so `QueryFinished` holds if and only if the round's last polling request is in
flight. `response_handle()` returns early on `!m_wait_for_response`, so the same round cannot emit
twice.

**Part (b): `is_first_time_polling` moves to the same place, for the same reason.**

*Added after the owner asked whether the flag could simply be **deleted**. It cannot — and the
reason is worth writing down, because "it only suppresses some log noise" is the obvious wrong
reading.* `update_m_map()` / `update_d_map()` zero-fill the shadow maps, so on the first round every
address has `hasPrevious == true` and every non-zero polled value is a change `0 → value`. Those
reach `valueChanged` → `handlePlcValues()` → **edge detection**. Delete the flag and a PLC holding
`bExecuteTrigger` high when the vision app starts delivers a rising edge, and **a cycle runs that
nobody triggered.** That is item 54's hazard, and the current behaviour is documented as deliberate:
*"A trigger already high before the runtime existed is therefore never delivered as a change, so no
rising edge occurs and no cycle starts."*

But the flag is mis-placed exactly like the emit — cleared at the wrap, so the round's **last range
escapes suppression**. And that is worse than it first looks: on an **M-only station**
`update_d_map()` adds no ranges, so the last request is an M range, and since `optimizeDeviceMap()`
merges into contiguous ranges there is typically only **one**. The entire first poll then escapes
suppression, on the station type where `bExecuteTrigger` lives. Item 59.4 has just established that
M-only stations are real.

Moving `is_first_time_polling = false` to the same tail makes `check_device_changed()` run with it
still `true` for the whole first round — the suppression finally covers what it was written to
cover — and the complete snapshot that E5(a) emits immediately afterwards is how the runtime learns
the initial state instead.

**Explicitly NOT in scope:** `onSetCommActiveDevice()` stays in `polling_query()`. It pushes to
`m_request_queue` *without* the mutex, documented as safe only because it shares the polling call
path. Moving it reopens exactly the boundary E1 removed the pending-map to avoid.

**Rejected alternative, recorded because the owner proposed it and it was reasonable.** Publish
after the full round *only on the first round*, keeping emit-at-wrap for the rest — minimising the
change to steady state. It works, but it costs a second flag (it cannot key off
`is_first_time_polling`, which is already false by then), two emit sites, and a snapshot contract
that differs by round. And the steady-state behaviour it preserves is itself wrong: **which** range
is stale depends on configuration, and on an **M-only station** — the configuration item 59.4 is
about — the stale range is an M range, which is where `bExecuteTrigger` lives and where Phase G2 /
item 54 plans to read the power-up level. One contract is worth more than the saved risk.

> The risk that motivated the alternative was **overstated by me and is corrected here**: a lost
> last response is retried up to 5 times by `retry_request_handle()`. A snapshot is only truly lost
> when the budget is exhausted and `setDeviceLostConnect()` fires — at which point the device is
> `LostConnected`, the runtime reports `PlcLost` (300), and `awaitPrimaryPlcSnapshot()` degrades to
> the project default with a USER-level warning. That is the designed degrade path, not a new hole.

**Acceptance criteria**
- [ ] The first `pollingUpdate` after connect carries the values actually read for **every**
      subscribed range, M and D — not the zero-fill.
- [ ] Exactly one snapshot per completed round; none on a round whose last response never arrives.
- [ ] Polling cadence, retry, timeout, change detection and the comm-active heartbeat are
      behaviourally unchanged, and the three C-frame codecs are untouched.
- [ ] An M-only station (no D ranges configured) publishes a complete snapshot too — the case where
      the last request is an M range.
- [ ] **(b)** No `valueChanged` is emitted for **any** range during the first polling round,
      including the last one. The runtime learns the initial state from the snapshot, not from a
      change stream that would read it as edges.

**Verification**
- [ ] Umbrella build clean; `mc_frame_test` **51 → 54**; contract test and `modbus_device_test`
      unchanged.
- [ ] The `FakeMcPort` harness is upgraded to answer **per request** rather than replaying one
      canned frame; without that a test cannot tell M data from D data and would pass either way.
- [ ] **Negative check (a).** Put the emit back at the wrap → the first-snapshot case goes red **on
      the D value being 0**, not on a timeout, and the existing E1 cases stay green. Restore.
- [ ] **Negative check (b).** Put `is_first_time_polling = false` back at the wrap → the
      first-round-is-silent case goes red **because a change was reported for the last range**, and
      the snapshot case stays green — proving the two halves are independent. Restore.
- [ ] **OWNER-RUN:** start the runtime against the MC device with camera 1 registered and confirm
      **no** startup fault at all — not a fault that clears itself.

**Dependencies:** E1 (the `createMsgInterface()` seam is what makes this testable at device level)
**Files:** `src/device/plc/mc_protocol_device.cpp`, `tests/mc_frame_test/main.cpp`
**Size:** S

#### ✅ E5 LANDED 2026-09-09 (a and b)

Two lines moved from the wrap in `polling_query()` to the tail of `response_handle()`, under
`else if (m_data_update_state == QueryFinished)`. `onSetCommActiveDevice()` stayed put, as scoped.

**Harness upgrade, and it was load-bearing.** `FakeMcPort` gained a `responseQueue` consumed in
send order, with `stagedResponse` kept as the fallback so the seven E1 cases are untouched. Without
it every send replays one canned frame, and a case asserting *"the snapshot carries the D values"*
would pass whether or not the D response had been parsed — which is the entire question. The reply
is also now captured **by value** into the deferred lambda: it belongs to the request that was
sent, not to whatever the queue holds when the timer fires.

Three cases, `mc_frame_test` **51 → 54**:

- `test_the_first_mc_snapshot_carries_the_values_that_were_read` — a **D-only** station, so the
  round's single request *is* its last one. Asserts `D2000 == 7` in the first snapshot, against the
  `0` the map is pre-filled with.
- `test_an_m_only_station_also_publishes_a_complete_first_snapshot` — the configuration where the
  last request is an **M** range, which is where `bExecuteTrigger` lives. A 16-bit M read goes
  word-aligned through `parse_read_bit_from_word`, so the reply is built with the same header
  helper.
- `test_the_first_polling_round_reports_no_changes_but_the_second_does` — part (b). Round 1 reports
  nothing; round 2's `D2000 → 9` is reported normally, proving the suppression is for the first
  round only and not permanent.

**Verification (measured 2026-09-09):** `mc_frame_test` **54 / 0** (shape `-functions` 52 == source
52); `modbus_device_test` **25 / 0**; contract test **150 / 1**, the single failure being
`test_both_shells_take_the_same_instance_key` — the documented environmental collision with a live
`ncr_picking.exe`, not a regression.

- **Negative check (a)** — emit returned to the wrap: **52 / 2**, both snapshot cases red **on the
  value being 0** (`Actual (value.toInt()): 0` / `Expected: 7`), not on a timeout, exactly as the
  task required. Part (b)'s case stayed **green**, which is the independence evidence from one side.
- **Negative check (b)** — `is_first_time_polling = false` returned to the wrap: **53 / 1**, only
  the first-round-silence case, failing on `reportedAtFirstSnapshot == 1` against `0` — one change
  *was* reported during round 1, which is precisely the leak. Both snapshot cases stayed green:
  independence from the other side. Restored.

> **A test-shape bug of mine, recorded because it is the same family as the three before it.** The
> first cut of the part-(b) case asserted `QTRY_COMPARE(snapshotCount, 1)`. At a 20 ms refresh the
> rounds keep arriving, so "wait until the count is 1" is a moving target that has already passed —
> it failed on **`snapshotCount == 460`**. The invariant belongs to *the instant the first round
> ends*, so it is now sampled inside the first `pollingUpdate` (`reportedAtFirstSnapshot`) instead
> of by a QTRY afterwards.

**Umbrella relink is OUTSTANDING**, blocked rather than failed: `LNK1104: cannot open file
ncr_picking.exe` because the owner's build (13:56) was running while this landed (source 14:33).
The running shell therefore does **not** contain E5. The library and all three suites are built and
green; only the two shell executables are stale.

---

### Checkpoint E — a degraded PLC link fails loudly

- [x] Umbrella build clean; both shells relink (**2026-09-09 14:39, after E5**). Contract test
      **151 / 0** (shape 149 == 149); `mc_frame_test` **54 / 0** (shape 52 == 52);
      `modbus_device_test` **25 / 0**. The written `141 / 0` is one of this plan's additive
      estimates and is superseded by the observed total.

      *An earlier run of this checkpoint read 150 / 1 on the contract suite, failing
      `test_both_shells_take_the_same_instance_key`. That was the documented collision with a live
      `ncr_picking.exe` holding the single-instance guard — the same run could not relink either
      (`LNK1104`). Re-measured with the app closed: 151 / 0. Recorded rather than quietly re-run,
      because "the suite went green on the second try" is exactly the shape a real flake also has.*
- [ ] `vision_output_device_test` / `vision_tcpip_client_device_test` — re-measured 2026-09-09 after
      the E5 relink: **10 / 1** and **9 / 1**, both on `test_disconnect_notice_on_graceful_close`.
      That is **item 32**, whose rate D2 measured at 5–13 failures in 20 runs; two in one batch is
      inside that. Nothing in Phase E touches vision output. "All device suites green" cannot be
      ticked while item 32 stands — see Checkpoint D, where the same line is open for the same
      reason.
- [x] **The stop rule did not fire.** E1(b) delivered resolve-exactly-once on every abandon path,
      so E3 and E4 were cleared to start and both landed. Every Phase E negative check is recorded
      at its task with the observed counts.
- [x] **OWNER-RUN — E5, confirmed on the cell 2026-09-09.** Runtime started with the PLC role bound
      to the MC device and camera 1 registered: **no startup fault at all**. Not a
      `CameraNotRegistered` that clears itself an instant later — none. The defect that opened E5 is
      gone.
- [x] **OWNER-RUN — E1, confirmed on the cell 2026-09-09.** Against the real C24: pulling the cable
      mid-write produces a **failure completion**, not silence. That is the half the stub could
      never prove — `requestFinished`/`ioWriteFinished` resolving from a real abandon path on real
      hardware — and it is the precondition D3 was blocked on.
      **Closes part of backlog 56** (write-command coverage on the real PLC), carried out of Phase 8.
- [ ] **OWNER-RUN — E4: BLOCKED, and not by the code.** The owner cannot run it: *"chưa giả lập
      được trường hợp PLC từ chối write"* — there is no way to make a healthy PLC refuse a write on
      demand. `VirtualPlcDevice::failWritesForTag` exists for exactly this and covers the bench, but
      it is the virtual device; a real MC or Modbus PLC has no such switch.

      **This is a test-access problem, not an unfinished implementation.** The policy is exercised
      by four automated cases plus two negative checks; what is unconfirmed is that a *real* PLC's
      refusal travels the same path. Two recipes that need no PLC cooperation, neither yet tried:

      | Family | Recipe | Why the write fails on a healthy link |
      |---|---|---|
      | **Modbus client** | Map a handshake signal (e.g. `bMatchingFinished`) to a **discrete-input** tag | `writeDigitalIoByName()` refuses it locally: *"a master cannot write discrete inputs"* — the protocol defines no function code for it. The link stays up, so this is 301's exact precondition, not `PlcLost` |
      | **MC** | Map a handshake signal to a device address the PLC does not have | The C24 answers with a non-zero end code; `response_handle()` resolves the write failed with that code, the retry budget runs out, 301 |

      > ⚠️ Check first whether **C4's signal-map gate** refuses the Modbus recipe at setup. The gate
      > asks whether the device *provides* the tag, and a discrete input is provided — so it should
      > pass and fail later at write time, which is what makes the recipe work. If the gate refuses
      > it instead, the recipe is wrong and the MC one is the only route.

      Until one of these runs, **E4 is verified on the bench and unverified on hardware**, and that
      is what this checkpoint records. It does not block Phase F.

> **What E4 changes on a live cell, stated plainly before it is run there.** Every
> `bTaskReady` / `bMatchingFinished` / `bTaskFault` / `nFaultCode` / `nDetectedNumber` write is now
> tracked and, on failure, re-issued up to twice before the cycle aborts with 301. On a healthy
> link nothing changes — the completion arrives ok and the record is dropped. The behaviour is new
> only where writes were previously failing silently, which is exactly the condition D3 exists to
> surface.

---

## Phase F — configuration and observability

### Task F1: Robot pick check belongs to the task, not to the transport (D2 / item 57)

**Description.** Per D2. Add `RobotKinematicCheckConfig` to `TaskLocalizeConfig`
(`kSchemaVersion` 3 → 4), persist it, and make `buildRuntimeContext()` (`task_localization.cpp:691-705`)
read it from the **task config**, ignoring `IResultOutputDevice::robotKinematicCheckConfig()` for
this purpose. Add the guard: `enabled == true` with a null **or unusable** checker is a hard setup
error — which needs `RobotKinematicPickingChecker::isReady()` exposing `m_robotValid` (`:149`),
because a wrong preset name currently fails **closed** (`isPickable()` `:195` returns false for
everything) and reads on the dashboard as "nothing is pickable today" rather than "the preset is
misspelled".

**Not a `Q_PROPERTY`.** `RobotKinematicCheckConfig` carries a nested `QVector<PickPathPoint>` the
property browser cannot render; a `P_PROPERTY_*` macro would add a display name, a marker entry and
`totalNames` churn for a control nobody can use. It gets `robotCheckConfig()` /
`setRobotCheckConfig()` accessors and is edited by the existing widget. State the reason in the header.

**Scope fence — do not half-migrate.** The **device-side** advisory check in
`VisionTcpipDeviceBase::runKinematicCheck()` (`:101-103`) keeps reading its own device config. D2
moves the *task runtime's* checker only. `IResultOutputDevice::robotKinematicCheckConfig()` **stays**
(both Modbus devices implement it) with a doc comment saying the localization task no longer reads
it — deleting a method with live implementors is a separate decision.

**Split point.** *(a)* config + persistence + runtime read + guard + `isReady()` + tests;
*(b)* host the existing `RobotKinematicCheckWidget` (`robot_kinematic_check_widget.h:47-56` —
`setConfig()` / `config()` / `configChanged()`, model-free, 517 lines) in the localization setting
widget. **OWNER-RUN.** Without (b) the setting moves somewhere nobody can reach, which is the shape
of failure this task exists to fix — so (b) is required for the item to close, and if it carries,
item 57 stays open.

**Acceptance criteria**
- [ ] Settings round-trip through `toJson()`/`fromJson()`; a v3 document loads with the check
      disabled; a v4 document is refused by a v3-era build.
- [ ] `buildRuntimeContext()` fills `robotCheckConfig` from the task config for **every** vision-output
      family — asserted with a result-output-capable **PLC** runner bound to the role. This is the
      case that has never worked.
- [ ] `enabled=true` + unregistered preset name → setup fails naming the preset.
      `enabled=true` + null checker → setup fails naming the reason. `enabled=false` + null checker →
      setup valid, unchanged behaviour.
- [ ] `device_capabilities.h:152-159` is rewritten; the sentence *"a default-constructed value (check
      disabled) is a valid answer"* does not survive.
- [ ] **`totalNames` is asserted unchanged at 124** — an accidental `P_PROPERTY_*` then fails the build.
- [ ] *(b)* The panel is reachable from the localization task settings; values persist across
      save/reload; the vision-output device widgets keep their own copies untouched (deprecating them
      is a separate decision, filed in Z3).

**Verification**
- [ ] Umbrella build clean; both shells relink; contract test **141 → 146**;
      `modbus_device_test` 25 / 0; translation sweep clean.
- [ ] **Negative check.** Restore the device-sourced read → the PLC-family case goes red and the
      vision-output family case stays green. Remove the `isReady()` half of the guard → the
      wrong-preset case goes red and the null-checker case stays green. Restore both.
- [ ] **OWNER-RUN, Checkpoint F:** on the dual-role Modbus cell, commission the pick check from the
      **task** settings and confirm unreachable poses are skipped — the acceptance Phase 8 carried
      forward as item 57 and never got.

**Dependencies:** Checkpoint E (or Checkpoint E-1 if the stop rule fired)
**Files:** `src/model/task_localization_config.h`, `src/model/task_localization.cpp`,
`src/model/robot_kinematic_picking_checker.h`, `src/device/device_capabilities.h`,
`tests/architecture_contract_test/main.cpp` *(+ `src/ui/forms/task/localization_setting_widget.{h,cpp,ui}`
in (b))*
**Size:** M *(split as stated)*

> ### ✅ F1(a) + F1(b) LANDED 2026-09-10
>
> **What was built.** `TaskLocalizeConfig` gained `robotCheckConfig()` / `setRobotCheckConfig()`
> backed by a new `m_robotCheckConfig` member, persisted under `"robotCheckConfig"`, with
> `kSchemaVersion` **3 → 4**. `buildRuntimeContext()` now reads that instead of
> `IResultOutputDevice::robotKinematicCheckConfig()`. `RobotKinematicPickingChecker::isReady()`
> exposes `m_robotValid`. `rebuildPickingChecker()` changed from `void` to returning a reason
> string, and `setup()` turns a non-empty reason into a refusal.
>
> **Deviation from the plan's wording, stated rather than made quietly — the checker is still
> installed when the preset does not resolve.** The plan implies an unusable checker need not be
> built. It is built anyway, because leaving `m_pickingChecker` null means *"matching not gated"* —
> fail-OPEN — and would send unreachable poses to the robot if any path ever reached it without
> setup's refusal. The refusal is what changes the outcome; the install only decides which way it
> fails if the refusal is ever bypassed.
>
> **Second deviation: the calibration reason is NOT suppressed by the calibration error.** The
> first cut suppressed the pick-check message whenever any other error existed, which would have
> hidden it in exactly the case the plan asks to assert. Only a refused camera *index* suppresses
> it now — that one really is derived noise.
>
> **F1(b) is a dialog, not an inline frame.** `RobotKinematicCheckWidget` carries a pick-path table
> and an FK/IK tester and is far taller than the other frames on a settings page that has **no
> scroll area of its own** (`localization_task_widget.cpp:659` inserts it straight into
> `content_stack`). A new `frame_robot_check` shows a one-line summary plus a **Set…** button that
> opens the editor in a `QDialog` — the same shape `onWorkspaceSetRequested()` already uses for
> ROI editing. Inline would have squashed the four frames above it.
>
> **Verification, observed 2026-09-10.** Contract test **151 → 157 / 0**, suite shape verified
> `-functions` **155 == 155** `void test_` (the plan's written *"141 → 146"* is a stale additive
> estimate; 151 was the measured Checkpoint E baseline). `totalNames` asserted **unchanged at 124**
> — the new setting is deliberately not a `P_PROPERTY_*`, so no `kDisplayNameSources[]` entry was
> needed. Umbrella build clean; **both shells relinked 2026-09-10 08:51 / 08:52**.
>
> **Negative check 1 — restore the device-sourced read in `buildRuntimeContext()`:**
> `..._when_a_plc_carries_the_output_role` went **red**, and
> `..._vision_output_family_cell_still_starts...` **stayed green** — 156 / 1, exactly the
> discrimination the plan predicted. Restored.
>
> **Negative check 2 — replace `!checker->isReady()` with `false`:** 155 / 2. The unregistered-preset
> case went red *and so did the end-to-end PLC case*, which shares the same guard — recorded rather
> than glossed, since the plan predicted one red. The two cases that had to stay green did:
> `..._without_calibration_names_the_reason` and `..._disabled_pick_check_leaves_setup_valid`.
> Restored; re-measured **157 / 0**.
>
> **A test-shape bug of mine, worth recording because it is the third of its kind in this plan.**
> The end-to-end case first read `logSpy` once after `QTRY_COMPARE(taskState, Faulted)` and found
> the log **empty** — while the refusal was demonstrably emitted. The controller → task log forward
> is a `Qt::QueuedConnection` (`task_localization.cpp:591`) and the Faulted transition is
> synchronous, so `QTRY_COMPARE` returned on its first check without ever spinning the event loop.
> Re-cut as `QTRY_VERIFY_WITH_TIMEOUT` over a re-scanning lambda, and the failure text now prints
> the whole task log — a bare "not found" could not tell *"it read the device"* from *"it refused
> for an unrelated reason"*, and those need opposite fixes.
>
> **Still owner-run:** the dual-role Modbus cell acceptance (commission the check from the **task**
> settings, confirm unreachable poses are skipped). Until that runs, **item 57 stays open**.

---

### Task F2: The dashboard reports what is actually connected (item 25)

**Description.** `LocalizationDashboardWidget` is constructed at `runtime_shell_window.cpp:472`, nine
lines **before** `beginRuntime()` at `:481`, so `wireConnectionLamp()` (`:299-330`) resolves
`taskRunner()->runnerFor(id)`, gets null, sets "—" and never retries. All three lamps are dead for
the life of the runtime. Re-wire on `ITask::runtimeStarted()` (emitted at `task_localization.cpp:200`)
and on `TaskRunner::phaseChanged()`, keeping the existing `devicesChanged` rebuild.

**Do not skip this because the lamps appear to work after C3.** `nActiveCamera`'s handler calls
`rebuildConnectionWiring()` (`:489-495`), and C3's setup-time status publish now lands after the
widget subscribes — so the lamps may come alive as a side effect. That is a coincidence, not a
wiring: it makes the PLC and output lamps depend on a camera signal firing.

> ⚠️ **A SECOND, INDEPENDENT CAUSE — field-confirmed 2026-09-08 (backlog 61). Re-wiring alone does
> not fix this task's symptom.** The dashboard holds a **copy** of the config taken at construction
> (`:180`) and refreshes it in exactly one place: the `devicesChanged` handler (`:209-215`). But
> `devicesChanged` fires only on assigning or unassigning a device (`itask.h:158`, `:168`) — **not**
> on a role-binding change. Editing "primary PLC" or "vision output" goes
> `localization_setting_widget.cpp:202/210` → `:545` → `task_localization.cpp:101` →
> `ITask::setTaskConfig()` → **`emit configChanged()`** (`itask.h:210`), and **nothing in the
> dashboard is connected to `configChanged`**. In the editor shell the widget is built once and
> cached (`localization_task_widget.cpp:619-625`), so the stale copy lives as long as the window.
>
> Everything read through `m_config.d->m_deviceBindings` is then wrong — the device **name labels**
> (`:421-422`), which device each lamp watches (`:290-292`), and `resolveActiveCameraDeviceId()`
> (`:365-373`). Observed: a Modbus client came up (`app_log_2026-09-08.txt:705`) and the dashboard
> showed neither the PLC nor the output device. Re-wiring on `runtimeStarted` / `phaseChanged`
> would have re-read the **same stale bindings** and shown the same nothing.
>
> **Both causes are in scope for this task.** Add a `configChanged` handler that re-reads
> `taskLocalizeConfig()` and then runs the existing `pushSignalTagsFromConfig()` +
> `updateTaskContext()` + `rebuildConnectionWiring()` trio.
>
> Note the camera lamp's refusal-path gap while here: `setActiveCameraNumber()` returns at `:189`
> and `:215` **before** publishing the index, so a refused selection updates no camera visual at
> all. C3 changes what is published on those paths; do not assume it closes this.

**Acceptance criteria**
- [ ] A role-binding change made in the Setting tab is reflected in the dashboard's device labels
      and lamp wiring **without** reopening the window — asserted by the owner run below, since the
      hop is UI-only.
- [ ] Lamps reflect real connection status within one second of `beginRuntime()`, without an operator
      action, and follow later status changes.
- [ ] Each lamp is **seeded** from the runner's current status at wiring time — a device already
      connected lights immediately rather than waiting for the next change.
- [ ] Re-wiring is idempotent: `m_connectionConns` is fully disconnected first, so several phase
      toggles cannot accumulate duplicates.
- [ ] A phase return to Idle drops the lamps back to "—" rather than freezing on the last colour; a
      bound device with no runner still shows "—" and an unbound role still shows "Not set", as today.
- [ ] The camera lamp still follows the **active** camera (`resolveActiveCameraDeviceId()`).

**Verification**
- [ ] Umbrella build clean; both shells relink; contract test unchanged at **146**.
- [ ] The only automatable assertion lives in **B2(a)**: that `runtimeStarted` fires after the runners
      are registered and attached. **Negative check on that half only:** remove the `runtimeStarted`
      connect → the B2 ordering assertion goes red.

      > **B2 landed in full on 2026-09-08 — B2(b) was not carried**, so the hop assertions do exist
      > and this task is **not** fully owner-run. `test_task_localization_begin_runtime_emits_runtime_started_after_runners_exist`
      > pins the ordering, and the two B2(b) cases pin the forwards. What remains owner-run here is
      > only what a widget test framework would cover, plus the stale-config half below.
- [ ] **OWNER-RUN — there is no widget-level test framework in this project and this cannot be
      claimed otherwise:** all three lamps show the true state within a second of runtime start;
      pulling a cable turns the right one red and restoring it turns it back; stopping and restarting
      the task re-wires rather than freezing.
- [ ] **OWNER-RUN, the stale-config half:** with the dashboard already open, change the primary-PLC
      or vision-output binding in the Setting tab and return to the dashboard **without closing the
      window**. The device label and the corresponding lamp must both follow. This is the exact
      2026-09-08 field failure and it is the only check that distinguishes the two causes — the
      re-wiring half passes it while still showing the wrong device.

**Dependencies:** F1
**Files:** `src/ui/forms/task/localization_dashboard_widget.{h,cpp}`
**Size:** S → **M** *(two causes, not one; the `configChanged` handler is new wiring, not a move)*

> ### ✅ F2 LANDED 2026-09-10
>
> **Both causes wired, in `initWidget()`.** Cause 1: `ITask::runtimeStarted` and
> `TaskRunner::phaseChanged` now both call `rebuildConnectionWiring()`. Cause 2: a new
> `ITask::configChanged` handler re-reads `taskLocalizeConfig()` into `m_config` and then runs the
> existing `pushSignalTagsFromConfig()` + `updateTaskContext()` + `rebuildConnectionWiring()` trio
> — which is what makes the device **labels** follow too, since `updateTaskContext()` writes
> `lbl_val_plc_device` / `lbl_val_vision_device` straight out of `m_config`.
>
> **Three acceptance criteria were already met and needed no code.** Recorded so a future reader
> does not go looking for changes that are not there: `wireConnectionLamp()` already **seeds** each
> lamp from `runner->device()->connectStatus()` before subscribing; `rebuildConnectionWiring()`
> already disconnects all of `m_connectionConns` first, so it is idempotent; and the camera lamp
> already resolves through `resolveActiveCameraDeviceId()`.
>
> **The return to Idle needed no separate handling either** — `phaseChanged(Idle)` re-runs the
> rebuild, by which point the runners are gone, so `wireConnectionLamp()` resolves null and sets
> "—" rather than freezing on the last colour.
>
> **`m_taskRunner` is connected once, not re-resolved.** It is constructed with the task and lives
> as long as it (`itask.h:390`), so there is no lifetime hazard in a single `connect` at
> construction.
>
> **The plan's negative check does not hold, and was replaced.** It reads *"remove the
> `runtimeStarted` connect → the B2 ordering assertion goes red"*. It cannot:
> `test_task_localization_begin_runtime_emits_runtime_started_after_runners_exist` observes
> `ITask::runtimeStarted` on the **task** and calls `runnerFor()/isAttached()` itself — it never
> touches the dashboard, so removing a dashboard connect changes nothing there. What that test
> actually guards is the **precondition** this fix depends on, so that is what was checked:
> `emit runtimeStarted()` was moved to the top of `beginRuntime()`, ahead of
> `syncRunnersWithDevices()`. Observed **155 / 2** — the B2 ordering case went red *and* so did
> `test_task_localization_incomplete_bindings_end_runtime_faulted_and_invalid`, which asserts the
> signal is not emitted on the faulted path. Restored; **159 / 0**.
>
> **Verification.** Contract test unchanged by F2 itself (**157 / 0** at the time it landed);
> umbrella build clean, both shells relinked.
>
> **Still owner-run, and this is the whole of the acceptance:** all three lamps live within a
> second of runtime start and following cable pulls; and — the half that distinguishes the two
> causes — changing the primary-PLC or vision-output binding in the Setting tab with the dashboard
> **already open** must move both the device label and the lamp, without reopening the window.

---

### Task F3: End-to-end cycle latency instrumentation

**Description.** Nothing measures a cycle end to end; `CycleResult` carries only `matchingTimeMs`,
reported by the matcher (`:1625`). The standing criterion *"revisit threading only on measured
latency evidence"* (`phase2_phase3_runtime_hardening.md`) therefore **cannot be evaluated at all**,
and neither can a customer question about cycle time. Timestamp the five boundaries the controller
already owns — trigger accepted (`startCycle()` `:1205`), grab finished (`:1565`), matching finished
(`:1604`), send finished (`:1677`), outputs published — carry them on `CycleResult`, log the
breakdown in the existing INFO summary at `:1700-1704`, and surface total cycle time as a dashboard KPI.

**This task does not depend on Phase E.** It needs a monotonic clock and `CycleResult` fields, and
nothing else. Chaining it behind the write-acknowledgement work would let an MC protocol difficulty
silently cancel the one deliverable that answers "what is my cycle time".

**Split point.** *(a)* timings measured, carried on `CycleResult`, reported in the task log;
*(b)* the dashboard KPI (OWNER-RUN).

**Acceptance criteria**
- [ ] Every stage is stamped from **one monotonic clock** (`QElapsedTimer` or `std::chrono::steady_clock`),
      never wall time, which an NTP step can move backwards mid-cycle.
- [ ] On a successful cycle every stage is populated and monotonically non-decreasing; the total is
      no smaller than `matchingTimeMs`.
- [ ] A faulted or aborted cycle carries the stages it reached and leaves the rest **unset** — never
      zero-filled, which reads as "instant". The slow stage before a timeout is the most interesting
      number in the phase.
- [ ] `matchingTimeMs` keeps its current meaning and is not redefined.
- [ ] The breakdown is one readable INFO line per cycle, not five; if that is too loud at cycle rate,
      make it a periodic summary and say so.
- [ ] No allocation, no formatting and no new synchronisation on the cycle path — timestamps are
      taken where the events already arrive.

**Verification**
- [ ] Umbrella build clean; contract test **146 → 148**: populated + monotonic + total ≥ matching;
      faulted-cycle partial fill.
- [ ] **Negative check.** Stop stamping the send stage → the monotonic case goes red and the
      partial-fill case stays green. Take the total from a second clock read before the send → the
      total ≥ matching assertion goes red. Restore both.
- [ ] **OWNER-RUN, Checkpoint F:** capture **20 cycles** on the cell and record the distribution in
      this file. **That number, not the test, is the deliverable** — it is what closes the
      threading-revisit criterion, and without it the instrumentation has proved nothing.

**Dependencies:** F2 (dashboard file)
**Files:** `src/model/localization_runtime_controller.{h,cpp}`,
`src/ui/forms/task/localization_dashboard_widget.cpp`, `tests/architecture_contract_test/main.cpp`
**Size:** M *(split as stated)*

> ### ✅ F3(a) + F3(b) LANDED 2026-09-10
>
> **What was built.** A `CycleTimings` struct on `CycleResult` carrying four
> `std::optional<double>` boundaries — `grabFinishedMs`, `matchingFinishedMs`, `sendFinishedMs`,
> `outputsPublishedMs` — all elapsed from one `QElapsedTimer` (`m_cycleClock`) started at
> trigger-accept in `startCycle()`. `std::optional`, not zero-initialised doubles, is the whole
> point of the "leaves the rest unset" criterion: a zero reads as *instant*.
>
> **`nsecsElapsed()`, not `elapsed()`.** `elapsed()` truncates to whole milliseconds, and on a
> fast cell a grab and a match can both land inside one — which would render the breakdown as
> simultaneous stages exactly where it is most interesting.
>
> **Two boundaries are stamped on ARRIVAL, ahead of the outcome check** — the grab and the send.
> A grab that came back empty and a send the device refused each still took however long they took,
> and per the task's own criterion that is the most interesting number in the phase. A camera that
> never answers is aborted by `onCameraCommandFinished()` instead and leaves the boundary unset,
> which is the honest record.
>
> **The clock starts before `publishCycleStartOutputs()` and the total is taken after
> `publishCycleSuccessOutputs()`**, so both handshake writes are inside the measured cycle. On a
> slow PLC link those writes are part of what the customer experiences as cycle time; excluding
> them would flatter the number.
>
> **One INFO line, as specified** — the existing summary at the tail of
> `onVisionOutputResultFinished()` gained a `formatCycleTimings()` clause reporting time spent
> **in** each stage (consecutive differences), not cumulative time:
> `cycle=142.3 ms (grab 98.1, match 39.7, send 4.2, publish 0.3)`. `matchingTimeMs` is still
> reported separately and is **not** redefined: it is the matcher's own measurement of its own
> work, and the controller's match stage answers a different question.
>
> **F3(b): a NEW KPI tile, not a relabelled one.** The existing `lbl_kpi_cycle_time` is captioned
> *"Matching time"* and honestly shows `matchingTimeMs` (only its widget name is misleading). A
> fifth tile `frame_kpi_cycle_total` — *"Cycle time"* — shows the end-to-end total, and shows
> **"—"**, never 0, when the cycle did not reach its last boundary.
>
> **Verification, observed 2026-09-10.** Contract test **157 → 159 / 0**, suite shape
> `-functions` **157 == 157** `void test_`. Umbrella build clean; both shells relinked.
>
> **Negative check A — stop stamping the send stage:** **158 / 1**. The monotonic case went red
> (*"a cycle that completed reached all four boundaries"*) and the partial-fill case stayed green.
> Exactly as predicted.
>
> **Negative check B — the plan's prediction was wrong, and this is what was observed instead.**
> The plan says *"take the total from a second clock read before the send → the total ≥ matching
> assertion goes red"*. Taking `outputsPublishedMs` off a freshly-started `QElapsedTimer` gave
> **158 / 1**, but the failure was the **monotonic** assertion, not the total ≥ matching one:
> `stages must not go backwards; got 12.0828, 51.6437, 61.9164, 0.0002`. That is not a fluke of
> ordering — *any* corruption of the total necessarily violates both properties at once, because a
> total that is too small is also smaller than the send boundary before it. So this check confirms
> the total is load-bearing but **cannot isolate which of the two assertions caught it**. Recorded
> rather than glossed. It is acceptable because both assertions carry distinct labelled failure
> text with the numbers printed, so a real regression still says which property broke. Restored;
> re-measured **159 / 0**.
>
> **Still owner-run, and it is the actual deliverable:** capture **20 cycles** on the cell and
> record the distribution here. Without that number the instrumentation has proved nothing, and
> the threading-revisit criterion stays unevaluable.

---

### Checkpoint F — configuration reaches the task, and the runtime can be diagnosed

- [x] **Umbrella build clean; both shells relinked 2026-09-10 09:12** (a second umbrella build after
      the sweep, so the regenerated `.qm` actually reaches the shells — without it the new
      translations never load). Contract test **159 / 0**, suite shape verified `-functions`
      **157 == 157** `void test_`. The written *"148 / 0"* is one of this plan's additive estimates
      and is superseded by the observed total; the measured Checkpoint E baseline was 151.

      > ⚠️ **On 2026-09-14 that suite briefly read 152 / 7**, for a reason unrelated to Phase F: the
      > repo's `app/` directory no longer resolved. **Resolved the same day** — the commissioning
      > shell's wiring was repointed to `components/app/` (backlog 65), and the suite is back to
      > **159 / 0** on the current tree.
- [x] **All four device suites green, shape verified before each total:**

      | Suite | `-functions` vs source | Observed |
      |---|---|---|
      | `mc_frame_test` | 52 == 52 | **54 / 0** |
      | `modbus_device_test` | 23 == 23 | **25 / 0** |
      | `vision_output_device_test` | 9 == 9 | **11 / 0** |
      | `vision_tcpip_client_device_test` | 8 == 8 | **10 / 0** |

- [x] **"No known flake" is now measured, not assumed — item 32 holds.** One clean pass is not
      evidence, so both vision suites were run **six more times each: 0 / 6 and 0 / 6 failed**.
      Against the pre-fix rate recorded for the same two suites — **2 / 6** and **5 / 6** — the
      `drainBeforeClose()` fix is holding. This is the line Checkpoint D and Checkpoint E both had
      to leave open.
- [x] **Translation sweep run 2026-09-10.** `finished 860→860, unfinished 458→465, vanished 33→33`
      — **0 newly vanished**, which is the criterion. The 7 new source texts are F1(b)'s and F3(b)'s
      new UI strings.
- [x] **Display-name total unchanged at 124**, asserted by the existing contract case. F1's new
      setting is deliberately not a `P_PROPERTY_*` — an accidental one would have failed the build,
      which is what that assertion is for.
- [x] **OWNER-RUN — F2 CONFIRMED IN FULL (2026-09-11 + 2026-09-14). Backlog 25 and 61 both close.**
      2026-09-11, the stale-config half: with the dashboard already open, changing the vision-output
      and primary-PLC bindings in the Setting tab updated the dashboard without reopening the window
      — the only check that tells the two causes apart (**item 61**). 2026-09-14, the liveness half:
      pulling the cable turned the lamp **red, then back to green** on reconnect (**item 25**).
      `app_log_2026-09-14.txt` corroborates the underlying transitions at `:1105`
      (`Ready -> Recovering`, `role=primary_plc deviceId=05 status=LostConnected`) and `:1113`
      (reconnect, 5 s later), so the lamp was following real status changes rather than a stale seed.
- [x] **OWNER-RUN — F1 CONFIRMED ON THE DUAL-ROLE MODBUS BINDING (2026-09-14). Backlog 57 closes.**
      The owner re-ran with a **Modbus TCP client carrying both roles at once** and confirmed
      unreachable poses are skipped, with the check commissioned from the **task** settings.
      `app_log_2026-09-14.txt` corroborates the binding independently: `:208-209` request
      `role= primary_plc deviceId= 05` and `role= vision_output deviceId= 05` — the same device
      (`Modbus_Client_1`, 192.168.1.59:801) — and 85 cycles then ran against it (08:48:35–08:51:57).

      That is item 57's literal case, the one Phase 8 carried forward and never got. The earlier
      2026-09-11 run had confirmed only the vision-output family (MC + `VisionOut_01`), which is why
      this line stayed open; it no longer is.
- [x] **SUPERSEDED by the 2026-09-14 run below — kept because it is how the F3(a) defect was
      found.** 67 cycles ran on 2026-09-11 but the deliverable could NOT be recorded, and that was a
      defect in F3(a), not in the run. The per-stage breakdown went only through
      `appendTaskLog()`, whose only consumer is the dashboard's task-log panel
      (`localization_runtime_controller.cpp:1506` → `task_localization.cpp:591` →
      `localization_dashboard_widget.cpp:274`). **It is never written to `app_log_*.txt`** — 0
      occurrences of `cycle=` in the 09-10 and 09-11 logs. This was established during the
      vision-output investigation two days earlier and not applied when F3 was designed; the unit
      tests could not catch it, because they read the signal, not a file.

      **What the app log does support**, reconstructed from the camera runner's
      `Camera command started/finished` lines and the task-state transitions (min / median / max):

      | Runtime session, 09-11 | n | cmd → grab done | grab done → trigger reset | total, upper bound |
      |---|---|---|---|---|
      | 16:52:27 – 16:52:37 | 3 | 199 / 199 / 232 | 306 / 308 / 372 | 505 / 507 / 604 |
      | 16:52:48 – 16:57:21 | 64 | 191 / **204** / 218 (p90 210) | 805 / 1013 / 1099 (p90 1067) | 1002 / 1217 / 1304 (p90 1276) |

      Four limits, each of which changes what the numbers mean:
      1. **The total is an upper bound, not the cycle time.** All 67 cycles ended *"Trigger reset.
         Runtime ready."* (`:985`): the task leaves `RunningCycle` only when the PLC drops
         `bExecuteTrigger`. So it includes the PLC program's reaction to `bMatchingFinished` plus up to
         one MC poll (50 ms refresh on this cell).
      2. **The middle column cannot be split.** Match, send, publish and the PLC's reset wait are all
         inside it — which is exactly what F3 was built to separate. The grab is the only stage the log
         isolates, and it is tight: 191–218 ms over 64 cycles.
      3. **The two sessions are two configurations, not one distribution.** The step from ~308 to
         ~1013 ms coincides with a runtime restart at 16:52:42 (`endRuntime` → `beginRuntime`), so a
         setting changed between them — the log does not say which. Within session 2 the middle column
         is bimodal: 17 cycles at 800–899 ms, 39 at 1000–1099 ms.
      4. **Wall-clock timestamps.** F3's criterion forbids that for the instrument; it is tolerable here
         only as a reconstruction across ~5 minutes.

      **The threading-revisit criterion therefore stays unevaluated.** What would close F3: write the
      same one-line summary to the app log as well (a `LOG_USER_INFO` beside the existing
      `appendTaskLog()`), then re-run 20 cycles. **Not done — awaiting the owner's go-ahead.**

      > **RESOLVED 2026-09-14 — the owner implemented that fix and re-ran.** The summary is now also
      > written at `localization_runtime_controller.cpp:2400` as
      > `LOG_DEV_INFO << "Task localization:" << cycleInfoLogString`, with `formatCycleTimings()`
      > hoisted into a local so both sinks share one string. **90 breakdown lines** are in
      > `app_log_2026-09-14.txt`. Note the level is **DEV**, not USER, so the line is only present
      > when the developer log is enabled — worth knowing before asking a customer site for it.

- [x] **OWNER-RUN — F3 DELIVERED 2026-09-14. This is the number the phase was owed.** 90 cycles
      logged, in two runtime sessions. Session 2 (**n = 85**, 08:48:35–08:51:57) is the
      representative one; all values in ms, measured by the instrument itself off one monotonic
      clock — not reconstructed from wall-clock log timestamps as the 09-11 attempt had to be:

      | stage | min | median | p90 | max |
      |---|---|---|---|---|
      | grab | 24.3 | **54.9** | 79.0 | 102.7 |
      | match | 196.9 | **241.0** | 279.0 | 313.5 |
      | send | 2.7 | **4.2** | 5.6 | 8.0 |
      | publish | 0.0 | **0.1** | 0.2 | 0.3 |
      | **cycle total** | 237.5 | **304.3** | 352.2 | 389.6 |

      **Attribution at the median: matching is 241.0 of 304.3 ms — 79% of the cycle.** Grab is 18%,
      the Modbus send 1.4%, publishing the handshake outputs 0.03%.

      **The threading-revisit criterion can now be evaluated, and the answer is "no".** Matching
      dominates, and matching already runs on its own worker thread; the grab/send/publish path
      together accounts for under a fifth of the cycle, so moving it between threads cannot pay for
      itself. That closes the standing criterion from `phase2_phase3_runtime_hardening.md`, which had
      been unevaluable since it was written.

      Three things this table is not, stated so nobody over-reads it:
      1. **The camera was `Virtual_cam_1`** (device 06 — the log says `virtual grab ok`). The grab
         column is a virtual grab. The Basler measured ~200 ms on the 09-11 run, so a real-camera
         cycle is roughly 150 ms longer and matching's share falls to about 54%. Still the largest
         stage, so the conclusion above holds, but the total is optimistic.
      2. **Session 1 was a different configuration** (n = 5, 08:45:31–08:45:50): match 2226–2655 ms,
         cycle 2250–2685 ms — roughly ten times session 2. Something changed across the restart at
         08:46:13; the log does not record what. Not averaged in.
      3. Two of the 85 cycles aborted mid-run on a Modbus collision and are not in the table — see
         the bullet below.

- [ ] ⚠️ **TWO DEFECTS FOUND IN THE 09-14 RUN, both filed, neither caused by Phase F:**
      - **RESOLVED 2026-09-14 — back to 159 / 0, see backlog 65.** As first recorded: *the contract
        suite read 152 passed / 7 failed*, not the 159 / 0 above.
        All seven failures are the repo's `app/` directory no longer resolving; a fresh `qmake`
        prints `Cannot read .../app/app.pri: No such file or directory` and **still exits 0**,
        producing a shell Makefile with no `main.cpp` at all. The 09-14 binaries were built from
        Makefiles dated 09-10 10:22, i.e. from before the change. **The seven tests are not stale —
        they are the guard that caught this.** Filed as backlog 65.
      - **A result publish can collide with the poll on a dual-role Modbus binding and abort the
        cycle** — observed twice in the 09-14 run, each needing a `bErrorReset` to clear. Filed as
        backlog 64. It is on the exact binding F1 was just commissioned on.

---

## Phase G — a held `bExecuteTrigger` at runtime start (item 54) — **OWNER-GATED**

### Task G1: Record what actually happens today

**Description.** Before any behaviour change, establish the reproduction the backlog entry never had.
Item 54 says a held-high trigger *"fires one cycle when the runtime starts"*. On MC that appears to
be unreachable: the device suppresses its whole first polling round and adopts the polled values into
its shadows (`:424`, `:476`, gates at `:710`/`:733`), and `ModbusRegisterMap` drops its baseline the
same way (`modbus_register_map.h:161`, `:215-224`). **Prove which it is** — with a virtual PLC driving
the controller directly, and by inspection for the two real families — and record the finding either
way. If the cycle does not fire, the defect is that the runtime goes **Ready with nothing said**, and
G2's acceptance changes accordingly.

**Acceptance criteria**
- [ ] A contract case documents the current behaviour for a trigger that is high before `setup()`
      completes: whether a cycle starts, and what `bTaskReady` reports.
- [ ] The finding is written into the plan and into item 54 — including, if confirmed, that the
      originally reported hazard is not reachable on MC or Modbus because the first poll is suppressed.
- [ ] No behaviour changes in this task.

**Verification**
- [ ] Contract test **148 → 149**; umbrella build clean.
- [ ] No negative check applies — the case asserts current behaviour. It becomes G2's baseline, and
      G2's negative check is that it goes red.

**Dependencies:** Checkpoint F
**Files:** `tests/architecture_contract_test/main.cpp`, `docs/backlog/later_todo_list.md`
**Size:** XS

---

### Task G2: A held trigger at startup is not-ready, and the runtime can see it

**Description.** Per Q2. Two parts, and the first is the real work.

*The snapshot read.* The runtime cannot learn a power-up level today: `PlcValueMap`
(`plc_device.h:114-119`) is an empty polymorphic base with only `clone()`, and the device widgets
downcast to concrete map types. Add a family-independent accessor — `virtual QVariant
valueForTag(const QString &) const` (or a `snapshot()` returning the name→value map) — implemented by
the MC, both Modbus and virtual maps, and have the controller subscribe to
`PlcRunner::pollingUpdate` (`plc_runner.h:155`, already connected at `:224-225`) to read
`bExecuteTrigger`'s actual level once, on the ready path.

*The behaviour.* Track "the trigger has been observed low since `setup()`". Until it is set, a high
trigger holds `bTaskReady` false with a task-log line and a dashboard state, and **no rising edge is
treated as a trigger**. The first observed low arms the runtime; the next rising edge runs a cycle
normally. This is **not** the seeding fix item 54 rejects: the PLC handshake requires the trigger to
drop after every cycle (`:550-573` is the falling-edge path), so a genuine trigger is always preceded
by a low, and the only edge refused is one that predates the runtime.

**Split point — and it shrank on 2026-09-08.** *(a)* the `PlcValueMap` accessor across the four maps
+ the controller's `pollingUpdate` consumer + a test that the power-up level is read; *(b)* the
arm-state behaviour and its outputs. **Task C6 now builds (a)**, so if C6 has landed, G2 is only (b)
and drops from L to M. If Phase G runs without C6, G2 owns (a) as well and is L.

**Acceptance criteria**
- [ ] `PlcValueMap` answers by tag name for MC, Modbus client, Modbus server and virtual, and a
      contract case reads a known power-up level through `PlcRunner::pollingUpdate` with no cast to a
      concrete map type.
- [ ] A trigger high at the first snapshot after `setup()` starts **no** cycle, holds `bTaskReady`
      false, does **not** raise `bTaskFault` — a held trigger is a handshake state, not an invalid
      selection, so **D7 does not apply to it**; this is a deliberate difference, logs the reason, and
      shows it on the dashboard.
- [ ] The first falling edge arms the runtime; the **next** rising edge runs a cycle normally.
- [ ] A runtime that starts with the trigger low is armed immediately and behaves exactly as today —
      **every existing cycle test passes unchanged**. If any breaks, the design is wrong and is
      reconsidered, not patched.
- [ ] The arm flag is reset by `setup()` and by nothing else, so a recovery or a re-arm cannot clear it.
- [ ] `plc_signal_contract.md`'s Trigger section is rewritten in this commit.

**Verification**
- [ ] Umbrella build clean; both shells relink; contract test **149 → 153**;
      `modbus_device_test` and `mc_frame_test` green.
- [ ] **Negative check.** Remove the arm flag → the held-trigger case goes red and the normal-cycle
      cases stay green. Then implement the **rejected** fix (seed `m_lastExecuteTrigger` from the
      first observed value) → the "first trigger after a falling edge still runs" case goes red,
      demonstrating on the bench that item 54's objection is real and that this design avoids it.
      Restore.
- [ ] **OWNER-RUN, Checkpoint G:** hold the trigger high, start the runtime, confirm no cycle and a
      clear reason on the dashboard and on the output signals; drop the trigger, raise it, confirm a
      normal cycle.

**If Q2 is refused:** drop this task. Its documentation half — stating in
`plc_signal_contract.md` which reading the cell wants and why, per item 54's actual instruction —
moves into **Z2**, and G1's recorded finding stands either way.

**Dependencies:** G1; **Q2 answered**
**Files:** `src/device/plc/plc_device.h`, the four `PlcValueMap` implementations,
`src/model/localization_runtime_controller.{h,cpp}`,
`docs/domains/task_localization/plc_signal_contract.md`, `tests/architecture_contract_test/main.cpp`
**Size:** M *(split as stated; over the file guideline — (a) and (b) must land separately)*

---

### Checkpoint G

- [ ] Umbrella build clean; contract test **153 / 0** (or 149 if Q2 was refused); all device suites green.
- [ ] **OWNER-RUN:** held-trigger behaviour confirmed on the cell.

---

## Phase Z — cleanups, documentation, close-out

> **Precondition cleared 2026-09-14, before Z1 — backlog 65.** Checkpoint Z's first line asks for an
> umbrella build *"clean from a fresh `qmake`"*, and on 2026-09-14 a fresh `qmake` could not read
> `app/app.pri`: the commissioning shell had moved to `components/app/` with nothing repointed. The
> owner chose to keep `components/app/` and repoint the wiring. Done and verified the same day — the
> full record, including the include-layering hole a plain rename would have opened, is in backlog 65.
>
> **A dependency deviation, stated rather than made quietly.** Z1 lists *Checkpoint G* as its
> dependency, and Phase G is owner-gated and not started. Z1 touches none of G's files except the
> contract test, so it proceeds. The parts of **Z2** and **Z3** that genuinely need G's outcome — the
> Trigger section of `plc_signal_contract.md`, and closing item 54 — are held back and marked as such
> where they occur.

### Task Z1: Delete `connectTimeoutMs` (D5)

**Description.** Remove `LocalizationRecoveryPolicy::connectTimeoutMs`
(`localization_recovery_policy.h:52`); nothing reads it but the assertion at `main.cpp:2195`. Record,
without acting on it, that `setRecoveryPolicies()`'s only caller in the repository is `main.cpp:2257`
— **the product never sets recovery policies at all**, so every role runs on
`defaultCameraRecoveryPolicy()` / `defaultPlcRecoveryPolicy()` / `defaultVisionOutputRecoveryPolicy()`.
Persistence stays deferred per D5.

**Acceptance criteria**
- [ ] The field and its assertion are gone; `test_localization_recovery_policy_defaults_match_runtime_spec`
      still asserts the surviving defaults and the **case count does not drop**.
- [ ] A comment on `setRecoveryPolicies()` and a line in `runtime_controller_api.md` state that no
      production caller exists, so the defaults are what ships, and name the injection point for when
      that changes.
- [ ] No reference remains anywhere, **including `uml\04_localization_task.puml` and
      `uml\03_runtime_threading.puml`**, which a `src\`-only grep would miss.

**Verification**
- [ ] Umbrella build clean; contract test count unchanged, 0 failed.
- [ ] Search recorded:
      ```powershell
      cd C:\DGB\Project\ncr_picking
      Select-String -Path src\*,app\*,runtime_app\*,tests\*,uml\*,docs\* -Include *.cpp,*.h,*.puml,*.md -Recurse -Pattern 'connectTimeoutMs'
      ```
      Expect no hit outside `build\`.
- [ ] No negative check applies — nothing is added. The build and the search are the evidence.

**Dependencies:** Checkpoint G
**Files:** `src/model/localization_recovery_policy.h`, `tests/architecture_contract_test/main.cpp`,
`docs/domains/task_localization/runtime_controller_api.md`, `uml/04_localization_task.puml`,
`uml/03_runtime_threading.puml`
**Size:** XS

> ### ✅ Z1 LANDED 2026-09-14
>
> **Removed.** `LocalizationRecoveryPolicy::connectTimeoutMs` (`localization_recovery_policy.h`),
> its single assertion in `test_localization_recovery_policy_defaults_match_runtime_spec` — at
> `main.cpp:2922`, not the `:2195` this task quotes — and its line in both UML diagrams.
>
> **Recorded, not acted on.** `setRecoveryPolicies()`'s only caller is
> `tests/architecture_contract_test/main.cpp:2983` (the task's `:2257` is stale), which shortens the
> retry interval to cover many attempts. The product never sets policies; the defaults are what
> ships. Stated in a doc comment on the header declaration and in `runtime_controller_api.md` →
> "Fault And Recovery", both naming the injection point for when that changes:
> `TaskLocalization::setupRuntimeController()`, immediately before `controller->setup(context)` and
> inside the same thread hop. Persistence stays deferred per D5.
>
> **A wrong comment found while writing that one, and corrected.** The `.cpp` doc on
> `setRecoveryPolicies()` said a new policy *"takes effect on the next status change"*. It does not:
> `bindRoleContext()` copies the policy into the role's context (`localization_runtime_controller.cpp:1090`)
> and every recovery path reads that copy (`:2042`–`:2226`), so a call after binding changes nothing
> until the role is bound again — by `setup()`, or an active-camera change for the camera role. The
> same comment still spoke of *"retry limits"*, which have not existed since reconnect became
> unbounded. Both comments now say what the code does.
>
> **Deviation 1 — the UML blocks were wrong about more than this field, and were fixed in the same
> edit.** Both `LocalizationRecoveryPolicy` blocks also carried `maxRetries`, and
> `03_runtime_threading.puml` carried `canRetry(currentRetryCount)`; both diagrams listed an
> `EscalateFault` value in `LocalizationRecoveryAction`. All three were removed when reconnect became
> unbounded (Phase 6 / B1) — verified against `localization_recovery_policy.h`, whose enum is
> `Ignore` / `RetryScheduled` and whose struct has no retry count. The task's own rule is that no
> stale reference survives in these exact blocks; leaving lines known to be false beside the one
> removed would have obeyed the letter and missed the point.
>
> **Deviation 2 — the recorded search is not literally empty.** Over `src\`, `components\app\`,
> `runtime_app\`, `tests\`, `uml\` and `docs\` (excluding `docs\generated\` — a build artifact — and
> `docs\history\`, which keeps 4 mentions as records), the search finds **two hits, both deliberate**:
> the removal notes in `localization_recovery_policy.h` and `runtime_controller_api.md`, which name
> the field so that someone grepping for it from an old project file or plan finds why it is gone.
> Neither reads, sets or documents it as existing.
>
> **Verification, observed 2026-09-14.** Umbrella build clean (`localization_runtime_controller.cpp`
> and `task_localization.cpp` recompiled through the header), no zero-byte objects, **both shells
> relinked 12:07 / 12:08**. Contract test **159 / 0**, shape `-functions` **157 == 157** — the case
> count did not drop, and the defaults case still asserts every surviving default. No negative check
> applies: nothing was added.
>
> **Noticed for Z2, not touched here:** `04_localization_task.puml`'s `LocalizationFaultCode` block
> still lists `PatternInvalid = 400` and lacks 103 and 301; and the doc comment on
> `RuntimeContext::robotCheckConfig` still says the settings are *"snapshotted from the assigned
> vision output device"*, which F1 made false.

---

### Task Z2: Documentation pass

**Description.**
- **`plc_signal_contract.md`** — the two new status signals, documented as optional outputs that are
  **never** the command registers, with the migration order from C3; D4's setup gate; D3's
  write-failure policy and fault code 301; the Trigger section per G2 (or per item 54's actual
  instruction if Q2 was refused); the item-58 warning box rewritten to describe what the code now
  does, including the honest statement that the runtime still adopts `firstKey()` and now announces it.
- **`maintenance_and_extension.md`** — the missing Phase F invariants (the `markRuntimeReady()`
  `WaitingTriggerReset` guard, the per-signal rejection latches, range-before-registration), the
  capability step for adding a device to a role, and F1's pick-check migration note.
- **`architecture_improvement_todo.md`** — A32's status still claims *"fault escalation"*, removed
  when reconnect became unbounded. Correct it.
- **`runtime_controller_api.md`** / **`task_localization_api.md`** — the `setup()` contract as it now
  stands: which failures are hard errors, which are not-ready, which are warnings.
- **`docs/generated/architecture_docs/LocalizationRuntimeController.md`** and
  `docs/domains/task_localization/README.md` — the new signals and validation.

**Acceptance criteria**
- [ ] No document still claims a power-up 0 is detected, or that A32 escalates, or that the pick
      check is read from the bound device.
- [ ] Each rewritten paragraph names the code that makes it true; a paragraph that cannot cite one is
      describing an intention.
- [ ] Where a decision was deferred (item 58 part B, recovery-policy persistence, the duplicate
      pick-check editors), the doc says it is deferred and links the backlog item.

**Verification**
- [ ] Documentation build clean per `docs/rules/documentation_build.md`; every cited line number
      opened and confirmed — stale line numbers are the failure mode these docs already have.
- [ ] `test_translations_are_updated_from_one_project_that_sees_every_source` green.
- [ ] **OWNER REVIEW of `plc_signal_contract.md` before Phase 9 closes.** It is a wire contract and a
      commissioned PLC program depends on it; C3 and E4 both change what it promises.

**Dependencies:** Z1
**Files:** the six documents above
**Size:** S

> ### ✅ Z2 LANDED 2026-09-14 — owner review of `plc_signal_contract.md` still outstanding
>
> **Two of this task's own instructions were stale, and were followed in intent rather than to the
> letter.** (1) It asks for *"the honest statement that the runtime still adopts `firstKey()`"*, with
> an acceptance line that no document may claim a power-up 0 is detected. Both predate C6. C6 made the
> runtime read its starting selection from the PLC, and the owner confirmed on 2026-09-09 that a master
> holding 0 now faults. The old warning box in `plc_signal_contract.md` said the opposite, so it was
> rewritten as **"At Startup"**: the four outcomes, citing `awaitPrimaryPlcSnapshot()`,
> `commandedIndexFromPlc()` and the two validators. (2) The Trigger section *"per G2"* — Phase G has
> not started, so that section is **held back**; it carries a note pointing at backlog item 54 and
> asserts neither behaviour.
>
> **What was rewritten.** Every paragraph names the code that makes it true.
>
> | Document | Change |
> |---|---|
> | `plc_signal_contract.md` | signal table (command registers are PLC → task only; two status rows); new **"Setup Validation"** (C4); **"At Startup"** replaces the item-58 box (C6); new **"Active Selection Status"** with C3's migration order; **"Handshake Writes Are Acknowledged"** (E4); all four `IPlcIoWriter` implementors; startup notes on 103 / 400; held-trigger note |
> | `runtime_controller_api.md` | `RuntimeContext` as it stands; `setup()` order and the **three-class contract** — hard error / valid-not-Ready / warning; status outputs; matching signature; the output filter and the two-position cap; the readiness gate; handshake tracking; `runtimeFault` causes; `CycleTimings` |
> | `task_localization_api.md` | startup order incl. `awaitPrimaryPlcSnapshot()` and `runtimeStarted()`; C5's deleted setters and the two live paths; context construction |
> | `maintenance_and_extension.md` | adding a signal (display-name marker, schema bump, `kSignalFields`, required-vs-optional, status-vs-command, handshake); the capability step for a role; camera switching after C5; **"Runtime Readiness Invariants"**; **"Robot Pick Check Ownership"**; required test coverage |
> | `task_localization/README.md` | contracts from C3 / C4 / C6 / E4 / F1 / F3; pick-check source row; matching signature |
> | `architecture_improvement_todo.md` A32 | escalation removed; what stands now |
> | `generated/architecture_docs/LocalizationRuntimeController.md` | **rewritten** — the old page listed signals (`cycleFinished`, `faultOccurred`), fault codes, config fields (`matchThreshold`) and policy fields (`retryCount`) that do not exist, a three-level recovery model the class never had, and `nFaultCode=100` for a refused camera |
> | beyond the task's list | `robot_kinematics_module.md` and `virtual_devices.md`, both still saying the runtime reads the pick check off the device; the curated `TaskLocalization.md` (C5's deleted methods); UML `04` (status tags, `robotCheckConfig`, deleted setters, fault codes 103 / 301 and the 400 rename) and `09` (the task-owned copy); source doc comments on `RuntimeContext::robotCheckConfig`, both `buildRuntimeContext()` comments, and `runtimeFault` |
>
> **A source comment was wrong about the retry budget; the plan line it contradicted was right.** The
> header called `kPlcWriteRetryBudget` (3) the number of times a write is *re-issued*. The code counts
> **attempts**: the first write is tracked as attempt 1, and `onPlcWriteFinished()` re-issues only
> while `attempts < 3` — one write plus two re-issues, matching Checkpoint E's *"re-issued up to
> twice"*. Comment corrected; the contract says *"3 attempts in total"*.
>
> **Found while describing the code, filed, not fixed.** `buildVisionOutputPositions()` sends **at most
> two positions per cycle** (`:1726`) and marks further pickable objects `Skipped` with no reason; and
> `handlePlcValues()` carries a `/// temp debug` `qDebug()` (`:938`). Both may be deliberate owner
> code — **backlog 68, owner to confirm.** The deferred decisions this task must link now have entries:
> **66** (two pick-check editors) and **67** (recovery policies not settable outside a test).
>
> **Verification, observed 2026-09-14.**
> - [x] Every line number cited in the new text re-opened after the last edit. **Three in the Z1 record
>       above were stale and are corrected there** — `:1088` → `:1090`, `:2040`–`:2224` →
>       `:2042`–`:2226`, `main.cpp:2984` → `:2983` — measured before a comment grew by two lines and an
>       assertion line was removed. It is the failure mode this task names, caught in this task's own
>       output.
> - [x] Forbidden-claim search over `docs\` (excluding `history\` and the generated Doxygen HTML) and
>       `src\`: no document or comment still says a power-up 0 goes undetected, that A32 escalates, or
>       that the pick check is read from the bound device.
> - [x] Umbrella build clean, **both shells relinked 12:28**; contract test **159 / 0**, shape 157 == 157;
>       `test_translations_are_updated_from_one_project_that_sees_every_source` **PASS**.
> - [ ] **Documentation build — BLOCKED on this machine, not run.** `documentation_build.md` expects
>       Doxygen, Graphviz and PlantUML under `C:\build_packages\…`; none of the three is present (Java
>       is). The generated HTML under `docs\generated\doxygen\` therefore still shows the old comments
>       and diagrams until the next docs build. It is a build artifact and was not hand-edited.
> - [ ] **OWNER REVIEW of `plc_signal_contract.md`** — a wire contract a commissioned PLC program
>       depends on, and C3, C4, C6 and E4 each change what it promises.

---

### Task Z3: Backlog hygiene

**Description.** Close umbrella item **26** per D6, folding its three remaining bullets into their
real homes: the operator-UI pass → **F2**, latency measurement → **F3**, remaining runtime edge-case
tests → **B1/B2**. (D6 names "item 8" as the third target; backlog item 8 is `RobotRunner` and
unrelated — see the numbering correction above.) **File the PLC write-acknowledgement work as a new
backlog entry**, because none exists. Rewrite item **57** as a missing-feature item before closing
it — it is filed as verification and can never pass as filed. Add **59.3b** to item 59; it was found
for this plan and is not in the backlog. Mark 1, 25, 32, 51 (half), 53, 54, 55, 57, 58 (part A),
59.1, 59.2, 59.3, 59.3b resolved with the task **and the test** that closed each.

**Acceptance criteria**
- [ ] Item 26 closed with each bullet's new home named; no bullet dropped.
- [ ] Every closed item cites its evidence — commit, test case, and the owner-run confirmation where
      one was required. Nothing is marked verified without it.
- [ ] Items that are **not** closed stay open with their remaining scope stated plainly: item 58
      **part B** (reading the true power-up selection), item 51 (no device emits `errorOccurred`),
      item 56's remaining command coverage, recovery-policy persistence, the duplicate pick-check
      editors, the MC device-level transport harness, and anything the Phase E stop rule carried.
- [ ] Any defect found while executing this phase and deliberately not fixed is filed, with its
      mechanism, before the phase closes.

**Dependencies:** Z2
**Files:** `docs/backlog/later_todo_list.md`, `docs/backlog/technical_debt_and_next_steps.md`,
`docs/backlog/architecture_improvement_todo.md`
**Size:** XS

> ### ✅ Z3 LANDED 2026-09-14
>
> **Closed, each with its task and its evidence** — the evidence lives on the item itself:
>
> | Item | Closed by |
> |---|---|
> | 1 | C4 — `checkEmpty()` deleted, not wired (recorded 2026-09-09) |
> | 25 | F2 — both halves owner-confirmed: binding change 2026-09-11, cable pull 2026-09-14 |
> | **26** | **closed now, per D6** — latency → F3; edge-case tests → B1 / B2; operator UI → F2 and F3(b). Its conditional PLC-write bullet was met by Phase 8 plus E1–E4 |
> | 32 | D2 — `drainBeforeClose()`, 0 / 20 on both vision suites |
> | 53 | A3 — deliberately with no test; the reason is on the item |
> | 55 | C2 + C7 |
> | **57** | F1 — **rewritten at the top as the missing feature it was**, then closed on the owner's dual-role Modbus observation |
> | 58 part A | C2 + C3 + C6 |
> | 59.1–59.4 | C5, A2, A1. **59.3b needed no adding: it was already filed, as 59.4** |
> | 61 | F2 |
> | 51, runner half | B1 — `test_every_runner_family_forwards_device_errors_to_the_controller` |
>
> **Not closed, and why** — stated on each item:
>
> | Item | Remaining scope |
> |---|---|
> | **54** | Phase G not started (owner-gated). **This task's instruction to mark it resolved could not be followed**: it assumed Phase G had run |
> | 51, device half | the emit-or-delete decision |
> | 56 | the 1C/3C command set on a real module, and bench numbers. E1 closed the write-completion half and delivered the device-level MC harness this plan had listed as out of scope |
> | 58 part B | a mapped-but-never-written register as its own fault; MC / Modbus-client first-poll adoption for every non-index signal |
> | 63 | awaits the owner allocating `PatternInvalid = 402` |
>
> **Filed during Phase Z** — the "every defect found and deliberately not fixed" criterion: **64**
> dual-role Modbus publish/poll collision; **65** `app/` path, resolved the same day; **66** two
> pick-check editors; **67** recovery policies not settable; **68** the two-position cap and a temp-debug
> print, owner to confirm; **69** PLC write acknowledgement (D3) — the entry this task said to file.
> The work had shipped by then, so it records what shipped and the one thing unverified: E4 on hardware,
> with the two untried recipes. **70** the vision-output client stuck in Recovering (field 2026-09-09,
> investigation paused).
>
> **Item 70 corrects the first round of that investigation.** Filing it meant re-reading the log for
> the device type, and the failing session's `VisionOut_01` was a **`VisionTcpipClientDevice`**
> (`app_log_2026-09-09.txt:6136`) — not the server the earlier analysis reasoned from. For a client, a
> heartbeat-only reconnect cannot report healthy, so the task staying Recovering is the controller
> behaving correctly. The likelier defect is that recovery's reconnect is a no-op ("already active",
> `vision_tcpip_device_base.cpp:52`) and the main socket is never re-dialled. It is recorded as a
> hypothesis on the item and was not investigated further, because the owner paused it.
>
> **`technical_debt_and_next_steps.md`**: the Phase 8 carry table (57 closed, 56 partly closed, 54
> still open); the "55 and 32 are open" line (both closed); the item-58 paragraph; the untranslated
> string count (436 → 465); the latency line — **done**, with F3's numbers and the threading verdict;
> and the generated-docs decision, noting Z2's rewrite. **`architecture_improvement_todo.md`**: A32,
> done in Z2.
>
> **Verification.** Documentation only; nothing to build. Every item number cited above was opened.

---

### Checkpoint Z — Phase 9 complete

**Status 2026-09-14: NOT CLOSED.** Z1, Z2 and Z3 have landed. The checkpoint is held open by owner-run
items, one blocked tool and one field defect, each named below.

**Status 2026-09-16: CLOSED WITH CARRIED ITEMS — see the carry table below and Phase 10 charter.**

**Carried out of Phase 9.** One row per item in the "Everything carried" bullet below. Every
destination is a Phase 10 work package (Phase 10 charter §3; the charter moves to `docs/plan/phase_10/`
at Checkpoint 0) or a backlog item in `docs/backlog/later_todo_list.md`. The owner approved the carry
on 2026-09-16 (charter §6). The same table is recorded in
`docs/backlog/technical_debt_and_next_steps.md` → "Carried Out Of Phase 9".

| Item | What is unfinished | Destination |
|---|---|---|
| **Phase G / item 54** — a held `bExecuteTrigger` at runtime start | Phase G not started (owner-gated), so G2's held-trigger behaviour was never run and item 54's decision is still open | **WP-50** — Stage 5 pilot on the new runtime core, reusing the G2 design under the DR that answers PQ-1. Item 54 stays open until WP-50 lands |
| **E4 on hardware** — a real PLC's refusal aborting the cycle with `301 PlcWriteFailed` | Blocked: the owner has no way to make a healthy PLC refuse a write; two untried recipes are recorded on the item. Bench-proven only | **Backlog 69** — non-blocking; not a Phase 10 gate |
| **D1 field check** — vision-output reconnect without a restart | Observed failing on the cell 2026-09-09: a `VisionTcpipClientDevice` stays Recovering when only its heartbeat link comes back; the reconnect-is-a-no-op hypothesis is on the item, investigation paused by the owner | **Backlog 70** for the device-side defect, fixed on its own; **WP-32** (role-health rows of the new core) must carry a row for scenario S-06 |
| **Owner review of `plc_signal_contract.md`** (the Z2 rewrite) | Not done | Replaced, not rescheduled: the owner reviews the contract **table** at Checkpoint 2 — **WP-21** (to-be table) — instead of the prose |
| **Documentation build** | Doxygen / Graphviz / PlantUML are absent on this machine, so the generated reference was not rebuilt after Z2 | **Backlog 44** — non-blocking |
| **51, device half** — `IDevice::errorOccurred` | No device emits it; the emit-or-delete decision is open (the runner half closed with B1) | **WP-03** triage — close, merge, keep, or absorb into the Phase 10 redesign; stays item 51 |
| **56** — MC 1C/3C on a real C24 | The 1C/3C command set on a real module, and the `tools/mc_protocol_bench` numbers (write completion closed by E1) | **WP-03** triage; stays item 56 |
| **58, part B** — the true power-up selection | A mapped-but-never-written register as its own fault; MC / Modbus-client first-poll adoption for every non-index signal (part A closed by C2 + C3 + C6) | **WP-03** triage; stays item 58 |
| **63** — fault code 400 for content-invalid pattern faults | Awaits the owner allocating `PatternInvalid = 402` | **WP-03** triage; stays item 63 |
| **64** — dual-role Modbus publish/poll collision | The result publish dispatched during an in-flight poll aborts the cycle (2 of 85 cycles on 2026-09-14); filed by Z3, not fixed | **WP-03** triage; stays item 64 |
| **66** — two editors for the robot pick check | The task's editor and each vision-output device's can disagree silently (open question O-2); removing one is a deprecation with a migration question | **WP-03** triage; stays item 66 |
| **67** — recovery policies not settable | `setRecoveryPolicies()` has no production caller and the values are not persisted (deferred by D5) | **WP-03** triage; stays item 67 |
| **68** — two-position cap and a debug print | `buildVisionOutputPositions()` caps at two positions with no reason in the row, and `handlePlcValues()` keeps a temp-debug print; owner to confirm whether the cap is deliberate | **WP-03** triage; stays item 68 |
| **A1 on hardware** — `bExecuteTrigger` delivery on an M-only MC PLC | Ships on unit evidence only (open question O-3): no M-only station was identified, so the field half of A1 was never run. Not a defect, an unverified claim | **Backlog 71** (filed 2026-09-16); non-blocking, re-opened when an M-only station exists |

- [x] **Umbrella build clean from a fresh `qmake`; both shells relink.** `qmake ncr_picking_all.pro`
      re-run in `build\all\Release` on 2026-09-14 at 11:54, after backlog 65's repoint: no
      `Cannot read`, and the editor's Makefile lists `components\app\main.cpp`. Both shells relinked,
      most recently at 12:28 after Z2.
- [x] **Final counts, shape verified before every total, 2026-09-14:**

      | Suite | `-functions` vs source | Observed |
      |---|---|---|
      | `architecture_contract_test` | 157 == 157 | **159 / 0** — the written ~153 was an estimate |
      | `mc_frame_test` | 52 == 52 | **54 / 0** |
      | `modbus_device_test` | 23 == 23 | **25 / 0** |
      | `vision_output_device_test` | 9 == 9 | **11 / 0** |
      | `vision_tcpip_client_device_test` | 8 == 8 | **10 / 0** |

      **No known flake:** item 32 is closed (0 / 20 on both vision suites, re-confirmed 0 / 6 at
      Checkpoint F). **`jai_camera_hardware_test` was not run.** It needs the bench camera, nothing in
      Phase 9 touched the JAI device, and backlog 62's guard says to rebuild before quoting any number
      from it — so it is recorded as unchanged by construction, not as re-measured.
- [x] **Display-name total 124**, asserted by the green contract suite. **Translation sweep with 0 newly
      vanished**, last run 2026-09-14 through the repointed pipeline: 1324 texts, vanished 33 → 33.
      Z1–Z3 added no translatable strings.
- [x] **Every new test negative-checked, both halves recorded** — at each task. Phase Z added no test; it
      changed the include-layering contract, and that change was negative-checked both ways (backlog 65).
- [ ] **OWNER-RUN list** — confirmed except where stated:

      | Check | Task | Status |
      |---|---|---|
      | condition-ROI filtering from cycle 1 | A2 | ✅ 2026-09-08 |
      | PLC inputs survive a camera change on the dual-role binding | A3 | ✅ 2026-09-08, from the field log |
      | startup announcement, command registers untouched | C3 | ✅ 2026-09-09 (Checkpoint C-1) |
      | refusal messages for a broken map | C4 | ✅ 2026-09-09 (Checkpoint C) |
      | vision-output reconnect without a restart | D1 | ❌ **observed failing 2026-09-09** — see Checkpoint D and backlog 70 |
      | MC write completion on the C24 | E1 | ✅ 2026-09-09 |
      | a real PLC's refusal aborting with 301 | E4 | ⛔ **blocked** — no way to make a healthy PLC refuse; two untried recipes on backlog 69 |
      | pick check commissioned from the task on Modbus | F1 | ✅ 2026-09-14 |
      | connection lamps | F2 | ✅ 2026-09-11 and 2026-09-14 |
      | 20 cycles of latency captured | F3 | ✅ 2026-09-14 — 85 cycles |
      | held-trigger behaviour | G2 | ⛔ Phase G not started — owner-gated |
      | M-only PLC trigger delivery | A1 | **untested on hardware** — no M-only station identified (open question O-3); A1 ships on unit evidence |
      | review of `plc_signal_contract.md` | Z2 | ⏳ not done |

- [ ] **Everything carried, with its reason:** Phase G and item 54 (owner-gated, not started); E4 on
      hardware (test access — backlog 69); D1's field check (a real defect — backlog 70); the owner's
      review of the PLC signal contract; the documentation build (tools absent on this machine —
      backlog 44); and the open items listed at Z3 — 51 (device half), 56, 58 part B, 63, 64, 66, 67, 68.

---

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| **A stale baseline hides a regression.** Phase 8 closed the contract suite at 95; it is 101. | Baselines re-measured 2026-09-08 and evidenced above. Any task that reports a count off by six has run the wrong binary — the live directories are `build\architecture_contract_test` and `build\tests\<name>`, **not** the `msvc_release` trees, whose Makefiles are from August. Counts in task bodies are **additive targets, not a chain** — see the note under Baselines. |
| **C3 and F1 each bump `kSchemaVersion` (2→3→4), and intermediate builds write project files on a commissioned station.** `fromJson()` refuses a document newer than the running build (`task_localization_config.h:213`), so once the cell saves under Phase 9 no earlier binary opens that project — and `AGENT.md` forbids a compatibility shim. | **Copy the cell's `.vproj` at every checkpoint before the owner runs anything**, and keep the pre-Phase-9 copy until Checkpoint Z. Stated as a checkpoint precondition, not as advice. |
| **C3 removes a PLC-visible echo.** A master reading its own command register back loses that readback. | The status tags carry it. Migration order is in C3, documented in Z2, and confirmed by the C3 owner-run item before the phase closes. `build\Huayan.vproj` maps the command registers to master-owned holding registers, which is why the echo was wrong. |
| **C6 makes a previously-silent startup fault real.** Today a master holding 0 in its index register starts the task on the project default. After C6 it **faults**. A cell that has been running on that accident stops running. | This is the point of the phase, not a side effect — but it is a behaviour change the owner meets on the first restart, so it is called out at C6's OWNER-RUN item and again at Checkpoint C. An unmapped signal is unaffected: a cell that does not switch cameras keeps working exactly as before, and C6 has an acceptance criterion saying so. |
| **C6 changes `beginRuntime()`'s ordering, which every task start goes through.** | Split point lands the accessor first, alone and independently testable. The bounded wait for the PLC role has its own criterion — a dead PLC must fault, never hang. |
| **E1 is the hardest code in the phase** — correlating writes across a round-robin state machine with retries, a partial-frame buffer and a documented mutex exception at `:320-321`. | An explicit stop rule at Checkpoint E-1, a real split point, and one acceptance property worth having: *resolves exactly once, always*. Phase F does not depend on E and starts regardless. |
| **A fixed test proves the wrong thing.** Phase 8/F4's first negative check passed for the wrong reason. | Every negative check names which cases go **red** and which stay **green**. Three of them (A3(b), C2, C3) exist specifically to observe a stay-green outcome. |
| **B2 is the phase's only M-sized harness and it gates F2's one automatable assertion.** | Written split point: (a) construction + `runtimeStarted` ordering carries F2; (b) the full hop. If (b) carries, F2 becomes fully owner-run and the plan says so at Checkpoint B. |
| **Owner-run items pile up.** There are ~11 of them. | Only two gate later work (A1's M-only reproduction, E1's C24 completion). The rest are batched at four checkpoints — A, C-1, F, Z — rather than being eleven separate interruptions, and each is named at the task that produces it. |
| **C4 could refuse a project on a commissioned station.** | Audited before the gate ships: `Huayan.vproj` and `virtual.vproj` map all 14 signals; `4E_Line.vproj` maps none but has no device bindings and already fails setup. Re-run as a C4 acceptance criterion. |

---

## Open questions

Only the genuinely open ones. The active-index fault question is **settled by D7** (it stays a
fault) and the startup-selection question by **D8** (Task C6). The held-trigger item (54) remains
owner-gated as Phase G.

| # | Question | Blocks | Recommendation |
|---|---|---|---|
| **O-1** | The dual-role Modbus binding (one device on `primary_plc` **and** `vision_output`) has **no hardware-free test path**. Giving `VirtualPlcDevice` `IResultOutputDevice` would create one — and would deliberately break `test_result_output_capability_carries_the_robot_pick_check_settings` (`main.cpp:1174-1184`) and `test_plc_runner_reports_result_output_per_device_not_per_family`, both of which assert the opposite on purpose. | A3 and F1 acceptance | **Yes**, as a separate harness task, not folded into either. The dual-role binding is a shipped configuration only the owner's cell can exercise, and "only the owner can test it" is how Phase F's three defects reached the cell. The two tests would be rewritten to use `McProtocolDevice` as the negative, which they already do in spirit. |
| **O-2** | Do the two vision-output device widgets keep their own robot-check editors after F1(b)? Two editors for one setting is a commissioning trap. | Nothing in this plan; F1 deliberately leaves them | Decide in Phase 9 or file it in Z3. Removing them is a deprecation with a migration question attached and does not belong inside F1. |
| **O-3** | Is an M-only MC station available for A1's hardware confirmation? | A1's owner-run half only | If not, A1 ships on unit evidence and **says so**. It must not be recorded as hardware-verified on the strength of a pure-function test. |

---

## Explicitly out of scope

- ~~**Item 58 part B** — making the runtime *read* its selection from the PLC instead of adopting
  `firstKey()`.~~ **Brought IN SCOPE on 2026-09-08 as Task C6 (D8).** The owner ruled it the root
  fix for the index-0 defect: *"Có — đây chính là gốc của bug index 0."* It is now the phase's main
  deliverable, and it builds the `PlcValueMap` accessor that G2 would otherwise have to pay for.
- Persisting recovery policy values, and giving `setRecoveryPolicies()` a production caller (D5 defers it).
- The MC device-level transport harness (a fake `McMsgInterface`) — recorded by A1 as a known
  coverage gap, not built. It is the only way to test `response_handle()` and it is an M-to-L task.
- Removing `IResultOutputDevice::robotKinematicCheckConfig()`, and the device-side advisory check in
  `VisionTcpipDeviceBase::runKinematicCheck()` — F1 fences both deliberately.
- Item 56's remaining MC 1C/3C command coverage (E1's owner-run half closes part of it), item 52
  (Modbus word order), item 51's device half (no device emits `errorOccurred`).
- Any widget-level test framework. Its absence is why so much of this plan is owner-run, and
  inventing one mid-phase would be a larger project than the phase.
