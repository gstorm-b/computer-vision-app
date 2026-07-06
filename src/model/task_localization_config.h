#ifndef TASK_LOCALIZATION_CONFIG_H
#define TASK_LOCALIZATION_CONFIG_H

#include <QSharedData>
#include <QJsonObject>

#include "model/itask_config.h"
#include "model/task_device_binding.h"
#include "model/camera_workspace.h"
#include "matching/pattern_group_manager.h"
#include "core/qgadget_macro.h"
#include "core/logger/app_logger.h"

namespace vc::model {

class TaskLocalizeConfigPrivate : public QSharedData {
public:
    TaskLocalizeConfigPrivate() {}

    TaskLocalizeConfigPrivate(const TaskLocalizeConfigPrivate &other) = default;

    // number signals
    QString m_nActiveCamera;
    QString m_nActivePatternGroup;
    QString m_nDetectedNumber;
    QString m_nFaultCode;

    // bool signals
    QString m_bCameraValid;
    QString m_bPatternValid;
    QString m_bTaskReady;

    QString m_bExecuteTrigger;
    QString m_bMatchingFinished;
    QString m_bMatchingBusy;
    QString m_bMatchingDetected;
    QString m_bMatchingLowArea;
    QString m_bTaskFault;

    TaskDeviceBindings m_deviceBindings;
    CameraWorkspaceMap m_cameraWorkspaces;
};

class TaskLocalizeConfig : public ITaskConfig {
    Q_GADGET

    P_PROPERTY_STRING_READWRITE(QString, nActiveCamera, "Camera selection")
    P_PROPERTY_STRING_READWRITE(QString, nActivePatternGroup, "Pattern group selection")
    P_PROPERTY_STRING_READWRITE(QString, nDetectedNumber, "Detected number")
    P_PROPERTY_STRING_READWRITE(QString, nFaultCode, "Fault code")

    P_PROPERTY_STRING_READWRITE(QString, bCameraValid, "Camera ready")
    P_PROPERTY_STRING_READWRITE(QString, bPatternValid, "Pattern ready")
    P_PROPERTY_STRING_READWRITE(QString, bTaskReady, "Task ready")

    P_PROPERTY_STRING_READWRITE(QString, bExecuteTrigger, "Trigger")
    P_PROPERTY_STRING_READWRITE(QString, bMatchingFinished, "Finished")
    P_PROPERTY_STRING_READWRITE(QString, bMatchingBusy, "Busy")
    P_PROPERTY_STRING_READWRITE(QString, bMatchingDetected, "Detected")
    P_PROPERTY_STRING_READWRITE(QString, bMatchingLowArea, "Low Area")
    P_PROPERTY_STRING_READWRITE(QString, bTaskFault, "Task fault")

public:
    explicit TaskLocalizeConfig()
        : ITaskConfig(), d(new TaskLocalizeConfigPrivate()) {}

    // Persistence schema version. Bump when the on-disk shape changes in a way
    // older readers cannot parse; add migration logic in fromJson(). A document
    // with no "version" key is the pre-versioning legacy baseline (version 0).
    static constexpr int kSchemaVersion = 1;

    TaskType taskType() const override {
        return TaskType::LocalizationTask;
    }

    const QMetaObject &getMetaObject() const override {
        return vc::model::TaskLocalizeConfig::staticMetaObject;
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["version"]             = kSchemaVersion;
        obj["nActiveCamera"]       = d->m_nActiveCamera;
        obj["nActivePatternGroup"] = d->m_nActivePatternGroup;
        obj["nDetectedNumber"]     = d->m_nDetectedNumber;
        obj["nFaultCode"]          = d->m_nFaultCode;

        obj["bCameraValid"]      = d->m_bCameraValid;
        obj["bPatternValid"]     = d->m_bPatternValid;
        obj["bTaskReady"]        = d->m_bTaskReady;
        obj["bExecuteTrigger"]   = d->m_bExecuteTrigger;
        obj["bMatchingFinished"] = d->m_bMatchingFinished;
        obj["bMatchingBusy"]     = d->m_bMatchingBusy;
        obj["bMatchingDetected"] = d->m_bMatchingDetected;
        obj["bMatchingLowArea"]  = d->m_bMatchingLowArea;
        obj["bTaskFault"]        = d->m_bTaskFault;

        obj["deviceBindings"]   = d->m_deviceBindings.toJson();
        obj["cameraWorkspaces"] = d->m_cameraWorkspaces.toJson();

        return obj;
    }


    bool fromJson(const QJsonObject& obj) override {
        if (obj.empty()) {
            return false;
        }

        // Schema/version gate: a document written by a newer app (version above
        // what this build understands) is refused rather than silently loaded
        // with partial-default state. A missing "version" key is the legacy
        // pre-versioning baseline (treated as version 0) and is accepted.
        const int version = obj.value("version").toInt(0);
        if (version > kSchemaVersion) {
            LOG_USER_ERR << QStringLiteral(
                                "TaskLocalizeConfig: document schema version %1 is newer "
                                "than supported %2; refusing to load.")
                                .arg(version)
                                .arg(kSchemaVersion);
            return false;
        }

        d->m_nActiveCamera       = obj["nActiveCamera"].toString("");
        d->m_nActivePatternGroup = obj["nActivePatternGroup"].toString("");
        d->m_nDetectedNumber     = obj["nDetectedNumber"].toString("");
        d->m_nFaultCode          = obj["nFaultCode"].toString("");

        d->m_bCameraValid       = obj["bCameraValid"].toString("");
        d->m_bPatternValid      = obj["bPatternValid"].toString("");
        d->m_bTaskReady         = obj["bTaskReady"].toString("");
        d->m_bExecuteTrigger    = obj["bExecuteTrigger"].toString("");
        d->m_bMatchingFinished  = obj["bMatchingFinished"].toString("");
        d->m_bMatchingBusy      = obj["bMatchingBusy"].toString("");
        d->m_bMatchingDetected  = obj["bMatchingDetected"].toString("");
        d->m_bMatchingLowArea   = obj["bMatchingLowArea"].toString("");
        d->m_bTaskFault         = obj["bTaskFault"].toString("");

        if (!d->m_deviceBindings.fromJson(obj["deviceBindings"])) {
            return false;
        }

        // Workspace map is optional (older projects predate it); a parse error
        // is tolerated as "no workspaces" rather than failing the whole load.
        d->m_cameraWorkspaces.fromJson(obj["cameraWorkspaces"]);

        return true;
    }

    TaskLocalizeConfig(const TaskLocalizeConfig &other) : d(other.d) {}

    TaskLocalizeConfig &operator=(const TaskLocalizeConfig &other) {
        if (this != &other) d = other.d;
        return *this;
    }

    virtual ITaskConfig* copy() override {
        TaskLocalizeConfig *ptr = new TaskLocalizeConfig();
        *ptr = *this;
        return ptr;
    }

    // ── Camera workspace (ROI) accessors ──────────────────────────────────
    CameraWorkspaceMap cameraWorkspaces() const { return d->m_cameraWorkspaces; }
    CameraWorkspace cameraWorkspace(const QString &cameraId) const {
        return d->m_cameraWorkspaces.workspace(cameraId);
    }
    void setCameraWorkspace(const QString &cameraId, const CameraWorkspace &ws) {
        d->m_cameraWorkspaces.setWorkspace(cameraId, ws);
    }

public:
    QSharedDataPointer<TaskLocalizeConfigPrivate> d;
};


} // namespace vc::model


#endif // TASK_LOCALIZATION_CONFIG_H
