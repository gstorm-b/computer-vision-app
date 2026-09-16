# Module: runtime (level 2)

**Purpose.** Threaded device execution: `TaskRunner` (owns the runtime
thread set), per-device runners (`CameraRunner`, `PlcRunner`,
`VisionOutputRunner`), device command + command queue plumbing.

**May include.** Qt, `core/`, `device/`, `model/` (model and runtime are the
same level and may include each other).
**Must NOT include.** UI (`ui/`), `components/app/`.
Enforced by the architecture contract test.

**Invariants.**
- Runners are THE only supported path for cross-thread device access. Any
  new device family gets its own runner here; never let UI or model code
  call device methods across threads directly.
- Runners validate requests (e.g. `PlcRunner` rejects invalid tag writes)
  and surface failures via signals, not silent drops.
- **A runner that reports a capability must honour it.** `IDeviceRunner` declares optional
  capabilities with a safe default — today `supportsResultOutput()` (false) with
  `requestSendResult()` emitting a refusal. Overriding the report to `true` without overriding the
  action is worse than not supporting it at all: `LocalizationRuntimeController` connects to
  `resultRequestFinished` and *waits*, so a silent runner stalls the cycle instead of faulting it.
  Every default in this header therefore answers rather than does nothing, and
  `tests/architecture_contract_test` asserts that a runner claiming result output completes a send.

**Roles are capabilities, not families.** The localization task's `vision_output` role is filled by
any runner reporting `supportsResultOutput()`, over any device implementing
`vc::device::IResultOutputDevice`. Nothing on the result path downcasts to `VisionOutputRunner`,
and `requestConnect()`/`requestDisconnect()` are on `IDeviceRunner` so the recovery path can
reconnect a bound runner without knowing its type. Reintroducing a downcast on either path
re-creates a defect that fails silently: the role keeps working for the family it was written for
and quietly does nothing for every other.

> ⚠️ **A capability lives on the device, so the runner must ASK rather than answer for its family.**
> `VisionOutputRunner` can hard-code `supportsResultOutput() { return true; }` because every device
> in that family implements `IResultOutputDevice`. The PLC family is mixed — Modbus client/server
> publish results into their register map, `McProtocolDevice` does not — so `PlcRunner` resolves it
> per device with a `dynamic_cast`. Both flat answers are wrong there, in opposite and unequal ways:
>
> - Flat `false` (what `PlcRunner` inherited) refuses a device that *can* do the job. A Modbus
>   server bound to both `primary_plc` and `vision_output` faulted at setup with **"Device bound to
>   vision_output cannot output results"** even though the device implemented the interface and the
>   task passed its runner through correctly. The capability was declared on the device and only
>   ever read off the runner.
> - Flat `true` is worse: an MC PLC would be accepted for the role and then **hang** the first
>   cycle, because the controller waits on `resultRequestFinished`. A fault is recoverable; a stall
>   is not.
>
> When adding a runner for a mixed family, ask the device. When adding a device to a family whose
> runner answers flat `true`, that device now owes the interface.

**Verify.** `tests/architecture_contract_test` (runner behavior tests),
root app build.

**Build registration.** `src/runtime/runtime.pri` only. That `.pri` is consumed by
`src/src.pro`, which compiles every module **once** into the `ncr_shared` static
library that both shells link. No shell `.pro` lists module sources, so a file
added anywhere else is simply not built.
