# Widgets module (UI level): reusable custom widgets — image/vision
# canvases, property browser, monitors, trees, dialogs.
# May depend on: every non-UI module. The vendored Qt Solutions property
# browser lives in 3rdparty/qtpropertybrowser (own vendor .pri).

SOURCES += \
    $$PWD/camera_mapping_widget.cpp \
    $$PWD/camera_workspace_widget.cpp \
    $$PWD/clamp.cpp \
    $$PWD/group_frame_widget.cpp \
    $$PWD/image_widget/image_view_only.cpp \
    $$PWD/image_widget/image_widget.cpp \
    $$PWD/image_widget/item_gripper_box.cpp \
    $$PWD/image_widget/item_picking_pos.cpp \
    $$PWD/image_widget/item_roi.cpp \
    $$PWD/image_widget/item_roi_rotated.cpp \
    $$PWD/vision/vision_canvas.cpp \
    $$PWD/vision/vision_geometry.cpp \
    $$PWD/vision/vision_numeric_inspector.cpp \
    $$PWD/vision/vision_result_adapter.cpp \
    $$PWD/vision/vision_result_viewer_widget.cpp \
    $$PWD/vision/vision_roi_config_adapter.cpp \
    $$PWD/vision/vision_roi_editor_widget.cpp \
    $$PWD/vision/vision_tool_palette.cpp \
    $$PWD/no_wheel_combobox.cpp \
    $$PWD/pattern_tree_widget.cpp \
    $$PWD/plc_widget/device_row_delegate.cpp \
    $$PWD/plc_widget/devices_monitor_widget.cpp \
    $$PWD/project_tree_widget.cpp \
    $$PWD/property_browser/custom_property_managers.cpp \
    $$PWD/property_browser/match_config_property_adapter.cpp \
    $$PWD/property_browser/property_browser_widget.cpp \
    $$PWD/calibration/calibration_board_dialog.cpp \
    $$PWD/calibration/calibration_threshold_dialog.cpp \
    $$PWD/calibration/calibration_points_table.cpp \
    $$PWD/signals_map_widget.cpp \
    $$PWD/signals_monitor_widget.cpp \
    $$PWD/task_event_log_widget.cpp

HEADERS += \
    $$PWD/camera_mapping_widget.h \
    $$PWD/camera_workspace_widget.h \
    $$PWD/clamp.h \
    $$PWD/group_frame_widget.h \
    $$PWD/image_widget/image_view_only.h \
    $$PWD/image_widget/image_widget.h \
    $$PWD/image_widget/item_gripper_box.h \
    $$PWD/image_widget/item_picking_pos.h \
    $$PWD/image_widget/item_pixmap_bounding.h \
    $$PWD/image_widget/item_roi.h \
    $$PWD/image_widget/item_roi_rotated.h \
    $$PWD/lamp_button.h \
    $$PWD/no_wheel_combobox.h \
    $$PWD/no_wheel_double_spinbox.h \
    $$PWD/no_wheel_spinbox.h \
    $$PWD/pattern_tree_widget.h \
    $$PWD/vision/vision_canvas.h \
    $$PWD/vision/vision_geometry.h \
    $$PWD/vision/vision_numeric_inspector.h \
    $$PWD/vision/vision_overlay_types.h \
    $$PWD/vision/vision_result_adapter.h \
    $$PWD/vision/vision_result_viewer_widget.h \
    $$PWD/vision/vision_roi_config_adapter.h \
    $$PWD/vision/vision_roi_editor_widget.h \
    $$PWD/vision/vision_tool_palette.h \
    $$PWD/plc_widget/device_row_delegate.h \
    $$PWD/plc_widget/devices_monitor_widget.h \
    $$PWD/project_tree_widget.h \
    $$PWD/property_browser/prop_spec.h \
    $$PWD/property_browser/custom_property_managers.h \
    $$PWD/property_browser/match_config_property_adapter.h \
    $$PWD/property_browser/property_browser_widget.h \
    $$PWD/calibration/calibration_board_dialog.h \
    $$PWD/calibration/calibration_threshold_dialog.h \
    $$PWD/calibration/calibration_points_table.h \
    $$PWD/signals_map_widget.h \
    $$PWD/signals_monitor_widget.h \
    $$PWD/task_event_log_widget.h

