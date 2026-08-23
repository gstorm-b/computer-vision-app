# Module: device (level 1)

**Purpose.** Device families and their lifecycle: cameras (`camera/`,
Basler GigE), PLC (`plc/`, Mitsubishi MC protocol), vision output
(`output_device/`, TCP/IP server/client), robots (`robot/`, Kawasaki/Nachi),
and hardware-free stand-ins for all of them (`virtual/`).
Plus `DeviceFactory`, `DeviceRegistry`, `DeviceManager`.

**Pattern.** Abstract family base + concrete subtype:
`CameraDevice`, `PlcDevice`, `VisionOutputDevice`, `RobotDevice`.

**`virtual/` is shipped code, not test doubles.** `VirtualCameraDevice`,
`VirtualPlcDevice` and `VirtualVisionOutputDevice` let the product be demonstrated
and the runtime exercised with no camera, PLC or listener attached. The contract
test drives *these* classes rather than its own copies — that is the point, and it
is the only thing keeping the simulated and the real paths honest about each other.
Their `connectSucceeds` / `grabSucceeds` / `sendSucceeds` flags are **features**: a
failed connect and a timed-out grab are what the recovery path exists for, and on a
device with no hardware the only way to produce one is to ask.

> ⚠️ **A virtual device must never pass for a real one in a commissioned project.**
> Once saved it is just another sub-type, and a station could be handed over
> "working" while running on nothing. Three markers answer this — the project-tree
> **VIRT** chip, the device panel's banner, and a per-runtime-start log warning —
> all driven by the single `vc::device::isVirtualDevice()` predicate so they cannot
> disagree. Add a marker, use that predicate.

> ⚠️ **The virtual camera's calibration is synthetic.** The homography is really
> fitted; the *scene* (flat, axis-aligned, at the configured mm/px) is asserted. Its
> positions are millimetres of a declared plane, not of anything measured. Setting
> `millimetresPerPixel` to 0 leaves it uncalibrated so the runtime refuses it exactly
> as it refuses a real one.

**Full reference:** [`docs/domains/virtual_devices/virtual_devices.md`](../../docs/domains/virtual_devices/virtual_devices.md)
— including the four defects these devices have already had, each of which the
contract test passed straight through.

**May include.** Qt, `core/`, other `device/` headers, `calibration/`
(camera devices own their `Calibrator`; calibration never includes device
back), vendor SDKs (Pylon via env-var include paths).
**Must NOT include.** `model/`, `runtime/`, `matching/`, any UI (`ui/`),
`app/`. Enforced by the architecture contract test.

**Invariants.**
- Every new device subtype must update: enum/string conversion, factory
  dispatch, UI dispatch (form module), persistence, and tests
  (see AGENT.md "Architecture Guardrails").
- Devices are driven cross-thread ONLY through the runtime runners
  (`CameraRunner`, `PlcRunner`, `VisionOutputRunner`) — never call device
  methods directly from another thread.
- Config types round-trip via `toJson()`/`fromJson()`; imported values are
  validated (range-checked camera numbers, capped id lengths).

**Verify.** `tests/architecture_contract_test` (factory/config round-trip
tests), `tests/vision_output_device_test`, root app build.

**Build registration.** `src/device/device.pri` only. That `.pri` is consumed by
`src/src.pro`, which compiles every module **once** into the `ncr_shared` static
library that both shells link. No shell `.pro` lists module sources, so a file
added anywhere else is simply not built.

**Docs.** `docs/architecture/device_type.md`,
`docs/domains/virtual_devices/virtual_devices.md`.

**Trap worth knowing before adding any device.** `IDevice` stores a **non-owning**
pointer to the config, and `IDevice::fromJson()` writes through it **without calling
`setDeviceConfig()`** — so a device gets no hook to react to being loaded. A concrete
device must publish its own config member (`IDevice::setDeviceConfig(&m_config)` in
the constructor) and override `fromJson()` if loading has to change anything derived.
Both of those were missed on the virtual camera and both produced silent, plausible
wrong behaviour that a config round-trip test does not catch.
