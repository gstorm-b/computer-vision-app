# Hardware-in-the-loop test for the JAI GigE camera. Unlike every other test in tests/, this one
# needs a real camera on the network: without one it SKIPS rather than fails, so it is safe to run
# anywhere, but it only proves anything on a station with the camera attached.
#
# Point it at a camera with NCR_JAI_TEST_IP (default 192.168.0.70).
QT       += core gui network testlib
QT       -= widgets

CONFIG   += console c++17
CONFIG   -= app_bundle

# NOT `testcase`: a `make check` on a machine without the camera would run this, and while it
# skips cleanly, the several-second discovery timeout is not something to pay on every build.
TEMPLATE  = app
TARGET    = jai_camera_hardware_test

ROOT_DIR  = $$PWD/../..

INCLUDEPATH += $$ROOT_DIR/src

# The SHIPPED device, compiled from src/ — a test that built its own copy would prove nothing
# about the product. See the same warning in modbus_device_test.pro: this list is maintained by
# hand and nothing checks it.
SOURCES += \
    main.cpp \
    $$ROOT_DIR/src/calibration/calibration_board.cpp \
    $$ROOT_DIR/src/calibration/calibration_board_factory.cpp \
    $$ROOT_DIR/src/calibration/calibrator.cpp \
    $$ROOT_DIR/src/calibration/fanuc_irvision_board.cpp \
    $$ROOT_DIR/src/device/camera/camera_jai_gige.cpp \
    $$ROOT_DIR/src/device/camera/jai_runtime.cpp \
    $$ROOT_DIR/src/core/logger/app_logger.cpp

HEADERS += \
    $$ROOT_DIR/src/device/camera/camera_device.h \
    $$ROOT_DIR/src/device/camera/camera_jai_gige.h \
    $$ROOT_DIR/src/device/camera/jai_define.h \
    $$ROOT_DIR/src/device/camera/jai_runtime.h \
    $$ROOT_DIR/src/device/device_capabilities.h \
    $$ROOT_DIR/src/device/idevice.h \
    $$ROOT_DIR/src/device/idevice_config.h \
    $$ROOT_DIR/src/device/irequest.h \
    $$ROOT_DIR/src/core/logger/app_logger.h \
    $$ROOT_DIR/src/core/qgadget_macro.h \
    $$ROOT_DIR/src/core/utils/meta_utils.h

include($$ROOT_DIR/qmake/local_dependencies.pri)
