# Operator Runtime Shell (`ncr_runtime.exe`)

The operator-facing executable: pick a project once, and from then on the machine
starts by starting the software. It shows task dashboards and nothing else.

Source: `runtime_app/` (scope card: [`runtime_app/AGENTS.md`](../../../runtime_app/AGENTS.md)),
split into `runtime_app/src/` (entry point, tile-layout controller) and `runtime_app/ui/`
(the shell window and its `.ui` form).

## Menus

| Menu | Item | What it does |
|---|---|---|
| **Project** | Load… | Choose a `.vproj` and start running it |
| | Close | Stop every running task and return to the project-select page — never a blank window |
| | Open Editor | Hand off to the commissioning shell (see "Switching Between The Two Applications") |
| **View** | System log | Show the shared log viewer as a dock; created on first use, because an operator watching dashboards has no use for it and it would otherwise cost a tile's worth of screen |
| | Layout | Tile count: 1 / 2 / 4 / 6 / 8 (see "Layout") |
| | Theme, Language | The same submenus the commissioning shell has. Declared in each shell's own `.ui` — only the exclusivity and the behaviour are wired in code, because Designer can express neither |

The task count, the project name and any cap notice go in the **status bar**.

## Why A Second Executable

Phase 4 chose one executable with Commission and Runtime modes and explicitly deferred a
split. **Phase 6 reversed that on 2026-08-19 by the project owner's decision.** The
original reasoning is preserved in
[../../product/phase4_product_release_plan.md](../../product/phase4_product_release_plan.md)
— it is still the right default for a single-shell product, and the costs it lists are now
obligations this document has to answer:

| Cost Phase 4 named | How it is answered |
|---|---|
| Two executables drift apart | Both build from `qmake/app_common.pri`. A shell `.pro` adds only its own `.pri`, icon and install rules. The contract test asserts both `.pro` files include the shared config. |
| Project-file compatibility between them | There is none to maintain: the runtime shell **never writes** a project file. One writer, one reader. |
| Two deployment manifests | `deploy_dependencies.pri` is included from the shared config, so each target deploys its own runtime by construction rather than by someone remembering. |
| Shell coupling | The two shells are **peers**, not a hierarchy: neither may include the other's headers, enforced by the layering contract test. |

## The Two Shells

|  | `ncr_picking.exe` | `ncr_runtime.exe` |
|---|---|---|
| Purpose | Commissioning: devices, calibration, patterns, signals | Operation: run tasks, watch dashboards |
| Project file | Reads **and writes** | Reads only |
| Entry | `components/app/` | `runtime_app/` |
| Task UI | `LocalizationTaskWidget` (starts Commission) | `LocalizationDashboardWidget` (read-only) |
| Starting runtime | Operator presses Enter Runtime | Automatic on launch |

> The runtime shell reuses `LocalizationDashboardWidget` and **must not** reuse
> `LocalizationTaskWidget`: the latter starts Commission on the task inside its
> constructor, which would put the operator executable into commissioning on launch.

## One Process For The Whole Product

Both shells own hardware exclusively — the Basler camera (Pylon grants exclusive access),
the MC PLC socket, and the vision-output TCP listen port. A second process cannot take
any of them, and what it produces instead are failed grabs, "device removed", and a port
that will not bind: **symptoms indistinguishable from real hardware faults.**

On a field machine a second launch is routine, not an edge case — the runtime starts at
boot *and* an operator may click its icon. So both shells hold a `SingleInstanceGuard`
(`src/core/utils/single_instance_guard.h`) on **one shared key**,
`vc::shell::ShellHandoff::kInstanceKey`: the second launch does not start, it asks the
running instance to come forward and exits.

> **Phase 7 changed this.** Phase 6 gave each shell its own key, so they blocked themselves
> but not each other, with the note that mutual exclusion "needs an ordered device hand-off,
> not a refusal to start". Phase 7 built that hand-off, so the refusal became the right
> default. **Only one of the two applications runs at a time**, and switching goes through
> the menu rather than through closing one and launching the other.

- **Crash recovery is automatic.** The guard's lock file records the owner's PID; a lock
  whose owner is gone is reclaimed. A killed process cannot lock the application out of
  its own machine.
- **Scope is the current user.** Two Windows accounts can each run one instance. This
  matters for boot-start: run the scheduled task as the operator's account, not a service
  account, or an interactive login can produce a second runtime.
- **A launch that loses says which shell won.** The lock file carries the holder's
  application name, so starting the editor while the runtime is up reports *"NCRN Pick
  Runtime is already running…"* and points at the menu action, rather than silently
  bringing forward an application the user did not ask for.

## Switching Between The Two Applications

**Project → Open Editor** in the runtime, and **File → Open Runtime** in the commissioning
app. It is a hand-off, never an overlap:

1. **Authorise.** Entering commissioning requires the `Admin` role — a line worker must not
   reach device and pattern configuration by mis-tapping. Going the other way needs no
   password: handing authority back is not an escalation.
2. **Confirm.** In both directions, and even when the user already holds `Admin`. The
   password answers *"are you allowed?"*; this answers *"did you mean to?"*, and a mis-tap
   by an authorised person still stops a production line.
3. **Release.** The runtime calls `endRuntime()` on every task it started; the editor closes
   its project (offering to save first). If that fails the switch is abandoned and nothing
   changes — handing off into a half-released camera is worse than not switching.
4. **Launch and exit.** The sibling is found next to this executable
   (`applicationDirPath()` plus a fixed filename — which is why both must ship in one
   folder), started with `--handoff`, and this process quits.

> **The incoming shell waits; it does not race.** The outgoing process still holds the
> instance lock while it shuts its devices down, so a shell started with `--handoff` retries
> for up to 15 s instead of giving up at once. Without that wait the new shell would exit
> with no window at all — which from the operator's side is indistinguishable from the menu
> item doing nothing. If the wait does expire, it says so rather than vanishing.

## Startup

**The window is shown and takes the foreground first; the project loads second.** Loading
reads the project file, decodes every stored training image, and opens the camera, the PLC
socket and the vision-output port — seconds, and the duration depends on the hardware. Doing
that inside the window's constructor meant the process existed with no window on screen for
the whole time, which broke a hand-off from the commissioning shell:
`AllowSetForegroundWindow()` does **not** expire with time, but it is revoked by the next
user input not directed at the granted process — and an operator staring at a bare desktop
clicks something. That is why the fault appeared only in the editor → runtime direction (the
commissioning window costs milliseconds to build) and why it was intermittent rather than
constant: it depended on whether anyone clicked during the gap.

So `RuntimeShellWindow`'s constructor builds UI only, and `beginStartup()` does the loading.
`main()` posts it on a queued call **after** `presentShellWindow()` — posting it from the
constructor would queue it ahead of that function's foreground claim and reinstate the bug.
Measured on the hand-off path: claim at **+197 ms** after process start, against **+3795 ms**
before the change.

Two consequences worth knowing:

- **The form starts on the project-select page**, showing *"Starting the last project…"*, not
  on an empty runtime page.
- **Project → Load…, Project → Open Editor and Browse are disabled while `beginStartup()`
  runs.** Each opens a modal dialog, and a modal dialog's nested event loop can dispatch a
  second trigger into `startProject()` while the first is still inside `stopTaskRuntimes()`,
  which iterates `m_runningTaskIds` and `m_taskDocks` and clears them only afterwards. System
  log, Theme and Language stay live — they touch neither the project nor the devices.
- **Log order changed.** *"Operator runtime shell ready."* now precedes *"Runtime shell
  started project."*; it used to follow it.

```text
read AppSettings::lastRuntimeProjectPath()
├── empty            → project-select page: "No project has been run yet."
├── file missing     → project-select page: "…could not be found. It may have been
│                       moved, renamed, or its drive is not connected."
├── load fails       → project-select page: "…exists but could not be loaded: <reason>"
└── load succeeds    → runtime view, tasks already running
```

Three failure modes, three different messages, on purpose. They call for three different
actions — do nothing yet, find the file, repair the file — and a single generic message
costs the operator the time to work out which one they are looking at.

The remembered path is written **only after a load succeeds**, so a corrupt or missing
file never becomes the one the next launch tries first.

## Task Lifecycle In The Shell

For each localization task, in task-id order, capped at 8:

1. create a dock and a `LocalizationDashboardWidget` for it;
2. call `beginRuntime()`.

No button press anywhere. An operator starts the machine, not the software.

**Failures stay visible.** A task whose setup fails goes Faulted and keeps its tile
showing that state. Tasks past the cap are named in the user log and in the window's
status bar. The rule behind both: a running or broken task that is not on screen is a
task nobody is watching, which is worse than a crowded display.

On close (and on switching projects) the shell calls `endRuntime()` on every task it
started, so device threads stop before the process exits rather than being torn down
underneath a running cycle.

## Layout

Tile counts and their grids, chosen from **View → Layout**. Stated as **columns × rows**,
which is what `RuntimeLayoutController::gridFor()` returns (`QSize(columns, rows)`) —
the class doc comment describes the same grids the other way round, as rows × columns,
so read the axis label rather than the numbers:

| Tiles | Columns × Rows | Shape |
|---:|---|---|
| 1 | 1 × 1 | single pane |
| 2 | 2 × 1 | side by side |
| 4 | 2 × 2 | square |
| 6 | 3 × 2 | two rows of three |
| 8 | 4 × 2 | two rows of four |

The default is the smallest grid that fits the running task count. Docks beyond the tile
count are tabbed into the last cell rather than hidden — same rule as above.

**Floating is an operator decision and is never undone by a layout change.** A floated
dock is skipped when the grid is rebuilt and does not consume a tile. Floating is a view
operation: a floated task keeps running.

ADS provides docking and floating natively; the grid itself is an explicit sequence of
`addDockWidget()` placements in `RuntimeLayoutController`, because ADS has no "tile into
N" primitive.

> ⚠️ **Never block a Layout action's signals.** The tile count is read back from
> `QActionGroup::checkedAction()`, and `QActionGroup` learns which action is checked from
> `QAction::changed`. Wrapping `setChecked()` in a `QSignalBlocker` hides that, the group
> reports nothing checked, and `applyCurrentLayout()` docks **nothing** — the operator gets
> an empty window with the task docks stranded at their size hint over the menu bar. It is
> also unnecessary: `setChecked()` never emits `triggered()`, which is the only signal
> wired to a slot. A contract test now enforces this for both shells.
>
> `applyCurrentLayout()` treats a missing checked action as a defect, not as "dock
> nothing": it falls back to the smallest fitting grid and logs an error, because this
> shell owns the screen and a blank one is the worst possible outcome.

## Starting With Windows

On a field machine the runtime should come up by itself, so a worker starts the machine
rather than the software.

### Use Task Scheduler, not the Run key or the Startup folder

The obvious mechanisms are the wrong ones. `HKCU\...\Run` and the Startup folder both fire
the instant the desktop appears, with no delay, no restart policy, and no control over the
account. Task Scheduler gives all three.

Create a task with:

| Setting | Value | Why |
|---|---|---|
| Trigger | **At log on**, delay 30–60 s | Not "At startup" — see the session warning below |
| Action | `<install dir>\ncr_runtime.exe` | Start-in set to the install dir |
| Run as | **the operator's own account** | See "the account matters" below |
| Security option | **Run only when user is logged on** | Anything else hides the window |
| On failure | Restart after 1 min, **bounded** (e.g. 3 attempts) | An unbounded restart hammers a machine that cannot start at all |
| Conditions | Clear "Start only if on AC power" | Irrelevant on a fixed station and blocks the start on some hardware |

Machines that must reach the runtime with nobody present need Windows **auto-login**
configured as well; the scheduled task then fires on that automatic logon.

> ⚠️ **"Run whether user is logged on or not" makes the application invisible.** It places
> the process in a non-interactive session with no desktop, so the runtime starts, runs,
> holds the camera and the PLC socket — and shows nothing on screen. The station looks
> dead while it is in fact working. This is the single most common way to get this wrong.

### The account matters

`SingleInstanceGuard` scopes itself to the current user (its lock file lives in the user's
temp directory). If the scheduled task runs as a service account while the operator is
logged in interactively, the two processes cannot see each other's lock and **both will
start** — straight into the camera/PLC/port conflict the guard exists to prevent.

Run the task as the same account that logs in.

### Why a delay, and why it is not critical

A GigE camera needs the network adapter up and Pylon device enumeration ready; a PLC needs
its link. At the instant the desktop appears, none of that is guaranteed.

**Phase B made this non-fatal.** Reconnect is unlimited, so a runtime that starts before
the network is ready enters recovery and comes to Ready by itself once the devices appear —
no operator action, no fault. Before Phase B it would have exhausted a 10-attempt budget
and faulted on every single boot.

So the delay is not a correctness requirement any more. It buys a clean event log instead
of one that opens with a burst of recovery churn at every start, which is worth having when
the log is the first place anyone looks.

### Auto-start plus a manual launch is normal

An operator who does not see a window will click the desktop icon. That is the expected
case, not an edge case: the single-instance guard blocks the duplicate and brings the
running window forward. The same guard protects the scheduled task's own restart attempts
from stacking a second process on top of a healthy one.

### The route out of a bad project is supported, not accidental

Auto-start means the runtime owns the screen, so it must never be a dead end. It is not:

- a missing or unreadable project file lands on the **project-select page** with a message
  naming which of the three cases it is, rather than failing to start;
- from there the operator can browse to another project without any tooling.

To reach commissioning, use **Project → Open Editor**. It asks for the administrator
password, confirms that every running task will stop, releases the devices and starts
`ncr_picking.exe` in this application's place — see "Switching Between The Two
Applications". Launching `ncr_picking.exe` directly while the runtime is up no longer
starts a second process; it reports that the runtime is running and brings it forward.

## What This Shell Does Not Do

- Edit or save anything in the project file.
- Offer pattern, device, calibration or signal configuration.
- Acknowledge faults. Fault acknowledge is a PLC input (`bErrorReset`) or the automatic
  2-second recovery — see
  [../task_localization/plc_signal_contract.md](../task_localization/plc_signal_contract.md).
