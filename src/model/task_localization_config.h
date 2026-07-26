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

/// Task model layer: per-task configuration types (e.g. TaskLocalizeConfig), state
/// machines, and device/camera bindings for the picking application.
namespace vc::model {

/// Implicitly-shared private data (QSharedData) backing TaskLocalizeConfig: holds the
/// PLC signal-name bindings and per-camera workspace ROIs for a localization task.
class TaskLocalizeConfigPrivate : public QSharedData {
public:
    /// Default-constructs with all signal-name strings empty and no device bindings/workspaces.
    TaskLocalizeConfigPrivate() {}

    /// Copies all signal-name bindings and workspace data from `other`.
    TaskLocalizeConfigPrivate(const TaskLocalizeConfigPrivate &other) = default;

    // number signals
    QString m_nActiveCamera;        ///< PLC signal name bound to the active-camera number.
    QString m_nActivePatternGroup;  ///< PLC signal name bound to the active pattern-group number.
    QString m_nDetectedNumber;      ///< PLC signal name bound to the detected-object number.
    QString m_nFaultCode;           ///< PLC signal name bound to the task fault code.

    // bool signals
    QString m_bCameraValid;         ///< PLC signal name bound to the "camera ready" flag.
    QString m_bPatternValid;        ///< PLC signal name bound to the "pattern ready" flag.
    QString m_bTaskReady;           ///< PLC signal name bound to the "task ready" flag.

    QString m_bExecuteTrigger;      ///< PLC signal name bound to the execute/trigger flag.
    QString m_bMatchingFinished;    ///< PLC signal name bound to the "matching finished" flag.
    QString m_bMatchingBusy;        ///< PLC signal name bound to the "matching busy" flag.
    QString m_bMatchingDetected;    ///< PLC signal name bound to the "matching detected" flag.
    QString m_bMatchingLowArea;     ///< PLC signal name bound to the "matching low area" flag.
    QString m_bTaskFault;           ///< PLC signal name bound to the "task fault" flag.

    TaskDeviceBindings m_deviceBindings;    ///< Device (PLC) bindings configured for this task.
    CameraWorkspaceMap m_cameraWorkspaces;  ///< Per-camera workspace (ROI) definitions.
};

/// Localization-task configuration: PLC signal-name bindings for camera/pattern selection
/// and matching status flags, plus per-camera workspace (ROI) definitions. Exposed as a
/// Q_GADGET so its bound signal names can be edited via the property-browser UI.
class TaskLocalizeConfig : public ITaskConfig {
    Q_GADGET

    /// PLC signal name bound to the active-camera selection ("Camera selection").
    P_PROPERTY_STRING_READWRITE(QString, nActiveCamera, "Camera selection")
    /// PLC signal name bound to the active pattern-group selection ("Pattern group selection").
    P_PROPERTY_STRING_READWRITE(QString, nActivePatternGroup, "Pattern group selection")
    /// PLC signal name bound to the detected-object number ("Detected number").
    P_PROPERTY_STRING_READWRITE(QString, nDetectedNumber, "Detected number")
    /// PLC signal name bound to the task fault code ("Fault code").
    P_PROPERTY_STRING_READWRITE(QString, nFaultCode, "Fault code")

    /// PLC signal name bound to the "camera ready" flag ("Camera ready").
    P_PROPERTY_STRING_READWRITE(QString, bCameraValid, "Camera ready")
    /// PLC signal name bound to the "pattern ready" flag ("Pattern ready").
    P_PROPERTY_STRING_READWRITE(QString, bPatternValid, "Pattern ready")
    /// PLC signal name bound to the "task ready" flag ("Task ready").
    P_PROPERTY_STRING_READWRITE(QString, bTaskReady, "Task ready")

    /// PLC signal name bound to the execute/trigger flag ("Trigger").
    P_PROPERTY_STRING_READWRITE(QString, bExecuteTrigger, "Trigger")
    /// PLC signal name bound to the "matching finished" flag ("Finished").
    P_PROPERTY_STRING_READWRITE(QString, bMatchingFinished, "Finished")
    /// PLC signal name bound to the "matching busy" flag ("Busy").
    P_PROPERTY_STRING_READWRITE(QString, bMatchingBusy, "Busy")
    /// PLC signal name bound to the "matching detected" flag ("Detected").
    P_PROPERTY_STRING_READWRITE(QString, bMatchingDetected, "Detected")
    /// PLC signal name bound to the "matching low area" flag ("Low Area").
    P_PROPERTY_STRING_READWRITE(QString, bMatchingLowArea, "Low Area")
    /// PLC signal name bound to the "task fault" flag ("Task fault").
    P_PROPERTY_STRING_READWRITE(QString, bTaskFault, "Task fault")

public:
    /// Constructs a config with all signal-name bindings empty and default (empty) device
    /// bindings/workspaces.
    explicit TaskLocalizeConfig()
        : ITaskConfig(), d(new TaskLocalizeConfigPrivate()) {}

    /// Persistence schema version. Bump when the on-disk shape changes in a way
    /// older readers cannot parse; add migration logic in fromJson(). A document
    /// with no "version" key is the pre-versioning legacy baseline (version 0).
    static constexpr int kSchemaVersion = 1;

    /// Identifies this config as belonging to a localization task.
    /// @return TaskType::LocalizationTask
    TaskType taskType() const override {
        return TaskType::LocalizationTask;
    }

    /// Returns the Q_GADGET static meta-object, used by property-browser code to enumerate
    /// and edit the signal-name properties by name and Q_CLASSINFO display name.
    /// @return TaskLocalizeConfig::staticMetaObject
    const QMetaObject &getMetaObject() const override {
        return vc::model::TaskLocalizeConfig::staticMetaObject;
    }

    /// Serializes the schema version, all PLC signal-name bindings, device bindings, and
    /// camera workspaces to a JSON object.
    /// @return the populated JSON object (always succeeds)
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


    /// Loads all signal-name bindings, device bindings, and camera workspaces from `obj`.
    /// Rejects documents whose "version" is newer than kSchemaVersion; a missing "version"
    /// key is treated as the legacy pre-versioning baseline. Camera workspaces are optional
    /// and tolerate a parse failure (treated as "no workspaces").
    /// @param obj the JSON object to load (as produced by toJson())
    /// @return false if `obj` is empty, the version is unsupported, or device bindings fail
    ///         to parse; true otherwise
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

    /// Shallow-copies `other`'s data pointer (QSharedDataPointer copy-on-write semantics).
    TaskLocalizeConfig(const TaskLocalizeConfig &other) : d(other.d) {}

    /// Assigns from `other`, sharing its underlying data pointer (copy-on-write).
    /// @return reference to this object
    TaskLocalizeConfig &operator=(const TaskLocalizeConfig &other) {
        if (this != &other) d = other.d;
        return *this;
    }

    /// Heap-allocates an independent copy of this config.
    /// @return a new TaskLocalizeConfig equal to this one; ownership transfers to the caller
    virtual ITaskConfig* copy() override {
        TaskLocalizeConfig *ptr = new TaskLocalizeConfig();
        *ptr = *this;
        return ptr;
    }

    // ── Camera workspace (ROI) accessors ──────────────────────────────────
    /// Returns all configured per-camera workspace (ROI) definitions.
    CameraWorkspaceMap cameraWorkspaces() const { return d->m_cameraWorkspaces; }
    /// Returns the workspace (ROI) configured for the given camera.
    /// @param cameraId identifier of the camera whose workspace to look up
    /// @return the camera's workspace, or a default-constructed (workspace-off) CameraWorkspace
    ///         if `cameraId` has none configured
    CameraWorkspace cameraWorkspace(const QString &cameraId) const {
        return d->m_cameraWorkspaces.workspace(cameraId);
    }
    /// Sets (or replaces) the workspace (ROI) for the given camera.
    /// @param cameraId identifier of the camera to configure
    /// @param ws the workspace to store
    void setCameraWorkspace(const QString &cameraId, const CameraWorkspace &ws) {
        d->m_cameraWorkspaces.setWorkspace(cameraId, ws);
    }

public:
    QSharedDataPointer<TaskLocalizeConfigPrivate> d;  ///< Implicitly-shared, copy-on-write private data.
};


} // namespace vc::model


#endif // TASK_LOCALIZATION_CONFIG_H
