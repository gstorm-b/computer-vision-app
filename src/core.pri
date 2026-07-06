# Core module (dependency level 0): app settings, logging, shared utils,
# gadget/meta helpers, settings keys. Must not include any other module.
# NOTE: a later restructure phase gathers these files under src/core/.

SOURCES += \
    $$PWD/app_settings/app_settings.cpp \
    $$PWD/logger/app_logger.cpp \
    $$PWD/utils/theme_manager.cpp

HEADERS += \
    $$PWD/app_settings/app_settings.h \
    $$PWD/logger/app_logger.h \
    $$PWD/qgadget_marco.h \
    $$PWD/setting_keys.h \
    $$PWD/utils/meta_utils.h \
    $$PWD/utils/theme_manager.h
