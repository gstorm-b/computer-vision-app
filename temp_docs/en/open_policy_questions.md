# Open policy questions — seed for step D0

**Date:** 2026-09-16. Extracted from the backlog
([`later_todo_list.md`](../../backlog/later_todo_list.md)), the Phase 9 plan,
[`phase_9_request.md`](../../history/request/phase_9_request.md),
[`phase_10_request.md`](../../history/request/phase_10_request.md),
[`cratch_notes.md`](../../history/request/cratch_notes.md), and a source survey. Each item becomes a
PQ card (template: the committed copy in [`docs/decisions/README.md`](../../decisions/README.md) § Template; working original in `temp_docs/templates/policy_decision_record_template.md`) when it is taken into the loop.

The **Today** column is behaviour verified in the code/docs, not conjecture. The **Why a ruling is
needed** column says who is affected if no ruling is made. The suggested order is at the end.

## Cluster A — Startup / arming (proposed as the pilot)

| PQ | Question | Source | Today | Why a ruling is needed |
|---|---|---|---|---|
| PQ-1 | The trigger is being held high when the runtime starts: run a cycle, ignore it silently, or hold not-ready until a low level has been seen? | item 54; Phase 9 G1/G2 (owner-gated); `phase_9_request.md` | MC/Modbus suppress the first poll → no edge arrives → the task is **Ready without saying anything**. The snapshot from C6 already allows the real level to be read but is not used for it. `m_lastErrorReset` is not reset in `setup()` (asymmetric with the trigger). | [`plc_signal_contract.md`](../../domains/task_localization/plc_signal_contract.md) currently says "Not yet specified"; the PLC program cannot be written with any certainty. Phase G already has the design (the "not-ready until seen low" option), it is only waiting for a ruling. |
| PQ-2 | The PLC connects **late** (setup has already fallen back to the project default because no snapshot arrived within 2 s); when the PLC comes up, is the selection read again? | `cratch_notes.md` "Bug found: … camera number và index number will not update" | `setup()` reads the snapshot once; after that only **changes** reach the setters. A register holding its value from before the connection is never delivered. | A cell that starts with the PLC coming up after the vision (the real boot order) will silently run the wrong camera/group — exactly the defect class of item 58. |
| PQ-3 | A register that is **mapped but has never been written** (0 since power-up): is that a commissioning fault of its own, or is it merged with "unmapped"? | item 58 part B | Unmapped and unreadable both fall back to the project default with a USER warning; neither is separated from "the master really did write 0" (that case already faults correctly). | Affects the message to whoever is commissioning; affects PQ-2. |
| PQ-4 | Connect order: block the camera **and the output** until the PLC is connected? | `cratch_notes.md` "Chặn luôn connect camera và output device trước khi connect thành công với plc" | C6: `beginRuntime()` connects the PLC + output first, the camera after. The output is still parallel with the PLC. | If the selection depends on the PLC (PQ-2) then the output may depend on it too (dual-role Modbus). The "required-before" graph between the roles needs to be stated explicitly. |

## Cluster B — Fault vs Recovering

| PQ | Question | Source | Today | Why a ruling is needed |
|---|---|---|---|---|
| PQ-5 | Does losing a role **outside a cycle** publish `bTaskFault` (a new code) before entering Recovering? | `cratch_notes.md` "Nên set fault trước khi về recovering" | Phase B decided this deliberately: a dead link **never** raises `bTaskFault`; the PLC must watch `bTaskReady`. The docs carry a clear ⚠️ warning. | The owner's note contradicts the existing contract → either the DR is reversed, or keeping it is confirmed. This is a point the PLC program depends on directly. |
| PQ-6 | `Recovering` currently carries **two meanings** (role outage; cycle fault waiting for auto-clear). Split the state or keep it? | source survey (`abortCycle()`, `handleRoleStatusChanged()`) | One enum value, two entry paths, two different output sets (`RECOVERING` vs `CYCLE_FAULT`). The dashboard shows the same lamp for both. | Affects the transition table (the number of rows), the dashboard, and the Phase 10 user manual. Ruling on it before the table is extracted at D0 is cheaper. |
| PQ-7 | When a role is `LostConnected`, is the transport torn down completely before redialling? | `cratch_notes.md` "Recovering không disconnect hoàn toàn output device"; item 70 | Reconnect is an idempotent `deviceConnect()` → for the vision-output client it returns "already active" → **no-op**; the task is stuck in Recovering. | Partly a device bug (item 70), partly policy: what "reconnect" means at core level. `RoleStatus(LostConnected)` → action needs an explicit row. |

## Cluster C — Cycle output

| PQ | Question | Source | Today | Why a ruling is needed |
|---|---|---|---|---|
| PQ-8 | Is the cap of **2 positions / cycle** deliberate? If so: a constant or a setting; and the reason in the "Skipped" row. | item 68 | A literal `2` in `buildVisionOutputPositions()`; surplus positions are `Skipped` **with no reason**; no doc mentions it. | The operator sees a third part dropped without knowing why; the robot program may be assuming 2. |
| PQ-9 | "No match" is a **successful cycle with 0** (not a fault) — confirm it and write the DR; and what `bMatchingLowArea` means to the PLC. | source survey | That is exactly how it is today; there is no fault code for no-match; there is no DR. | It has to be written down as a decision so that the user manual and the PLC program understand it the same way. Cheap. |
| PQ-10 | The result publish **collides with the poll** on a dual-role Modbus: retry the publish, give the publish priority over the poll, or treat it as retryable in the controller? | item 64 (field, ~2.4 % of cycles) | Abort the cycle with `201`, `bErrorReset` required. The three options are listed in the backlog, "needs a decision, not a quick patch". | It is happening on the real cell. Constraint: a send must resolve exactly once; it must never report "sent" when nothing reached the register. |
| PQ-11 | `onVisionOutputResultFinished()` assigns `ReadyForTrigger` directly, bypassing the `markRuntimeReady()` gate — deliberate (a fast path) or a hole? | source survey | It is the only path into `ReadyForTrigger` that does not go through the gate; `markRuntimeReady()` is called immediately afterwards, so it is harmless today. | When `transitionTo()` + the guard table are introduced, this row must have an answer. Cheap. |

## Cluster D — Command source (before the TCP command work)

| PQ | Question | Source | Today | Why a ruling is needed |
|---|---|---|---|---|
| PQ-12 | TCP command (`Trigger,`/`ChangePart,`/`ChangeCamera,`): alongside the PLC, or mutually exclusive per task? Which has priority on a conflict? Over which channel is it acked? What does a held trigger mean over TCP? | `cratch_notes.md` "TCP/IP msg control" | Nothing. The events in the controller are bound to PLC tags. | This is the largest change of shape the state machine faces; ruled on beforehand the core only gains Event rows, ruled on afterwards `handlePlcValues()` has to be reworked. |
| PQ-13 | Dead entry points (`execute()`/`executeLocalization()`, `queueSetActiveCameraNumber()`, `queueSetActivePatternGroupNumber()`, `onCommDeviceValueChanged()`): delete them, or keep them as the path for UI/TCP? | source survey; Phase 9 C5 already deleted 4 others | No production caller or connect. | The API surface is wider than the behaviour; a later agent is easily misled. If PQ-12 needs a "manual trigger" they are kept and wired; if not they are deleted. |

## Cluster E — Configuration / fault codes

| PQ | Question | Source | Today | Why a ruling is needed |
|---|---|---|---|---|
| PQ-14 | Recovery policy: keep the hard default, or a per-task setting that persists? | item 67; DR-0005 (defer) | `setRecoveryPolicies()` is called only by tests; ships on the default 5000 ms / unlimited. | Not urgent; a DR saying "defaults only until a cell asks" is enough. |
| PQ-15 | Code `400` carries 4 different content-invalid errors: split the code? | item 63 | One code, four causes, and the PLC cannot tell them apart. | A fault code is a stable contract; adding a new code is cheap, changing an old one is expensive. Rule on it before the Phase 10 manual. |
| PQ-16 | The robot pick check has **two editors** (task settings and each vision-output device): keep both, or drop the device-side editor (deprecation + migration)? | item 66; Phase 9 open question O-2 | The task reads `TaskLocalizeConfig::robotCheckConfig()`; the device-side editor now only controls the transport's advisory check. The two places can drift apart silently. | It is a configuration-ownership question, not core state, but the Phase 10 manual has to describe exactly one place. Added 2026-09-16 from the WP-03 triage. |

## Suggested order

1. **Cluster A** (PQ-1..4) — the pilot, one full loop; Phase G already provides the base; PQ-2 is a
   real open bug.
2. **PQ-9, PQ-11, PQ-8** — cheap, mostly writing a DR + a constant; do them inside D0 so the contract
   table has no "no DR" cell left.
3. **PQ-6** — rule on it before the table is extracted (it affects the number of states).
4. **Cluster D** — before the TCP command work starts.
5. **PQ-5, PQ-7, PQ-10** — together with the investigation of items 70 and 64 (which are partly
   device bugs).
6. **Cluster E** — before the Phase 10 manual.

Items that are not core policy are handled through the ordinary debug loop, but their scenarios still
go into the catalogue: item 64 (transport), item 70 (client redial), item 62 (build), item 65
(qmake).
