# MC Protocol Domain

Mitsubishi MELSEC Communication protocol support: how a frame, a transport and a
context compose into one `McProtocolDevice`, what each frame family is for, and
what is proven on real hardware versus only against reference bytes.

Source:

- `src/device/plc/mc_protocol/` — device, contexts, frame codecs, transports
- `src/device/AGENTS.md` — the module scope card, including the composition table
- `tests/mc_frame_test/` — byte-exact codec cases

## Three Independent Axes

Frame codec, transport and context are separate concerns that compose freely.
`McProtocolDevice` dispatches on the frame type and the message-interface type in
**two independent switches**, and nothing else in the device knows which pair it
was handed.

| Axis | Interface | Implementations |
|---|---|---|
| Frame codec | `MCFrameAbstract` | `Frame3E` (binary), `Frame1C`, `Frame3C` (ASCII computer link) |
| Transport | `McMsgInterface` | `McEthernetTcpPort`, `McMsgSerialPort` |
| Context | `McContext` | `Context_Mc3E`, `Context_Mc1C`, `Context_Mc3C` |

The pairing is not arbitrary in practice — 3E is the Ethernet binary frame and the
C-frames are the serial computer-link frames — but the code does not enforce a
pairing, and it should not start to. The axes stay independent so a frame can be
bench-tested over a transport it does not ship on.

## Frame Families

| Frame | Encoding | Typical transport | Status |
|---|---|---|---|
| `Frame3E` | Binary | Ethernet TCP | **Shipping.** The known-good control case |
| `Frame1C` | ASCII | Serial (C24 computer link) | Connect and read confirmed on real hardware |
| `Frame3C` | ASCII | Serial (C24 computer link) | Connect and read confirmed on real hardware |
| `Frame_1E` | — | — | **Declared in the enum, no factory arm.** Deliberately unbuilt; must stay out of the Add Device wizard's frame combo until it has one |

## Adding A Frame

Five edits, and missing any one of them produces a codec that compiles, passes its
tests and can never be selected:

1. A context (`McContext` subclass) **with a `kDisplayNameSources[]` entry**
2. A codec (`MCFrameAbstract` subclass)
3. An arm in `Factory::contextFactory()`
4. An arm in `McProtocolDevice::initialize_mc_device()`
5. Cases in `tests/mc_frame_test`
6. **An entry in the Add Device wizard's frame combo**

The last one is the trap. The frame is a `Q_PROPERTY(... CONSTANT)`, so a frame
absent from that combo cannot be chosen by anyone and the whole codec is dead code
that still builds and still passes its suite.

## Serial Parameters Must Match The C24 Module

For 1C and 3C, the station number, PC number, frame format and sum-check setting
have to match how the C24 module is configured. These are not defaults the app can
infer; they come from the module's own settings.

## What The Tests Can And Cannot Prove

`tests/mc_frame_test` (38 cases) proves a codec is **byte-correct against reference
frames**. That is the right thing for it to prove and it catches real defects.

It cannot prove a frame is accepted **on the wire**. Sum-check, station number, PC
number and access-route handling can differ *per command*, so a codec that passes
every reference case can still be refused by a real C24 for one command and not
another. Phase 7 closed with five defects that compiled clean and passed the suite;
the C-frames are the same class of work.

> ⚠️ **Current coverage gap.** 3E is proven with no regression. 1C and 3C are
> proven to **connect and read** against a real C24 module (owner-confirmed
> 2026-09-03). The remaining read/write commands are **not tested on hardware**, and
> **write commands are the ones that matter most**. There are also no measured
> latency figures: `tools/mc_protocol_bench` is built and smoke-tested but has never
> been pointed at a PLC.
>
> Tracked as **item 56** in [../../backlog/later_todo_list.md](../../backlog/later_todo_list.md).
> Do not read "Phase 8 closed" as "the MC protocol is fully exercised".

## Diagnosing A Fault

Start with the log. Connection-level failures and frame-level rejections are
rendered with named codes rather than raw numbers. For the Modbus equivalent of this
workflow — poll plan, register space, named exception codes — see the logging notes
in `src/device/AGENTS.md`; the MC side follows the same principle of dumping failed
exchanges in full regardless of whether tracing is enabled.

## Related

- `src/device/AGENTS.md` — module scope card and device-layer traps
- [../task_localization/plc_signal_contract.md](../task_localization/plc_signal_contract.md) — the logical signal contract an MC PLC drives
- [../../architecture/device_type.md](../../architecture/device_type.md) — device family/sub-type registry notes
