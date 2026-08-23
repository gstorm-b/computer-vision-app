# Everything needed to COMPILE code that lives in this repository — shared by the static
# library (src/src.pro) and by both application shells.
#
# The split, and why it exists
# ---------------------------------------------------------------------------------------
# Before Phase 6 / E7 both shells compiled every src/ module themselves, so a full rebuild
# compiled src/ twice (~25 minutes measured on 2026-08-21). Now src/ is compiled once into
# ncr_shared and both shells link it.
#
#   qmake/common_deps.pri   this file — compile settings and third-party dependencies.
#                           Included by the library AND by the shells.
#   qmake/app_common.pri    shell-only — resources, file version, the link against
#                           ncr_shared, and dependency deployment.
#
# A dependency belongs here when the library needs it to compile. It belongs in
# app_common.pri when only an executable can use it.
#
# Link dependencies are stated here as well, even though a static library does not link:
# ncr_shared.lib records no dependency on user32, coal or opencv_world, so the shells must
# name them or the link fails with unresolved externals that point at library code.
#
# All paths are $$PWD-relative ($$PWD is this qmake/ directory).

include($$PWD/version.pri)

QT += core gui network sql

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

CONFIG += c++17

# Opt-in MSVC parallel compilation; a no-op unless CONFIG+=multicore is passed to qmake.
# Off by default because whether it helps depends on the machine's RAM, not on the
# project. See the file for the jom interaction before enabling it in Qt Creator.
include($$PWD/multicore.pri)

# Module code resolves includes rooted at src/ (e.g. #include "model/project.h"), and
# 3rdparty-rooted ones (e.g. "qtpropertybrowser/QtVariantProperty").
INCLUDEPATH += \
    $$PWD/../src \
    $$PWD/../3rdparty

# --- Where the shared static library is produced ---------------------------------------
#
# A fixed location under the repo-root build/ rather than one derived from OUT_PWD, so the
# expression is identical for the library that writes it and the shells that link it, no
# matter which build directory each was configured with. That is what keeps the two shells
# buildable on their own (docs/rules/build_and_verification.md) as well as together
# through ncr_picking_all.pro.
#
# Debug and Release are kept apart by the subfolder. Two Release builds from two different
# build directories do share this one .lib — they produce the same library from the same
# sources with the same flags, so that is benign, but it is the reason the path carries the
# configuration in its name and not the build directory.
NCR_SHARED_LIB_NAME = ncr_shared
CONFIG(debug, debug|release): NCR_SHARED_LIB_DIR = $$PWD/../build/shared_lib/debug
else:                         NCR_SHARED_LIB_DIR = $$PWD/../build/shared_lib/release

# --- Windows API ------------------------------------------------------------------------
# src/core/utils/single_instance_guard.cpp calls AllowSetForegroundWindow(). src/core's own
# .pri already asks for user32 so the library resolves it, but a static library does not
# propagate that to whoever links it — the shells must ask for it themselves.
win32: LIBS += -luser32

# --- ADS docking (prebuilt, in-repo) ----------------------------------------------------
INCLUDEPATH += $$PWD/../3rdparty/advance_docking/include
DEPENDPATH  += $$PWD/../3rdparty/advance_docking/include

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../3rdparty/advance_docking/lib/ -lqtadvanceddocking
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../3rdparty/advance_docking/lib/ -lqtadvanceddockingd

# --- OpenCV + Basler Pylon --------------------------------------------------------------
# Both shells include <pylon/PylonIncludes.h> directly for PylonInitialize/Terminate, and
# src/ headers they include reach OpenCV, so both sides need these.
include($$PWD/local_dependencies.pri)

# --- Robot kinematics component ---------------------------------------------------------
#
# Forward / inverse kinematics + the optional Coal mesh-collision backend. Consumed as
# source from components/RobotKinematics/.
#
# Included on BOTH sides, but for different reasons, and the sources must land on only one:
#
#   library : compiles the component's sources.
#   shells  : need the same INCLUDEPATH and the same ROBOTKINEMATICS_HAVE_COAL_MESH_BACKEND
#             define — compiling the library and a shell with that define disagreeing would
#             be an ODR violation across a header the two share — plus the coal/assimp/boost
#             link libraries and the post-link copy of coal.dll and the mesh assets. They
#             must NOT compile the sources again: that is the duplication this task removes.
#
# The SOURCES/HEADERS the .pri appends are therefore taken away again on the shell side.
# Save-and-restore rather than a bare assignment so this stays correct regardless of what
# the including file had already declared.
!contains(CONFIG, ncr_building_shared_lib) {
    NCR_RK_SAVED_SOURCES = $$SOURCES
    NCR_RK_SAVED_HEADERS = $$HEADERS
}

include($$PWD/../components/RobotKinematics/robotkinematics.pri)

!contains(CONFIG, ncr_building_shared_lib) {
    SOURCES = $$NCR_RK_SAVED_SOURCES
    HEADERS = $$NCR_RK_SAVED_HEADERS
}
