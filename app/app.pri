# Application shell: entry point, main window, app-level log form, and the
# application translations. Depends on: every module (top of the stack).

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/system_log_form.cpp

HEADERS += \
    $$PWD/mainwindow.h \
    $$PWD/system_log_form.h

FORMS += \
    $$PWD/mainwindow.ui \
    $$PWD/system_log_form.ui

TRANSLATIONS += \
    $$PWD/translations/ncr_picking_ja_JP.ts
CONFIG += lrelease
CONFIG += embed_translations
