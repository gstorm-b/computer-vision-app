# Shared static library: every src/ module, compiled ONCE.
#
# Both application shells link this instead of compiling the modules themselves. Before
# Phase 6 / E7 each shell compiled all of src/, so an umbrella full rebuild compiled
# everything twice — ~25 minutes measured on 2026-08-21.
#
# What goes where
# ---------------------------------------------------------------------------------------
# A new source file still registers in its module's .pri, exactly as before. Nothing about
# adding code changed; only who compiles it. The .pri files below are the same ones the
# shells used to include.
#
# Build output
# ---------------------------------------------------------------------------------------
# ncr_shared.lib goes to build/shared_lib/<config>/ under the repo root — a fixed location
# so the shells can name it with one expression whether they were built through
# ncr_picking_all.pro or on their own. Object files stay in this project's own build
# directory. See qmake/common_deps.pri for the full reasoning.

TEMPLATE = lib
CONFIG  += staticlib

# Read by qmake/common_deps.pri, which is included below. Must be set FIRST: it decides
# whether the RobotKinematics sources are compiled here (yes) or dropped again for a shell.
CONFIG += ncr_building_shared_lib

# A static library is never linked and never run, so there is no binary for the component's
# post-link step to copy coal.dll and the mesh assets next to. The shells still do that.
CONFIG += robotkinematics_no_copy_dlls robotkinematics_no_copy_assets

include($$PWD/../qmake/common_deps.pri)

TARGET  = $$NCR_SHARED_LIB_NAME
DESTDIR = $$NCR_SHARED_LIB_DIR

# One .pri per module: each module lists only its own SOURCES/HEADERS/FORMS, so adding a
# file touches that module's .pri, never this file.
# Dependency levels (low to high) — lower levels must not include higher ones;
# see README.md "Module dependency rule".
include($$PWD/core/core.pri)                  # level 0: settings/logger/utils
include($$PWD/device/device.pri)              # level 1: device families
include($$PWD/calibration/calibration.pri)    # level 1: calibration
include($$PWD/matching/matching.pri)          # level 1: pattern matching
include($$PWD/model/model.pri)                # level 2: project/task/pipeline
include($$PWD/runtime/runtime.pri)            # level 2: per-device runners
include($$PWD/ui/ui.pri)                      # UI: forms (dialogs/wizards/pages) + widgets

# Vendored Qt Solutions property browser. Headers are included as
# "qtpropertybrowser/<name>" via the 3rdparty include root set in common_deps.pri.
include($$PWD/../3rdparty/qtpropertybrowser/qtpropertybrowser_vendor.pri)

# --- This library carries NO resources. Ever. --------------------------------------------
#
# Qt registers a .qrc through a static initialiser in the generated qrc_*.cpp. Inside a
# static library the linker drops that object file, because nothing references a symbol in
# it — and the resource then does not exist at runtime. Icons, the QSS theme and :/i18n
# resolve to nothing, with NO build error and no warning. That silent failure is the reason
# extracting this library was deferred for as long as it was.
#
# So every .qrc is listed in qmake/app_common.pri, at shell level, and this line takes back
# any that an included .pri added on its own — qtpropertybrowser_vendor.pri adds one. Do not
# "tidy" the resources in here.
#
# Library code may still USE those resources: the Qt resource system is process-global, so
# what the executable registers is visible here.
RESOURCES =
