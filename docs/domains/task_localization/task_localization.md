# Localization Task Behavior Contract

This document defines the expected behavior of `TaskLocalization`. It is the
reference contract for implementing the runtime logic and for keeping the UI,
PLC handshake, camera acquisition, matching pipeline, and vision-output device
aligned.

## Signal Naming

- `nActiveCamera` selects the logical camera number.
- `nActivePatternGroup` selects the active pattern group number.
- `nDetectedNumber` reports the number of valid detected objects sent to the
  vision-output device.
- `bCameraValid` reports whether the active camera is ready.
- `bPatternValid` reports whether the active pattern group is ready.
- `bTaskReady` reports whether the task is ready to accept a new trigger.
- `bExecuteTrigger` is the PLC trigger input.
- `bMatchingBusy` is true while a localization cycle is running.
- `bMatchingFinished` is true after the current cycle has finished, and remains
  true until `bExecuteTrigger` returns to false.
- `bMatchingDetected` is true when the finished cycle produced at least one
  valid detected object.
- `bMatchingLowArea` mirrors the matcher low-area result for the finished cycle.
- `bTaskFault` reports whether the task is faulted or whether the current
  trigger cycle ended with an abnormal failure. It is a pulse, not a latched
  state — see "Leaving A Cycle Fault".
- `nFaultCode` reports the current fault reason as a stable numeric code.
- `bErrorReset` acknowledges a latched fault on its rising edge. Optional: a
  cycle fault also clears itself after 2000 ms.

## Phases And States

There are three related concepts:

- Runner phase: `Idle`, `Commission`, `Runtime`.
- Task state: `Idle`, `CommissionStarting`, `Commission`, `RuntimeStarting`,
  `Ready`, `RunningCycle`, `Recovering`, `Faulted`, `Stopping`.
- Runtime cycle state: an internal controller state such as `NotReady`,
  `ReadyForTrigger`, `Running`, `WaitingTriggerReset`, `Recovering`, `Faulted`.

The runtime cycle state should stay inside the localization runtime controller.
It does not need to become a public task state unless the UI needs it later.

## Commission Phase

When entering commission:

1. Start runners for all devices assigned to the task.
2. Keep device operation driven by the commissioning UI. The task does not need
   to auto-connect every assigned device.
3. Test matching is handled by the task's matching worker thread.
4. The test flow receives the selected pattern group and source image, runs
   matching, and returns the result through `commissionMatchingFinished`.
5. Commission matching must not write PLC outputs, send a vision-output request,
   or mutate runtime cycle state.
6. Device disconnects in commission are surfaced through logs/UI status only.
   Runtime recovery policy is not active in commission.

## Runtime Startup

When entering runtime:

1. Synchronize task runners with the assigned devices.
2. Validate task bindings:
   - One primary PLC device is assigned and has a runner.
   - One vision-output device is assigned and has a runner.
   - At least one camera-number binding exists.
   - The active camera number resolves to an assigned camera runner.
   - The active pattern group exists and contains train images.
3. Start runtime resources.
4. Connect the required runtime devices:
   - Primary PLC.
   - Vision-output device.
   - Active camera only.
5. The task becomes ready only after required devices are connected and the
   active pattern group is valid.

Threading contract:

- Per-device runner threads are kept from commission into runtime.
- Camera, PLC, and vision-output I/O stay on their device runner threads.
- Runtime coordination must talk to devices through queued runner APIs, not
  direct cross-thread device calls.
- In the first implementation pass, the runtime controller object remains owned
  by the task object and coordinates through queued signals. Moving it into the
  task runtime thread is a follow-up that requires queued task-to-controller
  invocations and adjusted QObject ownership.

Initial PLC output state after runtime becomes ready:

- `bTaskReady = true`
- `bCameraValid = true`
- `bPatternValid = true`
- `bMatchingBusy = false`
- `bMatchingFinished = false`
- `bMatchingDetected = false`
- `bMatchingLowArea = false`
- `bTaskFault = false`
- `nDetectedNumber = 0`
- `nFaultCode = 0`

If setup or connection preparation fails, transition to `Faulted` and do not
accept triggers.

## Runtime Trigger Cycle

`bExecuteTrigger` is edge-triggered. A localization cycle starts only on the
rising edge `false -> true` while the internal cycle state is `ReadyForTrigger`.

Cycle sequence:

1. Transition task state to `RunningCycle`.
2. Set handshake outputs:
   - `bTaskReady = false`
   - `bMatchingBusy = true`
   - `bMatchingFinished = false`
   - `bMatchingDetected = false`
   - `bMatchingLowArea = false`
   - `bTaskFault = false`
   - `nDetectedNumber = 0`
   - `nFaultCode = 0`
3. Request a single-shot image from the active camera through `CameraRunner`.
4. If image acquisition fails, abort the cycle and enter recovery/fault handling.
5. Snapshot the active pattern group before matching so worker-thread matching
   does not race with GUI edits.
6. Run image matching. The first implementation pass runs matching
   synchronously after the frame arrives; moving runtime matching to a worker is
   a follow-up optimization if latency or UI responsiveness requires it.
7. Convert match-result image coordinates to world coordinates using the active
   camera's calibrator.
8. Build and send a `VisionOutputRequest` through the assigned vision-output
   device.
9. Complete the handshake:
   - `bMatchingBusy = false`
   - `bMatchingFinished = true`
   - `bMatchingDetected = detectedNumber > 0`
   - `bMatchingLowArea = matchResult.isAreaLessThanLimits`
   - `bTaskFault = false`
   - `nDetectedNumber = detectedNumber`
   - `nFaultCode = 0`
10. Move to `WaitingTriggerReset`.
11. When `bExecuteTrigger` returns to false:
    - `bMatchingFinished = false`
    - `bTaskReady = true`
    - Return to `ReadyForTrigger`.

While `bExecuteTrigger` remains true after a finished cycle, the task must not
start another cycle.

## Fault Outputs

`bMatchingFinished` means the trigger cycle has been handled. It does not mean
the cycle succeeded. A successful cycle has `bTaskFault = false`; an abnormal
cycle failure has `bTaskFault = true` and a non-zero `nFaultCode`.

Fault code contract:

| Code | Name | Meaning |
| ---: | --- | --- |
| 0 | `None` | No active fault. |
| 100 | `CameraLost` | The active camera lost connection. |
| 101 | `CameraConnectFailed` | The active camera could not connect. |
| 102 | `CameraGrabTimeout` | Every grab attempt failed. `CameraRunner` retries a failed single-shot up to 6 attempts first, so this code means the whole budget was exhausted — see "Camera Grab Retry". |
| 200 | `VisionOutputLost` | The vision-output device lost connection. |
| 201 | `VisionOutputSendFailed` | The result payload could not be sent. |
| 300 | `PlcLost` | The primary PLC lost connection. |
| 400 | `PatternInvalid` | The active pattern group is missing or not usable. |
| 401 | `CalibrationInvalid` | The active camera has no valid calibration for world-coordinate output. |
| 500 | `InternalError` | Unexpected internal task error. |

Cycle-fault handshake:

1. Abort the current cycle.
2. Set `bMatchingBusy = false`.
3. Set `bMatchingFinished = true` if PLC output is still writable, so the PLC
   does not wait forever for the accepted trigger.
4. Set `bTaskReady = false`.
5. Set `bMatchingDetected = false`.
6. Set `bMatchingLowArea = false`.
7. Set `nDetectedNumber = 0`.
8. Set `bTaskFault = true`.
9. Set `nFaultCode` to the specific fault code.
10. Move to recovery or `Faulted` depending on the recovery policy.

### Leaving A Cycle Fault

A latched cycle fault clears in one of two ways, both running the same recovery body:

- a rising edge of `bErrorReset` (edge-triggered, and optional — the tag may be unbound);
- automatically, after `LocalizationRuntimeController::kFaultAutoRecoverMs` (2000 ms),
  when no acknowledge arrives. An acknowledge that arrives first cancels the countdown.

Both clear `bTaskFault`/`nFaultCode` unconditionally and then *attempt* to re-arm. The
re-arm still validates roles, pattern group and calibration, so a configuration fault
clears its flags without returning to `bTaskReady = true`. Acknowledging a fault is not
repairing it.

Because of the automatic path, `bTaskFault` can be true for as little as 2 seconds.
Sample it (and `nFaultCode`) on `bMatchingFinished`, which stays true until the trigger
is reset. Full contract and rationale:
[plc_signal_contract.md](plc_signal_contract.md) → "Fault Acknowledge And Auto-Recovery".

If the PLC itself is disconnected, PLC outputs may not be writable. In that
case, task state and application logs are the source of truth until the PLC
reconnects. After PLC recovery, the task should publish the current fault state
instead of silently clearing it.

Grab timeout is a camera fault, not a valid "zero detected" result. The task
must not send a normal `0;` vision-output payload when no valid image was
captured.

### Camera Grab Retry

A failed single-shot is retried inside `CameraRunner` — 6 attempts in total — before the
cycle is faulted, so a single dropped frame does not interrupt automatic operation. The
task never sees the intermediate failures: the runner re-emits `grabFinished` only for the
outcome the cycle should act on.

The practical reading for an operator: **fault 102 means the camera failed six times in a
row.** Treat a repeating 102 as hardware, not as a flaky frame.

Implementation detail and the two constants that must stay ordered:
[runtime_controller_api.md](runtime_controller_api.md) → "Camera Grab Retry".

## Vision Output Payload

The vision-output payload contains world coordinates, not raw image-pixel
coordinates.

For each valid matched object:

1. Start from the matched object's image position and angle.
2. Convert image coordinates to world coordinates with the active camera's
   calibrator.
3. Emit `x`, `y`, `z`, and `r` into `VisionOutputPosition`.

Only valid, non-colliding objects should be counted and sent. If no valid
object is detected, send an empty result payload with detected number `0`.

## Runtime Updates During Operation

Camera changes:

- `nActiveCamera` may change only when no localization cycle is running.
- If the task is `ReadyForTrigger`, switch the active camera by disconnecting
  the current active camera and connecting the newly selected camera.
- If a cycle is running, defer or reject the change until the cycle reaches
  `WaitingTriggerReset` or `ReadyForTrigger`.
- `bCameraValid` must reflect whether the selected active camera is connected
  and ready.

Pattern-group changes:

- `nActivePatternGroup` may change only when no localization cycle is running.
- If the selected group is missing or has no train images, set
  `bPatternValid = false`, set `bTaskFault = true`, set
  `nFaultCode = 400`, and do not accept triggers.
- If the selected group is valid, set `bPatternValid = true`.
- If a cycle is running, defer or reject the change until the cycle reaches
  `WaitingTriggerReset` or `ReadyForTrigger`.

Trigger handling:

- A trigger received while the task is not ready is ignored and logged.
- A repeated trigger while `Running` or `WaitingTriggerReset` must not enqueue a
  second cycle.

## Recovery Behavior

Runtime recovery is active only in runtime phase.

Default policy:

- Retry count: **unlimited** — retrying continues until the device reconnects or
  the runtime ends.
- Retry interval: 5000 ms.
- Recoverable statuses: `LostConnected`, `ConnectFailed`.

The distinction that matters throughout this section: **losing a device is not a
fault.** A lost role withdraws readiness and retries; only a cycle that was
already accepted and then failed sets `bTaskFault`.

Device-specific behavior:

- Any role lost while ready: set `bTaskReady = false`, enter `Recovering`, and
  retry that role. `bTaskFault` stays false — there is no fault to report, only
  an outage in progress.
- Active camera lost while running: abort the current cycle, clear busy output,
  set `bTaskFault = true`, set `nFaultCode = 100`, enter `Recovering`, and
  retry the active camera.
- Active camera grab timeout while running: abort the current cycle, clear busy
  output, set `bTaskFault = true`, set `nFaultCode = 102`, enter
  `Recovering`, and retry/reconnect the active camera according to policy.
- Vision-output lost while running: abort the current cycle, clear busy output,
  set `bTaskFault = true`, set `nFaultCode = 200`, enter `Recovering`, and
  retry the vision-output device.
- Vision-output send failure while running: abort the current cycle, set
  `bTaskFault = true`, set `nFaultCode = 201`, and enter `Recovering`.
- PLC lost while ready or running: enter `Recovering`. PLC outputs are not
  writable while the PLC is disconnected, so task state/logs become the source
  of truth until PLC recovery succeeds.

If recovery succeeds:

1. Revalidate active camera and active pattern group.
2. If the fault happened outside an accepted trigger cycle, clear
   `bTaskFault` and `nFaultCode`.
3. If the fault happened during an accepted trigger cycle, keep
   `bTaskFault = true` and keep `nFaultCode` until `bExecuteTrigger` returns
   to false.
4. Restore ready outputs after the trigger is reset.
5. Transition task state to `Ready`.
6. Return to `ReadyForTrigger`.

If recovery never succeeds, the task stays in `Recovering` and keeps retrying.
It does not transition to `Faulted`, and it does not accept triggers, because
`bTaskReady` is false for the whole outage. A PLC that needs a bounded outage
must impose its own timeout — see
[plc_signal_contract.md](plc_signal_contract.md) → "Recovery Behavior".

## Runtime Stop

When runtime is stopping:

1. Stop accepting triggers.
2. If possible, clear handshake outputs:
   - `bTaskReady = false`
   - `bMatchingBusy = false`
   - `bMatchingFinished = false`
   - `bTaskFault = false`
   - `nFaultCode = 0`
3. Disconnect active camera, primary PLC, and vision-output device.
4. Stop runtime resources and return to `Idle`.

If the PLC is already disconnected, skip PLC output cleanup and continue
shutdown.
