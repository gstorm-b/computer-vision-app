# Basler Pylon qmake dependency.
#
# Set paths with environment variables or qmake command-line variables:
#   PYLON_INCLUDE_DIR, PYLON_LIB_DIR
#   PYLON_INCLUDE_DIR=C:\Program Files\Basler\pylon\Development\include
#   PYLON_LIB_DIR=C:\Program Files\Basler\pylon\Development\lib\x64
# Optional library-name override:
#   PYLON_BASE_LIB
#   PYLON_BASE_LIB=PylonBase_v11

isEmpty(PYLON_INCLUDE_DIR): PYLON_INCLUDE_DIR = $$(PYLON_INCLUDE_DIR)
isEmpty(PYLON_LIB_DIR):     PYLON_LIB_DIR     = $$(PYLON_LIB_DIR)
isEmpty(PYLON_BASE_LIB):    PYLON_BASE_LIB    = $$(PYLON_BASE_LIB)

# Machine-local fallback — same file and same reasoning as opencv_dependency.pri. It
# carries both dependency families, and every assignment in it is isEmpty()-guarded, so
# being included from both places is harmless.
exists($$PWD/local_paths.pri): include($$PWD/local_paths.pri)

isEmpty(PYLON_INCLUDE_DIR) {
    error("PYLON_INCLUDE_DIR must point to the Basler Pylon include directory")
}
isEmpty(PYLON_LIB_DIR) {
    error("PYLON_LIB_DIR must point to the Basler Pylon library directory")
}

isEmpty(PYLON_BASE_LIB): PYLON_BASE_LIB = PylonBase_v11

INCLUDEPATH += $$PYLON_INCLUDE_DIR
DEPENDPATH  += $$PYLON_INCLUDE_DIR

win32: LIBS += -L$$PYLON_LIB_DIR -l$$PYLON_BASE_LIB
