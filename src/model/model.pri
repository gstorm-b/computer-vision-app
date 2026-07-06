# Model module (dependency level 2): project/task persistence, task
# lifecycle, localization pipeline and runtime controller.
# May depend on: core, device, matching, calibration, components.
# Must not include UI (ui/).

SOURCES += \
    $$PWD/itask.cpp \
    $$PWD/localization_pipeline.cpp \
    $$PWD/localization_runtime_controller.cpp \
    $$PWD/localization_signal_mapper.cpp \
    $$PWD/project.cpp \
    $$PWD/project_repository.cpp \
    $$PWD/robot_kinematic_picking_checker.cpp \
    $$PWD/task_factory.cpp \
    $$PWD/task_localization.cpp

HEADERS += \
    $$PWD/camera_map_entry.h \
    $$PWD/isignal_group.h \
    $$PWD/itask.h \
    $$PWD/itask_config.h \
    $$PWD/localization_fault_code.h \
    $$PWD/localization_pipeline.h \
    $$PWD/localization_recovery_policy.h \
    $$PWD/localization_runtime_controller.h \
    $$PWD/localization_signal_mapper.h \
    $$PWD/task_state_machine.h \
    $$PWD/pick_and_place_task.h \
    $$PWD/project.h \
    $$PWD/project_repository.h \
    $$PWD/robot_kinematic_picking_checker.h \
    $$PWD/camera_workspace.h \
    $$PWD/task_device_binding.h \
    $$PWD/task_define.h \
    $$PWD/task_factory.h \
    $$PWD/task_localization.h \
    $$PWD/task_localization_config.h
