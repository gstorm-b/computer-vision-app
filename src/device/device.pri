# Device module (dependency level 1): device families (camera, PLC,
# vision-output, robot), factory/registry/manager. Abstract base + concrete
# subtype per family. May depend on: core. Must not include UI or model.

SOURCES += \
    $$PWD/camera/camera_basler_gige.cpp \
    $$PWD/device_factory.cpp \
    $$PWD/device_manager.cpp \
    $$PWD/device_registry.cpp \
    $$PWD/plc/mc_device_map.cpp \
    $$PWD/plc/mc_fame_3e.cpp \
    $$PWD/plc/mc_protocol_device.cpp \
    $$PWD/output_device/vision_tcpip_device_base.cpp \
    $$PWD/output_device/vision_tcpip_device.cpp \
    $$PWD/output_device/vision_tcpip_client_device.cpp \
    $$PWD/robot/kawasaki_robot_device.cpp \
    $$PWD/robot/nachi_robot_device.cpp

HEADERS += \
    $$PWD/camera/basler_define.h \
    $$PWD/camera/camera_basler_gige.h \
    $$PWD/device_capabilities.h \
    $$PWD/camera/camera_device.h \
    $$PWD/communication_device.h \
    $$PWD/device_factory.h \
    $$PWD/device_io_interface.h \
    $$PWD/device_manager.h \
    $$PWD/device_registry.h \
    $$PWD/idevice.h \
    $$PWD/idevice_config.h \
    $$PWD/irequest.h \
    $$PWD/plc/mc_context.h \
    $$PWD/plc/mc_context_3e.h \
    $$PWD/plc/mc_context_factory.h \
    $$PWD/plc/mc_define.h \
    $$PWD/plc/mc_device_map.h \
    $$PWD/plc/mc_fame_3e.h \
    $$PWD/plc/mc_frame_abstract.h \
    $$PWD/plc/mc_msg_interface.h \
    $$PWD/plc/mc_msg_tcp_client.h \
    $$PWD/plc/mc_protocol_config.h \
    $$PWD/plc/mc_protocol_device.h \
    $$PWD/plc/plc_device.h \
    $$PWD/plc/mc_request.h \
    $$PWD/plc/memory_utils.h \
    $$PWD/plc/plc_request.h \
    $$PWD/plc/plc_value.h \
    $$PWD/output_device/vision_output_config.h \
    $$PWD/output_device/vision_output_device.h \
    $$PWD/output_device/vision_output_request.h \
    $$PWD/output_device/vision_tcpip_protocol.h \
    $$PWD/output_device/vision_tcpip_device_base.h \
    $$PWD/output_device/vision_tcpip_config.h \
    $$PWD/output_device/vision_tcpip_device.h \
    $$PWD/output_device/vision_tcpip_client_config.h \
    $$PWD/output_device/vision_tcpip_client_device.h \
    $$PWD/robot/robot_device.h \
    $$PWD/robot/kawasaki_robot_config.h \
    $$PWD/robot/kawasaki_robot_device.h \
    $$PWD/robot/nachi_robot_config.h \
    $$PWD/robot/nachi_robot_device.h
