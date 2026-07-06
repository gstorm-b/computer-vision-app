# ncr_picking

Industrial vision-picking application (Qt 6 / C++17, qmake, MSVC 2022, Windows).
One `ncr_picking.exe` with explicit Commission and Runtime modes, centered on
`TaskLocalization`: camera capture, pattern matching, calibration, and result
output to PLC/robot targets.

First-release integrations: Basler GigE camera, Mitsubishi MC-protocol PLC,
VisionOutput TCP/IP server/client, and advisory RobotKinematics checking for
outgoing pick poses.

## Where to start

| You are | Read |
|---|---|
| A human contributor | [docs/README.md](docs/README.md) — documentation map and source-of-truth groups |
| An AI/code agent | [AGENT.md](AGENT.md) — entrypoint, operating rules, authority map |
| Setting up a build | [docs/rules/build_and_verification.md](docs/rules/build_and_verification.md) |

## Build (summary)

Local machine paths come from environment variables (no hard-coded paths):
`QT_MSVC_DIR`, `VCVARS`, `OPENCV_ROOT`/`OPENCV_BIN`, `PYLON_ROOT`/`PYLON_RUNTIME_DIR`, ...
See the build doc above for the full list and exact commands.

Fresh clones must also provision the prebuilt `3rdparty/` install trees
(Eigen, Coal, Boost, Assimp) — they are not tracked in git. See
[3rdparty/README.md](3rdparty/README.md).

```bat
call "%VCVARS%"
mkdir %NCR_PICKING_ROOT%\build\msvc_debug & cd /d %NCR_PICKING_ROOT%\build\msvc_debug
qmake -o Makefile "%NCR_PICKING_ROOT%\ncr_picking.pro" -spec win32-msvc CONFIG+=debug
nmake /nologo
```

## Repository layout

| Path | Contents |
|---|---|
| `ncr_picking.pro` | Root qmake project — includes one `.pri` per module, no file lists of its own |
| `app/` | Application shell: entry point, main window, translations (`app/app.pri`) |
| `src/<module>/` | Domain modules, one `.pri` each: `core` (settings/logger/utils), `device`, `calibration`, `matching`, `model`, `runtime`, `form`, `widgets`, `libwg` |
| `components/` | Reusable libraries with their own tests/docs (RobotKinematics) |
| `3rdparty/` | Vendored third-party code and prebuilt dependency trees |
| `qmake/` | Shared qmake dependency includes (OpenCV, Pylon, deployment) |
| `tests/` | Standalone test subprojects (architecture contract, calibration, device tests) |
| `tools/` | Developer tools (virtual vision TCP/IP device for manual integration testing) |
| `scripts/` | Helper scripts (`build_test.bat` builds the architecture contract test) |
| `docs/` | Rules, domain specs, backlog, history — see [docs/README.md](docs/README.md) |
| `uml/` | Current architecture diagrams |
| `resrc/` | Icons, themes (QSS), application resources |

## Module dependency rule

Lower layers must not include higher ones:
`core` → (`device`, `matching`, `calibration`) → (`model`, `runtime`) → UI (`form`, `widgets`, `libwg`) → app shell.
`components/*` stay standalone. Enforced conventions live in
[docs/rules/design_rules.md](docs/rules/design_rules.md).
