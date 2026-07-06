# Matching module (dependency level 1): pattern matching engine (OpenCV),
# match configs, pattern groups. May depend on: core. Must not include UI
# (known debt: match_config_property_adapter includes
# widgets/property_browser/prop_spec.h; scheduled to move UI-side).

SOURCES += \
    $$PWD/image_matcher.cpp \
    $$PWD/imatch_type_config.cpp \
    $$PWD/match_config_property_adapter.cpp \
    $$PWD/match_group.cpp \
    $$PWD/match_params.cpp \
    $$PWD/match_pattern.cpp \
    $$PWD/match_pattern_config.cpp \
    $$PWD/pattern_group_manager.cpp \
    $$PWD/utils_block_max.cpp \
    $$PWD/vision_utils.cpp

HEADERS += \
    $$PWD/edge_match_config.h \
    $$PWD/image_matcher.h \
    $$PWD/imatch_type_config.h \
    $$PWD/manager_result.h \
    $$PWD/match_config_property_adapter.h \
    $$PWD/match_box_gripper.h \
    $$PWD/match_group.h \
    $$PWD/match_object.h \
    $$PWD/match_params.h \
    $$PWD/match_pattern.h \
    $$PWD/match_pattern_config.h \
    $$PWD/match_pattern_layer.h \
    $$PWD/matching_types.h \
    $$PWD/pattern_group_manager.h \
    $$PWD/robot_picking_checker.h \
    $$PWD/utils_block_max.h \
    $$PWD/vision_utils.h
