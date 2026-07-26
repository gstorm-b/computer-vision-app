#ifndef BASLER_CAM_SELECT_DIALOG_H
#define BASLER_CAM_SELECT_DIALOG_H

#include <QDialog>
#include <QThread>
#include <pylon/PylonIncludes.h>
#include <pylon/gige/GigETransportLayer.h>

/// Forward declaration of the Qt Designer–generated UI form for BaslerCamSelectDialog.
namespace Ui {
class BaslerCamSelectDialog;
}

/// Background QThread that enumerates all reachable Basler GigE cameras via the Pylon
/// transport layer and stores them in cameraDeviceList before emitting resultReady().
class GetDevicesWorker: public QThread {
    Q_OBJECT

public:
    /// Constructs the worker thread; no enumeration happens until run() executes.
    explicit GetDevicesWorker(QObject *parent = nullptr) :
        QThread(parent) {

    }

protected:
    /// QThread entry point: creates the Basler GigE transport layer and enumerates all
    /// reachable devices into cameraDeviceList, then emits resultReady(). Logs and returns
    /// early if the transport layer cannot be created.
    void run() override {
        Pylon::ITransportLayer* tl = Pylon::CTlFactory::GetInstance().CreateTl("BaslerGigE");
        Pylon::IGigETransportLayer* gigetl = dynamic_cast<Pylon::IGigETransportLayer*>(tl);
        if (!gigetl) {
            qCritical() << "[Camera abstract] Cannot create transport layer";
            return;
        }
        gigetl->EnumerateAllDevices(cameraDeviceList, false);
        // qDebug() << "Number of detected cameras:" << cameraDeviceList.size();
        emit resultReady();
    }

signals:
    /// Emitted once run() has finished enumerating devices into cameraDeviceList.
    void resultReady();

public:
    Pylon::DeviceInfoList cameraDeviceList;  ///< Devices found by the most recent run(); populated on the worker thread, read on the GUI thread after resultReady().
};

/// Modal dialog that lists reachable Basler GigE cameras (queried asynchronously via
/// GetDevicesWorker) in a table and lets the user pick one to use.
class BaslerCamSelectDialog : public QDialog {
    Q_OBJECT

public:
    /// Constructs the dialog: sets up ui and calls initForm().
    explicit BaslerCamSelectDialog(QWidget *parent = nullptr);
    /// Terminates get_cam_thread if it is still running, then destroys ui.
    ~BaslerCamSelectDialog();

    /// One-time setup: configures the camera table (single-row selection, 7 stretched
    /// columns, non-editable), wires all button/table/dialog signals, and starts
    /// get_cam_thread if it is not already running.
    void initForm();
    /// Re-shows the dialog for a fresh selection: restarts get_cam_thread if it is not
    /// running, resets the selection state, disables the confirm button, and calls show().
    void showCameraSelectForm();

private:
    /// Slot for btn_refresh: restarts get_cam_thread if it is not already running.
    void btn_refresh_clicked();
    /// Slot for btn_cancel: rejects the dialog.
    void btn_cancel_clicked();
    /// Slot for btn_select_confirm: accepts the dialog.
    void btn_select_confirm_clicked();
    /// Slot for QDialog::finished(): if accepted with a valid row selected, emits
    /// userSelectionFinished(true, ...) with the chosen camera's info; otherwise emits
    /// userSelectionFinished(false, {}).
    /// @param state the QDialog::DialogCode the dialog finished with
    void dialogClosing(int state);
    /// Slot for the table's itemSelectionChanged: recomputes m_current_select_row and
    /// m_is_selected from the table's current row, and enables/disables
    /// btn_select_confirm to match.
    void tableViewSelectionChanged();
    /// Slot for get_cam_thread's started signal: shows a querying-in-progress status
    /// message and disables btn_refresh until the query completes.
    void cameraWorkerStart();
    /// Slot for get_cam_thread's resultReady signal: updates the status label,
    /// re-enables btn_refresh, copies the discovered devices into cameraDeviceList, and
    /// rebuilds the table.
    void cameraListCame();
    /// Removes all rows from the camera table widget.
    void clearCameraTableView();
    /// Appends one row to the camera table populated from `info` (model name,
    /// user-defined ID, serial number, IP/MAC address, subnet mask, and an
    /// accessibility-derived status of "Ok"/"In use").
    /// @param info the Pylon device info to render as a new row
    void cameraTableViewAddNewRow(Pylon::CDeviceInfo &info);

    /// Returns whether `info`'s device is currently accessible (i.e. not already opened
    /// elsewhere), via the Pylon transport-layer factory.
    inline bool isCameraDeviceAccessible(Pylon::CDeviceInfo &info) {
        return Pylon::CTlFactory::GetInstance().IsDeviceAccessible(info);
    }

signals:
    /// Emitted when the dialog closes, reporting the user's choice.
    /// @param isAccept true if a camera was picked and confirmed; false if cancelled or no valid selection
    /// @param devices the selected camera's info when isAccept is true; default-constructed otherwise
    void userSelectionFinished(bool isAccept, Pylon::CDeviceInfo devices);

private:
    Ui::BaslerCamSelectDialog *ui;  ///< Qt Designer–generated UI form for this dialog.
    GetDevicesWorker *get_cam_thread;  ///< Background thread that enumerates Basler GigE cameras for the table.

    Pylon::DeviceInfoList cameraDeviceList;  ///< Last enumerated camera list; indices correspond to table rows.

    bool m_is_selected{false};  ///< True when the current table row is a valid selection.
    int m_current_select_row{-1};  ///< Index of the currently selected table row, or -1 if none.

    /// Column headers for the camera table widget.
    const QStringList table_headers {
        tr("Name"),
        tr("Device User ID"),
        tr("Serial Number"),
        tr("IP Address"),
        tr("MAC Address"),
        tr("Status"),
        tr("Subnet mark"),
    };
};

#endif // BASLER_CAM_SELECT_DIALOG_H
