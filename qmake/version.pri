# Single source of the application version.
#
# Consumed two ways from this one definition:
#   - NCR_APP_VERSION_STR -> compiled into src/core/app_version.h, reported at startup and
#                           shown on the runtime shell's project-select page. Needed by
#                           EVERYTHING that compiles src/, so it is set here;
#   - VERSION            -> Windows file-resource version, readable from Explorer's
#                           Properties dialog without launching the executable. Set by
#                           qmake/app_common.pri instead, because only an executable has a
#                           file resource — a static library has nothing to carry it.
#
# Both shells ship in one folder and must come from the same build (see
# docs/product/install_image.md), so the value must not be written down twice.
#
# Bump this when producing an install image.

NCR_APP_VERSION = 0.6.0

DEFINES += NCR_APP_VERSION_STR=\\\"$$NCR_APP_VERSION\\\"
