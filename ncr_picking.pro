QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

CONFIG += c++17
CONFIG += deploy_deps

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Module code resolves includes rooted at src/ (e.g. #include "model/project.h").
INCLUDEPATH += \
    src/

# One .pri per module: each module lists only its own SOURCES/HEADERS/FORMS,
# so adding a file touches that module's .pri, never this root file.
# Dependency levels (low to high) — lower levels must not include higher ones;
# see README.md "Module dependency rule".
include(src/core/core.pri)                  # level 0: settings/logger/utils
include(src/device/device.pri)              # level 1: device families
include(src/calibration/calibration.pri)    # level 1: calibration
include(src/matching/matching.pri)          # level 1: pattern matching
include(src/model/model.pri)                # level 2: project/task/pipeline
include(src/runtime/runtime.pri)            # level 2: per-device runners
include(src/form/form.pri)                  # UI: dialogs/wizards/pages
include(src/widgets/widgets.pri)            # UI: reusable widgets
include(src/libwg/libwg.pri)                # UI: widget primitives
include(app/app.pri)                        # app shell (top level, owns translations)

# Robot kinematics component (forward / inverse kinematics + optional Coal
# mesh-collision). Reusable library under components/RobotKinematics/, consumed
# here as source via its integration .pri. It pulls in the header-only Eigen
# and the prebuilt Coal/Boost/Assimp install trees under the repo-root 3rdparty
# folder. The .pri auto-copies the mesh-collision runtime DLL set and the
# Nachi MZ04D mesh assets next to the built binary after link. Customer installer
# packaging is still tracked in docs/backlog/later_todo_list.md #27.
include(components/RobotKinematics/robotkinematics.pri)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

include(qmake/local_dependencies.pri)

# Opt-in dependency deployment: copy third-party runtime DLLs (ADS docking,
# OpenCV world, Basler Pylon) next to the built binary when the target dir is
# missing them, and run windeployqt for the Qt runtime + plugins. Off by
# default; enable with `qmake ... CONFIG+=deploy_deps`. RobotKinematics/Coal is
# deployed by robotkinematics.pri.
include(qmake/deploy_dependencies.pri)

RESOURCES += \
    3rdparty/advance_docking/include/ads.qrc \
    resrc.qrc

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/3rdparty/advance_docking/lib/ -lqtadvanceddocking
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/3rdparty/advance_docking/lib/ -lqtadvanceddockingd

INCLUDEPATH += $$PWD/3rdparty/advance_docking/include
DEPENDPATH += $$PWD/3rdparty/advance_docking/include

# Vendored Qt Solutions property browser (compiled into the app). Headers are
# included as "qtpropertybrowser/<name>" via the 3rdparty include root below.
include(3rdparty/qtpropertybrowser/qtpropertybrowser_vendor.pri)
INCLUDEPATH += $$PWD/3rdparty

RC_ICONS = resrc/icon/software_icon.ico
