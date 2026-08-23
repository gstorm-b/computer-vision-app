# Module: runtime (level 2)

**Purpose.** Threaded device execution: `TaskRunner` (owns the runtime
thread set), per-device runners (`CameraRunner`, `PlcRunner`,
`VisionOutputRunner`), device command + command queue plumbing.

**May include.** Qt, `core/`, `device/`, `model/` (model and runtime are the
same level and may include each other).
**Must NOT include.** UI (`ui/`), `app/`.
Enforced by the architecture contract test.

**Invariants.**
- Runners are THE only supported path for cross-thread device access. Any
  new device family gets its own runner here; never let UI or model code
  call device methods across threads directly.
- Runners validate requests (e.g. `PlcRunner` rejects invalid tag writes)
  and surface failures via signals, not silent drops.

**Verify.** `tests/architecture_contract_test` (runner behavior tests),
root app build.

**Build registration.** `src/runtime/runtime.pri` only. That `.pri` is consumed by
`src/src.pro`, which compiles every module **once** into the `ncr_shared` static
library that both shells link. No shell `.pro` lists module sources, so a file
added anywhere else is simply not built.
