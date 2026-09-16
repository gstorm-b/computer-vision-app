#ifndef JAI_CAM_SELECT_DIALOG_H
#define JAI_CAM_SELECT_DIALOG_H

#include <QDialog>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QThread>

/**
 * @file jai_cam_select_dialog.h
 * @brief JaiDeviceScanWorker (eBUS GigE discovery thread), JaiCameraInfo (its plain result)
 *        and JaiCamSelectDialog (the camera-picker dialog built on them).
 */

namespace Ui {
/// Forward declaration of the Qt Designer-generated UI form for JaiCamSelectDialog.
class JaiCamSelectDialog;
}

/**
 * @struct JaiCameraInfo
 * @brief One discovered GigE Vision camera, flattened out of the SDK's `PvDeviceInfoGEV`.
 *
 * @warning Plain data on purpose. `PvDeviceInfo` is owned by the `PvSystem` that found it and dies
 * with that object, so carrying a pointer to it out of the scan — or across a queued signal —
 * would be a dangling read by the time the dialog rendered it. The Basler dialog can pass its
 * `Pylon::CDeviceInfo` by value because Pylon's is a value type; eBUS's is not.
 */
struct JaiCameraInfo {
    QString modelName;
    QString vendorName;
    QString userDefinedName;
    QString serialNumber;
    QString macAddress;
    QString ipAddress;
    QString subnetMask;
    QString connectionId;
    bool configurationValid{false};   ///< False when the camera has no usable IP on this subnet.
};

Q_DECLARE_METATYPE(JaiCameraInfo)

/**
 * @class JaiDeviceScanWorker
 * @brief Background QThread that enumerates reachable GigE Vision cameras through the eBUS SDK.
 *
 * Discovery blocks for as long as the SDK's detection timeout, which is why it is not run on the
 * GUI thread. Results land in `cameras` before resultReady() is emitted.
 */
class JaiDeviceScanWorker : public QThread {
    Q_OBJECT

public:
    explicit JaiDeviceScanWorker(QObject *parent = nullptr) : QThread(parent) {}

protected:
    /// QThread entry point: runs a PvSystem discovery pass and fills `cameras`.
    void run() override;

signals:
    /// Emitted once run() has finished filling `cameras`.
    void resultReady();

public:
    /// Cameras found by the most recent run(); written on the worker thread, read on the GUI
    /// thread only after resultReady().
    QList<JaiCameraInfo> cameras;
};

/**
 * @class JaiCamSelectDialog
 * @brief Modal dialog listing reachable JAI/GigE Vision cameras and letting the user pick one.
 */
class JaiCamSelectDialog : public QDialog {
    Q_OBJECT

public:
    explicit JaiCamSelectDialog(QWidget *parent = nullptr);
    ~JaiCamSelectDialog();

    /// One-time setup: configures the table, wires the signals and starts the first scan.
    void initForm();
    /// Re-shows the dialog for a fresh selection, restarting the scan.
    void showCameraSelectForm();

signals:
    /**
     * @brief Emitted when the dialog closes, reporting the user's choice.
     * @param[in] isAccept true if a camera was picked and confirmed
     * @param[in] camera   the selected camera when isAccept is true; default-constructed otherwise
     */
    void userSelectionFinished(bool isAccept, JaiCameraInfo camera);

private:
    void btn_refresh_clicked();
    void btn_cancel_clicked();
    void btn_select_confirm_clicked();
    /// On accept with a valid row, emits userSelectionFinished(true, ...); otherwise (false, {}).
    void dialogClosing(int state);
    /// Recomputes the selection state and enables/disables the confirm button to match.
    void tableViewSelectionChanged();
    /// Shows a scanning message and disables Refresh until the scan completes.
    void cameraWorkerStart();
    /// Copies the discovered cameras into m_cameras and rebuilds the table.
    void cameraListCame();
    /// Removes every row from the camera table.
    void clearCameraTableView();
    /// Appends one row rendered from `info`.
    void cameraTableViewAddNewRow(const JaiCameraInfo &info);

private:
    Ui::JaiCamSelectDialog *ui;              ///< Generated UI form.
    JaiDeviceScanWorker *m_scanThread{nullptr};  ///< Background discovery; child of this dialog.

    QList<JaiCameraInfo> m_cameras;          ///< Last scan result; indices match table rows.

    bool m_is_selected{false};               ///< True when the current row is a valid selection.
    int m_current_select_row{-1};            ///< Selected row index, or -1.

    /// Column headers for the camera table.
    const QStringList table_headers {
        tr("Name"),
        tr("Device User ID"),
        tr("Serial Number"),
        tr("IP Address"),
        tr("MAC Address"),
        tr("Status"),
        tr("Subnet mask"),
    };
};

#endif // JAI_CAM_SELECT_DIALOG_H
