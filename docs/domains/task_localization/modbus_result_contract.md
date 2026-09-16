# Modbus vision-result register contract

**Status:** specification. Implemented by
[`modbus_result_layout.h`](../../../src/device/plc/modbus/modbus_result_layout.h) and
[`modbus_result_layout.cpp`](../../../src/device/plc/modbus/modbus_result_layout.cpp).
Read the two together; the header carries the constants, this document carries the reasoning
and the parts a PLC programmer has to know that no header can express.

> ⚠️ **This is a wire contract.** Once a customer PLC program reads these registers, the offsets,
> the scale factor, the word order and the handshake are frozen. A machine that is already
> running cannot absorb a layout change without its ladder code being rewritten and the cell
> being re-commissioned. The only safe change is an additive one into the reserved header word.
> This is the same standing as the TCP/IP result frame documented on `VisionOutputRequest` —
> and that frame's field format has not moved in the life of the product for exactly this reason.

> ⚠️ **Commissioning a server device? Allow the app through Windows Defender Firewall first.**
> Our server binds, the panel says *Connected*, and no master on the network can reach it —
> Windows drops the connection before it reaches the socket, so nothing appears in the log.
> The client sub-type is unaffected, which makes a client/server pair on one station look
> half-broken. Details and the exact settings:
> [`../../product/customer_installer_packaging.md`](../../product/customer_installer_packaging.md)
> → "Windows Defender Firewall".

---

## 1. Where the block lives

The result block occupies **holding registers (4x)** starting at the configured
`Result Start Address` (`resultStartAddress`, default **1000**).

### The server may publish into input registers instead

The **client** sub-type has no choice. It publishes by writing into the slave's memory, and the
Modbus protocol gives a master no function code that writes an input register — 4x is the only
word area it can reach.

The **server** sub-type owns its register space, so it may publish into **input registers (3x)**
instead, by setting `Result In Input Registers` on the device. Everything else is identical: the
same offsets, the same encoding, the same count-last handshake. Only the reference class changes.

**Why you would choose it.** A master can write holding registers. That means a PLC program — or a
second master, or a commissioning tool left connected — can overwrite the vision result block,
accidentally or otherwise. In input registers it physically cannot: there is no function code for
it. If nothing on your network needs to write into the result block, 3x is the safer home for it.

**Why it is not the default.** A PLC program pointed at 4x will not find a block that moved to 3x,
and the default has to match what the client does and what this document has always described.
Changing it is a commissioning decision, made once, before the PLC program is written.

It holds a **4-register header** followed by `Result Max Positions` position slots of
**12 registers each**:

```
offset  registers  contents
------  ---------  ------------------------------------------------------------
  +0        1      count      number of valid positions (0..maxPositions)
  +1        1      sequence   publish counter, 1..32767, wraps to 1
  +2        1      flags      bit0 = low area, bit1 = truncated
  +3        1      reserved   always written as 0
  +4       12      position 1
 +16       12      position 2
  ...
```

Total registers = `4 + 12 × maxPositions`. At the default of 8 positions that is **100
registers**, occupying 1000..1099.

Both Modbus sub-types use this layout unchanged. A PLC program written against the
**client** device (we write into your slave) works against the **server** device (you read our
registers) without modification. That is the point of having one layout.

### The default start address is clear of the polled range on purpose

The default polled holding range is 0..63; the result block starts at 1000. Overlapping them
is permitted but is almost never what you want: the task would poll back the words the vision
result just wrote, and the signal map would fill with half-encoded pose values. The device
warns on overlap rather than refusing it, because a customer PLC may genuinely want the result
inside a block it already reads.

---

## 2. Position encoding

Each position is **six axes in this order**:

```
x, y, z, rx, ry, rz
```

matching `VisionOutputPosition` and the TCP/IP result frame exactly. `rz` is the top-down pick
rotation that the pre-Phase-5 four-axis contract called `r`.

Each axis is a **signed 32-bit fixed-point value, scaled by 100, stored high word first**:

```
register 2k     high word (bits 31..16)
register 2k+1   low  word (bits 15..0)

value_mm = int32(high << 16 | low) / 100
```

So `12.34 mm` is `1234`, and `-5.00 mm` is `-500` (0xFFFF 0xFE0C).

### Why scaled int32 rather than IEEE-754 float

Both take two registers, so the choice is not about size.

- **It matches the resolution the product already ships.** The TCP/IP result frame has always
  sent `%08.2f` — two decimals. Choosing ×100 means the identical commissioned pose reads
  identically on both transports. A ×10 scale in a single register would have been cheaper but
  would have quietly halved the precision of every station that migrated from TCP/IP to Modbus,
  and it caps at ±3276.7 mm — inside the working envelope of a real robot cell.
- **It avoids the float word-order trap.** Modbus specifies byte order *within* a register and
  says nothing about the order of two registers making a 32-bit value. Float layouts therefore
  differ between vendors, and the failure mode is not an error — it is a plausible-looking wrong
  number. An integer has exactly the same ambiguity, but a wrongly-ordered integer is usually
  absurd (millions of millimetres) where a wrongly-ordered float is often merely wrong.
- **Ladder code handles integers more easily.** Scaled integers compare, add and store without
  a floating-point instruction set, which some of the controllers this product is deployed
  against do not have.

**Word order is high word first, and this is a choice, not a standard.** If your master reads
these as 32-bit values and gets nonsense in the millions, swap the word order at your end.

---

## 3. The publish handshake

The count word is the handshake. The vision side publishes in this order, always:

1. **Write `count` = 0.** A master that reads 0 knows a publish is in progress and that the
   payload beneath is being rewritten.
2. **Write the full payload** — all `maxPositions × 12` registers, including the zero-filled
   tail past the last valid position.
3. **Write `sequence` and `flags`.**
4. **Write `count` = N, last.**

A master should therefore:

- Read `count`. If 0, there is no result to take — either none has been published since
  power-on, or one is being written right now.
- If non-zero, read `sequence` and the first `count` position slots.
- Re-read `count` and `sequence` and confirm they are unchanged. If they moved, the block was
  republished mid-read; discard and retry.

`sequence` exists so that two consecutive cycles producing the *same* count are still
distinguishable. It runs 1..32767 and wraps back to 1 — never 0, so 0 means "nothing published
since power-on". It stays inside the positive range of a signed 16-bit word because ladder code
reads registers as signed by default and a sequence that suddenly went negative would look like
a fault.

### Why the count word must be written last

At the default of 8 positions the whole 100-register block fits one Modbus *write multiple
registers* request, whose payload ceiling is 123 registers — so the common case is genuinely
atomic on the wire. **Above 9 positions it is not.** `4 + 12 × 10 = 124` exceeds the ceiling,
so the publish necessarily splits into more than one request and a master polling in between
can see a half-written block. Writing `count` last, after everything it describes, is what makes
that safe. It is not decoration; it is the only thing standing between a two-request publish and
a robot picking at a pose from the previous cycle.

---

## 4. Flags

| Bit | Name        | Meaning |
|-----|-------------|---------|
| 0   | `lowArea`   | The matcher reported the matched area below its configured limit for this cycle. Advisory: the positions are still valid. |
| 1   | `truncated` | More positions were produced than the block can hold. The extra ones are **not** in the registers. |

All other bits are reserved and written as 0.

---

## 5. What a PLC programmer must **not** assume

- **`count` is not "how many objects the camera found."** It is how many positions fit in the
  block and passed every gate — the workspace crop, the calibration conversion, and (when
  enabled) the robot reachability and self-collision check. A cycle can detect six objects and
  publish two. If you need the detected number, map the task's `nDetectedNumber` signal; it is a
  different quantity and it is published separately.
- **A non-zero `count` does not mean the positions are new.** Nothing clears the block between
  cycles other than the next publish. Use `sequence` to tell a fresh result from the previous
  one still sitting in the registers.
- **The payload past `count` is not empty of meaning — it is zero.** The device writes the full
  payload length every publish, zero-filling the tail, precisely so the previous cycle's poses
  cannot be read as current by a program that trusts a stale `count`. Do not rely on the tail
  holding anything; do not read past `count`.
- **Word registers the task reads are signed.** A task number signal is a signed 16-bit quantity
  throughout this application, so a holding register your program writes as 40000 reaches the
  task signal map as **-25536**. This is the same behaviour the Mitsubishi MC family has always
  had for D devices. Keep values the task reads inside 0..32767 unless you intend the wrap.
- **"Read-only" means read-only *to you*, the master — not to the server.** The Modbus protocol
  defines no function code by which a master writes a discrete input (1x) or an input register
  (3x), so the client sub-type refuses such a write locally, before anything reaches the wire.
  The **server** sub-type is the opposite case: it *owns* those areas and is the only side that
  can put a value in them. On a server, a `DI…` or `IR…` tag is a **one-way outward** signal —
  the task writes it, you read it — where a coil or holding register is two-way.

  So: if this product is your **master**, anything the task must write has to live in coils or
  holding registers. If this product is your **slave**, all four areas are available, and the two
  input areas are the ones you cannot corrupt from your side.
- **Axis count is six and the wire depends on it.** Adding an axis moves every register after
  the first position. If a future phase needs a seventh axis, it goes in a new block at a new
  address, not by widening this one.

---

## 6. Tag names

The register tags the task signal map is written against are
`<prefix><5-digit zero-padded address>`:

| Area | Prefix | Example | Master may write | Server may write |
|---|---|---|---|---|
| Coils | `COIL` | `COIL00100` | yes | yes |
| Discrete inputs | `DI` | `DI00008` | **no** (no function code exists) | yes — it owns them |
| Holding registers | `HR` | `HR01000` | yes | yes |
| Input registers | `IR` | `IR00016` | **no** (no function code exists) | yes — it owns them |

The two columns are different questions and this product answers them separately. Conflating them
is not hypothetical: the server sub-type originally refused to write the two areas it is the only
legitimate writer of, because it asked the master's question.

**These strings are persisted in saved projects.** A task's `bExecuteTrigger` is stored as the
literal text `"COIL00100"`, so the prefix and the padding width are as frozen as the register
layout is.

The prefixes deliberately do not collide with the Mitsubishi family's `M` and `D`. If they did,
a project switched from an MC device to a Modbus device would resolve *some* tags by accident
and bind them to unrelated registers — half-working in silence. With disjoint prefixes the
switch fails loudly on every tag, which is the outcome you want when the underlying hardware
changed.

---

## 7. Worked example

Task publishes one position at `x=125.50, y=-40.25, z=0.00, rx=0, ry=0, rz=37.10`, no low area,
nothing truncated, third publish since start. Result block at 1000, capacity 8.

```
HR01000  count      = 1
HR01001  sequence   = 3
HR01002  flags      = 0x0000
HR01003  reserved   = 0
HR01004  x high     = 0x0000        x = 12550 / 100 = 125.50
HR01005  x low      = 0x3106
HR01006  y high     = 0xFFFF        y = -4025 / 100 = -40.25
HR01007  y low      = 0xF047
HR01008  z high     = 0x0000        z = 0
HR01009  z low      = 0x0000
HR01010  rx high    = 0x0000        rx = 0
HR01011  rx low     = 0x0000
HR01012  ry high    = 0x0000        ry = 0
HR01013  ry low     = 0x0000
HR01014  rz high    = 0x0000        rz = 3710 / 100 = 37.10
HR01015  rz low     = 0x0E7E
HR01016..HR01099    = 0             (positions 2..8, zero-filled)
```
