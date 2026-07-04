QT       += core network testlib
QT       -= gui

CONFIG   += console c++17 testcase
CONFIG   -= app_bundle

TEMPLATE  = app
TARGET    = vision_output_device_test

# Đường dẫn về source của project chính.
ROOT_DIR  = $$PWD/../..

INCLUDEPATH += $$ROOT_DIR/src

# vision_tcpip_device_base.cpp performs the advisory robot-kinematics check, so
# the test must compile the RobotKinematics component in (same as the app and
# the architecture contract test).
include($$ROOT_DIR/components/RobotKinematics/robotkinematics.pri)

SOURCES += \
    main.cpp \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_device_base.cpp \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_device.cpp \
    $$ROOT_DIR/src/logger/app_logger.cpp

HEADERS += \
    $$ROOT_DIR/src/device/idevice.h \
    $$ROOT_DIR/src/device/idevice_config.h \
    $$ROOT_DIR/src/device/irequest.h \
    $$ROOT_DIR/src/device/output_device/vision_output_config.h \
    $$ROOT_DIR/src/device/output_device/vision_output_device.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_protocol.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_device_base.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_config.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_device.h \
    $$ROOT_DIR/src/device/output_device/vision_output_request.h \
    $$ROOT_DIR/src/logger/app_logger.h \
    $$ROOT_DIR/src/qgadget_marco.h \
    $$ROOT_DIR/src/utils/meta_utils.h
