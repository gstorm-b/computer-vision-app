#ifndef WORKSPACE_SETTING_DIALOG_H
#define WORKSPACE_SETTING_DIALOG_H

#include <QDialog>
#include <QPixmap>
#include <QRectF>
#include <QString>

#include "ui/widgets/image_widget/image_widget.h"
#include "ui/widgets/image_widget/item_roi.h"

namespace Ui {
class WorkspaceSettingDialog;
}

// ---------------------------------------------------------------------------
// WorkspaceSettingDialog — define up to two axis-aligned ROIs on one reference
// image: the working ROI (crop region, drawn green) and the condition ROI
// (task-dependent region, drawn orange). Both are optional and live in the same
// image's pixel coordinates.
//
// The image comes from a file ("Choose image") or a camera grab ("Grab", which
// the owner serves by handling requestGrab() and calling setMainViewImage()).
// On accept, resultImage() holds the image the ROIs were drawn on, resultRoi()
// holds the working ROI and resultConditionRoi() the condition ROI (each empty
// when not drawn), all in image-pixel coordinates.
//
// Mirrors AddPatternImageDialog: the ImageWidget is embedded in code; the .ui
// only carries the buttons + the host stack.
// ---------------------------------------------------------------------------
class WorkspaceSettingDialog : public QDialog {
    Q_OBJECT

public:
    explicit WorkspaceSettingDialog(QWidget *parent = nullptr);
    ~WorkspaceSettingDialog() override;

    void setTitleText(const QString &title);

    // Preload an existing workspace. image may be null; either roi may be empty.
    void setInitial(const QPixmap &image, const QRectF &workingRoi,
                    const QRectF &conditionRoi);

    bool    hasResult()          const { return m_hasResult; }
    QPixmap resultImage()        const { return m_resultImage; }
    QRectF  resultRoi()          const { return m_resultRoi; }          // working
    QRectF  resultConditionRoi() const { return m_resultConditionRoi; }

public slots:
    // Supplied by the owner after a requestGrab() (or any external image source).
    void setMainViewImage(QPixmap image);

signals:
    void requestGrab();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class RoiKind { None, Working, Condition };

    void onChooseImageClicked();
    void onGrabClicked();
    void onSetWorkingRoiClicked();
    void onClearWorkingRoiClicked();
    void onSetConditionRoiClicked();
    void onClearConditionRoiClicked();
    void onSaveClicked();

    // Capture a freshly created ROI (drawn or preloaded) into the pending kind.
    void onRoiCreated(QGraphicsItem *item);
    // Apply the kind-specific bounding colour (working = green, condition = orange).
    void styleRoi(ItemRoi *roi, RoiKind kind);
    // Drop all ROIs and reset tracked pointers (used when the image changes).
    void resetRois();
    void updateButtons();

    Ui::WorkspaceSettingDialog *ui;
    ImageWidget *m_view{nullptr};
    QString      m_lastDir;

    RoiKind  m_pendingKind{RoiKind::None};
    ItemRoi *m_workingRoi{nullptr};
    ItemRoi *m_conditionRoi{nullptr};

    bool    m_hasResult{false};
    QPixmap m_resultImage;
    QRectF  m_resultRoi;
    QRectF  m_resultConditionRoi;
};

#endif // WORKSPACE_SETTING_DIALOG_H
