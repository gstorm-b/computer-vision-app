QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

CONFIG += c++17
CONFIG+=deploy_deps

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    src/app_settings/app_settings.cpp \
    src/calibration/calibration_board.cpp \
    src/calibration/calibration_board_factory.cpp \
    src/calibration/calibrator.cpp \
    src/calibration/fanuc_irvision_board.cpp \
    src/device/camera/camera_basler_gige.cpp \
    src/device/device_factory.cpp \
    src/device/device_manager.cpp \
    src/device/device_registry.cpp \
    src/device/plc/mc_device_map.cpp \
    src/device/plc/mc_fame_3e.cpp \
    src/device/plc/mc_protocol_device.cpp \
    src/device/output_device/vision_tcpip_device_base.cpp \
    src/device/output_device/vision_tcpip_device.cpp \
    src/device/output_device/vision_tcpip_client_device.cpp \
    src/device/robot/kawasaki_robot_device.cpp \
    src/device/robot/nachi_robot_device.cpp \
    src/form/add_device_wizard.cpp \
    src/form/camera/basler_cam_select_dialog.cpp \
    src/form/camera/basler_camera_widget.cpp \
    src/form/device_widget_factory.cpp \
    src/form/new_project_dialog.cpp \
    src/form/new_task_dialog.cpp \
    src/form/pattern/add_pattern_image_dialog.cpp \
    src/form/task/workspace_setting_dialog.cpp \
    src/form/pattern/pattern_canvas.cpp \
    src/form/pattern/add_pattern_wizard.cpp \
    src/form/pattern/edit_pattern_wizard.cpp \
    src/form/widgets/compact_combo_box.cpp \
    src/form/widgets/connection_action_button.cpp \
    src/form/widgets/connection_state_dot.cpp \
    src/form/widgets/connection_state_label.cpp \
    src/form/widgets/device_nav_dot.cpp \
    src/form/widgets/device_nav_item_widget.cpp \
    src/form/widgets/flat_list_widget.cpp \
    src/form/widgets/frame_box.cpp \
    src/form/plc/mitsubishi_mc_device_widget.cpp \
    src/form/plc/plc_mitsu_device_wizard.cpp \
    src/form/project_infor_setting.cpp \
    src/form/widgets/state_pill_label.cpp \
    src/form/widgets/status_lamp_dot.cpp \
    src/form/task/localization_dashboard_widget.cpp \
    src/form/task/localization_patterns_widget.cpp \
    src/form/task/localization_setting_widget.cpp \
    src/form/task/localization_task_widget.cpp \
    src/form/widgets/status_text_label.cpp \
    src/form/widgets/type_chip_label.cpp \
    src/form/vision_output/vision_tcpip_device_widget.cpp \
    src/form/vision_output/vision_tcpip_client_device_widget.cpp \
    src/form/vision_output/robot_kinematic_check_widget.cpp \
    src/form/widgets/send_state_hint_label.cpp \
    src/form/widgets/status_lamp.cpp \
    src/libwg/group_frame.cpp \
    src/logger/app_logger.cpp \
    src/matching/image_matcher.cpp \
    src/matching/imatch_type_config.cpp \
    src/matching/match_config_property_adapter.cpp \
    src/matching/match_group.cpp \
    src/matching/match_params.cpp \
    src/matching/match_pattern.cpp \
    src/matching/match_pattern_config.cpp \
    src/matching/pattern_group_manager.cpp \
    src/matching/utils_block_max.cpp \
    src/matching/vision_utils.cpp \
    src/model/itask.cpp \
    src/model/localization_pipeline.cpp \
    src/model/localization_runtime_controller.cpp \
    src/model/localization_signal_mapper.cpp \
    src/model/project.cpp \
    src/model/project_repository.cpp \
    src/model/robot_kinematic_picking_checker.cpp \
    src/model/task_factory.cpp \
    src/model/task_localization.cpp \
    src/runtime/task_runner.cpp \
    src/system_log_form.cpp \
    src/widgets/camera_mapping_widget.cpp \
    src/widgets/camera_workspace_widget.cpp \
    src/widgets/clamp.cpp \
    src/widgets/group_frame_widget.cpp \
    src/widgets/image_widget/image_view_only.cpp \
    src/widgets/image_widget/image_widget.cpp \
    src/widgets/image_widget/item_gripper_box.cpp \
    src/widgets/image_widget/item_picking_pos.cpp \
    src/widgets/image_widget/item_roi.cpp \
    src/widgets/image_widget/item_roi_rotated.cpp \
    src/widgets/vision/vision_canvas.cpp \
    src/widgets/vision/vision_geometry.cpp \
    src/widgets/vision/vision_numeric_inspector.cpp \
    src/widgets/vision/vision_result_adapter.cpp \
    src/widgets/vision/vision_result_viewer_widget.cpp \
    src/widgets/vision/vision_roi_config_adapter.cpp \
    src/widgets/vision/vision_roi_editor_widget.cpp \
    src/widgets/vision/vision_tool_palette.cpp \
    src/widgets/no_wheel_combobox.cpp \
    src/widgets/pattern_tree_widget.cpp \
    src/widgets/plc_widget/device_row_delegate.cpp \
    src/widgets/plc_widget/devices_monitor_widget.cpp \
    src/widgets/project_tree_widget.cpp \
    src/widgets/qtpropertybrowser/qtbuttonpropertybrowser.cpp \
    src/widgets/qtpropertybrowser/qteditorfactory.cpp \
    src/widgets/qtpropertybrowser/qtgroupboxpropertybrowser.cpp \
    src/widgets/qtpropertybrowser/qtpropertybrowser.cpp \
    src/widgets/qtpropertybrowser/qtpropertybrowserutils.cpp \
    src/widgets/qtpropertybrowser/qtpropertymanager.cpp \
    src/widgets/qtpropertybrowser/qttreepropertybrowser.cpp \
    src/widgets/qtpropertybrowser/qtvariantproperty.cpp \
    src/widgets/property_browser/custom_property_managers.cpp \
    src/widgets/property_browser/property_browser_widget.cpp \
    src/widgets/calibration/calibration_board_dialog.cpp \
    src/widgets/calibration/calibration_threshold_dialog.cpp \
    src/widgets/calibration/calibration_points_table.cpp \
    src/widgets/signals_map_widget.cpp \
    src/widgets/signals_monitor_widget.cpp \
    src/widgets/task_event_log_widget.cpp \
    src/utils/theme_manager.cpp


HEADERS += \
    mainwindow.h \
    src/app_settings/app_settings.h \
    src/calibration/calibration_board.h \
    src/calibration/calibration_board_factory.h \
    src/calibration/calibrator.h \
    src/calibration/fanuc_irvision_board.h \
    src/device/camera/basler_define.h \
    src/device/camera/camera_basler_gige.h \
    src/device/device_capabilities.h \
    src/device/camera/camera_device.h \
    src/device/communication_device.h \
    src/device/device_factory.h \
    src/device/device_io_interface.h \
    src/device/device_manager.h \
    src/device/device_registry.h \
    src/device/idevice.h \
    src/device/idevice_config.h \
    src/device/irequest.h \
    src/device/plc/mc_context.h \
    src/device/plc/mc_context_3e.h \
    src/device/plc/mc_context_factory.h \
    src/device/plc/mc_define.h \
    src/device/plc/mc_device_map.h \
    src/device/plc/mc_fame_3e.h \
    src/device/plc/mc_frame_abstract.h \
    src/device/plc/mc_msg_interface.h \
    src/device/plc/mc_msg_tcp_client.h \
    src/device/plc/mc_protocol_config.h \
    src/device/plc/mc_protocol_device.h \
    src/device/plc/plc_device.h \
    src/device/plc/mc_request.h \
    src/device/plc/memory_utils.h \
    src/device/plc/plc_request.h \
    src/device/plc/plc_value.h \
    src/device/output_device/vision_output_config.h \
    src/device/output_device/vision_output_device.h \
    src/device/output_device/vision_output_request.h \
    src/device/output_device/vision_tcpip_protocol.h \
    src/device/output_device/vision_tcpip_device_base.h \
    src/device/output_device/vision_tcpip_config.h \
    src/device/output_device/vision_tcpip_device.h \
    src/device/output_device/vision_tcpip_client_config.h \
    src/device/output_device/vision_tcpip_client_device.h \
    src/device/robot/robot_device.h \
    src/device/robot/kawasaki_robot_config.h \
    src/device/robot/kawasaki_robot_device.h \
    src/device/robot/nachi_robot_config.h \
    src/device/robot/nachi_robot_device.h \
    src/form/add_device_wizard.h \
    src/form/camera/basler_cam_select_dialog.h \
    src/form/camera/basler_camera_widget.h \
    src/form/device_widget.h \
    src/form/device_widget_factory.h \
    src/form/new_project_dialog.h \
    src/form/new_task_dialog.h \
    src/form/pattern/add_pattern_image_dialog.h \
    src/form/task/workspace_setting_dialog.h \
    src/form/pattern/pattern_manager_dialog.h \
    src/form/pattern/pattern_theme.h \
    src/form/pattern/pattern_canvas.h \
    src/form/pattern/add_pattern_wizard.h \
    src/form/pattern/edit_pattern_wizard.h \
    src/form/widgets/compact_combo_box.h \
    src/form/widgets/connection_action_button.h \
    src/form/widgets/connection_state_dot.h \
    src/form/widgets/connection_state_label.h \
    src/form/widgets/device_nav_dot.h \
    src/form/widgets/device_nav_item_widget.h \
    src/form/widgets/flat_list_widget.h \
    src/form/widgets/frame_box.h \
    src/form/plc/mitsubishi_mc_device_widget.h \
    src/form/plc/plc_mitsu_device_wizard.h \
    src/form/project_infor_setting.h \
    src/form/widgets/state_pill_label.h \
    src/form/widgets/status_lamp_dot.h \
    src/form/task/localization_dashboard_widget.h \
    src/form/task/localization_patterns_widget.h \
    src/form/task/localization_setting_widget.h \
    src/form/task/localization_task_widget.h \
    src/form/task_widget.h \
    src/form/widgets/status_text_label.h \
    src/form/widgets/send_state_hint_label.h \
    src/form/widgets/status_lamp.h \
    src/form/widgets/type_chip_label.h \
    src/form/vision_output/vision_tcpip_device_widget.h \
    src/form/vision_output/vision_tcpip_client_device_widget.h \
    src/form/vision_output/robot_kinematic_check_widget.h \
    src/libwg/group_frame.h \
    src/libwg/validating_line_edit.h \
    src/logger/app_logger.h \
    src/matching/edge_match_config.h \
    src/matching/image_matcher.h \
    src/matching/imatch_type_config.h \
    src/matching/manager_result.h \
    src/matching/match_config_property_adapter.h \
    src/matching/match_box_gripper.h \
    src/matching/match_group.h \
    src/matching/match_object.h \
    src/matching/match_params.h \
    src/matching/match_pattern.h \
    src/matching/match_pattern_config.h \
    src/matching/match_pattern_layer.h \
    src/matching/matching_types.h \
    src/matching/pattern_group_manager.h \
    src/matching/robot_picking_checker.h \
    src/matching/utils_block_max.h \
    src/matching/vision_utils.h \
    src/model/camera_map_entry.h \
    src/model/isignal_group.h \
    src/model/itask.h \
    src/model/itask_config.h \
    src/model/localization_fault_code.h \
    src/model/localization_pipeline.h \
    src/model/localization_recovery_policy.h \
    src/model/localization_runtime_controller.h \
    src/model/localization_signal_mapper.h \
    src/model/task_state_machine.h \
    src/runtime/device_command.h \
    src/runtime/device_command_queue.h \
    src/runtime/idevice_runner.h \
    src/runtime/device_runner.h \
    src/runtime/camera_runner.h \
    src/runtime/plc_runner.h \
    src/runtime/task_runner.h \
    src/model/pick_and_place_task.h \
    src/model/project.h \
    src/model/project_repository.h \
    src/model/robot_kinematic_picking_checker.h \
    src/model/camera_workspace.h \
    src/model/task_device_binding.h \
    src/model/task_define.h \
    src/model/task_factory.h \
    src/model/task_localization.h \
    src/model/task_localization_config.h \
    src/qgadget_marco.h \
    src/runtime/vision_output_runner.h \
    src/setting_keys.h \
    src/system_log_form.h \
    src/utils/meta_utils.h \
    src/utils/theme_manager.h \
    src/widgets/camera_mapping_widget.h \
    src/widgets/camera_workspace_widget.h \
    src/widgets/clamp.h \
    src/widgets/group_frame_widget.h \
    src/widgets/image_widget/image_view_only.h \
    src/widgets/image_widget/image_widget.h \
    src/widgets/image_widget/item_gripper_box.h \
    src/widgets/image_widget/item_picking_pos.h \
    src/widgets/image_widget/item_pixmap_bounding.h \
    src/widgets/image_widget/item_roi.h \
    src/widgets/image_widget/item_roi_rotated.h \
    src/widgets/lamp_button.h \
    src/widgets/no_wheel_combobox.h \
    src/widgets/no_wheel_double_spinbox.h \
    src/widgets/no_wheel_spinbox.h \
    src/widgets/pattern_tree_widget.h \
    src/widgets/vision/vision_canvas.h \
    src/widgets/vision/vision_geometry.h \
    src/widgets/vision/vision_numeric_inspector.h \
    src/widgets/vision/vision_overlay_types.h \
    src/widgets/vision/vision_result_adapter.h \
    src/widgets/vision/vision_result_viewer_widget.h \
    src/widgets/vision/vision_roi_config_adapter.h \
    src/widgets/vision/vision_roi_editor_widget.h \
    src/widgets/vision/vision_tool_palette.h \
    src/widgets/plc_widget/device_row_delegate.h \
    src/widgets/plc_widget/devices_monitor_widget.h \
    src/widgets/project_tree_widget.h \
    src/widgets/qtpropertybrowser/QtAbstractEditorFactoryBase \
    src/widgets/qtpropertybrowser/QtAbstractPropertyBrowser \
    src/widgets/qtpropertybrowser/QtAbstractPropertyManager \
    src/widgets/qtpropertybrowser/QtBoolPropertyManager \
    src/widgets/qtpropertybrowser/QtBrowserItem \
    src/widgets/qtpropertybrowser/QtButtonPropertyBrowser \
    src/widgets/qtpropertybrowser/QtCharEditorFactory \
    src/widgets/qtpropertybrowser/QtCharPropertyManager \
    src/widgets/qtpropertybrowser/QtCheckBoxFactory \
    src/widgets/qtpropertybrowser/QtColorEditorFactory \
    src/widgets/qtpropertybrowser/QtColorPropertyManager \
    src/widgets/qtpropertybrowser/QtCursorEditorFactory \
    src/widgets/qtpropertybrowser/QtCursorPropertyManager \
    src/widgets/qtpropertybrowser/QtDateEditFactory \
    src/widgets/qtpropertybrowser/QtDatePropertyManager \
    src/widgets/qtpropertybrowser/QtDateTimeEditFactory \
    src/widgets/qtpropertybrowser/QtDateTimePropertyManager \
    src/widgets/qtpropertybrowser/QtDoublePropertyManager \
    src/widgets/qtpropertybrowser/QtDoubleSpinBoxFactory \
    src/widgets/qtpropertybrowser/QtEnumEditorFactory \
    src/widgets/qtpropertybrowser/QtEnumPropertyManager \
    src/widgets/qtpropertybrowser/QtFlagPropertyManager \
    src/widgets/qtpropertybrowser/QtFontEditorFactory \
    src/widgets/qtpropertybrowser/QtFontPropertyManager \
    src/widgets/qtpropertybrowser/QtGroupBoxPropertyBrowser \
    src/widgets/qtpropertybrowser/QtGroupPropertyManager \
    src/widgets/qtpropertybrowser/QtIntPropertyManager \
    src/widgets/qtpropertybrowser/QtKeySequenceEditorFactory \
    src/widgets/qtpropertybrowser/QtKeySequencePropertyManager \
    src/widgets/qtpropertybrowser/QtLineEditFactory \
    src/widgets/qtpropertybrowser/QtLocalePropertyManager \
    src/widgets/qtpropertybrowser/QtPointFPropertyManager \
    src/widgets/qtpropertybrowser/QtPointPropertyManager \
    src/widgets/qtpropertybrowser/QtProperty \
    src/widgets/qtpropertybrowser/QtRectFPropertyManager \
    src/widgets/qtpropertybrowser/QtRectPropertyManager \
    src/widgets/qtpropertybrowser/QtScrollBarFactory \
    src/widgets/qtpropertybrowser/QtSizeFPropertyManager \
    src/widgets/qtpropertybrowser/QtSizePolicyPropertyManager \
    src/widgets/qtpropertybrowser/QtSizePropertyManager \
    src/widgets/qtpropertybrowser/QtSliderFactory \
    src/widgets/qtpropertybrowser/QtSpinBoxFactory \
    src/widgets/qtpropertybrowser/QtStringPropertyManager \
    src/widgets/qtpropertybrowser/QtTimeEditFactory \
    src/widgets/qtpropertybrowser/QtTimePropertyManager \
    src/widgets/qtpropertybrowser/QtTreePropertyBrowser \
    src/widgets/qtpropertybrowser/QtVariantEditorFactory \
    src/widgets/qtpropertybrowser/QtVariantProperty \
    src/widgets/qtpropertybrowser/QtVariantPropertyManager \
    src/widgets/qtpropertybrowser/qtbuttonpropertybrowser.h \
    src/widgets/qtpropertybrowser/qteditorfactory.h \
    src/widgets/qtpropertybrowser/qtgroupboxpropertybrowser.h \
    src/widgets/qtpropertybrowser/qtpropertybrowser.h \
    src/widgets/qtpropertybrowser/qtpropertybrowserutils_p.h \
    src/widgets/qtpropertybrowser/qtpropertymanager.h \
    src/widgets/qtpropertybrowser/qttreepropertybrowser.h \
    src/widgets/qtpropertybrowser/qtvariantproperty.h \
    src/widgets/property_browser/prop_spec.h \
    src/widgets/property_browser/custom_property_managers.h \
    src/widgets/property_browser/property_browser_widget.h \
    src/widgets/calibration/calibration_board_dialog.h \
    src/widgets/calibration/calibration_threshold_dialog.h \
    src/widgets/calibration/calibration_points_table.h \
    src/widgets/signals_map_widget.h \
    src/widgets/signals_monitor_widget.h \
    src/widgets/task_event_log_widget.h \
    windows_helper.h

FORMS += \
    mainwindow.ui \
    src/form/add_device_wizard.ui \
    src/form/camera/basler_cam_select_dialog.ui \
    src/form/camera/basler_camera_widget.ui \
    src/form/new_project_dialog.ui \
    src/form/new_task_dialog.ui \
    src/form/pattern/add_pattern_image_dialog.ui \
    src/form/task/workspace_setting_dialog.ui \
    src/form/plc/mitsubishi_mc_device_widget.ui \
    src/form/plc/plc_mitsu_device_wizard.ui \
    src/form/project_infor_setting.ui \
    src/form/task/localization_dashboard_widget.ui \
    src/form/task/localization_patterns_widget.ui \
    src/form/task/localization_setting_widget.ui \
    src/form/task/localization_task_widget.ui \
    src/form/vision_output/vision_tcpip_device_widget.ui \
    src/form/vision_output/vision_tcpip_client_device_widget.ui \
    src/form/vision_output/robot_kinematic_check_widget.ui \
    src/system_log_form.ui

TRANSLATIONS += \
    ncr_picking_ja_JP.ts
CONFIG += lrelease
CONFIG += embed_translations

INCLUDEPATH += \
    third_party/advance_docking/inlcude \
    src/

# Robot kinematics component (forward / inverse kinematics + optional Coal
# mesh-collision). Reusable library under components/RobotKinematics/, consumed
# here as source via its integration .pri. It pulls in the header-only Eigen
# and the prebuilt Coal/Boost/Assimp install trees under the repo-root 3rdparty
# folder. The .pri auto-copies the mesh-collision runtime DLL set and the
# Nachi MZ04D mesh assets next to the built binary after link. Customer installer
# packaging is still tracked in docs/backlog/later_todo_list.md #27.
include(components/RobotKinematics/robotkinematics.pri)

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

include(qmake/local_dependencies.pri)

# Opt-in dependency deployment: copy third-party runtime DLLs (ADS docking,
# OpenCV world, Basler Pylon) next to the built binary when the target dir is
# missing them, and run windeployqt for the Qt runtime + plugins. Off by
# default; enable with `qmake ... CONFIG+=deploy_deps`. RobotKinematics/Coal is
# deployed by robotkinematics.pri.
include(qmake/deploy_dependencies.pri)

RESOURCES += \
    3rdparty/advance_docking/include/ads.qrc \
    resrc.qrc \
    src/widgets/qtpropertybrowser/qtpropertybrowser.qrc

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/3rdparty/advance_docking/lib/ -lqtadvanceddocking
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/3rdparty/advance_docking/lib/ -lqtadvanceddockingd

INCLUDEPATH += $$PWD/3rdparty/advance_docking/include
DEPENDPATH += $$PWD/3rdparty/advance_docking/include

DISTFILES += \
    src/widgets/qtpropertybrowser/images/button-reset.ico \
    src/widgets/qtpropertybrowser/images/cursor-arrow.png \
    src/widgets/qtpropertybrowser/images/cursor-busy.png \
    src/widgets/qtpropertybrowser/images/cursor-closedhand.png \
    src/widgets/qtpropertybrowser/images/cursor-cross.png \
    src/widgets/qtpropertybrowser/images/cursor-forbidden.png \
    src/widgets/qtpropertybrowser/images/cursor-hand.png \
    src/widgets/qtpropertybrowser/images/cursor-hsplit.png \
    src/widgets/qtpropertybrowser/images/cursor-ibeam.png \
    src/widgets/qtpropertybrowser/images/cursor-openhand.png \
    src/widgets/qtpropertybrowser/images/cursor-sizeall.png \
    src/widgets/qtpropertybrowser/images/cursor-sizeb.png \
    src/widgets/qtpropertybrowser/images/cursor-sizef.png \
    src/widgets/qtpropertybrowser/images/cursor-sizeh.png \
    src/widgets/qtpropertybrowser/images/cursor-sizev.png \
    src/widgets/qtpropertybrowser/images/cursor-uparrow.png \
    src/widgets/qtpropertybrowser/images/cursor-vsplit.png \
    src/widgets/qtpropertybrowser/images/cursor-wait.png \
    src/widgets/qtpropertybrowser/images/cursor-whatsthis.png \
    src/widgets/qtpropertybrowser/qtpropertybrowser.pri

RC_ICONS = resrc/icon/software_icon.ico
