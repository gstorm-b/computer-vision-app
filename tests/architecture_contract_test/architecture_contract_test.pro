QT       += core gui widgets network sql testlib

CONFIG   += console c++17 testcase
CONFIG   -= app_bundle

TEMPLATE  = app
TARGET    = architecture_contract_test

ROOT_DIR  = $$PWD/../..

# Absolute repo root for the include-layering contract scan.
DEFINES += NCR_REPO_ROOT=\\\"$$clean_path($$ROOT_DIR)\\\"

INCLUDEPATH += $$ROOT_DIR/src
MOC_DIR = $$OUT_PWD/debug

include($$ROOT_DIR/components/RobotKinematics/robotkinematics.pri)

SOURCES += \
    main.cpp \
    $$ROOT_DIR/src/core/app_settings/app_settings.cpp \
    $$ROOT_DIR/src/calibration/calibration_board.cpp \
    $$ROOT_DIR/src/calibration/calibration_board_factory.cpp \
    $$ROOT_DIR/src/calibration/calibrator.cpp \
    $$ROOT_DIR/src/calibration/fanuc_irvision_board.cpp \
    $$ROOT_DIR/src/device/camera/camera_basler_gige.cpp \
    $$ROOT_DIR/src/device/device_factory.cpp \
    $$ROOT_DIR/src/device/device_manager.cpp \
    $$ROOT_DIR/src/device/device_registry.cpp \
    $$ROOT_DIR/src/device/plc/mc_device_map.cpp \
    $$ROOT_DIR/src/device/plc/mc_fame_3e.cpp \
    $$ROOT_DIR/src/device/plc/mc_protocol_device.cpp \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_device_base.cpp \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_device.cpp \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_client_device.cpp \
    $$ROOT_DIR/src/device/robot/kawasaki_robot_device.cpp \
    $$ROOT_DIR/src/device/robot/nachi_robot_device.cpp \
    $$ROOT_DIR/src/core/logger/app_logger.cpp \
    $$ROOT_DIR/src/matching/image_matcher.cpp \
    $$ROOT_DIR/src/matching/imatch_type_config.cpp \
    $$ROOT_DIR/src/matching/match_group.cpp \
    $$ROOT_DIR/src/matching/match_params.cpp \
    $$ROOT_DIR/src/matching/match_pattern.cpp \
    $$ROOT_DIR/src/matching/match_pattern_config.cpp \
    $$ROOT_DIR/src/matching/pattern_group_manager.cpp \
    $$ROOT_DIR/src/matching/utils_block_max.cpp \
    $$ROOT_DIR/src/matching/vision_utils.cpp \
    $$ROOT_DIR/src/model/itask.cpp \
    $$ROOT_DIR/src/model/localization_pipeline.cpp \
    $$ROOT_DIR/src/model/localization_runtime_controller.cpp \
    $$ROOT_DIR/src/model/localization_signal_mapper.cpp \
    $$ROOT_DIR/src/model/project.cpp \
    $$ROOT_DIR/src/model/robot_kinematic_picking_checker.cpp \
    $$ROOT_DIR/src/model/task_factory.cpp \
    $$ROOT_DIR/src/model/task_localization.cpp \
    $$ROOT_DIR/src/runtime/task_runner.cpp \
    $$ROOT_DIR/src/core/utils/theme_manager.cpp \
    $$ROOT_DIR/src/ui/widgets/vision/vision_geometry.cpp \
    $$ROOT_DIR/src/ui/widgets/vision/vision_result_adapter.cpp

HEADERS += \
    $$ROOT_DIR/src/core/app_settings/app_settings.h \
    $$ROOT_DIR/src/calibration/calibration_board.h \
    $$ROOT_DIR/src/calibration/calibration_board_factory.h \
    $$ROOT_DIR/src/calibration/calibrator.h \
    $$ROOT_DIR/src/calibration/fanuc_irvision_board.h \
    $$ROOT_DIR/src/device/camera/basler_define.h \
    $$ROOT_DIR/src/device/camera/camera_basler_gige.h \
    $$ROOT_DIR/src/device/camera/camera_device.h \
    $$ROOT_DIR/src/device/device_capabilities.h \
    $$ROOT_DIR/src/device/device_factory.h \
    $$ROOT_DIR/src/device/device_manager.h \
    $$ROOT_DIR/src/device/device_registry.h \
    $$ROOT_DIR/src/device/idevice.h \
    $$ROOT_DIR/src/device/idevice_config.h \
    $$ROOT_DIR/src/device/irequest.h \
    $$ROOT_DIR/src/device/output_device/vision_output_config.h \
    $$ROOT_DIR/src/device/output_device/vision_output_device.h \
    $$ROOT_DIR/src/device/output_device/vision_output_request.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_protocol.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_device_base.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_config.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_device.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_client_config.h \
    $$ROOT_DIR/src/device/output_device/vision_tcpip_client_device.h \
    $$ROOT_DIR/src/device/plc/mc_context.h \
    $$ROOT_DIR/src/device/plc/mc_context_3e.h \
    $$ROOT_DIR/src/device/plc/mc_context_factory.h \
    $$ROOT_DIR/src/device/plc/mc_define.h \
    $$ROOT_DIR/src/device/plc/mc_device_map.h \
    $$ROOT_DIR/src/device/plc/mc_fame_3e.h \
    $$ROOT_DIR/src/device/plc/mc_frame_abstract.h \
    $$ROOT_DIR/src/device/plc/mc_msg_interface.h \
    $$ROOT_DIR/src/device/plc/mc_msg_tcp_client.h \
    $$ROOT_DIR/src/device/plc/mc_protocol_config.h \
    $$ROOT_DIR/src/device/plc/mc_protocol_device.h \
    $$ROOT_DIR/src/device/plc/mc_request.h \
    $$ROOT_DIR/src/device/plc/memory_utils.h \
    $$ROOT_DIR/src/device/plc/plc_device.h \
    $$ROOT_DIR/src/device/plc/plc_value.h \
    $$ROOT_DIR/src/device/robot/kawasaki_robot_config.h \
    $$ROOT_DIR/src/device/robot/kawasaki_robot_device.h \
    $$ROOT_DIR/src/device/robot/nachi_robot_config.h \
    $$ROOT_DIR/src/device/robot/nachi_robot_device.h \
    $$ROOT_DIR/src/device/robot/robot_device.h \
    $$ROOT_DIR/src/core/logger/app_logger.h \
    $$ROOT_DIR/src/matching/edge_match_config.h \
    $$ROOT_DIR/src/matching/image_matcher.h \
    $$ROOT_DIR/src/matching/imatch_type_config.h \
    $$ROOT_DIR/src/matching/manager_result.h \
    $$ROOT_DIR/src/matching/match_box_gripper.h \
    $$ROOT_DIR/src/matching/match_group.h \
    $$ROOT_DIR/src/matching/match_object.h \
    $$ROOT_DIR/src/matching/match_params.h \
    $$ROOT_DIR/src/matching/match_pattern.h \
    $$ROOT_DIR/src/matching/match_pattern_config.h \
    $$ROOT_DIR/src/matching/match_pattern_layer.h \
    $$ROOT_DIR/src/matching/matching_types.h \
    $$ROOT_DIR/src/matching/pattern_group_manager.h \
    $$ROOT_DIR/src/matching/utils_block_max.h \
    $$ROOT_DIR/src/matching/vision_utils.h \
    $$ROOT_DIR/src/model/camera_map_entry.h \
    $$ROOT_DIR/src/model/camera_workspace.h \
    $$ROOT_DIR/src/model/itask.h \
    $$ROOT_DIR/src/model/itask_config.h \
    $$ROOT_DIR/src/model/localization_fault_code.h \
    $$ROOT_DIR/src/model/localization_pipeline.h \
    $$ROOT_DIR/src/model/localization_recovery_policy.h \
    $$ROOT_DIR/src/model/localization_runtime_controller.h \
    $$ROOT_DIR/src/model/localization_signal_mapper.h \
    $$ROOT_DIR/src/model/project.h \
    $$ROOT_DIR/src/model/robot_kinematic_picking_checker.h \
    $$ROOT_DIR/src/model/task_state_machine.h \
    $$ROOT_DIR/src/model/task_device_binding.h \
    $$ROOT_DIR/src/model/task_define.h \
    $$ROOT_DIR/src/model/task_factory.h \
    $$ROOT_DIR/src/model/task_localization.h \
    $$ROOT_DIR/src/model/task_localization_config.h \
    $$ROOT_DIR/src/core/qgadget_macro.h \
    $$ROOT_DIR/src/runtime/device_command.h \
    $$ROOT_DIR/src/runtime/device_command_queue.h \
    $$ROOT_DIR/src/runtime/camera_runner.h \
    $$ROOT_DIR/src/runtime/device_runner.h \
    $$ROOT_DIR/src/runtime/idevice_runner.h \
    $$ROOT_DIR/src/runtime/plc_runner.h \
    $$ROOT_DIR/src/runtime/task_runner.h \
    $$ROOT_DIR/src/runtime/vision_output_runner.h \
    $$ROOT_DIR/src/core/utils/meta_utils.h \
    $$ROOT_DIR/src/core/utils/theme_manager.h \
    $$ROOT_DIR/src/ui/widgets/vision/vision_geometry.h \
    $$ROOT_DIR/src/ui/widgets/vision/vision_overlay_types.h \
    $$ROOT_DIR/src/ui/widgets/vision/vision_result_adapter.h

include($$ROOT_DIR/qmake/local_dependencies.pri)
