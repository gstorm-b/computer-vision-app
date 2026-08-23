# Application shell: entry point, main window, and the application translations.
# Depends on: every module (top of the stack).
#
# SystemLogForm used to live here. Phase 7 / A1 moved it to src/ui/forms/ because the
# operator runtime shell needs the same log viewer, and the two shells are peers that may
# not include each other's headers — src/ui is the module both may reach.

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/mainwindow.cpp

HEADERS += \
    $$PWD/mainwindow.h

FORMS += \
    $$PWD/mainwindow.ui

TRANSLATIONS += \
    $$PWD/translations/ncr_picking_ja_JP.ts
CONFIG += lrelease
CONFIG += embed_translations
