#include "camera_basler_gige.h"

#include "core/logger/app_logger.h"
#include <pylon/gige/GigETransportLayer.h>

#include <QJsonArray>
#include <QThread>

using namespace Pylon;

/// Device implementations: this file defines BaslerGigeCfg (JSON (de)serialization of Basler
/// GigE camera settings) and BaslerGigECamera (Pylon/GenICam-backed GigE camera device).
namespace vc::device {

/// Serializes this config to JSON, extending CameraCfg::toJson() with all Basler-specific
/// fields (identity, backlight, exposure, gain, frame rate, and IO line capabilities).
/// @return the populated JSON object
QJsonObject BaslerGigeCfg::toJson() const {
    QJsonObject obj = CameraCfg::toJson();
    obj["ModelName"]                = m_modelName;
    obj["UserDefinedName"]          = m_userDefinedName;
    obj["SerialNumber"]             = m_serialNumber;
    obj["IpAddress"]                = m_ipAddress;
    obj["AutoBacklightControl"]     = m_autoBacklightControl;
    obj["AutoBacklightLine"]        = m_autoBacklightLine;
    obj["AutoBacklightLineInvert"]  = m_autoBacklightInvert;
    obj["AutoBackLightDelay"]       = m_autoBacklightDelay;
    obj["AutoExposureMode"]         = BaslerExposureTypeToString(m_autoExposureMode);
    obj["ExposureLimitMax"]         = m_paramsExposureMax;
    obj["ExposureLimitMin"]         = m_paramsExposureMin;
    obj["Exposure"]                 = m_paramsExposureTime;
    obj["GainLimitMax"]             = m_paramsGainMax;
    obj["GainLimitMin"]             = m_paramsGainMin;
    obj["Gain"]                     = m_paramsGain;
    obj["EnableAcquisitionFrameRate"] = m_enableAcquisitionFrameRate;
    obj["AcquisitionFrameRate"]     = m_paramsAcquisitionFrameRate;

    QJsonArray line_arr;
    for(BaslerIOLine line : m_ioCapabilities) {
        QJsonObject obj = line.toJson();
        line_arr.append(obj);
    }
    obj["ioCapabilities"]           = line_arr;

    return obj;
}

/// Populates this config from JSON previously written by toJson(), including the
/// ioCapabilities array (entries with an empty name are skipped).
/// @param obj the JSON object to read
/// @return false if the base CameraCfg::fromJson() check fails (e.g. wrong camera type), else true
bool BaslerGigeCfg::fromJson(const QJsonObject &obj) {
    // check camera type first
    if (!CameraCfg::fromJson(obj)) {
        return false;
    }

    m_modelName                   = obj["ModelName"].toString();
    m_userDefinedName             = obj["UserDefinedName"].toString();
    m_serialNumber                = obj["SerialNumber"].toString();
    m_ipAddress                   = obj["IpAddress"].toString();

    m_autoBacklightControl        = obj["AutoBacklightControl"].toBool(false);
    m_autoBacklightLine           = obj["AutoBacklightLine"].toString();
    m_autoBacklightInvert         = obj["AutoBacklightLineInvert"].toBool(false);
    m_autoBacklightDelay          = obj["AutoBackLightDelay"].toInt(0);

    m_autoExposureMode            = BaslerExposureTypeFromString(obj["AutoExposureMode"].toString());

    m_paramsExposureMax           = obj["ExposureLimitMax"].toDouble(0.0);
    m_paramsExposureMin           = obj["ExposureLimitMin"].toDouble(0.0);
    m_paramsExposureTime          = obj["Exposure"].toDouble(0.0);

    m_paramsGainMax               = obj["GainLimitMax"].toInt(0);
    m_paramsGainMin               = obj["GainLimitMin"].toInt(0);
    m_paramsGain                  = obj["Gain"].toInt(0);

    m_enableAcquisitionFrameRate  = obj["EnableAcquisitionFrameRate"].toBool(false);
    m_paramsAcquisitionFrameRate  = obj["AcquisitionFrameRate"].toDouble(0.0);

    QJsonArray line_arr           = obj["ioCapabilities"].toArray();
    for (int idx=0;idx<line_arr.size();idx++) {
        BaslerIOLine new_line;
        QJsonObject obj = line_arr[idx].toObject();
        if (obj.isEmpty()) {
            continue;
        }
        new_line.fromJson(obj);
        if (!new_line.name.isEmpty()) {
            m_ioCapabilities.append(new_line);
        }
    }

    return true;
}

/// Constructs the device and registers m_config as its IDeviceCfg, with signals blocked
/// during registration so construction does not emit a config-changed notification.
BaslerGigECamera::BaslerGigECamera(QString id, QString name, QObject* parent)
    : CameraDevice(id, name, parent) {

    this->blockSignals(true);
    IDevice::setDeviceConfig(&m_config);
    this->blockSignals(false);
}

/// Disconnects the camera if currently connected, logging the device name/id either way.
void BaslerGigECamera::deviceTerminate() {
    if (isDeviceConnected()) {
        LOG_DEV_DEBUG << "Basler GigE Camera disconnect";
        deviceDisconnect();
    }
    LOG_DEV_DEBUG << "Basler GigE Camera device terminate, device name" << name()
                  << ", id" << id();
}

/// Connects to the physical Basler GigE camera configured by m_config.m_ipAddress: enumerates
/// GigE devices via the Pylon transport layer, matches by IP address, opens the camera instance,
/// caps the internal grab buffer at 64, reads the discovered IO line capabilities, and reads
/// current settings back from the hardware. On any failure the connection status is set to
/// ConnectFailed with a translated message in m_last_msg.
/// @return true if the camera was found, opened, and initialized successfully; false otherwise
bool BaslerGigECamera::deviceConnect() {
    m_camera_ip_address = m_config.m_ipAddress;

    try {
        Pylon::DeviceInfoList cameraDeviceList;
        Pylon::ITransportLayer* tl = Pylon::CTlFactory::GetInstance().CreateTl("BaslerGigE");
        Pylon::IGigETransportLayer* gigetl = dynamic_cast<Pylon::IGigETransportLayer*>(tl);
        if (!gigetl) {
            LOG_DEV_ERR << "Camera connect error, Basler SDL cannot create transport layer";
            m_last_msg = tr("SDK error cannot create transport layer");
            setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
            return false;
        }
        gigetl->EnumerateAllDevices(cameraDeviceList, false);


        bool camera_found = false;
        for (int idx=0;idx<cameraDeviceList.size();idx++) {
            QString ip_address = QString::fromStdString(cameraDeviceList.at(idx).GetIpAddress().c_str());
            if (m_camera_ip_address == ip_address) {
                m_config.m_deviceInfo = cameraDeviceList.at(idx);
                m_camera_info = m_config.m_deviceInfo;
                camera_found = true;
                break;
            }
        }

        if (!camera_found) {
            m_last_msg = tr("Not found camera, ip address: %1").arg(m_camera_ip_address);
            LOG_USER_ERR << m_last_msg;
            setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
            return false;
        }

        m_str_camera_id = QString::fromUtf8(m_camera_info.GetSerialNumber().c_str());
        LOG_USER_INFO << QString("Starting connect with camera %1, %2, %3, %4")
                         .arg(QString::fromUtf8(m_camera_info.GetModelName().c_str()))
                         .arg(QString::fromUtf8(m_camera_info.GetSerialNumber().c_str()))
                         .arg(QString::fromUtf8(m_camera_info.GetUserDefinedName().c_str()))
                         .arg(QString::fromUtf8(m_camera_info.GetIpAddress().c_str()));

        // create new camera instance
        if (m_camera_instance != nullptr) {
            m_camera_instance.reset();
        }
        m_camera_instance = std::make_shared<Pylon::CInstantCamera>(Pylon::CTlFactory::GetInstance().CreateDevice(m_camera_info));
        m_camera_instance->Open();

        if (!m_camera_instance->IsOpen()) {
            m_last_msg = tr("Camera %1 open fail.").arg(
                QString::fromStdString(m_camera_instance->GetDeviceInfo().GetDeviceID().c_str()));
            LOG_USER_ERR << m_last_msg;
            setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
            m_camera_instance->DestroyDevice();
            m_camera_instance.reset();
            return false;
        }


        LOG_USER_INFO << QString("Camera opened (%1)").arg(QString::fromUtf8(m_camera_info.GetSerialNumber().c_str()));
        m_camera_instance->MaxNumBuffer = 64;
        initializeIOPort();
        m_config.m_ioCapabilities= m_io_lines;
        m_config.m_ioCapabilities.detach();
        readCameraSetting();

    } catch (const Pylon::GenericException& e) {
        LOG_DEV_ERR << tr("Camera error, An exception occurred while connecting to camera");
        LOG_DEV_ERR << e.GetDescription();
        m_last_msg = e.GetDescription();
        setConnectionStatus(ConnectStatus::ConnectFailed, m_last_msg);
        return false;
    }

    LOG_USER_INFO << tr("Camera connect successful.");
    setConnectionStatus(ConnectStatus::Connected);
    return true;
}

/// Stops any in-progress grab and closes the camera instance if open, resets the shot-ready
/// flags, and updates the connection status.
/// @return false if there is no camera instance (status set to NoConnection); true otherwise
bool BaslerGigECamera::deviceDisconnect() {
    if (m_camera_instance == nullptr) {
        LOG_USER_INFO << tr("Camera disconnect failed, m_camera_instance is null");
        setConnectionStatus(ConnectStatus::NoConnection);
        return false;
    }

    if (m_camera_instance->IsOpen()) {
        if (m_camera_instance->IsGrabbing()) {
            m_camera_instance->StopGrabbing();
        }
        m_camera_instance->Close();
        m_last_msg = tr("Camera %1 closed.").arg(m_str_camera_id);
    } else {
        m_last_msg = tr("Camera %1 is not opend.").arg(m_str_camera_id);
    }
    LOG_USER_INFO << m_last_msg;

    m_software_trigger_shot_ready = false;
    m_continuous_shot_ready = false;
    setConnectionStatus(ConnectStatus::Disconnected);
    return true;
}

/// Thread-safe replacement of the entire Basler config, then re-registers it as the device's
/// IDeviceCfg so IDevice::setDeviceConfig can emit its change notification.
/// @param cfg the new config to copy into m_config
void BaslerGigECamera::setBaslerGigeConfig(BaslerGigeCfg& cfg) {
    QMutexLocker locker(&m_mutex);
    m_config = cfg;

    // change and emit signal
    IDevice::setDeviceConfig(&m_config);
}

/// Returns a copy of the current Basler GigE config.
BaslerGigeCfg BaslerGigECamera::baslerGigeConfig() const {
    return m_config;
}

/// IDevice generic config setter: casts `cfg` to BaslerGigeCfg and copies its fields
/// (identity, exposure, gain, frame rate, backlight, IO capabilities) into m_config one by
/// one, thread-safe via m_mutex, then re-registers m_config to emit the change notification.
/// @param cfg the source config; ignored (no-op) if null
void BaslerGigECamera::setDeviceConfig(IDeviceCfg *cfg) {
    if (!cfg) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    BaslerGigeCfg *gige_cfg = static_cast<BaslerGigeCfg*>(cfg);

    m_config.m_ipAddress = gige_cfg->m_ipAddress;
    m_config.m_modelName = gige_cfg->m_modelName;
    m_config.m_serialNumber = gige_cfg->m_serialNumber;
    m_config.m_userDefinedName = gige_cfg->m_userDefinedName;

    m_config.m_autoExposureMode = gige_cfg->m_autoExposureMode;
    m_config.m_paramsExposureTime = gige_cfg->m_paramsExposureTime;
    m_config.m_paramsGain = gige_cfg->m_paramsGain;
    m_config.m_enableAcquisitionFrameRate = gige_cfg->m_enableAcquisitionFrameRate;
    m_config.m_paramsAcquisitionFrameRate = gige_cfg->m_paramsAcquisitionFrameRate;

    m_config.m_autoBacklightControl = gige_cfg->m_autoBacklightControl;
    m_config.m_autoBacklightLine = gige_cfg->m_autoBacklightLine;
    m_config.m_autoBacklightInvert = gige_cfg->m_autoBacklightInvert;
    m_config.m_autoBacklightDelay = gige_cfg->m_autoBacklightDelay;

    m_config.m_ioCapabilities = gige_cfg->m_ioCapabilities;

    // change and emit signal
    IDevice::setDeviceConfig(&m_config);
}

/// Thread-safe setter for the single-shot grab timeout.
/// @param ms timeout in milliseconds
void BaslerGigECamera::setGrabTimeout(int ms) {
    QMutexLocker locker(&m_mutex);
    m_grab_timeout = ms;
}

/// Thread-safe setter for the exposure time, validated against the configured
/// [m_paramsExposureMin, m_paramsExposureMax] range; emits exposureChanged on success.
/// @param exposure the new exposure time to apply
/// @return false if `exposure` is outside the configured min/max range; true otherwise
bool BaslerGigECamera::setExposure(double exposure) {
    QMutexLocker locker(&m_mutex);
    if ((exposure < m_config.m_paramsExposureMin) ||
        (exposure > m_config.m_paramsExposureMax)) {
        return false;
    }

    m_config.m_paramsExposureTime = exposure;
    emit exposureChanged(exposure);
    return true;
}

/// Thread-safe setter for the gain, validated against the configured
/// [m_paramsGainMin, m_paramsGainMax] range (stored truncated to int); emits gainChanged
/// on success.
/// @param gain the new gain value to apply
/// @return false if `gain` is outside the configured min/max range; true otherwise
bool BaslerGigECamera::setGain(double gain) {
    QMutexLocker locker(&m_mutex);
    if ((gain < m_config.m_paramsGainMin) ||
        (gain > m_config.m_paramsGainMax)) {
        return false;
    }

    m_config.m_paramsGain = (int)gain;
    emit gainChanged(gain);
    return true;
}

/// Sets the auto-backlight-control flag and emits backlightControlChanged.
void BaslerGigECamera::setBacklightControl(bool enable) {
    m_config.m_autoBacklightControl = enable;
    emit backlightControlChanged(enable);
}

/// @note Not yet implemented: always returns false without querying the camera. The intended
/// GenICam LineSelector-based read logic is left commented out below as reference.
/// @return always false
bool BaslerGigECamera::readIO(QString name) {
    if (m_camera_instance == nullptr) {
        LOG_DEV_ERR << "set output fail, camera instance is null";
        return false;
    }

    return false;

    // GenApi::INodeMap& nodemap = m_camera_instance->GetNodeMap();
    // GenApi_3_1_Basler_pylon_v3::CEnumerationPtr lineSelector(nodemap.GetNode("LineSelector"));
    // if (GenApi::IsAvailable(lineSelector)) {
    //     StringList_t entries;
    //     lineSelector->GetSymbolics(entries);
    //     Pylon::String_t line_name(line.toStdString().c_str());
    //     if (entries.contains(line_name)) {
    //         CEnumParameter(nodemap, "LineSelector").SetValue(line_name);
    //         CEnumParameter(nodemap, "LineSource").SetValue("UserOutput1");
    //         // CEnumParameter(nodemap, "LineMode").SetValue("Output");
    //         CEnumParameter(nodemap, "UserOutputSelector").SetValue("UserOutput1");
    //         CBooleanParameter(nodemap, "UserOutputValue").SetValue(value);
    //     } else {
    //         LOG_USER_ERR << tr("%1 not available in this camera").arg(line);
    //     }
    // } else {
    //     LOG_USER_ERR << tr("LineSelector not exists in this camera");
    // }
}

/// Writes a boolean value to the named GenICam output line via the LineSelector/LineSource/
/// UserOutputSelector/UserOutputValue nodes (line is fixed to UserOutput1 source).
/// @param name the GenICam line name (LineSelector symbolic) to write
/// @param value the output state to set
/// @return false if the camera instance is null, LineSelector is unavailable, or `name` is not
///         one of the line's symbolics; true otherwise
bool BaslerGigECamera::writeIO(QString name, bool value) {
    if (m_camera_instance == nullptr) {
        LOG_DEV_ERR << "set output fail, camera instance is null";
        return false;
    }

    GenApi::INodeMap& nodemap = m_camera_instance->GetNodeMap();
    GenApi::CEnumerationPtr lineSelector(nodemap.GetNode("LineSelector"));
    if (GenApi::IsAvailable(lineSelector)) {
        StringList_t entries;
        lineSelector->GetSymbolics(entries);
        Pylon::String_t line_name(name.toStdString().c_str());
        if (entries.contains(line_name)) {
            CEnumParameter(nodemap, "LineSelector").SetValue(line_name);
            CEnumParameter(nodemap, "LineSource").SetValue("UserOutput1");
            // CEnumParameter(nodemap, "LineMode").SetValue("Output");
            CEnumParameter(nodemap, "UserOutputSelector").SetValue("UserOutput1");
            CBooleanParameter(nodemap, "UserOutputValue").SetValue(value);
        } else {
            LOG_USER_ERR << tr("%1 not available in this camera").arg(name);
            return false;
        }
    } else {
        LOG_USER_ERR << tr("LineSelector not exists in this camera");
        return false;
    }
    return true;
}

/// Pushes the current m_config exposure/gain/frame-rate settings to the camera's GenApi node
/// map (ExposureAuto, ExposureTimeAbs when exposure mode is Off, GainRaw,
/// AcquisitionFrameRateEnable/Abs), then emits parametersApplied(true).
/// @return false if the camera instance is null; true otherwise
bool BaslerGigECamera::applyParametersChange() {
    if (m_camera_instance == nullptr) {
        LOG_DEV_ERR << "setting parameters fail, camera instance is null";
        return false;
    }

    GenApi::INodeMap& nodemap = m_camera_instance->GetNodeMap();

    bool frameRateEnable;
    double frameRate;
    m_config.acquisitionFrameRate(frameRateEnable, frameRate);

    // only set exposure if auto exposure mode is off

    /// convert here to coressponse enum type
    /// you're doing greate


    QString exposure_mode;
    if (m_config.m_autoExposureMode == BaslerExposureMode::Exposure_Off) {
        exposure_mode = "Off";
    } else if (m_config.m_autoExposureMode == BaslerExposureMode::Exposure_Once) {
        exposure_mode = "Once";
    } else if (m_config.m_autoExposureMode == BaslerExposureMode::Exposure_Continuous) {
        exposure_mode = "Continuous";
    }

    CEnumParameter(nodemap, "ExposureAuto").SetValue(exposure_mode.toUtf8().cbegin());
    if (exposure_mode == "Off") {
        CFloatParameter(nodemap, "ExposureTimeAbs").SetValue(m_config.exposureTime());
    }
    CIntegerParameter(nodemap, "GainRaw").SetValue(m_config.gain());
    CBooleanParameter(nodemap, "AcquisitionFrameRateEnable").SetValue(frameRateEnable);
    CFloatParameter(nodemap, "AcquisitionFrameRateAbs").SetValue(frameRate);
    // OLOG_INFO << "Set paramters done," << frameRateEnable << frameRate
    //           << params_setter.get_exposure_time() << params_setter.get_gain();

    emit parametersApplied(true);
    return true;
}

/// Grabs a single frame synchronously (5000ms timeout via Pylon GrabOne), toggling the
/// configured backlight output line on/off around the grab if auto-backlight-control is
/// enabled, converting the result to a cv::Mat via pylon_image_to_mat(), and emitting
/// grabFinished with the outcome.
/// @note the success branch declares a local `result` that shadows the function-scope one;
///       grabFinished is emitted with that local (isGrabSuccess=true, frame set), but the
///       GrabResult actually returned by this function is the outer one, which is never
///       updated to reflect success and so always reports isGrabSuccess=false.
/// @return a GrabResult; see note above regarding its isGrabSuccess field
GrabResult BaslerGigECamera::grabSingleShot() {
    GrabResult result;
    result.isGrabSuccess = false;
    if (m_camera_instance == nullptr) {
        LOG_DEV_ERR << "grab single shot fail, camera instance is null";
        result.msg = "grab single shot fail, camera instance is null";
        return result;
    }

    if (m_camera_instance->IsGrabbing()) {
        LOG_DEV_ERR << "grab single shot fail, camera already grabbing";
        m_last_msg = "grab single shot fail, camera already grabbing";
        result.msg = m_last_msg;
        return result;
    }

    try {
        // enable output before grabbing
        if (m_config.m_autoBacklightControl) {
            setOutputLineState(m_config.m_autoBacklightLine, true);
            QThread::usleep(m_config.m_autoBacklightDelay);
        }

        // smart pointer receive the grab result data
        Pylon::CInstantCamera::GrabResultPtr_t ptrGrabResult;
        m_camera_instance->GrabOne(5000, ptrGrabResult, TimeoutHandling_ThrowException);

        // retrieve result
        if (ptrGrabResult->GrabSucceeded()) {
            cv::Mat image;
            pylon_image_to_mat(ptrGrabResult, image);
            GrabResult result;
            result.frame = image.clone();
            result.isGrabSuccess= true;
            result.msg = "Grab Succesfull";
            emit grabFinished(result);
        } else {\
            LOG_DEV_ERR << "grab single shot error:"
                         << QString::number(ptrGrabResult->GetErrorCode(), 16)
                         << ptrGrabResult->GetErrorDescription();
            m_last_msg = tr("grab single shot failed, %1, %2")
                             .arg(QString::number(ptrGrabResult->GetErrorCode(), 16))
                             .arg(ptrGrabResult->GetErrorDescription().c_str());

            result.msg = "Error";
            emit grabFinished(result);
        }

        if (m_config.m_autoBacklightControl) {
            setOutputLineState(m_config.m_autoBacklightLine, false);
        }

    } catch (GenICam::GenericException &e) {
        LOG_DEV_ERR << "An exception occurred while handle continuous shot";
        LOG_DEV_ERR << e.GetDescription();
        m_last_msg = e.GetDescription();
    }

    return result;
}

/// @note Not yet implemented: always returns false.
bool BaslerGigECamera::startAutoContinuousShot() {

    return false;
}

/// @note Not yet implemented: no-op.
void BaslerGigECamera::stopAutoContinousShot() {

}

/// @note Not yet implemented: always returns false.
bool BaslerGigECamera::startContinuousShot() {
    return false;
}

/// @note Not yet implemented: no-op.
void BaslerGigECamera::stopContinuousShot() {

}

/// @note Not yet implemented: always returns a default-constructed (unsuccessful) GrabResult.
GrabResult BaslerGigECamera::softwareTriggerShot() {
    return GrabResult();
}

// void BaslerGigECamera::setOutput(int output_number, bool value) {
//     LOG_DEV_DEBUG << "Basler camera set output by name, please use method setOutputLineState(QString, bool).";
// }

/// Writes a boolean output state to the named GenICam line via the LineSelector/LineSource/
/// UserOutputSelector/UserOutputValue nodes (line is fixed to UserOutput1 source); logs and
/// silently does nothing further if the camera instance, line, or LineSelector is unavailable.
/// @param line the GenICam line name (LineSelector symbolic) to write
/// @param value the output state to set
void BaslerGigECamera::setOutputLineState(QString line, bool value) {
    if (m_camera_instance == nullptr) {
        LOG_DEV_ERR << "set output fail, camera instance is null";
        return;
    }

    GenApi::INodeMap& nodemap = m_camera_instance->GetNodeMap();
    GenApi::CEnumerationPtr lineSelector(nodemap.GetNode("LineSelector"));
    if (GenApi::IsAvailable(lineSelector)) {
        StringList_t entries;
        lineSelector->GetSymbolics(entries);
        Pylon::String_t line_name(line.toStdString().c_str());
        if (entries.contains(line_name)) {
            CEnumParameter(nodemap, "LineSelector").SetValue(line_name);
            CEnumParameter(nodemap, "LineSource").SetValue("UserOutput1");
            // CEnumParameter(nodemap, "LineMode").SetValue("Output");
            CEnumParameter(nodemap, "UserOutputSelector").SetValue("UserOutput1");
            CBooleanParameter(nodemap, "UserOutputValue").SetValue(value);
        } else {
            LOG_USER_ERR << tr("%1 not available in this camera").arg(line);
        }
    } else {
        LOG_USER_ERR << tr("LineSelector not exists in this camera");
    }
}

/// Rebuilds m_io_lines (cleared first) by enumerating the camera's GenICam LineSelector
/// symbolics and, for each line, querying its LineMode entries to determine whether it
/// supports Input and/or Output and whether LineMode is writable; logs the discovered
/// capability per line. Silently ignores any Pylon::GenericException.
void BaslerGigECamera::initializeIOPort() {
    m_io_lines.clear();
    try {
        LOG_DEV_INFO << QString("Start check IO configuration capabilities.");

        // Get all IO Line information from camera node map
        GenApi::INodeMap& node_map = m_camera_instance->GetNodeMap();
        GenApi::CEnumerationPtr lineSelector(node_map.GetNode("LineSelector"));
        if (GenApi::IsAvailable(lineSelector)) {

            GenApi::StringList_t line_names;
            lineSelector->GetSymbolics(line_names);

            for (int idx=0;idx<line_names.size();idx++) {
                // add to all line option
                Pylon::String_t line_name = line_names[idx];
                QString line_option = line_name.c_str();
                lineSelector->FromString(line_name);
                GenApi::CEnumerationPtr lineMode(node_map.GetNode("LineMode"));
                BaslerIOLine line_capabilities;
                if (GenApi::IsAvailable(lineMode)) {

                    // check input/output change able mode
                    line_capabilities.is_writable = GenApi::IsWritable(lineMode);
                    GenApi::NodeList_t modeEntries;
                    lineMode->GetEntries(modeEntries);

                    for (auto const& node : modeEntries) {
                        GenApi::CEnumEntryPtr pEntry(node);
                        if (GenApi::IsAvailable(pEntry)) {
                            std::string modeStr = pEntry->GetSymbolic().c_str();
                            if (modeStr == "Input") line_capabilities.can_be_input = true;
                            if (modeStr == "Output") line_capabilities.can_be_output = true;
                        }
                    }

                    if (line_capabilities.can_be_input && line_capabilities.can_be_output) {
                        LOG_DEV_INFO << QString("Camera basler: %1 can be Input or Output").arg(line_option);
                    } else if (line_capabilities.can_be_input) {
                        LOG_DEV_INFO << QString("Camera basler: %1 can be Input only").arg(line_option);
                    } else if ( line_capabilities.can_be_output) {
                        LOG_DEV_INFO << QString("Camera basler: %1 can be Output only").arg(line_option);
                    }

                    line_capabilities.name = line_option;
                    m_io_lines.append(line_capabilities);
                } else {
                    LOG_DEV_INFO << QString("Camera basler: %1 Line Mode not available!").arg(line_option);
                }
            }
        } else {
            LOG_DEV_INFO << QString("Camera basler: Line selector is not available!");
            return;
        }
    } catch (const Pylon::GenericException& e) {
        // Handle error
    }
}

/// Reads exposure/gain/frame-rate limits and current values, plus identity fields
/// (model name, serial number, user-defined name, IP address), directly from the camera's
/// GenApi node map into m_config. Falls back to the hardware-read exposure/gain values when
/// the currently configured ones are outside the hardware's min/max range, and falls back the
/// frame rate to the hardware-read value when the configured one is outside [1.0, 100.0].
void BaslerGigECamera::readCameraSetting() {
    if (m_camera_instance == nullptr) {
        LOG_DEV_ERR << "read parameters setting from camera, m_camera_instance is null";
        return;
    }

    GenApi::INodeMap& nodemap = m_camera_instance->GetNodeMap();

    double minExposureLimit = CFloatParameter(nodemap, "ExposureTimeAbs").GetMin();
    double maxExposureLimit = CFloatParameter(nodemap, "ExposureTimeAbs").GetMax();
    double exposureTime = CFloatParameter(nodemap, "ExposureTimeAbs").GetValue();

    int minGainLimit = CIntegerParameter(nodemap, "GainRaw").GetMin();
    int maxGainLimit = CIntegerParameter(nodemap, "GainRaw").GetMax();
    int gain = CIntegerParameter(nodemap, "GainRaw").GetValue();
    // OLOG_INFO << "Exposure time" << gain << minGainLimit << maxGainLimit;

    // bool frameRateEnable = CBooleanParameter(nodemap, "AcquisitionFrameRateEnable").GetValue();
    double frameRate = CFloatParameter(nodemap, "AcquisitionFrameRateAbs").GetValue();

    QString auto_exposure = CEnumParameter(nodemap, "ExposureAuto").GetValue().c_str();
    if (auto_exposure == "Off") {
        m_config.m_autoExposureMode = BaslerExposureMode::Exposure_Off;
    } else if (auto_exposure == "Once") {
        m_config.m_autoExposureMode = BaslerExposureMode::Exposure_Once;
    } else if (auto_exposure == "Continuous") {
        m_config.m_autoExposureMode = BaslerExposureMode::Exposure_Continuous;
    }

    m_config.m_modelName = m_camera_instance->GetDeviceInfo().GetModelName();
    m_config.m_serialNumber = m_camera_instance->GetDeviceInfo().GetSerialNumber();
    m_config.m_userDefinedName= m_camera_instance->GetDeviceInfo().GetUserDefinedName();
    m_config.m_ipAddress = m_camera_instance->GetDeviceInfo().GetIpAddress();

    // m_config.setExposureTimeLimit(minExposureLimit, maxExposureLimit);
    // m_config.setGainLimit(minGainLimit, maxGainLimit);
    m_config.m_paramsExposureMin = minExposureLimit;
    m_config.m_paramsExposureMax = maxExposureLimit;

    m_config.m_paramsGainMin = minGainLimit;
    m_config.m_paramsGainMax = maxGainLimit;

    if ((m_config.m_paramsExposureTime < minExposureLimit) ||
        (m_config.m_paramsExposureTime > maxExposureLimit)) {
        m_config.m_paramsExposureTime = exposureTime;
    }

    if ((m_config.m_paramsGain < minGainLimit) ||
        (m_config.m_paramsGain > maxGainLimit)) {
        m_config.m_paramsGain = gain;
    }

    if ((m_config.m_paramsAcquisitionFrameRate < 1.0) ||
        (m_config.m_paramsAcquisitionFrameRate > 100.0)) {
        m_config.m_paramsAcquisitionFrameRate = frameRate;
    }
}

/// Pushes the current m_config exposure/gain/frame-rate settings to the camera's GenApi node
/// map (ExposureAuto, ExposureTimeAbs, GainRaw, AcquisitionFrameRateEnable/Abs).
void BaslerGigECamera::loadSettingToCamera() {
    if (m_camera_instance == nullptr) {
        LOG_DEV_ERR << "set parameters setting to camera, m_camera_instance is null";
        return;
    }

    // OLOG_DEBUG << "Is camera open" << m_camera_instance->IsOpen();

    GenApi::INodeMap& nodemap = m_camera_instance->GetNodeMap();
    bool frameRateEnable;
    double frameRate;
    m_config.acquisitionFrameRate(frameRateEnable, frameRate);

    // only set exposure if auto exposure mode is off
    CEnumParameter(nodemap, "ExposureAuto").SetValue(m_config.autoExposure().toUtf8().cbegin());
    CFloatParameter(nodemap, "ExposureTimeAbs").SetValue(m_config.exposureTime());
    // if (m_config.get_auto_exposure() == "Off") {
    //     CFloatParameter(nodemap, "ExposureTimeAbs").SetValue(m_config.get_exposure_time());
    // }
    CIntegerParameter(nodemap, "GainRaw").SetValue(m_config.gain());
    CBooleanParameter(nodemap, "AcquisitionFrameRateEnable").SetValue(frameRateEnable);
    CFloatParameter(nodemap, "AcquisitionFrameRateAbs").SetValue(frameRate);
    // OLOG_INFO << "Set paramters done," << frameRateEnable << frameRate
    //           << params_setter.get_exposure_time() << params_setter.get_gain();

}

/// Converts a Pylon grab result into an OpenCV cv::Mat: converts to BGR8packed for color
/// images or Mono8 for monochrome images, then wraps the converted buffer and clones it into
/// `mat`. Logs an error and sets `mat` to an empty cv::Mat for any other output pixel format.
/// @param ptrGrabResult the Pylon grab result to convert
/// @param mat output matrix; overwritten with a cloned copy of the converted image
inline void BaslerGigECamera::pylon_image_to_mat(Pylon::CInstantCamera::GrabResultPtr_t &ptrGrabResult, cv::Mat &mat) {
    Pylon::CPylonImage pylon_image;
    Pylon::CImageFormatConverter formatConverter;
    Pylon::EPixelType pixel_type = ptrGrabResult->GetPixelType();
    bool is_color = Pylon::IsColorImage(pixel_type);
    if (is_color) {
        formatConverter.OutputPixelFormat = Pylon::PixelType_BGR8packed;
        formatConverter.Convert(pylon_image, ptrGrabResult);
    } else {
        formatConverter.OutputPixelFormat = Pylon::PixelType_Mono8;
        formatConverter.Convert(pylon_image, ptrGrabResult);
    }

    switch (formatConverter.OutputPixelFormat.GetValue()) {
    case Pylon::PixelType_Mono8:
        mat = cv::Mat(pylon_image.GetHeight(), pylon_image.GetWidth(), CV_8UC1, (uint8_t*)pylon_image.GetBuffer()).clone();
        return;
    case Pylon::PixelType_BGR8packed:
        mat = cv::Mat(pylon_image.GetHeight(), pylon_image.GetWidth(), CV_8UC3, (uint8_t*)pylon_image.GetBuffer()).clone();
        return;
    default:
        break;
    }

    LOG_DEV_ERR << "convert image fail, unsupported pixel format" << formatConverter.OutputPixelFormat.GetValue();
    mat = cv::Mat();
    return;
}

} // namespace vc::device
