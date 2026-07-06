# UI module (dependency top of src/, below app): the entire user interface.
# Merged from the former form/, widgets/, and libwg/ modules.
#   forms/    - dialogs, wizards, device/task configuration pages (with
#               camera/, pattern/, plc/, task/, vision_output/ subgroups)
#   widgets/  - reusable custom widgets (vision/, image_widget/,
#               property_browser/, plc_widget/, calibration/), plus
#               controls/ (small composed status/connection controls, ex
#               form/widgets) and generic primitives (ex libwg).
# May depend on: every non-UI module. Must NOT be included by non-UI modules
# (enforced by tests/architecture_contract_test). Add new files here only.
SOURCES += \
    $$PWD/forms/add_device_wizard.cpp \
    $$PWD/forms/camera/basler_cam_select_dialog.cpp \
    $$PWD/forms/camera/basler_camera_widget.cpp \
    $$PWD/forms/device_widget_factory.cpp \
    $$PWD/forms/new_project_dialog.cpp \
    $$PWD/forms/new_task_dialog.cpp \
    $$PWD/forms/pattern/add_pattern_image_dialog.cpp \
    $$PWD/forms/pattern/add_pattern_wizard.cpp \
    $$PWD/forms/pattern/edit_pattern_wizard.cpp \
    $$PWD/forms/pattern/pattern_canvas.cpp \
    $$PWD/forms/plc/mitsubishi_mc_device_widget.cpp \
    $$PWD/forms/plc/plc_mitsu_device_wizard.cpp \
    $$PWD/forms/project_infor_setting.cpp \
    $$PWD/forms/task/localization_dashboard_widget.cpp \
    $$PWD/forms/task/localization_patterns_widget.cpp \
    $$PWD/forms/task/localization_setting_widget.cpp \
    $$PWD/forms/task/localization_task_widget.cpp \
    $$PWD/forms/task/workspace_setting_dialog.cpp \
    $$PWD/forms/vision_output/robot_kinematic_check_widget.cpp \
    $$PWD/forms/vision_output/vision_tcpip_client_device_widget.cpp \
    $$PWD/forms/vision_output/vision_tcpip_device_widget.cpp \
    $$PWD/widgets/calibration/calibration_board_dialog.cpp \
    $$PWD/widgets/calibration/calibration_points_table.cpp \
    $$PWD/widgets/calibration/calibration_threshold_dialog.cpp \
    $$PWD/widgets/camera_mapping_widget.cpp \
    $$PWD/widgets/camera_workspace_widget.cpp \
    $$PWD/widgets/clamp.cpp \
    $$PWD/widgets/controls/compact_combo_box.cpp \
    $$PWD/widgets/controls/connection_action_button.cpp \
    $$PWD/widgets/controls/connection_state_dot.cpp \
    $$PWD/widgets/controls/connection_state_label.cpp \
    $$PWD/widgets/controls/device_nav_dot.cpp \
    $$PWD/widgets/controls/device_nav_item_widget.cpp \
    $$PWD/widgets/controls/flat_list_widget.cpp \
    $$PWD/widgets/controls/frame_box.cpp \
    $$PWD/widgets/controls/send_state_hint_label.cpp \
    $$PWD/widgets/controls/state_pill_label.cpp \
    $$PWD/widgets/controls/status_lamp_dot.cpp \
    $$PWD/widgets/controls/status_lamp.cpp \
    $$PWD/widgets/controls/status_text_label.cpp \
    $$PWD/widgets/controls/type_chip_label.cpp \
    $$PWD/widgets/group_frame_widget.cpp \
    $$PWD/widgets/group_frame.cpp \
    $$PWD/widgets/image_widget/image_view_only.cpp \
    $$PWD/widgets/image_widget/image_widget.cpp \
    $$PWD/widgets/image_widget/item_gripper_box.cpp \
    $$PWD/widgets/image_widget/item_picking_pos.cpp \
    $$PWD/widgets/image_widget/item_roi_rotated.cpp \
    $$PWD/widgets/image_widget/item_roi.cpp \
    $$PWD/widgets/no_wheel_combobox.cpp \
    $$PWD/widgets/pattern_tree_widget.cpp \
    $$PWD/widgets/plc_widget/device_row_delegate.cpp \
    $$PWD/widgets/plc_widget/devices_monitor_widget.cpp \
    $$PWD/widgets/project_tree_widget.cpp \
    $$PWD/widgets/property_browser/custom_property_managers.cpp \
    $$PWD/widgets/property_browser/match_config_property_adapter.cpp \
    $$PWD/widgets/property_browser/property_browser_widget.cpp \
    $$PWD/widgets/signals_map_widget.cpp \
    $$PWD/widgets/signals_monitor_widget.cpp \
    $$PWD/widgets/task_event_log_widget.cpp \
    $$PWD/widgets/vision/vision_canvas.cpp \
    $$PWD/widgets/vision/vision_geometry.cpp \
    $$PWD/widgets/vision/vision_numeric_inspector.cpp \
    $$PWD/widgets/vision/vision_result_adapter.cpp \
    $$PWD/widgets/vision/vision_result_viewer_widget.cpp \
    $$PWD/widgets/vision/vision_roi_config_adapter.cpp \
    $$PWD/widgets/vision/vision_roi_editor_widget.cpp \
    $$PWD/widgets/vision/vision_tool_palette.cpp

HEADERS += \
    $$PWD/forms/add_device_wizard.h \
    $$PWD/forms/camera/basler_cam_select_dialog.h \
    $$PWD/forms/camera/basler_camera_widget.h \
    $$PWD/forms/device_widget_factory.h \
    $$PWD/forms/device_widget.h \
    $$PWD/forms/new_project_dialog.h \
    $$PWD/forms/new_task_dialog.h \
    $$PWD/forms/pattern/add_pattern_image_dialog.h \
    $$PWD/forms/pattern/add_pattern_wizard.h \
    $$PWD/forms/pattern/edit_pattern_wizard.h \
    $$PWD/forms/pattern/pattern_canvas.h \
    $$PWD/forms/pattern/pattern_manager_dialog.h \
    $$PWD/forms/pattern/pattern_theme.h \
    $$PWD/forms/plc/mitsubishi_mc_device_widget.h \
    $$PWD/forms/plc/plc_mitsu_device_wizard.h \
    $$PWD/forms/project_infor_setting.h \
    $$PWD/forms/task_widget.h \
    $$PWD/forms/task/localization_dashboard_widget.h \
    $$PWD/forms/task/localization_patterns_widget.h \
    $$PWD/forms/task/localization_setting_widget.h \
    $$PWD/forms/task/localization_task_widget.h \
    $$PWD/forms/task/workspace_setting_dialog.h \
    $$PWD/forms/vision_output/robot_kinematic_check_widget.h \
    $$PWD/forms/vision_output/vision_tcpip_client_device_widget.h \
    $$PWD/forms/vision_output/vision_tcpip_device_widget.h \
    $$PWD/widgets/calibration/calibration_board_dialog.h \
    $$PWD/widgets/calibration/calibration_points_table.h \
    $$PWD/widgets/calibration/calibration_threshold_dialog.h \
    $$PWD/widgets/camera_mapping_widget.h \
    $$PWD/widgets/camera_workspace_widget.h \
    $$PWD/widgets/clamp.h \
    $$PWD/widgets/controls/compact_combo_box.h \
    $$PWD/widgets/controls/connection_action_button.h \
    $$PWD/widgets/controls/connection_state_dot.h \
    $$PWD/widgets/controls/connection_state_label.h \
    $$PWD/widgets/controls/device_nav_dot.h \
    $$PWD/widgets/controls/device_nav_item_widget.h \
    $$PWD/widgets/controls/flat_list_widget.h \
    $$PWD/widgets/controls/frame_box.h \
    $$PWD/widgets/controls/send_state_hint_label.h \
    $$PWD/widgets/controls/state_pill_label.h \
    $$PWD/widgets/controls/status_lamp_dot.h \
    $$PWD/widgets/controls/status_lamp.h \
    $$PWD/widgets/controls/status_text_label.h \
    $$PWD/widgets/controls/type_chip_label.h \
    $$PWD/widgets/group_frame_widget.h \
    $$PWD/widgets/group_frame.h \
    $$PWD/widgets/image_widget/image_view_only.h \
    $$PWD/widgets/image_widget/image_widget.h \
    $$PWD/widgets/image_widget/item_gripper_box.h \
    $$PWD/widgets/image_widget/item_picking_pos.h \
    $$PWD/widgets/image_widget/item_pixmap_bounding.h \
    $$PWD/widgets/image_widget/item_roi_rotated.h \
    $$PWD/widgets/image_widget/item_roi.h \
    $$PWD/widgets/lamp_button.h \
    $$PWD/widgets/no_wheel_combobox.h \
    $$PWD/widgets/no_wheel_double_spinbox.h \
    $$PWD/widgets/no_wheel_spinbox.h \
    $$PWD/widgets/pattern_tree_widget.h \
    $$PWD/widgets/plc_widget/device_row_delegate.h \
    $$PWD/widgets/plc_widget/devices_monitor_widget.h \
    $$PWD/widgets/project_tree_widget.h \
    $$PWD/widgets/property_browser/custom_property_managers.h \
    $$PWD/widgets/property_browser/match_config_property_adapter.h \
    $$PWD/widgets/property_browser/prop_spec.h \
    $$PWD/widgets/property_browser/property_browser_widget.h \
    $$PWD/widgets/signals_map_widget.h \
    $$PWD/widgets/signals_monitor_widget.h \
    $$PWD/widgets/task_event_log_widget.h \
    $$PWD/widgets/validating_line_edit.h \
    $$PWD/widgets/vision/vision_canvas.h \
    $$PWD/widgets/vision/vision_geometry.h \
    $$PWD/widgets/vision/vision_numeric_inspector.h \
    $$PWD/widgets/vision/vision_overlay_types.h \
    $$PWD/widgets/vision/vision_result_adapter.h \
    $$PWD/widgets/vision/vision_result_viewer_widget.h \
    $$PWD/widgets/vision/vision_roi_config_adapter.h \
    $$PWD/widgets/vision/vision_roi_editor_widget.h \
    $$PWD/widgets/vision/vision_tool_palette.h

FORMS += \
    $$PWD/forms/add_device_wizard.ui \
    $$PWD/forms/camera/basler_cam_select_dialog.ui \
    $$PWD/forms/camera/basler_camera_widget.ui \
    $$PWD/forms/new_project_dialog.ui \
    $$PWD/forms/new_task_dialog.ui \
    $$PWD/forms/pattern/add_pattern_image_dialog.ui \
    $$PWD/forms/plc/mitsubishi_mc_device_widget.ui \
    $$PWD/forms/plc/plc_mitsu_device_wizard.ui \
    $$PWD/forms/project_infor_setting.ui \
    $$PWD/forms/task/localization_dashboard_widget.ui \
    $$PWD/forms/task/localization_patterns_widget.ui \
    $$PWD/forms/task/localization_setting_widget.ui \
    $$PWD/forms/task/localization_task_widget.ui \
    $$PWD/forms/task/workspace_setting_dialog.ui \
    $$PWD/forms/vision_output/robot_kinematic_check_widget.ui \
    $$PWD/forms/vision_output/vision_tcpip_client_device_widget.ui \
    $$PWD/forms/vision_output/vision_tcpip_device_widget.ui
