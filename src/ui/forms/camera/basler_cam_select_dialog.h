#ifndef BASLER_CAM_SELECT_DIALOG_H
#define BASLER_CAM_SELECT_DIALOG_H

#include <QDialog>
#include <QThread>
#include <pylon/PylonIncludes.h>
#include <pylon/gige/GigETransportLayer.h>

namespace Ui {
class BaslerCamSelectDialog;
}

class GetDevicesWorker: public QThread {
    Q_OBJECT

public:
    explicit GetDevicesWorker(QObject *parent = nullptr) :
        QThread(parent) {

    }

protected:
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
    void resultReady();

public:
    Pylon::DeviceInfoList cameraDeviceList;
};

class BaslerCamSelectDialog : public QDialog {
    Q_OBJECT

public:
    explicit BaslerCamSelectDialog(QWidget *parent = nullptr);
    ~BaslerCamSelectDialog();

    void initForm();
    void showCameraSelectForm();

private:
    void btn_refresh_clicked();
    void btn_cancel_clicked();
    void btn_select_confirm_clicked();
    void dialogClosing(int state);
    void tableViewSelectionChanged();
    void cameraWorkerStart();
    void cameraListCame();
    void clearCameraTableView();
    void cameraTableViewAddNewRow(Pylon::CDeviceInfo &info);

    inline bool isCameraDeviceAccessible(Pylon::CDeviceInfo &info) {
        return Pylon::CTlFactory::GetInstance().IsDeviceAccessible(info);
    }

signals:
    void userSelectionFinished(bool isAccept, Pylon::CDeviceInfo devices);

private:
    Ui::BaslerCamSelectDialog *ui;
    GetDevicesWorker *get_cam_thread;

    Pylon::DeviceInfoList cameraDeviceList;

    bool m_is_selected{false};
    int m_current_select_row{-1};

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
