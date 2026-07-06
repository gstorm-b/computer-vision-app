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

// ---------------------------------------------------------------------------
// CameraWorkspaceWidget — per-camera workspace (ROI) row list.
//
// One row per mapped camera: a "use workspace" checkbox, a status label, and a
// "Set workspace…" button. Model-free on purpose: the owner feeds the camera
// id -> display-name map and per-camera state via primitives, and reacts to
// toggles / set-requests through signals. Keeping it free of vc::model types
// lets it stay a reusable widget under src/widgets/ (mirrors
// CameraMappingWidget).
// ---------------------------------------------------------------------------
class CameraWorkspaceWidget : public QWidget {
    Q_OBJECT

public:
    explicit CameraWorkspaceWidget(QWidget *parent = nullptr);

    // Rebuild one row per camera (ordered by display name). After rebuilding,
    // re-apply each camera's state via setWorkspaceState().
    void setCameras(const QMap<QString, QString> &idToName);

    // Update the visible state of one camera's row. Both ROIs are in image-pixel
    // coordinates; an empty roi renders as "Not set". A single "Set workspace…"
    // button edits both ROIs on the same reference image.
    void setWorkspaceState(const QString &cameraId,
                           bool useWorkspace, const QRectF &roi,
                           bool useCondition, const QRectF &conditionRoi,
                           bool hasImage);

signals:
    void useWorkspaceToggled(const QString &cameraId, bool enabled);
    void useConditionToggled(const QString &cameraId, bool enabled);
    void setWorkspaceRequested(const QString &cameraId);

private:
    struct Row {
        QWidget     *container{nullptr};
        QCheckBox   *useCheck{nullptr};
        QLabel      *statusLabel{nullptr};
        QPushButton *setButton{nullptr};
        QCheckBox   *conditionCheck{nullptr};
        QLabel      *conditionStatus{nullptr};
    };

    void clearRows();

    QVBoxLayout           *m_rowsLayout{nullptr};
    QMap<QString, QString> m_idToName;   // id -> display name
    QMap<QString, Row>     m_rows;       // id -> row
};

#endif // CAMERA_WORKSPACE_WIDGET_H
