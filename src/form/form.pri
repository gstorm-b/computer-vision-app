# Form module (UI level): dialogs, wizards, device/task pages and their
# small composed widgets. UI structure belongs in .ui files, behavior in
# .cpp, styling in .qss (see docs/rules/ui_design_rules.md).
# May depend on: every non-UI module + widgets/libwg.

SOURCES += \
    $$PWD/add_device_wizard.cpp \
    $$PWD/camera/basler_cam_select_dialog.cpp \
    $$PWD/camera/basler_camera_widget.cpp \
    $$PWD/device_widget_factory.cpp \
    $$PWD/new_project_dialog.cpp \
    $$PWD/new_task_dialog.cpp \
    $$PWD/pattern/add_pattern_image_dialog.cpp \
    $$PWD/task/workspace_setting_dialog.cpp \
    $$PWD/pattern/pattern_canvas.cpp \
    $$PWD/pattern/add_pattern_wizard.cpp \
    $$PWD/pattern/edit_pattern_wizard.cpp \
    $$PWD/widgets/compact_combo_box.cpp \
    $$PWD/widgets/connection_action_button.cpp \
    $$PWD/widgets/connection_state_dot.cpp \
    $$PWD/widgets/connection_state_label.cpp \
    $$PWD/widgets/device_nav_dot.cpp \
    $$PWD/widgets/device_nav_item_widget.cpp \
    $$PWD/widgets/flat_list_widget.cpp \
    $$PWD/widgets/frame_box.cpp \
    $$PWD/plc/mitsubishi_mc_device_widget.cpp \
    $$PWD/plc/plc_mitsu_device_wizard.cpp \
    $$PWD/project_infor_setting.cpp \
    $$PWD/widgets/state_pill_label.cpp \
    $$PWD/widgets/status_lamp_dot.cpp \
    $$PWD/task/localization_dashboard_widget.cpp \
    $$PWD/task/localization_patterns_widget.cpp \
    $$PWD/task/localization_setting_widget.cpp \
    $$PWD/task/localization_task_widget.cpp \
    $$PWD/widgets/status_text_label.cpp \
    $$PWD/widgets/type_chip_label.cpp \
    $$PWD/vision_output/vision_tcpip_device_widget.cpp \
    $$PWD/vision_output/vision_tcpip_client_device_widget.cpp \
    $$PWD/vision_output/robot_kinematic_check_widget.cpp \
    $$PWD/widgets/send_state_hint_label.cpp \
    $$PWD/widgets/status_lamp.cpp

HEADERS += \
    $$PWD/add_device_wizard.h \
    $$PWD/camera/basler_cam_select_dialog.h \
    $$PWD/camera/basler_camera_widget.h \
    $$PWD/device_widget.h \
    $$PWD/device_widget_factory.h \
    $$PWD/new_project_dialog.h \
    $$PWD/new_task_dialog.h \
    $$PWD/pattern/add_pattern_image_dialog.h \
    $$PWD/task/workspace_setting_dialog.h \
    $$PWD/pattern/pattern_manager_dialog.h \
    $$PWD/pattern/pattern_theme.h \
    $$PWD/pattern/pattern_canvas.h \
    $$PWD/pattern/add_pattern_wizard.h \
    $$PWD/pattern/edit_pattern_wizard.h \
    $$PWD/widgets/compact_combo_box.h \
    $$PWD/widgets/connection_action_button.h \
    $$PWD/widgets/connection_state_dot.h \
    $$PWD/widgets/connection_state_label.h \
    $$PWD/widgets/device_nav_dot.h \
    $$PWD/widgets/device_nav_item_widget.h \
    $$PWD/widgets/flat_list_widget.h \
    $$PWD/widgets/frame_box.h \
    $$PWD/plc/mitsubishi_mc_device_widget.h \
    $$PWD/plc/plc_mitsu_device_wizard.h \
    $$PWD/project_infor_setting.h \
    $$PWD/widgets/state_pill_label.h \
    $$PWD/widgets/status_lamp_dot.h \
    $$PWD/task/localization_dashboard_widget.h \
    $$PWD/task/localization_patterns_widget.h \
    $$PWD/task/localization_setting_widget.h \
    $$PWD/task/localization_task_widget.h \
    $$PWD/task_widget.h \
    $$PWD/widgets/status_text_label.h \
    $$PWD/widgets/send_state_hint_label.h \
    $$PWD/widgets/status_lamp.h \
    $$PWD/widgets/type_chip_label.h \
    $$PWD/vision_output/vision_tcpip_device_widget.h \
    $$PWD/vision_output/vision_tcpip_client_device_widget.h \
    $$PWD/vision_output/robot_kinematic_check_widget.h

FORMS += \
    $$PWD/add_device_wizard.ui \
    $$PWD/camera/basler_cam_select_dialog.ui \
    $$PWD/camera/basler_camera_widget.ui \
    $$PWD/new_project_dialog.ui \
    $$PWD/new_task_dialog.ui \
    $$PWD/pattern/add_pattern_image_dialog.ui \
    $$PWD/task/workspace_setting_dialog.ui \
    $$PWD/plc/mitsubishi_mc_device_widget.ui \
    $$PWD/plc/plc_mitsu_device_wizard.ui \
    $$PWD/project_infor_setting.ui \
    $$PWD/task/localization_dashboard_widget.ui \
    $$PWD/task/localization_patterns_widget.ui \
    $$PWD/task/localization_setting_widget.ui \
    $$PWD/task/localization_task_widget.ui \
    $$PWD/vision_output/vision_tcpip_device_widget.ui \
    $$PWD/vision_output/vision_tcpip_client_device_widget.ui \
    $$PWD/vision_output/robot_kinematic_check_widget.ui
