# Commissioning application shell.
#
# Everything shared with the operator runtime shell (runtime_app/ncr_runtime.pro) lives in
# qmake/app_common.pri: the link against the ncr_shared static library that carries all of
# src/, Qt modules, RobotKinematics, ADS, resources and dependency deployment. Only this
# shell's own pieces belong below.
#
# This .pro still builds on its own, but it no longer builds src/ — it LINKS ncr_shared,
# and qmake has no way to build another project from an app template. So a standalone build
# needs the library built first (see docs/rules/build_and_verification.md). Building through
# ncr_picking_all.pro does that for you and is the normal path.

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

include(qmake/app_common.pri)

include(components/app/app.pri)             # app shell — commissioning (owns translations)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RC_ICONS = resrc/icon/software_icon.ico
