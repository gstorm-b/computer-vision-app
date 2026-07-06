# Module: device (level 1)

**Purpose.** Device families and their lifecycle: cameras (`camera/`,
Basler GigE), PLC (`plc/`, Mitsubishi MC protocol), vision output
(`output_device/`, TCP/IP server/client), robots (`robot/`, Kawasaki/Nachi).
Plus `DeviceFactory`, `DeviceRegistry`, `DeviceManager`.

**Pattern.** Abstract family base + concrete subtype:
`CameraDevice`, `PlcDevice`, `VisionOutputDevice`, `RobotDevice`.

**May include.** Qt, `core/`, other `device/` headers, `calibration/`
(camera devices own their `Calibrator`; calibration never includes device
back), vendor SDKs (Pylon via env-var include paths).
**Must NOT include.** `model/`, `runtime/`, `matching/`, any UI (`form/`,
`widgets/`, `libwg/`), `app/`. Enforced by the architecture contract test.

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

**Build registration.** `src/device/device.pri` only.

**Docs.** `docs/architecture/device_type.md`.
