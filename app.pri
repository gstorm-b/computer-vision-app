# Application shell: entry point, main window, app-level log form.
# Depends on: every module (top of the dependency stack).
# NOTE: these files still live at the repo root / src root; a later
# restructure phase moves them under app/.

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/src/system_log_form.cpp

HEADERS += \
    $$PWD/mainwindow.h \
    $$PWD/windows_helper.h \
    $$PWD/src/system_log_form.h

FORMS += \
    $$PWD/mainwindow.ui \
    $$PWD/src/system_log_form.ui
