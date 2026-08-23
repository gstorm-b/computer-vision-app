# Operator runtime application shell (`ncr_runtime.exe`).
#
# Peer of the repo-root ncr_picking.pro: both link the same ncr_shared static library
# through qmake/app_common.pri and differ only in which shell they add. See
# docs/domains/runtime_app/runtime_shell.md for what this executable is for.
#
# Like its peer, this .pro links the library but cannot build it — a standalone build needs
# src/src.pro built first. ncr_picking_all.pro does that for you.
#
# Build output belongs in build/runtime_build/<build-name> — one subfolder per target
# under the repo-root build/. Never share a build directory with the other shell: two
# qmake projects collide on Makefile, .qmake.stash and the generated moc/ui files. See
# docs/rules/build_and_verification.md.

include($$PWD/../qmake/app_common.pri)

include($$PWD/runtime_app.pri)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RC_ICONS = $$PWD/../resrc/icon/software_icon.ico
