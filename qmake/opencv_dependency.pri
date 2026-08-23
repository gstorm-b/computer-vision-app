# OpenCV qmake dependency.
#
# Set paths with environment variables or qmake command-line variables:
#   OPENCV_INCLUDE_DIR, OPENCV_LIB_DIR
#   OPENCV_INCLUDE_DIR=C:\opencv\build\include
#   OPENCV_LIB_DIR=C:\opencv\build\x64\vc16\lib
# Optional library-name overrides:
#   OPENCV_WORLD_RELEASE, OPENCV_WORLD_DEBUG
#   OPENCV_WORLD_RELEASE=opencv_world4120
#   OPENCV_WORLD_DEBUG=opencv_world4120d

isEmpty(OPENCV_INCLUDE_DIR): OPENCV_INCLUDE_DIR = $$(OPENCV_INCLUDE_DIR)
isEmpty(OPENCV_LIB_DIR):     OPENCV_LIB_DIR     = $$(OPENCV_LIB_DIR)
isEmpty(OPENCV_WORLD_RELEASE): OPENCV_WORLD_RELEASE = $$(OPENCV_WORLD_RELEASE)
isEmpty(OPENCV_WORLD_DEBUG):   OPENCV_WORLD_DEBUG   = $$(OPENCV_WORLD_DEBUG)

# Machine-local fallback, read by qmake itself so a build from Qt Creator and a build from
# a terminal resolve the same install without either side exporting anything. Untracked;
# create it from qmake/local_paths.pri.example. Included HERE, after the environment reads
# above and before the checks below, so it fills gaps and never overrides a command-line
# or environment value.
exists($$PWD/local_paths.pri): include($$PWD/local_paths.pri)

isEmpty(OPENCV_INCLUDE_DIR) {
    error("OPENCV_INCLUDE_DIR must point to the OpenCV include directory")
}
isEmpty(OPENCV_LIB_DIR) {
    error("OPENCV_LIB_DIR must point to the OpenCV library directory")
}

isEmpty(OPENCV_WORLD_RELEASE): OPENCV_WORLD_RELEASE = opencv_world4120
isEmpty(OPENCV_WORLD_DEBUG):   OPENCV_WORLD_DEBUG   = opencv_world4120d

INCLUDEPATH += $$OPENCV_INCLUDE_DIR
DEPENDPATH  += $$OPENCV_INCLUDE_DIR

win32:CONFIG(release, debug|release): LIBS += -L$$OPENCV_LIB_DIR -l$$OPENCV_WORLD_RELEASE
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OPENCV_LIB_DIR -l$$OPENCV_WORLD_DEBUG
else:unix: LIBS += -L$$OPENCV_LIB_DIR -l$$OPENCV_WORLD_RELEASE
