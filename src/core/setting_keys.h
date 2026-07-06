#ifndef SETTING_KEYS_H
#define SETTING_KEYS_H

// application stored file name
#define SETTING_FILE_NAME                       "softstart.ini"
#define SETTING_LAST_OPEN_PATH                  "LastOpenPath"

// language
#define SETTING_DISPLAY_LANGUAGE                "DisplayLanguage"
#define SETTING_LANGUAGE_FILE_VN                "ncrn_picking_vi_VN"
#define SETTING_LANGUAGE_FILE_JP                "ncrn_picking_ja_JP"

// sub setting key
#define SETTING_LAST_TIME_OPEN                  "LastTimeOpen"
#define SETTING_LAST_TIME_CLOSE                 "LastTimeClose"
#define SETTING_LAST_TIME_SAVE                  "LastTimeSave"
#define SETTING_APP_PASSWORD                    "AppPassword"

// contain all robot, camera, plc setting
#define SETTING_OPERATION                       "Operation"

// logical worker
#define SETTING_KEY_LOGICAL_WORKER              "LogicalWorker"
#define SETTING_LOCK_CONTROL                    "LockControl"

// vision setting key
#define SETTING_KEY_VISION                      "Vision"

// camera setting keys
#define SETTING_KEY_CAMERA                      "Camera"
#define SETTING_KEY_CAMERA_NUM                  "NumOfCamera"
#define SETTING_KEY_CAMERA_ARRAY                "CameraArray"
#define SETTING_KEY_CAMERA_PARAMS               "CameraParams"
#define SETTING_KEY_CAMERA_PARAMS_DETAILS       "CameraParamsDetail"
#define SETTING_KEY_CAMERA_INDEX                "CameraIndex"
#define SETTING_KEY_USE_MATCH_ROI               "IsUseMatchROI"
#define SETTING_KEY_MATCH_ROI                   "MatchROI"
#define SETTING_KEY_ROI_TOPLEFT                 "TopLeft"
#define SETTING_KEY_ROI_BOTTOMRIGHT             "BottomRight"
#define SETTING_KEY_PICKING_BOX_SIZE            "PickingBoxSize"
#define SETTING_KEY_PICKING_BOX_HEIGHT          "Height"
#define SETTING_KEY_PICKING_BOX_WIDTH           "Width"

// camera coordination key
#define SETTING_KEY_COOR_CALIB                  "CoordinationCalib"

// camera calibration key
#define SETTING_KEY_CAMERA_CALIB                "CameraCalibration"
#define SETTING_KEY_CAMERA_MATRIX               "CameraMatrix"
#define SETTING_KEY_CAMERA_DISTCOEFFS           "DistCoeffs"
#define SETTING_KEY_CAMERA_MATRIX_R             "MatrixR"
#define SETTING_KEY_CAMERA_MATRIX_T             "MatrixT"
#define SETTING_KEY_CAMERA_IMAGE_SIZE           "ImageSize"
#define SETTING_KEY_CAMERA_REPRJ_ERR            "ReprojectorError"

// pattern setting keys
#define SETTING_KEY_PATTERN                     "Pattern"
#define SETTING_KEY_IMAGE_PATH                  "ImagePath"

#define PLC_CONNECT_INTERFACE                   "PlcConnectInterface"
#define PLC_CONNECT_ITF_PARAMS                  "PlcConnectParams"
#define PLC_CONNECT_ITF_TYPE                    "PlcConnectInterfaceType"
#define PLC_CONNECT_ITF_TYPE_E                  "ItfEthernet"
#define PLC_CONNECT_ITF_TYPE_S                  "ItfSerial"
#define PLC_CONNECT_ITF_E_IP                    "IpAddress"
#define PLC_CONNECT_ITF_E_PORT                  "Port"
#define PLC_PROTOCOL_FRAME_TYPE                 "FrameType"
#define PLC_PROTOCOL_DATA_CODE                  "DataCode"

#define JSON_O_TMPL_SRC                         "Pattern source"
#define JSON_O_TMPL_GROUP_NAME                  "Pattern group name"
#define JSON_O_TMPL_GROUP_INDEX                 "Pattern group index"
#define JSON_O_TMPL_GROUP_DATA                  "Pattern group data"
#define JSON_O_TMPL_SRC_PARAMS                  "Pattern parameters"

#define JSON_O_NAME                             "Pattern name"
#define JSON_O_INDEX                            "Pattern index"
#define JSON_O_PICKING_POSITION                 "Picking position"
#define JSON_O_MIN_SCORE                        "Min scores"
#define JSON_O_PICKING_ANGLE                    "Picking angle"
#define JSON_O_PT_OVERLAP                       "PtOverlap"
#define JSON_O_SEARCH_ANGLE                     "SearchAngle"
#define JSON_O_MIN_REDUCE_LENGTH                "MinReduceLength"
#define JSON_O_T_SAMPLE_LEVEL                   "TSampleLevel"
#define JSON_O_INV_THRESH                       "Binary threshold invert"
#define JSON_O_PICKING_BOX_SIZE                 "PickingBoxSize"
#define JSON_O_PICKING_BOX_DISTANCE             "PickingBoxDistance"
#define JSON_O_PICKING_BOX_ANGLE                "PickingBoxAngle"
#define JSON_O_SUB_PIXEL_ESTIMATION             "SubPixelEstimation"
#define JSON_O_STOP_LAYER_1                     "StopLayer1"

#define JSON_O_FILE_PATH                        "Image path"

// PLC Setting keys
#define SETTING_KEY_PLC                         "PLC"
#define SETTING_KEY_PLC_DEVICE_SETTINGS         "DeviceSettings"
#define SETTING_KEY_PLC_DEVICE_COMMENT          "DeviceComment"

#define JS_O_MITSU_M_DEVICE                     "MCDeviceM"
#define JS_O_MITSU_D_DEVICE                     "MCDeviceD"
#define JS_O_MITSU_DEVICE_ADDRESS               "Address"
#define JS_O_MITSU_DEVICE_COMMENT               "Comment"
#define JS_O_DEVICE_SETTINGS_NAME               "SetName"
#define JS_O_DEVICE_SETTINGS_ADDRESS            "SetAddress"

/// PLC DEVICES KEYS
#define MC_DV_

// robot setting keys
#define SETTING_KEY_ROBOT                       "Robot"
#define SETTING_KEY_ROBOT_TEMP_SQ               "RobotTempSequence"
#define SETTING_KEY_RB_IP_ADDRESS               "IpAddress"
#define SETTING_KEY_RB_SEQUENCE                 "Sequence"
#define SETTING_KEY_RB_HOMEPOS                  "HomePos"
#define SETTING_KEY_RB_SAFE_Z_UPPER             "SafeZUpper"
#define SETTING_KEY_RB_SAFE_Z_LOWER             "SafeZLower"
#define SETTING_KEY_RB_TOOL_OFFSET              "ToolOffset"
#define SETTING_KEY_RB_MAIN_PROG_NUMNER         "MainProgNumber"
#define SETTING_KEY_RB_USE_HOME_PROG            "UseHomeProg"
#define SETTING_KEY_RB_HOME_PROG_NUMNER         "HomeProgNumber"


#include "QJsonObject"
#include "opencv2/opencv.hpp"

namespace jsu {

inline QJsonObject Point2f2Json(cv::Point2f point) {
    QJsonObject json;
    json["X"] = point.x;
    json["Y"] = point.y;
    return json;
}

inline QJsonObject Point2Json(cv::Point point) {
    QJsonObject json;
    json["X"] = point.x;
    json["Y"] = point.y;
    return json;
}

inline cv::Point Json2Point(QJsonObject &obj) {
    cv::Point point;
    point.x = obj["X"].toDouble();
    point.y = obj["Y"].toDouble();
    return point;
}

inline cv::Point2f Json2Point2f(QJsonObject &obj) {
    cv::Point2f point;
    point.x = obj["X"].toDouble();
    point.y = obj["Y"].toDouble();
    return point;
}

inline QJsonObject Size2f2Json(cv::Size2f _size) {
    QJsonObject json;
    json["W"] = _size.width;
    json["H"] = _size.height;
    return json;
}

inline cv::Size2f Json2Size2f(QJsonObject &obj) {
    cv::Size2f sz;
    sz.width = obj["W"].toDouble();
    sz.height = obj["H"].toDouble();
    return sz;
}

}

#endif // SETTING_KEYS_H
