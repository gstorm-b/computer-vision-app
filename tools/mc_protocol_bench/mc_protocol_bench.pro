QT       += core gui widgets network serialport

CONFIG   += c++17
TEMPLATE  = app
TARGET    = mc_protocol_bench

# Points back at the main project's sources. The codecs and transports exercised here are the
# SHIPPED ones, compiled from src/ — a bench that measured its own private copy of a frame codec
# would produce numbers about code the product does not run.
ROOT_DIR  = $$PWD/../..

INCLUDEPATH += $$ROOT_DIR/src

# Its own build directory next to this .pro, never under the repository-root build/ (AGENT.md).
DESTDIR = $$PWD/build/bin

SOURCES += \
    main.cpp \
    bench_runner.cpp \
    gadget_table.cpp \
    mainwindow.cpp \
    $$ROOT_DIR/src/device/plc/mc_device_map.cpp \
    $$ROOT_DIR/src/device/plc/mc_fame_3e.cpp \
    $$ROOT_DIR/src/device/plc/mc_frame_1c.cpp \
    $$ROOT_DIR/src/device/plc/mc_frame_3c.cpp \
    $$ROOT_DIR/src/core/logger/app_logger.cpp

HEADERS += \
    bench_runner.h \
    gadget_table.h \
    mainwindow.h \
    $$ROOT_DIR/src/device/idevice.h \
    $$ROOT_DIR/src/device/idevice_config.h \
    $$ROOT_DIR/src/device/irequest.h \
    $$ROOT_DIR/src/device/plc/mc_ascii_utils.h \
    $$ROOT_DIR/src/device/plc/mc_context.h \
    $$ROOT_DIR/src/device/plc/mc_context_1c.h \
    $$ROOT_DIR/src/device/plc/mc_context_3c.h \
    $$ROOT_DIR/src/device/plc/mc_context_3e.h \
    $$ROOT_DIR/src/device/plc/mc_context_factory.h \
    $$ROOT_DIR/src/device/plc/mc_define.h \
    $$ROOT_DIR/src/device/plc/mc_device_map.h \
    $$ROOT_DIR/src/device/plc/mc_fame_3e.h \
    $$ROOT_DIR/src/device/plc/mc_frame_1c.h \
    $$ROOT_DIR/src/device/plc/mc_frame_3c.h \
    $$ROOT_DIR/src/device/plc/mc_frame_abstract.h \
    $$ROOT_DIR/src/device/plc/mc_msg_interface.h \
    $$ROOT_DIR/src/device/plc/mc_msg_serial_port.h \
    $$ROOT_DIR/src/device/plc/mc_msg_tcp_client.h \
    $$ROOT_DIR/src/device/plc/mc_request.h \
    $$ROOT_DIR/src/device/plc/memory_utils.h \
    $$ROOT_DIR/src/device/plc/plc_device.h \
    $$ROOT_DIR/src/core/logger/app_logger.h \
    $$ROOT_DIR/src/core/qgadget_macro.h \
    $$ROOT_DIR/src/core/utils/meta_utils.h

FORMS += \
    mainwindow.ui
