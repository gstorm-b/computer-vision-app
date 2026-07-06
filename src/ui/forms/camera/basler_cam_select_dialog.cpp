#include "basler_cam_select_dialog.h"
#include "ui_basler_cam_select_dialog.h"

BaslerCamSelectDialog::BaslerCamSelectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BaslerCamSelectDialog) {
    ui->setupUi(this);

    initForm();
}

BaslerCamSelectDialog::~BaslerCamSelectDialog() {
    if (get_cam_thread != nullptr) {
        if (get_cam_thread->isRunning()) {
            get_cam_thread->terminate();
        }
    }
    delete ui;
}

void BaslerCamSelectDialog::initForm() {
    this->setWindowTitle(tr("Balser camera selection"));
    this->setModal(true);

    // void lock UI thread
    get_cam_thread = new GetDevicesWorker(this);
    connect(get_cam_thread, &GetDevicesWorker::started,
            this, &BaslerCamSelectDialog::cameraWorkerStart,
            Qt::QueuedConnection);
    connect(get_cam_thread, &GetDevicesWorker::resultReady,
            this, &BaslerCamSelectDialog::cameraListCame,
            Qt::QueuedConnection);

    ui->tablewg_camera_list->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tablewg_camera_list->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

    ui->tablewg_camera_list->setMinimumWidth(800);

    QHeaderView* header = ui->tablewg_camera_list->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Stretch);

    ui->tablewg_camera_list->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->tablewg_camera_list->setColumnCount(7);
    ui->tablewg_camera_list->setHorizontalHeaderLabels(table_headers);

    connect(ui->btn_cancel, &QPushButton::clicked,
            this, &BaslerCamSelectDialog::btn_cancel_clicked);
    connect(ui->btn_refresh, &QPushButton::clicked,
            this, &BaslerCamSelectDialog::btn_refresh_clicked);
    connect(ui->btn_select_confirm, &QPushButton::clicked,
            this, &BaslerCamSelectDialog::btn_select_confirm_clicked);

    connect(ui->tablewg_camera_list, &QTableWidget::itemSelectionChanged,
            this, &BaslerCamSelectDialog::tableViewSelectionChanged);
    connect(this, &QDialog::finished,
            this, &BaslerCamSelectDialog::dialogClosing);

    if (!get_cam_thread->isRunning()) {
        get_cam_thread->start();
    }
}

void BaslerCamSelectDialog::showCameraSelectForm() {
    if (get_cam_thread == nullptr) {
        return;
    }

    if (!get_cam_thread->isRunning()) {
        get_cam_thread->start();
    }

    m_is_selected = false;
    m_current_select_row = -1;
    ui->btn_select_confirm->setEnabled(m_is_selected);
    this->show();
}

void BaslerCamSelectDialog::btn_refresh_clicked() {
    if (!get_cam_thread->isRunning()) {
        get_cam_thread->start();
    }
}

void BaslerCamSelectDialog::btn_cancel_clicked() {
    this->reject();
}

void BaslerCamSelectDialog::btn_select_confirm_clicked() {
    this->accept();
}

void BaslerCamSelectDialog::dialogClosing(int state) {
    if (state == QDialog::Accepted) {
        if ((m_is_selected)
            && (m_current_select_row >= 0)
            && (m_current_select_row < cameraDeviceList.size())) {
            emit userSelectionFinished(true, cameraDeviceList[m_current_select_row]);
            return;
        }
    }
    emit userSelectionFinished(false, Pylon::CDeviceInfo());
}

void BaslerCamSelectDialog::tableViewSelectionChanged() {
    m_current_select_row = ui->tablewg_camera_list->currentRow();
    if((m_current_select_row < 0)
        && (m_current_select_row >= cameraDeviceList.size())) {
        m_is_selected = false;
    } else {
        m_is_selected = true;
    }
    ui->btn_select_confirm->setEnabled(m_is_selected);
}

void BaslerCamSelectDialog::cameraWorkerStart() {
    ui->label_status->setText(tr("Please wait! Devices querying"));
    ui->btn_refresh->setEnabled(false);
}

void BaslerCamSelectDialog::cameraListCame() {
    ui->label_status->setText(tr("Devices queried"));
    ui->btn_refresh->setEnabled(true);
    cameraDeviceList = get_cam_thread->cameraDeviceList;
    clearCameraTableView();
    if(cameraDeviceList.size() > 0 ) {
        for(int addCounter = 0; addCounter < cameraDeviceList.size(); addCounter++) {
            cameraTableViewAddNewRow(cameraDeviceList[addCounter]);
        }
    }
}

void BaslerCamSelectDialog::clearCameraTableView() {
    int removeRow = ui->tablewg_camera_list->rowCount();
    for(int removeCounter=0;removeCounter<removeRow;removeCounter++) {
        ui->tablewg_camera_list->removeRow(0);
    }
}

void BaslerCamSelectDialog::cameraTableViewAddNewRow(Pylon::CDeviceInfo &info) {
    QTableWidgetItem *nameItem = new QTableWidgetItem();
    QTableWidgetItem *userIDItem = new QTableWidgetItem();
    QTableWidgetItem *serialNumberItem = new QTableWidgetItem();
    QTableWidgetItem *macAddrItem = new QTableWidgetItem();
    QTableWidgetItem *statusItem = new QTableWidgetItem();
    // QTableWidgetItem *ipConfigItem = new QTableWidgetItem();
    QTableWidgetItem *ipAddrItem = new QTableWidgetItem();
    QTableWidgetItem *subnetMarkItem = new QTableWidgetItem();
    // add new row
    int row = ui->tablewg_camera_list->rowCount();
    ui->tablewg_camera_list->insertRow(row);
    // add item
    // header: Name | Device User ID | Serial Number | MAC Address | Status | IP Address | Subnet mark

    // Name
    nameItem->setText(QString::fromUtf8(info.GetModelName().c_str()));
    ui->tablewg_camera_list->setItem(row, 0, nameItem);

    // Device User ID
    userIDItem->setText(QString::fromUtf8(info.GetUserDefinedName().c_str()));
    ui->tablewg_camera_list->setItem(row, 1, userIDItem);

    // Serial Number
    serialNumberItem->setText(QString::fromUtf8(info.GetSerialNumber().c_str()));
    ui->tablewg_camera_list->setItem(row, 2, serialNumberItem);

    // IP Address
    ipAddrItem->setText(QString::fromUtf8(info.GetIpAddress().c_str()));
    ui->tablewg_camera_list->setItem(row, 3, ipAddrItem);

    // MAC Address
    macAddrItem->setText(QString::fromUtf8(info.GetMacAddress().c_str()));
    ui->tablewg_camera_list->setItem(row, 4, macAddrItem);

    // Status, check device can accessible or not
    if(isCameraDeviceAccessible(info)) {
        statusItem->setText(tr("Ok"));
    }
    else {
        statusItem->setText(tr("In use"));
    }
    ui->tablewg_camera_list->setItem(row, 5, statusItem);

    // Subnet mark
    subnetMarkItem->setText(QString::fromUtf8(info.GetSubnetMask().c_str()));
    ui->tablewg_camera_list->setItem(row, 6, subnetMarkItem);
}

