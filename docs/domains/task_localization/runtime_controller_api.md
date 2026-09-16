# LocalizationRuntimeController API

`LocalizationRuntimeController` is the runtime state machine and coordinator
for localization.

Source:

- `src/model/localization_runtime_controller.h`
- `src/model/localization_runtime_controller.cpp`

## Responsibility

The controller owns runtime behavior:

- PLC signal mapping and trigger handling.
- Runtime readiness and cycle state.
- Camera, PLC, and VisionOutput role bindings.
- Device connect/reconnect requests through runners.
- PLC output publishing through `PlcRunner`.
- Camera single-shot requests through `CameraRunner`.
- Vision result sends through the bound runner's result-output capability
  (`IDeviceRunner::supportsResultOutput()` / `requestSendResult()`), not through a
  named runner type.
- Fault publishing and recovery state.
- Dashboard cycle result and task-local log events.
- Runtime matching request/response coordination by cycle id.

The controller does not own device objects, task objects, pattern managers, or
the matching pipeline.

## Thread Ownership

During runtime, `TaskLocalization` moves the controller to
`TaskRunner::runtimeThread()`. The controller must not be parented to
`TaskLocalization` while moved.

All public runtime calls from the task should be invoked with queued
connections when the controller is on another thread.

## RuntimeContext

```cpp
struct RuntimeContext {
    TaskLocalizeConfig config;
    QString primaryPlcDeviceId;
    QString visionOutputDeviceId;
    QPointer<PlcRunner> primaryPlcRunner;
    QPointer<IDeviceRunner> visionOutputRunner;   // any family; capability-checked at setup
    QMap<int, QString> cameraDeviceIds;
    QMap<int, QPointer<CameraRunner>> cameraRunners;
    QMap<int, std::shared_ptr<mtc::MatchGroup>> patternGroups;
    QMap<int, calib::Calibrator> cameraCalibrators;
    RobotKinematicCheckConfig robotCheckConfig;   // the TASK's setting, never the device's
    CameraWorkspace activeCameraWorkspace;        // resolved by setup()
    int activeCameraNumber{-1};                   // -1: let setup() resolve it
    int activePatternGroupNumber{-1};             // -1: let setup() resolve it
    std::shared_ptr<PlcValueMap> plcSnapshot;     // null means "not read", never "holds 0"
};
```

`RuntimeContext` is a snapshot. Pattern groups and calibrators are copied before
runtime setup. Runtime edits to pattern groups or calibration are not supported.

Three fields changed meaning in Phase 9 and are easy to get wrong:

- **`robotCheckConfig`** comes from `TaskLocalizeConfig::robotCheckConfig()`. It used to be read off
  the device bound to `vision_output`, which silently disabled the check whenever that device had
  none commissioned (backlog item 57).
- **`activeCameraNumber` / `activePatternGroupNumber`** are left at `-1` by
  `TaskLocalization::buildRuntimeContext()`. `setup()` resolves them — from the PLC first, the
  project default second — because only it can report which of the two it used.
- **`plcSnapshot`** is what the PLC held when the runtime started. `null` is meaningful: it is
  treated exactly like an unmapped signal, never as a register holding 0.

If the controller needs more runtime data, extend `RuntimeContext`; do not add a
direct dependency on `TaskLocalization`.

## Setup API

```cpp
void configure(const TaskLocalizeConfig &config);
SetupResult setup(const RuntimeContext &context);
bool isValid() const;
```

`configure()` refreshes the internal `LocalizationSignalMapper`.

`setup()` runs, in order:

1. Resets cycle state, both selection latches and every role binding.
2. Checks the roles exist: a primary PLC runner, a vision-output runner that
   `supportsResultOutput()`, and a runner for every bound camera number.
3. Connects PLC value updates to `handlePlcValues` and binds the PLC and vision-output roles.
4. Runs the signal-map gate, `validateSignalMap()` — after step 3 on purpose, because the gate
   reads the PLC's tag list through the bound role.
5. Resolves the active camera — PLC-commanded first (`commandedIndexFromPlc()`), project default
   second — validates it, binds the camera role and resolves its workspace.
6. Resolves and validates the active pattern group the same way, then its content.
7. Validates the active camera's calibration and builds the robot pick checker
   (`rebuildPickingChecker()`).
8. If valid: logs the startup selection summary, publishes a refused PLC-commanded selection as a
   fault, publishes the two status outputs, requests every role to connect, and marks the runtime
   ready if all roles are already healthy.

Every failure falls into exactly one of three classes:

| Class | Causes | What happens |
| --- | --- | --- |
| **Hard error** — `SetupResult::errors`, `valid = false` | a missing role runner; a vision-output device that cannot output results; a missing camera binding or unregistered camera runner; any signal-map gate refusal; an index signal holding a non-number at startup; the resolved camera number refused (publishes `103`); the resolved pattern group out of range or unusable (publishes `400`); the active camera's calibration invalid (publishes `401`); the robot pick check **enabled** with a preset that does not resolve or a camera without usable calibration | The runtime does not start: the controller goes `Faulted`, every message goes to the user log and the task log, and `TaskLocalization` ends `Faulted` without emitting `runtimeStarted()`. |
| **Valid, not Ready** | a PLC-commanded index refused at startup (latched; publishes `103` / `400`); a required role still connecting | The runtime is set up and waits. A refused selection re-arms on the next valid write to that signal; a connecting role re-arms when it reports `Connected`. |
| **Warning only** | an optional signal with no tag; a PLC family that lists no tags (orphan check skipped, DEV log); no PLC snapshot within `TaskLocalization::kPlcSnapshotWaitMs` (logged before `setup()` runs) | Starts normally. The last case falls back to the project defaults. |

The pick-check refusal is suppressed only when the camera **index** itself was refused: "camera 7
has no usable calibration" is then derived noise on top of the message that names the real problem.
Full PLC-side detail: [plc_signal_contract.md](plc_signal_contract.md) → "Setup Validation" and
"At Startup".

## Runtime Inputs

```cpp
void execute();
void handlePlcValues(const QMap<QString, QVariant> &values);
void setActiveCameraNumber(int cameraNumber);
void setActivePatternGroupNumber(int patternGroupNumber);
```

`execute()` is a manual runtime trigger. It starts a cycle only from
`ReadyForTrigger`.

`handlePlcValues()` maps PLC tags into logical signal events. Important event
names:

- `nActiveCamera`
- `nActivePatternGroup`
- `bExecuteTrigger`
- `bErrorReset`

`bExecuteTrigger` is edge-triggered. Only a rising edge from false to true can
start a cycle, and only while the controller is ready.

`bErrorReset` is also edge-triggered: a rising edge acknowledges a latched fault
via `acknowledgeFault()`, which cancels any pending auto-recovery and then runs
the shared `recoverFromFault()` body. The tag is optional — a latched cycle
fault clears itself after `kFaultAutoRecoverMs` (2000 ms) through the same body.
See [plc_signal_contract.md](plc_signal_contract.md) →
"Fault Acknowledge And Auto-Recovery".

Both index values reach their setter **unfiltered**. There is no reserved or magic
value, and in particular `0` is not "no selection" — it is out of range, because
both index spaces start at 1. See [plc_signal_contract.md](plc_signal_contract.md)
→ "Active Index Signals" for the authoritative rule.

`setActiveCameraNumber()` rejects changes while a cycle is running. It then checks
**range first, registration second** — 0 or 99 can never name a camera, while 3
could but is not bound in this project, and the two deserve different messages. The
range comes from `TaskDeviceBinding::kMinCameraNumber`/`kMaxCameraNumber` (1..16),
the same constants that reject a saved binding at load, so the two checks agree by
construction. An accepted change disconnects the previous camera runner, binds the
new one, validates calibration, publishes `bCameraValid`, and requests connect
through `CameraRunner`.

`setActivePatternGroupNumber()` has the same shape, with the range taken from
`mtc::MatchGroup::validateIndexRange()` (1..32).

Neither setter ever writes the command register that drove it. An **accepted** number is reported
on `nActiveCameraStatus` / `nActivePatternGroupStatus`; a **refused** one on neither. `setup()`
uses the same two validators for the startup selection, so a number means the same thing whether it
arrives at start or later. See [plc_signal_contract.md](plc_signal_contract.md) →
"Active Selection Status".

**A refused number is latched, per signal.** A refusal is not adopted — the last
good camera or group stays bound, which is what lets a later valid write recover
with no operator action — so it leaves nothing for `markRuntimeReady()`'s other
checks to see: they read the *previous, good* selection and all pass. Each setter
therefore records its own rejection, `markRuntimeReady()` refuses to re-arm while
either latch is held, and a latch is cleared **only** by an accepted value for that
same signal. Not by a valid value on the other index signal, not by a device
reconnecting, and not by `bErrorReset` — acknowledging is not repairing, exactly as
for a `PatternNotRegistered` fault.

A **non-numeric** value on either signal is a third, separate failure: the tag is
mapped to a bit area, so no index check can diagnose it. `reportSignalTypeMismatch()`
faults, names the tag, publishes the domain flag false and latches like a bad index.

## Matching API

```cpp
signals:
    void runtimeMatchingRequested(int cycleId,
                                  std::shared_ptr<mtc::MatchGroup> group,
                                  CameraWorkspace workspace,
                                  cv::Mat image,
                                  std::shared_ptr<mtc::IRobotPickingChecker> pickingChecker);

public:
    void onRuntimeMatchingFinished(int cycleId, mtc::MatchResult matchResult);
```

The controller never runs matching directly. It emits
`runtimeMatchingRequested(...)` after a successful camera grab.

`TaskLocalization` connects this signal to the matching worker. The worker runs
`LocalizationPipeline::runMatch(...)` and queues
`onRuntimeMatchingFinished(cycleId, result)` back to the controller.

The controller ignores a matching result when:

- `cycleId` does not equal the current active cycle id.
- the controller is no longer in `Running`.

This makes late results harmless after aborts or recovery.

## Runtime Cycle State

Internal cycle states:

- `NotReady`
- `ReadyForTrigger`
- `Running`
- `WaitingTriggerReset`
- `Recovering`
- `Faulted`

The normal happy path:

1. `ReadyForTrigger`.
2. Rising `bExecuteTrigger`.
3. `Running`.
4. Publish cycle-start outputs.
5. Request camera single shot.
6. Receive grab frame.
7. Emit `runtimeMatchingRequested`.
8. Receive matching result.
9. Convert image coordinates to world coordinates through active calibrator.
10. Send positions through the bound runner's `requestSendResult()`.
11. Publish success outputs.
12. If trigger is still true, enter `WaitingTriggerReset`.
13. On trigger falling edge, clear `bMatchingFinished` and return ready.

Held trigger behavior is deliberate: a held true trigger must not enqueue a
second cycle.

## Output Coordinate Contract

`buildVisionOutputPositions()` converts each matched object from image
coordinates into world coordinates:

- `imageX/imageY` come from `MatchedObject::point_Center`.
- `imageR` comes from `MatchedObject::point_angle`.
- `world.x/y/z` comes from `calib::Calibrator::imageToRobot`.
- `world.r` comes from `calib::Calibrator::rotateImageToRobot`.

An object is sent only when it has **no collision**, lies **inside the active camera's condition
workspace**, and the robot pick check reports it **possible to pick**
(`buildVisionOutputPositions()`). Any other object gets a `Skipped: …` row naming the reasons.

> ⚠️ **At most two positions are sent per cycle.** `buildVisionOutputPositions()` appends only while
> `positions.size() < 2` and marks every further object that passed all three checks `Skipped` —
> with **no reason** in the row, unlike every other skip. The limit is not a named constant and no
> other document states it. It is described here because this page must say what the code does,
> and it is filed as backlog item 68 for the owner to confirm or remove.

## Fault And Recovery

The controller publishes fault state with:

- `bTaskFault = true`
- `nFaultCode = <stable code>`
- `bTaskReady = false`
- `bMatchingBusy = false`
- `bMatchingFinished = true` for cycle faults

Cycle abort increments the active cycle id so late matching results are ignored.

Role recovery is policy based. Default policies retry recoverable statuses:

- `LostConnected`
- `ConnectFailed`

Default retry settings are:

- unlimited retries — until the device reconnects or the runtime ends
- 5000 ms interval

**These defaults are what ships.** `setRecoveryPolicies()` has no production caller — its only
caller is the architecture contract test, which shortens the interval so it can cover many
attempts — so every runtime uses `defaultCameraRecoveryPolicy()`, `defaultPlcRecoveryPolicy()` and
`defaultVisionOutputRecoveryPolicy()` from `src/model/localization_recovery_policy.h`. Persisting
per-task policy values is deferred. When a production caller is added, the injection point is
`TaskLocalization::setupRuntimeController()`, immediately before `controller->setup(context)` and
inside the same thread hop: policies are copied into each role's recovery context when the role is
bound, and the controller already lives on the runtime thread by then.

A policy carries **no per-attempt connect timeout**. A `connectTimeoutMs` field used to exist and was
read by nothing; how long one connect attempt may take is decided by the role's runner and device.

Recovering roles:

- camera
- primary PLC
- vision output

Losing a role outside a running cycle publishes `bTaskReady = false` and enters
`Recovering`. It does **not** set `bTaskFault` and never escalates to `Faulted`.
`runtimeRecovering` is emitted when the outage starts and when its status changes,
not per retry.

`runtimeFault` is raised by an invalid setup, **by a refused active-index selection** — out of
range, in range but unregistered, or not a number, whether written at runtime or commanded by the
PLC at startup — and **by a handshake write that exhausted its retry budget** (`301`,
`escalatePlcWriteFailure()`). A lost device connection never reaches it, because role recovery
retries indefinitely instead of escalating.
Full contract: [plc_signal_contract.md](plc_signal_contract.md) →
"Recovery Behavior".

## Signals To Task/UI

```cpp
void signalChanged(QString name, QVariant value);
void cycleResultUpdated(CycleResult result);
void taskLogAppended(TaskLogEntry entry);
void runtimeCycleStarted(QString message);
void runtimeRecovering(QString message);
void runtimeReady(QString message);
void runtimeFault(QString message);
```

`signalChanged` is emitted for both incoming mapped PLC values and controller
published outputs. The dashboard and task-level signal monitor consume this.

`runtimeCycleStarted`, `runtimeRecovering`, `runtimeReady`, and `runtimeFault`
drive the `TaskLocalization` task state machine.

`cycleResultUpdated` carries `CycleResult::timings` (`CycleTimings`, Phase 9 / F3): milliseconds
from trigger-accept to grab finished, matching finished, send finished and outputs published, all
off one monotonic clock (`m_cycleClock`). A boundary the cycle never reached is **unset**, never 0 —
a faulted cycle keeps the stages it did reach. On a successful cycle the same breakdown goes to the
task log and, at DEV level, to the app log as
`Task localization: cycle=… ms (grab …, match …, send …, publish …)`. `matchingTimeMs` is the
matcher's own measurement and is not one of these stages.

## PLC Output Publishing

`publishBoolSignal(name, value)` and `publishNumberSignal(name, value)`:

1. Emit `signalChanged(name, value)`.
2. Resolve the mapped PLC tag by signal name.
3. If a tag exists, request a PLC write through `PlcRunner`.
4. For the five handshake outputs (`isHandshakeSignal()`), track the write's id. A failed write is
   re-issued until `kPlcWriteRetryBudget` (3) attempts have been made in total, then the cycle
   aborts with `301 PlcWriteFailed`. See [plc_signal_contract.md](plc_signal_contract.md) →
   "Handshake Writes Are Acknowledged" for what is deliberately not tracked.

Empty PLC output tags are allowed. The task-facing signal still emits.

## Race Guards

Every re-arm goes through `markRuntimeReady()`, and it refuses when any of these holds:

- **`Running` or `WaitingTriggerReset`.** `WaitingTriggerReset` counts as busy: the PLC still holds
  `bExecuteTrigger` and has not yet read the latched `bMatchingFinished`, `bMatchingDetected` and
  `nDetectedNumber`, and re-arming would publish the ready outputs over them. The same guard keeps a
  delayed device `Connected` event, or an accepted index write, from disturbing a cycle or its
  handshake.
- **Setup invalid, a required role unhealthy, the active pattern group unusable, or the active
  camera's calibration invalid.**
- **Either per-signal selection latch held.** A refused index is never adopted, so the checks above
  read the previous, good selection and pass; the latch is what keeps the refusal in force.

Camera command failures only abort immediately on command timeout. Non-timeout
grab failures are mapped by `onCameraGrabFinished()` to
`CameraGrabTimeout` when the frame is missing or invalid.

## Camera Grab Retry

**The retry lives in `CameraRunner`, not in this controller.** A failed single-shot is
re-issued up to `CameraRunner::kMaxGrabAttempts` (6 attempts: one grab plus five retries)
before the command is failed, so one dropped frame does not interrupt an automatic cycle.

The controller is deliberately blind to the retries:

- `grabFinished` is **not** re-emitted for an intermediate failure, so
  `onCameraGrabFinished()` sees exactly one outcome per cycle and cannot mistake a retry
  for a finished cycle;
- consequently **fault code 102 `CameraGrabTimeout` now means every attempt failed**, not
  that one did. A repeating 102 is a real hardware problem, not a flaky frame — which is
  why the fault auto-recovery path reports a repeating fault at
  `kAutoRecoverWarnStride` (see "Fault Acknowledge And Auto-Recovery" in the signal
  contract).

Two rules make the budget mean what it says:

- it resets when a `CameraSingleShot` command **starts**, not when one succeeds, so
  failures during commissioning cannot consume the next runtime cycle's budget;
- each attempt gets its own watchdog window. `CameraRunner::kSingleShotTimeoutMs` (8000 ms)
  must stay above the camera's own blocking-grab timeout
  (`BaslerGigECamera::kDefaultGrabTimeoutMs`, 5000 ms, overridable via `setGrabTimeout()`).
  When it did not, the runner gave up while the camera was still legitimately waiting and
  the real result arrived with no command left to resolve.
  `test_camera_runner_watchdog_outlasts_device_grab_timeout` locks that ordering.

### What the runner cannot do

The watchdog ends the *command*; it cannot unblock a device thread sitting inside a driver
call. The device is responsible for:

- reporting **every** grab through `grabFinished()`, on success and on failure alike — a
  silent return leaves the command hanging until the watchdog fires;
- publishing `LostConnected` when it detects removal, so recovery can start.

`BaslerGigECamera` does this from `grabSingleShot()` and
`publishRemovalIfDetected()`. That camera has no removal callback or heartbeat, so a
cable pulled while the runtime is **idle** is not noticed until the next grab.

