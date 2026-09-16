#include "jai_cam_select_dialog.h"
#include "ui_jai_cam_select_dialog.h"

#include "core/logger/app_logger.h"
#include "device/camera/jai_runtime.h"

#include <QHeaderView>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <PvDeviceInfoGEV.h>
#include <PvInterface.h>
#include <PvSystem.h>

namespace {

/// Converts an SDK string to QString.
QString qs(const PvString &text)
{
    return QString::fromUtf8(text.GetAscii());
}

} // namespace

// ── Discovery worker ─────────────────────────────────────────────────────────────────────────

void JaiDeviceScanWorker::run()
{
    cameras.clear();

    // Discovery itself does not need the GenICam runtime (PvSystem64.dll delay-loads nothing),
    // but the Connect that follows does. Reporting it here means the operator learns about a
    // missing runtime while picking a camera, instead of at the moment the application would
    // otherwise have been killed. See jai_runtime.h.
    vc::device::jai::ensureGenICamRuntime();

    PvSystem system;
    const PvResult found = system.Find();
    if (!found.IsOK()) {
        LOG_USER_ERR << QObject::tr("GigE camera discovery failed: %1 - %2")
                            .arg(qs(found.GetCodeString()), qs(found.GetDescription()));
        emit resultReady();
        return;
    }

    // eBUS enumerates PER NETWORK ADAPTER, so a camera reachable through more than one
    // adapter is reported once per adapter — the same physical camera, listed several times.
    // Deduplicated on MAC address, which is the one identifier that belongs to the camera
    // rather than to the path taken to reach it: IP can be duplicated across subnets, and the
    // connection ID differs per interface, so neither can be the key.
    QSet<QString> seenMac;

    const uint32_t interfaceCount = system.GetInterfaceCount();
    for (uint32_t i = 0; i < interfaceCount; ++i) {
        const PvInterface *iface = system.GetInterface(i);
        if (iface == nullptr) {
            continue;
        }
        const uint32_t deviceCount = iface->GetDeviceCount();
        for (uint32_t d = 0; d < deviceCount; ++d) {
            const auto *gev = dynamic_cast<const PvDeviceInfoGEV *>(iface->GetDeviceInfo(d));
            if (gev == nullptr) {
                continue;   // USB3 Vision or another transport; this dialog lists GigE only
            }

            // Copied out field by field, not stored as a pointer: `gev` belongs to `system`
            // and dies when this function returns. See the note on JaiCameraInfo.
            JaiCameraInfo info;
            info.modelName         = qs(gev->GetModelName());
            info.vendorName        = qs(gev->GetVendorName());
            info.userDefinedName   = qs(gev->GetUserDefinedName());
            info.serialNumber      = qs(gev->GetSerialNumber());
            info.macAddress        = qs(gev->GetMACAddress());
            info.ipAddress         = qs(gev->GetIPAddress());
            info.subnetMask        = qs(gev->GetSubnetMask());
            info.connectionId      = qs(gev->GetConnectionID());
            info.configurationValid = gev->IsConfigurationValid();

            // Fall back to serial when a camera reports no MAC, and to the connection ID when it
            // reports neither — never drop a camera just because it identified itself poorly.
            QString key = info.macAddress;
            if (key.isEmpty()) {
                key = info.serialNumber;
            }
            if (key.isEmpty()) {
                key = info.connectionId;
            }
            if (!key.isEmpty() && seenMac.contains(key)) {
                continue;
            }
            if (!key.isEmpty()) {
                seenMac.insert(key);
            }

            cameras.append(info);
        }
    }

    emit resultReady();
}

// ── Dialog ───────────────────────────────────────────────────────────────────────────────────

JaiCamSelectDialog::JaiCamSelectDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::JaiCamSelectDialog)
{
    ui->setupUi(this);
    initForm();
}

JaiCamSelectDialog::~JaiCamSelectDialog()
{
    if (m_scanThread != nullptr && m_scanThread->isRunning()) {
        // Waited for, not terminated. The scan holds an eBUS PvSystem whose destructor releases
        // driver resources; killing the thread mid-Find() leaks them for the life of the process
        // and can leave the next discovery pass returning nothing.
        m_scanThread->wait(5000);
    }
    delete ui;
}

void JaiCamSelectDialog::initForm()
{
    setWindowTitle(tr("JAI camera selection"));
    setModal(true);

    // Discovery blocks for the SDK's detection timeout, so it never runs on the GUI thread.
    m_scanThread = new JaiDeviceScanWorker(this);
    connect(m_scanThread, &JaiDeviceScanWorker::started,
            this, &JaiCamSelectDialog::cameraWorkerStart, Qt::QueuedConnection);
    connect(m_scanThread, &JaiDeviceScanWorker::resultReady,
            this, &JaiCamSelectDialog::cameraListCame, Qt::QueuedConnection);

    ui->tablewg_camera_list->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tablewg_camera_list->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tablewg_camera_list->setMinimumWidth(800);
    ui->tablewg_camera_list->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tablewg_camera_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tablewg_camera_list->setColumnCount(7);
    ui->tablewg_camera_list->setHorizontalHeaderLabels(table_headers);

    connect(ui->btn_cancel, &QPushButton::clicked,
            this, &JaiCamSelectDialog::btn_cancel_clicked);
    connect(ui->btn_refresh, &QPushButton::clicked,
            this, &JaiCamSelectDialog::btn_refresh_clicked);
    connect(ui->btn_select_confirm, &QPushButton::clicked,
            this, &JaiCamSelectDialog::btn_select_confirm_clicked);
    connect(ui->tablewg_camera_list, &QTableWidget::itemSelectionChanged,
            this, &JaiCamSelectDialog::tableViewSelectionChanged);
    connect(this, &QDialog::finished, this, &JaiCamSelectDialog::dialogClosing);

    ui->btn_select_confirm->setEnabled(false);

    if (!m_scanThread->isRunning()) {
        m_scanThread->start();
    }
}

void JaiCamSelectDialog::showCameraSelectForm()
{
    if (m_scanThread == nullptr) {
        return;
    }
    if (!m_scanThread->isRunning()) {
        m_scanThread->start();
    }

    m_is_selected = false;
    m_current_select_row = -1;
    ui->btn_select_confirm->setEnabled(false);
    show();
}

void JaiCamSelectDialog::btn_refresh_clicked()
{
    if (!m_scanThread->isRunning()) {
        m_scanThread->start();
    }
}

void JaiCamSelectDialog::btn_cancel_clicked()
{
    reject();
}

void JaiCamSelectDialog::btn_select_confirm_clicked()
{
    accept();
}

void JaiCamSelectDialog::dialogClosing(int state)
{
    if (state == QDialog::Accepted
        && m_is_selected
        && m_current_select_row >= 0
        && m_current_select_row < m_cameras.size()) {
        emit userSelectionFinished(true, m_cameras.at(m_current_select_row));
        return;
    }
    emit userSelectionFinished(false, JaiCameraInfo());
}

void JaiCamSelectDialog::tableViewSelectionChanged()
{
    m_current_select_row = ui->tablewg_camera_list->currentRow();
    // Both bounds, with &&. The Basler dialog writes `(row < 0) && (row >= size)` — a condition
    // that is never true — so its selection flag is set even with nothing selected.
    m_is_selected = (m_current_select_row >= 0) && (m_current_select_row < m_cameras.size());
    ui->btn_select_confirm->setEnabled(m_is_selected);
}

void JaiCamSelectDialog::cameraWorkerStart()
{
    ui->label_status->setText(tr("Please wait! Devices querying"));
    ui->btn_refresh->setEnabled(false);
}

void JaiCamSelectDialog::cameraListCame()
{
    ui->btn_refresh->setEnabled(true);
    m_cameras = m_scanThread->cameras;

    clearCameraTableView();
    for (const JaiCameraInfo &info : m_cameras) {
        cameraTableViewAddNewRow(info);
    }

    if (m_cameras.isEmpty()) {
        // The same three causes as the camera device's own connect failure, in the one place an
        // operator looks first. The firewall one is invisible from anywhere else.
        ui->label_status->setText(tr("No camera found — check cabling, IP configuration, and "
                                     "that this application is allowed through Windows "
                                     "Defender Firewall."));
    } else {
        ui->label_status->setText(tr("%1 camera(s) found").arg(m_cameras.size()));
    }

    m_is_selected = false;
    m_current_select_row = -1;
    ui->btn_select_confirm->setEnabled(false);
}

void JaiCamSelectDialog::clearCameraTableView()
{
    ui->tablewg_camera_list->setRowCount(0);
}

void JaiCamSelectDialog::cameraTableViewAddNewRow(const JaiCameraInfo &info)
{
    const int row = ui->tablewg_camera_list->rowCount();
    ui->tablewg_camera_list->insertRow(row);

    const auto setCell = [this, row](int column, const QString &text) {
        ui->tablewg_camera_list->setItem(row, column, new QTableWidgetItem(text));
    };

    setCell(0, info.modelName);
    setCell(1, info.userDefinedName);
    setCell(2, info.serialNumber);
    setCell(3, info.ipAddress);
    setCell(4, info.macAddress);
    // "Ok" / "Needs IP" rather than Basler's "Ok" / "In use": eBUS reports whether the camera's
    // address is valid for this subnet, which is the condition that actually blocks a connect
    // here. A camera on the wrong subnet is discoverable and cannot be opened, and the operator
    // needs to be told which of those two it is.
    setCell(5, info.configurationValid ? tr("Ok") : tr("Needs IP assignment"));
    setCell(6, info.subnetMask);
}
