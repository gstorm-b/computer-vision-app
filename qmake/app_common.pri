# Shared qmake configuration for every application SHELL built from this repository.
#
# Two shells exist (Phase 6): the commissioning app `ncr_picking.pro` at the repo root and
# the operator runtime `runtime_app/ncr_runtime.pro`. Everything they share lives here or
# in qmake/common_deps.pri rather than being copied into each .pro — a copy drifts the
# first time a module is added, and the drift only shows up as a link error in whichever
# shell was forgotten.
#
# A shell .pro is expected to add only what is genuinely its own: its module .pri, its
# icon, and its install rules.
#
# This file holds what only an executable can have. The compile settings and the
# third-party dependencies are in qmake/common_deps.pri, which the static library
# (src/src.pro) includes as well — see its header for the reasoning behind the split.
#
# All paths are $$PWD-relative ($$PWD is this qmake/ directory) so the file behaves
# identically whether it is included from the repo root or from a subdirectory.

# --- One output folder for both shells ---------------------------------------------------
#
# Build layout and deploy layout are different questions. The two shells must build in
# separate directories — they collide on Makefile, .qmake.stash, ui_*.h and the moc_*.cpp
# generated from the same shared src/ headers — but they must LAND in one folder, because
# the runtime beside them is ~40 DLLs plus the Qt plugin tree, and two copies of that can
# drift to different Qt or Pylon versions. A drift that only shows up on the customer's
# machine. Intermediates stay in each shell's own OUT_PWD; only the linked binary and its
# runtime come here.
#
# A FIXED location under the repo-root build/, not one derived from OUT_PWD — the same
# reasoning as NCR_SHARED_LIB_DIR in common_deps.pri. The expression has to name one folder
# whether a shell was built through ncr_picking_all.pro or on its own, and an OUT_PWD-
# relative path cannot: the umbrella and the two standalone build directories sit at three
# different depths, so it would quietly go back to producing one folder per shell.
#
# ORDER IS LOAD-BEARING. DESTDIR must be set BEFORE common_deps.pri, because that file
# includes components/RobotKinematics/robotkinematics.pri, which resolves its DLL/asset
# copy destination from DESTDIR at include time. Set it after, and coal.dll, assimp and
# robot_assets/ go to each shell's own release/ folder while everything else goes to bin/ —
# an install image that is missing the mesh-collision runtime and says nothing about it.
CONFIG(debug, debug|release): DESTDIR = $$PWD/../build/bin/debug
else:                         DESTDIR = $$PWD/../build/bin/release

include($$PWD/common_deps.pri)

# Windows file-resource version. Only an executable carries one; the number itself comes
# from qmake/version.pri, which common_deps.pri already included.
VERSION = $$NCR_APP_VERSION

CONFIG += deploy_deps

# --- The shared library ------------------------------------------------------------------
#
# src/ — every module, the vendored property browser and the RobotKinematics sources — is
# compiled ONCE into this library and linked by both shells. Adding a source file still
# means editing that module's .pri; the library is what consumes them now.
#
# PRE_TARGETDEPS makes the shell relink when the library changes. Without it a rebuilt
# library would leave an already-linked shell silently stale, which is the exact failure
# the umbrella project exists to prevent.
win32-msvc*: NCR_SHARED_LIB_FILE = $$NCR_SHARED_LIB_DIR/$${NCR_SHARED_LIB_NAME}.lib
else:        NCR_SHARED_LIB_FILE = $$NCR_SHARED_LIB_DIR/lib$${NCR_SHARED_LIB_NAME}.a

LIBS           += -L$$NCR_SHARED_LIB_DIR -l$$NCR_SHARED_LIB_NAME
PRE_TARGETDEPS += $$NCR_SHARED_LIB_FILE

# --- Resources ---------------------------------------------------------------------------
#
# Every .qrc in the project is listed HERE, at shell level, and nowhere else. This is not a
# tidiness preference — it is the reason the static library above is safe.
#
# Qt registers a resource through a static initialiser in the generated qrc_*.cpp. Inside a
# static library that object file is only pulled in if something already references a symbol
# in it, and nothing does; the linker drops it. The resource then does not exist at runtime:
# icons, the QSS theme and :/i18n silently resolve to nothing, with NO build error and no
# warning. Q_INIT_RESOURCE() in every consumer is the workaround, and it is one that gets
# forgotten.
#
# Compiling three small qrc_*.cpp twice costs a second or two. Keep them here.
#
# The code that USES these resources lives in the library, and that is fine: the resource
# system is process-global, so a resource registered by the executable is visible to
# library code.
RESOURCES += \
    $$PWD/../3rdparty/advance_docking/include/ads.qrc \
    $$PWD/../3rdparty/qtpropertybrowser/qtpropertybrowser.qrc \
    $$PWD/../resrc.qrc

# Opt-in dependency deployment: copy third-party runtime DLLs (ADS docking, OpenCV world,
# Basler Pylon) next to the built binary when the target dir is missing them, and run
# windeployqt for the Qt runtime + plugins. Enabled by the CONFIG line above.
# RobotKinematics/Coal is deployed by robotkinematics.pri, included via common_deps.pri.
#
# Included here rather than per-.pro so a second shell gets its own deployed runtime by
# construction instead of by someone remembering to add it. Must come after
# local_dependencies.pri (common_deps.pri) so the OpenCV world DLL names are resolved.
include($$PWD/deploy_dependencies.pri)
