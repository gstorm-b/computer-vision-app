# network: McContext includes the TCP transport header (QTcpSocket) for every frame type,
# including the serial ones. serialport: the computer-link transport the 1C/3C contexts carry.
QT       += core network serialport testlib
QT       -= gui

CONFIG   += console c++17 testcase
CONFIG   -= app_bundle

TEMPLATE  = app
TARGET    = mc_frame_test

# Points back at the main project's sources: the codecs under test are the SHIPPED ones,
# compiled from src/. A test that built its own copy of a frame codec would prove nothing
# about the frames the product puts on the wire.
ROOT_DIR  = $$PWD/../..

INCLUDEPATH += $$ROOT_DIR/src

SOURCES += \
    main.cpp \
    $$ROOT_DIR/src/device/plc/mc_device_map.cpp \
    $$ROOT_DIR/src/device/plc/mc_fame_3e.cpp \
    $$ROOT_DIR/src/device/plc/mc_frame_1c.cpp \
    $$ROOT_DIR/src/device/plc/mc_frame_3c.cpp \
    $$ROOT_DIR/src/device/plc/mc_protocol_device.cpp \
    $$ROOT_DIR/src/core/logger/app_logger.cpp

HEADERS += \
    $$ROOT_DIR/src/device/idevice.h \
    $$ROOT_DIR/src/device/idevice_config.h \
    $$ROOT_DIR/src/device/irequest.h \
    $$ROOT_DIR/src/device/plc/mc_ascii_utils.h \
    $$ROOT_DIR/src/device/plc/mc_context.h \
    $$ROOT_DIR/src/device/plc/mc_context_1c.h \
    $$ROOT_DIR/src/device/plc/mc_context_3c.h \
    $$ROOT_DIR/src/device/plc/mc_context_3e.h \
    $$ROOT_DIR/src/device/plc/mc_define.h \
    $$ROOT_DIR/src/device/plc/mc_device_map.h \
    $$ROOT_DIR/src/device/plc/mc_device_map_diff.h \
    $$ROOT_DIR/src/device/plc/mc_fame_3e.h \
    $$ROOT_DIR/src/device/plc/mc_frame_1c.h \
    $$ROOT_DIR/src/device/plc/mc_frame_3c.h \
    $$ROOT_DIR/src/device/plc/mc_frame_abstract.h \
    $$ROOT_DIR/src/device/plc/mc_msg_interface.h \
    $$ROOT_DIR/src/device/plc/mc_msg_serial_port.h \
    $$ROOT_DIR/src/device/plc/mc_msg_tcp_client.h \
    $$ROOT_DIR/src/device/plc/mc_protocol_config.h \
    $$ROOT_DIR/src/device/plc/mc_protocol_device.h \
    $$ROOT_DIR/src/device/plc/mc_request.h \
    $$ROOT_DIR/src/device/plc/memory_utils.h \
    $$ROOT_DIR/src/device/plc/plc_device.h \
    $$ROOT_DIR/src/core/logger/app_logger.h \
    $$ROOT_DIR/src/core/qgadget_macro.h \
    $$ROOT_DIR/src/core/utils/meta_utils.h
