# Local dependency bundle for qmake projects that need OpenCV and the two camera SDKs.

include($$PWD/opencv_dependency.pri)
include($$PWD/pylon_dependency.pri)
# JAI / Pleora eBUS SDK — the GigE Vision stack behind the JAI camera device. A hard
# requirement, the same standing as Pylon: src/device/camera/ compiles against it, so a
# machine without it cannot build the library at all. It fails at qmake time with a message
# naming the SDK, not at link time with an unresolved Pv* symbol.
include($$PWD/ebus_dependency.pri)
