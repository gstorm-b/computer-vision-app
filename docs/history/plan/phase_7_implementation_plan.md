# Phase 7 Implementation Plan — Shell Unification, Mode Switch, Virtual Devices

**Date:** 2026-08-23
**Status:** **Phase A ✅ DONE (2026-08-23)** — A1–A5 implemented; contract test **54 passed / 0 failed**;
both shells build, and **Checkpoint A is fully confirmed by the project owner** including the
GUI items (editor menubar, theme shared across both shells, access-level dialog).
**Phase B ✅ DONE (2026-08-23)** — B1–B3 implemented; contract test **56 passed / 0 failed**;
the hand-off lock race was proven end-to-end with the real executables, and
**Checkpoint B is fully confirmed by the project owner** including the GUI paths.
**Phase C ✅ CODE COMPLETE (2026-08-23)** — C1–C3 implemented; contract test **58 passed /
0 failed**; both shells build, start and close cleanly.

**Checkpoint C confirmed by the project owner** with one exception. Verified working: every
menu bar item, Theme and Language switching, Project → Load…, the system-log dock, the runtime
dock layout, and `btn_browse` after it was wired. **Not resolved: the editor → runtime
hand-off still does not raise the runtime window** — two contributing defects were found and
fixed and it still reproduces, so it is carried as **backlog #41** with the ruled-out list, at
the owner's direction, rather than blocking Phase D.

Defects found and fixed during Checkpoint C: the dock host was never sized; the task docks were
never docked at all (`QSignalBlocker` defeating `QActionGroup` — now guarded by contract test
#58, proven to fail against an injected violation); `btn_browse` was unwired; and the runtime
shell's multi-second startup ran before its window existed. A sweep for the `btn_browse` defect
class found three more dead controls in the commissioning shell — **backlog #40**.
**Phase D ✅ DONE (2026-08-23)** — D1–D3 implemented; contract test **64 passed / 0 failed**;
both shells build. Checkpoint D is closed with one item partial: a hardware-free project reaches
`Ready` but cannot be driven past it, because no PLC **input** value can be injected — carried as
**backlog #43**. Reference doc:
[../../domains/virtual_devices/virtual_devices.md](../../domains/virtual_devices/virtual_devices.md).

---

## Phase 7 — closed

| Phase | Outcome |
|---|---|
| **A** — shared UI, settings, access control | ✅ owner-confirmed, GUI included |
| **B** — one instance key, ordered hand-off | ✅ owner-confirmed, race proven with the real executables |
| **C** — `runtime_app/` restructure and menubar | ✅ owner-confirmed **except** the editor → runtime foreground raise (**backlog #41**) |
| **D** — virtual devices | ✅ Checkpoint D closed, one item partial (**backlog #43**) |

**Contract test: 54 → 64 cases** across the phase. Every new case was written against a specific
invariant this phase introduced, and the safety-critical ones were proven to fail against an
injected violation before being trusted.

**Carried forward:** #37 (editor's dock host builds its layout in code), #38 (a zero-task project
lands the runtime on a blank page), #39 (flaky instance-lock test), #40 (three dead controls in
the editor), #41 (hand-off foreground), #42 (wizard shows the raw JSON token), #43 (virtual PLC
value injection).

**Five of those seven were found by the project owner running the software**, not by a test or a
review — worth remembering when weighing how much of the next phase's confidence should rest on
the contract test alone.
**Source request:** [../request/phase_7_request.md](../request/phase_7_request.md)

> **Location note.** Same exception as Phases 5 and 6: this file lives under
> `docs/history/`, which `AGENT.md` declares traceability-only, but it is the CURRENT plan
> for Phase 7. The standing recommendation to move `docs/history/plan/` to `docs/plan/` and
> link it from `docs/README.md` is now three phases old.

---

## Overview

The request has six threads. They are not independent: three of them all need the same
missing thing — **UI that both shells can use** — and two of them are the two halves of one
mechanism.

| # | Request | Reduces to |
|---|---|---|
| 1 | Fix `RuntimeShellWindow`'s rule violation; restructure `runtime_app/` into `src/` + `ui/` | Backlog #35, plus a folder move |
| 2 | Switch between runtime and editor | The Phase 6 "deferred mode switch", now in scope |
| 3 | One app at a time, one instance only | One shared instance key; **prerequisite** for #2 |
| 4 | Shared AppSettings and AppLogger; log both apps' open events | Backlog #36, plus log lines |
| 5 | Runtime menubar: Project (Load/Close/Open Editor), View (System log/Theme/Language) | Needs shared UI components that do not exist yet |
| 6 | Virtual devices under `tools/virtual_device/` for unit tests | Extract the fakes that already live inside the contract test |

**The load-bearing observation:** every menu item the request asks the runtime shell for
already exists — in `app/`. And `app/` is exactly what `runtime_app/` may not include: the
two shells are peers, enforced by
`tests/architecture_contract_test::test_module_include_layering_contract`. So thread 5 is
not "add a menubar", it is "move the shared parts of the editor's menubar into `src/ui/`
first". That extraction is the foundation the rest of the phase sits on, which is why it
is Phase A.

---

## Current State (verified against source, 2026-08-23)

| Entity | Location | Today |
|---|---|---|
| Runtime shell window | [runtime_app/runtime_shell_window.cpp](../../../runtime_app/runtime_shell_window.cpp) | UI built entirely in C++ (`new QStackedWidget`, `new QVBoxLayout`, `new QToolBar`, …). **No `.ui`**, no `FORMS` in `runtime_app.pri`, no `ui_*.h` include. **No menubar at all.** |
| UI rule it breaks | [docs/rules/ui_design_rules.md](../../rules/ui_design_rules.md) Rule 1.1 | "Layout primitives are authored in `.ui`"; checklist: "no layout primitives `new`-ed in cpp". Logged as backlog #35 |
| System log viewer | `app/system_log_form.{h,cpp,ui}`, registered in `app/app.pri` | Editor-only. `runtime_app/` **may not include it** where it is |
| Theme + Language menus | [app/mainwindow.cpp:256-303](../../../app/mainwindow.cpp#L256-L303) | Built inline in `MainWindow::createToolBarActions()`; not reusable |
| Access level | [app/mainwindow.cpp:346](../../../app/mainwindow.cpp#L346), [708-715](../../../app/mainwindow.cpp#L708-L715) | **Display only.** `onPrivilegeAdmin()`/`onPrivilegeStandard()` change a menu *title* and nothing else. No password, no stored state, nothing reads it |
| Instance guard | [app/main.cpp:41](../../../app/main.cpp#L41), [runtime_app/main.cpp:41](../../../runtime_app/main.cpp#L41) | Two **different** keys (`"ncr_picking"`, `"ncr_runtime"`) — deliberately, so the shells do **not** block each other. Request #3 reverses that |
| Settings file | [app_settings.cpp:143](../../../src/core/app_settings/app_settings.cpp#L143) | `QStandardPaths::AppDataLocation + "/settings.dat"`. `AppDataLocation` includes `applicationName`, which differs per shell → **two files** (backlog #36) |
| Logger file | [app_logger.cpp:24](../../../src/core/logger/app_logger.cpp#L24) | `applicationDirPath() + "/logs/app_log_<date>.txt"`. Since Phase 6 / E7b both exes share one folder, so they **already share one log file** |
| Startup log lines | `app/main.cpp`, `runtime_app/main.cpp` | Both already log one. The editor's reads `"Sytem startup..."` — a typo, worth fixing while here |
| Fake devices | [contract test main.cpp:186](../../../tests/architecture_contract_test/main.cpp#L186), [:286](../../../tests/architecture_contract_test/main.cpp#L286), [:336](../../../tests/architecture_contract_test/main.cpp#L336) | `FakeCameraDevice`, `FakePlcDevice`, `FakeVisionOutputDevice` — ~200 lines **inside** the test. No fake robot |
| `tools/` | `tools/vision_tcpip_output_device/` | One standalone app with its own `.pro`. No shared `.pri` precedent |
| Deferred mode-switch constraints | [phase_6_implementation_plan.md](phase_6_implementation_plan.md) → "Deferred: editor ↔ runtime mode switch" | Five constraints recorded while the reasoning was fresh. Phase 7 is where they get honoured |

---

## Resolved Decisions

Settled by the project owner on 2026-08-23.

| # | Decision | Consequence |
|---|---|---|
| **D1** | **A losing cross-launch says which shell is running, then raises it.** *"The operator runtime is already running. Close it, or use Project → Open Editor to switch."* | `QLockFile::getLockInfo()` returns the holder's pid, hostname **and application name**, so the message can name the shell without any extra IPC. Rejected: silently raising the other shell (reads as a bug — you click the editor icon and the runtime appears), and offering an immediate switch (a desktop double-click must not be able to stop a running line). |
| **D2** | **A real role/password gate, built now, with a provider API for a future dongle.** Role `Operator` needs no password. Role `Admin` does; in this version the password is `admin` by default. A later version will read it from a hardware dongle **or** from system settings, so the credential source must be swappable **without touching any call site**. | This is the largest single addition in the phase and it is not in the written request — see D2a/D2b below. It replaces the display-only Admin/Standard menu, which today changes a menu title and nothing else. |
| **D2a** | **The confirmation dialog is kept as well, not replaced by the gate.** Every switch between runtime and editor confirms that all running tasks will be stopped. | The two guard different things. The password answers *"are you allowed?"*; the confirmation answers *"did you mean to?"* — and a mis-tap by someone who legitimately holds the admin password is still a stopped production line. |
| **D2b** | **Credential storage is an interface, not a setting.** `IAdminCredentialProvider::verify(password)` with one implementation now (settings-backed) and a dongle-backed one later. | `AGENT.md` says no abstraction before a second implementation proves the shape; the owner has stated the second implementation explicitly, which is what makes this the exception rather than speculation. Keep it to one interface and one method — a plugin framework is not what was asked for. |
| **D3** | **Virtual devices are registered in `DeviceRegistry`** so a project can run against them with no hardware attached. | Chosen over test-only. **This contradicts the request's stated folder — see the conflict note below.** They become real device sub-types the product persists and must support, which per `AGENT.md` means updating enum/string conversion, factory dispatch, UI dispatch, persistence **and** tests for each one. |
| **D4** | **One `settings.dat` for the whole product.** Both shells read and write the same file; keys only one shell uses simply sit there. | Splitting would mean a second storage concept for no benefit, and with D5 the two shells can never co-run, so there is no concurrent-write problem. Carries R3: the path change must **migrate** the existing file, not merely repoint at a new one. |
| **D5** | **One instance key for the product**, replacing the two per-shell keys. | Reverses the Phase 6 / E1 decision, which deliberately gave each shell its own key so they would *not* block each other. The contract test asserts that; the assertion must be **inverted, not deleted**, so the reversal is visible in the diff. |
| **D6** | **`runtime_app/`'s own headers are included rooted at the shell** — `#include "runtime_app/ui/runtime_shell_window.h"`, via `INCLUDEPATH += $$PWD/..`. | `src/` is already on the include path, so a bare `"ui/..."` would shadow `src/ui/` — and the contract test would read it as a reference to the `src/ui` module, so the ambiguity would not even be reported. Which header won would depend on INCLUDEPATH order. |

### ✅ Conflict between D3 and the written request — RESOLVED 2026-08-23

**The owner chose the recommended resolution: one set of virtual devices in
`src/device/virtual/`, registered in `DeviceRegistry`.** `tools/virtual_device/` is not
created, and the contract test's three `Fake*Device` classes are deleted in favour of the
real virtual devices. Phase D below is written against this. The original analysis is kept
because it is the reason the folder in the request is not the folder that gets built.

The request says virtual devices go in **`tools/vitual_device` (dưới root)**, and describes
them as *"dùng cho trường hợp chạy unit test"*. D3 says they are registered in
`DeviceRegistry` so the product can run on them without hardware.

**Those two cannot both be true.** A device in `DeviceRegistry` is constructed by
`DeviceFactory` inside `ncr_shared`, which both shipping executables link. Code under
`tools/` is not part of `ncr_shared` and is not shipped — that is what `tools/` means in
this repository (`tools/vision_tcpip_output_device` is a standalone test harness, not
product code). Registering something that lives in `tools/` would mean shipping `tools/`.

**Recommended resolution — one set of virtual devices in `src/device/virtual/`:**

- they are product code, so they live with the other device families;
- the contract test's three `Fake*Device` classes are **deleted** and the ~20 tests that use
  them switch to the real virtual devices. That removes the duplication the request was
  reaching for, which was the point of asking for a shared folder in the first place;
- `tools/virtual_device/` is then not created at all.

The alternative — two sets, product virtual devices in `src/device/virtual/` **and**
test-only fakes in `tools/virtual_device/` — is the thing worth avoiding: two families of
almost-identical fake hardware that drift apart.

**This is Phase D and it does not block Phases A–C**, so implementation starts on A while
this is decided.

---

## Architecture Decisions

Stated as decisions rather than questions because they follow from rules already in the
repository.

- **Shared shell UI moves to `src/ui/`, it is not duplicated.** `SystemLogForm` and the
  Theme/Language menu construction are needed by both shells, and the shells may not
  include each other. `src/ui` is the module both may include. Copying instead would create
  two log viewers that drift.
- **No common `Shell` base class.** `AGENT.md`: no abstraction before a second concrete
  implementation proves the shape. Two shells sharing *components* is not the same as two
  shells sharing a *hierarchy*, and a base class would pull the editor's project-editing
  lifecycle into the runtime's.
- **The mode switch is a hand-off, never co-running.** Phase 6's constraint, unchanged: stop
  runtime on every task → confirm devices released → launch the sibling → exit. Two
  processes cannot both own the Basler camera, the vision-output port or the PLC socket.
- **The sibling is located via `QCoreApplication::applicationDirPath()` + a fixed
  filename.** This is why Phase 6 / E7b put both executables in one folder; that work is
  the prerequisite and it is done.
- **The incoming process waits for the lock rather than the outgoing one waiting to be
  gone.** The launching shell cannot exit *after* the new one starts, and the new one
  cannot start before the lock is free — so the new process retries `tryAcquire()` for a
  bounded window when it is started with a hand-off flag. Anything else is a race with a
  silent failure mode: the second shell exits without a window and the operator sees
  nothing happen.
- **Virtual devices are extracted, not written from scratch.** Three of them already exist
  and are already exercised by ~20 contract tests. Rewriting them would risk changing
  behaviour those tests depend on; moving them cannot.

---

## Phase A — Shared shell UI (foundation)

Nothing in the request mentions this phase. It exists because threads 4 and 5 cannot be
built without it.

### A1. Move `SystemLogForm` into `src/ui/`

**Files:** `app/system_log_form.{h,cpp,ui}` → `src/ui/forms/system_log_form.{h,cpp,ui}`,
`app/app.pri`, `src/ui/ui.pri`, `app/mainwindow.{h,cpp}`

- Move the three files; register them in `ui.pri`, unregister from `app.pri`.
- `MainWindow` includes `"ui/forms/system_log_form.h"` instead of `"system_log_form.h"`.
- No behaviour change. The widget already depends only on `AppLogger` and `ThemeManager`,
  both level-0 `core`, so the move introduces no new dependency.

**Acceptance:**
- [x] `SystemLogForm` compiles inside `ncr_shared`; the editor still shows its log dock. *(owner-confirmed)*
- [x] Contract test layering passes (`ui` may include `core`).

**Scope:** Small. **Dependencies:** none.

### A2. Extract the Theme and Language menus into a reusable component

> ⚠️ **SUPERSEDED 2026-08-23 by the project owner, during Phase C.** `AppViewMenu` is
> deleted. Theme and Language are now declared **in each shell's `.ui`** — menus and
> actions both — with only the exclusivity (`QActionGroup`, which Designer cannot express)
> and the behaviour wired in code. See the Phase C outcome for what that trades away.

**Files:** `src/ui/widgets/app_view_menu.{h,cpp}` (new), `app/mainwindow.{h,cpp}`,
`src/ui/ui.pri`

- One component that, given a `QMenu*`, populates Theme (from
  `ThemeManager::styles()`, exclusive, live-updating via `styleRegistered`) and Language
  (the fixed en/ja list, persisted to `AppSettings`).
- `MainWindow` keeps its extra behaviour — refreshing the project tree on
  `themeChanged` — by connecting to `ThemeManager` itself, not by owning the menu code.
- **Not** extracted: the toolbar, project actions, access level. Those are editor concerns.

**Acceptance:**
- [x] The editor's Theme and Language menus behave exactly as before, from the shared component. *(owner-confirmed)*
- [x] The component has no dependency on `app/` or on a project being open.

**Scope:** Medium. **Dependencies:** none.

### A3. One settings file for both shells

**Files:** `app/main.cpp`, `runtime_app/main.cpp`, `src/core/app_settings/app_settings.{h,cpp}`,
`docs/backlog/later_todo_list.md` (#36 closed)

- Give both shells the same settings identity so `AppDataLocation` resolves to one path.
  The window title and taskbar name must stay different, so this is **not** simply making
  `applicationName` equal — set `organizationName` + a fixed settings folder, or give
  `AppSettings::filePath()` an explicit product-scoped path.
- Verify the existing `%APPDATA%\NCRN Pick\settings.dat` still loads: it carries a magic
  number and a schema version, and changing the *path* must not read as a corrupt file.

**Acceptance:**
- [x] Both shells resolve one settings path. *(asserted by contract test; the end-to-end theme check is manual)*
- [x] An existing settings file is still readable — the new path IS the editor's old path, and `legacyFilePath()` covers the runtime's.
- [x] Backlog #36 closed with a pointer here.

**Scope:** Small. **Dependencies:** none. **Risk:** losing the user's existing settings —
see R3.

### A4. Access control: roles and a swappable credential source (D2)

**Files:** `src/core/auth/access_control.{h,cpp}` (new),
`src/core/auth/admin_credential_provider.h` (new interface),
`src/core/auth/settings_admin_credential_provider.{h,cpp}` (new), `src/core/core.pri`,
`src/core/AGENTS.md`

The shape, kept as small as the requirement allows:

```
AccessRole            { Operator, Admin }

IAdminCredentialProvider          // the seam a dongle plugs into later
    bool verify(const QString &password) const
    bool isConfigured() const     // false when no credential source is available

SettingsAdminCredentialProvider   // the one implementation for this version
AccessControl                     // process-wide current role; verify + elevate + drop
    signals: roleChanged(AccessRole)
```

- `Operator` is the **default role at startup** and requires no password. Nothing that runs
  today changes behaviour until a caller asks for `Admin`.
- `Admin` requires `verify()`. The settings-backed provider stores a **salted SHA-256
  hash**, not the password — `settings.dat` is obfuscated, not encrypted, and a plaintext
  admin password sitting in a file an operator can copy is not worth the convenience. It
  seeds itself with the default password `admin` on first run.
- `verify()` takes the password and returns a bool; it does **not** expose or return the
  stored credential. That is what lets a dongle implementation exist without changing a
  single call site — a dongle can answer the question without ever handing over a secret.
- **Nothing in this phase reads the credential from a dongle.** The interface is the
  deliverable; a second implementation is not.

**Acceptance:**
- [x] Default role is `Operator`; no password prompt appears anywhere unless `Admin` is asked for.
- [x] `admin` elevates; a wrong password does not, and says so.
- [x] The stored value is not the plaintext password — salted SHA-256, fresh salt per write.
- [x] Swapping in a different `IAdminCredentialProvider` requires no change outside its construction. *(driven in the test by injecting a fixed-password provider)*

**Scope:** Medium. **Dependencies:** A3 (needs the unified settings file).

### A5. Login dialog

**Files:** `src/ui/forms/admin_login_dialog.{h,cpp,ui}` (new), `src/ui/ui.pri`,
`app/mainwindow.{h,cpp}`

- One shared dialog: password field, error text, OK/Cancel. Structure in the `.ui`
  (Rule 1.1), no inline stylesheet (Rule 2.2).
- `MainWindow`'s existing Access Level menu stops being decorative: choosing Admin prompts,
  choosing Standard drops back with no prompt. `updateAccessLevelLabel()` becomes a slot on
  `AccessControl::roleChanged` rather than something the click handlers set directly.

**Acceptance:**
- [x] The editor's Access Level menu reflects the real role and cannot be set to Admin without the password. *(owner-confirmed)*
- [x] Cancelling leaves the previous role untouched.

**Scope:** Small–Medium. **Dependencies:** A4.

### Checkpoint A
- [x] Both shells build; contract test passes. *(**54 passed, 0 failed** — up from 51: A3 added one case and A4 two)*
- [x] Both shells start and reach their main window after the refactor.
- [x] Editor: system log, theme switch, language switch all still work. *(confirmed by the project owner, 2026-08-23)*
- [x] Theme set in one shell is observed by the other. *(confirmed by the project owner on both shells, 2026-08-23 — the end-to-end half of what `test_settings_path_is_product_scoped_not_application_scoped` asserts as a mechanism)*
- [x] An existing `settings.dat` survives the path change. *(the new path resolves to the same `%APPDATA%\NCRN Pick\settings.dat` the editor always used, and its mtime was unchanged across every build and smoke run in this session)*
- [x] Access Level: Admin prompts, `admin` works, a wrong password is refused, Standard needs no prompt. *(confirmed by the project owner, 2026-08-23)*

### Phase A outcome (2026-08-23)

**A1/A2 were a move and an extraction, and stayed that size.** `SystemLogForm` went to
`src/ui/forms/` with no behaviour change — its promoted `CompactComboBox` header was already
written as a `src/`-rooted path, and its QSS lives in `resrc.qrc` at shell level, so neither
followed the file. `AppViewMenu` took the Theme and Language construction out of
`MainWindow`; the editor's one extra behaviour — repainting the project tree, whose icons
differ between light and dark — stayed behind, connected to a new `themeApplied` signal
rather than being carried into shared code.

**A3 needed no migration on the editor side, which was worth checking rather than
assuming.** `GenericDataLocation` + the product folder resolves to exactly the
`%APPDATA%\NCRN Pick\settings.dat` the editor has always written. The runtime *had* already
created its own `%APPDATA%\NCRN Pick Runtime\settings.dat` during Phase 6 smoke tests, so
the legacy fallback is not hypothetical — it is what stops that machine's runtime settings
being silently dropped.

**A4/A5 replaced a control that could not fail.** The Access Level menu previously changed
a menu *title*; `onPrivilegeAdmin()` called `updateAccessLevelLabel(true)` and nothing else
in the codebase read the result. It is now backed by a real role, and the label follows
`AccessControl::roleChanged` rather than the click — so a cancelled or failed login leaves
the menu showing what is actually in force.

Two things worth recording because they are easy to get wrong later:

- **The contract test now enables `QStandardPaths::setTestModeEnabled(true)` in
  `initTestCase()`.** The credential tests seed and rewrite an admin password, and
  `AppSettings` is a singleton that writes on first change — without the sandbox, running
  the suite would have edited the developer's real `settings.dat`. Verified: the real file's
  mtime is unchanged and the test wrote to `%LOCALAPPDATA%\qttest\NCRN Pick\settings.dat`.
- **`AccessControl` does not build its credential provider in its constructor.** Doing so
  would seed the settings file as a side effect of something merely asking what the current
  role is at startup. The provider is created on first use instead.

**Deliberately not done in Phase A:** any UI to change the admin password. The capability
exists and is tested (`SettingsAdminCredentialProvider::setPassword`), so the password is
not permanently `admin` at the API level — but nothing in either shell calls it yet, so in
practice it is. That is a gap, not a design: it needs a decision about who may change it and
what happens when it is forgotten, and neither is in the request.

---

## Phase B — One app at a time, and the hand-off

### B1. One instance key for the product

**Files:** `app/main.cpp`, `runtime_app/main.cpp`, `src/core/utils/single_instance_guard.{h,cpp}`,
`tests/architecture_contract_test/main.cpp`

- Both shells acquire the same key. A losing launch reads the holder's identity from
  `QLockFile::getLockInfo()` (which returns pid, hostname **and application name**) and
  reports which shell is running before raising it and exiting — per Q1a.
- The guard's own doc comment currently states the opposite invariant ("deliberately does
  NOT stop the commissioning shell and the runtime shell from running at the same time").
  It must be rewritten, not left contradicting the code.
- Contract test: the two shells use the **same** key — the inverse of the assertion added
  in Phase 6 / E1, which must be updated rather than deleted so the change is visible.

**Acceptance:**
- [x] Launching either shell while the other runs does not start a second process. *(contract test asserts the shared key refuses a second acquire; proven end-to-end with the real executables — see the outcome below)*
- [x] The message names which shell is running. *(owner-confirmed; the name comes from `QLockFile::getLockInfo()` and the two names are asserted distinct)*
- [x] A stale lock from a crash is still reclaimed. *(the end-to-end test **hard-killed** the lock holder and the waiting shell took over)*

**Scope:** Medium. **Dependencies:** none.

### B2. The hand-off

**Files:** `src/core/utils/shell_handoff.{h,cpp}` (new, `core`), `app/main.cpp`,
`runtime_app/main.cpp`, `app/mainwindow.{h,cpp}`, runtime shell window

- A `core` helper that: locates the sibling executable via `applicationDirPath()` + a fixed
  filename, starts it detached with a `--handoff` flag, and returns whether the start
  succeeded.
- A shell started with `--handoff` retries `tryAcquire()` for a bounded window (proposed
  10 s at 200 ms) before giving up, because the outgoing process is still exiting.
- The outgoing shell stops its work **first** and only exits once devices are released;
  if that fails it reports and stays, rather than handing off into a half-released camera.
- Both directions: runtime → editor (Project → Open Editor) and editor → runtime.

**Two gates before any switch, in this order (D2, D2a):**

1. **Privilege** — reaching the editor requires `Admin`. If the current role is `Operator`,
   the login dialog appears first; cancelling abandons the switch. Going the other way
   (editor → runtime) needs no elevation: dropping *into* the operator view is not a
   privilege escalation.
2. **Confirmation** — *"This will stop all running tasks and release the camera and PLC.
   Continue?"* Shown in **both** directions, including when the user already holds `Admin`.
   The password answers "are you allowed?"; this answers "did you mean to?".

**Acceptance:**
- [x] Switching either way ends with exactly one process running, in the expected shell. *(owner-confirmed via the menu; the take-over half was also proven end-to-end)*
- [ ] A failed sibling launch leaves the current shell running and says why. **Still unverified** — it needs the sibling executable removed from the install folder, which no normal check produces. Low risk: the path is checked with `QFileInfo::exists()` before launching and the failure branch does not quit.
- [ ] Switching from the runtime stops every task's runtime before the process exits. **Still unverified against real devices** — `stopTaskRuntimes()` runs before `launchSibling()` by construction, and the switch itself is owner-confirmed, but only hardware shows the camera and PLC actually being released.
- [x] An `Operator` cannot reach the editor without the admin password. *(owner-confirmed)*
- [x] The confirmation appears in both directions and cancelling changes nothing. *(owner-confirmed)*

**Scope:** Medium–Large. **Dependencies:** B1, A4, A5. **Risk:** the lock race — see R1.

### B3. Log the open and switch events

**Files:** `app/main.cpp`, `runtime_app/main.cpp`, `src/core/utils/shell_handoff.cpp`

- Both shells already log a startup line; make them consistent, name the shell explicitly,
  and add lines for hand-off requested / sibling started / hand-off refused.
- Fix the `"Sytem startup..."` typo in `app/main.cpp` while here.
- Note in the docs that both shells write the **same** `logs/app_log_<date>.txt` because
  they share an install folder — and that this is only safe because B1 stops them
  co-running. `AppLogger`'s mutex is in-process; it has never guarded a second process.

**Acceptance:**
- [x] One log file shows the full story. *(`app_log_2026-08-23.txt`: editor start → runtime start with `handoff=1` → runtime ready **12.8 s later**, which is the wait doing its job, visible in the log)*

**Scope:** Small. **Dependencies:** B1, B2.

### Checkpoint B
- [x] Double-launch of each shell, and cross-launch, behave per D1. *(owner-confirmed, 2026-08-23)*
- [x] Switch runtime → editor → runtime, **twice**, with no orphan process. *(owner-confirmed, 2026-08-23 — twice on purpose: a race that fires one time in five passes a single check)*
- [x] Kill a shell mid-switch; confirm the next launch still starts. *(the end-to-end test hard-killed the lock holder while a `--handoff` launch was waiting; it took over and came up)*
- [x] The shared log file reads correctly across the switches.

### Phase B outcome (2026-08-23)

**The lock race — the phase's highest risk (R1) — was tested end to end on the real
executables, not reasoned about.** The sequence, from the run log:

1. `ncr_picking.exe` started and took the product lock;
2. `ncr_runtime.exe --handoff` started while the lock was still held. **It stayed alive with
   no window** for the whole wait — which is the point: without the timeout it would have
   exited immediately and silently, and "the menu item did nothing" is indistinguishable
   from that;
3. the editor was **hard-killed** (its `closeEvent` puts up a modal save prompt, so an
   automated `WM_CLOSE` cannot close it) — so this also exercised stale-lock reclaim;
4. the runtime acquired the lock and showed its window. One process left.

The shared `logs/app_log_<date>.txt` records it in one place, and the **12.8 s gap** between
the runtime's `handoff=1` start line and its ready line is the wait, visible after the fact.

**Two things found while implementing, both worth recording:**

- **`QCoreApplication::quit()` does not run `closeEvent()`.** That is what we want — the
  user has already confirmed the switch and already been through `maybeSave()`, and a third
  dialog asking the same question is noise. But `closeEvent()` is also where
  `MainWindow::handleCloseEvent()` persists the last-used folder, so a switch would have
  quietly forgotten it. The release lambda now calls it explicitly.
- **A test that mutates persistent state must reset it.** `test_admin_credential_is_stored_hashed_and_salted`
  ends by changing the admin password, and the sandbox `settings.dat` survives between
  runs — so the *second* run found a configured credential, skipped seeding, and failed on
  a default password the first run had replaced. It now clears the keys first and restores
  the default at the end; verified by running the suite twice back to back, 56/56 both times.

**Deliberately not done:** the runtime's "Open Editor" trigger currently sits on the runtime
toolbar rather than in a menubar. That shell has no menubar yet — building one is Phase C —
and B2 needs a trigger in *both* shells or only half the hand-off can be exercised. C3 moves
it under Project, where the request asks for it.

**Phase 6 decision reversed here, on purpose:** E1 gave each shell its own instance key so
they would not block each other. `test_single_instance_guard_keys_are_independent` still
asserts that distinct keys stay independent — the mechanism is unchanged — but the *policy*
assertion is inverted into `test_both_shells_take_the_same_instance_key`, which also checks
that neither `main()` has reintroduced a private key. Inverted rather than deleted, so the
reversal shows up in the diff instead of looking like a test someone quietly dropped.

---

## Phase C — `runtime_app/` restructure and its menubar

### C1. Restructure into `src/` and `ui/`

**Files:** all of `runtime_app/`, `runtime_app/runtime_app.pri`, `runtime_app/AGENTS.md`

- `runtime_app/src/` — `main.cpp`, `runtime_layout_controller.{h,cpp}`, the hand-off wiring.
- `runtime_app/ui/` — `runtime_shell_window.{h,cpp,ui}`.
- Include rooting per Q5.
- Contract test: the layering scan walks `runtime_app/` recursively already, so no change
  is required — but a case asserting the new folders exist would stop the structure being
  silently flattened again.

**Acceptance:**
- [x] Both shells build; contract test passes. *(57 passed, 0 failed)*
- [x] No `../` escapes and no ambiguous `"ui/..."` includes inside `runtime_app/`. *(own headers are included with the `runtime_app/` prefix; `test_runtime_shell_is_structured_and_form_driven` fails on a bare one)*

**Scope:** Medium. **Dependencies:** none, but doing it before C2 avoids moving files twice.

### C2. Author `runtime_shell_window.ui`

**Files:** `runtime_app/ui/runtime_shell_window.{h,cpp,ui}`, `runtime_app/runtime_app.pri`,
`docs/backlog/later_todo_list.md` (#35 closed)

- Both pages authored in Designer: project-select (title, reason, path, Browse, version)
  and runtime view (toolbar, dock host). Every styled widget gets an `objectName`.
- `.cpp` reduced to wiring. No `styleSheet` property in the `.ui` (Rule 2.2).
- `m_dockManager->setStyleSheet(QString())` stays — it disables the ADS internal stylesheet
  so the global QSS owns `ads--` rules, exactly as `app/mainwindow.cpp:320` does.

**Acceptance:**
- [x] No layout primitives `new`-ed in the `.cpp`. *(asserted mechanically, and the assertion was verified able to fail by injecting `new QVBoxLayout`)*
- [ ] All four startup paths still show their distinct messages. **Manual** — this is the part a layout port most easily breaks.
- [x] Backlog #35 closed.

**Scope:** Medium. **Dependencies:** C1.

### C3. The menubar

**Files:** `runtime_app/ui/runtime_shell_window.{h,cpp,ui}`

- **Project:** Load (the existing browse flow), Close (stop tasks, return to the
  project-select page), Open Editor (B2 hand-off, with the Q2 confirmation).
- **View:** System log (a dock hosting the shared `SystemLogForm`), Theme, Language (both
  from A2's component).
- Close must leave the shell on the project-select page, never on a blank window — the
  runtime owns the screen on a field machine and must not become a dead end. That property
  is already an invariant in `runtime_app/AGENTS.md`.

**Acceptance:**
- [ ] Every listed menu item works. **Manual.**
- [ ] Theme and language changes behave the same as in the editor. **Manual** — the component is the same one the editor uses, which the owner already confirmed there.
- [ ] Close then Load a different project leaves exactly the new project's tasks running. **Manual, needs a project file.**

**Scope:** Medium. **Dependencies:** A1, A2, B2, C2.

### Checkpoint C
- [x] Both shells build, start, and close cleanly after the restructure.
- [ ] Runtime shell: load, close, reload, switch to editor, switch back. **Manual.**
- [ ] System log dock shows entries from both shells' shared file. **Manual.**
- [ ] Theme and language switch from the runtime and persist. **Manual.**

### Phase C outcome (2026-08-23)

**C1 — the folder split forced a decision the request did not mention.** `src/` is already
on the include path, so `#include "ui/runtime_shell_window.h"` from inside `runtime_app/`
would have shadowed `src/ui/` — and the contract test would have classified it as a
reference to the `src/ui` module, so the ambiguity would not even have been reported. Own
headers are therefore included as `"runtime_app/ui/..."`, with the repository on
INCLUDEPATH, and the contract test fails on a bare self-include.

**C2 — the `.ui` port changed two widgets into things Designer can express.** The old code
built a `QToolBar` holding a `QComboBox` and a `QLabel`; a toolbar in Designer takes
actions, not arbitrary widgets, so both would have had to stay in code and the rule would
have been half-kept. Instead:

- the tile-count combo became **View → Layout**, five checkable actions declared in the
  `.ui` (only their exclusivity is made in code — Designer has no `QActionGroup`);
- the status label became the window's real **status bar**, which needs no widget at all.

That leaves the ADS dock manager as the only thing constructed in code, and it has to be:
it is not a Designer widget. It goes into the form's `wg_dock` host, the same pattern
`app/mainwindow.cpp` uses.

**The rule is now mechanical.** `test_runtime_shell_is_structured_and_form_driven` fails on
`new QVBoxLayout`/`QStackedWidget`/`QToolBar`/… anywhere under `runtime_app/` — verified
able to fail by injecting exactly such a line. It is **scoped to this shell on purpose**:
widening it to `app/` would fail on `mainwindow.cpp:300`, which is the same pattern in the
editor's dock host. Recorded as backlog #37 rather than fixed here, because mixing an
unrequested UI change into a structure phase is how a just-verified window stops being
verified.

**One stylesheet, not the usual `_light`/`_dark` pair.** Every colour in
`runtime_shell_window.qss` is a `@{token}`, and tokens already resolve per theme — so the
two files would have been byte-identical, which is a trap rather than a convention. The
window still re-applies the sheet on `themeChanged`, because the *resolved* output differs
even though the source does not.

**Two existing tests caught the move**, which is what they are for:
`test_both_shells_report_one_shared_version` and `test_both_shells_take_the_same_instance_key`
both read `runtime_app/main.cpp` by path and failed the moment it became
`runtime_app/src/main.cpp`.

**Superseded from Phase B:** the temporary "Open Editor" button on the runtime toolbar is
gone — the toolbar itself is gone. It is now **Project → Open Editor**, where the request
asked for it.

#### Two defects found by the owner running the shell, and one design reversal (2026-08-23)

**The runtime view came up empty with a few stray dock tabs in the corner.** This turned
out to be **two independent defects with one symptom**, found in two passes. The first fix
was necessary and not sufficient, and the shell still looked broken after it — which is
what the owner reported next.

*First defect — the dock host was never sized.* `buildDockHost()` made the dock manager a
*child* of `wg_dock` but never put it *in* a layout. Being a child is not enough: Qt leaves
the widget at its size hint at (0,0) and never resizes it. Fixed; measured on the running
window as `dockManager geom=2,2 1276x753`, with `menubar 1280x21` and `statusbar 1280x22`
both visible. **The menu bar was never broken**; the shrunken dock manager was drawn over
where its text is.

*Second defect — nothing was ever docked into it.* A full widget-tree dump of the running
shell showed the container correct and **empty**, with the task dock stranded outside it:

```text
QWidget 'wg_dock'          geom=0,0 1280x757   layout=QVBoxLayout
  ads::CDockManager ''     geom=9,9 1262x739
    ads::CDockSplitter ''  geom=0,0 1262x739   splitterSizes=[]     ← empty
...
ads::CDockWidget 'task_3d' geom=0,0 100x30     ← child of the window, never added
```

The cause is a Qt trap worth remembering: **`QSignalBlocker` on a `QAction` breaks the
`QActionGroup` it belongs to.** `selectLayoutAction()` blocked each action's signals while
checking it, to "avoid re-triggering the group". But `QActionGroup` tracks its checked
action through `QAction::changed` — blocking that leaves the group believing nothing is
checked, so `checkedAction()` returned `nullptr`, `applyCurrentLayout()` returned early,
and no dock was ever added. The blocker also prevented nothing: `setChecked()` emits
`toggled()`/`changed()`, never `triggered()`, and `QActionGroup::triggered` is the only
signal wired to a slot.

Removed from `selectLayoutAction()` and `syncThemeAction()`, and from the identical line in
`MainWindow::onThemeChanged()` (`app/mainwindow.cpp`) — harmless there only because nothing
reads `checkedAction()`, but it was corrupting the same bookkeeping. `applyCurrentLayout()`
now falls back to the smallest fitting grid and logs an error instead of silently docking
nothing: on a shell that owns the screen, an undocked task dock is not invisible, it is
drawn at its size hint over the menu bar.

After: `splitterSizes=[751]`, one `CDockAreaWidget` at `1276x751`, dashboard at `1276x729`.

> **`wg_dock` itself was never the problem.** The owner asked for it to be rebuilt the way
> `app/mainwindow.cpp` does it; both arrangements were measured and produce the identical
> tree. It is kept declared in `runtime_shell_window.ui` because that is what Rule 1.1
> requires and what the contract test enforces — the commissioning shell is the one still
> out of line (backlog #37), so copying it here would have been a step backwards.

> Two things that looked like defects and were not: the **orange status bar** is the
> project's own theme (`QStatusBar { background-color: @{accent.primary} }` in the global
> QSS), and a `PrintWindow`/screen capture of this window is unreliable — it produced a
> blank image even after the fix, which is why the geometry dump was the evidence used.

Also tightened while here: the window stylesheet is applied to `page_project_select`, not
to the whole window. Every rule in it targets a widget on that page, and a stylesheet set
on a `QMainWindow` re-resolves styling for its **entire** subtree — menu bar, status bar
and ADS docks included, none of which that sheet says anything about.

**A shell started by a hand-off did not come to the front.** Windows only lets the process
that *currently holds* the foreground grant that right to another, so this needs both
halves: the outgoing shell calls `AllowSetForegroundWindow(childPid)` right after
`startDetached`, and the incoming shell raises itself — deferred to the first turn of the
event loop, because `presentShellWindow()` runs before `exec()` and a foreground request
against a window the compositor has not finished mapping is ignored.

#### Two more owner findings, and a corrected diagnosis (2026-08-23)

**`btn_browse` on the project-select page did nothing.** It existed only in the `.ui` and was
referenced nowhere in the `.cpp`. Wired to `onLoadProject()`, the same slot as Project →
Load…. A sweep of every interactive control in both shells found no other unwired control in
the runtime shell, and three in the commissioning shell — recorded as backlog #40 rather than
fixed, because that window had just been verified by hand.

**Switching editor → runtime sometimes left the new window behind others; runtime → editor
never did.** The first explanation attempted here was that the Win32 foreground grant goes
stale with elapsed time. **That is wrong and is corrected for the record:**
`AllowSetForegroundWindow()` has **no time-based expiry**. It is revoked by exactly two
things — the next user input not directed at the granted process, and another
`AllowSetForegroundWindow()` call naming a different PID. It survives the granting process
exiting, which the reliable runtime → editor direction demonstrates in this very codebase:
by the time the incoming editor asks, its granter is long dead.

The real mechanism is a **blind gap**. `RuntimeShellWindow`'s constructor ended with
`tryStartRememberedProject()`, so the process spent seconds loading the project, decoding
training images and opening the camera, PLC and output port **before any window existed**.
The operator watched the editor vanish with nothing replacing it, clicked something, and that
input — not the passage of time — revoked the grant. Intermittent because it was conditioned
on operator behaviour; one-directional because the commissioning window costs milliseconds to
build.

Addressed by separating construction from startup: `beginStartup()` now does the loading,
posted from `main()` on a queued call **after** `presentShellWindow()`. The ordering is
load-bearing — posting it from the constructor would queue it *ahead* of that function's
foreground claim and reinstate the gap. Measured on the hand-off path:

| | Claim reached at |
|---|---|
| Before | **+3795 ms** after process start (project loaded first) |
| After | **+197 ms** (window up at +196 ms, project loaded at +656 ms) |

> ⚠️ **This did NOT fix the reported symptom.** The owner re-tested and the runtime still
> fails to come to the front when switched to from the editor. So the blind gap was at most a
> contributing factor, not the root cause, and the diagnosis above is **incomplete**. The
> change is kept because both parts are independently correct — the window now appears in
> ~200 ms instead of ~4 s, and the activation is no longer queued behind the device threads'
> signal backlog — but the defect is open as **backlog #41**, which records what has been
> ruled out and why the next step must be instrumentation rather than another hypothesis.

Also fixed while here: `presentShellWindow()` split `show()` from `activateWindow()` across an
event-loop turn, and in this shell that first turn is not empty — the device threads started
during construction had been posting queued signals into it. It now claims the foreground
immediately *and* re-asserts on the deferred turn.

Guards that came with the change, because showing the window first makes the menus reachable
during startup: the form starts on the project-select page (an empty runtime page would
otherwise be on screen for the whole device startup), and Load / Open Editor / Browse are
`setEnabled(false)` for the duration — each opens a modal dialog whose nested event loop
could re-enter `startProject()` while `stopTaskRuntimes()` is still iterating the containers
it clears at the end.

**`AppViewMenu` deleted — owner's decision.** Phase A2 extracted the Theme and Language
submenus into one shared `src/ui` component precisely because the two shells may not
include each other. The owner chose instead to declare them statically in each shell's
`.ui`, so the menu bar is entirely Designer-authored in both shells.

What that trades away, stated rather than discovered later:

- **Themes registered at runtime no longer appear.** `ThemeManager::registerStyle()` and
  its `styleRegistered` signal exist for exactly that, and the previous menu followed them.
  Static actions cannot. Checked before agreeing: nothing outside `ThemeManager`'s own
  constructor registers a style, so `light` and `dark` are the whole set today — but adding
  a third now means editing two `.ui` files and two `.cpp` files, and forgetting one is
  silent.
- **The wiring is duplicated** across `MainWindow` and `RuntimeShellWindow`: roughly 25
  lines each of action-group setup, theme sync and the language-changed message. The
  *structure* is declarative per shell, which is what the owner asked for; the *behaviour*
  is now in two places.

---

## Phase D — Virtual devices

**Blocked on the D3 conflict above.** Written against the recommended resolution — virtual
devices as shipped product code in `src/device/virtual/`, with the contract test's fakes
deleted in favour of them. If the owner keeps `tools/virtual_device/` instead, D2 and D3
below change and registration is dropped.

> The request spells the folder `tools/vitual_device`. Read as a typo for `virtual_device`.

### D1. Promote the existing fakes into `src/device/virtual/`

**Files:** `src/device/virtual/virtual_camera_device.{h,cpp}`,
`virtual_plc_device.{h,cpp}`, `virtual_vision_output_device.{h,cpp}` (new),
`src/device/device.pri`, `tests/architecture_contract_test/main.cpp`

- Move `FakeCameraDevice`, `FakePlcDevice`, `FakeVisionOutputDevice` out of the contract
  test **unchanged first**, then rename. They are exercised by ~20 existing tests; the
  tests must pass before and after with **no edits to their bodies** — that is what makes
  this a move rather than a rewrite, and it is the only reason this is a medium task
  instead of a risky one.
- Keep the knobs that exist for a reason (`connectSucceeds`, which lets a test hold a
  camera unreachable across an unbounded retry). On a *virtual* device, "simulate a
  failure" is a feature, not a test hook.

**Acceptance:**
- [x] Contract test passes with an unchanged test count and unchanged test bodies.
- [x] No `Fake*Device` class remains in `tests/architecture_contract_test/main.cpp`.

**Scope:** Medium. **Dependencies:** none.

> **D1 ✅ DONE (2026-08-23).** Contract test **58 passed / 0 failed** — the same count as
> before the move, with only the type names changed in the test bodies. The owner asked for
> the `Virtual` prefix rather than carrying `Fake` into shipped code, so the classes are
> `VirtualCameraDevice`, `VirtualPlcDevice` and `VirtualVisionOutputDevice` in
> `src/device/virtual/`, registered in `src/device/device.pri` **and** in the contract test's
> own `.pro` (that project lists `src/` sources individually instead of including the module
> `.pri` files — easy to miss, and the failure is a link error, not a compile error).
>
> Carried over unchanged on purpose, and undone by D2: all three still report the sub-type of
> the real device they stood in for (`BaslerGigE` / `MitsubishiMc` / `VisionTCPIP`), and
> `VirtualCameraDevice` still includes `camera_basler_gige.h` only to reuse `BaslerGigeCfg` —
> which drags Pylon into a device that exists to avoid needing it. Keeping those made the move
> verifiable against the existing tests without editing a single assertion; **they are not a
> design and must not be treated as one.** Until D2 gives each its own token and config class,
> none of the three can be created from a project file.

### D2. Register them as real device sub-types

**Files:** `src/device/device_registry.cpp`, `src/device/device_factory.cpp`,
`src/ui/forms/device_widget_factory.{h,cpp}`, config classes, persistence,
`tests/architecture_contract_test/main.cpp`

`AGENT.md`: *"Every new device subtype must update enum/string conversion, factory
dispatch, UI dispatch, persistence, and tests."* Three sub-types, five places each. This is
the bulk of Phase D, not D1.

- Each virtual device needs a config class that round-trips JSON, a `DeviceRegistryEntry`
  with a sub-type token, factory dispatch, and a device widget so the Add Device wizard can
  show it.
- **Decide the sub-type tokens now and do not change them later** — they are persisted in
  project files, exactly like `bErrorReset` in Phase 6 / A1.

**Acceptance:**
- [ ] A project can be created with a virtual camera, PLC and vision output, saved, closed and reloaded.
- [ ] The runtime shell runs a full cycle against them with no hardware attached.
- [ ] Existing contract-test assertions on registry contents are updated, not bypassed.

**Scope:** Large — **break into one sub-task per device family** before starting.
**Dependencies:** D1.

> **Sub-type token: `"Virtual"`, all three families.** Tokens are family-local (each family reads
> a different JSON key), matching the existing `"Basler_GigE"` / `"MitsubishiMc"` style. Verified
> against every existing token in all four families — no collision. **These are persisted into
> customer project files and can never be changed.**
>
> **⚠️ Registry ORDER is load-bearing, not cosmetic.** `displayNamesFor()` returns `kEntries`
> order verbatim, the Add Device wizard's combo leaves index 0 current, and `buildDeviceJson()`
> writes back `currentText()`. So the **first** entry of a family is what an operator creates by
> not choosing. Every `Virtual` row goes **last** within its family, or the default camera
> silently becomes a simulated one — risk R8, reachable by doing nothing. The contract test now
> asserts the order, not just membership.
>
> **D2a — Camera ✅ DONE (2026-08-23).** Contract test **59 passed / 0 failed**, both shells build.
> - `CameraType::Virtual` + `CAM_TYPE_VIRTUAL` + both string conversions.
> - **`VirtualCameraCfg`, its own config class** — not `BaslerGigeCfg`. Sharing it would have
>   written `"Basler_GigE"` into the nested `DeviceConfig` while the top level said `"Virtual"`.
>   Both load correctly *today* because `subTypeValueFrom()` reads the top level first — but the
>   moment that key is dropped or hand-edited, the device comes back as a **real Basler**. That is
>   exactly the R8 failure, one hand-edit away. It also removes the Pylon include from a device
>   that exists to avoid needing Pylon.
> - Config gained `imagePath`: a flat generated frame cannot produce a match, so replaying a
>   still image is what makes Checkpoint D's "full localization cycle with no hardware"
>   reachable. Falls back to the generated frame when unset or unreadable.
> - Registry entry + factory, ordered last.
> - New test `test_virtual_camera_round_trips_as_virtual_not_as_hardware`, which **caught a real
>   defect**: the device never called `IDevice::setDeviceConfig(&m_config)`, so the base class —
>   which serialises through a non-owning pointer — wrote an empty `DeviceConfig`. It would have
>   saved and reloaded with every setting reset, silently.
>
> **D2a remaining: UI dispatch.** `DeviceWidgetFactory` is the only sub-type dispatch outside
> `src/device`, and its Camera and PLC arms are written as equality **rejections**
> (`cameraType() != BaslerGigE`, `device_widget_factory.cpp:32`), so a virtual device returns
> nullptr and the task page substitutes "No configuration panel available for this device" — a
> soft failure that reads as a missing panel rather than a missing registration. Plan: **one
> shared read-only `VirtualDeviceWidget`** for all three families (three near-identical widgets
> would be worse; the factory signature already takes the base runner), dispatched from a
> `isVirtualDevice()` predicate hoisted **above** the family switch. R8's visible markers need
> the identical predicate, so adding it once keeps R8 a rendering tweak rather than new plumbing.
>
> **D2b — PLC ✅, D2c — Vision output ✅, UI dispatch ✅, D3 markers ✅ (2026-08-23).**
> Contract test **61 passed / 0 failed**, both shells build.
>
> **Enumerator naming.** These enums are **unscoped**, so every enumerator lands in
> `vc::device` and the three families cannot each have a `Virtual`. Spelled `VirtualCamera`,
> `VirtualPlc`, `VirtualVisionOutput` — the same reason `BaslerGigE` and `MitsubishiMc` are
> spelled the way they are. **The JSON tokens are all still `"Virtual"`**, which is safe
> because each family reads a different JSON key.
>
> **Configs.** `VirtualPlcCfg` is deliberately empty — a real PLC config carries an address,
> a port and timeouts, and inventing those for a device that connects to nothing would be
> inventing settings that lie. `VirtualVisionOutputCfg` adds nothing but **keeps the inherited
> robot kinematic check**: that is a property of the picking geometry, not of the wire, so a
> hardware-free run must not be allowed to pass poses a real robot could never reach.
>
> **UI dispatch.** One shared read-only `VirtualDeviceWidget` for all three families, chosen
> by `vc::device::isVirtualDevice()` hoisted **above** the family switch in
> `DeviceWidgetFactory`. The hoist is required, not stylistic: the Camera and PLC arms are
> written as equality rejections (`cameraType() != BaslerGigE`), so without it a virtual device
> falls through to `nullptr` and the task page substitutes "No configuration panel available" —
> a soft failure that reads as a missing panel rather than a missing registration.
>
> **Wizard.** Two gaps closed. The vision-output branch had no `Virtual` case. The PLC page had
> **no sub-type combo at all** — it hard-coded a Mitsubishi MC config, which was correct while
> MC was the only PLC; `cbxPlcType` now exists and the MC-specific frame-type/data-code rows
> hide for any other sub-type, because offering settings that do nothing is how an operator
> learns to stop trusting the panel.
>
> **D3 — risk R8, three markers, one predicate.** `isVirtualDevice()` is the single source: the
> widget factory picks the panel from it, the project tree draws a **VIRT** chip (replacing the
> family chip, in `state.warning`) from it, and `TaskLocalization::beginRuntime()` writes a
> **user-log warning per virtual device** from it. The chip replaces rather than accompanies the
> family label because the device *name* is the operator's own — nothing stops them calling a
> simulated camera "Camera 1" — so the marker has to survive whatever it was named. The log line
> is the only marker that survives nobody looking at the screen, which is the actual R8 scenario:
> a station handed over "working", everything passing, nothing moving.
>
> New test `test_is_virtual_device_answers_for_every_family` asserts the predicate **both ways**
> — every virtual device is virtual, no real device is — and was **proven to fail** against an
> injected violation. Two markers disagreeing would be worse than no marker: an operator who has
> seen one wrong VIRT badge stops reading them.
>
> **`VirtualRobotDevice` deliberately NOT written.** D3 says "add it only if something exercises
> it"; nothing does. `isVirtualDevice()` records that in a comment so the gap reads as a decision
> rather than an oversight.
>
> **Owner found on first GUI run: no configuration UI, and a crash on navigating to a virtual
> device (2026-08-23).** Both traced to one wrong decision of mine and one latent host defect.
>
> *The crash.* `LocalizationTaskWidget::populateBrowser()` does
> `changePropertyBrowserWidget(dw->getPropertyBrowser())` for anything that is an
> `IDeviceWidget`. `VirtualDeviceWidget` **is** one, but it never called
> `initPropertyBrowser()`, so it handed over a `nullptr` that went straight into
> `QStackedWidget::addWidget()`. Fixed on both sides: the widget now owns a browser, and
> `changePropertyBrowserWidget()` falls back to the shared default browser on null. The host
> guard matters independently — nothing forces an `IDeviceWidget` subclass to create a browser,
> so **any** future one would have detonated the same way.
>
> *The missing UI, and the decision that caused it.* This widget was deliberately written
> without a property browser, on the reasoning that a virtual device has nothing worth
> configuring. **That was wrong.** `VirtualCameraCfg::imagePath` is precisely what makes a
> hardware-free localization cycle able to find anything at all — without it the camera returns
> a flat grey frame and every match correctly fails — and with no UI the only way to set it was
> to hand-edit the project file. It made Checkpoint D's own acceptance criterion unreachable
> through the GUI. The panel now builds both groups from Q_PROPERTY metadata (device
> properties, then the config gadget's), so a new setting appears the moment it is declared and
> the empty virtual PLC config correctly shows an empty group.
>
> *A third defect found while fixing those.* `VirtualPlcDevice` and `VirtualVisionOutputDevice`
> did not override `setDeviceConfig()`. `IDevice::setDeviceConfig()` stores a **non-owning**
> pointer, so the widget pushing an edited config down would have left the base pointing at a
> heap object nobody owned — dangling the moment the widget freed it — while the device's own
> config member went unread. Both now override it the way the camera does: copy into the owned
> member, delete the caller's object, re-publish the member's address.
>
> **Owner's second GUI run: property editing works; two more defects and two genuine gaps
> (2026-08-23).** Contract test **63 passed / 0 failed**.
>
> *A reloaded virtual camera kept its image PATH but grabbed the grey fallback.*
> `IDevice::fromJson()` writes into the config through its stored pointer and never calls
> `setDeviceConfig()`, so a device gets **no hook to react to being loaded**. The frame was
> built once in the constructor, from the default empty path, and never rebuilt. Fixed by
> overriding `fromJson()`. The earlier round-trip test passed throughout, because it asserted
> the CONFIG round-trips — not that the DEVICE behaves. `test_reloaded_virtual_camera_grabs_
> its_configured_image()` now asserts the grabbed frame's dimensions, which is the thing an
> operator actually sees.
>
> *A virtual PLC could not be given a signal map.* `LocalizationSettingWidget` builds the
> editor's tag lists from `IDigitalIoProvider`/`IWordIoProvider` and **clears both lists** when
> the device implements neither. `VirtualPlcDevice` implemented only `IPlcIoWriter`, so it
> existed, connected, accepted writes — and no task using it could ever be configured. It now
> implements `IPlcTagProvider`, advertising `M0…` and `D0…` ranges whose sizes are config
> properties (so the previously empty `VirtualPlcCfg` now earns its existence). The test also
> asserts every advertised tag is actually writable: offering a tag the device then rejects
> would put the mistake in the operator's signal map instead of in the test.
>
> *The virtual camera could not be calibrated — resolved by owner decision.*
> `LocalizationRuntimeController::validateActiveCameraCalibration()` refuses to run a camera
> whose `Calibrator::isCalibrated()` is false, and the real workflow (board setup, threshold
> tuning, corner detection) lives entirely inside `BaslerCameraWidget`. Four options were put
> to the owner; they chose **synthetic calibration**.
>
> `VirtualCameraCfg` gained `millimetresPerPixel`, `originXMm`, `originYMm` and
> `workPlaneZMm`. `rebuildCalibration()` generates a 3×3 correspondence grid across the frame
> at that scale and runs `Calibrator::calibrate()` on it.
>
> **The homography is real; the geometry is asserted.** Nothing about the fit is faked — it is
> the same `calibrate()` a real board goes through. What is asserted is the *scene*: flat,
> axis-aligned, at the declared scale and origin. Positions this camera reports are millimetres
> of a plane somebody declared, not of anything measured. That is why the scale is an explicit,
> visible property rather than a hidden constant, and why it sits behind the three VIRT markers.
>
> `millimetresPerPixel = 0` is the documented opt-out: it leaves the camera uncalibrated and
> the runtime refuses it exactly as it refuses a real one. The test asserts both that path and
> that the declared scale is actually honoured — a calibration that were merely *valid* would
> satisfy the runtime while reporting positions nobody chose.
>
> Rejected, and why: *import from file* needs a calibration to exist already, so a clean
> machine still cannot demo; *extract the shared workflow* is the right long-term move but
> touches the Basler widget the owner had just verified and would slip Checkpoint D; *drop the
> requirement* would have the runtime treat virtual and real cameras differently, which is the
> divergence this whole design avoids.
>
> **Deferred to backlog #42:** the wizard combo shows the raw JSON token because
> `displayNamesFor()` returns `subTypeValue` and `DeviceRegistryEntry::displayName` is read by
> nobody. That fuses a **frozen-forever** persistence key to an operator-facing label that should
> be free to improve and translate.

### D3. The missing family, and making virtual obvious

**Files:** `src/device/virtual/virtual_robot_device.{h,cpp}` (new), UI labelling, docs

- `RobotDevice` is the one abstract family with no fake. Add it only if something exercises
  it; writing it with no consumer is the abstraction `AGENT.md` warns against.
- **A commissioned project must never contain a virtual device by accident.** They are
  indistinguishable from real ones once saved. Needs a visible marker — at minimum a
  distinct display name and an icon or badge in the project tree, and a line in the task
  log when a runtime cycle runs against one. Worth deciding whether the runtime shell
  should refuse them outright.

**Scope:** Medium. **Dependencies:** D2.

### Checkpoint D — closed 2026-08-23

- [x] **Contract test passes** — **64 passed / 0 failed** (58 before Phase D). The six new cases
      are guards, not replacements: sub-type round trip × 3 families, the `isVirtualDevice()`
      predicate, the reloaded camera's *grabbed frame*, the PLC's advertised tag space, and the
      synthetic calibration's declared scale.
- [x] **A virtual device is visibly virtual** — owner-verified: the **VIRT** chip, the
      "SIMULATED DEVICE" banner, and the per-runtime-start log warning.
- [~] **A hardware-free project runs a full localization cycle end to end** — **reaches Ready,
      stops there.** Owner-verified: a project with a Virtual camera, PLC and vision output can
      be created from the wizard, saved, closed, reloaded, configured (signal map included) and
      taken to `Ready` with no hardware attached. It cannot go further, because **nothing can
      drive a PLC input value**: `bExecuteTrigger`, `bErrorReset` and the camera/pattern-group
      selectors are all inputs, and `VirtualPlcDevice` records writes without being able to
      produce reads. Tracked as **backlog #43**, a self-contained follow-up rather than a gap in
      what this phase set out to build — the registration, persistence, UI and R8 markers are
      all done and exercised.

**What Checkpoint D actually proved.** The hardware-free path is real up to Ready: project
creation, save/load, device configuration, calibration, signal mapping, task setup and runtime
entry all work with nothing plugged in. That is the whole registration-and-plumbing half of the
goal. The remaining half is one capability — value injection — on one device.

---

## Close-out: generated reference rebuilt (2026-08-23)

The Doxygen reference under `docs/generated/doxygen/` was last built **2026-07-29** — before
Phase 6 and Phase 7. Rebuilding it was not a no-op: three things had to be fixed first, and each
one was silent.

| Drift | Effect | Fix |
|---|---|---|
| `runtime_app/` was never added to the Doxyfile `INPUT` | The entire second shell — Phase 6's headline deliverable — was absent from the reference. `RECURSIVE = YES` only descends into roots already listed, so a new top-level source root stays invisible until named | `INPUT` now includes `$(NCR_PICKING_ROOT)/runtime_app` |
| `uml/11_runtime_shell.puml` was rendered but never published | `build_docs.bat` renders every `uml/*.puml`, so `runtime_shell.svg` was produced and copied into the HTML on every build — and unreachable, because nothing on `architecture_diagrams.dox` linked to it | `@section diag_11` added |
| `uml/02_device_families.puml` predated Phase D | The device diagram showed no virtual sub-type, and its three enums were missing `VirtualCamera`, `VirtualPlc`, `VirtualVisionOutput` | Virtual package added: six classes, the `isVirtualDevice()` predicate, inheritance and capability edges, plus notes on token-vs-enumerator spelling, the synthetic calibration and the `IPlcTagProvider` requirement |

`mainpage.dox` was also stale in a way that mattered: it still described "one application with
explicit Commission and Runtime modes," which is the architecture Phase 7 replaced. It now
documents two peer executables, the `ncr_shared` build rule, and the virtual devices.

**One real code defect surfaced by the rebuild.** Adding `runtime_app/` to the input produced
`runtime_shell_window.cpp:572: warning: end of file with unbalanced grouping commands`. A comment
describing the QSS theme tokens wrote `@{token}` literally, which Doxygen parsed as the
open-group command `@{` and never saw closed — so it swallowed the rest of the file into an
unterminated group. Escaped to `` `\@{token}` ``. Comment-only change; the warning is gone.

**Result:** exit 0, 8,629 HTML files (was 7,828), warnings 151 with none from `src/device/virtual`
or `runtime_app`. Verified present: `RuntimeShellWindow`, `RuntimeLayoutController`, all three
`Virtual*Device`/`Virtual*Cfg` pairs, `VirtualDeviceWidget`, and diagram 11.

Two unrelated findings were carried to the backlog rather than fixed here: **#44** (the script's
tool defaults point at `C:\BAO`, which does not exist on this machine — the build ran via the
documented environment-variable override) and **#45** (`AGENTS.md` scope cards produce 3
unresolvable `\ref` warnings; pre-existing).

---

## Risks

| # | Risk | Impact | Handling |
|---|---|---|---|
| **R1** | **The hand-off races the instance lock.** The outgoing shell must release the lock before the incoming one acquires it; if the incoming one tries too early it exits **silently, with no window** — indistinguishable from "clicking the menu item did nothing". | High | The incoming shell retries for a bounded window when launched with `--handoff`, and logs each failed attempt. Checkpoint B switches back and forth twice, because a race that fires one time in five passes a single check |
| **R2** | **A switch mid-cycle leaves a device half-released.** The runtime owns a camera, a PLC socket and a TCP port; exiting without an orderly stop can leave the port in TIME_WAIT and the next shell unable to bind. | High | Stop every task's runtime and confirm release **before** launching the sibling; refuse the switch and report if that fails. Never exit first and hope |
| **R3** | **Unifying the settings path orphans the user's existing settings.** `settings.dat` is obfuscated and carries a magic number and schema version; a path change reads as "no file", which silently resets theme, language and the remembered project. | Medium | A3 must migrate the existing `%APPDATA%\NCRN Pick\settings.dat` rather than merely repoint. Verify by checking that a pre-existing theme survives the first launch after the change |
| **R4** | **Moving `SystemLogForm` breaks the editor's log dock quietly.** It loads its own QSS resource and re-themes on `themeChanged`; a move that misses the `.ui` or the resource path produces an unstyled, still-working widget. | Low | Visual check in Checkpoint A. `.qrc` paths do not change — the resource stays at shell level, and the widget only *uses* it |
| **R5** | **One shared lock key makes development harder.** A developer can no longer run the editor and the runtime side by side to compare them. | Medium | That is the requested behaviour, and it matches the hardware reality. Worth deciding whether a debug-only escape hatch is wanted — **not** proposed here, because an escape hatch that reaches a field machine defeats the whole guard |
| **R6** | **Extracting the fakes changes test behaviour without changing test code.** ~20 tests depend on their exact semantics. | Medium | Move first with zero edits, run the suite, and only then rename. Any test that changes result is a bug introduced by the move |
| **R7** | **The `.ui` port silently changes the project-select page.** Its three distinct failure messages are the operator's only route out of a bad project, and they are laid out in code today. | Medium | `runtime_app/AGENTS.md` already carries "the startup path must never be a dead end" as an invariant. Re-run all four startup paths after C2 |
| **R8** | **A virtual device ends up in a commissioned project.** Once saved, a virtual camera is just another device sub-type; a station could be handed over "working" while running on simulated hardware, and the symptom is that everything passes and nothing moves. | **High** | D3: a distinct display name, a visible marker in the project tree, and a line in the task log every time a cycle runs against one. Decide whether the runtime shell refuses them outright — the safest answer, and the one that costs the demo capability the owner asked for, so it is the owner's call |
| **R9** | **The admin password is a shared secret in a file.** `settings.dat` is obfuscated, not encrypted; the default is `admin` and will be on every machine until someone changes it. | Medium | A4 stores a salted hash rather than the password, so copying the file does not hand over the credential. It does **not** stop anyone who knows the default — that is what the dongle is for, and until it exists the gate stops mis-taps and casual curiosity, not a determined operator. Say so in the docs rather than implying more |
| **R10** | **The credential interface gets designed for a dongle nobody has yet.** The owner named a second implementation, which is what justifies the seam — but a seam is one interface with one method, and it is easy to grow it into a plugin framework on speculation. | Low | `IAdminCredentialProvider` stays at `verify()` + `isConfigured()`. `verify()` deliberately never returns the stored secret, which is exactly what a dongle cannot provide and does not need to |

---

## Sequencing

```text
A1 SystemLogForm ──┐
A2 view menus  ────┼──► C3 runtime menubar
A3 settings    ────┘        ▲
                            │
B1 one key ──► B2 hand-off ─┤
                  │         │
                  └► B3 logs
C1 restructure ──► C2 .ui ──┘

D1 ──► D2/D3        (independent of everything above)
```

**A before C** because the menubar needs components that do not exist yet.
**B1 before B2** because a hand-off into a lock that still allows both shells proves
nothing.
**C1 before C2** so files are not moved twice.
**D is independent** and can be done at any point, including first if a warm-up task is
wanted.

Phase A and Phase D touch nothing the other needs and are safe to interleave. B and C are
the coupled pair.

---

## Definition of Done for this phase

Beyond each task's acceptance criteria:

- both shells build through `ncr_picking_all.pro`;
- architecture contract test passes, with new cases for the invariants this phase
  introduces (one instance key, `runtime_app/` structure, `tools/` not shipped);
- `docs/domains/runtime_app/runtime_shell.md` updated — its "One Process Per Shell" section
  currently states the *opposite* of what Phase 7 delivers, and its "To reach commissioning,
  launch `ncr_picking.exe` from the same folder. Close the runtime first" instruction is
  replaced by the menu action;
- `AGENT.md`, both shell scope cards, and `uml/11_runtime_shell.puml` reflect the new
  structure;
- backlog #35 and #36 closed with pointers here.
