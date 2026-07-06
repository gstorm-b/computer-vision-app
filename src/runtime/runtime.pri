# Runtime module (dependency level 2): per-device runners and command
# queues. Cross-thread device access must go through these runners
# (CameraRunner, PlcRunner, VisionOutputRunner) — never call device
# methods directly across runtime threads.
# May depend on: core, device. Must not include UI.

SOURCES += \
    $$PWD/task_runner.cpp

HEADERS += \
    $$PWD/device_command.h \
    $$PWD/device_command_queue.h \
    $$PWD/idevice_runner.h \
    $$PWD/device_runner.h \
    $$PWD/camera_runner.h \
    $$PWD/plc_runner.h \
    $$PWD/task_runner.h \
    $$PWD/vision_output_runner.h
