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
| `nActiveCamera` | number | PLC -> task and task -> PLC | Active logical camera number. |
| `nActivePatternGroup` | number | PLC -> task and task -> PLC | Active pattern group number. |
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
and camera calibration. So a `PatternInvalid` or `CalibrationInvalid` fault clears its
flags but does **not** return to `bTaskReady = true` — nothing has repaired the cause.
This is an acknowledge, not a repair: if the cause persists, the next cycle re-faults.

The countdown is armed only where a cycle fault comes to rest — either in `abortCycle()`
when `bExecuteTrigger` is already low, or on the trigger's falling edge. It is never
armed underneath an asserted trigger, which would break the rising-edge handshake. It is
cancelled by an acknowledge, by a new cycle starting, and by runtime teardown.

Setup-failure faults (missing role, invalid pattern group, invalid calibration at
`setup()` time) do **not** arm the countdown. Those causes cannot resolve on their own.

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
| 200 | `VisionOutputLost` | Vision-output device lost connection or runner is unavailable. |
| 201 | `VisionOutputSendFailed` | Result payload could not be sent. |
| 300 | `PlcLost` | Primary PLC lost connection. |
| 400 | `PatternInvalid` | Active pattern group is missing or has no train image. |
| 401 | `CalibrationInvalid` | Active camera has no valid calibration for world-coordinate output. |
| 500 | `InternalError` | Unexpected internal runtime error. |

Do not renumber existing codes. Add new codes in a documented range and update
tests.

## PLC Write Path

Runtime output publishing calls:

- `PlcRunner::requestWriteDigitalIo(tag, value)`
- `PlcRunner::requestWriteWordIo(tag, value)`

`PlcRunner` resolves the optional `IPlcIoWriter` capability on the concrete PLC
device. `McProtocolDevice` implements the writer and parses tag families such
as `Mxxxx` and `Dxxxx`.

If an output signal has no mapped PLC tag, the runtime still emits
`signalChanged(name, value)` for UI/dashboard consumers and skips the PLC write.

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

