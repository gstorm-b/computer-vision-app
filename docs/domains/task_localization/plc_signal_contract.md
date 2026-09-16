# PLC Signal Contract

This document defines the logical PLC contract for localization runtime.

`TaskLocalizeConfig` stores PLC tag names for each logical signal. The
`LocalizationSignalMapper` maps incoming PLC tag values to logical signal
names and maps outgoing logical signal names back to PLC tags.

Source:

- `src/model/task_localization_config.h`
- `src/model/localization_signal_mapper.*`
- `src/model/localization_runtime_controller.*`
- `src/model/localization_fault_code.h`

## Signal Names

| Signal | Type | Direction | Meaning |
| --- | --- | --- | --- |
| `nActiveCamera` | number | PLC -> task | Commanded camera number. **A command register the master owns; the task never writes it.** |
| `nActivePatternGroup` | number | PLC -> task | Commanded pattern group number. Master-owned; never written by the task. |
| `nActiveCameraStatus` | number | task -> PLC | Camera number the task actually adopted. Optional; see "Active Selection Status". |
| `nActivePatternGroupStatus` | number | task -> PLC | Pattern group number the task actually adopted. Optional. |
| `nDetectedNumber` | number | task -> PLC | Number of detected possible pick positions. |
| `nFaultCode` | number | task -> PLC | Stable fault code. |
| `bCameraValid` | bool | task -> PLC | Active camera binding and calibration are valid. |
| `bPatternValid` | bool | task -> PLC | Active pattern group is usable. |
| `bTaskReady` | bool | task -> PLC | Runtime is ready to accept a new trigger. |
| `bExecuteTrigger` | bool | PLC -> task | Rising-edge cycle trigger. |
| `bMatchingFinished` | bool | task -> PLC | Accepted trigger has been handled. |
| `bMatchingBusy` | bool | task -> PLC | Cycle is currently running. |
| `bMatchingDetected` | bool | task -> PLC | Last cycle detected at least one object. |
| `bMatchingLowArea` | bool | task -> PLC | Last result was below configured work-area threshold. |
| `bTaskFault` | bool | task -> PLC | Task has an active fault or last cycle faulted. |
| `bErrorReset` | bool | PLC -> task | Rising-edge fault acknowledge. Optional; see "Fault Acknowledge And Auto-Recovery". |

`nActivePattern` is intentionally not supported. Use
`nActivePatternGroup`.

`bErrorReset` may be left unbound. A latched cycle fault clears itself after
`LocalizationRuntimeController::kFaultAutoRecoverMs` (2000 ms) whether or not the
input is mapped.

## Setup Validation (The Signal-Map Gate)

The map is checked when the runtime starts, **before any device is asked to connect**, and a map
that cannot work refuses the start with a message that names the signal
(`LocalizationRuntimeController::validateSignalMap()`, called from `setup()`). Every refusal is
written to the user log and to the operator's task log.

| Condition | Result |
| --- | --- |
| A **required** signal has no tag. Required: `bExecuteTrigger`, `bTaskReady`, `bMatchingFinished`, `bTaskFault`, `nFaultCode` (`LocalizationRuntimeController::requiredSignalNames()`). | Refused: *"Required signal … is not mapped to a PLC tag."* |
| An **optional** signal — every other one — has no tag. | Starts. One warning per signal: *"Optional signal … is not mapped; the runtime will not report it to the PLC."* |
| Two signals share **one tag**. | Refused, naming both signals and the tag. |
| A tag the bound PLC **does not provide**, checked per kind: a `b…` signal must name a bit, an `n…` signal a register. | Refused, naming the signal, the tag and the device. Skipped only for a PLC family that cannot list its tags (no `IPlcTagProvider`). |
| At startup, the PLC holds a **non-number** for `nActiveCamera` or `nActivePatternGroup` — the tag is a bit area. | Refused: *"… is mapped to a tag that does not hold a number; check whether it is bound to a bit area."* |

A refused start leaves the task `Faulted` with *"Runtime start aborted: setupTask failed"* in its
state history. That line names nothing; the actionable reasons are the logged messages above.

## Active Index Signals

`nActiveCamera` and `nActivePatternGroup` select which camera and which pattern
group the next cycle uses. **A value is valid only when it is both inside the
index range and already registered in the project.** Nothing else is accepted,
and there is no reserved or magic value.

| | Valid range | Where the range is enforced |
| --- | --- | --- |
| `nActiveCamera` | **1..16** | `TaskDeviceBinding::kMinCameraNumber` / `kMaxCameraNumber`; a saved binding outside the range is rejected when the project loads |
| `nActivePatternGroup` | **1..32** | `MatchGroup::validateIndexRange()`; the pattern manager enforces the same bound when a group is created or renumbered |

Both checks run, in this order, and each fails differently on purpose:

1. **Out of range** — the number can never name a camera or a group. Includes
   **0** and every negative value.
2. **In range but not registered** — the number could name one, but this project
   binds no camera and defines no group with it.
3. **Not a number** — the tag is mapped to a bit area (a coil or discrete
   input), so no index check can diagnose it. The message names the tag.

Every one of the three is a fault, not a silent no-op:

```text
bTaskReady   = false
bCameraValid = false      (nActiveCamera)      -- or --
bPatternValid = false     (nActivePatternGroup)
bTaskFault   = true
nFaultCode   = 103 (camera) or 400 (pattern group)
```

`103` is `CameraNotRegistered`, not `100`/`CameraLost`. A number that names no
camera never named a connection either, and a master that branches on 100 sends
someone to check cabling for a selection problem. Changed 2026-09-09; a PLC
program written against the old 100 for this case must be updated.

and the runtime stops accepting triggers until a valid number is written.

**`0` is not "no selection".** It is simply out of range, because both index
spaces start at 1. The runtime's own "nothing selected yet" sentinel is `-1`,
used only internally at setup, and it is never written to or read from the PLC.

### At Startup

The runtime takes its starting selection **from the PLC**, not from the project file, and it
does so before it binds the camera (Phase 9 / C6):

1. `TaskLocalization::beginRuntime()` connects the primary PLC and vision-output roles and waits,
   bounded by `TaskLocalization::kPlcSnapshotWaitMs` (2000 ms), for the PLC's first
   whole-register snapshot (`TaskLocalization::awaitPrimaryPlcSnapshot()`). The MC device
   publishes that snapshot only after a complete read pass (Phase 9 / E5), so it holds what the
   master has in its registers rather than a partial read.
2. `setup()` asks the snapshot for each index (`commandedIndexFromPlc()`) and runs the value
   through **the same validator** a runtime write goes through (`validateCameraNumber()`,
   `validatePatternGroupNumber()`).

| The PLC… | The task… |
| --- | --- |
| holds a **valid** number | adopts it. The startup log line says `commanded`. |
| holds **0**, or any number out of range or unregistered | **does not adopt it** and faults, exactly as a written 0 does: `bTaskReady = false`, `bTaskFault = true`, the domain flag false, `nFaultCode = 103` (camera; it wins if both are refused) or `400` (pattern group). The refusal is latched per signal, the project default stays bound, and the runtime stays set up — so writing a valid number re-arms it with no restart and no operator action. |
| holds a **non-number** (the tag is a bit area) | refuses the start; see "Setup Validation". |
| has the signal **unmapped**, or sent **no snapshot within 2000 ms** | falls back to the project's first binding (`firstKey()`). The startup log line says `project default (firstKey)`; the timeout case also logs a warning, because a selection the master intended may not have been read. |

The resolved selection is reported once per start, on the user log and the task log
(`logStartupSelectionSummary()`):

```text
Runtime selection: camera 1 (commanded, device 02), pattern group 3 (commanded); workspace crop=on condition=on conditionRoi=(...).
```

and it is published on the two status outputs described below.

Confirmed on the cell 2026-09-09: a runtime started with the master holding camera 0 and pattern
group 0 faults instead of going Ready, and correcting both registers returns it to Ready without a
restart.

> **Earlier versions of this section said the opposite** — that a power-up 0 was never detected
> and the task quietly ran on `firstKey()`. That was true until Phase 9 / C6 and is not true now.
> It is recorded because a PLC program written while it was true may rely on it: such a master now
> sees a fault where it used to see Ready.

**Each signal clears only its own refusal.** A refusal is latched per signal, and
the latch is released only by a valid value written to *that same signal*.
Nothing else releases it: not a valid value on the other index signal, not a
device reconnecting, not `bErrorReset`. While either latch is held, `bTaskReady`
stays false and the runtime does not re-arm, however healthy everything else is.

This is what the latch exists for. A rejected number is never adopted — the
previously selected camera or group stays bound, so the runtime remains usable
and a later valid write can recover it — which means the rejection leaves nothing
behind for the readiness check to notice. Without a latch, the sequence *"write
`nActiveCamera` = 0 (refused), then write a perfectly good
`nActivePatternGroup`"* re-armed the task and republished `bCameraValid` as true
while the master's own command register still held the 0 it had refused. The task
then accepted triggers and ran cycles on a camera nobody had selected.

`bErrorReset` behaves here exactly as it does for a `PatternNotRegistered` fault: it
clears `bTaskFault` and `nFaultCode` but does not re-arm, because acknowledging
is not repairing. The only repair is a valid number.

**Recovery needs no operator action.** Writing a valid number to the refused
signal re-arms the runtime by itself; `bErrorReset` is not required for this
class of fault.

**A cell that does not switch camera or pattern group should leave these signals
unbound**, with an empty tag. An unbound signal is never dispatched at all, so
the project's commissioned selection stands and nothing faults. That is the
supported way to say "always camera 1" — not mapping the register and leaving
it at 0.

### Active Selection Status

**The task never writes `nActiveCamera` or `nActivePatternGroup`.** Both are command registers the
master owns. The task used to echo an accepted number back onto them — a write race with the master
on a server binding, and refused outright on a Modbus client binding whose command registers are
input registers (backlog item 60). Since Phase 9 / C3 the adopted number has outputs of its own:

| Output | Carries | Published (`localization_runtime_controller.cpp`) |
| --- | --- | --- |
| `nActiveCameraStatus` | the camera number the task adopted | on every accepted change (`setActiveCameraNumber()`), at the end of every valid `setup()`, and on every return to Ready (`publishInitialReadyOutputs()`, before `bTaskReady`) |
| `nActivePatternGroupStatus` | the pattern group number the task adopted | at the same three points, from `setActivePatternGroupNumber()` on a change |

Both are **optional**. Left unmapped, the value still reaches the dashboard and nothing is written to
the PLC; the setup gate warns once per signal. A **refused** number is never published on either:
the status output keeps reporting what is actually bound, which after a refusal is the previous
selection. That difference between a command register and its status output is one way a master can
see a refusal without decoding `nFaultCode`.

**Migrating a cell whose master read the command register back** to confirm a selection: map the two
status tags first, move the master's readback logic onto them, **then** upgrade. Upgrading first
leaves that master reading back a register nothing confirms any more.

## Handshake Behavior

### Ready

When runtime is healthy and ready:

```text
bTaskReady = true
bMatchingBusy = false
bMatchingFinished = false
bMatchingDetected = false
bMatchingLowArea = false
bTaskFault = false
nDetectedNumber = 0
nFaultCode = 0
```

### Trigger

`bExecuteTrigger` is rising-edge triggered. A cycle starts only when:

- previous trigger state was false
- new trigger state is true
- internal state is `ReadyForTrigger`

If the PLC keeps `bExecuteTrigger = true`, the task must not enqueue another
cycle.

> **Not yet specified: a trigger that is already held high when the runtime starts.** Whether it
> fires one cycle is backlog item 54, owner-gated as Phase 9 / Phase G and not yet measured on
> hardware. Do not build a PLC program on either behaviour until this section states it.

### Cycle Start

When a trigger is accepted:

```text
bTaskReady = false
bMatchingBusy = true
bMatchingFinished = false
bMatchingDetected = false
bMatchingLowArea = false
bTaskFault = false
nDetectedNumber = 0
nFaultCode = 0
```

### Successful Cycle

After matching and VisionOutput send succeed:

```text
bMatchingBusy = false
bMatchingFinished = true
bMatchingDetected = (detectedNumber > 0)
bMatchingLowArea = <matchResult.isAreaLessThanLimits>
bTaskFault = false
nDetectedNumber = <detectedNumber>
nFaultCode = 0
```

If `bExecuteTrigger` is still true, the controller waits in
`WaitingTriggerReset`.

When `bExecuteTrigger` falls to false:

```text
bMatchingFinished = false
bTaskReady = true
```

### Faulted Cycle

For an abnormal cycle failure:

```text
bMatchingBusy = false
bMatchingFinished = true
bTaskReady = false
bMatchingDetected = false
bMatchingLowArea = false
nDetectedNumber = 0
bTaskFault = true
nFaultCode = <specific fault code>
```

`bMatchingFinished = true` means the accepted trigger has been handled, even
when it failed. The PLC must check `bTaskFault` and `nFaultCode`.

### Fault Acknowledge And Auto-Recovery

A cycle fault is latched, but never permanently. There are two ways out and they run
the same recovery body, so they differ only in what the event log records.

**1. `bErrorReset` rising edge.** Edge-triggered, not level-triggered: holding the bit
high does not repeatedly clear the fault, so a genuinely repeating fault stays visible.

**2. Automatic, after 2000 ms.** If no `bErrorReset` edge arrives within
`LocalizationRuntimeController::kFaultAutoRecoverMs` of the fault coming to rest, the
runtime clears it itself. An acknowledge that arrives first cancels the timer.

Either way:

```text
bTaskFault = false
nFaultCode = 0
```

then the runtime attempts to re-arm. **Clearing is unconditional; re-arming is not.**
`markRuntimeReady()` still requires every role healthy and a valid active pattern group
and camera calibration. So a `PatternNotRegistered` or `CalibrationInvalid` fault clears its
flags but does **not** return to `bTaskReady = true` — nothing has repaired the cause.
This is an acknowledge, not a repair: if the cause persists, the next cycle re-faults.

The countdown is armed only where a cycle fault comes to rest — either in `abortCycle()`
when `bExecuteTrigger` is already low, or on the trigger's falling edge. It is never
armed underneath an asserted trigger, which would break the rising-edge handshake. It is
cancelled by an acknowledge, by a new cycle starting, and by runtime teardown.

Setup-failure faults (missing role, invalid pattern group, invalid calibration at
`setup()` time) do **not** arm the countdown. Those causes cannot resolve on their own. Neither
does a PLC-commanded index refused at startup: it is repaired only by a valid write to that
signal (see "At Startup").

> ⚠️ **`bTaskFault` is a pulse, not a latched state.** With auto-recovery it can be true
> for as little as 2 seconds. A PLC that samples slowly may miss it, and any logic that
> waits for a *persistent* `bTaskFault` will wait forever.
>
> **Sample on `bMatchingFinished` instead.** It goes true on the faulted cycle and stays
> true until the trigger is reset, so it is a stable edge on which to read `bTaskFault`
> and `nFaultCode` together. A PLC program that needs a persistent fault record must
> latch it on its own side.

A fault that keeps auto-clearing is reported: every
`LocalizationRuntimeController::kAutoRecoverWarnStride` (5) consecutive automatic
recoveries produce one `LOG_USER_WARN` naming the repeating fault code. The counter
resets on the first cycle that completes end to end. Individual auto-recoveries are
logged at DEV level only — reporting each one would bury the event log this path exists
to keep readable.

## Fault Codes

Stable numeric values:

| Code | Name | Meaning |
| ---: | --- | --- |
| 0 | `None` | No active fault. |
| 100 | `CameraLost` | Active camera lost connection or runner is unavailable. |
| 101 | `CameraConnectFailed` | Active camera could not connect. |
| 102 | `CameraGrabTimeout` | Every single-shot attempt failed, timed out, or returned no valid frame. `CameraRunner` retries a failed grab up to 6 attempts before this is raised, so a repeating 102 is a hardware problem rather than a dropped frame. |
| 103 | `CameraNotRegistered` | The active camera number names no usable camera: outside the legal range, or no camera bound to it. A **selection** fault, not a connection one — check `nActiveCamera` and the task's camera bindings, not the cabling. Also raised when `nActiveCamera` is mapped to a tag that does not hold a number, because no camera number was received at all. Raised at startup too, when the PLC already holds such a number; see "At Startup". |
| 200 | `VisionOutputLost` | Vision-output device lost connection or runner is unavailable. |
| 201 | `VisionOutputSendFailed` | Result payload could not be sent. |
| 300 | `PlcLost` | Primary PLC lost connection. |
| 301 | `PlcWriteFailed` | A **handshake** output could not be written and the retry budget ran out. The link is up — writes are being refused or lost — so a cable check finds nothing; look at the tag's writability and the register span. Only `bTaskReady`, `bMatchingFinished`, `bTaskFault`, `nFaultCode` and `nDetectedNumber` are retried and escalated; every other output is advisory and log-only. A write issued while the PLC role is disconnected does **not** raise this — that is `PlcLost`. |
| 400 | `PatternNotRegistered` | Active pattern group number names no usable group: out of range, missing, or holding no train image. Renamed from `PatternInvalid`; **the value 400 is unchanged**, so PLC programs branching on it need no edit. Raised at startup too, when the PLC already holds such a number. |
| 401 | `CalibrationInvalid` | Active camera has no valid calibration for world-coordinate output. |
| 500 | `InternalError` | Unexpected internal runtime error. |

Do not renumber existing codes. Add new codes in a documented range and update
tests.

## PLC Write Path

Runtime output publishing calls:

- `PlcRunner::requestWriteDigitalIo(tag, value)`
- `PlcRunner::requestWriteWordIo(tag, value)`

`PlcRunner` resolves the optional `IPlcIoWriter` capability on the concrete PLC
device. It is implemented by `McProtocolDevice` (tag families such as `Mxxxx` and
`Dxxxx`), `ModbusTcpClientDevice`, `ModbusTcpServerDevice` and the hardware-free
`VirtualPlcDevice`.

If an output signal has no mapped PLC tag, the runtime still emits
`signalChanged(name, value)` for UI/dashboard consumers and skips the PLC write.

### Handshake Writes Are Acknowledged

Since Phase 9 / E4 every write completes with a result (`PlcRunner::writeFinished(id, ok,
message)`), and the runtime acts on that result for the **five outputs a PLC program blocks on**
(`LocalizationRuntimeController::isHandshakeSignal()`):

`bTaskReady`, `bMatchingFinished`, `bTaskFault`, `nFaultCode`, `nDetectedNumber`

A failed handshake write is re-issued after `kPlcWriteRetryDelayMs` (40 ms), for at most
`kPlcWriteRetryBudget` (**3**) attempts in total — the original write plus two re-issues
(`onPlcWriteFinished()`). When the last attempt fails, the cycle is aborted with **301
`PlcWriteFailed`**, and the failure is also reported through the task log and `runtimeFault`,
which do not depend on the link that just refused a write (`escalatePlcWriteFailure()`).

Three kinds of write are deliberately **not** tracked (`trackHandshakeWrite()`):

- **Every other output.** They are advisory — a lost `bMatchingLowArea` costs a lamp, a lost
  `bMatchingFinished` hangs the PLC — and retrying all of them would turn a degraded link into a
  write storm. Their failures are logged only.
- **Any write issued while the PLC role is not `Connected`.** That is the recovery path's
  business and it already reports `300 PlcLost`; counting it here as well would raise a second,
  wrong fault on every cable pull.
- **The `bTaskFault` / `nFaultCode` writes the 301 abort itself publishes.** Tracking them over the
  link that just failed would re-enter the retry machinery; escalation is attempted once.

## Recovery Behavior

When a required device role becomes unhealthy:

- The controller enters recovery and publishes `bTaskReady = false`.
- Recoverable statuses are retried at the role policy's interval.
- **Retrying is unlimited.** It continues until the device reconnects or the runtime
  ends. There is no retry budget and no escalation to a task fault.
- On reconnect, the runtime returns to Ready by itself, with no operator action. The
  ready message reports how many attempts it took and how long the outage lasted.

> ⚠️ **A dead link no longer raises `bTaskFault`.** A device that never comes back leaves
> the task in Recovering indefinitely, publishing:
>
> ```text
> bTaskReady = false
> bTaskFault = false
> ```
>
> This is the intended trade — the line recovers from a dropped cable without anyone
> walking to the panel — but it means **a PLC program that detects a dead device by
> waiting for `bTaskFault` will wait forever.** Watch `bTaskReady` instead, and apply
> your own timeout if a maximum outage duration matters.

If a device is lost during `Running`, the cycle is aborted with the role-specific
fault code and stale matching results are ignored. That is a *cycle* fault and still
sets `bTaskFault` — see "Fault Acknowledge And Auto-Recovery" for how it clears.

If the PLC is disconnected, app task state and logs remain the source of truth.
On recovery, current runtime state is republished instead of silently clearing a
fault.

### Recovery Logging

Because retrying is unlimited, the logging is rate-limited instead — an outage lasting
minutes would otherwise emit an identical line every `retryIntervalMs` and bury
everything else in the operator's event log.

- `runtimeRecovering` is emitted when an outage begins and when its status changes
  (e.g. `LostConnected` → `ConnectFailed`), **not** on every retry. One outage produces
  one or two event-log entries, not one per attempt.
- Reconnect attempts reach the user log on attempt 1 and then once every
  `LocalizationRuntimeController::kQuietRetryLogStride` (12) attempts — roughly once a
  minute at the default 5000 ms interval.
- Every attempt is still written to the developer log, so a flapping link stays fully
  reconstructable after the fact. Nothing is discarded, only demoted.

