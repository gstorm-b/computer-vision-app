#ifndef CAMERA_WORKSPACE_WIDGET_H
#define CAMERA_WORKSPACE_WIDGET_H

#include <QMap>
#include <QRectF>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class QCheckBox;
class QPushButton;
class QLabel;

/// Per-camera workspace (ROI) row list. One row per mapped camera: a "use
/// workspace" checkbox, a status label, and a "Set workspace…" button, plus a
/// second "use as condition" checkbox/status line. Model-free on purpose: the
/// owner feeds the camera id -> display-name map and per-camera state via
/// primitives, and reacts to toggles / set-requests through signals. Keeping
/// it free of vc::model types lets it stay a reusable widget under
/// src/widgets/ (mirrors CameraMappingWidget).
class CameraWorkspaceWidget : public QWidget {
    Q_OBJECT

public:
    /// Builds an empty row layout (rows are added by setCameras()).
    explicit CameraWorkspaceWidget(QWidget *parent = nullptr);

    /// Rebuilds one row per camera, ordered by display name (id as tie-breaker).
    /// Discards any existing rows; callers must re-apply each camera's state
    /// via setWorkspaceState() afterward.
    void setCameras(const QMap<QString, QString> &idToName);

    /// Updates the visible state of one camera's row (no-op if `cameraId` has
    /// no row). Both ROIs are in image-pixel coordinates; an empty roi renders
    /// as "Not set", and the status text notes "(no image)" when `hasImage` is
    /// false. A single "Set workspace…" button edits both ROIs on the same
    /// reference image. Checkbox states are set without emitting their toggle signals.
    void setWorkspaceState(const QString &cameraId,
                           bool useWorkspace, const QRectF &roi,
                           bool useCondition, const QRectF &conditionRoi,
                           bool hasImage);

signals:
    /// Emitted when the "use workspace" checkbox for `cameraId` is toggled by the user.
    void useWorkspaceToggled(const QString &cameraId, bool enabled);
    /// Emitted when the "use as condition" checkbox for `cameraId` is toggled by the user.
    void useConditionToggled(const QString &cameraId, bool enabled);
    /// Emitted when the "Set workspace…" button for `cameraId` is clicked.
    void setWorkspaceRequested(const QString &cameraId);

private:
    /// Widgets making up a single camera row.
    struct Row {
        QWidget     *container{nullptr};      ///< Top-level row container added to m_rowsLayout.
        QCheckBox   *useCheck{nullptr};        ///< "Use workspace" checkbox, labeled with the camera's display name.
        QLabel      *statusLabel{nullptr};     ///< Shows the working-ROI status text (or "Not set").
        QPushButton *setButton{nullptr};       ///< "Set workspace…" button; emits setWorkspaceRequested().
        QCheckBox   *conditionCheck{nullptr};  ///< "Use as condition" checkbox.
        QLabel      *conditionStatus{nullptr}; ///< Shows the condition-ROI status text (or "Not set").
    };

    /// Removes and schedules deletion of all current row widgets, clearing m_rows.
    void clearRows();

    QVBoxLayout           *m_rowsLayout{nullptr}; ///< Vertical layout holding all rows above a trailing stretch.
    QMap<QString, QString> m_idToName;   ///< id -> display name
    QMap<QString, Row>     m_rows;       ///< id -> row
};

#endif // CAMERA_WORKSPACE_WIDGET_H
