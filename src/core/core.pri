# Core module (dependency level 0): app settings, logging, shared utils,
# gadget/meta helpers, settings keys, Windows helpers.
# Must not include any other module.

# single_instance_guard.cpp calls AllowSetForegroundWindow() so the running instance can
# actually take the foreground when a second launch asks it to; without the handover
# Windows only flashes the taskbar button. Declared here rather than in a shell .pro
# because the dependency belongs to this module — a consumer should not have to know.
win32: LIBS += -luser32

SOURCES += \
    $$PWD/app_settings/app_settings.cpp \
    $$PWD/auth/access_control.cpp \
    $$PWD/auth/settings_admin_credential_provider.cpp \
    $$PWD/logger/app_logger.cpp \
    $$PWD/utils/shell_handoff.cpp \
    $$PWD/utils/single_instance_guard.cpp \
    $$PWD/utils/theme_manager.cpp

HEADERS += \
    $$PWD/app_settings/app_settings.h \
    $$PWD/auth/access_control.h \
    $$PWD/auth/admin_credential_provider.h \
    $$PWD/auth/settings_admin_credential_provider.h \
    $$PWD/app_version.h \
    $$PWD/logger/app_logger.h \
    $$PWD/qgadget_macro.h \
    $$PWD/setting_keys.h \
    $$PWD/utils/meta_utils.h \
    $$PWD/utils/shell_handoff.h \
    $$PWD/utils/single_instance_guard.h \
    $$PWD/utils/theme_manager.h \
    $$PWD/utils/windows_helper.h
