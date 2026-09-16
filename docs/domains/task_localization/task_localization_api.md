# TaskLocalization API

`TaskLocalization` is the facade between the project/task framework and the
localization runtime implementation.

Source:

- `src/model/task_localization.h`
- `src/model/task_localization.cpp`

## Responsibility

`TaskLocalization` owns:

- The task identity and base `ITask` lifecycle.
- Persistent `TaskLocalizeConfig`.
- Assigned-device limits for PLC, VisionOutput, and Camera devices.
- Pattern group persistence through `PatternGroupManager`.
- Pattern image BLOB export/import for `ProjectRepository`.
- The `LocalizationPipeline` instance used by commission and runtime matching.
- The matching worker thread.
- Creation, destruction, and signal wiring of `LocalizationRuntimeController`.

`TaskLocalization` does not own runtime device I/O details. Runtime device
connect, disconnect, single-shot, PLC writes, and vision-output sends are routed
through the runtime controller and device runners.

## Public Surface

### Lifecycle

```cpp
void beginRuntime(bool mergeToTaskThread = false) override;
void endRuntime() override;
void stopAll() override;
```

`beginRuntime(false)` is the supported runtime mode for localization. If
`mergeToTaskThread` is passed as true, localization logs a warning and keeps
per-device threads.

Runtime startup order (`TaskLocalization::beginRuntime()`):

1. Transition task state to `RuntimeStarting`.
2. Sync task runners with assigned devices, and log one warning per virtual device in use.
3. Enter `TaskRunner` runtime in per-device-thread mode.
4. **Read the PLC first** (`awaitPrimaryPlcSnapshot()`): connect the primary PLC and
   vision-output roles, then wait — bounded by `kPlcSnapshotWaitMs` (2000 ms) — for the PLC's first
   whole-register snapshot. The active camera and pattern group are inputs the PLC owns, so they
   cannot be resolved before it has been read. Expiry is logged, not failed: `setup()` then falls
   back to the project defaults.
5. Create the runtime controller if needed and move it to `TaskRunner::runtimeThread()`.
6. Build `RuntimeContext` and set up the controller (`setupTask()`), with a blocking queued call
   when the controller is on another thread.
7. If setup is invalid, transition to `Faulted` (*"Runtime start aborted: setupTask failed"*) and
   **do not** emit `runtimeStarted()`.
8. Otherwise emit `ITask::runtimeStarted()`. It is emitted only after the runners are registered
   and attached, so a listener that resolves `taskRunner()->runnerFor(id)` on it gets a runner — the
   dashboard's connection lamps depend on exactly that. The task reaches `Ready` once every required
   role is healthy.

`endRuntime()` and `stopAll()` destroy the runtime controller before entering
idle, then recreate a fresh controller in the task thread for the next session.

Entering idle is a full teardown. `TaskRunner::enterIdle()` first closes each
device connection on its own worker thread (`IDeviceRunner::disconnectAndWait()`)
and only then detaches the device and stops the worker thread. This ordering is
mandatory: device transports (for example the MC PLC `QTcpSocket`) are created on
the worker thread and must be closed there. Skipping the disconnect leaks the
open connection and breaks the supported "stop runtime, edit, start runtime
again" workflow — the next phase entry cannot reconnect because the previous
session is still half-open. Device-side connect is therefore idempotent: calling
`deviceConnect()` while already connected is a no-op, not a re-init.

### Configuration

```cpp
void setTaskLocalizeConfig(const TaskLocalizeConfig &cfg);
TaskLocalizeConfig taskLocalizeConfig() const;
```

`setTaskLocalizeConfig()` updates the persistent config and queues
`LocalizationRuntimeController::configure()` when a controller exists.

`TaskLocalizeConfig` contains PLC tag names and device role bindings. See
[plc_signal_contract.md](plc_signal_contract.md).

### Pattern Management

```cpp
mtc::PatternGroupManager *patternManager() const;
void startCommissionMatching(std::shared_ptr<mtc::MatchGroup> group,
                             cv::Mat &image);
```

Commission matching is asynchronous. `startCommissionMatching()` clones the
image and emits `startCommissionMatchingRequest(...)`. The matching worker runs
`LocalizationPipeline::runMatch(...)` and emits `commissionMatchingFinished`.

Runtime matching uses the same worker thread but is initiated by the runtime
controller through `runtimeMatchingRequested(...)`.

### Runtime Commands

```cpp
public slots:
    void setupTask();
    void executeLocalization();
```

`setupTask()` builds the runtime context and calls controller setup.

`executeLocalization()` queues `LocalizationRuntimeController::execute()`.
Manual execution is accepted only when the task is in `Ready` or
`RunningCycle`.

**There is no task-level setter for the active camera or pattern group.** `setCameraNumber()`,
`setPatternNumber()` and the two `onSignalChange*` slots were deleted in Phase 9 / C5: nothing
connected the slots and nothing else called the methods, so they were live-looking entry points
reachable only by name. The two real paths are:

- **the PLC** — `onCommDeviceValueChanged()` → `queueHandlePlcValues()` →
  `LocalizationRuntimeController::handlePlcValues()`, which passes each index to its setter
  unfiltered;
- **a manual change** — the private `queueSetActiveCameraNumber()` /
  `queueSetActivePatternGroupNumber()`, queued onto the controller's thread.

Both end in the controller's setters, which validate range then registration and never touch a
device directly.

### Signals

```cpp
void commissionMatchingFinished(mtc::MatchResult result);
void cycleResultUpdated(LocalizationRuntimeController::CycleResult result);
void taskLogAppended(LocalizationRuntimeController::TaskLogEntry entry);
```

`cycleResultUpdated` drives dashboard result views. `taskLogAppended` is a
task-scoped operator log stream.

`TaskLocalization` also forwards controller signal changes through the base
`ITask::signalChanged` signal.

## RuntimeContext Construction

`TaskLocalization::buildRuntimeContext()` snapshots everything the runtime
controller needs:

- copied `TaskLocalizeConfig`
- primary PLC device id and `PlcRunner`
- vision-output device id and its runner, held as `IDeviceRunner` (the role is a
  capability, so the runner may come from any device family)
- camera number to device id map
- camera number to `CameraRunner` map
- camera number to camera calibrator map
- pattern group snapshots
- the **task's** robot pick-check settings, `TaskLocalizeConfig::robotCheckConfig()` — never the
  output device's (Phase 9 / F1)
- the PLC register snapshot read by `awaitPrimaryPlcSnapshot()`, or null if none arrived
- the active camera and pattern group left at `-1`, for `setup()` to resolve: only it can tell
  "commanded by the PLC" from "the project default" and report which one it used

The controller must not call back into `TaskLocalization` to resolve runtime
data after setup.

## Threading Model

During runtime:

- `TaskLocalization` stays in the task/object owner thread.
- `LocalizationRuntimeController` lives in `TaskRunner::runtimeThread()`.
- Camera, PLC, and VisionOutput devices stay on their per-device threads.
- Matching runs on `TaskLocalization::matchingRunner`.

All task-to-controller runtime calls must be queued. Controller-to-task
notifications are Qt signals connected as queued connections.

## Persistence API

```cpp
QJsonObject toJson() const override;
bool fromJson(const QJsonObject &obj) override;
QMap<QString, cv::Mat> getTaskImageMap() override;
bool loadTaskImageMap(QMap<QString, cv::Mat> &mapping) override;
```

JSON persistence is delegated:

- Base task state and config are handled by `ITask`.
- Pattern library schema is handled by `PatternGroupManager`.
- Pattern images are stored separately as BLOBs by `ProjectRepository`.

Pattern image keys use the stable format:

```text
g{groupNumber}_p{patternNumber}
```

Do not key image storage by group or pattern name; names are user-editable.

## Maintenance Notes

- Do not add runtime calls from `TaskLocalization` directly to concrete devices.
  Route through runners and the controller.
- Do not let `LocalizationRuntimeController` call `TaskLocalization` methods.
  Add fields to `RuntimeContext` if the controller needs more runtime data.
- If a new task-facing runtime signal is added, wire it through both controller
  and dashboard/settings as needed.
- If lifecycle or ownership changes, update `uml/03_runtime_threading.puml` and
  `uml/04_localization_task.puml`.

