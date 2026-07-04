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
#   - Qt runtime       : Qt DLLs + plugins via windeployqt (--release/--debug).
#
# Intentionally NOT handled here:
#   - RobotKinematics / Coal / Assimp / Boost — already deployed by
#     components/RobotKinematics/robotkinematics.pri (would double-copy).
#   - Full Basler deployment (GenTL producers + GENICAM_GENTL64_PATH) — that is
#     installer territory, tracked in docs/backlog/later_todo_list.md #27.
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
    isEmpty(DEPLOY_OPENCV_BIN) {
        warning("deploy_deps: OPENCV_BIN not set - OpenCV world DLL will not be deployed")
    } else {
        CONFIG(debug, debug|release): DEPLOY_RUNTIME_DLLS += $$DEPLOY_OPENCV_BIN/$${OPENCV_WORLD_DEBUG}.dll
        else:                         DEPLOY_RUNTIME_DLLS += $$DEPLOY_OPENCV_BIN/$${OPENCV_WORLD_RELEASE}.dll
    }

    # --- Basler Pylon runtime (every top-level DLL in PYLON_RUNTIME_DIR) ---
    DEPLOY_PYLON_DIR = $$(PYLON_RUNTIME_DIR)
    isEmpty(DEPLOY_PYLON_DIR) {
        warning("deploy_deps: PYLON_RUNTIME_DIR not set - Pylon runtime DLLs will not be deployed")
    } else {
        DEPLOY_RUNTIME_DLLS += $$files($$DEPLOY_PYLON_DIR/*.dll)
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
