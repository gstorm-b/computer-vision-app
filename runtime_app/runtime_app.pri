# Operator runtime shell: entry point and tile-layout controller (src/), shell window (ui/).
# Depends on: every src/ module (top of the stack), same as components/app/.
#
# Nothing outside runtime_app/ may include these headers, and this shell must not include
# components/app/ headers either — the two shells are peers over the same src/ modules, not
# a hierarchy. Enforced by the architecture contract test.
#
# ── Why the include root is the REPOSITORY, not this folder ───────────────────────────
#
# Headers here are included as "runtime_app/ui/..." and "runtime_app/src/...".
#
# The obvious alternative — putting this directory on INCLUDEPATH and writing "ui/..." —
# would SHADOW src/ui/, which is already on the include path. Which header won would depend
# on INCLUDEPATH order, and the architecture contract test would read such an include as a
# reference to the src/ui module, so the ambiguity would not even be reported. Rooting at
# the repository makes every include say exactly which tree it means.
#
# It also puts components/app/ within reach of the preprocessor. That is fine: the contract
# test forbids this shell from including it, and it checks the text rather than trusting the
# include path to make it impossible.
INCLUDEPATH += $$PWD/..

SOURCES += \
    $$PWD/src/main.cpp \
    $$PWD/src/runtime_layout_controller.cpp \
    $$PWD/ui/runtime_shell_window.cpp

HEADERS += \
    $$PWD/src/runtime_layout_controller.h \
    $$PWD/ui/runtime_shell_window.h

FORMS += \
    $$PWD/ui/runtime_shell_window.ui

# EXTRA_TRANSLATIONS, deliberately — NOT TRANSLATIONS. Same reasoning as components/app/app.pri: both
# variables are released and embedded, but only TRANSLATIONS is updated by lupdate, from the
# declaring project's own sources. This shell compiles three files; letting it drive lupdate
# against the shared .ts marks everything else in the product as vanished.
#
# translations/ncr_translations.pro owns the update and sees every source.
EXTRA_TRANSLATIONS += \
    $$PWD/../components/app/translations/ncr_picking_ja_JP.ts
CONFIG += lrelease
CONFIG += embed_translations
