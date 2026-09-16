# serialbus: QModbusTcpClient/QModbusTcpServer, both the devices under test and the fixtures
# on the other end of the link. network: IDevice's transport headers reach QTcpSocket.
QT       += core network serialbus testlib
QT       -= gui

CONFIG   += console c++17 testcase
CONFIG   -= app_bundle

TEMPLATE  = app
TARGET    = modbus_device_test

# Points back at the main project's sources: the devices under test are the SHIPPED ones,
# compiled from src/. A test that built its own copy would prove nothing about the product.
#
# WARNING: this list is maintained by hand and nothing checks it. A new .cpp under
# src/device/plc/modbus/ that this file does not name links as an unresolved external, which is
# the good case; a source that quietly stops being needed here is the bad one. See
# src/core/AGENTS.md on the same hazard in the architecture contract test.
ROOT_DIR  = $$PWD/../..

INCLUDEPATH += $$ROOT_DIR/src

SOURCES += \
    main.cpp \
    $$ROOT_DIR/src/device/plc/modbus/modbus_register_map.cpp \
    $$ROOT_DIR/src/device/plc/modbus/modbus_result_layout.cpp \
    $$ROOT_DIR/src/device/plc/modbus/modbus_trace.cpp \
    $$ROOT_DIR/src/device/plc/modbus/modbus_tcp_client_device.cpp \
    $$ROOT_DIR/src/device/plc/modbus/modbus_tcp_server_device.cpp \
    $$ROOT_DIR/src/core/logger/app_logger.cpp

HEADERS += \
    $$ROOT_DIR/src/device/device_capabilities.h \
    $$ROOT_DIR/src/device/idevice.h \
    $$ROOT_DIR/src/device/idevice_config.h \
    $$ROOT_DIR/src/device/irequest.h \
    $$ROOT_DIR/src/device/robot_kinematic_check_config.h \
    $$ROOT_DIR/src/device/output_device/vision_output_request.h \
    $$ROOT_DIR/src/device/plc/plc_device.h \
    $$ROOT_DIR/src/device/plc/modbus/modbus_config.h \
    $$ROOT_DIR/src/device/plc/modbus/modbus_register_map.h \
    $$ROOT_DIR/src/device/plc/modbus/modbus_result_layout.h \
    $$ROOT_DIR/src/device/plc/modbus/modbus_trace.h \
    $$ROOT_DIR/src/device/plc/modbus/modbus_tcp_client_config.h \
    $$ROOT_DIR/src/device/plc/modbus/modbus_tcp_client_device.h \
    $$ROOT_DIR/src/device/plc/modbus/modbus_tcp_server_config.h \
    $$ROOT_DIR/src/device/plc/modbus/modbus_tcp_server_device.h \
    $$ROOT_DIR/src/core/logger/app_logger.h \
    $$ROOT_DIR/src/core/qgadget_macro.h \
    $$ROOT_DIR/src/core/utils/meta_utils.h
