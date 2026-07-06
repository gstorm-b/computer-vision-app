QT += widgets network

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    vision_virtual_client.cpp

HEADERS += \
    mainwindow.h \
    vision_virtual_client.h

FORMS += \
    mainwindow.ui

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
