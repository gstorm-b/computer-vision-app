# Umbrella project: builds BOTH application shells in one pass.
#
# Open this in Qt Creator (or qmake it from the CLI) to produce ncr_picking.exe and
# ncr_runtime.exe together, so a change under src/ cannot leave one shell rebuilt and the
# other stale — which is the failure this file exists to prevent.
#
# The two shell .pro files remain fully usable on their own; this only adds a way to drive
# both at once. Nothing here changes how either one builds.
#
# ── Canonical build directory ────────────────────────────────────────────────────────
#
#   build/all/<config>          e.g. build\all\Release
#
# Point BOTH the CLI and Qt Creator at that one directory (Qt Creator: Projects → Build
# Settings → Build directory). Sharing it is the whole benefit: one set of object files
# and one incremental state, so a build started in the IDE is not redone from the command
# line and vice versa.
#
# Layout qmake produces inside it:
#
#   Makefile                       this umbrella
#   src/Makefile                   shared static library
#   src/release/                   all src/ objects
#   Makefile.ncr_picking           editor shell
#   release/                       editor objects
#   runtime_app/Makefile.ncr_runtime
#   runtime_app/release/           runtime objects
#
# Neither executable is in that tree, and neither is the library. Both land in FIXED
# locations outside it, so one expression names them whether a target was built from here
# or on its own:
#
#   build/bin/<config>/            ncr_picking.exe + ncr_runtime.exe + ONE runtime set
#                                  + robot_assets/          (DESTDIR, qmake/app_common.pri)
#   build/shared_lib/<config>/     ncr_shared.lib           (qmake/common_deps.pri)
#
# Two shells in one output folder is deliberate: the runtime beside them is ~40 DLLs plus
# the Qt plugin tree, and two copies of that can drift to different Qt or Pylon versions —
# a mismatch that only surfaces on the customer's machine.
#
# ── What this costs ──────────────────────────────────────────────────────────────────
#
# src/ is compiled ONCE (Phase 6 / E7a). What still happens twice is linking the two
# executables and compiling each shell's own sources and .qrc set — the resources stay at
# shell level deliberately, because a .qrc inside a static library is dropped by the linker
# and its content silently disappears at runtime. See src/src.pro.

TEMPLATE = subdirs

SUBDIRS = shared editor runtime

# Paths MUST stay relative. With an absolute $$PWD/... path qmake resolves the editor
# subproject's makefile to plain "Makefile" — the same name as this umbrella's own — and
# the umbrella then invokes itself: infinite recursion, no build. Relative paths make
# qmake emit "Makefile.ncr_picking" and "runtime_app/Makefile.ncr_runtime" instead.
shared.file  = src/src.pro
editor.file  = ncr_picking.pro
runtime.file = runtime_app/ncr_runtime.pro

# The two shells remain peers with no dependency between them — a parallel-capable make can
# still build them concurrently — but both link the library, so it has to exist first.
editor.depends  = shared
runtime.depends = shared
