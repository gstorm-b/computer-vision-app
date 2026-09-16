# Phase 8 Implementation Plan — JAI Camera, MC 1C/3C Frames, Modbus Devices

**Date:** 2026-08-25
**Status:** ✅ **PHASE 8 CLOSED — 2026-09-07.** All four checkpoints (A, B, C, F) closed with
owner confirmation on real hardware. Final suites: `architecture_contract_test` **95**,
`mc_frame_test` 38, `modbus_device_test` **22**, `vision_output_device_test` 9,
`vision_tcpip_client_device_test` 9 (+1 known flake, backlog 32), `jai_camera_hardware_test` 21.
Both shells relink clean. Three items were **carried to
[`docs/backlog/later_todo_list.md`](../../backlog/later_todo_list.md)** rather than ticked —
**56** (MC 1C/3C command coverage + benchmarks), **57** (robot pick check active on the dual-role
Modbus binding), **54** (a held `bExecuteTrigger` at runtime start). See "Checkpoint: Phase 8
complete" at the end of this file for why each is a carry rather than a defect.

> Everything below this banner is the running record kept while the phase was open. It is
> **history, not current guidance** — read `docs/domains/` and `uml/` for how the code behaves now.

---

**Status while the phase was open:** **Phase A in progress.** **Checkpoint A-1 ✅ owner-confirmed (2026-08-25).**
**A1** serial transport, **A2** 1C/3C contexts, **A3** `Frame1C`, **A4** `Frame3C`,
**A7** `tests/mc_frame_test` — all ✅ DONE (2026-08-25). A7 was brought forward to sit with the
codecs it verifies. Umbrella build clean; contract test **72 passed / 0 failed** (70 → 72);
`mc_frame_test` **38 passed / 0 failed**; display-name total 69 → 87. Every new test was proven
to fail against an injected violation before being trusted.
**A5** (device dispatch), **A6** (widget serial card + wizard) and **A8**
(`tools/mc_protocol_bench`) ✅ DONE (2026-08-25); the translation sweep added **45** new source
texts with nothing lost.

**Phase A is code-complete. Everything that remains needs a PLC behind the serial port** — the
owner-run halves of A5, A6 and A8, and Checkpoint A itself; the owner is running Checkpoint A now.

**Phase B code-complete (2026-08-26).** **Checkpoint B-1 ✅ owner-confirmed** — a full localization
cycle ran on an existing project. **B1** (result output as a runner capability), **B2** (register-map
core + result contract), **B3** (`ModbusTcpClientDevice`), **B4** (`ModbusTcpServerDevice`),
**B5** (registry/factory/wizard/enums) and **B6** (`ModbusDeviceWidget`) all ✅ DONE.
Umbrella build clean; contract test **78 passed / 0 failed** (72 → 78); new
`tests/modbus_device_test` **13 passed / 0 failed**; `mc_frame_test` 38/0;
`vision_output_device_test` 8/0; `vision_tcpip_client_device_test` 9/0; translation sweep added
**46** new source texts with nothing lost. Display-name total 87 → 105.

> ⚠️ **One plan gate was passed without the owner** — ✅ **resolved: reviewed and APPROVED
> 2026-09-07.** B2 says *"Owner reviews the layout doc before B3 starts"*. It was not reviewed at
> the time; the owner's instruction was to run the rest of Phase B, and stopping with only a
> document to show would have delivered nothing.
> **[modbus_result_contract.md](../../domains/task_localization/modbus_result_contract.md) is now
> approved as shipped**, scaled int32 (×100), high word first. The layout was cheap to
> change while this gate was open; it no longer is. **The encoder is a frozen contract** — a
> commissioned robot program depends on that word order.

**Correction applied 2026-08-26, on an owner question.** The owner asked which side may write
discrete inputs and input registers. The answer is the **server** — and our server refused to,
because it asked `isWritable()`, a *master-perspective* predicate, as if it were a question about
ownership. Fixed: the predicate is split into `isMasterWritable()` / `isServerWritable()`, the
server writes all four areas, and `ModbusTcpServerCfg::resultInInputRegisters` lets the result
block be published into input registers — the layout a master has no function code to overwrite.
`modbus_device_test` 13 → **16 passed / 0 failed**; contract test still 78/0; display names
105 → 106; translation sweep +3, nothing lost. Proven to fail against the old behaviour before
being trusted.

**Diagnostic logging added 2026-08-26, on an owner report.** The owner paired a server and a
client device on one task and hit a Modbus exception response whose message said only
*"Modbus Exception Response."* — no code, no address range, no clue which of forty requests a
second it was. The Modbus devices now log: the client's **poll plan** (every range it will ask
for), the server's **register space** (every range it will answer, plus areas absent entirely),
every master connect/disconnect, transport state transitions, and — on any failure — the request,
the reply and the **named exception code with its plain-language consequence**. A `Protocol Trace`
flag adds the full per-frame stream; it is off by default (a 100 ms poll is forty lines a second)
but **failures dump in full regardless**, and it is the one setting both devices accept while
connected. Two Qt traps were found doing it, both recorded in `src/device/AGENTS.md`:
`installConnectionObserver()` takes ownership (our own owning handle was a double free that
surfaced as heap corruption), and `QModbusPdu::isValid()` is false for an exception response, so
gating on it silently discarded the exception code. `modbus_device_test` 16 → **18 passed**,
including a deliberate mismatched-span reproduction of the owner's failure.

**What remains in Phase B is hardware:** Checkpoint B itself — a task bound to one Modbus device
for *both* the PLC role and the vision-output role completing a cycle against a real slave/master,
with the robot pick check confirmed still active on that configuration. Phase C not started.
**Source request:** [../request/phase_8_request.md](../request/phase_8_request.md)

> **Location note.** Same exception as Phases 5–7: this file lives under `docs/history/`,
> which `AGENT.md` declares traceability-only, but it is the CURRENT plan for Phase 8. The
> standing recommendation to move `docs/history/plan/` to `docs/plan/` and link it from
> `docs/README.md` is now four phases old.

---

## Overview

The request has four threads. Three are new device sub-types; one is a protocol extension of a
device that already ships.

| # | Request | Reduces to |
|---|---|---|
| 1 | Camera device `Jai_Gige` + its widget | A new `CameraDevice` sub-type on the JAI SDK, following the Basler GigE shape |
| 2 | MC frames 1C and 3C + contexts | Two new frame codecs **plus a serial transport the project does not have** |
| 3 | Unit tests for every command + a hardware benchmark for 3E/1C/3C | One hardware-free codec test suite + one parameterised tool the owner runs against a real PLC |
| 4 | Modbus client and server, PLC **and** vision-output capable, + widgets | Two device sub-types **plus a change to how the runtime assigns roles** |

**The load-bearing observation:** two of the four threads are not device work at all.

- 1C/3C are *computer-link* protocols. Both reference implementations drive them over a serial
  port ([`reference source/1C_frame/fx3communicator.h`](../../../reference%20source/1C_frame/fx3communicator.h)
  uses `QSerialPort`; the Python reference splits interface `"c"` = 1C/3C from `"e"` = 1E/3E).
  The project has exactly one transport — `McEthernetTcpPort`. **The serial transport is the
  prerequisite, not the codecs.**
- "Modbus is both a PLC device and an output device" is impossible in the current model:
  `IDevice::deviceType()` returns one family, `VisionOutputRunner` is
  `DeviceRunner<VisionOutputDevice>`, `PlcRunner` is `DeviceRunner<PlcDevice>`, and
  `TaskRunner::m_runners` holds **one runner per device id**. Making one device serve both roles
  is a change to the role model, and it must land before the Modbus devices are written, not
  after.

Both of those are why Phase A is the MC thread and why the Modbus phase opens with a
runtime-model task rather than a device.

---

## Decisions Taken With The Project Owner (2026-08-25)

| # | Question | Decision |
|---|---|---|
| **Q1** | Modbus serving two roles | **One device, capability-based role.** The `vision_output` role stops meaning "a device of the VisionOutput family" and starts meaning "a device whose runner supports result output". |
| **Q2** | 1C/3C transport | **Serial (`QSerialPort`)**, matching both reference implementations. No TCP path for the C-frames in this phase. |
| **Q3** | JAI feature scope | **Parity with the Basler GigE camera as actually implemented**: connect by IP, exposure/gain/frame-rate through the node map, single-shot grab, IO lines + auto backlight, `cv::Mat` conversion, removal detection. Continuous and software-trigger grabs stay unimplemented, exactly as on Basler. |
| **Q4** | Order | **MC frames → Modbus → JAI.** The MC thread carries the most risk and needs the owner's real PLC for acceptance, so it opens first to leave time for hardware testing. |

**Q1 has a consequence worth stating before any code is written.**
[task_localization.cpp:692-697](../../../src/model/task_localization.cpp#L692-L697) reads the
advisory robot kinematic-check settings out of the vision-output device's config by
`dynamic_cast<VisionOutputDeviceCfg *>`. A Modbus device bound to the `vision_output` role has no
`VisionOutputDeviceCfg`, so that cast returns null and **`robotCheckConfig` silently falls back to
its default** — the pick check quietly stops using the commissioned settings. This is exactly the
failure shape Phase 7 kept producing: builds clean, runs, wrong. Task **B1** owns it explicitly.

---

## Current State (verified against source, 2026-08-25)

| Entity | Location | Today |
|---|---|---|
| Camera family | [src/device/camera/camera_device.h](../../../src/device/camera/camera_device.h) | `CameraType` = `Realsense`, `BaslerGigE`, `BaslerUSB`, `VirtualCamera`. Only `BaslerGigE` and `VirtualCamera` are implemented and registered |
| MC frame codecs | [src/device/plc/mc_fame_3e.h](../../../src/device/plc/mc_fame_3e.h) | `Frame3E` only. `MCFrameAbstract` already abstracts build/parse per frame variant |
| MC contexts | [mc_context_factory.h:22-35](../../../src/device/plc/mc_context_factory.h#L22-L35) | `Frame_1C`, `Frame_1E`, `Frame_3C` all `return nullptr`. Only `Context_Mc3E` exists |
| MC transports | [mc_msg_interface.h](../../../src/device/plc/mc_msg_interface.h), `mc_msg_tcp_client.h` | `McMsgInterface` abstract + one implementation, `McEthernetTcpPort`. `McMsgItfType` already declares `SerialPort` — **the enum value exists, nothing implements it** |
| Frame/transport dispatch | [mc_protocol_device.cpp:344-363](../../../src/device/plc/mc_protocol_device.cpp#L344-L363) | Two `switch` statements, each with one arm and a `default: return false` |
| PLC families | [plc_device.h:29-35](../../../src/device/plc/plc_device.h#L29-L35) | `MitsubishiMc`, `VirtualPlc` |
| Vision-output families | [vision_output_device.h](../../../src/device/output_device/vision_output_device.h) | TCP/IP server, TCP/IP client, virtual |
| Role → runner | [task_runner.cpp:209-233](../../../src/runtime/task_runner.cpp#L209-L233), [task_runner.h:131](../../../src/runtime/task_runner.h#L131) | `createRunner()` switches on `deviceType()`; `m_runners` is **keyed by device id**, one runner per device |
| Result send path | [vision_output_runner.h:48-59](../../../src/runtime/vision_output_runner.h#L48-L59), [localization_runtime_controller.cpp:1437-1451](../../../src/model/localization_runtime_controller.cpp#L1437-L1451) | Controller holds a `QPointer<VisionOutputRunner>`, calls `requestSendResult()`, listens for `resultRequestFinished` |
| Runner capability precedent | [idevice_runner.h:73-81](../../../src/runtime/idevice_runner.h#L73-L81) | `IDeviceRunner::submitCommand()` already models an optional capability with a "not supported" default. **B1 follows this existing pattern rather than inventing one** |
| Device widget dispatch | [device_widget_factory.cpp:41-89](../../../src/ui/forms/device_widget_factory.cpp#L41-L89) | Switches on family, then rejects any camera that is not `BaslerGigE` and any PLC that is not `MitsubishiMc` by equality |
| Registry | [device_registry.cpp:196-256](../../../src/device/device_registry.cpp#L196-L256) | Nine entries. **Ordering is load-bearing** — virtual entries go last within a family (comment at line 203) |
| Display-name markers | [architecture_contract_test/main.cpp:2704-2831](../../../tests/architecture_contract_test/main.cpp#L2704-L2831) | `QCOMPARE(totalNames, 69)`. **Every new config class with a display name breaks this test until its `kDisplayNameSources[]` is added and the count is raised** |
| Tests | `tests/` | `architecture_contract_test` (70 cases), `calibration_test`, `vision_output_device_test`, `vision_tcpip_client_device_test`, `nachi_client`. No MC test exists |
| Tools | `tools/vision_tcpip_output_device/` | One GUI counterpart tool. The precedent for a hardware-facing tool that is not part of the product |

### Environment facts checked on this machine

| Dependency | Evidence | Verdict |
|---|---|---|
| Qt Modbus | `Qt6SerialBus.lib` + `qmodbustcpclient.h` / `qmodbustcpserver.h` present in `C:\Qt\6.8.3\msvc2022_64` | Available — `QT += serialbus`. No third-party Modbus stack needed |
| Qt Serial Port | `include/QtSerialPort` present | Available — `QT += serialport` |
| JAI SDK (C API) | `jai_factory.h`, x64 `Jai_Factory.lib`, env `JAI_SDK_INCLUDE` / `JAI_SDK_LIB_64` / `JAI_SDK_BIN_64` | Installed and reachable through environment variables — but **not necessarily the SDK the camera is driven with**; see the correction at the head of Phase C |
| Pleora eBUS SDK | `reference source/Jai_ebus_docs` + eBUS samples under `reference source/JaiCamTest` (`PvDevice`, `PvStream`, `SoftDeviceGEV*`, …) | The SDK the supplied reference material is written against. C1 settles which one is used before any wiring |
| Serial port hardware | `SerialPort.GetPortNames()` returns nothing on this machine | **None present.** A1–A4 and A7 are hardware-free and unaffected; A5/A6/A8 acceptance needs a real port or a virtual pair |

### Request-vs-repository discrepancies

The request names `reference_source/1C` and `reference_source/3C`. The repository actually has
**`reference source/`** (with a space) containing **`1C_frame/`** (C++/Qt, serial, FX3) and
**`3C_frame/`** (Python, containing *both* `mc_frame_1c.py` and `mc_frame_3c.py`). No content is
missing; only the paths in the request differ. This plan uses the real paths.

---

## Architecture Decisions

1. **The serial transport is a peer of the TCP transport, not a 1C/3C detail.** It implements
   `McMsgInterface` and is selected by `McMsgItfType::SerialPort` — the enum value that has been
   sitting unimplemented since the interface was written. A future 1E-over-serial or
   3E-over-serial setup then costs nothing.
2. **1C and 3C get their own contexts, not flags on a shared one.** `Context_Mc3E` carries 3E's
   frame header as concrete fields; the C-frames carry station/PC number, frame format and
   sum-check instead. A single context with both sets would put every field on every UI.
3. **Result output becomes a runner capability, mirroring `submitCommand()`.**
   `IDeviceRunner` gains `supportsResultOutput()`, `requestSendResult()` and the
   `resultRequestFinished` signal, defaulting to "not supported". `VisionOutputRunner` moves its
   existing implementation up into that override; `PlcRunner` implements it when its device
   implements `IResultOutputDevice`. Nothing else in the role model changes: the binding is still
   a device id, the runner is still one per device.
4. **One Modbus core, two devices.** Register map, addressing, encoding and the result layout are
   one implementation shared by the client (we are master) and the server (we are slave). Only
   the Qt object underneath (`QModbusTcpClient` / `QModbusTcpServer`) and the connect/listen
   lifecycle differ.
5. **The Modbus result layout is a wire contract and gets written down before it is coded** —
   same standing as the TCP/IP result frame documented on `VisionOutputRequest`. Once a customer
   PLC reads those registers, the layout cannot move.
6. **No backwards-compatibility shims** (`AGENT.md`). New JSON tokens are added; no existing
   token changes meaning.

### Rules that apply to every task in this phase

Stated once here rather than repeated in fifteen acceptance lists:

- **A new device sub-type must update all six sites** (`AGENT.md` → Architecture Guardrails):
  enum + string conversion, `DeviceRegistry` entry, `DeviceFactory` dispatch,
  `DeviceWidgetFactory` dispatch, JSON persistence, tests.
- **A new config class with display names must carry `kDisplayNameSources[]`** with the context
  spelled exactly as `staticMetaObject.className()`, and
  `architecture_contract_test`'s `QCOMPARE(totalNames, 69)` must be raised in the same edit.
  A missed marker is a label that works in English and can never be translated, with no build
  error — see `docs/rules/build_and_verification.md` → "Strings lupdate cannot see on its own".
- **A new enum shown in a combo needs an `enum_keys_*[]` marker table** whose context matches
  `QMetaEnum::scope()`, not the class name.
- **New files are registered in the module `.pri` only**, never in a shell `.pro`.
- **New `.qrc` resources stay at shell level** in `qmake/app_common.pri` — a `.qrc` inside
  `ncr_shared` is dropped by the linker with no build error.
- **Machine paths come from environment variables.** The JAI SDK is wired the way Pylon is.

---

## Phase A — MC 1C/3C Frames, Serial Transport, Tests And Benchmark

### Task A1: Serial message interface for the MC protocol

**Description:** Implement `McMsgSerialCfg` (port name, baud rate, data bits, parity, stop bits,
flow control, plus the inherited timeouts) and `McMsgSerialPort` (`McMsgInterface` over
`QSerialPort`), and enable `QT += serialport`. This is the missing half of
`McMsgItfType::SerialPort`, which the enum has always declared and nothing has ever implemented.

**Acceptance criteria:**
- [x] `McMsgSerialPort::type()` returns `McMsgItfType::SerialPort`; `SetConfig()` rejects a config
      of any other type and returns false rather than casting blindly.
- [x] `ioDevice()` returns the `QSerialPort` so `McProtocolDevice` can hook `readyRead` unchanged.
- [x] `ConnectToPort()` on an absent/busy port returns `ConnectFail` and populates
      `GetErrorDescription()` — never throws, never leaves a half-open port.
- [x] `DestroyMsgPort()` closes and deletes the port synchronously, matching `McEthernetTcpPort`'s
      contract (it is called from the device's own thread during teardown).
- [x] `McMsgSerialCfg` round-trips through `toJson()`/`fromJson()` and carries
      `kDisplayNameSources[]`.

**Verification:**
- [x] Umbrella build: `qmake ncr_picking_all.pro && nmake /nologo` — clean, 98 s incremental
- [x] `tests/architecture_contract_test` — **71 passed / 0 failed**, display-name total 69 → 75
- [x] The new case was **proven to fail against an injected violation** before being trusted:
      making `ConnectToPort()` return `Connected` for an unconfigured port turns the suite to
      70 passed / 1 failed. Reverted, re-run, green again.
- [ ] Manual: available port names enumerate via `QSerialPortInfo` — **deferred**: this machine
      currently reports no COM ports at all, so there is nothing to enumerate. See the note under
      Checkpoint A-1.

**Dependencies:** None
**Files touched:** `src/device/plc/mc_msg_serial_port.h` (new, header-only — the TCP transport
next to it is header-only too), `src/device/plc/mc_define.h` (four serial enums + their marker
entries), `src/device/device.pri`, `qmake/common_deps.pri`,
`tests/architecture_contract_test/{main.cpp,architecture_contract_test.pro}`
**Estimated scope:** M

**Decisions taken while implementing:**
- **The line settings are our own enums in `vc::device::mc`, not `QSerialPort`'s.** Reusing Qt's
  would have put the translated combo labels under the `QSerialPort` context and forced every
  header that includes `mc_define.h` to pull in QtSerialPort. The conversion to Qt's values lives
  in the four `toQt*()` helpers inside the transport — the one place that talks to the port.
- **Enums persist as key names, not integers.** A numeric round-trip would survive a reordered
  enumerator and silently reconfigure a commissioned line on the next release; the contract test
  asserts the key-name form specifically.
- **An unrecognized enum key keeps the current value** rather than falling back to the zero
  enumerator, so a partially written document cannot quietly change parity.
- **`ConnectToPort()` closes any open handle first.** A serial port is exclusive, so a leaked
  handle from a failed attempt makes every later attempt fail with "access denied" — which reads
  like a cable fault and is not one.
- **`clearBuffer()` uses `QSerialPort::clear()`, not `readAll()`.** After a response timeout the
  unread tail of the stale frame sits in the driver's buffer, where `readAll()` does not reach it,
  and it would be parsed as the head of the next response. The TCP transport does not have this
  problem, which is why the two differ here.

---

### Task A2: `Context_Mc1C` and `Context_Mc3C`

**Description:** Add the two C-frame contexts and wire them into `Factory::contextFactory()`,
replacing the two `return nullptr` arms. Parameters follow the references: **1C** = frame format
(1–4, default 4) + sum-check on/off + station number; **3C** = the same plus PLC series
(A/Q/L/iQ-R) and the access-route numbers. Both default their message-interface config to
`McMsgSerialCfg`, the way `Context_Mc3E` defaults to `McMsgEthernetTcpCfg`.

**Acceptance criteria:**
- [x] `frameType()` / `msgIntefaceType()` return `Frame_1C`/`Frame_3C` and `SerialPort`.
- [x] `clone()` deep-copies, **including** the message-interface config.
      ⚠️ `Context_Mc3E::clone()` is *not* the reference for this: it is `new Context_Mc3E(*this)`,
      and the implicit copy shares the `shared_ptr<McMsgItfConfig>` with the original. Found
      during A1 and recorded as **backlog #46** together with the wider aliasing it belongs to
      (`McProtocolDevice::mcProtocolConfig()` returns a "copy" that shares the live context).
      The new contexts deep-copy; that makes 3E the odd one out until #46 is fixed, which is
      deliberate and recorded so it is not read as an oversight in the new code.
- [x] `toJson()`/`fromJson()` round-trip every field including the nested `MsgConfig`.
      ~~and a document written by one context is rejected by the other~~ — **this criterion was
      wrong about the design and is withdrawn.** A context's JSON carries no frame tag; the frame
      type lives one level up in `McProtocolConfig` (`DEVICE_JSK_MC_FRAME`), which is what picks
      the context class before handing it the nested object. Adding a per-context rejection would
      duplicate a check that already exists where the decision is actually made. The test asserts
      the real path instead: a full `McProtocolConfig` round trip comes back as a `Context_Mc3C`
      with its station number intact.
- [x] `contextFactory(Frame_1C)` and `(Frame_3C)` return non-null; the `Frame_1E` arm is left
      returning nullptr and is **not** silently "fixed" — it is out of scope.
- [x] Both classes carry `kDisplayNameSources[]`; new enums carry marker entries in
      `enum_keys_mc_defines[]`.

**Verification:**
- [x] Umbrella build clean
- [x] New contract-test case `test_computer_link_contexts_round_trip_and_stay_distinct`
- [x] `tests/architecture_contract_test` — **72 passed / 0 failed**, display-name total 75 → 87
- [x] Proven to fail against an injected violation: dropping the deep copy from
      `Context_Mc1C`'s copy constructor (i.e. making it behave exactly like 3E's) turns the suite
      to 71 passed / 1 failed on `c1Clone->msgConfig() != c1.msgConfig()`. Reverted, green again.

**Dependencies:** A1
**Files touched:** `src/device/plc/mc_context_1c.h` (new), `mc_context_3c.h` (new),
`mc_context_factory.h`, `mc_define.h`, `src/device/device.pri`,
`tests/architecture_contract_test/{main.cpp,architecture_contract_test.pro}`
**Estimated scope:** M

**Decisions taken while implementing:**
- **The context fields are exposed as `Q_PROPERTY` via the `G_PROPERTY_*` macros**, unlike
  `Context_Mc3E`, whose 3E header fields are plain members no property browser can see. That was
  defensible for 3E — its fields are effectively fixed (0/FF/03FF/00) — but a 1C station number
  and a 3C access route are commissioning parameters that have to be editable. This also means
  the property browser picks them up with no per-frame UI code, which removes most of A6's work
  for the context half.
- **`McFrameFormat` declares only formats 1 and 4, and `McPlcSeries` only Q/L/iQ-R.** The
  references implement format 4 (format 1 is the same frame without the CR+LF terminator), and
  `mc_frame_3c.py` explicitly rejects the A series. Declaring the others would put options in a
  combo that the codec has to refuse at connect time.
- **Both contexts default to `McDataCode::Ascii`** and ignore the factory's `data_code` argument.
  Computer-link frames are ASCII on the wire; a Binary context here would build frames the PLC
  cannot read. Asserted by the test.

> **Consequence for A4:** the plan's A4 criterion "byte-exact frames … for the default format and
> at least one non-default format" now means **format 4 and format 1**, which differ only by the
> terminator. Formats 2/3/5 have no reference implementation here and are not in scope.

---

### Checkpoint A-1: transport and contexts — ✅ CLOSED, owner-confirmed 2026-08-25
- [x] Umbrella build clean, contract test green (72/0)
- [x] A serial MC device can be created, configured and persisted — it cannot talk yet
- [x] Reviewed with the project owner before the codecs were written

> **Hardware gap found at A1: this machine had no COM port.** `SerialPort.GetPortNames()`
> returned nothing, so there was no port to open and no way to prove a frame ever left the
> machine. A1–A4 and A7 were unaffected — hardware-free by design — but **A5, A6 and A8
> acceptance all need a port**.
>
> **Resolved by the owner (2026-08-25): a USB serial port is being attached**, not yet wired
> through to a real PLC. That is enough for A6 (the port must appear in the picker) and for the
> transport half of A5 (the port opens, bytes leave). It is **not** enough for the half that
> matters most: no PLC means no response, so 1C and 3C stay unverified on the wire until A8 runs
> against the real C24 port.

---

### Task A3: `Frame1C` codec

**Description:** Implement `MCFrameAbstract` for the 1C computer-link frame: build BR/BW/WR/WW
requests and parse ACK/NAK responses, with STX/ETX framing, station and PC number blocks, and the
sum check. Command set and framing come from
[`reference source/1C_frame/fx3communicator.cpp`](../../../reference%20source/1C_frame/fx3communicator.cpp)
and `mc_frame_1c.py`; the control characters and command codes are **already** defined in
[mc_define.h:12-47](../../../src/device/plc/mc_define.h#L12-L47) and must be used rather than
re-declared.

**Acceptance criteria:**
- [x] `makeSendFrame()` produces byte-exact frames for read-bit, write-bit, read-word and
      write-word against reference frames **derived from** the 1C reference implementation.
      ⚠️ Wording corrected: the plan said "captured from". Nothing was captured — no traffic from
      a running 1C link exists here. The expectations encode the reference's construction rules,
      which is a weaker claim and is stated as such at the head of `tests/mc_frame_test/main.cpp`.
- [x] `parseReceiveFrame()` returns `WaitingReceive` on a partial frame and does not consume it,
      `ResponseOk` on ACK, `ResponseError` on NAK **with the NAK code mapped to a message**, and
      `ResponseInvalid` on a malformed frame. Partial-frame behaviour is asserted at **every**
      truncation point of a complete frame, not at one chosen cut.
- [x] Sum check is verified on receive and generated on send when the context enables it; a bad
      sum is `ResponseInvalid`, never silently accepted.
- [x] A null request or null context returns `ObjectError` — no dereference. A context of the
      wrong frame type is refused the same way, which the plan did not ask for and which matters
      more: it is the reachable mistake, since both are non-null `McContext*`.

**Verification:**
- [x] `tests/mc_frame_test` — **38 passed / 0 failed** (see A7)
- [x] Umbrella build clean

**Dependencies:** A2
**Files touched:** `src/device/plc/mc_frame_1c.{h,cpp}` (new),
`src/device/plc/mc_ascii_utils.h` (new, shared with 3C), `src/device/device.pri`
**Estimated scope:** M

**Beyond the plan, and why:**
- **Leading bytes before the control code are skipped, not parsed as a header.** After a
  response timeout the device retries, and the abandoned response's tail is still in the buffer.
  Reading that tail as a frame header is how a link that has just recovered starts publishing
  values that were never read — worse than staying disconnected, because the UI looks healthy.
- **The station and PLC numbers in a response are checked against the context.** A multi-drop
  line carries other stations' answers; accepting one writes another PLC's registers into this
  device's map.
- **The reference's `Fx3Response::ParseResponse` has a missing `break`** — the STX arm falls
  through into the ACK arm, so a truncated STX frame with no ETX is reported valid. Not
  reproduced here; a frame without ETX is `WaitingReceive` until it arrives.

---

### Task A4: `Frame3C` codec

**Description:** The same for the 3C frame: access route (station/network/PC/self-station),
frame ID number, format 1–5 (default 4), optional end code, PLC-series-dependent device coding.
Reference: [`reference source/3C_frame/mc_frame_3c.py`](../../../reference%20source/3C_frame/mc_frame_3c.py).

**Acceptance criteria:**
- [x] Byte-exact frames for all four commands, for format 4 and format 1 (see A2's note: those
      are the two formats that exist here), and for **both device-header widths** — Q/L `M*`
      with a 6-digit address and iQ-R `M***` with an 8-digit address, each with its own
      sub-command. The series widths were the likeliest thing to get silently wrong, since a
      wrong-width frame is still a well-formed frame.
- [x] The access-route block is rejected on receive when it does not match what was sent — a
      response addressed to another station is `ResponseInvalid`, not data. The frame ID is
      checked the same way.
- [x] Partial-frame, NAK and sum-check behaviour identical in spirit to A3.
- [x] A bit read of more than 8 points is issued as a word read and unpacked on receive,
      matching `Frame3E`, so the polling loop's request shapes do not change with frame type.

**Verification:**
- [x] `tests/mc_frame_test` — **38 passed / 0 failed** (see A7)
- [x] Umbrella build clean

**Dependencies:** A2
**Files touched:** `src/device/plc/mc_frame_3c.{h,cpp}` (new), `src/device/device.pri`
**Estimated scope:** M

**Two places this codec deliberately diverges from the Python reference.** Both are documented
at the function that diverges, so a later reader comparing the two files is not left guessing:

1. **Word reads decode to one 16-bit value per device**, not to two byte values. The reference
   splits each word's four hex characters into two bytes and appends both, which suits its own
   byte-oriented storage but is not what a register map holds. The 1C reference parses the same
   field as a single hexadecimal number, and so does this codec.
2. **Word-packed bit reads parse the four characters as hexadecimal.** The reference reads each
   character with a decimal conversion, which cannot represent A–F — so any bit pattern with a
   nibble above 9 would fail there. Each group of four characters is parsed as one 16-bit word
   and unpacked least-significant bit first, matching `Frame3E::parse_read_bit_from_word`.

Neither divergence is provable without hardware. Both are flagged for **A8** as the first things
to check against the real PLC.

---

### Task A5: Device dispatch for the new frames and transport

**Description:** Extend the two `switch` statements in `McProtocolDevice::initialize_mc_device()`
so `Frame_1C`/`Frame_3C` build their codec and `SerialPort` builds `McMsgSerialPort`. Nothing else
in the device changes: polling, retry, timeout and the device map are frame-agnostic by design.

**Acceptance criteria:**
- [x] Selecting 1C or 3C with a serial interface builds the right codec and transport, through
      the **same** paths as 3E — including the `ConnectFailed` publish on initialization failure
      that [mc_protocol_device.cpp:98-106](../../../src/device/plc/mc_protocol_device.cpp#L98-L106)
      exists to guarantee.
- [x] An unsupported frame/transport pair still returns false and publishes `ConnectFailed`
      rather than wedging `PlcRunner`'s busy flag, and now **logs which** frame or interface was
      unsupported instead of returning false silently.
- [x] Address-range optimisation, comm-active heartbeat and change detection are untouched: they
      never referenced the frame or the transport.

**Verification:**
- [x] Umbrella build clean; contract test **72 passed / 0 failed**
- [ ] Owner-run: connect to the real PLC on 1C and on 3C, watch M/D values move in the monitor
      — **blocked on hardware**, the USB port has no PLC behind it yet

**Dependencies:** A3, A4
**Files touched:** `src/device/plc/mc_protocol_device.cpp`,
`tests/architecture_contract_test/architecture_contract_test.pro` (the contract test lists src
sources individually, so it needed the two new codec `.cpp` files — the sync hazard recorded in
`src/core/AGENTS.md`, which caught this at link time)
**Estimated scope:** S

**Two defects fixed in passing, both in the function being edited:**
- **`initialize_mc_device()` dereferenced `m_config.context()` with no null check.** A project
  saved with a frame type this build has no factory arm for (Frame_1E today, or a corrupted
  frame token) leaves the context null, and `McProtocolConfig::fromJson()`'s `false` return is
  discarded by `IDevice::fromJson()`. Loading such a project and pressing Connect **crashed**.
  It now refuses to connect and says why. Reachability went up with this task, not down: 1E is
  now the only unimplemented frame sitting next to three that work.
- **`setDeviceMap()` was called inside the 3E arm of the frame switch**, so every new frame arm
  had to remember it. Hoisted above the switch — it is frame-independent, and a codec whose
  context has no device map parses a response and has nowhere to put the values.
- Also: the frame codec is now released when the transport arm fails, so a half-built stack
  cannot survive into the next connect attempt.

---

### Task A6: Mitsubishi widget — serial connection card

**Description:** The widget's connection card is hard-wired to IP/port and merely *disables*
those fields when the interface is not Ethernet
([mitsubishi_mc_device_widget.h:96-105](../../../src/ui/forms/plc/mitsubishi_mc_device_widget.h#L96-L105)).
Add a serial card — port (populated from `QSerialPortInfo`), baud, data bits, parity, stop bits,
flow control — and show whichever card matches the configured interface.

**Acceptance criteria:**
- [x] ~~Switching frame type in the property browser switches the visible card~~ — **the
      criterion was wrong.** `McContext::frameType` is declared `Q_PROPERTY(... CONSTANT)`, so
      the browser renders it read-only and the frame cannot be changed on an existing device at
      all. It is chosen once, in the Add Device wizard. The card therefore reflects the
      configured transport; it does not switch under the user's hands. What the criterion was
      really protecting against — stale values from the other transport — is handled by showing
      one card and **hiding** the other rather than disabling it.
- [x] Every new label is translatable and reaches the `.ts` (structure in `.ui`, behaviour in
      `.cpp`, styling in `.qss`, per the UI rules).
- [ ] Edits persist through save/reload of the project — **owner-run**, below.

**Verification:**
- [x] Umbrella build clean; contract test **72 passed / 0 failed**
- [x] `scripts/update_translations.ps1`: **45 new source texts**, `finished` 863 → 863 and
      `vanished` 29 → 29, so nothing existing was lost. Spot-checked that the card labels
      (`SERIAL PORT`, `BAUD RATE`, …), the context display names (`Station No.`,
      `Message wait time`, `Self-station No.`, …) and the enum keys (`Parity_Even`,
      `StopBits_One`, `Format_4`, `PlcSeries_Q`, …) are all present.
- [ ] Owner-run: configure a 3C serial device from a clean project and reopen it

**Dependencies:** A5
**Files touched:** `src/ui/forms/plc/mitsubishi_mc_device_widget.{h,cpp,ui}`,
`src/ui/forms/add_device_wizard.cpp`,
`resrc/styles/mitsubishi_mc_device_widget_{dark,light}.qss`,
`app/translations/ncr_picking_ja_JP.ts`
**Estimated scope:** M

**The blocker this task actually uncovered: 1C and 3C were unreachable from the UI.**
The Add Device wizard's frame combo was populated with a single hard-coded `Frame_3E`
([add_device_wizard.cpp:50](../../../src/ui/forms/add_device_wizard.cpp#L50)), and the frame is
`CONSTANT` afterwards — so nothing an operator could do would ever produce a 1C or 3C device.
Every task from A1 to A5 would have been dead code. The combo now offers the three frames a
codec exists for, with 3E still first (the registry-ordering rule: what you get by not touching
the combo). `Frame_1E` is deliberately absent — it has no context factory arm, so choosing it
would create a device that cannot connect.

**Also:** the data-code row is now hidden for 1C/3C. Those frames are ASCII on the wire and
their contexts force it, so a "Binary" option there was a control that silently did nothing —
the exact trap the comment already sitting above that code warns about.

**What the serial card adds beyond the property browser.** After A1 the browser already renders
every `McMsgSerialCfg` field, so the card is not what makes the settings editable. It earns its
place on one point: the **port picker**, filled from `QSerialPortInfo`. A free-text port name is
a typo away from an unopenable port, and the operator has no other way to see what this machine
actually has. The combo stays editable, and a configured port that is not currently attached is
kept in the list — dropping it would silently rewrite a commissioned project to whatever port
happened to be first.

The line-setting combos are filled from the gadget's own `QMetaEnum` through
`vc::gadget_meta::enumKeyNames()`, not from a second hand-written list of the same keys — that
duplication, and translating those keys in the wrong context, is precisely what Phase 7 / E6
spent a task removing.

---

### Task A7: `tests/mc_frame_test` — hardware-free command coverage

**Description:** A QtTest project covering **every command in both directions for 3E, 1C and 3C**:
build a request, compare the bytes against a reference frame; feed a reference response, compare
the parsed values. Plus the failure modes that matter operationally — partial frames, NAK/error
end codes, bad sum check, wrong access route, oversized amount.

> **Brought forward.** A7 was written and run **with A3/A4 rather than after A5**, because A3's
> and A4's own acceptance criteria are stated in terms of these tests. Two new codecs with no
> byte-level coverage is precisely the Phase 7 defect shape: they compile, they link, and the UI
> looks completely normal.

**Acceptance criteria:**
- [x] Read-bit, write-bit, read-word, write-word covered for all three frames — **38 cases**,
      each against a byte-exact expectation.
- [x] `Frame3E` is included even though it ships today. Its expectations were hand-derived from
      the frame layout and the context defaults **before** being run, and matched on the first
      execution — which is what makes it a usable control: it shows the harness drives a codec the
      way `McProtocolDevice` does.
- [x] Every failure mode has a case, and each asserts the *specific* `FrameReturnCode`.
- [x] Runs headless with `QT_QPA_PLATFORM=minimal`; no socket, no serial port, no PLC (7 ms).

**Verification:**
- [x] `nmake /nologo -f Makefile.Release` in `tests/mc_frame_test/build/msvc_release`
- [x] **38 passed / 0 failed**, exit code 0
- [x] Proven non-vacuous: dropping ETX from the summed region in `Frame1C::locateFrame` — a
      one-character off-by-one no compiler and no build would notice — turns the suite to
      37 passed / 1 failed. Reverted, green again.

**Dependencies:** A3, A4
**Files touched:** `tests/mc_frame_test/mc_frame_test.pro` (new),
`tests/mc_frame_test/main.cpp` (new)
**Estimated scope:** M

> ⚠️ **What these 38 cases do NOT prove.** Every 1C and 3C expectation is *derived from the
> reference implementations*, not captured from a PLC. They prove "we build what the reference
> builds, and read back what the reference would have written". If a reading of a reference is
> wrong, the codec and the test are wrong **together** and both stay green. The 3E cases are a
> regression baseline, so they can only catch a change to a codec that already works.
> **Only A8 against the real C24 port closes this**, and until it has run, 1C and 3C are
> unverified on the wire. The same warning heads `tests/mc_frame_test/main.cpp`, where someone
> looking at a green run will actually see it.

---

### Task A8: `tools/mc_protocol_bench` — real-PLC benchmark

**Description:** A standalone tool, outside the product, that the owner points at a real PLC and
runs. Parameters set in the UI: frame (3E/1C/3C), transport and its settings, station/PC numbers,
device type + start address + amount, command mix, iteration count. It reports round-trip latency
(min/avg/max/p95), throughput, error counts by category, and dumps the raw frames for the failures.

**Acceptance criteria:**
- [x] Reuses the **product's** codecs and transports by source, not a copy — a bench that passed
      against its own private codec would prove nothing about the shipped one.
- [x] Every parameter is settable without recompiling, and the last settings are remembered
      (per frame, so the 1C serial settings and the 3E IP address both survive switching).
- [x] A failed run reports *which* stage failed (Connect / Build / Send / Timeout / Parse) and
      the offending bytes, as hex, for the first few failures of each command.
- [x] Builds into its own `build/` dir next to its `.pro`, never under root `build/`.

**Verification:**
- [x] Builds clean; starts and stays up with no PLC attached (smoke-tested)
- [ ] Owner-run against the real PLC on 3E first (the known-good control), then 1C and 3C

**Dependencies:** A5
**Files touched:** `tools/mc_protocol_bench/{mc_protocol_bench.pro, main.cpp, mainwindow.{h,cpp,ui},
bench_runner.{h,cpp}, gadget_table.{h,cpp}}`
**Estimated scope:** L — **did not need the documented split.** It came out much smaller than
planned, for the reason below.

**The design decision that shrank this task: the bench does not re-declare a single protocol
parameter.** The plan listed them out — frame, transport settings, station/PC numbers, device
area, command mix, iterations — which reads like a form with forty fields and a per-frame page
for each of the three frames. It is not, because *those parameters already exist as a Q_GADGET*:
`McContext` and `McMsgItfConfig` carry every one of them as a `Q_PROPERTY` with a display name
and min/max in `Q_CLASSINFO`. So the bench edits **the product's own config objects** through one
generic table (`GadgetTable`) built from their meta-objects.

Three consequences worth having:
- **The bench configures a PLC the same way the application does.** A number typed here goes
  through the same property, the same range check and the same `toJson()` as the commissioning
  UI, so a bench result describes a configuration the product can actually hold.
- **The next frame added costs no bench UI at all** — 4C, 1E, whatever it is, appears with its
  own parameters the moment it has a context.
- **The poll area is the context's own M/D range**, not a bench-local field, so "the range I
  benched" and "the range the device polls" cannot drift apart.

**Defaults, and why these ones:**
- **1C: COM3, 38400 8N1, no flow control** — the settings the project owner measured on the real
  port after A6. A bench that opens on settings known to have worked once beats re-entering them
  every launch.
- **3C: the same line settings.** The two C-frames are alternative protocols over one cable, so
  starting them on the same port is the useful default; the access route keeps the reference
  implementation's 00/00/FF/00.
- **3E: untouched** — `McMsgEthernetTcpCfg`'s own 192.168.0.1:5000. The one frame with field
  history stays on the values it has always had.
- **Poll area M100 ×16 / D100 ×8.** Deliberately small: a first run that reads 64 devices gives a
  number that cannot be compared against a single round trip, and on an unfamiliar PLC a wide
  range is likelier to hit an unconfigured address and fail for a reason that has nothing to do
  with timing. All of it is editable and remembered.

> ⚠️ **Writes are off by default and ask before running.** The bench can write bits and words,
> and on a live machine that can move an axis or start a cycle. The two write commands are
> unchecked by default, carry a warning label, and a run with either enabled puts up a
> confirmation naming the exact addresses and the iteration count. `loadSettings()` also defaults
> them to off rather than to their last value's opposite — a bench that comes up armed to write
> is one misplaced click from a machine movement.

---

### Checkpoint A: MC thread complete — ✅ CLOSED 2026-09-07, coverage gap carried forward
- [x] Umbrella build clean; `architecture_contract_test` green (83); `mc_frame_test` green (38)
      *(run 2026-09-02)*
- [x] **Owner-confirmed on real hardware (2026-09-03):** 3E still works — **no regression**; 1C
      connects and reads; 3C connects and reads. The regression risk this checkpoint was built
      around is therefore retired: the C-frames did not break the shipping 3E path, and both new
      frames talk to a real C24 module.
- [x] Docs updated: `src/device/AGENTS.md` carries the three-axis composition table and the
      "a frame missing from the wizard combo is dead code" rule; **`docs/domains/mc_protocol/
      mc_protocol.md` written 2026-09-07** — the MC domain doc did not exist before. It states the
      coverage gap in its own ⚠️ block so a reader who never opens this plan still finds it, and
      is indexed in `docs/README.md`.
- [x] **Closed 2026-09-07 at the owner's direction**, with the two hardware items carried to
      `docs/backlog/later_todo_list.md` **item 56** rather than ticked:
      - **Command coverage is partial.** Owner: *"chưa test hết các lệnh đọc ghi mà protocol có"* —
        connect and read are proven on 1C/3C; the remaining read/write commands are not. What is
        untested is exactly the class `mc_frame_test` cannot reach: a codec can be byte-correct
        against a reference frame and still be refused by a real C24 whose sum-check, station or
        access-route handling differs **per command**. **Write commands matter most** and are the
        ones a bench run defaults to off.
      - **No benchmark numbers.** `tools/mc_protocol_bench` (Task A8) is built and smoke-tested; it
        has never been pointed at the PLC, so there is no measured latency figure for any frame.

      > Closing the checkpoint retires the **regression** risk it was built around — 3E is
      > unaffected and both C-frames talk to a real C24. It does not certify the C-frames as fully
      > exercised, and item 56 exists so that distinction survives this document.
- [x] "Review before Phase B" — moot. Phase B was implemented and closed before this gate was
      revisited, so it is recorded as bypassed rather than passed.

> **Note on what the contract test can and cannot prove here.** It can prove JSON round-trips,
> factory arms and marker tables. It cannot prove a frame is correct on the wire — only A7's
> byte-exact references and A8 against the real PLC can. Phase 7 closed with five defects that
> compiled clean and passed the suite; the C-frames are exactly that class of work.

---

## Phase B — Modbus TCP Client And Server

### Task B1: Result output becomes a runner capability — ✅ DONE (2026-08-25)

**Description:** Make the `vision_output` role capability-based, following the existing
`IDeviceRunner::submitCommand()` precedent: add `supportsResultOutput()`, `requestSendResult()`
and the `resultRequestFinished` signal to `IDeviceRunner` with a "not supported" default, move
`VisionOutputRunner`'s implementation into that override, and change the controller's
`RuntimeContext.visionOutputRunner` to `QPointer<IDeviceRunner>`.

**And fix the robot-check regression this creates**: `buildRuntimeContext()` reads
`robotCheckConfig` by casting the bound device's config to `VisionOutputDeviceCfg`. That cast
fails for any non-VisionOutput device and the settings silently default. Either the check settings
move to a device-family-independent home, or a bound device that cannot supply them is **refused
at binding time with a visible message**. Silent default is not an option.

**Acceptance criteria:**
- [x] `LocalizationRuntimeController` sends results through the capability, with no
      `qobject_cast<VisionOutputRunner *>` remaining on the result path.
- [x] Binding a device whose runner does not support result output is rejected in the UI with a
      reason — not accepted-then-silent at runtime.
- [x] The three existing vision-output devices behave identically to before; the TCP/IP wire
      output is byte-identical.
- [x] `robotCheckConfig` is either resolved for every bindable device or the binding is refused.
      **Whichever it is, a contract test asserts it.**
- [x] Contract test proven to **fail** against an injected violation (a runner that claims support
      and does nothing), before being trusted.

**Verification:**
- [x] Umbrella build; contract test green with new cases — **74 passed / 0 failed** (72 → 74)
- [x] `tests/vision_output_device_test` (8/0), `tests/vision_tcpip_client_device_test` (9/0) still
      green. Both build directories held **stale makefiles** referencing the pre-Phase-7
      `src/logger/` path and had to be re-`qmake`d; nothing in the tests themselves changed.
- [ ] Owner-run: an existing project with a TCP/IP output device runs a full localization cycle
      and the robot pick check still uses its commissioned settings

**How the four criteria were actually met**

1. **The capability lives on `IDeviceRunner`** — `supportsResultOutput()` (false by default),
   `requestSendResult()` (default emits a refusal), and the `resultRequestFinished` signal moved up
   from `VisionOutputRunner`. `RuntimeContext.visionOutputRunner` is now
   `QPointer<IDeviceRunner>`, `visionOutputRunner()` returns `IDeviceRunner *`, and the result-path
   `connect()` names `&IDeviceRunner::resultRequestFinished`. No cast remains.

2. **The default is a refusal, not silence.** A runner that does not implement the capability emits
   `resultRequestFinished(false, …)` synchronously. This matters more than it looks: the controller
   connects to that signal and *waits* for it, so a default that did nothing would hang the cycle
   forever instead of faulting it. The contract test asserts the refusal fires.

3. **`robotCheckConfig` is resolved for every bindable device — the first branch of the criterion,
   not the refusal branch.** `IResultOutputDevice` was an **empty marker interface**; it now carries
   the two things the role actually needs — `sendVisionResult()` and
   `robotKinematicCheckConfig()`. Putting both on one interface is the point: a device cannot offer
   to send results without also answering for the pick-check settings, which is exactly the
   combination the old code let drift apart.

4. **`RobotKinematicCheckConfig` and `PickPathPoint` moved** out of
   `output_device/vision_output_config.h` into `device/robot_kinematic_check_config.h`. Pure
   relocation: `vision_output_config.h` includes the new header, so all 22 existing users compile
   untouched, `VisionOutputDeviceCfg` still owns the `m_kinematicCheck` member, and **the JSON key
   and layout did not move** — existing project files load unchanged, and no compatibility shim was
   needed or added. They live outside the vision-output family now because a PLC-family device
   serving this role has to carry them too.

5. **The UI refusal is by construction.** `rebuildDeviceCombos()` lists devices implementing
   `IResultOutputDevice` instead of `assignedDevicesOfType(DeviceType::VisionOutput)`, so an
   incapable device cannot be picked at all — and this is the same edit that will make a Modbus
   device selectable in B5 without touching the widget again. `loadConfigToWidget()` additionally
   logs a user-visible warning when a *stored* binding no longer qualifies, because the combo
   silently falling back to empty while the config still names the device is precisely the
   accepted-then-silent state this task exists to remove. `setup()` refuses the same case with a
   named error, so both the commissioning end and the runtime end say why.

**Decisions taken while implementing**

- **`requestConnect()` / `requestDisconnect()` were hoisted onto `IDeviceRunner`** (pure virtual;
  all three runners already had identical signatures). `requestRoleConnectNow()` used to switch on
  the role and downcast to the concrete runner type — with a capability-filled role that cast can
  fail, and the failure mode is a role that silently **never reconnects**. This deleted more code
  than it added and was not optional: it would have become a live defect the moment B3 landed.
- **`sendVisionResult()` builds the `VisionOutputRequest`, not the runner.** The runner now only
  marshals threads. That is what keeps the TCP/IP bytes identical (same request, same
  `pushRequest()`) while letting a PLC runner implement the same capability without ever
  constructing a `VisionOutputRequest`.
- **`PlcRunner` was left alone.** B1's description anticipated it implementing the capability, but
  there is no PLC device implementing `IResultOutputDevice` until B3. Overriding it now would ship
  a runner that claims a capability its device cannot honour — the exact violation criterion 5
  tests for. It gets the override in B3, with the device.

**Injected-violation proof (run, not assumed).** With `VisionOutputRunner::requestSendResult()`
emptied so it claims support and does nothing, the suite went to **68 passed / 6 failed** — the new
`test_runner_that_claims_result_output_actually_delivers_it` failed first, along with five existing
cycle tests. Reverted; back to 74/0.

**Dependencies:** None (may start any time; independent of Phase A)
**Files actually touched:** `src/device/robot_kinematic_check_config.h` (new),
`src/device/device_capabilities.h`, `src/device/output_device/vision_output_config.h`,
`src/device/output_device/vision_output_device.h`, `src/device/device.pri`,
`src/runtime/idevice_runner.h`, `src/runtime/vision_output_runner.h`, `src/runtime/plc_runner.h`,
`src/runtime/camera_runner.h`, `src/model/localization_runtime_controller.{h,cpp}`,
`src/model/task_localization.cpp`, `src/ui/forms/task/localization_setting_widget.cpp`,
`tests/architecture_contract_test/main.cpp`, plus three docs
(`runtime_controller_api.md`, `task_localization_api.md`, `robot_kinematics_module.md`)
**Estimated scope:** L — **the riskiest task in the phase.** It touches the commissioned runtime
path. Do it first, alone, and stop at the checkpoint.

---

### Checkpoint B-1: role model — ✅ CLOSED, owner-confirmed 2026-08-26
- [x] Everything above green **and** an existing project verified end-to-end by the owner
      (full localization cycle run)
- [x] The Q1 option-2 fallback (separate PLC-side and output-side Modbus sub-types) was **not**
      needed; the capability model holds.

---

### Task B2: Modbus register-map core and result layout contract — ✅ DONE (2026-08-26)

**Description:** One config + register map shared by client and server: unit/slave id, the four
Modbus areas (coils, discrete inputs, holding registers, input registers), polled ranges, and the
tag naming used by `IPlcTagProvider`. Then **write down the vision-result register layout** —
start address, word order, the count word, per-position field order and encoding (scaled int16 vs
IEEE-754 across two registers), and the completion handshake — as a document, before it is coded.

**Acceptance criteria:**
- [x] Tag names are unambiguous across areas and stable (they are persisted in task signal maps).
- [x] The result layout doc states the axis count and field order explicitly, and says what a
      downstream PLC programmer must not assume — the `VisionOutputPosition` doc comment is the
      model for the tone and the warning.
- [x] Encoding choice is justified in the doc, not just stated.
- [x] Config round-trips through JSON and carries `kDisplayNameSources[]`.

**Verification:**
- [x] Umbrella build; contract test round-trip case (four new cases, not one)
- [x] **Owner reviews the layout doc — APPROVED 2026-09-07.** The layout ships as commissioned:
      **scaled int32 (×100), high word first** — *not* IEEE-754. An earlier version of this line
      and of Checkpoint B said "IEEE-754 across two registers", carried over from Open Question 1's
      recorded answer rather than from what was built; corrected 2026-09-07. The implementation
      deviated from that answer deliberately and said so at the time (see the "Key decisions" note
      below and `modbus_result_contract.md` → "Why scaled int32 rather than IEEE-754 float"), so
      what the owner approved is the shipped scaled-int32 layout. **`kScale = 100`**
      (`modbus_result_layout.h:70`), giving two decimals — the same resolution the TCP/IP frame's
      `%08.2f` has always sent, so one commissioned pose reads identically on both transports.
      That word order was raised as a possible
      defect on 2026-09-03 and resolved on 2026-09-04 as *not* one — the recorded "same as
      Mitsubishi order" answer was about a Mitsubishi master, and there is no Mitsubishi master on
      this path; the vision result goes to the robot. The approval makes the encoder a **frozen
      contract**: it is now what a working machine depends on, and changing it silently would break
      that machine to satisfy a note. The still-unbuilt half of Open Question 2 — an argument to
      *select* the word order — remains in `docs/backlog/later_todo_list.md`, where it waits for a
      second master to shape it.

**Key decisions, with the reasoning that is not obvious from the code**

- **Tag names are `COIL`/`DI`/`HR`/`IR` + a 5-digit zero-padded address.** Five digits because the
  Modbus space runs to 65535 and a narrower field would sort wrongly past the padding width. The
  prefixes deliberately do **not** collide with the Mitsubishi family's `M`/`D`: if they did, a
  project switched from an MC device to a Modbus device would resolve *some* tags by accident and
  bind them to unrelated registers — half-working in silence. Disjoint prefixes make the switch
  fail loudly on every tag, which is the outcome you want when the hardware changed.
- **Positions encode as scaled int32 (×100), high word first**, not IEEE-754. The decisive
  argument is not size — both take two registers — but that ×100 is exactly the resolution the
  TCP/IP result frame has always sent (`%08.2f`), so the same commissioned pose reads identically
  on both transports. A single scaled int16 would have capped at ±3276.7 mm, inside a real robot
  envelope. Word order is a choice, not a standard, and is stated as such in the doc.
- **Default capacity is 8 positions**, making the whole block 100 registers — inside the 123-register
  ceiling of one Modbus write request, so the common case publishes atomically on the wire. Above
  9 positions it necessarily splits, which is what makes the count-last handshake load-bearing
  rather than decorative. Both facts are surfaced on the device panel, not only in the doc.
- **Word values reach the task signed.** A holding register carrying 40000 arrives as −25536,
  matching what the Mitsubishi family does for D devices, because a task number signal is signed
  16-bit throughout the application. Recorded in the doc's "must not assume" list.
- **Three config headers, not one file.** The contract test reads each header and requires every
  `QT_TRANSLATE_NOOP` context in it to name that header's class, so two marker tables in one file
  cannot both be right. This was found by the test, not predicted.

**Dependencies:** B1
**Files touched:** `src/device/plc/modbus/{modbus_config.h, modbus_register_map.{h,cpp},
modbus_result_layout.{h,cpp}, modbus_tcp_client_config.h, modbus_tcp_server_config.h}`,
`src/device/plc/plc_device.h`, `src/device/idevice_config.h`, `src/device/device.pri`,
`docs/domains/task_localization/modbus_result_contract.md`,
`tests/architecture_contract_test/main.cpp`
**Estimated scope:** M

---

### Task B3: `ModbusTcpClientDevice` — ✅ DONE (2026-08-26)

**Description:** PLC-family device on `QModbusTcpClient`: connect to a slave, poll the configured
ranges on the refresh interval, expose `IPlcTagProvider`/`IPlcIoWriter` for IO mapping, and
implement `IResultOutputDevice` to write vision results into holding registers per B2.

**Acceptance criteria:**
- [x] Poll → `valueChanged()` / `pollingUpdate()` matches what the localization signal mapper
      already consumes from `McProtocolDevice`; no change is needed in the mapper.
- [x] Lost connection publishes `LostConnected` so the existing recovery policy starts
      reconnecting — the same contract `McProtocolDevice` meets.
- [x] A write while disconnected fails and is reported; it is never queued forever.
- [x] Result write is atomic from the PLC's point of view: the count/handshake word is written
      **after** the payload, never before.

**Verification:**
- [x] Umbrella build; contract test
- [x] `tests/modbus_device_test` against a local `QModbusTcpServer` fixture — no hardware (13/0)
- [ ] Owner-run against the real slave

**Decisions worth knowing**

- **Every Modbus transaction is a bounded blocking wait on the device's own worker thread.** Qt's
  Modbus API is asynchronous only, and both alternatives are worse: an async publish cannot honour
  `IResultOutputDevice::sendVisionResult()`'s synchronous contract, and interleaving a poll with the
  publish sequence would let a master observe a half-written result block. The runner already
  marshals every call onto this thread, so nothing the GUI can feel is blocked.
- **A disconnect arriving mid-transaction is deferred, not honoured immediately.** The nested wait
  holds a reply owned by the client; tearing the client down underneath it would destroy what the
  wait is holding. `teardown()` records the request and the transaction unwinds first.
- **Polling pauses for the duration of a result publish** — stopping the timer is not enough on its
  own, because `transact()` runs a nested event loop, which is exactly where a timer tick would
  otherwise fire.
- **`pushRequest()` returns false rather than accepting anything.** Results arrive through
  `IResultOutputDevice` and IO through `IPlcIoWriter`; silently accepting a request nothing will
  ever send would be a queue that never drains.

**Dependencies:** B2
**Files likely touched:** `src/device/plc/modbus/modbus_tcp_client_device.{h,cpp}`,
`src/device/device.pri`, `qmake/common_deps.pri` (`QT += serialbus`)
**Estimated scope:** M

---

### Task B4: `ModbusTcpServerDevice` — ✅ DONE (2026-08-26)

**Description:** The same surface with `QModbusTcpServer`: we are the slave, we own the register
space, a PLC master reads and writes it. IO mapping reads what the master wrote; results are
published into our holding registers for the master to read.

**Acceptance criteria:**
- [x] Listen address/port configurable; a port already in use fails visibly, not silently
      (asserted by occupying the port with a `QTcpServer` first).
- [x] Master writes surface as value changes through the same signal path as the client device.
- [x] Multiple master connections behave predictably (documented, whichever way Qt handles it —
      **verified, not assumed**).
- [x] Result publication and the handshake word follow B2 exactly, so a PLC program written
      against the client works against the server — asserted by reading the server's block through
      a real `QModbusTcpClient` and comparing against the client device's.

**What "verified, not assumed" turned up.** `QModbusTcpServer` accepts concurrent master
connections and serves all of them from the one shared register space. There is **no per-master
isolation and no per-master handshake**: two masters consuming the same result block both see the
same count and sequence, and neither can tell the other has taken it. A cell with two consumers
needs its own arbitration; this layer will not provide one. Asserted in
`test_server_accepts_multiple_masters_against_one_register_space`.

**A real defect the tests caught (not an injected one).** The server originally called
`resetChangeBaseline()` at connect, copied from the client. For the client that is right — the
first poll brings a slave state it has never seen. A server has no such moment: it owns the space
and the space starts at zero, so resetting swallowed **the first write a master made after
connecting** — which, for a handshake bit, is precisely the write that matters. Fixed by adding
`ModbusRegisterMap::adoptCurrentAsBaseline()` and using it on the server side.
`test_server_master_writes_surface_as_value_changes` failed on exactly this before the fix.

**The served span of the result's own area is the union of the mapped span and the result block.**
The result block sits outside the mapped span by default; a server that only owned the mapped span
would answer a master's read of the result with an illegal-data-address exception — publishing to
nobody. Asserted in `test_server_serves_the_result_block_outside_the_mapped_span`.

**A second real defect, found by the owner rather than by a test (2026-08-26).** The server
refused to write discrete inputs and input registers, using `isWritable()` — which is documented
as *"whether the Modbus protocol allows a **master** to write this area"* — as though it answered
"may **we** write it". It does not. "Input" in *discrete input* and *input register* is named from
the server's point of view, and the server is the **only** side that can put a value there; the
protocol defines no function code by which a master writes 1x or 3x.

Two things were lost by that mistake:
1. The task could not publish an outgoing bit or word into 1x/3x at all on the server sub-type.
2. The result block was pinned to holding registers, so the **safest server layout was
   unavailable** — a result in input registers cannot be overwritten by a master, accidentally or
   otherwise, while one in holding registers can.

The fix splits the predicate in two (`isMasterWritable()` / `isServerWritable()`) so every call
site has to answer "who is asking?", makes the server write all four areas, threads the result
area through the publish path, and adds `ModbusTcpServerCfg::resultInInputRegisters` (default
false, so an unchanged project keeps the published layout). The widget's monitor subtitle now says
what *this* device may do with the area — "read / write", "write-only outward", or "read-only" —
instead of quoting the protocol in the abstract.

Covered by three new cases in `tests/modbus_device_test`:
`test_server_writes_the_input_areas_the_master_can_only_read` (server writes them, a real master
reads them back, and the master's own write does **not** succeed),
`test_client_still_cannot_write_the_input_areas`, and
`test_server_publishes_the_result_into_input_registers_when_configured`. The first was proven to
fail against the old behaviour — `isServerWritable()` temporarily reverted to `isMasterWritable()`
— before being trusted.

**A protocol artifact worth knowing.** A one-bit read comes back byte-padded: asking a server for
one discrete input yields a reply carrying the whole byte, so Qt reports eight values. Assert the
value, not the count, or a correct implementation reads as a failure.

**Verification:** as B3, with a `QModbusTcpClient` fixture — done, `tests/modbus_device_test` 13/0
**Dependencies:** B2
**Files likely touched:** `src/device/plc/modbus/modbus_tcp_server_device.{h,cpp}`,
`src/device/device.pri`
**Estimated scope:** M

---

### Task B5: Registry, factory, enums, persistence — ✅ DONE (2026-08-26)

**Description:** Wire both devices into `PlcType`, the JSON tokens, `DeviceRegistry` (**after**
`MitsubishiMc` and **before** `VirtualPlc` — the virtual-last ordering is load-bearing),
`DeviceFactory` and the marker tables.

**Acceptance criteria:**
- [x] Both appear in the Add Device wizard and create correctly. The wizard's PLC combo is filled
      from the registry, so listing them was automatic — but `buildDeviceJson()` fell through to
      the Mitsubishi branch for **any** non-virtual sub-type, which would have written an
      `McProtocolConfig` into a Modbus device. Arms added.
- [x] Registry ordering keeps `Virtual PLC` last within the family (asserted, order and all).
- [x] JSON tokens are final on first write: `"ModbusTcpClient"` / `"ModbusTcpServer"` plus the
      `DEVICE_JSK_MB_*` config keys.
- [x] `totalNames` raised 87 → 105; no new combo enums, so no enum-key marker table was needed.

**Verification:** umbrella build; contract test (creation of both sub-types from JSON asserted,
including that the register map is rebuilt on load — without it a restored device offers no tags
to the signal-map editor and the commissioned mapping cannot be re-edited); owner creates one of
each from the wizard
**Dependencies:** B3, B4
**Files likely touched:** `src/device/plc/plc_device.h`, `src/device/device_registry.cpp`,
`src/device/device_factory.cpp`, `tests/architecture_contract_test/main.cpp`
**Estimated scope:** S

---

### Task B6: Modbus client and server widgets — ✅ DONE (2026-08-26), delivered as **one** widget

**Description:** Two device widgets following `MitsubishiMcDeviceWidget`: connection card,
property browser over the config, and `DevicesMonitorWidget` instances for the bit and word areas.
Plus the `DeviceWidgetFactory` dispatch — noting that the current PLC arm rejects by *equality*
against `MitsubishiMc`, so it must become a switch or every new PLC silently gets "no panel".

**Acceptance criteria:**
- [x] Register monitors show live polled values and accept manual writes while connected.
- [x] The result-register block is visible and labelled as such — an operator can read the exact
      register range off the panel, plus a warning when the capacity exceeds one Modbus write and
      another when the block overlaps the mapped holding range.
- [x] All labels translatable and present in the `.ts` (sweep added 46 source texts, none lost).
- [x] `DeviceWidgetFactory` returns the right widget for both sub-types and still logs a clear
      error for a genuinely unknown one.

**Delivered as one widget, not two — the plan was wrong that they are independent.** The client and
the server differ in exactly one thing, the connection card, and share the entire register-map
surface: same tag names, same monitors, same result block, same property browser. Two classes would
have been two copies of a 600-line panel kept in sync by hand, and the one that mattered would be
the one someone forgot to update. `MitsubishiMcDeviceWidget` already carries two connection cards
(TCP and serial) in one widget for exactly this reason; this follows it.

**One design problem the plan did not anticipate.** A Modbus device has **four** areas where the
Mitsubishi one has two, and the addresses of two areas overlap (coil 0 and discrete input 0 are
different registers). A single monitor cannot show both without its rows becoming ambiguous, and
four stacked monitors would not fit. Each monitor therefore shows one area at a time, with a combo
choosing which, and its subtitle states whether the area is writable — because two of the four are
read-only **by the Modbus specification**, not by our choice.

**The `DeviceWidgetFactory` hazard the plan flagged was real.** The PLC arm rejected by equality
against `MitsubishiMc`, so every PLC sub-type added after it would have silently got
"No configuration panel available for this device" — a soft failure that reads as a missing panel
rather than a missing registration. Now a switch, with `VirtualPlc` and `PlcTypeNone` enumerated so
a future sub-type surfaces a compiler warning here.

**Verification:** umbrella build ✅; `update_translations.ps1` ✅ (46 new, 0 vanished); owner drives
both widgets against a real slave/master — pending
**Dependencies:** B5
**Files touched:** `src/ui/forms/plc/modbus_device_widget.{h,cpp,ui}`,
`resrc/styles/modbus_device_widget_{dark,light}.qss`, `resrc.qrc`,
`src/ui/forms/device_widget_factory.cpp`, `src/ui/ui.pri`,
`app/translations/ncr_picking_ja_JP.ts`
**Estimated scope:** L → M in practice, because it became one widget.
**Debt recorded, not fixed:** backlog #48 — `addPropertyToBrowser` now exists in three device
widgets. Extracting it means editing two commissioned panels inside a task about adding a third.

---

### Checkpoint B: Modbus thread complete — ✅ CLOSED 2026-09-07
- [x] Umbrella build clean; contract test green (**86**); `modbus_device_test` green (**21**)
- [x] **Commissioning defect found and made visible (2026-09-04), not a blocker.** The robot wrote
      `nActiveCamera` and `nActivePatternGroup` in **one** FC16 request and only the camera took
      effect; the same two values as two separate requests both worked. Cause is master-side
      addressing — FC16 covers a **contiguous** block, so batching two signals that are not
      adjacent puts the second value on whatever register follows the first. The app's part was
      that it **ACKs such a write and silently drops** the unmapped portion: `servedRange()` is the
      union of the mapped span and the result block, while `isConfigured()` covers only the mapped
      span (HR0–63 mirrored inside HR0–1000+ served). `onDataWritten()` now reports
      `delivered=N of M` at **user** level, with the result block exempted so the device's own four
      writes per cycle do not train the operator to ignore it. Regression test
      `test_a_master_write_spanning_past_the_mapped_span_delivers_only_the_mapped_part`
      (20 → 21 cases), negative-checked. Trap written up in `src/device/AGENTS.md`.
- [x] **Owner-confirmed (2026-08-27):** a Modbus TCP server and client both run in the commission
      phase and their I/O mapping is configured
- [x] **Owner-confirmed (2026-09-03):** a task bound to one Modbus device for **both** the PLC role
      and the vision-output role completes a localization cycle — handshake through coils, result
      in holding registers. Owner: *"Modbus server device đã được test thực tế với PLC, xác nhận
      hoạt động đúng"* and *"Đã test chạy thử trong runtime với robot và camera thật, xác nhận hoạt
      động đúng kì vọng của plan."* This closes the 2026-09-01 blocker below end-to-end, not just
      at the unit level: the `PlcRunner` capability fix is what let this configuration reach setup,
      and the cycle then ran against a real PLC and a real robot.
- [x] **Owner-confirmed (2026-09-03):** a Modbus TCP **server** and a Modbus TCP **client** each
      connect to a third-party device. Both sub-types are exercised against equipment this project
      does not control, which is the only way the register/coil mapping is proven interoperable
      rather than self-consistent.
- [x] **Blocker found and fixed (2026-09-01).** The owner's first attempt at exactly this
      configuration faulted at setup:
      `LocalizationRuntimeController setup: Device bound to vision_output cannot output results: 02`.

      Nothing was missing from the device or the task. `ModbusTcpServerDevice` implements
      `IResultOutputDevice`, and `buildRuntimeContext()` resolved and passed its runner correctly
      (had it not, the error would have read *"Missing vision_output role runner"*). The capability
      was declared on the **device** and only ever read off the **runner**, and `PlcRunner` — unlike
      `VisionOutputRunner` — never overrode `supportsResultOutput()` / `requestSendResult()`. The
      comment on `VisionOutputRunner::requestSendResult()` had already anticipated this
      ("*which is what lets a PLC-family runner implement the same capability*"); it was simply
      never written.

      `PlcRunner` now resolves it **per device** with a `dynamic_cast`, not per family: the PLC
      family is mixed, and a flat `true` would let an MC PLC be accepted for the role and then hang
      the first cycle waiting on `resultRequestFinished`. Contract test
      `test_plc_runner_reports_result_output_per_device_not_per_family`, **verified to fail before
      the fix** (79 → 83 cases).
- [x] **Modbus result layout APPROVED — owner, 2026-09-07.** This is the B2 review gate, open
      since the phase began and carried through the 2026-09-04 word-order finding. The shipped
      layout stands as commissioned: **scaled int32 (×100), high word first** — *not* IEEE-754
      (this line said IEEE-754 until 2026-09-07; see Task B2), which is
      what the robot program expects and what the cell proves end to end. The encoder is now a
      frozen contract — a silent flip would break a working machine.
- [x] **Modbus confirmed working on BOTH roles — owner, 2026-09-07.** Server and client, each
      against equipment this project does not control, plus the dual-role binding where one Modbus
      device fills the PLC role and the vision-output role in the same task.
- [ ] ➡️ **CARRIED to `docs/backlog/later_todo_list.md` item 57** (was: robot pick check verified
      still **active** on that configuration — the B1 hazard, re-checked at the end). Not part of
      the owner's 2026-09-07 confirmation, and it needs a different kind of evidence than the
      other two boxes, so it moves rather than closes.

      > **Deliberately left open even though the 2026-09-03 runtime run used a real robot.**
      > "The robot picked correctly" is not evidence here, because the failure mode is silent:
      > B1's hazard is `buildRuntimeContext()` casting the bound device's config to
      > `VisionOutputDeviceCfg` to read `robotCheckConfig`, a cast that fails for a Modbus device
      > and **defaults the settings**. A defaulted-off check produces a *working* robot — the cell
      > runs, parts get picked, and nothing looks wrong until the one cycle where the check was
      > supposed to stop it. So this box needs the check observed *doing something*: one run with
      > a deliberately bad pick, or the check's own settings read back from the live task, on the
      > dual-role Modbus binding specifically.
- [x] **Word order on the wire reconciled** — raised 2026-09-03, closed 2026-09-04. The shipped
      layout packs 32-bit values high word first, which read as contradicting Open Question 2's
      recorded answer. It does not: that answer was about a Mitsubishi master, and the master on
      this path is **the robot**, which the owner commissioned against the current encoder and
      which decodes it correctly. The encoder stays as it is. The settable word-order argument is
      still unbuilt and is now backlog, not a Phase 8 item — see Open Question 2.
- [x] Docs updated: `src/device/AGENTS.md`, the result-contract doc (incl. the firewall note),
      `docs/architecture/device_type.md`
- [x] **Commissioning note captured (2026-08-27):** a Modbus TCP **server** binds and reports
      *Connected* while remaining unreachable to every master on the network until the app is
      allowed through Windows Defender Firewall. Inbound only, so the client sub-type is
      unaffected — which makes a client/server pair on one station look half-broken. Written up
      in `docs/product/customer_installer_packaging.md` → "Windows Defender Firewall", and
      cross-referenced from the result-contract doc. It applies equally to GigE camera discovery
      and to VisionOutput in server mode.

---

## Phase C — JAI GigE Camera

> ⚠️ **Correction, 2026-08-25.** When this plan was written the only JAI evidence on the machine
> was the JAI SDK C API (`jai_factory.h`, `Jai_Factory.lib`, `JAI_SDK_*` environment variables),
> and C1/C2 below were written against it. The updated request points instead at
> `reference source/JaiCamTest` and `reference source/Jai_ebus_docs` — the **Pleora eBUS SDK**
> (`PvDevice` / `PvStream` / `PvBuffer`), a different API with a different deployment set.
> The reference tree carries eBUS samples and documentation, so **eBUS is the intended path**.
> C1 and C2 are scoped against the wrong SDK until they are re-read against those samples;
> that re-read is the first step of C1, not a detail inside it. Everything else in Phase C —
> registry wiring, widget, the marker rules, the traps in `src/device/AGENTS.md` — is
> SDK-independent and stands as written.

> ✅ **C1 first step settled (2026-08-27).** `reference source/JaiCamTest` builds against
> `C:/Program Files/JAI/eBUS SDK` and its `main.cpp` uses `PvSystem` / `PvInterface` /
> `PvDeviceInfoGEV` / `PvDevice`. **The Pleora eBUS SDK is the path**; the JAI SDK C API is
> installed on the machine and is not used. C2 was scoped against eBUS accordingly.

### Task C1: JAI camera SDK build wiring — ✅ DONE (2026-08-27)

**Delivered:** `qmake/ebus_dependency.pri`, included from `qmake/local_dependencies.pri` so the
library and both shells all get it. No `-l` flags: the eBUS headers auto-link through
`#pragma comment(lib, ...)` and pick the `64`-suffixed import libraries themselves under
`_WIN64`, so `-L` is genuinely all that is needed (documented in the .pri, because the absence
looks like an omission otherwise).

Paths come from the installer's own `PUREGEV_ROOT` machine variable, so **a station with the SDK
installed needs no configuration at all**; `EBUS_INCLUDE_DIR` / `EBUS_LIB_DIR` /
`EBUS_RUNTIME_DIR` override it for a non-default install and are documented in
`qmake/local_paths.pri.example`, `AGENT.md` and `3rdparty/README.md`.

**Verified:**
- Fresh `qmake src/src.pro` in an empty build dir resolves
  `-I"C:\Program Files\JAI\eBUS SDK\Includes"` with nothing exported. ✅
- Fresh `qmake ncr_picking.pro CONFIG+=deploy_deps` queues **19** eBUS runtime DLLs and
  **zero** foreign OpenCV files. ✅
- Negative case run, not assumed: `qmake "EBUS_INCLUDE_DIR=C:/no/such/ebus/Includes"` fails with
  `Project ERROR: EBUS_INCLUDE_DIR does not look like the eBUS SDK includes: '...PvDevice.h' not
  found.` — at qmake time, naming the SDK, not at link time on an unresolved `Pv*` symbol. ✅

**Two things found while doing it, both recorded in `src/device/AGENTS.md`:**
- The eBUS *runtime* folder is shared and contains an unrelated OpenCV 4.10 build (~208 MB).
  Deployment therefore copies **by name prefix**, never `*.dll`. Today the names differ from
  ours so a glob would break nothing visibly — which is precisely what would let it sit
  unnoticed until this project moved to OpenCV 4.10.
- `qmake/local_paths.pri` did **not exist** on this machine: `OPENCV_*` / `PYLON_*` were set only
  inside the Qt Creator kit, so `qmake src/src.pro` from a plain shell had never worked
  (`OPENCV_INCLUDE_DIR must point to...`). Created it from the values read back out of the
  working build's generated makefiles — the exact split that file exists to close.

### Task C1 (original scoping): JAI camera SDK build wiring

**Description:** Include/lib wiring for the SDK the samples in `reference source/JaiCamTest`
actually use, driven by environment variables and following exactly how Pylon is wired, with
`qmake/local_paths.pri.example` updated and a clear failure when the SDK is absent.

**First step, before any wiring:** read `reference source/JaiCamTest` and settle which SDK the
camera is driven with — eBUS (`PvDevice`/`PvStream`) or the JAI SDK C API (`jai_factory.h`).
Both are installed on this machine. The answer decides C2's entire shape, and getting it from the
samples costs minutes; getting it wrong costs C2 twice.

**Acceptance criteria:**
- [ ] No absolute JAI/eBUS path in any tracked file.
- [ ] A machine without the SDK fails at qmake time with a readable message, not at link time with
      an unresolved symbol.
- [ ] The SDK's runtime DLLs reach `build/bin/<config>` the way the Pylon and RobotKinematics
      runtimes do.

**Verification:** clean umbrella build from a fresh build dir
**Dependencies:** None
**Files likely touched:** `qmake/common_deps.pri`, `qmake/local_paths.pri.example`,
`3rdparty/README.md`, `AGENT.md` (expected variables list)
**Estimated scope:** S

---

### Task C2: `JaiGigeCfg` and `JaiGigECamera` — ✅ DONE (2026-08-27), hardware acceptance pending

**Delivered:** `src/device/camera/jai_define.h` and `camera_jai_gige.{h,cpp}`.

| Acceptance criterion | Status |
|---|---|
| `grabFinished()` on **every** path | ✅ One `GrabResult`, one `finish()` lambda, every return goes through it |
| Unsupported pixel format ⇒ empty `cv::Mat` + log | ✅ Gated on `PvBufferConverter::IsConversionSupported()` before converting |
| Pulled cable ⇒ `LostConnected` | ✅ Via `OnLinkDisconnected`, **plus** a backstop on `ABORTED`/`NOT_CONNECTED` from the grab |
| `setDeviceConfig(&m_config)` in ctor + `fromJson()` override | ✅ Both traps from `src/device/AGENTS.md` |
| Grab timeout configurable and **below** `kSingleShotTimeoutMs` | ✅ `setGrabTimeout()` clamps to 4000 with a `static_assert` against the runner's 8000 |
| `kDisplayNameSources[]`; `totalNames` raised | ✅ 15 names; `totalNames` 107 → **122** |

**Better than the plan asked for, and why:**
- **`OnLinkDisconnected` is registered.** The plan assumed Basler parity, where a pulled cable is
  only ever noticed on a failed grab. eBUS has a real callback, so an idle station reports the
  loss within the heartbeat instead of at the next trigger. The callback arrives on an
  eBUS-internal thread, so `JaiLinkEventSink` queues it onto the device thread — the same
  cross-thread rule the Modbus devices paid for.
- **Feature names are resolved, not hard-coded** (`jai_define.h` `node_names` + `resolveNode()`).
  See the AGENTS.md note; the model is unknown until connect, which is the plan's own answer to
  Q4.
- **The backlight invert flag actually inverts.** `m_autoBacklightInvert` exists on the Basler
  config and is read by nothing there.
- **Discovery failure names the firewall.** Zero cameras found now says so, which after the
  owner's 2026-08-26 finding is the single most likely cause on a fresh station.

**Not done, deliberately:** continuous and software-trigger acquisition return false / no-op,
matching the Basler device. Nothing in the localization runtime uses them.

**Two defects found by the owner's first hardware run (2026-08-27), both fixed:**

1. **App crashed on Connect with an empty log.** Not a bug in connect. `PvDevice64.dll` and
   `PvGenICam64.dll` **delay-load** GenApi/GCBase from
   `…\Pleora\eBUS SDK\GenICam\bin\Win64_x64`, a folder the installer leaves off PATH;
   `PvSystem64.dll` delay-loads nothing, which is why discovery worked and disguised it.
   The first node-map call raised `0xC06D007E` — an SEH exception no `catch (...)` sees — and
   the process died before the logger flushed. Windows Error Reporting named it:
   exception `0xc06d007e`, faulting module `KERNELBASE.dll`.
   Fixed by `src/device/camera/jai_runtime.{h,cpp}`, called before any node-map call.
   **Measured both directions:** a bare-name `LoadLibraryW("GenApi_MD_VC141_v3_4.dll")` returns
   error **126 (ERROR_MOD_NOT_FOUND)** with the stock PATH and succeeds once the directory is
   prepended. The contract test now performs that same bare-name load — the exact operation the
   delay-loader does — so it fails on an unfixed build. **79 cases** now.
   No absolute path is compiled in: `EBUS_GENICAM_BIN`, then `%CommonProgramFiles%`, then
   `%PUREGEV_ROOT%`.
2. **The select dialog listed the same camera several times.** eBUS enumerates per network
   adapter, so a camera reachable through more than one adapter is reported once per adapter.
   Deduplicated on MAC address — the only identifier belonging to the camera rather than to the
   route to it.

**Two more from the second hardware run (2026-08-27, GO-5000M-PGE at 192.168.0.70):**

3. **Every grab logged a burst of `RESENDS_FAILURE` incomplete buffers.** The grabs all
   *succeeded* — the log shows `status= Succeeded` every time — so this read as noise. It was
   not. The camera was left in its default `Continuous` acquisition mode, so `AcquisitionStart`
   made it free-run: 5 MP mono at ~22 fps ≈ **115 MB/s**, past what Gigabit Ethernet carries.
   Most blocks lost packets, resends failed against the saturated link, and the grab returned
   whichever frame survived. Cost: up to **13 discarded frames and 1.25 s for one trigger**,
   against a 1.5 s timeout, and **272 log lines** in one session.
   Fixed by `SingleFrame` acquisition (one frame per trigger), `PvAcquisitionStateManager` for
   `TLParamsLocked`, pipeline buffers 4 → 16, and `SetHandleBufferTooSmall(true)`. The
   per-buffer log line became one summary per grab that names the causes worth checking.
4. **Exposure limits on the property browser were wrong.** Two independent causes, both fixed:
   the range was read with `GetFloatRange()` regardless of the node's real type (it silently
   fails on an Integer node and leaves 0..0, which the browser renders as a field pinned to
   zero); and GenICam ranges move — the exposure maximum depends on the acquisition frame rate —
   while the browser only ever showed what was read at connect. Now type-aware
   (`NumericFeature`), degenerate ranges are rejected and logged rather than written into the
   config, the frame rate is written before the exposure, and the limits refresh in place after
   every apply without rebuilding the tree under the operator's cursor.
   A connect-time line now records what was actually resolved:
   `JAI camera features: exposure ExposureTime[float] 10..8000000 = 5000 | gain … | payload …`

**A regression introduced by fix 3, found by the third hardware run (2026-08-27) — fixed:**

5. **Every single shot failed with `STATE_ERROR - Cannot start, state already set to locked`,
   and the `SingleFrame` fix had never actually applied.** One cause, two symptoms. eBUS offers
   two mutually exclusive acquisition designs and fix 3 welded half of each together: the
   pre-existing `StreamEnable()` in `openStreamAndPipeline()` (manual design) was left in place
   while `PvAcquisitionStateManager` (the other design) was added on top. For a GigE device
   `StreamEnable()` does exactly one thing — take `TLParamsLocked` — and
   `PvAcquisitionStateManager.h` documents `Start()` as returning `STATE_ERROR` *"if TLParamsLocked
   is already set"*. So the lock was held from connect onward:
   - every grab failed at the first step, 6 retries in ~15 ms each, no image ever requested;
   - and because `TLParamsLocked` freezes the streaming-related nodes, the camera refused to leave
     `Continuous` — logged at connect as
     `JAI camera refused SingleFrame acquisition (… AcquisitionMode SetValue SingleFrame: Node is
     not writable.)`. The warning fix 3 added is what identified this.

   Fixed by dropping `StreamEnable()` (the `StreamDisable()` in teardown stays as a backstop, not
   as its counterpart), and by giving `grabSingleShot()` a stale-lock release before `Start()` —
   nothing in `Start()`'s failure path clears the lock, so one interrupted grab would otherwise
   make every later grab fail identically until the camera was reconnected. `Stop()` is now
   conditional on the state, since in `SingleFrame` the manager releases the lock itself after the
   one expected frame. Verified against the SDK's own samples: `PvPipelineSample` uses
   `StreamEnable` + bare commands and no manager; `eBUSPlayerSample` uses the manager and never
   calls `StreamEnable`.

   Confirmed working in the same log: after the frame rate was set to 5 fps, the exposure maximum
   moved `44730 → 199892 µs` — fix 4's write-frame-rate-first and re-read-after-apply behaving as
   designed. Also deduplicated the `invalid IP configuration` warning, which was emitted three
   times per connect for the one camera at 192.168.5.71 (same per-adapter cause as defect 2, in
   the connect path rather than the dialog).

**The fourth hardware run (2026-08-27) closed it out — and changed how this device is tested:**

6. **A hardware-in-the-loop test now exists:** `tests/jai_camera_hardware_test`. It drives the
   shipped `JaiGigECamera` against the real camera and asserts on actual pixels (10 grabs, frame
   size, channel count, non-uniform image), skipping cleanly where no camera answers. Everything
   below was found and measured with it, without a single round trip through the UI. Paired with
   `JaiGigECamera::streamDiagnostics()`, which logs `GevSCPD`, packet size and block/error counts
   automatically whenever a grab fails.

7. **Two real defects behind "single shot keeps failing", and one wrong turn worth recording.**

   **(a) A damaged frame was waited out instead of re-armed.** In `SingleFrame` the camera sends
   exactly one frame per `AcquisitionStart`; the grab loop discarded an incomplete one and kept
   waiting for a successor that could never arrive. Measured: one discard at ~120 ms, then 2.9 s
   of waiting, every grab. `grabSingleShot()` now has an outer re-arm loop, kept off the
   `Continuous` path where waiting genuinely is the recovery.

   **(b) The camera was never paced.** Symptom was `RESENDS_FAILURE` on **every** block, which
   names packets — so jumbo frames and the cable were the obvious suspects, and both were already
   fine (8976-byte packets negotiated successfully). The cause was `GevSCPD = 0`: this camera
   pushes a 5 MP frame onto the wire as one burst at line rate and the adapter drops what it
   cannot absorb. Measured, 10 grabs per row at 4000-byte packets:

   | GevSCPD | link use | grabs | incomplete blocks | mean |
   |---|---|---|---|---|
   | 0 | 100% | **0/10** | 118 of 118 | 3031 ms |
   | 800 | 71% | 10/10 | 15 of 25 | 609 ms |
   | 1495 | 57% | 10/10 | 13 of 23 | 561 ms |
   | 3000 | **40%** | 10/10 | **0 of 10** | **239 ms** |
   | 6000 | 25% | 10/10 | 0 of 10 | 245 ms |
   | 20000 | 9% | 10/10 | 0 of 10 | 517 ms |

   Hence `kTargetLinkUtilisation = 0.40`. Pacing makes grabs *faster*, because a frame that
   arrives once beats a frame retried until the timeout. Also: `GevSCPD` is stored **on the
   camera** and survives power cycles, so it is written unconditionally — a value left behind by
   another application would otherwise silently become this station's behaviour.

   **(c) The wrong turn.** An intermediate fix concluded the path could not carry jumbo packets
   and added automatic packet-size stepping with a verification grab. It was **removed**. Held at
   constant link utilisation, packet size made no difference worth having (8976 → 1 error,
   8192 → 0, 4000 → 7 — all inside run-to-run noise), and probing decides on noise: one run
   settled on 2048, which measured worst of all. The evidence for it came from single-grab
   samples taken while a stale `GevSCPD` from an earlier experiment was still on the camera — a
   confound that only became visible once the 2×2 was run with both variables controlled.

   Final state, three consecutive runs: **10/10 grabs, 0 incomplete blocks, ~249 ms mean**, at the
   negotiated 8976-byte packet size.

The owner also confirmed a camera on an unreachable subnet (192.168.5.71) is still discovered,
which is the intended behaviour: `IsConfigurationValid()` separates "visible" from "openable",
the dialog shows *Needs IP assignment*, and selecting it is refused with that reason rather than
failing later at connect.

**Still owner-run:** connect, live exposure/gain change, single grab, cable pull and recovery
against the real camera.

### Task C2 (original scoping): `JaiGigeCfg` and `JaiGigECamera`

**Description:** The camera itself, at Basler parity (Q3): enumerate and open by IP, read IO-line
capabilities and current settings on connect, exposure/gain/frame-rate through the node map,
blocking single-shot grab with optional backlight toggle, `cv::Mat` conversion for colour and
mono, and removal detection on a failed grab.

**Acceptance criteria:**
- [ ] `grabFinished()` is emitted on **every** path, success or failure — `CameraRunner` resolves
      its in-flight command from that signal and a missing emit hangs the runner
      (`camera_basler_gige.h` carries this note for the same reason).
- [ ] Unsupported pixel formats leave the `cv::Mat` empty and log, rather than producing garbage.
- [ ] A pulled cable eventually publishes `LostConnected` so recovery starts.
- [ ] The config publishes itself via `setDeviceConfig(&m_config)` in the constructor and
      overrides `fromJson()` if loading changes anything derived — the two traps recorded in
      `src/device/AGENTS.md`, both of which have already bitten this codebase once.
- [ ] Grab timeout is configurable and stays **below** `CameraRunner::kSingleShotTimeoutMs`.
- [ ] `kDisplayNameSources[]` present; `totalNames` raised.

**Verification:**
- [ ] Umbrella build; contract test
- [ ] Owner-run with the real JAI camera: connect, live exposure/gain change, single grab, cable
      pull and recovery

**Dependencies:** C1
**Files likely touched:** `src/device/camera/camera_jai_gige.{h,cpp}`, `jai_define.h`,
`src/device/device.pri`, `tests/architecture_contract_test/main.cpp`
**Estimated scope:** L — split into "connect + settings" and "grab + IO + recovery" if it does not
fit one session.

---

### Task C3: Registry, enum, factory, persistence — ✅ DONE (2026-08-27)

`CameraType::JaiGigE` + the **`"Jai_GigE"`** token (frozen from the first save), a
`createJaiGige` registry entry placed **after Basler and before Virtual Camera**, and the marker
table. `DeviceFactory` needed no change — it dispatches purely through the registry.

**Verified by the contract test:** registry order is now asserted as
`Basler_GigE, Jai_GigE, Virtual` (the virtual entry is still last, which is the load-bearing
part), the entry is findable, and `JaiGigeCfg` round-trips through JSON including the
fractional gain, the I/O-line list and the invert flag. One extra case: a project saved
**before** the `GrabTimeoutMs` key existed must load the default, not 0 — a zero timeout makes
every grab fail instantly with no clue that a missing JSON key is why.

### Task C3 (original scoping): Registry, enum, factory, persistence

**Description:** `CameraType::JaiGigE` + the `"Jai_GigE"` JSON token, registry entry (before the
virtual camera), factory dispatch, marker tables.

**Acceptance criteria:**
- [ ] Appears in the Add Device wizard; creates and persists.
- [ ] `Virtual Camera` stays last in the camera family.
- [ ] The token is final on first write.

**Verification:** umbrella build; contract test; owner creates one from the wizard
**Dependencies:** C2
**Files likely touched:** `src/device/camera/camera_device.h`, `src/device/device_registry.cpp`,
`src/device/device_factory.cpp`, `tests/architecture_contract_test/main.cpp`
**Estimated scope:** S

---

### Task C4: `JaiCameraWidget` — ✅ DONE (2026-08-27), hardware acceptance pending

`jai_camera_widget.{h,cpp,ui}` and `jai_cam_select_dialog.{h,cpp,ui}`, plus the
`DeviceWidgetFactory` camera arm converted from an **equality rejection to an exhaustive
switch** — the same defect the PLC arm had, where every camera sub-type added after Basler would
have silently got "No configuration panel available for this device".

The select dialog was warranted: without it the operator has to know the camera's IP, and eBUS
discovery is the only way to find it. It reports "Needs IP assignment" for a camera that is
discoverable but on the wrong subnet — visible to discovery, impossible to open — and names the
firewall when nothing is found at all.

> ⚠️ **Written as a near-sibling of `BaslerCameraWidget`, not as a unification.** They now differ
> in three things (config type, exposure enum, select dialog) while ~450 lines of device-info
> browser, connect/trigger controls and the whole calibration workflow exist twice. Unifying
> would have rewritten the panel of a camera **already running in production**, with no
> widget-level test to catch a regression, in the same change that introduces a second camera
> family. Recorded as Medium-priority debt in
> `docs/backlog/technical_debt_and_next_steps.md`, with the four Basler defects the JAI copy
> already fixed listed there so the unified version keeps the right behaviour:
> the connection lamp that can never go green, the shadowed `GrabResult`, the empty Save handler,
> and the selection check written `(row < 0) && (row >= size)`.

**Verification:** umbrella build clean (both shells relinked); `update_translations.ps1` added
**128** new source texts with **0** newly vanished (29 → 29). Owner drives the widget against the
real camera.

### Task C4 (original scoping): `JaiCameraWidget`

**Description:** Device widget following `BaslerCameraWidget`, plus a camera-select dialog if JAI
enumeration warrants one (Basler has `basler_cam_select_dialog`), and `DeviceWidgetFactory`
dispatch — the camera arm has the same equality-rejection problem as the PLC arm.

**Acceptance criteria:**
- [ ] Live view, exposure/gain controls, calibration entry point and IO/backlight controls work
      the way the Basler widget's do.
- [ ] All labels translatable and present in the `.ts`.
- [ ] The factory's camera arm handles both real camera types and still logs clearly for unknown
      ones.

**Verification:** umbrella build; `update_translations.ps1`; owner drives the widget against the
real camera
**Dependencies:** C3
**Files likely touched:** `src/ui/forms/camera/jai_camera_widget.{h,cpp,ui}`,
`src/ui/forms/device_widget_factory.cpp`, `src/ui/ui.pri`,
`app/translations/ncr_picking_ja_JP.ts`
**Estimated scope:** M

---

## Phase C-2 — Continuous Shot (live view) for the JAI Camera

**Requested by the owner 2026-08-27**, after single-shot grab was confirmed working against the
real camera.

### What this is for, and what it is not

A **commissioning aid**: the operator needs a live image to set focus, aperture, working distance
and lighting, which single-shot makes painful. It is not a runtime feature — the localization
runtime triggers grabs on PLC handshake and must never see a free-running camera.

### Current state (verified against source, 2026-08-27)

Continuous shot is **entirely unimplemented, everywhere** — this is greenfield, not a port:

| Layer | State |
|---|---|
| `CameraDevice` | `startContinuousShot()` / `stopContinuousShot()` are pure virtuals |
| `BaslerGigECamera` | `return false;` / `{}` — no reference implementation to copy |
| `JaiGigECamera` | `return false;` / `{}` |
| `VirtualCameraDevice` | `return true;` / `{}` — a lie, but harmless today |
| `CameraRunner` | No continuous command kind; `DeviceCommandKind` has only Connect/Disconnect/CameraSingleShot/CameraApplyParams |
| Both camera widgets | A `btn_auto_shot` labelled **"Continuous shot"** already exists in both `.ui` files, wired to nothing (the Basler one has the `connect()` commented out) |

**Nothing calls any of these four methods.** They are dead virtuals, so there is no behaviour to
preserve and no caller to keep compatible.

### Architecture decisions for this phase

**1. Continuous frames must NOT travel on `grabFinished`.** `CameraRunner`'s contract is "exactly
ONE outcome per command", and `onGrabFinished()` resolves the active command and applies the
single-shot retry budget. A stream of frames arriving there would resolve commands that are not
running and spend a retry budget that is not theirs. Continuous frames get their own signal
(`continuousFrameReady`), and the start/stop commands resolve on the *start/stop* succeeding —
not on a frame.

**2. The frame pump must not block the device thread.** The device lives on the runner's worker
thread and is driven by queued signals. A `while (streaming) { RetrieveNextBuffer(...) }` loop
would starve that thread's event loop, so the **stop command could never be delivered** — the only
way out would be killing the task. The pump re-posts itself with a zero-delay queued call, so
every frame is a separate trip through the event loop and stop is always deliverable.

**3. Continuous is bandwidth-bounded, and the bound is already known.** Pacing holds the link at
`kTargetLinkUtilisation` (40%) ≈ 50 MB/s. One 2560×2048 Mono8 frame is 5.24 MB, so the paced
ceiling is **~9.5 fps**. Left free-running at the sensor's 22 fps the camera reproduces exactly
the defect Task C2 finding 7(b) fixed: every block incomplete. Continuous therefore caps
`AcquisitionFrameRate` to the paced budget for its duration and restores it on stop.

**4. Single-shot stays authoritative.** Starting a single shot while continuous is running stops
continuous first. The alternative — rejecting the grab — would make calibration Detect fail with a
confusing error whenever live view happened to be on.

**5. `startAutoContinuousShot()` / `stopAutoContinousShot()` are left alone.** No caller, no
defined meaning distinct from the pair above, and inventing one would be a second abstraction
before the first has a user. Recorded in `docs/backlog/later_todo_list.md` instead.

**Assumptions** (stated rather than asked, both reversible): continuous frames feed **only** the
image view — calibration Detect and the threshold dialog keep using single-shot, so a tuning
result is always traceable to one identified frame. And continuous is JAI-only for now; the
Basler device keeps returning false, and `DeviceWidgetFactory` already dispatches per family.

---

### Phase C-2 status — ✅ C5–C8 DONE (2026-08-27), owner acceptance pending

| Task | State | Evidence |
|---|---|---|
| C5 device | ✅ | `jai_camera_hardware_test` 14/14 against the real camera |
| C6 runner | ✅ | `architecture_contract_test` **79 → 82** |
| C7 widget | ✅ | umbrella build clean; owner acceptance pending |
| C8 test coverage | ✅ | 4 new hardware cases |

**Measured on the GO-5000M-PGE:** 16 frames in 3.0 s (**5.3 fps**, the operator's configured 5 fps
— under the ~8.6 fps paced ceiling, so the cap correctly left it alone), **0 incomplete frames**,
and `stopContinuousShot()` honoured from another thread in **12 ms**.

**Deviation from the plan, and why.** Decision 4 said single-shot pre-emption belongs in the
runner. It went in the **device** instead (`grabSingleShot()` stops streaming first): the runner is
not the only caller, and a guarantee that holds only when one particular caller remembers it is
not a guarantee. The runner-level behaviour the plan asked for still holds, and is tested.

---

### Task C5: Continuous acquisition on `JaiGigECamera` — ✅ DONE (2026-08-27), boxes reconciled 2026-09-03

**Description:** Implement `startContinuousShot()` / `stopContinuousShot()` on the device. Switch
`AcquisitionMode` to `Continuous` for the duration and back to `SingleFrame` on stop, cap
`AcquisitionFrameRate` to the paced bandwidth budget, run a self-posting frame pump that emits
each frame, and guarantee teardown from every exit (stop, disconnect, link loss, `deviceTerminate`).

**Acceptance criteria:**
- [x] `startContinuousShot()` returns false and logs when not connected
      (`camera_jai_gige.cpp:1874-1879`). **Deviates deliberately on the second half:** a start
      while already streaming returns **true**, logs at DEBUG and re-reports the state
      (`:1880-1886`), rather than failing. Refusing loudly would make a double click look like a
      fault, and the widget cannot always know the device's state. The "reject a second Start"
      behaviour the criterion wanted lives one layer up, in C6's runner queue policy, which is
      where a *command* can be answered `Busy` without lying to the device layer.
- [x] While streaming, frames are emitted on a **new** `continuousFrameReady(GrabResult)` signal;
      `grabFinished` is **not** emitted for them (`camera_device.h`, contract test
      `test_camera_runner_continuous_frames_never_resolve_a_command`).
- [x] The device thread stays responsive: `stopContinuousShot()` takes effect within one frame
      period, proven by a test that stops from another thread while streaming.
- [x] `AcquisitionMode` returns to `SingleFrame` and `AcquisitionFrameRate` to its previous value
      on stop, on disconnect, and on link loss — `test_single_shot_works_immediately_after_continuous`.
- [x] Auto-backlight, when enabled, is switched on once at start and off once at stop — not
      toggled per frame (`camera_jai_gige.cpp:1897-1901`, with the strobe/wear rationale in place).
- [x] Incomplete frames are dropped and counted, not emitted; the count is logged on stop, not
      per frame.

**Verification:**
- [x] Umbrella build clean
- [x] `jai_camera_hardware_test` extended (Task C8) and green against the real camera — **19 cases**
- [x] Manual: owner-confirmed live view 2026-09-01 and again 2026-09-03, stable

**Dependencies:** C2 (done)
**Files likely touched:** `src/device/camera/camera_jai_gige.{h,cpp}`,
`src/device/camera/camera_device.h` (the new signal)
**Estimated scope:** M

---

### Task C6: `CameraRunner` continuous commands — ✅ DONE (2026-08-27), boxes reconciled 2026-09-03

**Description:** Add `CameraContinuousStart` / `CameraContinuousStop` to `DeviceCommandKind`, their
triggers and queue policy in `CameraRunner`, and re-emit `continuousFrameReady` on the GUI-thread
side. Make `CameraSingleShot` stop continuous first (decision 4).

**Acceptance criteria:**
- [x] Both commands resolve exactly once, on the start/stop outcome — never on a frame
      (`camera_runner.h:439-444`; `test_camera_runner_continuous_start_and_stop_each_resolve_once`).
- [x] `continuousFrameReady` is re-emitted without touching `m_activeCommand` or the retry budget
      (`test_camera_runner_continuous_frames_never_resolve_a_command`).
- [x] A `CameraSingleShot` submitted while streaming stops continuous, grabs, and reports the grab
      outcome — one outcome, as the contract requires.
- [x] Queue policy stated and tested: `QueueWhenBusy`, and a Stop while not streaming succeeds as a
      no-op — `test_camera_runner_continuous_stop_succeeds_when_nothing_is_streaming`. The
      idempotent half is exactly what let C5's device layer answer a redundant start with `true`
      instead of an error.
- [x] `Disconnect` stops continuous before disconnecting.

**Verification:**
- [x] `architecture_contract_test` extended with the command-lifecycle cases above, driven through
      `VirtualCameraDevice` — which did stop lying: it now carries a real `m_continuousActive`
      (`virtual_camera_device.h:72,79,149`) instead of returning a bare `true`
- [x] Contract test count recorded in this plan (**83**)

**Dependencies:** C5
**Files likely touched:** `src/runtime/device_command.h`, `src/runtime/camera_runner.h`,
`src/device/virtual/virtual_camera_device.h`, `tests/architecture_contract_test/main.cpp`
**Estimated scope:** M

---

### Checkpoint C-2a: device and runner — ✅ CLOSED 2026-09-02
- [x] Umbrella build clean; all suites green with counts recorded (contract **83**, hardware **19**)
- [x] Continuous start/stop provably leaves the camera able to single-shot —
      `test_single_shot_works_immediately_after_continuous`, against the real camera

---

### Task C7: Live view in `JaiCameraWidget` — ✅ DONE (2026-08-27), boxes reconciled 2026-09-03

**Description:** Wire the existing `btn_auto_shot` as a toggle, display `continuousFrameReady`
frames in the image view, and keep the button's state honest when streaming stops for reasons the
widget did not cause (disconnect, link loss, a single shot).

**Acceptance criteria:**
- [x] The button toggles between "Continuous shot" and "Stop" and reflects the *device's* state,
      not the last click (`onContinuousStateChanged()`, `jai_camera_widget.cpp:501`).
- [x] Frames are dropped rather than queued when the GUI cannot keep up — the
      `m_paintingContinuousFrame` drop flag.
- [x] Save image, calibration Detect and the threshold dialog continue to use single-shot and
      behave correctly with live view on.
- [x] Streaming stops when the widget is destroyed, when the camera disconnects, and when the task
      leaves Commission.
- [x] All new labels translatable and present in the `.ts`.

> **This task wired `btn_auto_shot` and left `btn_baklight_toggle` beside it untouched** — both
> were dead in the Basler widget C4 was copied from, and only one was in scope here. The owner
> found the other on 2026-09-03. See **Task C9**.

**Verification:**
- [x] Umbrella build; `update_translations.ps1` with 0 newly vanished
- [x] Owner drives live view against the real camera: focus/aperture adjustment visibly usable
      *(confirmed 2026-09-01, re-confirmed stable 2026-09-03)*

**Dependencies:** C6
**Files likely touched:** `src/ui/forms/camera/jai_camera_widget.{h,cpp}`,
`app/translations/ncr_picking_ja_JP.ts`
**Estimated scope:** S

---

### Task C8: Hardware-test coverage for continuous — ✅ DONE (2026-08-27), boxes reconciled 2026-09-03

**Description:** Extend `tests/jai_camera_hardware_test` to cover streaming against the real
camera. This is the task that makes C5's acceptance criteria checkable without the UI.

**Acceptance criteria:**
- [x] A case that streams for a fixed window and asserts a **minimum frame count** and that every
      emitted frame is a non-empty `cv::Mat` of the expected size.
- [x] A case that asserts stop is honoured promptly from another thread.
- [x] A case that asserts a single shot succeeds immediately after continuous stops — the
      regression that mode/frame-rate restoration exists to prevent.
- [x] Skips cleanly with no camera, like the existing cases.

**Verification:** [x] the suite is green against the real camera (**19 cases**), with frame rate
and discard count reported in the test output
**Dependencies:** C5
**Files likely touched:** `tests/jai_camera_hardware_test/main.cpp`
**Estimated scope:** S

---

### Checkpoint C-2: continuous shot complete — ✅ CLOSED 2026-09-02
- [x] Umbrella build clean; all suites green with counts recorded
- [x] **Owner-confirmed with the real camera (2026-09-01):** live view usable for focus/lighting,
      stop works, single shot still works afterwards. *Calibration is NOT covered by this
      confirmation* — it is tracked under Checkpoint C, which stays open for it.
- [x] Docs updated: `src/device/AGENTS.md` (the two threading traps above)
- [x] `docs/backlog/later_todo_list.md` item 49: the unimplemented `startAutoContinuousShot()`
      pair, and continuous for the Basler camera

---

### Risks specific to Phase C-2

| Risk | Impact | Mitigation |
|---|---|---|
| Frame pump starves the device thread; stop undeliverable | High — needs a task kill | Self-posting queued call, one frame per event-loop trip (decision 2); test stops from another thread |
| Free-running exceeds the paced link budget, every frame incomplete | High — repeats the C2 7(b) defect | Cap `AcquisitionFrameRate` to the paced ceiling (decision 3) |
| Continuous frames resolve single-shot commands | High — corrupts the runner contract | Separate signal, never `grabFinished` (decision 1) |
| Streaming survives a task leaving Commission | Medium — camera runs unattended | Stop on disconnect, link loss and `deviceTerminate`; asserted in C5 |
| 5 MP frames flood the GUI thread | Medium — UI stutters | Drop-if-busy in the widget (C7) |

---

### Task C9: Wire the backlight toggle button — ✅ DONE (2026-09-04)

**Outcome.** Both shells relink clean. `architecture_contract_test` **83 → 86**,
`jai_camera_hardware_test` **19 → 21**, all six suites 0 failed, translations +4 new with
**0 newly vanished** (29 → 29). The two new hardware cases ran **against the real camera**, not
skipped.

> **Note on re-running these numbers.** They were measured with no application instance running.
> A later run on the same tree showed three anomalies, all environmental, none a regression:
> `test_both_shells_take_the_same_instance_key` fails while `ncr_picking.exe` is open, because
> the running app legitimately holds the single-instance guard the test tries to acquire;
> `jai_camera_hardware_test` skips 18 of 21 for the same reason one layer down — a GigE camera
> accepts one control connection, and the open app has it; and
> `test_disconnect_notice_on_graceful_close` is the long-standing flake recorded as
> `docs/backlog/later_todo_list.md` item 32 (~2 runs in 3, reproduces in isolation, diagnosed to
> the product side in Phase 5). **Close the app before trusting a suite run**, and expect item 32
> either way.

**How the design question was answered.** The button is a manual override and it **wins** over
auto-backlight, as proposed below. `JaiGigECamera::setBacklightOverride()` takes the lamp; while
held, the automatic sequence does not touch it.

The mechanism that makes that reliable is worth stating, because it is not the obvious one. There
were five places driving the lamp automatically — around a single shot, and around the start,
failed start and stop of a stream — each repeating the same
`m_autoBacklightControl && !m_autoBacklightLine.isEmpty()` guard. Rather than adding an override
test to all five, they now route through a single `setAutoBacklightState()`, which is the only
automatic path to the lamp. **A future grab path cannot forget the override, because it has no
other way to reach the output.** That function also returns whether it actually drove the lamp,
which is what lets the settle delay be owed only when this grab is what switched it on — under an
override the lamp has been lit since the operator pressed the button, and sleeping again would add
`autoBacklightDelay` to every trigger for nothing.

Two things fell out of the implementation that the scoping did not anticipate:

- **The override is released in `releaseSdkObjects()`**, not in the disconnect path, because that
  is the one function both a clean disconnect and a lost link pass through. A camera reconnected
  with the flag still set would suppress its own auto-backlight for every grab with nothing on
  screen explaining why — and after a power cycle the lamp is off anyway, so the flag would not
  even be telling the truth.
- **The acceptance criterion "disabled while disconnected, like the other camera actions" had a
  false premise.** The sibling buttons are *not* disabled; they guard inside their slots and do
  nothing visible when disconnected. That is the same "looks live, is not" shape this task exists
  to fix, so the backlight button is genuinely disabled via `applyConnectionVisual()` and the
  siblings were left alone as out of scope. Recorded in `docs/backlog/later_todo_list.md`.

**Verification actually performed:**
- [x] Contract test: `test_camera_runner_refuses_backlight_on_a_camera_with_no_io_port`,
      `test_camera_runner_backlight_commands_resolve_once_on_the_reported_level`,
      `test_camera_runner_backlight_reports_never_resolve_a_grab`.
- [x] Hardware: `test_backlight_override_drives_the_lamp_and_reports_it`,
      `test_backlight_override_survives_a_grab` — **21 passed, 0 skipped** at 192.168.4.2.
- [x] **Negative-checked, all three claims.** Removing the `hasIOPort()` refusal and making
      `onBacklightStateChanged()` resolve any active command turned exactly the two matching
      contract cases red (86 → 84 passed, 2 failed). Removing the override's precedence in
      `setAutoBacklightState()` turned the hardware case red with the camera's own diagnostics:
      *"the grab switched the lamp off despite the override: … UserOutputValue=0"*. All three
      restored and re-verified green.
- [ ] Owner-run: press the button on the real camera and confirm the lamp follows, and that a grab
      with auto-backlight enabled does not fight the override. The automated cases assert the
      register state; only the owner can confirm the lamp itself and that the button reads right.

---

### Task C9 (original scoping): Wire the backlight toggle button — found at Checkpoint C, 2026-09-03

**Description:** `btn_baklight_toggle` is declared in `src/ui/forms/camera/jai_camera_widget.ui:174`,
is enabled, and is connected to **nothing**. `jai_camera_widget.cpp` wires nine controls
(`:236-255`) and this is not one of them; no `btn_backlight_toggle_clicked` slot exists. The
operator presses a live-looking button and the light does not change.

**This is not a missing `connect()` line.** The device side is complete — `setBacklightState(bool)`
drives `UserOutput0` through the line resolved by `configureBacklightOutput()`, and the owner
confirmed auto backlight works on hardware. What is missing is the **middle layer**: `CameraRunner`
exposes only `requestConnect/Disconnect/SingleShot/ApplyParams/ContinuousStart/ContinuousStop`
(`camera_runner.h:108-148`) and `DeviceCommand` has no backlight kind. The widget runs on the GUI
thread and the camera on a worker thread, so it cannot call the device directly; the button needs
a command of its own, the same shape C6 built for continuous.

**Inherited, not introduced.** The same button is dead in `basler_camera_widget.ui:174`, where the
wiring is explicitly commented out (`basler_camera_widget.cpp:243-244`) next to the commented-out
`btn_auto_shot`. C4 copied the Basler widget including this gap; C7 wired the auto-shot half and
left this one. Basler is outside Phase 8 — see the deferral note below.

**The design question this task must answer first: what does the button mean when auto backlight
is on?** The two behaviours are not the same feature:
- **Manual override of the light** (`setBacklightState`) — a commissioning aid, like live view:
  the operator turns the lamp on to set exposure and see the part. But with
  `autoBacklightControl` enabled, the next grab's auto sequence switches the lamp back, so the
  button appears to work and then silently undoes itself.
- **Toggling `autoBacklightControl` itself** — a config edit, which the property browser already
  offers, making the button redundant.

Proposed resolution, to be confirmed at review: the button is a **manual override, and it takes
precedence** — pressing it sets an override flag that suppresses the auto sequence until released,
and the button's visual state reports the lamp's actual state, driven by the device, the way
`onContinuousStateChanged()` reports streaming (C7 decision). A button that reports the last click
rather than the hardware is the defect this checkpoint just found in another form.

**Acceptance criteria:**
- [ ] A `CameraBacklightSet` command kind on `DeviceCommand` and a `requestBacklight(bool)` on
      `CameraRunner`, resolving exactly one outcome per command like every other kind.
- [ ] The button drives the runner, never the device, from the GUI thread.
- [ ] Button state is repainted from a device-reported signal, not from the click.
- [ ] Precedence against `autoBacklightControl` implemented as reviewed, and stated in a comment
      at the point where the auto sequence checks the override.
- [ ] Disabled while the camera is disconnected, like the other camera actions.
- [ ] The `.ui` object name typo `btn_baklight_toggle` → `btn_backlight_toggle` fixed in the JAI
      form (a one-file rename, since nothing references the old name).

**Verification:**
- [ ] Contract test: the backlight command resolves once, and is refused with a visible reason on
      a camera whose `hasIOPort()` is false. `VirtualCameraDevice` is the only such camera today
      (`virtual_camera_device.h:46`; both GigE cameras return true), which makes it the test
      vehicle — and means the refusal path has no production camera exercising it, so the test is
      the only thing that will keep it honest.
- [ ] `jai_camera_hardware_test`: toggle on, read the line back, toggle off, read back.
- [ ] Owner-run: the lamp responds to the button on the real camera, and a grab taken with auto
      backlight enabled does not fight the override.

**Deferred deliberately:** the identical dead button and dead `btn_auto_shot` in
`BaslerCameraWidget`. Fixing them here would mean editing the panel of a camera **already running
in production** for a commissioning convenience, and the two widgets are scheduled to be unified
(`docs/backlog/technical_debt_and_next_steps.md`). Recorded in
`docs/backlog/later_todo_list.md` so the unified widget inherits the working version rather than
the commented-out one.

**Dependencies:** C6 (the command/runner pattern this follows)
**Files likely touched:** `src/runtime/device_command.h`, `src/runtime/camera_runner.h`,
`src/ui/forms/camera/jai_camera_widget.{h,cpp,ui}`, `tests/architecture_contract_test/main.cpp`,
`tests/jai_camera_hardware_test/main.cpp`
**Estimated scope:** S

---

### Checkpoint C: JAI thread complete
- [x] Umbrella build clean (both shells); contract test green (**86**); all other suites green
      (`mc_frame_test` 38, `modbus_device_test` **21**, `vision_output_device_test` 9,
      `vision_tcpip_client_device_test` 10, `jai_camera_hardware_test` **21**, 0 skipped)
      *(counts as measured 2026-09-04; the vision suites report one more than the 8/9 recorded
      earlier in this plan — nothing in either was touched, and the older figures came from a
      previous build layout's result files)*
- [x] Translations swept: +128 then +4 new source texts, 0 newly vanished (29 → 29)
- [x] **Owner-confirmed with the real camera (2026-09-03):** connect, grab, live view, parameter
      change, **auto backlight control**, and reconnect — *"hoạt động ổn định"*. Reconnect covers
      the 2026-09-02 power-cycle defect (a camera that came back at factory defaults because
      `deviceConnect()` only read the node map and never pushed the config); the fix holds on
      hardware.
- [x] **Calibration through the JAI widget** — owner-confirmed 2026-09-04: *"calibration của cam
      jai làm từ widget jai"*. The board workflow was driven through this widget against the real
      camera, and the resulting calibration is what the 2026-09-03 runtime run ran on. This was
      the oldest open item on Checkpoint C and the last one blocking B's localization cycle.
- [x] **Defect found at this checkpoint (2026-09-03): the backlight toggle button does nothing.**
      Fixed — **Task C9**, done 2026-09-04, verified on the real camera.
- [x] Docs updated: `src/device/AGENTS.md`, `docs/architecture/device_type.md`, **`uml/`**
      *(done 2026-09-07; before this, no diagram mentioned Modbus or JAI at all)*

      Three diagrams were stale, and one of them was actively misleading:
      - **`02_device_families.puml`** — `JaiGigE` added to `CameraType`; `ModbusTcpClient`/
        `ModbusTcpServer` to `PlcType`; `JaiGigeCfg`/`JaiGigECamera` and the six Modbus classes
        added with their inheritance edges. **The important edge is
        `IResultOutputDevice <|.. ModbusTcpServerDevice`**: the diagram previously showed that
        interface reaching only `VisionOutputDevice`, which is precisely the assumption that
        cost Checkpoint B its 2026-09-01 blocker. The diagram was teaching the bug.
      - **`03_runtime_threading.puml`** — the four new `DeviceCommandKind` values,
        `CameraRunner`'s continuous and backlight API, and `PlcRunner`'s per-device
        `supportsResultOutput()`.
      - **`06_ui_widgets.puml`** — showed 3 of the 7 device widgets the factory actually
        builds; now all seven, plus the exhaustive-switch note.

      The other six diagrams were checked and genuinely needed no change: none of them names a
      concrete camera or PLC sub-type. `uml/README.md` gained a currency section recording
      exactly that, so the next reader can tell "reviewed and correct" from "not looked at".

      > **Not rendered.** No PlantUML jar is available on this machine, so the files were
      > checked structurally only (`@startuml`/`@enduml`, `note`/`end note` and brace balance all
      > match). Render them before relying on the images.

---

## Phase F — Commissioning defects (owner-reported 2026-09-07)

Three defects found running the cell, plus the `uml/` gap closed the same day. None was found by
a test suite; all three are the "looks like it worked, didn't" class this project keeps producing.

### Task F1: An unregistered camera or pattern index must FAIL, not stay Ready — ✅ DONE (2026-09-07)

**Outcome.** `architecture_contract_test` **86 → 89**, all suites green, both shells relink clean.
The fix is the one the review predicted: **guard `markRuntimeReady()` against `WaitingTriggerReset`
first**, then give both setters the same shape.

**A regression the existing suite caught immediately, and it is worth recording.** Adding the guard
broke `test_localization_runtime_trigger_cycle_uses_matching_worker_contract`, because the trigger
**falling edge** calls `markRuntimeReady()` *while the state is still `WaitingTriggerReset`* — that
call is the handshake completing, the one legitimate exit from the state, and the new guard refused
it. The fault branch beside it already cleared the state before arming its own way out
(`m_cycleState = Recovering`); the success branch simply never had to. It now sets `NotReady`
before re-arming, so `markRuntimeReady()`'s validation decides whether the runtime is actually
ready rather than the handshake declaring it so.

**Verification:**
- [x] `test_localization_unregistered_camera_number_faults_and_stays_faulted`
- [x] `test_localization_unregistered_pattern_group_faults_and_stays_faulted`
- [x] `test_localization_recovers_when_a_valid_index_follows_a_rejected_one` — the objection-1 case
- [x] **Negative-checked:** removing the two `Faulted`/`runtimeFault` transitions turned exactly
      the two rejection tests red (89 → 87 passed, 2 failed) and left the recovery test green.
      Restored and re-verified.

---

### Task F4: Zero was never a contract value — ✅ DONE (2026-09-07)

**Owner, after accepting F1:** *"bạn đã bỏ qua trường hợp number của nActiveCamera và
nActivePatternGroup thay đổi thành 0, nên khi start lên mặc dù chọn index = 0 nhưng task vẫn ready…
hãy xác nhận điều này có trong contract hay không hay chỉ vô tình làm."*

**The answer is that it was accidental, and five independent lines of evidence say so.**

1. **No document defines 0.** `plc_signal_contract.md:20-21` gives only "Active logical camera
   number", with no reserved value. The doc discusses 0 elsewhere, but only for *output* signals
   (`nDetectedNumber`, `nFaultCode`). Nothing in `task_localization.md`,
   `runtime_controller_api.md`, any phase plan, or any source comment mentions it.
2. **0 can never be a registered index, by validation.** Cameras are **1..16**
   (`TaskDeviceBinding::kMinCameraNumber`, and `fromJson()` *rejects* a saved binding outside it);
   pattern groups are **1..32** (`MatchGroup::validateIndexRange()`, enforced by the pattern
   manager). So 0 was exactly as unregistered as the number 7, which faulted correctly.
3. **The "unset" sentinel in this design is `-1`, not 0.** `m_activeCameraNumber{-1}` — "-1 if
   unset"; `setup()` tests `< 0`; `validateActivePatternGroup()` tests `groupNumber < 0`. Had 0
   meant "no selection", these would test `<= 0`.
4. **The guard existed in one of three entry points, and a sibling documented the opposite.**
   `handlePlcValues()` had it. `TaskLocalization::onSignalChangeCameraNumber()` /
   `onSignalChangePatternNumber()` — also PLC-driven — call the setter *"with the result
   regardless"*, in their own doc comments. The manual `queueSetActive*()` path has no guard.
5. **The two branches were written differently from each other** — camera used a bare `toInt()`,
   pattern `toInt(&ok)` plus an `ok` check, on adjacent lines. Accretion, not design.

**What changed.** The guard is gone; `handlePlcValues()` passes both values through unfiltered, and
the setters decide. Each setter now runs a **range check first, registration second**, because they
are different mistakes: 0 or 99 can never name anything, while 3 could but is not bound here. The
ranges come from `TaskDeviceBinding` and `MatchGroup` rather than constants copied into the
controller, so there is one definition of a legal index.

A **non-numeric** value gets its own path, `reportSignalTypeMismatch()`: it means the tag is mapped
to a coil or discrete input, which no index check can diagnose, so the message names the tag.

**One defect found while doing it.** `setActivePatternGroupNumber()` published
`nActivePatternGroup` back to the PLC **before** validating — so a rejected group number was
written onto the master's own command register, reading as the task confirming a selection it had
refused. The camera setter never did that. It now publishes only on the accepted path.

**The contract now says all of this**, in a new "Active Index Signals" section: the two ranges,
the three failure modes and their signal patterns, that 0 is out of range rather than "no
selection", that recovery needs no `bErrorReset`, and that a cell which does not switch camera or
pattern group should leave the signals **unbound** — an unbound signal is never dispatched, which
is the supported way to say "always camera 1". `docs/generated/architecture_docs/` said
*"Camera index (0-based)"* with an example calling `setActiveCameraNumber(0)`; both were corrected.

**Verification:**
- [x] `test_localization_zero_active_index_is_refused_by_the_range_check`
- [x] `test_localization_out_of_range_active_index_is_refused`
- [x] `test_localization_non_numeric_active_index_reports_the_mapping_not_the_value`
- [x] **Negative-checked, and the first attempt was not good enough.** With both range checks
      disabled, only the out-of-range test went red: 0 still faulted, via the registration check.
      So the zero test was asserting "it faulted" and pinning nothing. Strengthened to assert the
      *reason* names the range — after which both tests go red (92 → 90 passed, 2 failed) and pass
      again on restore.

**Files:** `src/model/localization_runtime_controller.{h,cpp}`,
`docs/domains/task_localization/plc_signal_contract.md`,
`docs/generated/architecture_docs/LocalizationRuntimeController.md`,
`tests/architecture_contract_test/main.cpp`
**Scope:** S

---

### Task F5: A refused index was forgiven by the *other* index — ✅ DONE (2026-09-07)

**Reported by the owner, from the cell**, after F4 shipped: set camera 1 (Ready), set camera 0
(fault — correct), then set pattern group 0 (fault) and back to 1 — and the task returned to
**Ready with `bCameraValid` true**, while the master's own `nActiveCamera` register still held the
0 the task had refused. They asked whether this matched the contract, and for a full audit of the
runtime controller against it.

**It did not match the contract, and the log proves the sequence.** `build/bin/release/logs/
app_log_2026-09-07.txt:5801` records *"Ready -> Faulted (Camera number 0 is outside the valid range
1..16.)"*, and `:5807`, three seconds later, *"Faulted -> Ready (Active pattern group changed.
Runtime ready.)"* — with the Modbus trace on either side showing `DI00000` (`bCameraValid`) going
false and then back to true while `HR00000` stayed at 0.

**Root cause.** A refused number is deliberately never adopted — the last good camera stays bound,
which is what lets a later valid write recover with no operator action. The cost of that choice is
that the refusal leaves **nothing behind for `markRuntimeReady()` to see**: its gate reads
`allRequiredRolesHealthy()`, `validateActivePatternGroup()` and `validateActiveCameraCalibration()`,
and all three still pass because they read the *previous, good* selection. Every re-arm path in the
class funnels through that one function, so the refusal has to be remembered there or not at all.

Fixed with a per-signal latch (`m_activeCameraSelectionRejected`,
`m_activePatternGroupSelectionRejected`), set by every refusal and cleared **only by an accepted
value for that same signal**. `markRuntimeReady()` refuses while either is held. `setup()` clears
both, because a fresh context carries a fresh selection.

This closes the whole class, not just the reported path — the same hole let a **role reconnect** or
a **`bErrorReset`** re-arm a task whose commanded index was still refused. `bErrorReset` now
behaves here exactly as the contract already specifies for `PatternInvalid`: it clears `bTaskFault`
and `nFaultCode` but does not re-arm, because acknowledging is not repairing.

**Second defect, found by the audit rather than reported.** `reportSignalTypeMismatch()` — the
non-numeric path added in F4 — published `bTaskReady`/`bTaskFault`/`nFaultCode` but **not** the
domain flag, so a mis-mapped `nActiveCamera` left `bCameraValid` standing at **true** underneath an
active fault. The contract block lists `bCameraValid=false` for all three failure modes; only two
of them did it.

**Audit of the rest of the controller against the contract.** Trigger handshake, cycle-start /
success / fault output sets, `WaitingTriggerReset` exit, fault auto-recovery arming points,
unbounded role retry and its rate-limited logging all match the document. Two gaps found that are
**not** defects and are the owner's call, filed as backlog **54** (a *held* `bExecuteTrigger` fires
one cycle when the runtime starts, because `setup()` seeds `m_lastExecuteTrigger = false`; the
contract's wording permits it either way and does not say which the cell wants) and **55**
(`setup()` range-checks neither active index, so a hand-built context with camera 0 fails with
*"Active camera calibration is invalid"* instead of naming the number).

**Verification:**
- [x] `test_localization_a_refused_index_is_not_forgiven_by_the_other_index` — the owner's exact
      sequence, including that a trigger does not get through
- [x] `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera` — the mirror
      case. Not symmetric by accident: an in-range but *unregistered* group is committed, so
      `validateActivePatternGroup()` blocks the re-arm on its own; only an **out-of-range** group
      depends on the latch, which is why 0 is the value under test
- [x] `test_localization_error_reset_does_not_lift_a_refused_index`
- [x] `test_localization_non_numeric_active_index_reports_the_mapping_not_the_value` extended to
      pin the missing domain flag and the latch
- [x] **Negative-checked separately, once per fix.** Gate disabled → 4 failed, each on its own
      assertion (95 → 91). Domain-flag publish disabled → 1 failed, on the `bCameraValid`
      assertion (95 → 94). Both green again on restore.
- [x] `architecture_contract_test` **95 passed, 0 failed**; both shells relink clean

**Files:** `src/model/localization_runtime_controller.{h,cpp}`,
`docs/domains/task_localization/plc_signal_contract.md`,
`docs/generated/architecture_docs/LocalizationRuntimeController.md`,
`docs/backlog/later_todo_list.md`, `tests/architecture_contract_test/main.cpp`
**Scope:** S

---

### Task F1 (original scoping): An unregistered camera or pattern index must FAIL, not stay Ready

**Owner:** *"Camera number và pattern number không tìm thấy phải báo failed. Hiện tại đổi part và
camera number sang một index chưa được đăng ký, task vẫn ready, camera valid và pattern valid."*

**Root cause — the camera and pattern setters fail differently, and neither records the failure.**

`setActiveCameraNumber()`'s missing-runner branch (`localization_runtime_controller.cpp:161-169`)
publishes `bCameraValid=false`, `bTaskFault=true`, `nFaultCode=CameraLost`, then **returns before**
`publishBoolSignal("bTaskReady", false)` at `:173`. It never sets `m_cycleState = Faulted`. Compare
the calibration-invalid branch twenty lines below (`:186-192`), which does all of that. So the
rejection is **a signal pulse with no memory**:

- `m_cycleState` stays `ReadyForTrigger`, so a trigger rising edge is accepted (`:413-415`) and the
  cycle runs on the **old** camera.
- `m_activeCameraNumber` is unchanged, so `validateActiveCameraCalibration()` (`:772`) keeps
  returning true and `markRuntimeReady()`'s guards cannot see that a bad index was requested.
- The next `markRuntimeReady()` → `publishInitialReadyOutputs()` (`:667`) republishes
  `bTaskReady/bCameraValid/bPatternValid = true`, `bTaskFault = false` **unconditionally**, wiping
  the fault. `publishCycleStartOutputs()` (`:692-695`) does the same.

`setActivePatternGroupNumber()` (`:212-229`) has the same hole — no `bTaskReady=false`, no state
change — but is otherwise *correct*: `validateActivePatternGroup()` (`:747`) does return false for
an unregistered key, and `startCycle()` re-validates and aborts. The pattern failure therefore
surfaces **one consumed trigger late** rather than never.

**Two fixes were proposed and rejected during review. Both would have broken working machinery:**

1. *"Mirror the camera fix into the pattern setter."* The camera setter pairs its fault with
   `if (allRequiredRolesHealthy()) markRuntimeReady(...)` on the success path (`:203-205`); the
   pattern setter has **no re-arm call at all**. Copying only the fault half means the ordinary
   sequence *"PLC writes a bad group, then the correct one"* strands the runtime Faulted with
   `bTaskReady` false forever — the only exit is an operator `bErrorReset`. That contradicts the
   design rule at `localization_runtime_controller.h:147-151`: *"The runtime must never park on a
   transient fault waiting for an operator."*
2. *"So add `markRuntimeReady()` to the pattern setter too."* `markRuntimeReady()`'s only state
   guard is `m_cycleState == Running` (`:610`), so **`WaitingTriggerReset` passes**. A PLC write of
   `nActivePatternGroup` while `bExecuteTrigger` is still asserted after a completed cycle would
   jump to `ReadyForTrigger` and run `publishInitialReadyOutputs()`, wiping `bMatchingFinished`,
   `bMatchingDetected` and `nDetectedNumber` **before the PLC has read them** — breaking the
   rising-edge handshake that `:1057-1060` and `:1502-1509` go out of their way to protect.

**The fix that survives both objections:** guard `markRuntimeReady()` against `WaitingTriggerReset`
as well as `Running` — the hole exists in the *camera* setter today too, so this closes it once
instead of duplicating it — and only then give both setters the same shape: fault on rejection,
re-arm on acceptance.

**Acceptance criteria:**
- [ ] `markRuntimeReady()` refuses to re-arm while the trigger handshake is outstanding
      (`WaitingTriggerReset`), with the reason stated in a comment at the guard.
- [ ] An unregistered **camera** index publishes `bTaskReady=false`, sets `Faulted`, emits
      `runtimeFault`, and keeps `bCameraValid=false` / `nFaultCode=CameraLost`.
- [ ] An unregistered **pattern** index does the same with `nFaultCode=PatternInvalid`.
- [ ] Writing a **valid** index afterwards re-arms the runtime with no operator action — the case
      objection 1 identified. `m_activeCameraNumber` must NOT be updated to the rejected index:
      leaving the previously-bound camera is what makes this recovery possible.
- [ ] A cycle cannot start on a stale camera after a rejected index.

**Verification:** contract-test cases for both rejections and for the re-arm; each negative-checked.
**Files:** `src/model/localization_runtime_controller.cpp`, `tests/architecture_contract_test/main.cpp`
**Scope:** S

---

### Task F2: The Modbus client tears its own link down on a burst of writes — ✅ DONE (2026-09-07)

**Outcome.** `modbus_device_test` **21 → 22**, all suites green. Both halves landed together,
because F2a alone would have stopped the teardown while still losing nine writes in ten.

- **F2a — the in-flight rejection no longer counts against the retry budget.** A one-shot flag set
  only by that rejection and consumed by the very next `noteTransactionFailure()`, which is safe
  because all seven `transact()` call sites report a failure immediately. `pollOnce()` had always
  treated the same condition as "not now" rather than as a failure; the write path now agrees.
- **F2b — the write is deferred instead of refused.** Parked at the top of
  `writeDigitalIoByName()`/`writeWordIoByName()` when `m_inTransaction` is set — **after**
  validation, so a malformed or out-of-span tag still fails immediately and loudly — and replayed
  by `drainPendingIoWrites()` when the transaction unwinds. The drain is iterative, not recursive:
  a write arriving inside a replayed write's own wait is picked up by the same loop.

**The refuted objection is answered, not ignored.** `sendVisionResult()` calls `transact()` four
times sequentially with `m_inTransaction` false between each, so a naive drain would fire *between
the count write and the payload write* and expose a half-written result block. It now holds
`m_suppressPendingDrain` across all four writes via a scope guard, releasing and draining only once
the block is consistent — the same reasoning, and the same place, as the existing poll-timer stop.

The `.h` contract text was amended in the same change rather than left to drift: a **failed** write
is still never retried and never queued; a write that never reached the wire is a different thing.

**Verification:**
- [x] `test_a_queued_burst_of_writes_all_reach_the_slave_and_keep_the_link` — ten writes posted the
      way `PlcRunner` posts them, so the collisions really happen. Asserts every coil carries its
      own value **and** that the link is still connected afterwards.
- [x] **Negative-checked:** disabling the park left the test red (the burst never reached coil 8)
      and the run took 16 s instead of 0.8 s. Restored and re-verified.

> **A trap for the next person reading this test.** `QModbusServer` stores a coil set by FC5 as the
> protocol's ON word `0xFF00`, not `1`, so the assertion compares truth values. An earlier draft
> compared `== 1` and failed against a coil that was genuinely on — and a draft before that waited
> on a coil whose expected value was `0`, which every coil already is, so the wait passed before
> anything had happened at all.

---

### Task F2 (original scoping): The Modbus client tears its own link down on a burst of writes

**Owner:** *"Về modbus client, trong runtime không thể ghi coil value."*

**Root cause — confirmed, and it is re-entrancy, not the write path.** Every candidate the log
messages suggest was ruled out by inspection: `PlcRunner` **does** wire `sig_writeDigitalIo` queued
(`plc_runner.h:155-161`), `coilCount` defaults to 64 not 0 (`modbus_config.h:179`), and the tag is
literally `format()` output because the mapping editor is populated from
`availableDigitalIoNames()`. None of the four rejection messages in `writeDigitalIoByName()` is the
one being hit.

What actually happens: `transact()` waits for its reply in a **nested `QEventLoop`**
(`modbus_tcp_client_device.cpp:421-426`). `loop.exec(ExcludeUserInputEvents)` defers only *user
input*, so the runner's other queued write meta-calls are dispatched **inside** it. Each re-entrant
write clears all four guards and is refused by the in-flight guard (`:379-383`) with *"A Modbus
transaction is already in flight."* — and `:616` charges that rejection to the **link retry
budget**. `noteTransactionFailure()` (`:471-487`) tears the link down past `m_retryCount` (default
**3**).

The runtime publishes in bursts — `publishInitialReadyOutputs()` alone is 8 bools and 2 words with
no change filter — so **collision #4 declares the link lost**. The client is not failing to write;
it is disconnecting itself, and the peer was never contacted.

No test reaches this: every write test in `modbus_device_test` calls the device directly on its own
thread, so a queued burst never forms.

**The obvious fix was rejected.** *"Park the re-entrant write and drain it when transact unwinds"*
corrupts `sendVisionResult()`, which calls `transact()` four times **sequentially** (`:717, :736,
:757, :768`) with `m_inTransaction` false between each — so each is its own outermost unwind, and a
parked write drains **between the count write and the payload write**. That is exactly the
interleaving the poll timer is stopped to prevent (`:696-708`): a master reading the block mid-drain
sees a half-written result. It also violates the "never queues" contract stated at
`modbus_tcp_client_device.h:86-87` and `:612-613` and pinned by
`tests/modbus_device_test/main.cpp:310-325`.

**Staged fix, safe half first:**
- **F2a — stop the self-inflicted teardown.** The in-flight rejection is *our own scheduler
  colliding with itself*; the socket is healthy and the peer was never contacted. It must not count
  as a transport failure. **Only** that rejection is exempt — a genuine timeout or exception
  response must still count, or a dead link is never declared lost and runtime recovery never
  starts.
- **F2b — make the write actually happen.** Serialize so a burst does not collide at all, with the
  result-publish sequence held atomic. This needs the "never queues" contract amended deliberately
  rather than silently, so it is scoped separately below.

**Acceptance criteria (F2a):**
- [ ] An in-flight rejection does not increment `m_consecutiveFailures` and cannot cause teardown.
- [ ] A real write timeout / exception response still does, with the retry budget unchanged.
- [ ] Test: a burst of queued writes from another thread leaves the link **Connected**.

**Acceptance criteria (F2b):**
- [ ] A burst of queued writes all reach the wire, in order.
- [ ] `sendVisionResult()` stays atomic — nothing interleaves between its four writes.
- [ ] The `.h` contract text and `test_client_write_while_disconnected_fails_rather_than_queues`
      are updated together with the behaviour, not after it.

**Files:** `src/device/plc/modbus/modbus_tcp_client_device.{h,cpp}`, `tests/modbus_device_test/main.cpp`
**Scope:** F2a S, F2b M

---

### Task F3: The Modbus panel shows Mitsubishi addresses — ✅ DONE (2026-09-07)

**Outcome.** All suites green; translations swept with **0 newly vanished** (29 → 29).

`DevicesMonitorWidget::setAddressFormat(prefix, digits)` stores the two values `formatName()` now
reads, defaulted in the constructor to the Mitsubishi scheme this widget was written for. That one
setter fixes the filter box as well, because it searches the same string. `buildRow()` stores the
spelled tag on the address item under a new `DeviceRowDelegate::AddressTextRole`, and
`paintAddress()` draws it when present — **keeping its M/D computation as the fallback**, so the
Mitsubishi panel, which never calls the setter, paints exactly what it painted before.
`ModbusDeviceWidget` calls it with `modbus_tags::prefix(area)` and `kAddressDigits` immediately
before each `setRange()`, and the setter rebuilds rows itself so the header and the address column
cannot disagree after an area switch.

`kAddressDigits` moved from the `.cpp` anonymous namespace into `modbus_register_map.h`. The width
is half the contract: `COIL0100` is not the tag `COIL00100`, so a panel padding to its own width
prints something that binds nothing. Now there is one definition and the UI reads it.

The config labels carry their prefix — *"Discrete Input (DI) Start"*, *"Holding Register (HR)
Count"* — because `DI`/`HR`/`IR` shared **no word** with their old labels, and *"Input Register"*
and *"Discrete Input"* both contained "Input", which is how someone reaches for a `DI` tag when
they wanted `IR`. The `kDisplayNameSources` marker table was updated in the same edit; the contract
test asserts the two agree, and it stays green.

**Nothing persisted changed.** The monitor's write signals carry an int address and the real tag is
still built from it via `modbus_tags::format()`, so saved projects and commissioned signal maps are
untouched — which is exactly why the fix went into the UI rather than into `parse()`.

**Verification:**
- [x] Contract test green (89), including the display-name marker assertions.
- [x] Translations: the 8 old labels went obsolete and 8 new ones took their place, net zero, with
      **0 newly vanished**.
- [ ] Owner-run: open a Modbus panel and confirm the address column reads `COIL00007` / `HR01000`,
      that typing a real tag into the filter finds its row, and that the Mitsubishi panel is
      unchanged. No widget-level test exists in this project, so the painted string is the one
      thing only a person can confirm.

---

### Checkpoint F: commissioning defects closed — ✅ CLOSED 2026-09-07
- [x] `architecture_contract_test` green (**95**); `mc_frame_test` 38, `modbus_device_test` **22**,
      `vision_output_device_test` 9, `vision_tcpip_client_device_test` 9 (+1 known flake, backlog
      32), `jai_camera_hardware_test` 21. Both shells relink clean
- [x] Translations swept: 0 new source texts, 0 newly vanished (29 → 29)
- [x] **Owner-confirmed (2026-09-07):** F2 (Modbus client coil writes in runtime) and F3 (Modbus
      panel address naming) *"hoạt động đúng"*
- [x] **Owner-reported regression on F1/F4 found and fixed the same day — Task F5.** The owner ran
      the sequence the earlier tasks had not covered (refuse one index, then write the *other*) and
      caught the task returning to Ready with `bCameraValid` true. Worth recording as a pattern:
      **every task in this phase was found by the owner running the cell, and F5 was found by the
      owner re-testing a fix.** No suite produced any of them.
- [x] **Owner-confirmed (2026-09-07): F3's painted address column is correct.** The Modbus panel
      shows real tags and the filter finds them. This is the half no test in this project can
      reach — there is no widget-level suite, so the painted string was only ever going to be
      confirmed by a person.
- [x] **Owner-confirmed (2026-09-07): F5 holds the fault correctly on the cell.** The reported
      sequence no longer re-arms the task, and `bCameraValid` stays false until a valid **camera**
      number is written. The fix is confirmed by the same person, on the same machine, running the
      same sequence that found it — which is the strongest evidence available for this defect.
- [ ] ➡️ **CARRIED: owner decision on backlog 54** (a held `bExecuteTrigger` fires one cycle when
      the runtime starts). Not a defect and not blocking — the contract's wording permits either
      reading. It needs a decision about what the cell wants, then a sentence in the contract
      either way.

---

### Task F3 (original scoping): The Modbus panel shows Mitsubishi addresses

**Owner:** *"Hãy đồng bộ tên gọi trên UI và tên gọi của các địa chỉ trong các vùng memory của
modbus."*

**Root cause.** `DevicesMonitorWidget` and `DeviceRowDelegate` were written for the Mitsubishi
device map and hard-code its scheme: `device_row_delegate.cpp:177-187` does
`const QChar prefix = m_mode == Bit ? 'M' : 'D'` with **4** digits, and
`devices_monitor_widget.cpp:185-188` repeats it. `ModbusDeviceWidget` reuses both unchanged and
overrides only the header title, so **the address column of the Modbus monitor reads `M0000`/
`D0000`** — prefixes that are not in the Modbus prefix table at all and that `parse()` rejects
outright. The same `formatName()` drives the filter box, so typing the real tag `HR01000` hides
every row.

Both halves are wrong: the prefix **and** the width — Modbus tags are `format()` = prefix +
**5** zero-padded digits, so even `COIL0100` is an orphan; only `COIL00100` binds.

The config labels are a milder version of the same problem: only *"Coil"* shares a word with its
prefix. `DI`, `HR` and `IR` share **no** token with *"Discrete Input"*, *"Holding Register"* and
*"Input Register"* — and *"Input Register"* and *"Discrete Input"* both contain "Input", which is
how an operator picks a `DI` tag when they wanted `IR`.

**Fix.** One data-only setter — `setAddressFormat(prefix, digits)` — defaulted to today's `M`/`D`
and 4 digits, read by `formatName()` (which fixes the filter) and carried to the delegate through a
role on the address item. `ModbusDeviceWidget` calls it with `modbus_tags::prefix(area)` and 5
before each `setRange()`.

**This is not a speculative abstraction:** Mitsubishi and Modbus are two concrete cases and both are
literally *prefix + zero-padded decimal*, so the shape is observed, not guessed.

**Acceptance criteria:**
- [ ] The Modbus monitor's address column and filter show real tags (`COIL00007`, `HR01000`).
- [ ] The Mitsubishi panel is **byte-identical** — it never calls the setter.
- [ ] `setAddressFormat()` before `setRange()`, or it rebuilds rows itself: after an area switch the
      header must not say `DI` while the rows still say `COIL`, which would be a worse lie than today.
- [ ] Nothing persisted changes — the monitor's write signals carry an int address and the real tag
      is already built from it via `modbus_tags::format()`.
- [ ] Config labels name their prefix, so the panel and the tag vocabulary agree.

**Files:** `src/ui/widgets/plc_widget/devices_monitor_widget.{h,cpp}`,
`src/ui/widgets/plc_widget/device_row_delegate.cpp`, `src/ui/forms/plc/modbus_device_widget.cpp`,
`src/device/plc/modbus/modbus_config.h`
**Scope:** S

---

## Checkpoint audit — 2026-09-02 *(superseded by the 2026-09-03 audit below; kept for history)*

Where every checkpoint actually stands, re-verified against a full build and test run today
rather than against what was ticked when each was written.

**All suites, this run:** `architecture_contract_test` **83**, `mc_frame_test` **38**,
`modbus_device_test` **20**, `vision_output_device_test` **8**,
`vision_tcpip_client_device_test` **9**, `jai_camera_hardware_test` **19** — 0 failed.

| Checkpoint | State | What is left |
|---|---|---|
| A-1 transport and contexts | ✅ closed | — |
| A MC thread | ⛔ **open** | Owner-run on the real PLC: 3E no regression, 1C and 3C connect and poll. Also the MC domain doc. |
| B-1 role model | ✅ closed | — |
| B Modbus thread | ⛔ **open** | The one-device-both-roles cycle, and the robot pick check re-verified on it. |
| C JAI thread | ⛔ **open** | Calibration against the real camera; `uml/` diagrams. |
| C-2a device and runner | ✅ **closable** | Everything verified — see below. |
| C-2 continuous shot | ✅ **closable** | Owner confirmed live view 2026-09-01. |
| Phase 8 complete | ⛔ open | Blocked on A, B and C above. |

**Automated evidence is complete.** Every "umbrella build clean / suites green / translations
swept" box across all checkpoints is satisfied by this run. What remains is, without exception,
**owner-run against hardware** — plus two documentation gaps listed below.

### Closable now

- **Checkpoint C-2a** — build clean, all suites green, and `jai_camera_hardware_test` proves
  continuous start/stop leaves the camera able to single-shot
  (`test_single_shot_works_immediately_after_continuous`).
- **Checkpoint C-2** — the owner confirmed on 2026-09-01: *"Camera live view đã hoạt động ổn,
  chưa phát hiện ra bug nào."* Docs and the backlog entry are done.

### Genuinely open, and why

1. **Checkpoint A — the MC PLC has never been run against hardware.** Every Phase A item that a
   machine can check is green, and *none* of the hardware ones are: 1C/3C connect and poll, 3E
   no-regression, the serial-port enumeration deferred back in A1, and the `mc_bench` runs. This
   is the oldest outstanding block in Phase 8 and nothing since has touched it.
2. **Checkpoint B — the one-device-both-roles cycle.** The blocker that stopped it is fixed
   (`PlcRunner` result-output capability, 2026-09-01), but the cycle itself has not run: the
   owner's attempt also reported *"Active pattern group is missing"* and *"Active camera
   calibration is invalid"*, which are commissioning steps, not defects. The robot-pick-check
   re-verification rides on the same run.
3. **Checkpoint C — calibration.** Connect, grab, parameter change, live view, backlight and
   power-cycle recovery are all confirmed. **Board calibration through the JAI widget is not**,
   and it is the one item that also blocks B's localization cycle.
4. **Documentation gaps, both real:**
   - `uml/02_device_families.puml` last changed 2026-08-24 — **before** Modbus and JAI landed.
     Neither family appears in any diagram. `docs/architecture/device_type.md` *is* current.
   - The MC domain doc named in Checkpoint A.

### Not gaps

The unchecked boxes under *"Task C1/C2/C3 (original scoping)"* are the superseded originals kept
for history; their `✅ DONE` counterparts carry the real state. Same for the C4 scoping block.

---

## Checkpoint audit — 2026-09-03

The owner ran all three device families against real hardware and reported back. This audit
records what that closed, what it did not, and one defect it surfaced. It supersedes the
2026-09-02 audit above.

**Owner's report, verbatim, so later readers can judge the evidence themselves:**

> *"Mình đã test MC Thread với PLC thật frame 3E, xác nhận no regression, frame 1C và 3C đã connect
> và read được nhưng chưa test hết các lệnh đọc ghi mà protocol có. Modbus đã chạy thử và connect
> được với device khác cả modbus server và modbus client. Modbus server device đã được test thực tế
> với PLC, xác nhận hoạt động đúng. Camera JAI đã test connect, grab, live view, thay đổi param,
> auto backlight control, reconnect hoạt động ổn định, nhưng có vẻ JAI cam widget chưa wire toggle
> backlight button với runner của camera. Đã test chạy thử trong runtime với robot và camera thật,
> xác nhận hoạt động đúng kì vọng của plan."*

| Checkpoint | Was (09-02) | Now (09-03) | What is left |
|---|---|---|---|
| A-1 transport and contexts | ✅ closed | ✅ closed | — |
| A MC thread | ⛔ open, **nothing** hardware-verified | 🟡 **substantially confirmed** | Full read/write command coverage on 1C/3C; bench numbers; MC domain doc |
| B-1 role model | ✅ closed | ✅ closed | — |
| B Modbus thread | ⛔ open, dual-role cycle unrun | 🟡 **cycle confirmed** | Robot pick check observed *active* (not merely "the robot worked") |
| C JAI thread | ⛔ open | 🟡 **all but calibration** | Calibration via the JAI widget (one word); **Task C9**; `uml/` |
| C-2a device and runner | ✅ closed | ✅ closed | — |
| C-2 continuous shot | ✅ closed | ✅ closed | — |
| Phase 8 complete | ⛔ open | ⛔ open | Six items, listed below |

### What this run actually retired

The three largest risks in the plan's own risk table are now closed by hardware, not by argument:

1. **"C-frame codecs look right and are wrong on the wire"** — the risk register called this
   *"exactly the Phase 7 defect class"*, the one that shipped five defects which compiled clean and
   passed the suite. 1C and 3C now connect and read from a real C24, and 3E is unchanged. The
   codecs are not wrong on the wire.
2. **"B1 breaks the commissioned vision-output path"** — rated High, mitigated by putting B1 first
   behind its own checkpoint. A full localization cycle ran through a Modbus device holding both
   roles, with a real robot. The rewiring did not break the path it rewired.
3. **"JAI SDK API differs more from Pylon than assumed"** — the whole JAI device is confirmed
   stable on hardware including the power-cycle recovery fixed on 09-02.

### The seven remaining items, in the order worth doing them

*(Superseded by the 2026-09-04 update below; three of the seven closed the next day. Kept because
the reasoning about item 1 is what produced the correction.)*

1. **Establish the PLC's actual 32-bit word order (owner, minutes).** Now the top item, ahead of
   everything else, because it is the only open question where *waiting makes it worse*: every
   additional machine commissioned against the current encoder raises the cost of changing it.
   See Open Question 2. Do not change code before answering it.
2. **Confirm the calibration question (owner, ~1 minute).** Was the calibration used in the runtime
   run produced by the JAI widget's board workflow? If yes, Checkpoint C loses its oldest open
   item and only C9 and `uml/` remain.
3. **Task C9 — wire the backlight toggle button.** Scoped above. S-sized.
4. **Robot pick check, observed active** on the dual-role Modbus binding. Cheap to check, and the
   only reason it is still open is that its failure mode is invisible — see the note at Checkpoint B.
5. **MC read/write command coverage on 1C/3C.** `tools/mc_protocol_bench` exists for exactly this
   and has never been pointed at the PLC; one session covers this item and item 6 together.
   Write commands default to off in the bench and need arming deliberately.
6. **Bench numbers for 3E/1C/3C.**
7. **Documentation gaps** — enumerated precisely in the sweep results below, no longer just
   "`uml/` is stale".

Items 1–4 are hours; 5–7 are the bulk. **Nothing on this list blocks the cell from running** — the
runtime works with real hardware today. These are the difference between working and *signed off*.

### Update — 2026-09-04

Three of the seven closed, and one of them closed by being **wrong**.

| # | Item | Now |
|---|---|---|
| 1 | 32-bit word order | ✅ **not a defect.** The master is the robot, not a Mitsubishi PLC; it decodes the shipped high-word-first order correctly. The encoder stays. The settable-order arg moves to backlog. |
| 2 | Calibration source | ✅ owner-confirmed: produced by the JAI widget's board workflow |
| 3 | Task C9 | ✅ implemented, negative-checked, verified on the real camera |
| 4 | Robot pick check observed active | ⛔ open |
| 5 | MC 1C/3C command coverage | ⛔ open |
| 6 | Bench numbers | ⛔ open |
| 7 | Documentation gaps | ⛔ open |

**Item 1 is the one to remember.** It was raised as a high-impact contradiction between a recorded
owner decision and shipped code, and the analysis was right about the code and wrong about the
world: the plan assumed the Modbus consumer was a PLC because the *role* is called `plc`. It is a
robot. The lesson is not "check harder" — the discrepancy was real and worth raising, and the
instruction to establish the decoder's order before changing anything is exactly what prevented a
commissioned machine from being broken to satisfy a document. **A finding that resolves to "the
document was wrong" is a good outcome, not a wasted one.**

Checkpoint C now has one item left: `uml/`. Checkpoint B has one: the pick check.
Checkpoint A has three: command coverage, bench numbers, the domain doc.

### Update — 2026-09-07

`uml/` closed, so **Checkpoint C is complete**. Three commissioning defects found on the cell were
fixed as Phase F above (F1 invalid index, F2 Modbus client writes, F3 panel naming).

| Checkpoint | State | Left |
|---|---|---|
| A MC thread | 🟡 | Command coverage on 1C/3C, bench numbers, MC domain doc |
| B Modbus thread | 🟡 | Robot pick check observed **active** |
| C JAI thread | ✅ **closed** | — |
| Phase 8 complete | ⛔ | A and B |

**All suites, this run:** `architecture_contract_test` **89**, `mc_frame_test` **38**,
`modbus_device_test` **22**, `vision_output_device_test` **9**,
`vision_tcpip_client_device_test` **10**, `jai_camera_hardware_test` **21** (0 skipped, real
camera). Both shells relink clean; translations 0 newly vanished.

> `test_disconnect_notice_on_graceful_close` fails intermittently in the two vision suites —
> measured **3 runs: 0, 1, then 2 failures** — which is backlog item 32 (~2 in 3, product-side,
> diagnosed in Phase 5, reproduces in isolation). Nothing in this round touched that path.

### Sweep results — 2026-09-03

The owner's backlight-button catch prompted a four-angle sweep of the tree for defects of the same
class (a thing that looks wired and is not), each finding then checked by an independent agent
briefed to refute it. **Twelve candidates raised, four refuted, eight stand.** The refutations are
worth as much as the findings: `isContinuousActive()` "has no caller" (the hardware test calls it
five times), `setExposure()/setGain()` "bypass range checks" (the widget enforces them and the
device rejects out-of-range writes), the Add-Pattern dialog's dead lamp (the dialog is never
shown), and "emoji survive in comments" (the one hit is a faithful diagram of a shipped UI string,
which §10 of the comment-style rules explicitly exempts).

**Code (both go to the backlog, neither blocks Phase 8):**

- **`PlcMitsuDeviceWizard` is dead code, entirely.** Compiled and linked (`src/ui/ui.pri:29,112,196`)
  and constructed by nothing anywhere in `src/`, `app/` or `runtime_app/`; its harvest method
  `getWizardJson()` is commented out (`plc_mitsu_device_wizard.cpp:18-20`). `AddDeviceWizard`'s
  `pgMc` page superseded it. Its unread IP and port fields are a symptom, not a live data-loss bug
  — no operator can reach them.
- **`IDevice::errorOccurred` is never emitted by any device.** The channel is wired end to end —
  `CameraRunner` and `PlcRunner` forward it, and `LocalizationRuntimeController` routes it into
  per-role fault reporting — but no device subclass emits it; the only traffic is what the runners
  synthesise. Narrow real consequence: device-internal failures reach the app log via
  `LOG_USER_ERR` but never produce the **task-log** ERROR entry that connection failures and PLC
  write failures do. Grab failures are *not* affected (they abort the cycle with
  `CameraGrabTimeout`). Also `VisionOutputRunner::wireSignals()` omits the forward that the other
  two runners have — currently moot, and a trap for whoever emits the signal first.

**Documentation — this is what "`uml/` is stale" actually means:**

- `uml/02_device_families.puml` — `CameraType` has no `JaiGigE`; `PlcType` has no
  `ModbusTcpClient`/`ModbusTcpServer`; no `JaiGigECamera` or `ModbusTcp*Device` classes. Its only
  `IResultOutputDevice` edge goes to `VisionOutputDevice` — and *"result output is
  vision-output-only"* is precisely the assumption `src/runtime/AGENTS.md` records as a shipped
  defect, the one that cost Checkpoint B its 2026-09-01 blocker. The diagram teaches the bug.
- `uml/03_runtime_threading.puml` — `DeviceCommandKind` shows 5 of 7 enumerators (no
  `CameraContinuousStart`/`Stop`); `CameraRunner` lacks the continuous requests; `PlcRunner` lacks
  the `supportsResultOutput()`/`requestSendResult()` overrides.
- `uml/06_ui_widgets.puml` — shows 3 of the 7 device widgets the factory actually builds; missing
  `JaiCameraWidget`, `ModbusDeviceWidget`, `VisionTcpipClientDeviceWidget`, `VirtualDeviceWidget`.
- `docs/doxygen/pages/mainpage.dox:19-26` — the generated API reference's landing page still lists
  the pre-Phase-8 "first-release integrations", naming neither JAI nor Modbus. Two more copies of
  the same stale list exist (`customer_installer_packaging.md:66`, which contradicts its own line
  117, and the checked-in rendered HTML).

`uml/README.md` states the diagrams describe the code as it exists now, and its "Known
Placeholders" section names neither family — so a reader gets no signal to distrust them. That is
what makes these worth fixing rather than tolerating: a diagram that is merely incomplete is
harmless, and one that is confidently wrong about a contract is not.

### Defect found this round

**The backlight toggle button does nothing** — `btn_baklight_toggle`, JAI and Basler alike. The
owner diagnosed it correctly as a missing runner wire-up. Confirmed and scoped as **Task C9**;
notable because it is a **third instance of the same class**: a control that looks live and is not.
The first was the Basler connection lamp that could never go green, the second the empty Basler
Save handler — both listed in C4's debt note. All three shipped because there is no widget-level
test in this project, and a dead `connect()` is invisible to every suite that exists.

---

## Checkpoint: Phase 8 complete
- [x] **All four checkpoints closed with owner confirmation on real hardware** — A, B, C and F,
      all closed 2026-09-07 or earlier. Three items were **carried forward rather than ticked**;
      see "What Phase 8 did not prove" below.
- [x] Contract test count recorded (**95** as of 2026-09-07, was 86 at the C checkpoint);
      `mc_frame_test` (38) and `modbus_device_test` (**22**) green
- [x] Translations swept at every task; **0 strings newly vanished** across the whole phase
      (29 → 29 unresolved). ➡️ The Japanese *rendering* is still unverified and **436 strings
      remain untranslated** — carried, not claimed. That is a translation-content task, not a
      Phase 8 deliverable.
- [x] `docs/backlog/technical_debt_and_next_steps.md` updated with anything deferred (2026-09-07)
- [x] Docs, reference docs and diagrams updated (2026-09-07): `docs/domains/mc_protocol/
      mc_protocol.md` **created** and indexed in `docs/README.md`;
      `docs/domains/task_localization/runtime_controller_api.md` corrected — it still claimed
      *"`runtimeFault` is now raised only by an invalid setup"*, which three commissioning fixes had
      made false; `uml/08_runtime_state_machines.puml` corrected — it carried a
      `retry limit exceeded` transition that **no longer exists**, from the era before role
      reconnect became unbounded; `docs/doxygen/pages/mainpage.dox` integrations list refreshed
      (it named neither JAI nor Modbus).

## Phase 8 — ✅ CLOSED 2026-09-07

Six phases of work: MC 1C/3C frames (A), Modbus TCP client and server (B), the JAI GigE camera
(C), and the commissioning defects the cell surfaced afterwards (F).

### What Phase 8 did not prove

Recorded here because a closed phase is read as a guarantee, and these three are not covered by
one. All are in `docs/backlog/later_todo_list.md`.

| # | Item | Why it is not a defect |
|---|---|---|
| **56** | MC 1C/3C read/write **command coverage**, and the benchmark numbers | Connect and read are proven on real hardware; the rest of the command set is untested, and `mc_frame_test` structurally cannot cover it. **Write commands matter most.** |
| **57** | Robot pick check verified **active** on the dual-role Modbus binding | The B1 hazard defaults the kinematic-check settings silently. A defaulted-off check produces a *working* robot, so a successful run is not evidence. |
| **54** | A **held** `bExecuteTrigger` fires one cycle when the runtime starts | The contract permits either reading and does not say which the cell wants. Needs an owner decision, then a sentence in the contract. |

### The pattern worth keeping

**Every defect in Phase F was found by the owner running the cell. None was found by a test
suite** — and F5 was found by the owner *re-testing a fix*. The suites did their job for the work
they cover (they caught the `WaitingTriggerReset` regression within seconds of it being written),
but the recurring failure shape in this project is **"looks like it worked, didn't"**: a dead
`connect()`, a published signal with no state behind it, a diagram teaching an assumption that
costs a day. There is still no widget-level test here, so a painted string and a button's wiring
remain things only a person can confirm.

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| **B1 breaks the commissioned vision-output path.** It rewires the result path used by every shipping project | High | B1 is first, alone, behind its own checkpoint. Existing output-device tests must stay green. Fallback to Q1 option 2 is pre-agreed and costs only the Modbus widgets |
| **The `robotCheckConfig` silent default** (B1) | High | Called out as its own acceptance criterion with a contract test; re-verified at Checkpoint B, not only when fixed |
| **C-frame codecs look right and are wrong on the wire.** Exactly the Phase 7 defect class | High | Byte-exact reference frames in A7 (not self-generated expectations), 3E as the control case, and A8 against the real PLC before Checkpoint A closes |
| **Serial hardware unavailable during development** | Medium | A1–A4 and A7 are all hardware-free. Only A5/A6/A8 acceptance needs the PLC, and the owner runs those |
| **JAI SDK API differs more from Pylon than assumed.** C2 is scoped as "Basler parity" from the Basler *implementation*, not from the JAI docs | Medium | C1 is a standalone build-wiring task, so the SDK is proven to compile and link before C2 designs against it. C2 splits if the node-map model differs |
| **New display names never reach the `.ts`** — silent, no build error | Medium | Marker rule stated once in this plan, asserted per task, machine-checked by the contract test in both directions |
| **JSON tokens wrong on first write.** They land in customer project files and cannot change | Medium | Tokens fixed in B5/C3 and reviewed at the checkpoint before any project is saved with them |
| **Registry ordering regression** — a virtual device becomes the wizard default | Medium | Explicit acceptance criterion in B5 and C3; the existing comment at `device_registry.cpp:203` explains why |
| **Three L-sized tasks** (A8, B6, C2) | Low | Each carries a written split point; split at implementation time rather than pretending they are M |

---

## Open Questions

1. **Modbus result encoding** — scaled int16 (compact, needs an agreed scale factor) or IEEE-754
   across two registers (exact, doubles the register count)? B2 proposes one with justification;
   the owner decides at the B2 review, before it is coded.

Answer: IEEE-754 across two registers

2. **Modbus register word order** for 32-bit values — big-endian or little-endian word pairs.
   Vendor-dependent; needs the target PLC. Blocks nothing before B2's review.

Answer: normal word order is the same with Mitsubishi PLC order, API have arg to set order.

> ⛔ **The shipped code does neither — found 2026-09-03, needs an owner decision before anything
> else is commissioned on it.**
>
> | | 32-bit word order |
> |---|---|
> | Owner's answer above | Mitsubishi order, **plus a settable arg** |
> | Mitsubishi MC, as implemented | **low word first** — `mc_request.h:150-151`, and the comment says so: *"low word first, then high word"* |
> | Modbus result layout, as shipped | **high word first** — `modbus_result_layout.cpp:80-81` writes `high` into the lower register, `low` into the higher |
>
> So the Modbus layout is the **opposite** of the order that was asked for, and there is no arg to
> change it: a grep for `wordOrder|byteOrder|endian|swap` across `src/device/plc/modbus` returns
> nothing. `encodeAxis()`/`decodeAxis()` (`modbus_result_layout.cpp:40-55`) hard-code the split.
>
> **This is not subtly wrong, which is what made it worth chasing.** With `kScale = 100`,
> X = 250.00 mm encodes as 25000 = `0x000061A8`. Read in the wrong word order a master sees
> `0x61A80000` — 1,637,875,712, or 16,378,757 mm. Any wrong-order read is off by orders of
> magnitude, not by a rounding error, so a working cell is strong evidence about the decoder.
>
> **Resolved 2026-09-04 — the consumer is a robot, not a Mitsubishi PLC.** The owner corrected the
> premise: the Modbus server device was commissioned against **the real robot**, which is the
> Modbus master reading these holding registers. It decodes the coordinates correctly today.
> That settles it:
> - The recorded answer *"same as Mitsubishi PLC order"* was about a Mitsubishi master. There
>   isn't one on this path — the vision result goes to the robot.
> - High-word-first is therefore **not a defect**; it is the order the commissioned robot program
>   expects, and the cell proves it end to end.
> - **The encoder must not change.** This is the frozen-contract case exactly: a silent flip would
>   break a working machine to satisfy a note in a plan.
>
> What remains true and unbuilt is the **second half** of the answer: there is still no arg to set
> the order. It costs nothing today and is the only thing that makes the next master cheap —
> especially a Mitsubishi one, which really does read low word first (`mc_request.h:150-151`) and
> would need the opposite of what ships. Carried to
> `docs/backlog/later_todo_list.md` rather than done here, because adding a knob with exactly one
> caller and no second implementation to shape it is the abstraction this project's rules warn
> against. When a second master appears, it will define the parameter properly.
>
> The B2 review gate (*"Owner reviews the layout doc"*) was **approved by the owner on
> 2026-09-07**, which closes it and freezes the encoder. See Checkpoint B.

3. **1C/3C serial parameters on the real PLC** — station number, PC number, frame format and
   sum-check setting must match the C24 module's configuration. Needed for A5 acceptance, not for
   A1–A4.

Answer: accept.

4. **JAI camera model and colour/mono** — affects the pixel-format arm in C2. Needed at C2, not
   before.

Answer: pixel-format and camera model read from camera after connected.

5. **Whether the JAI camera exposes usable IO lines** for backlight control. If not, C2's
   IO/backlight criterion drops and the widget hides those controls; the owner confirms at C2.

Answer: JAI camera exposes usable IO lines.

---

## Explicitly Out Of Scope

- `Frame_1E` — its factory arm stays `nullptr`. The request names 1C and 3C only.
- Modbus RTU / serial Modbus — request says TCP/IP only. `QT += serialbus` brings the RTU classes
  along, and they are **not** to be wired up "since they are there".
- Modbus as a *camera* or robot role; Modbus 4C/4E frames; the Nachi `.prg` file (standing Q1
  decision from an earlier phase: not touched, the owner handles it).
- Anything in the carried-forward backlog (#37–#45) unless a Phase 8 task lands on it directly.
