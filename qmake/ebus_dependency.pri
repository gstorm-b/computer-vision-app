# JAI / Pleora eBUS SDK qmake dependency — the GigE Vision stack behind the JAI camera device.
#
# Set paths with environment variables or qmake command-line variables:
#   EBUS_INCLUDE_DIR, EBUS_LIB_DIR
#   EBUS_INCLUDE_DIR=C:\Program Files\JAI\eBUS SDK\Includes
#   EBUS_LIB_DIR=C:\Program Files\JAI\eBUS SDK\Libraries
# Optional runtime directory, used by qmake/deploy_dependencies.pri:
#   EBUS_RUNTIME_DIR
#
# Neither is normally needed. The eBUS installer sets PUREGEV_ROOT, and both directories are
# fixed offsets inside it, so a machine with the SDK installed is already configured. The
# explicit variables exist for a non-default install and for parity with the Pylon wiring.
#
# ── Why there is no -l here ──────────────────────────────────────────────────────────────
#
# The eBUS headers auto-link through `#pragma comment(lib, ...)` and pick the "64"-suffixed
# import libraries themselves under _WIN64. Naming them again would be a second, silently
# divergent list to maintain. -L is genuinely all that is required; the absence of -l below
# is deliberate, not an omission. (`reference source/JaiCamTest/JaiCamTest.pro` documents the
# same thing, and that project links and runs against a real camera.)

isEmpty(EBUS_INCLUDE_DIR): EBUS_INCLUDE_DIR = $$(EBUS_INCLUDE_DIR)
isEmpty(EBUS_LIB_DIR):     EBUS_LIB_DIR     = $$(EBUS_LIB_DIR)
isEmpty(EBUS_RUNTIME_DIR): EBUS_RUNTIME_DIR = $$(EBUS_RUNTIME_DIR)

# Machine-local fallback — same file and same reasoning as pylon_dependency.pri.
exists($$PWD/local_paths.pri): include($$PWD/local_paths.pri)

# Last resort: derive both from the installer's own variable. This is what makes the JAI
# camera build with no per-machine setup at all on a station that has the SDK.
EBUS_ROOT_FROM_ENV = $$(PUREGEV_ROOT)
!isEmpty(EBUS_ROOT_FROM_ENV) {
    # PUREGEV_ROOT is set with a trailing backslash by the installer; clean() normalises it so
    # the joined paths do not come out with a doubled separator.
    EBUS_ROOT_FROM_ENV = $$clean_path($$EBUS_ROOT_FROM_ENV)
    isEmpty(EBUS_INCLUDE_DIR): EBUS_INCLUDE_DIR = $$EBUS_ROOT_FROM_ENV/Includes
    isEmpty(EBUS_LIB_DIR):     EBUS_LIB_DIR     = $$EBUS_ROOT_FROM_ENV/Libraries
}

isEmpty(EBUS_INCLUDE_DIR) {
    error("EBUS_INCLUDE_DIR must point to the JAI/Pleora eBUS SDK 'Includes' directory. \
Install the eBUS SDK (it sets PUREGEV_ROOT), or set EBUS_INCLUDE_DIR / EBUS_LIB_DIR in \
qmake/local_paths.pri.")
}
isEmpty(EBUS_LIB_DIR) {
    error("EBUS_LIB_DIR must point to the JAI/Pleora eBUS SDK 'Libraries' directory. \
Install the eBUS SDK (it sets PUREGEV_ROOT), or set EBUS_INCLUDE_DIR / EBUS_LIB_DIR in \
qmake/local_paths.pri.")
}

# Fail here rather than at link time. An unresolved PvDevice symbol names a file in src/ and
# sends the reader after our code; this names the SDK and the variable that is wrong.
!exists($$EBUS_INCLUDE_DIR/PvDevice.h) {
    error("EBUS_INCLUDE_DIR does not look like the eBUS SDK includes: \
'$$EBUS_INCLUDE_DIR/PvDevice.h' not found.")
}

INCLUDEPATH += $$EBUS_INCLUDE_DIR
DEPENDPATH  += $$EBUS_INCLUDE_DIR

win32: LIBS += -L$$EBUS_LIB_DIR
