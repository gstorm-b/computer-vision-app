# Calibration module (dependency level 1): calibration boards, calibrator,
# board factory. May depend on: core. Must not include UI.
# Standalone tests live in src/calibration/tests/ (own .pro).

SOURCES += \
    $$PWD/calibration_board.cpp \
    $$PWD/calibration_board_factory.cpp \
    $$PWD/calibrator.cpp \
    $$PWD/fanuc_irvision_board.cpp

HEADERS += \
    $$PWD/calibration_board.h \
    $$PWD/calibration_board_factory.h \
    $$PWD/calibrator.h \
    $$PWD/fanuc_irvision_board.h
