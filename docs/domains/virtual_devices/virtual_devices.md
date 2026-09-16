# Virtual Devices

Hardware-free stand-ins for the camera, the PLC and the vision output, so the product can be
demonstrated and the runtime exercised on a machine with nothing attached.

Source: `src/device/virtual/` (scope card: [`src/device/AGENTS.md`](../../../src/device/AGENTS.md)).

## They are shipped code, not test doubles

They began as `Fake*Device` classes inside the architecture contract test. Phase 7 / D1 moved
them into `src/` and renamed them, and the test now drives **these** classes rather than its own
copies.

That is the point, not a tidy-up. A test double that drifts from the shipped device stops
testing anything, and the drift is silent — the test keeps passing. One implementation, two
consumers, no drift possible.

The consequence runs the other way too: **a bug in a virtual device is a product bug.** Four
were found this way during Phase 7, each of which the contract test had been passing straight
through — see "Defects these have already had" below.

## The three families

| Device | Sub-type token | Enumerator | What it substitutes |
|---|---|---|---|
| `VirtualCameraDevice` | `"Virtual"` | `CameraType::VirtualCamera` | A still image from disk, or a generated flat frame |
| `VirtualPlcDevice` | `"Virtual"` | `PlcType::VirtualPlc` | Records writes instead of sending them |
| `VirtualVisionOutputDevice` | `"Virtual"` | `VisionOutputType::VirtualVisionOutput` | Captures positions instead of transmitting them |

There is **no virtual robot**. `RobotDevice` is the one family without one, deliberately:
nothing exercises it, and an abstraction with no consumer is what `AGENT.md` warns against.
`isVirtualDevice()` records that in a comment so the gap reads as a decision.

### Why the tokens and the enumerators are spelled differently

The **token** is `"Virtual"` in all three families. Tokens are family-local — each family reads
a different JSON key (`CameraType`, `PlcType`, `VisionOutputType`) — so there is no collision.

The **enumerators** cannot be. These enums are `enum`, not `enum class`, so every enumerator
lands directly in `vc::device` and three `Virtual`s would be a redefinition. Hence
`VirtualCamera` / `VirtualPlc` / `VirtualVisionOutput`, for the same reason `BaslerGigE` and
`MitsubishiMc` are spelled the way they are.

> ⚠️ **The tokens are persisted into customer project files and can never be changed.** A
> project saved with `"Virtual"` will look for `"Virtual"` forever.

### Registry order is load-bearing

`DeviceRegistry::displayNamesFor()` returns the registry table's order verbatim, the Add Device
wizard's combo leaves index 0 current, and the wizard writes back whatever is current. **The
first entry of a family is what an operator creates by not choosing.** Every `Virtual` row is
registered *last* within its family, and the contract test asserts the order rather than just
membership — put one first and the default camera silently becomes a simulated one.

## The failure flags are features

`connectSucceeds`, `grabSucceeds`, `sendSucceeds` are not test hooks. A camera that cannot
connect and a send that fails are the two cases the runtime's recovery path exists for, and on
a device with no hardware the only way to produce one is to ask.

## Camera: the frame, and the calibration

**The frame.** `imagePath` replays a still image; when it is empty or unreadable the device
falls back to a flat `frameWidth` × `frameHeight` frame at `frameGreyLevel`. The fallback is not
an error path — a generated frame exercises grab, connect and recovery perfectly well. It is
only *matching* that needs a real image, because a flat frame correctly finds nothing.

**The calibration is synthetic, and that word is doing real work.**

`LocalizationRuntimeController::validateActiveCameraCalibration()` faults a task whose active
camera reports `isCalibrated() == false`, and the real calibration workflow (board setup,
threshold tuning, corner detection) lives inside `BaslerCameraWidget`. Without an answer here a
virtual camera could be created, opened and configured — and never run.

The device generates a 3×3 grid of pixel↔millimetre correspondences across the frame at the
configured `millimetresPerPixel` and runs `Calibrator::calibrate()` on them.

> **The homography is real; the geometry is asserted.** Nothing about the fit is faked — it is
> the same `calibrate()` a real board goes through, RANSAC and plane fit included. What is
> asserted is the **scene**: that it is flat, axis-aligned, at the declared scale and origin.
>
> Positions this camera reports are millimetres of a plane *somebody declared*, not of anything
> measured. That is why the scale is an explicit, visible property rather than a hidden
> constant.

`millimetresPerPixel = 0` is the documented opt-out: it leaves the camera uncalibrated, and the
runtime refuses it **exactly as it refuses a real uncalibrated camera**. The runtime does not
know or care that the device is virtual, which is the property worth protecting.

| Property | Meaning |
|---|---|
| `Still image path` | Image replayed by every grab; empty = generated frame |
| `Frame width/height (px)`, `Frame grey level` | The generated fallback frame |
| `Millimetres per pixel` | Declared scale of the synthetic calibration; **0 disables it** |
| `Origin X/Y (mm)` | Robot-frame position that image pixel (0,0) maps to |
| `Work plane Z (mm)` | Height of the assumed flat plane |
| Exposure, gain, acquisition rate, backlight | **Stored, never applied.** There is no sensor. They exist because `CameraCfg`'s contract is not optional, and they round-trip so a save/load stays lossless |

## PLC: the tag space is what makes it usable

`VirtualPlcDevice` implements `IPlcTagProvider`, advertising `M0…M<n-1>` and `D0…D<n-1>` whose
sizes are config properties (128 each by default).

This is not decoration. `LocalizationSettingWidget` builds the signal-map editor's lists from
`IDigitalIoProvider`/`IWordIoProvider` and **clears both lists** when the device implements
neither. Before the provider existed, a virtual PLC could be created, connected and written to —
and no task using it could ever be configured.

**Tag names are validated, not merely accepted.** A write to a tag that is not a valid MC
address is rejected the way a real PLC rejects it, and every advertised tag is asserted writable
by the contract test. A virtual device that accepted anything would make a signal-mapping
mistake invisible until it reached real hardware.

## Vision output

Captures the positions it is sent; `capturedPositions` holds the last request and every push is
counted, including rejected ones — "how many times did the runtime try to send" and "how many
succeeded" are different questions and the failure path is where they diverge.

The **robot pick check still gates a hardware-free run**, and since Phase 9 / F1 that no longer
depends on this device: the runtime reads the check from the task
(`TaskLocalizeConfig::robotCheckConfig()`), so a virtual output cannot let through a pose a real
robot could never reach. The device still inherits an `m_kinematicCheck` config from
`VisionOutputDeviceCfg`, but the localization task no longer asks for it and this device runs no
advisory check of its own.

## Making virtual obvious — risk R8

The risk: a station handed over "working" while running on simulated hardware. Everything
passes and nothing moves.

Three markers, **one predicate** — `vc::device::isVirtualDevice()`:

| Marker | Where |
|---|---|
| **VIRT** chip, replacing the family chip, in `state.warning` | Project tree |
| "SIMULATED DEVICE — no hardware is attached" banner | The device panel (`VirtualDeviceWidget`) |
| One `[USER][WARN]` line per virtual device, every runtime start | The shared log |

The chip **replaces** the family label rather than sitting beside it, because the device *name*
belongs to the operator — nothing stops them calling a simulated camera "Camera 1". The marker
has to survive whatever it was named.

The log line is the only marker that survives nobody looking at the screen, which is the actual
R8 scenario.

They share one predicate on purpose: two markers disagreeing would be worse than no marker at
all, because an operator who has seen one wrong VIRT badge stops reading them.

> The runtime shell does **not** refuse virtual devices. That was considered and rejected by the
> project owner: refusing them removes the demo capability they exist for. R8 is answered by
> disclosure, not by prohibition.

## One shared device panel

`VirtualDeviceWidget` serves all three families. Its editable rows come from the config gadget's
`Q_PROPERTY` metadata, so it knows nothing about any family: a new setting appears the moment it
is declared, and a fourth virtual family would work on the day it is written.

It is dispatched from `isVirtualDevice()` **hoisted above** the family switch in
`DeviceWidgetFactory`. That hoist is required, not stylistic — the Camera and PLC arms are
written as equality *rejections* (`cameraType() != BaslerGigE`), so without it a virtual device
falls through to `nullptr` and the task page substitutes "No configuration panel available for
this device": a soft failure that reads as a missing panel rather than a missing registration.

## Defects these have already had

Kept because each one names a trap the next person can fall into.

| Defect | Root cause |
|---|---|
| Saved and reloaded with every setting reset | The device never called `IDevice::setDeviceConfig(&m_config)`, so the base — which serialises through a **non-owning** pointer — wrote an empty `DeviceConfig` |
| Reloaded camera kept its image path but grabbed the grey fallback | `IDevice::fromJson()` writes into the config through its stored pointer and **never calls `setDeviceConfig()`**, so the device gets no hook to react to being loaded. Fixed by overriding `fromJson()` |
| Crash on navigating to a virtual device | The panel had no property browser; `LocalizationTaskWidget::populateBrowser()` passed the resulting `nullptr` into `QStackedWidget::addWidget()`. Fixed on both sides |
| A virtual PLC could not be given a signal map | It implemented only `IPlcIoWriter`, not the tag providers |

The first two share a shape worth remembering: **a config that round-trips is not the same as a
device that behaves.** The round-trip test passed throughout both.

## Driving a virtual PLC's inputs

**Closed 2026-09-07** (backlog item #43). Until then `VirtualPlcDevice` recorded what the runtime
*wrote* but nothing could drive a value *in*, so a task reached `Ready` and stopped: triggering a
cycle, faulting and fault-reset all begin with the PLC changing an input, and every transition the
task state machine has was unreachable.

`VirtualPlcDevice` now implements `vc::device::IPlcInputSimulator`:

| | |
|---|---|
| Capability | `IPlcInputSimulator` — `injectInputValue(tag, value, error)` and `simulatedInputs()` |
| Reached from the UI via | `PlcRunner::requestInjectInputValue()`, queued onto the device thread |
| Offered only when | `PlcRunner::supportsInputSimulation()` — resolved **per device**, so no real PLC ever advertises it |
| UI | `VirtualPlcInputPanel`, added to the virtual device page by `VirtualDeviceWidget` |

A cycle is driven the way the PLC drives one: set the trigger tag to 1, then back to 0. The trigger
is rising-edge, so holding it at 1 does not enqueue a second cycle.

> ⚠️ **Injected inputs and recorded writes are two separate stores, and must stay that way.** A
> value the runtime *wrote* must never read back as an input. If it did, the handshake would
> complete itself — the runtime publishes `bMatchingFinished`, reads it back, and the cycle appears
> to work — and it would hide the mapping mistake where two logical signals are bound to the same
> tag. Real hardware keeps them apart because the plant owns the inputs, not the vision system, and
> a simulation that blurs the two stops proving anything. Pinned by
> `test_virtual_plc_keeps_injected_inputs_and_recorded_writes_apart`.

**Only the commissioning shell has these controls.** `ncr_runtime.exe` shows the operator dashboard
and no device pages, so a hardware-free demo is driven from `ncr_picking.exe`. That is a deliberate
choice, not an oversight: the operator shell is not a place to forge PLC inputs.

**Real PLC devices must never implement this capability.** The point of the virtual device is that
software written against it also works against hardware; a real device whose inputs this software
could forge would be lying about the plant.
