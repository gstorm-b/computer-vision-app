#include "camera_basler_gige.h"

#include "core/logger/app_logger.h"
#include <pylon/gige/GigETransportLayer.h>

#include <QJsonArray>
#include <QThread>

using namespace Pylon;

namespace vc::device {

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

BaslerGigECamera::BaslerGigECamera(QString id, QString name, QObject* parent)
    : CameraDevice(id, name, parent) {

    this->blockSignals(true);
    IDevice::setDeviceConfig(&m_config);
    this->blockSignals(false);
}

void BaslerGigECamera::deviceTerminate() {
    if (isDeviceConnected()) {
        LOG_DEV_DEBUG << "Basler GigE Camera disconnect";
        deviceDisconnect();
    }
    LOG_DEV_DEBUG << "Basler GigE Camera device terminate, device name" << name()
                  << ", id" << id();
}

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

void BaslerGigECamera::setBaslerGigeConfig(BaslerGigeCfg& cfg) {
    QMutexLocker locker(&m_mutex);
    m_config = cfg;

    // change and emit signal
    IDevice::setDeviceConfig(&m_config);
}

BaslerGigeCfg BaslerGigECamera::baslerGigeConfig() const {
    return m_config;
}

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

void BaslerGigECamera::setGrabTimeout(int ms) {
    QMutexLocker locker(&m_mutex);
    m_grab_timeout = ms;
}

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

void BaslerGigECamera::setBacklightControl(bool enable) {
    m_config.m_autoBacklightControl = enable;
    emit backlightControlChanged(enable);
}

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

bool BaslerGigECamera::startAutoContinuousShot() {

    return false;
}

void BaslerGigECamera::stopAutoContinousShot() {

}

bool BaslerGigECamera::startContinuousShot() {
    return false;
}

void BaslerGigECamera::stopContinuousShot() {

}

GrabResult BaslerGigECamera::softwareTriggerShot() {
    return GrabResult();
}

// void BaslerGigECamera::setOutput(int output_number, bool value) {
//     LOG_DEV_DEBUG << "Basler camera set output by name, please use method setOutputLineState(QString, bool).";
// }

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
