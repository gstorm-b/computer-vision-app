#-------------------------------------------------
# Modbus TCP Server demo - qmake project
#
# Open this file directly in Qt Creator (File > Open File or Project),
# pick a Qt 6 desktop kit and press Ctrl+R.
#-------------------------------------------------

# Qt 6 only: QModbusDataUnit is QList<quint16> based since Qt 6, and the code
# uses the single-overload QComboBox::currentIndexChanged and qsizetype.
lessThan(QT_MAJOR_VERSION, 6): error("This project requires Qt 6 (tested with Qt 6.11).")

!qtHaveModule(serialbus) {
    error("The Qt SerialBus module is missing. Install \"Qt Serial Bus\" with the Qt Maintenance Tool.")
}

QT += core gui widgets network serialbus

TEMPLATE = app
TARGET   = ModbusTcpServer

CONFIG += c++17

# MSVC: make sure UTF-8 sources are decoded correctly.
msvc: QMAKE_CXXFLAGS += /utf-8

SOURCES += \
    datacodec.cpp \
    loggingmodbusserver.cpp \
    main.cpp \
    mainwindow.cpp \
    modbusrunner.cpp \
    rowhoverdelegate.cpp

HEADERS += \
    datacodec.h \
    loggingmodbusserver.h \
    mainwindow.h \
    modbusdefs.h \
    modbusrunner.h \
    rowhoverdelegate.h

FORMS += \
    mainwindow.ui

# Default deployment rules.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
