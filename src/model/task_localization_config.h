#ifndef TASK_LOCALIZATION_CONFIG_H
#define TASK_LOCALIZATION_CONFIG_H

#include <QSharedData>
#include <QJsonObject>

#include "model/itask_config.h"
#include "model/task_device_binding.h"
#include "model/camera_workspace.h"
#include "device/robot_kinematic_check_config.h"
#include "matching/pattern_group_manager.h"
#include "core/qgadget_macro.h"
#include "core/logger/app_logger.h"

/**
 * @file task_localization_config.h
 * @brief Task model layer: per-task configuration types (e.g. TaskLocalizeConfig), state
 *        machines, and device/camera bindings for the picking application.
 */
namespace vc::model {

/**
 * @class TaskLocalizeConfigPrivate
 * @brief Implicitly-shared private data (QSharedData) backing TaskLocalizeConfig: holds the
 *        PLC signal-name bindings and per-camera workspace ROIs for a localization task.
 */
class TaskLocalizeConfigPrivate : public QSharedData {
public:
    /// Default-constructs with all signal-name strings empty and no device bindings/workspaces.
    TaskLocalizeConfigPrivate() {}

    /// Copies all signal-name bindings and workspace data from `other`.
    TaskLocalizeConfigPrivate(const TaskLocalizeConfigPrivate &other) = default;

    // number signals
    QString m_nActiveCamera;        ///< PLC signal name bound to the active-camera number.
    QString m_nActivePatternGroup;  ///< PLC signal name bound to the active pattern-group number.
    /// PLC signal name the runtime REPORTS the adopted camera number on. Optional.
    ///
    /// Separate from m_nActiveCamera because that one is a **command register the master owns**.
    /// The runtime used to echo the accepted number straight back onto it, which is a write race
    /// against the master on a server binding and is refused outright on a client binding whose
    /// command registers are input registers (backlog item 60).
    QString m_nActiveCameraStatus;
    /// PLC signal name the runtime REPORTS the adopted pattern group on. Optional; see
    /// m_nActiveCameraStatus.
    QString m_nActivePatternGroupStatus;
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
    QString m_bErrorReset;          ///< PLC signal name bound to the fault-acknowledge input.

    TaskDeviceBindings m_deviceBindings;    ///< Device (PLC) bindings configured for this task.
    CameraWorkspaceMap m_cameraWorkspaces;  ///< Per-camera workspace (ROI) definitions.
    /// Robot reachability/pick-path gate the runtime applies to every candidate. Owned by the
    /// TASK, not by whichever device happens to carry the vision_output role — see
    /// TaskLocalizeConfig::robotCheckConfig().
    vc::device::RobotKinematicCheckConfig m_robotCheckConfig;
};

/**
 * @class TaskLocalizeConfig
 * @brief Localization-task configuration: PLC signal-name bindings for camera/pattern selection
 *        and matching status flags, plus per-camera workspace (ROI) definitions.
 *
 * Exposed as a Q_GADGET so its bound signal names can be edited via the property-browser UI.
 */
class TaskLocalizeConfig : public ITaskConfig {
    Q_GADGET

    /// PLC signal name bound to the active-camera selection ("Camera selection").
    P_PROPERTY_STRING_READWRITE(QString, nActiveCamera, "Camera selection")
    /// PLC signal name bound to the active pattern-group selection ("Pattern group selection").
    P_PROPERTY_STRING_READWRITE(QString, nActivePatternGroup, "Pattern group selection")
    /// PLC signal name the runtime reports the adopted camera on ("Active camera (status)").
    /// Optional: left empty, the value is still published to the UI but never written to the PLC.
    P_PROPERTY_STRING_READWRITE(QString, nActiveCameraStatus, "Active camera (status)")
    /// PLC signal name the runtime reports the adopted pattern group on
    /// ("Active pattern group (status)"). Optional; see nActiveCameraStatus.
    P_PROPERTY_STRING_READWRITE(QString, nActivePatternGroupStatus, "Active pattern group (status)")
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
    /// PLC signal name bound to the fault-acknowledge input ("Error reset"). Rising edge
    /// clears bTaskFault/nFaultCode and re-arms the runtime; leaving it unbound is valid,
    /// because a latched cycle fault also clears itself after
    /// LocalizationRuntimeController::kFaultAutoRecoverMs.
    P_PROPERTY_STRING_READWRITE(QString, bErrorReset, "Error reset")

    /// Translation markers for the display names above. **Not read by any code.**
    ///
    /// Those names reach the UI through Q_CLASSINFO, and `lupdate` does not read
    /// Q_CLASSINFO — so without this table they never enter the .ts and can never be
    /// translated, no matter how the translation pipeline is run. Marking them at the macro
    /// call site is not possible either: moc rejects QT_TRANSLATE_NOOP inside Q_CLASSINFO,
    /// and putting it in the macro definition is invisible because lupdate does not expand
    /// user macros. Both were measured, not assumed — see the Phase 7 / E6 plan.
    ///
    /// The context MUST be this class's `staticMetaObject.className()`, because that is what
    /// `vc::gadget_meta::displayName()` passes to QCoreApplication::translate(). A different
    /// spelling compiles, extracts, translates — and is never found at runtime.
    ///
    /// Every string here must appear exactly once above, and vice versa. The architecture
    /// contract test asserts both directions; adding a property without a marker fails it.
    static inline constexpr const char *const kDisplayNameSources[] = {
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Camera selection"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Pattern group selection"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Active camera (status)"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Active pattern group (status)"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Detected number"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Fault code"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Camera ready"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Pattern ready"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Task ready"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Trigger"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Finished"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Busy"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Detected"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Low Area"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Task fault"),
        QT_TRANSLATE_NOOP("vc::model::TaskLocalizeConfig", "Error reset"),
    };

public:
    /// Constructs a config with all signal-name bindings empty and default (empty) device
    /// bindings/workspaces.
    explicit TaskLocalizeConfig()
        : ITaskConfig(), d(new TaskLocalizeConfigPrivate()) {}

    /// Persistence schema version. Bump when the on-disk shape changes in a way
    /// older readers cannot parse; add migration logic in fromJson(). A document
    /// with no "version" key is the pre-versioning legacy baseline (version 0).
    ///
    /// History:
    ///   0 — pre-versioning baseline (no "version" key).
    ///   1 — versioned; 13 signal bindings.
    ///   2 — adds "bErrorReset". A v1 document loads unchanged (the key is absent,
    ///       so the acknowledge input is simply unbound). The bump exists so a v1-era
    ///       build refuses a v2 document outright instead of loading it with the
    ///       acknowledge silently dropped.
    ///   3 — adds the two active-selection status tags. A v2 document loads with both
    ///       unbound, which is a supported configuration. A v3 document is refused by a
    ///       v2-era build, which is the point of the bump: that build would echo the
    ///       adopted index back onto the master's command register (backlog item 60).
    ///   4 — adds "robotCheckConfig". A v3 document loads with the check DISABLED, which
    ///       is the same behaviour a v3-era build produced whenever the bound output device
    ///       was not of the vision-output family. The bump exists because a v3-era build
    ///       reading a v4 document would ignore the key and go back to reading the check
    ///       off the device — i.e. silently un-commission a station that was signed off
    ///       with reachability checking (backlog item 57).
    static constexpr int kSchemaVersion = 4;

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
        obj["nActiveCameraStatus"] = d->m_nActiveCameraStatus;
        obj["nActivePatternGroupStatus"] = d->m_nActivePatternGroupStatus;
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
        obj["bErrorReset"]       = d->m_bErrorReset;

        obj["deviceBindings"]   = d->m_deviceBindings.toJson();
        obj["cameraWorkspaces"] = d->m_cameraWorkspaces.toJson();
        obj["robotCheckConfig"] = d->m_robotCheckConfig.toJson();

        return obj;
    }


    /**
     * @brief Loads all signal-name bindings, device bindings, and camera workspaces from `obj`.
     *        Rejects documents whose "version" is newer than kSchemaVersion; a missing
     *        "version" key is treated as the legacy pre-versioning baseline. Camera workspaces
     *        are optional and tolerate a parse failure (treated as "no workspaces").
     * @param[in] obj the JSON object to load (as produced by toJson())
     * @return false if `obj` is empty, the version is unsupported, or device bindings fail
     *         to parse; true otherwise
     */
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
        // Absent in v0..v2 documents; an empty tag means the runtime reports the adopted index
        // to the UI only and writes nothing to the PLC, which is a supported configuration.
        d->m_nActiveCameraStatus = obj["nActiveCameraStatus"].toString("");
        d->m_nActivePatternGroupStatus = obj["nActivePatternGroupStatus"].toString("");
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
        // Absent in v0/v1 documents; an empty tag simply means the acknowledge input
        // is unbound, which is a supported configuration (see kSchemaVersion).
        d->m_bErrorReset        = obj["bErrorReset"].toString("");

        if (!d->m_deviceBindings.fromJson(obj["deviceBindings"])) {
            return false;
        }

        // Workspace map is optional (older projects predate it); a parse error
        // is tolerated as "no workspaces" rather than failing the whole load.
        d->m_cameraWorkspaces.fromJson(obj["cameraWorkspaces"]);

        // Absent in v0..v3 documents. RobotKinematicCheckConfig::fromJson() defaults every
        // field, so a missing key loads as "check disabled" — the safe reading, and the one
        // a v3-era project already behaved as whenever the output role was not a
        // vision-output device.
        d->m_robotCheckConfig.fromJson(obj["robotCheckConfig"].toObject());

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
    /**
     * @brief Returns the workspace (ROI) configured for the given camera.
     * @param[in] cameraId identifier of the camera whose workspace to look up
     * @return the camera's workspace, or a default-constructed (workspace-off) CameraWorkspace
     *         if `cameraId` has none configured
     */
    CameraWorkspace cameraWorkspace(const QString &cameraId) const {
        return d->m_cameraWorkspaces.workspace(cameraId);
    }
    /**
     * @brief Sets (or replaces) the workspace (ROI) for the given camera.
     * @param[in] cameraId identifier of the camera to configure
     * @param[in] ws       the workspace to store
     */
    void setCameraWorkspace(const QString &cameraId, const CameraWorkspace &ws) {
        d->m_cameraWorkspaces.setWorkspace(cameraId, ws);
    }

    // ── Robot pick check ──────────────────────────────────────────────────
    /**
     * @brief Returns the robot reachability/pick-path gate commissioned on this TASK.
     *
     * This is what `TaskLocalization::buildRuntimeContext()` reads. It used to come from the
     * device bound to the `vision_output` role, through
     * `IResultOutputDevice::robotKinematicCheckConfig()` — which meant the setting belonged to
     * a *transport*. Binding a PLC to that role then left the check at its default (disabled)
     * with nothing said, so a cell commissioned with reachability checking quietly stopped
     * doing it (backlog item 57).
     *
     * @note Deliberately **not** a `P_PROPERTY_*`. The value carries a nested
     *       `QVector<PickPathPoint>` that the property browser cannot render, so a property
     *       would add a display name, a `kDisplayNameSources[]` entry and `totalNames` churn
     *       for a control nobody could use. It is edited by `RobotKinematicCheckWidget`
     *       instead.
     */
    vc::device::RobotKinematicCheckConfig robotCheckConfig() const {
        return d->m_robotCheckConfig;
    }
    /// Replaces the task's robot pick-check settings. See robotCheckConfig().
    void setRobotCheckConfig(const vc::device::RobotKinematicCheckConfig &cfg) {
        d->m_robotCheckConfig = cfg;
    }

public:
    QSharedDataPointer<TaskLocalizeConfigPrivate> d;  ///< Implicitly-shared, copy-on-write private data.
};


} // namespace vc::model


#endif // TASK_LOCALIZATION_CONFIG_H
