QT       += core testlib
QT       -= gui

CONFIG   += c++17 console testcase
CONFIG   -= app_bundle

TARGET    = calibration_test

# Paths back to the main project source tree.
ROOT_DIR  = $$PWD/../..

INCLUDEPATH += $$ROOT_DIR/src

SOURCES += \
    tst_main.cpp \
    $$ROOT_DIR/src/calibration/calibration_board.cpp \
    $$ROOT_DIR/src/calibration/fanuc_irvision_board.cpp \
    $$ROOT_DIR/src/calibration/calibration_board_factory.cpp \
    $$ROOT_DIR/src/calibration/calibrator.cpp

HEADERS += \
    $$ROOT_DIR/src/calibration/calibration_board.h \
    $$ROOT_DIR/src/calibration/fanuc_irvision_board.h \
    $$ROOT_DIR/src/calibration/calibration_board_factory.h \
    $$ROOT_DIR/src/calibration/calibrator.h

include($$ROOT_DIR/qmake/opencv_dependency.pri)
