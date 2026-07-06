# 3rdparty layout

Two kinds of content live here. Only the first kind is tracked in git.

## Tracked in git (vendored)

| Folder | What | Consumed by |
|---|---|---|
| `advance_docking/` | Qt Advanced Docking System (headers, import libs, runtime DLLs, `ads.qrc`) | root `.pro` (`LIBS`, `INCLUDEPATH`, `RESOURCES`) |
| `qtpropertybrowser/` | Qt Solutions property browser, compiled into the app as source | `qtpropertybrowser_vendor.pri` included by the root `.pro` |

Do not modify vendored files for app features. App-side property-browser
extensions belong in `src/ui/widgets/property_browser/`.

## Provisioned locally (NOT tracked in git)

These prebuilt install trees are required to build, but are not tracked
(~17k files; see the repo-root `.gitignore`). Copy them from the team share
or an existing working clone into exactly these paths:

| Folder | What | Consumed by |
|---|---|---|
| `eigen/` | Eigen headers (header-only) | `components/RobotKinematics/robotkinematics.pri` |
| `coal/` | Coal collision library install tree (`include/`, `lib/coal.lib`, `bin/` runtime DLLs) | `robotkinematics.pri` (mesh-collision backend) |
| `boost/` | Boost 1.87 install tree (`include/boost-1_87`, `lib/`) | Coal dependency via `robotkinematics.pri` |
| `assimp/` | Assimp install tree (mesh loading) | `robotkinematics.pri` |
| `fcl/` | FCL install tree | not referenced by any tracked `.pro`/`.pri`; kept for RobotKinematics experiments — removal candidate |
| `libccd/` | libccd install tree | same as `fcl/` — removal candidate |

`robotkinematics.pri` resolves everything relative to the repository root
(`ROBOTKINEMATICS_3RDPARTY = $$PWD/../../3rdparty`), so no environment
variables are needed for these trees — the folders just have to exist here.

OpenCV and Basler Pylon are NOT under `3rdparty/`; they are located through
environment variables (`OPENCV_*`, `PYLON_*`) — see
[docs/rules/build_and_verification.md](../docs/rules/build_and_verification.md).

## Verify your provisioning

A build fails fast and obviously when a tree is missing. Quick manual check:

```powershell
Get-ChildItem 3rdparty -Directory   # expect: advance_docking assimp boost coal eigen fcl libccd qtpropertybrowser
Test-Path 3rdparty/boost/include/boost-1_87
Test-Path 3rdparty/coal/lib/coal.lib
```

Note: git history from before 2026-07 still contains these trees, so
`git checkout <old-commit>` restores them. Purging them from history
(`git filter-repo`) is possible but needs a coordinated force-push.
