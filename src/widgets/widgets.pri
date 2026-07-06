# Widgets module (UI level): reusable custom widgets — image/vision
# canvases, property browser, monitors, trees, dialogs.
# May depend on: every non-UI module.
#
# The qtpropertybrowser/ subfolder is vendored Qt Solutions code; it is
# listed in its own section below and moves to 3rdparty/ in a later
# restructure phase. Do not modify vendored files for app features.

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
    $$PWD/property_browser/property_browser_widget.h \
    $$PWD/calibration/calibration_board_dialog.h \
    $$PWD/calibration/calibration_threshold_dialog.h \
    $$PWD/calibration/calibration_points_table.h \
    $$PWD/signals_map_widget.h \
    $$PWD/signals_monitor_widget.h \
    $$PWD/task_event_log_widget.h

# --- Vendored Qt Solutions property browser (moves to 3rdparty/ later) ---

SOURCES += \
    $$PWD/qtpropertybrowser/qtbuttonpropertybrowser.cpp \
    $$PWD/qtpropertybrowser/qteditorfactory.cpp \
    $$PWD/qtpropertybrowser/qtgroupboxpropertybrowser.cpp \
    $$PWD/qtpropertybrowser/qtpropertybrowser.cpp \
    $$PWD/qtpropertybrowser/qtpropertybrowserutils.cpp \
    $$PWD/qtpropertybrowser/qtpropertymanager.cpp \
    $$PWD/qtpropertybrowser/qttreepropertybrowser.cpp \
    $$PWD/qtpropertybrowser/qtvariantproperty.cpp

HEADERS += \
    $$PWD/qtpropertybrowser/QtAbstractEditorFactoryBase \
    $$PWD/qtpropertybrowser/QtAbstractPropertyBrowser \
    $$PWD/qtpropertybrowser/QtAbstractPropertyManager \
    $$PWD/qtpropertybrowser/QtBoolPropertyManager \
    $$PWD/qtpropertybrowser/QtBrowserItem \
    $$PWD/qtpropertybrowser/QtButtonPropertyBrowser \
    $$PWD/qtpropertybrowser/QtCharEditorFactory \
    $$PWD/qtpropertybrowser/QtCharPropertyManager \
    $$PWD/qtpropertybrowser/QtCheckBoxFactory \
    $$PWD/qtpropertybrowser/QtColorEditorFactory \
    $$PWD/qtpropertybrowser/QtColorPropertyManager \
    $$PWD/qtpropertybrowser/QtCursorEditorFactory \
    $$PWD/qtpropertybrowser/QtCursorPropertyManager \
    $$PWD/qtpropertybrowser/QtDateEditFactory \
    $$PWD/qtpropertybrowser/QtDatePropertyManager \
    $$PWD/qtpropertybrowser/QtDateTimeEditFactory \
    $$PWD/qtpropertybrowser/QtDateTimePropertyManager \
    $$PWD/qtpropertybrowser/QtDoublePropertyManager \
    $$PWD/qtpropertybrowser/QtDoubleSpinBoxFactory \
    $$PWD/qtpropertybrowser/QtEnumEditorFactory \
    $$PWD/qtpropertybrowser/QtEnumPropertyManager \
    $$PWD/qtpropertybrowser/QtFlagPropertyManager \
    $$PWD/qtpropertybrowser/QtFontEditorFactory \
    $$PWD/qtpropertybrowser/QtFontPropertyManager \
    $$PWD/qtpropertybrowser/QtGroupBoxPropertyBrowser \
    $$PWD/qtpropertybrowser/QtGroupPropertyManager \
    $$PWD/qtpropertybrowser/QtIntPropertyManager \
    $$PWD/qtpropertybrowser/QtKeySequenceEditorFactory \
    $$PWD/qtpropertybrowser/QtKeySequencePropertyManager \
    $$PWD/qtpropertybrowser/QtLineEditFactory \
    $$PWD/qtpropertybrowser/QtLocalePropertyManager \
    $$PWD/qtpropertybrowser/QtPointFPropertyManager \
    $$PWD/qtpropertybrowser/QtPointPropertyManager \
    $$PWD/qtpropertybrowser/QtProperty \
    $$PWD/qtpropertybrowser/QtRectFPropertyManager \
    $$PWD/qtpropertybrowser/QtRectPropertyManager \
    $$PWD/qtpropertybrowser/QtScrollBarFactory \
    $$PWD/qtpropertybrowser/QtSizeFPropertyManager \
    $$PWD/qtpropertybrowser/QtSizePolicyPropertyManager \
    $$PWD/qtpropertybrowser/QtSizePropertyManager \
    $$PWD/qtpropertybrowser/QtSliderFactory \
    $$PWD/qtpropertybrowser/QtSpinBoxFactory \
    $$PWD/qtpropertybrowser/QtStringPropertyManager \
    $$PWD/qtpropertybrowser/QtTimeEditFactory \
    $$PWD/qtpropertybrowser/QtTimePropertyManager \
    $$PWD/qtpropertybrowser/QtTreePropertyBrowser \
    $$PWD/qtpropertybrowser/QtVariantEditorFactory \
    $$PWD/qtpropertybrowser/QtVariantProperty \
    $$PWD/qtpropertybrowser/QtVariantPropertyManager \
    $$PWD/qtpropertybrowser/qtbuttonpropertybrowser.h \
    $$PWD/qtpropertybrowser/qteditorfactory.h \
    $$PWD/qtpropertybrowser/qtgroupboxpropertybrowser.h \
    $$PWD/qtpropertybrowser/qtpropertybrowser.h \
    $$PWD/qtpropertybrowser/qtpropertybrowserutils_p.h \
    $$PWD/qtpropertybrowser/qtpropertymanager.h \
    $$PWD/qtpropertybrowser/qttreepropertybrowser.h \
    $$PWD/qtpropertybrowser/qtvariantproperty.h

RESOURCES += \
    $$PWD/qtpropertybrowser/qtpropertybrowser.qrc

DISTFILES += \
    $$PWD/qtpropertybrowser/images/button-reset.ico \
    $$PWD/qtpropertybrowser/images/cursor-arrow.png \
    $$PWD/qtpropertybrowser/images/cursor-busy.png \
    $$PWD/qtpropertybrowser/images/cursor-closedhand.png \
    $$PWD/qtpropertybrowser/images/cursor-cross.png \
    $$PWD/qtpropertybrowser/images/cursor-forbidden.png \
    $$PWD/qtpropertybrowser/images/cursor-hand.png \
    $$PWD/qtpropertybrowser/images/cursor-hsplit.png \
    $$PWD/qtpropertybrowser/images/cursor-ibeam.png \
    $$PWD/qtpropertybrowser/images/cursor-openhand.png \
    $$PWD/qtpropertybrowser/images/cursor-sizeall.png \
    $$PWD/qtpropertybrowser/images/cursor-sizeb.png \
    $$PWD/qtpropertybrowser/images/cursor-sizef.png \
    $$PWD/qtpropertybrowser/images/cursor-sizeh.png \
    $$PWD/qtpropertybrowser/images/cursor-sizev.png \
    $$PWD/qtpropertybrowser/images/cursor-uparrow.png \
    $$PWD/qtpropertybrowser/images/cursor-vsplit.png \
    $$PWD/qtpropertybrowser/images/cursor-wait.png \
    $$PWD/qtpropertybrowser/images/cursor-whatsthis.png \
    $$PWD/qtpropertybrowser/qtpropertybrowser.pri
