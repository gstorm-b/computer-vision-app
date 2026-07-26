#ifndef CAMERA_WORKSPACE_H
#define CAMERA_WORKSPACE_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QString>

#include <opencv2/core.hpp>

/// Per-camera workspace ROI model types: CameraWorkspace (working + condition crop regions) and
/// CameraWorkspaceMap, its camera-id-keyed container with JSON (de)serialization and
/// reference-image BLOB helpers.
namespace vc::model {

/// Per-camera workspace (ROI) used to crop the grabbed image before matching.
///
/// Keyed by camera device id (stable across logical camera-number
/// reassignment). The reference image (the last image used to define the ROI)
/// is kept in memory so the workspace dialog can reopen it; it is persisted
/// out-of-band as a BLOB through the project_images table (see
/// CameraWorkspaceMap::imageKey). Only the ROI rect + flag are serialized to
/// JSON.
///
/// Default is "workspace off": matching runs on the full grabbed frame.
struct CameraWorkspace {
    /// Working workspace — crops the grabbed frame before matching.
    bool       useWorkspace = false;          ///< false => match the full frame
    cv::Rect2f roi{0.0f, 0.0f, 0.0f, 0.0f};   ///< image-pixel coordinates

    /// Condition workspace — task-dependent region of interest. Unlike the working workspace
    /// (which crops the input), the condition workspace is interpreted by the owning task. For
    /// a localization task it is the region used to filter which detected objects count as
    /// valid. Defined in the same frame's pixel coordinates as the working ROI and shares this
    /// camera's reference image. LocalizationPipeline passes it to
    /// ImageMatcher::setMatchingConditionROI() when enabled.
    bool       useConditionWorkspace = false;   ///< enables the condition ROI described above
    cv::Rect2f conditionRoi{0.0f, 0.0f, 0.0f, 0.0f};   ///< image-pixel coordinates

    cv::Mat    referenceImage;                ///< in-memory only; not in JSON

    /// Returns true if the working ROI has a positive width and height.
    bool hasRoi() const { return roi.width > 0.0f && roi.height > 0.0f; }
    /// Returns true if the condition ROI has a positive width and height.
    bool hasConditionRoi() const
    {
        return conditionRoi.width > 0.0f && conditionRoi.height > 0.0f;
    }
};

/// Container of per-camera workspaces, keyed by camera device id.
class CameraWorkspaceMap {
public:
    /// Returns true if no camera has a workspace entry.
    bool isEmpty() const { return m_workspaces.isEmpty(); }
    /// Returns true if `cameraId` has a workspace entry.
    bool contains(const QString &cameraId) const { return m_workspaces.contains(cameraId); }
    /// Returns the camera device ids that have a workspace entry.
    QList<QString> cameraIds() const { return m_workspaces.keys(); }

    /// Returns the workspace for `cameraId`, or a default-constructed CameraWorkspace
    /// ("workspace off") if none is set.
    CameraWorkspace workspace(const QString &cameraId) const
    {
        return m_workspaces.value(cameraId);
    }

    /// Sets (or replaces) the workspace for `cameraId`; a no-op if `cameraId` is empty.
    void setWorkspace(const QString &cameraId, const CameraWorkspace &ws)
    {
        if (cameraId.isEmpty()) return;
        m_workspaces.insert(cameraId, ws);
    }

    /// Removes the workspace entry for `cameraId`, if any.
    void remove(const QString &cameraId) { m_workspaces.remove(cameraId); }

    /// Serializes all workspaces to a JSON array — ROI + flag only; the reference image travels
    /// separately as a BLOB (see getImageMap()).
    /// @return one JSON object per camera with "cameraId", "useWorkspace", "roi",
    ///         "useConditionWorkspace", and "conditionRoi" fields
    QJsonArray toJson() const
    {
        QJsonArray arr;
        for (auto it = m_workspaces.cbegin(); it != m_workspaces.cend(); ++it) {
            const CameraWorkspace &ws = it.value();
            QJsonObject obj;
            obj["cameraId"]     = it.key();
            obj["useWorkspace"] = ws.useWorkspace;
            obj["roi"] = QJsonObject{{ "x", ws.roi.x },     { "y", ws.roi.y },
                                     { "w", ws.roi.width },  { "h", ws.roi.height }};
            obj["useConditionWorkspace"] = ws.useConditionWorkspace;
            obj["conditionRoi"] = QJsonObject{
                { "x", ws.conditionRoi.x },     { "y", ws.conditionRoi.y },
                { "w", ws.conditionRoi.width }, { "h", ws.conditionRoi.height }};
            arr.append(obj);
        }
        return arr;
    }

    /// Rebuilds all workspaces (ROI + flags only) from a JSON array produced by toJson();
    /// clears any existing entries first. Reference images are not restored by this call.
    /// @param value expected to be a JSON array, or null/undefined for "no workspaces"
    /// @return true on success; false if `value` is neither null/undefined nor an array. Array
    ///         entries with an empty "cameraId" are silently skipped.
    bool fromJson(const QJsonValue &value)
    {
        m_workspaces.clear();
        if (value.isUndefined() || value.isNull())
            return true;
        if (!value.isArray())
            return false;

        for (const QJsonValue &item : value.toArray()) {
            const QJsonObject obj = item.toObject();
            const QString cameraId = obj["cameraId"].toString();
            if (cameraId.isEmpty())
                continue;

            CameraWorkspace ws;
            ws.useWorkspace = obj["useWorkspace"].toBool(false);
            const QJsonObject r = obj["roi"].toObject();
            ws.roi = cv::Rect2f(static_cast<float>(r["x"].toDouble(0.0)),
                                static_cast<float>(r["y"].toDouble(0.0)),
                                static_cast<float>(r["w"].toDouble(0.0)),
                                static_cast<float>(r["h"].toDouble(0.0)));

            ws.useConditionWorkspace = obj["useConditionWorkspace"].toBool(false);
            const QJsonObject cr = obj["conditionRoi"].toObject();
            ws.conditionRoi = cv::Rect2f(static_cast<float>(cr["x"].toDouble(0.0)),
                                         static_cast<float>(cr["y"].toDouble(0.0)),
                                         static_cast<float>(cr["w"].toDouble(0.0)),
                                         static_cast<float>(cr["h"].toDouble(0.0)));
            m_workspaces.insert(cameraId, ws);
        }
        return true;
    }

    /// Builds the project_images BLOB key ("ws_{cameraId}") used to persist a workspace's
    /// reference image out-of-band from the JSON.
    static QString imageKey(const QString &cameraId)
    {
        return QStringLiteral("ws_") + cameraId;
    }

    /// Parses a BLOB key produced by imageKey() back into its camera id.
    /// @param key candidate BLOB key
    /// @param cameraId set to the extracted camera id on success
    /// @return true if `key` starts with the "ws_" prefix and the remainder is non-empty
    static bool parseImageKey(const QString &key, QString &cameraId)
    {
        if (!key.startsWith(QStringLiteral("ws_")))
            return false;
        cameraId = key.mid(3);
        return !cameraId.isEmpty();
    }

    /// Collects the in-memory reference image of every camera that has one, keyed by
    /// imageKey(cameraId), for persisting as project_images BLOBs.
    /// @return map of BLOB key -> reference image; cameras with no reference image are omitted
    QMap<QString, cv::Mat> getImageMap() const
    {
        QMap<QString, cv::Mat> map;
        for (auto it = m_workspaces.cbegin(); it != m_workspaces.cend(); ++it) {
            if (!it.value().referenceImage.empty())
                map.insert(imageKey(it.key()), it.value().referenceImage);
        }
        return map;
    }

    /// Sets (or replaces) the in-memory reference image for `cameraId`; creates a default
    /// workspace entry if `cameraId` had none. A no-op if `cameraId` is empty.
    void setReferenceImage(const QString &cameraId, const cv::Mat &image)
    {
        if (cameraId.isEmpty()) return;
        m_workspaces[cameraId].referenceImage = image;   // creates entry if absent
    }

private:
    QMap<QString, CameraWorkspace> m_workspaces;  ///< cameraId -> workspace
};

} // namespace vc::model

Q_DECLARE_METATYPE(vc::model::CameraWorkspace)

#endif // CAMERA_WORKSPACE_H
