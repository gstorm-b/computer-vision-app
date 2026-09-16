# ---------------------------------------------------------------------------
# deploy_dependencies.pri
#
# Opt-in post-link step that copies the project's third-party RUNTIME DLLs next
# to the built executable, so the app can run from the build/deploy folder
# without those dependency directories on PATH. Enable it with:
#
#     qmake ... CONFIG+=deploy_deps
#
# It is OFF by default so normal dev builds stay fast and never touch the target
# directory. Scope (Windows only):
#   - ADS docking      : qtadvanceddocking[d].dll, bundled in the repo under
#                        3rdparty/advance_docking/bin.
#   - OpenCV world     : $${OPENCV_WORLD_*}.dll from OPENCV_BIN.
#   - Basler Pylon     : every top-level *.dll in PYLON_RUNTIME_DIR.
#   - JAI/Pleora eBUS  : Pv*64 / Eb*64 / Pt*64 / SimpleImagingLib64 from
#                        EBUS_RUNTIME_DIR. By name prefix and NOT by *.dll glob —
#                        that folder is shared and also holds an unrelated OpenCV
#                        4.10 build. See the block itself for why that matters.
#   - Qt runtime       : Qt DLLs + plugins via windeployqt (--release/--debug).
#
# Intentionally NOT handled here:
#   - RobotKinematics / Coal / Assimp / Boost / robot_assets — deployed by
#     components/RobotKinematics/robotkinematics.pri (listing them here too would
#     double-copy). That component is reusable outside this repository and ships
#     its own examples, so it owns the names of its own runtime files; copying
#     them from here would duplicate that knowledge and break its other consumers
#     the first time a version changed. Both mechanisms resolve their destination
#     from DESTDIR, so the split costs nothing at the output: one folder either
#     way. See qmake/app_common.pri for where DESTDIR is set and why the order
#     matters.
#   - Full Basler deployment (GenTL producers + GENICAM_GENTL64_PATH) — that is
#     installer territory, tracked in docs/backlog/later_todo_list.md #27.
#   - The eBUS NDIS6 filter driver and its GenICam/log4cxx subtrees. The driver
#     cannot be deployed by copying files at all; a customer machine needs the
#     eBUS installer. Same boundary as the Basler GenTL producers above.
#
# Each DLL is copied only when it is missing from the target ("if not exist"),
# so pre-existing DLLs are left untouched; windeployqt is itself idempotent and
# only refreshes stale/missing Qt artefacts. A missing OPENCV_BIN /
# PYLON_RUNTIME_DIR / windeployqt only warns and skips that family; it never
# fails the build.
#
# Must be included AFTER qmake/local_dependencies.pri so OPENCV_WORLD_RELEASE /
# OPENCV_WORLD_DEBUG are already resolved.
# ---------------------------------------------------------------------------

win32:contains(CONFIG, deploy_deps) {

    # Target = directory of the produced binary (honour an explicit DESTDIR).
    CONFIG(debug, debug|release): DEPLOY_DEST = $$OUT_PWD/debug
    else:                         DEPLOY_DEST = $$OUT_PWD/release
    !isEmpty(DESTDIR):            DEPLOY_DEST = $$DESTDIR

    DEPLOY_RUNTIME_DLLS =

    # --- ADS docking (bundled in the repo) ---
    DEPLOY_ADS_BIN = $$PWD/../3rdparty/advance_docking/bin
    CONFIG(debug, debug|release): DEPLOY_RUNTIME_DLLS += $$DEPLOY_ADS_BIN/qtadvanceddockingd.dll
    else:                         DEPLOY_RUNTIME_DLLS += $$DEPLOY_ADS_BIN/qtadvanceddocking.dll

    # --- OpenCV world (from OPENCV_BIN; names resolved by opencv_dependency.pri) ---
    DEPLOY_OPENCV_BIN = $$(OPENCV_BIN)
    # Same precedence as the include/lib paths: environment first, then the untracked
    # qmake/local_paths.pri that local_dependencies.pri has already read.
    isEmpty(DEPLOY_OPENCV_BIN): DEPLOY_OPENCV_BIN = $$OPENCV_BIN
    isEmpty(DEPLOY_OPENCV_BIN) {
        warning("deploy_deps: OPENCV_BIN not set - OpenCV world DLL will not be deployed")
    } else {
        CONFIG(debug, debug|release): DEPLOY_RUNTIME_DLLS += $$DEPLOY_OPENCV_BIN/$${OPENCV_WORLD_DEBUG}.dll
        else:                         DEPLOY_RUNTIME_DLLS += $$DEPLOY_OPENCV_BIN/$${OPENCV_WORLD_RELEASE}.dll
    }

    # --- Basler Pylon runtime (every top-level DLL in PYLON_RUNTIME_DIR) ---
    DEPLOY_PYLON_DIR = $$(PYLON_RUNTIME_DIR)
    isEmpty(DEPLOY_PYLON_DIR): DEPLOY_PYLON_DIR = $$PYLON_RUNTIME_DIR
    isEmpty(DEPLOY_PYLON_DIR) {
        warning("deploy_deps: PYLON_RUNTIME_DIR not set - Pylon runtime DLLs will not be deployed")
    } else {
        DEPLOY_RUNTIME_DLLS += $$files($$DEPLOY_PYLON_DIR/*.dll)
    }

    # --- JAI / Pleora eBUS runtime ---
    #
    # ⚠️ By NAME PREFIX, never $$files(*.dll). The eBUS runtime folder is shared, and it also
    # holds an unrelated OpenCV 4.10 build — opencv_world410.dll, opencv_world410d.dll and
    # opencv_ffmpeg410_64.dll, ~208 MB together. A glob would copy all three next to our
    # executable. Today the names differ from ours (opencv_world4110) so nothing would break
    # visibly, which is precisely what makes it dangerous: it would sit there unnoticed until
    # this project moved to OpenCV 4.10, and then our exe would load a stranger's build from
    # its own directory. The prefixes below cover the C++ SDK the camera device links.
    #
    # Not copied, deliberately: the *DotNet* assemblies, PvGUI64_VC*.dll (the eBUS dialogs,
    # which we do not use), and the tools. The GenICam XML/log4cxx subtrees are resolved by
    # eBUS through its own environment, and the NDIS6 filter driver cannot be deployed by
    # copying files at all — a customer machine needs the eBUS installer. That is the same
    # boundary Pylon's GenTL producers sit behind (later_todo_list.md #27).
    DEPLOY_EBUS_DIR = $$(EBUS_RUNTIME_DIR)
    isEmpty(DEPLOY_EBUS_DIR): DEPLOY_EBUS_DIR = $$EBUS_RUNTIME_DIR
    isEmpty(DEPLOY_EBUS_DIR) {
        # Derived from the installer's own variable, the same fallback ebus_dependency.pri uses.
        DEPLOY_EBUS_ROOT = $$(PUREGEV_ROOT)
        !isEmpty(DEPLOY_EBUS_ROOT) {
            DEPLOY_EBUS_DIR = $$clean_path($$(CommonProgramFiles))/Pleora/eBUS SDK
        }
    }
    isEmpty(DEPLOY_EBUS_DIR) {
        warning("deploy_deps: EBUS_RUNTIME_DIR not set - eBUS runtime DLLs will not be deployed")
    } else {
        DEPLOY_RUNTIME_DLLS += $$files($$DEPLOY_EBUS_DIR/Pv*64.dll)
        DEPLOY_RUNTIME_DLLS += $$files($$DEPLOY_EBUS_DIR/Eb*64.dll)
        DEPLOY_RUNTIME_DLLS += $$files($$DEPLOY_EBUS_DIR/Pt*64.dll)
        DEPLOY_RUNTIME_DLLS += $$files($$DEPLOY_EBUS_DIR/SimpleImagingLib64.dll)
    }

    # --- Emit a "copy if missing" post-link command per existing source DLL ---
    for(dll, DEPLOY_RUNTIME_DLLS) {
        exists($$dll) {
            DEPLOY_TARGET = $$DEPLOY_DEST/$$basename(dll)
            QMAKE_POST_LINK += if not exist $$shell_quote($$shell_path($$DEPLOY_TARGET)) \
                copy /Y $$shell_quote($$shell_path($$dll)) $$shell_quote($$shell_path($$DEPLOY_TARGET)) \
                $$escape_expand(\\n\\t)
        }
    }

    # --- Qt runtime + plugins (windeployqt on the linked binary) ---
    DEPLOY_WINDEPLOYQT = $$[QT_INSTALL_BINS]/windeployqt.exe
    DEPLOY_QT_EXE      = $$DEPLOY_DEST/$${TARGET}.exe
    CONFIG(debug, debug|release): DEPLOY_QT_MODE = --debug
    else:                         DEPLOY_QT_MODE = --release
    exists($$DEPLOY_WINDEPLOYQT) {
        QMAKE_POST_LINK += $$shell_quote($$shell_path($$DEPLOY_WINDEPLOYQT)) $$DEPLOY_QT_MODE \
            $$shell_quote($$shell_path($$DEPLOY_QT_EXE)) \
            $$escape_expand(\\n\\t)
    } else {
        warning("deploy_deps: windeployqt not found at $$DEPLOY_WINDEPLOYQT - Qt runtime will not be deployed")
    }
}
