# Module: device (level 1)

**Purpose.** Device families and their lifecycle: cameras (`camera/`,
Basler GigE via Pylon and JAI GigE via the Pleora eBUS SDK), PLC (`plc/`, Mitsubishi MC
protocol and Modbus TCP), vision output (`output_device/`, TCP/IP server/client), robots
(`robot/`, Kawasaki/Nachi), and hardware-free stand-ins for all of them (`virtual/`).
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
`components/app/`. Enforced by the architecture contract test.

**MC protocol: three independent axes.** Frame codec, transport and context are separate and
compose freely — `McProtocolDevice` dispatches on the frame type and the message-interface type
in two independent switches, and nothing else in the device knows which pair it got.

| Axis | Interface | Implementations |
|---|---|---|
| Frame codec | `MCFrameAbstract` | `Frame3E` (binary), `Frame1C`, `Frame3C` (ASCII computer link) |
| Transport | `McMsgInterface` | `McEthernetTcpPort`, `McMsgSerialPort` |
| Context | `McContext` | `Context_Mc3E`, `Context_Mc1C`, `Context_Mc3C` |

Adding a frame means: a context (with `kDisplayNameSources[]`), a codec, an arm in
`Factory::contextFactory()`, an arm in `McProtocolDevice::initialize_mc_device()`, cases in
`tests/mc_frame_test`, **and an entry in the Add Device wizard's frame combo** — the frame is
`Q_PROPERTY(... CONSTANT)`, so a frame missing from that combo can never be selected by anyone
and the whole codec is dead code. `Frame_1E` is declared in the enum with no factory arm on
purpose; it must stay out of the combo until it has one.

**`IPlcInputSimulator` is the one capability a real device must never implement.** Every other
capability in `device_capabilities.h` describes something a device *can do*; this one describes
something only a **simulation** may do — let this software forge the values the plant is supposed
to produce. It exists because every runtime state past Ready begins with the PLC changing an input,
so without it a hardware-free project stopped at Ready and every state transition, fault paths
included, was unreachable. `PlcRunner::supportsInputSimulation()` resolves it **per device**, and
the UI asks that rather than testing for a sub-type, so a real PLC can never be offered the
controls. Two rules go with it: injected inputs are a **separate store** from the recorded writes
(sharing one makes the handshake complete itself and hides a duplicate-tag mapping mistake), and
tag validation is **as strict as the write path** (a virtual device that accepts any tag moves the
discovery of a mapping error onto real hardware, which is the most expensive place to find one).

**Modbus: one core, two devices.** `plc/modbus/` holds the register map, the tag naming and the
result layout **once**, shared by `ModbusTcpClientDevice` (we are master, we poll a slave) and
`ModbusTcpServerDevice` (we are slave, a master polls us). Only the Qt object underneath and the
connect/listen lifecycle differ, and one device widget (`ModbusDeviceWidget`) serves both because
they differ in nothing but the connection card.

| Concern | Lives in | Note |
|---|---|---|
| Areas, tag names, storage, change detection | `modbus_register_map.{h,cpp}` | Tags are `COIL`/`DI`/`HR`/`IR` + 5 digits |
| Vision-result register layout | `modbus_result_layout.{h,cpp}` | **Wire contract**; see the doc below |
| Register spans + result block settings | `modbus_config.h` | Shared base; client/server configs add their own header each |
| Log renderings of frames, function and exception codes | `modbus_trace.{h,cpp}` | See the logging note below |

**Diagnosing a Modbus fault starts with three log lines, and they are always emitted.** The client
logs its **poll plan** on every (re)configure — every range it will ask for; the server logs its
**register space** at connect — every range it will answer, and which areas are absent entirely;
and any failed request logs the request, the reply and the **named exception code**. Comparing the
first two settles an `IllegalDataAddress` without attaching a protocol analyser. The per-frame
trace (`Protocol Trace` on the config) is off by default because a 100 ms poll over four areas is
forty lines a second — but **failures are dumped in full either way**, so a fault never has to be
reproduced with tracing switched on. The flag is the one setting both devices accept while
connected, because a debugging switch you must drop the link to use is useless on the fault you
are chasing.

> ⚠️ **Two Qt traps this layer already paid for.**
> `QModbusTcpServer::installConnectionObserver()` **takes ownership** — Qt holds the observer in a
> `QScopedPointer`. Keeping our own owning handle was a double free that surfaced as heap
> corruption, not as a null dereference. And `QModbusPdu::isValid()` is **false for an exception
> response**, so gating the error rendering on it silently discarded the exception code — the one
> fact the log existed to carry. Gate on `isException()` / `functionCode()` instead.

> ⚠️ **A Modbus write from the UI must go through `PlcRunner`, never straight to `IPlcIoWriter`.**
> `ModbusDeviceWidget` called the writer directly at first. The device lives on the runner's worker
> thread and owns a thread-affine `QTcpSocket`: Qt reported nothing, the frame never reached the
> wire, the peer logged nothing because nothing arrived, and the master waited out its full
> response timeout — *intermittently*, since a frame sometimes did get out. It also ran
> `transact()`'s nested event loop on the GUI thread. `PlcRunner::requestWriteDigitalIo()` /
> `requestWriteWordIo()` exist for exactly this and were, until then, called by nobody.
> Both devices now refuse an off-thread write by name (`assertOnDeviceThread()`), at the write
> entry points so a wiring mistake cannot spend the retry budget and tear a healthy link down.
> `MitsubishiMcDeviceWidget` is safe calling `pushRequest()` directly only because that method is a
> mutex-guarded enqueue that touches no socket — an accident of design worth knowing before
> copying the pattern to a device that does synchronous I/O in the writer.

> ⚠️ **The result register layout is a published wire contract.**
> [`docs/domains/task_localization/modbus_result_contract.md`](../../docs/domains/task_localization/modbus_result_contract.md)
> is the specification and the header is its implementation. Once a customer PLC reads those
> registers the offsets, the ×100 scale, the high-word-first order and the count-last handshake
> are frozen; the only safe change is an additive one into the reserved header word. The tag
> names are equally frozen — they are persisted verbatim in task signal maps.

> ⚠️ **"Writable" is two questions, and they have different answers.**
> `modbus_tags::isMasterWritable()` — the protocol defines no function code by which a *master*
> writes a discrete input or an input register, so the **client** refuses those locally.
> `modbus_tags::isServerWritable()` — true for all four, because the **server** owns the register
> space and is the *only* legitimate writer of the two input areas ("input" is named from the
> server's point of view).
> Asking the master's question on the server side is a defect this module has already shipped
> once: it left the server unable to publish into the areas it owns, and cost the safest layout
> available — vision results in input registers, where a master has no way to overwrite them
> (`ModbusTcpServerCfg::resultInInputRegisters`). Never use one predicate for both sides.

> ⚠️ **The computer-link frames are unverified on the wire.** `tests/mc_frame_test` asserts that
> 1C and 3C build what the reference implementations in `reference source/` describe. If that
> reading of a reference is wrong, the codec and its test are wrong together and both stay green.
> Only `tools/mc_protocol_bench` against a real C24 port settles it. Two known divergences from
> the 3C Python reference are documented at the functions that diverge in `mc_frame_3c.cpp`.

**Two camera families, two SDKs, and they behave differently.** `BaslerGigECamera` is built on
Pylon (`CInstantCamera`); `JaiGigECamera` is built on the **Pleora eBUS SDK** (`PvDevice` /
`PvStream` / `PvPipeline`). Both are installed on this machine — the JAI SDK C API
(`jai_factory.h`, `JAI_SDK_*`) is present too and is **not** what the JAI camera uses; the
reference that settles it is `reference source/JaiCamTest`. Build wiring is
`qmake/ebus_dependency.pri`, which derives everything from the installer's `PUREGEV_ROOT`, so a
machine with the SDK installed needs no setup.

| Concern | Basler / Pylon | JAI / eBUS |
|---|---|---|
| Errors | C++ exceptions (`GenericException`) | `PvResult` return codes — **the SDK never throws**, so an ignored result is a silent failure |
| Cable pull | No callback exists; noticed only on a failed grab | `PvDeviceEventSink::OnLinkDisconnected` fires on its own |
| Acquisition | `GrabOne()` wraps start/stop | Explicit `AcquisitionStart` / `RetrieveNextBuffer` / `AcquisitionStop` |
| Device info | `CInstantCamera::GetDeviceInfo()` | **No such accessor.** `PvDeviceInfo` lives only during enumeration; carry the identity out of the discovery pass |

> ⚠️ **Never hard-code a GenICam feature name for exposure, gain or frame rate.**
> The SFNC renamed them, and which spelling a camera answers to depends on the model and its
> firmware: `ExposureTime`/`Gain` (float) on SFNC firmware, `ExposureTimeAbs`/`GainRaw` (integer)
> on older. The model is not known until connect. `jai_define.h`'s `node_names` +
> `resolveNode()` pick whichever the connected camera has; the Basler device hard-codes the
> legacy pair because that is what its one camera model answers to, which is exactly the
> assumption that works on the camera it was written against and silently fails on the next one.

> ⚠️ **The eBUS SDK delay-loads GenICam, and a missing GenICam does not fail — it kills the
> process.** `PvDevice64.dll` and `PvGenICam64.dll` **delay-load** `GenApi_MD_VC141_v3_4.dll`
> and `GCBase_MD_VC141_v3_4.dll`, which live in the *subdirectory*
> `…\Pleora\eBUS SDK\GenICam\bin\Win64_x64` that the installer does **not** put on PATH — only
> the parent. `PvSystem64.dll` delay-loads nothing, so camera **discovery works perfectly** and
> the first **Connect** raises the VC++ delay-load exception `0xC06D007E`
> (`ERROR_MOD_NOT_FOUND`). That is a Windows SEH exception: `catch (...)` never sees it, the
> process dies, and the log ends mid-sentence with nothing about the failure. It presents as
> "the app crashes on connect with nothing in the log", and it is not a bug in connect.
> `jai_runtime.h`'s `ensureGenICamRuntime()` prepends the directory to the process PATH and
> proves the result by loading the DLL; call it before **any** eBUS call that builds a node map.
> Another GenICam on PATH does not help — a machine with the JAI SDK carries
> `GenApi_MD_VC80_JAI_v2_4.dll` and Pylon carries `GenApi_MD_VC141_v3_1_Basler_pylon.dll`;
> neither can satisfy a `_v3_4` import. Measured on this machine: a bare-name
> `LoadLibraryW("GenApi_MD_VC141_v3_4.dll")` returns error 126 before the fix and succeeds after.

> ⚠️ **A single-shot grab must put the camera in `SingleFrame`, or it floods the link.**
> Left in its default `Continuous` mode, `AcquisitionStart` makes the camera free-run at full
> rate for the whole grab window. On the GO-5000M-PGE that is 5 MP mono at ~22 fps ≈ **115 MB/s**,
> more than Gigabit Ethernet carries: most blocks arrive with packets missing, the resends fail
> against an already-saturated link (`RESENDS_FAILURE`), and the grab succeeds only on whichever
> frame happens to survive. Measured before the fix: up to **13 discarded frames and 1.25 s** for
> one trigger, against a 1.5 s timeout — it *worked*, which is what made it look like noise
> rather than a defect. `configureAcquisitionMode()` sets `SingleFrame` at connect and restores
> the previous mode at disconnect. Use `PvAcquisitionStateManager` rather than bare
> `AcquisitionStart`/`Stop` commands: it also manages `TLParamsLocked`, which the SDK's own
> samples set and this device originally skipped — but see the next trap before wiring it in.

> ⚠️ **eBUS has two acquisition designs and mixing them bricks grabbing silently.** Either
> `StreamEnable()` at connect + the bare `AcquisitionStart`/`AcquisitionStop` commands per grab +
> `StreamDisable()` at teardown (`PvPipelineSample`), **or** no `StreamEnable()` at all and
> `PvAcquisitionStateManager::Start()`/`Stop()` per grab (`eBUSPlayerSample`). Never both. For a
> GigE device `StreamEnable()` does exactly one thing — take `TLParamsLocked` — and
> `PvAcquisitionStateManager.h` documents `Start()` as returning `STATE_ERROR` *"if TLParamsLocked
> is already set"*. Holding both cost two separate failures at once, and neither error message
> pointed at the cause: every grab died with `STATE_ERROR - Cannot start, state already set to
> locked`, and because `TLParamsLocked` freezes the streaming-related nodes, the camera also
> refused to leave `Continuous` (`AcquisitionMode: Node is not writable`) — quietly disabling the
> `SingleFrame` fix above. `openStreamAndPipeline()` therefore does **not** call `StreamEnable()`;
> the `StreamDisable()` left in teardown is a backstop for a lock the manager could not release,
> not its counterpart. Related: nothing in `Start()`'s failure path clears the lock, so
> `grabSingleShot()` releases a stale one before starting — otherwise a single interrupted grab
> makes every later grab fail identically until the camera is reconnected.

> ⚠️ **A GigE camera that can saturate the link must be paced, or nothing arrives at all.**
> The GO-5000M-PGE sends a 5 MP frame as one wire-rate burst; the adapter cannot absorb it, and
> the resend requests then fail against the same saturated link. Symptom: `RESENDS_FAILURE` /
> `TOO_MANY_CONSECUTIVE_RESENDS` on **every** block. That names packets, so jumbo frames and
> cabling are the obvious suspects — on this station both were already fine, and neither was the
> cause. `GevSCPD`, the camera-side inter-packet gap, is. Measured, 10 grabs per row, 4000-byte
> packets:
>
> | GevSCPD | link use | grabs | incomplete blocks | mean |
> |---|---|---|---|---|
> | 0 | 100% | **0/10** | 118 of 118 | 3031 ms |
> | 1495 | 57% | 10/10 | 13 of 23 | 561 ms |
> | 3000 | **40%** | 10/10 | **0 of 10** | **239 ms** |
> | 20000 | 9% | 10/10 | 0 of 10 | 517 ms |
>
> Pacing makes grabs *faster*, because a frame that arrives once beats a frame retried until the
> timeout. Two further traps around it: **`GevSCPD` is stored on the camera and survives power
> cycles**, so a value left by another application silently becomes this station's behaviour —
> `configurePacketDelay()` writes it unconditionally rather than deferring to what it finds. And
> packet size, held at constant utilisation, made **no** difference worth having (8976 → 1 error,
> 8192 → 0, 4000 → 7, all inside run-to-run noise), so the negotiated size is left alone. An
> earlier attempt to auto-tune packet size by probing was removed: single-frame probes decide on
> noise, and one run picked 2048, which measured worst of all.

> ⚠️ **One frame per trigger means a damaged frame must be re-armed, not waited out.**
> In `SingleFrame` the camera sends exactly one frame per `AcquisitionStart`. If it arrives
> incomplete there is nothing else coming, so the natural "discard it and keep waiting" loop burns
> the entire grab timeout on a stream that already finished — measured as one discard at ~120 ms
> followed by 2.9 s of waiting, on every grab. `grabSingleShot()` therefore has two nested loops:
> the inner one retrieves, the outer one re-arms. In `Continuous` the opposite holds — the next
> frame is already in flight, so waiting IS the recovery and re-arming would throw it away. The
> mode check on that path is load-bearing.

> ⚠️ **Connect must PUSH the commissioned settings onto the camera, not just read them.**
> A GenICam camera keeps its settings across a reconnect, so a connect that only reads looks
> correct indefinitely — until the camera is power-cycled, comes back at factory defaults, and
> grabs with them while the panel still shows what was commissioned. Nothing is logged, because
> nothing failed. `JaiGigECamera::deviceConnect()` therefore reads the ranges, then calls
> `applyParametersChange()` — the same path the Apply button uses, so there is only one write path
> to keep in step with the config.
>
> The order inside that is load-bearing. **The pre-push read must not adopt the camera's values**
> (`ValueSync::LimitsOnly`): the exposure maximum moves with the frame rate, so a camera that
> powered up at 22 fps reports `10..44730`, and a commissioned 199892 µs — legal at the configured
> 5 fps — looks out of range and gets discarded moments before the push that would have restored
> the frame rate and made it legal again. Clamping belongs in the post-push read, where the ranges
> are the ones grabs will actually run at. Same for the auto-exposure mode and the frame-rate-limit
> switch: read unconditionally, they overwrite commissioned intent with a power-up default and
> then the panel agrees with the camera, erasing the evidence.

> ⚠️ **`isDeviceConnected()` is FALSE for the whole of `deviceConnect()`.** It reports the app's
> published `ConnectStatus`, which `deviceConnect()` only sets on its last line. Guarding a helper
> on it means that helper silently does nothing when called during connect — which is how the
> backlight setup came to work only after the operator pressed Apply, and came up unconfigured on
> every fresh session. Helpers that must run during connect guard on the SDK handle
> (`m_device == nullptr`) instead, which is what "do we have a live device" actually means.

> ⚠️ **A digital output's routing is configuration; only its level is per-grab.** On the JAI
> camera the backlight is set up **once** — `LineSelector` = the configured line, `LineMode` =
> Output where the model allows it, `LineInverter` = the invert flag, `LineSource` = `UserOutput0`
> — and switched by writing `UserOutputValue` alone. Doing the routing on every toggle is four
> extra register writes per trigger for a wiring decision that cannot change between them.
>
> Two specific traps this replaced. **Polarity belongs in `LineInverter`, not in the value**: the
> old code computed `invert ? !value : value` before writing, which is observationally identical
> while this code is the only thing driving the pin and wrong the moment anything else does — the
> camera's own power-up default included. And **the backlight's invert flag must not reach other
> lines**: the generic `writeIO()` path shared that software flip, so a station with an inverted
> backlight drove every other output upside down too. `backlightDiagnostics()` reads all of it
> back from the camera, because "the light does not switch" has four causes that look identical
> from outside: wrong line, not routed, wrong polarity, value never written.

> ⚠️ **The Modbus server serves a WIDER address range than it mirrors, so it can ACK a write and
> then drop it.** `servedRange()` hands Qt the **union** of the mapped span and the vision-result
> block, because the server must own every register a master may touch or a result read fails with
> an illegal-data-address exception. But `ModbusRegisterMap::isConfigured()` covers only the
> **mapped** span. On the defaults that is HR0–HR63 mirrored inside HR0–HR1000+ served: a write
> anywhere in the gap succeeds on the wire, the master is told it worked, and the value reaches
> nothing.
>
> This produced a field defect (2026-09-04): a robot wrote `nActiveCamera` and
> `nActivePatternGroup` in one FC16 request and only the camera took effect, while the same two
> values written as two separate requests both worked. **FC16 covers a CONTIGUOUS block**, so a
> master batching two signals that are not adjacent puts the second value on whatever register
> follows the first. Nothing above the device layer can see this — by the time values reach the
> runtime they are just a `QMap`, and one request versus two is invisible there.
>
> `onDataWritten()` counts what it actually mirrored and says so at **user** level
> (`delivered=N of M`, with both ranges). Keep that at user level: it is the only evidence the
> operator gets, and the device genuinely cannot refuse the write — Qt has already served it. The
> result block is exempted from the warning because this device writes it four times per cycle
> itself; without that exemption the message would appear every cycle and be learned as noise.

> ⚠️ **A nested `QEventLoop` waiting for a reply still dispatches queued meta-calls, so a device
> that waits that way re-enters itself.** `ModbusTcpClientDevice::transact()` runs
> `loop.exec(ExcludeUserInputEvents)`; that flag defers only *user input*, and `PlcRunner` posts
> one queued call per write. A runtime burst — `publishInitialReadyOutputs()` alone is ten signals
> — therefore lands **inside the first write's wait**.
>
> This shipped two failures at once. The in-flight guard refused every write after the first, so
> nine in ten never reached the PLC; and each refusal was charged to the link retry budget, so past
> `retryCount` (default 3) the client declared a healthy link lost and disconnected itself. The
> owner reported it as *"trong runtime không thể ghi coil value"*.
>
> Two rules came out of it. **A collision with your own in-flight request is not a transport
> failure** — the peer was never contacted, so it must not spend a budget that exists to detect a
> dead link. And **a write that never reached the wire is not a failed write**: it is deferred and
> replayed when the transaction unwinds, which is a different thing from retrying a write that
> failed, and the header contract says so explicitly. The replay must be held off across a
> multi-write sequence that has to stay atomic — `sendVisionResult()` writes the count, the
> payload, the sequence and the count again, and a drain between any two of them exposes a
> half-written result block.

> ⚠️ **A manual override of an automatic output must be enforced at a single funnel, not at each
> call site.** The JAI backlight is driven automatically from five places — around a single shot,
> and around the start, failed start and stop of a stream. When the operator's backlight button
> arrived (Task C9), adding "unless overridden" to all five would have worked and would have been
> one forgotten grab path away from the original defect: a manual toggle that appears to work and
> is silently undone at the next trigger, which reads as a failing lamp rather than as software.
> Instead every automatic write goes through `setAutoBacklightState()`, the only automatic route
> to the lamp, and the override is checked once there. A new grab path cannot forget it because
> it has no other way to reach the output.
>
> Two details that fell out of it. The helper returns **whether it actually drove the lamp**, so
> `autoBacklightDelay` is slept only when this grab is what switched the light on — under an
> override it has been on for a while and the delay would be added to every trigger for nothing.
> And the override is cleared in `releaseSdkObjects()`, the one function both a clean disconnect
> and a lost link pass through: a camera reconnected with the flag still set would suppress its
> own auto-backlight forever, and after a power cycle the lamp is off anyway, so the flag would
> not even be true.

> ⚠️ **A continuous frame pump must re-post itself, never loop.** A device lives on its runner's
> worker thread and is driven entirely by queued signals. A `while (streaming) { retrieve... }`
> loop starves that thread's event loop, so **the stop command can never be delivered** — the only
> way out is killing the task. `JaiGigECamera::pumpContinuousFrame()` retrieves at most one frame
> and re-posts itself with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`; worst-case stop
> latency is then the retrieve timeout (`kContinuousRetrieveMs`), measured at 12 ms in practice.
> Two consequences worth keeping: continuous also has to cap `AcquisitionFrameRate` to what the
> paced link carries (see the pacing trap above — free-running reproduces the every-frame-
> incomplete defect exactly), and stopping must restore both `SingleFrame` and the operator's
> frame rate, or the next single shot behaves differently for no visible reason.

> ⚠️ **Continuous frames must not travel on `grabFinished`.** `CameraRunner`'s contract is
> *exactly one outcome per command*, and `onGrabFinished()` resolves the active command and spends
> the single-shot retry budget. A stream arriving there resolves commands that are not running.
> Frames go on `continuousFrameReady()`; `continuousStateChanged(bool)` reports state and is what
> the runner resolves `CameraContinuousStart`/`Stop` from. That signal is deliberately **not an
> edge** — the device emits it after every start/stop request including the ones that changed
> nothing, so an idempotent stop resolves immediately instead of waiting out the watchdog.

> ⚠️ **GenICam ranges are not constants, and a failed range read looks exactly like a narrow one.**
> On this camera family the exposure maximum moves with the acquisition frame rate. Two
> consequences: write the frame rate **before** the exposure in `applyParametersChange()`, or the
> exposure is clamped against the old limit; and re-read the ranges after applying, or the
> property browser keeps showing the limits read at connect. Separately, resolving a feature's
> *name* generically is only half the job — its **type** varies too (SFNC `ExposureTime` is a
> Float, older firmware exposes the same concept as an Integer), and `GetFloatRange()` on an
> Integer node just fails. `jai_define.h`'s `NumericFeature` / `readNumericFeature()` handle
> both, and a range that comes back degenerate is logged and rejected rather than written into
> the config, where `min == max` reaches the UI as a field the operator cannot move.

> ⚠️ **eBUS enumerates per network adapter, so discovery reports duplicates.** A camera
> reachable through more than one adapter comes back once per adapter — the same physical
> camera, listed several times. Deduplicate on **MAC address**: it is the only identifier that
> belongs to the camera rather than to the path taken to reach it (IP can repeat across subnets,
> and the connection ID differs per interface). Note that discovery also finds cameras on
> subnets the host cannot reach; `PvDeviceInfoGEV::IsConfigurationValid()` is what separates
> "visible" from "openable", and a camera that fails it needs an IP, not a cable.

> ⚠️ **The eBUS runtime folder is shared, and it contains someone else's OpenCV.**
> `Common Files\Pleora\eBUS SDK` holds `opencv_world410.dll`, `opencv_world410d.dll` and
> `opencv_ffmpeg410_64.dll` — ~208 MB. `qmake/deploy_dependencies.pri` copies eBUS DLLs **by
> name prefix** (`Pv*64`, `Eb*64`, `Pt*64`, `SimpleImagingLib64`) and must never glob `*.dll`
> out of it. Today the names differ from ours (`opencv_world4110`) so a glob would break
> nothing visibly — which is what makes it dangerous: it would sit unnoticed until this project
> moved to OpenCV 4.10, and then the app would load a stranger's build from its own directory.

**Capabilities are how a device qualifies for a task role.** `device_capabilities.h` holds the
mix-in interfaces; a device offers a role by implementing one, and callers query with
`dynamic_cast`. `IResultOutputDevice` is the one with teeth: it carries **both**
`sendVisionResult()` and `robotKinematicCheckConfig()`, so a device cannot offer to output vision
results without also answering for the robot pick-check settings that gate them. Those settings
live in `device/robot_kinematic_check_config.h`, outside the vision-output family, precisely so a
PLC-family device can carry them.

> ⚠️ **Never read a role's settings by casting to a family config.** `buildRuntimeContext()` used
> to read the pick-check settings via `dynamic_cast<VisionOutputDeviceCfg *>`. That cast fails for
> any other family and the settings then default to *check disabled* — a station commissioned with
> reachability checking would silently stop doing it, with no error, no log and nothing visible on
> the UI. Ask the capability.

**Invariants.**
- Every new device subtype must update: enum/string conversion, factory
  dispatch, UI dispatch (form module), persistence, and tests
  (see AGENT.md "Architecture Guardrails").
- Devices are driven cross-thread ONLY through the runtime runners
  (`CameraRunner`, `PlcRunner`, `VisionOutputRunner`) — never call device
  methods directly from another thread.
- Config types round-trip via `toJson()`/`fromJson()`; imported values are
  validated (range-checked camera numbers, capped id lengths).

**The JAI camera has a hardware-in-the-loop test, and it is how these defects were found.**
`tests/jai_camera_hardware_test` drives the shipped `JaiGigECamera` against a real camera and
asserts on actual pixels — grab success rate over 10 triggers, frame dimensions, channel count,
and that the image is not uniform. It **skips** rather than fails when no camera answers, and it
is deliberately not `CONFIG += testcase` so `make check` never pays its discovery timeout. Point
it elsewhere with `NCR_JAI_TEST_IP` (default 192.168.0.70).

Reach for it before changing anything in the grab or stream path. Every defect this device has
had — the GenApi delay-load crash, the `StreamEnable`/`PvAcquisitionStateManager` conflict, the
SingleFrame re-arm, the packet pacing — was invisible offline, and the alternative is a round trip
through someone else running the UI and mailing back a log. `streamDiagnostics()` exists for the
same reason: `GevSCPD`, packet size, block and error counts in one line, logged automatically
whenever a grab fails.

**Verify.** `tests/architecture_contract_test` (factory/config round-trip
tests), `tests/mc_frame_test` (byte-level MC frame build/parse for 3E/1C/3C),
`tests/modbus_device_test` (20 cases: both Modbus devices against real Qt Modbus peers over
loopback, including the off-thread write refusals and a mismatched-span `IllegalDataAddress`),
`tests/vision_output_device_test`, root app build.

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

**A config property with a display name needs a translation marker in the same edit.**
Every `G_PROPERTY_*` macro emits `Q_CLASSINFO("<prop>_name", …)`, and `lupdate` does not read
`Q_CLASSINFO` — so the label reaches the UI and never reaches the `.ts`. Add the string to
that class's `kDisplayNameSources[]` table, with the context spelled exactly as
`staticMetaObject.className()`. The contract test checks both directions and the context; a
missed marker fails the suite. Note this applies to hand-written `Q_CLASSINFO` too — the
requirement comes from `Q_CLASSINFO`, not from the macros, which is how `mc_context.h`,
`idevice.h` and `itask.h` went unnoticed in the first inventory.
