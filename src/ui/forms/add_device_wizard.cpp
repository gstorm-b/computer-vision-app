#include "add_device_wizard.h"
#include "ui_add_device_wizard.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>

#include "device/device_factory.h"
#include "device/camera/camera_device.h"
#include "device/output_device/vision_output_device.h"
#include "device/output_device/vision_tcpip_config.h"
#include "device/output_device/vision_tcpip_client_config.h"
#include "device/plc/mc_protocol_device.h"
#include "core/logger/app_logger.h"
#include "core/utils/theme_manager.h"
#include "core/utils/windows_helper.h"

AddDeviceWizard::AddDeviceWizard(std::shared_ptr<vc::device::DeviceManager> mng,
                                 const QString &taskName,
                                 QWidget *parent)
    : QDialog(parent), ui(new Ui::AddDeviceWizard), m_manager(mng)
{
    ui->setupUi(this);
    setModal(true);

    if (!taskName.isEmpty()) {
        ui->adwSubtitle->setText(tr("Task: ") + taskName);
    } else {
        ui->adwSubtitle->hide();
    }

    if (m_manager) {
        ui->cbxCameraType->addItems(m_manager->getSubDeviceTypeList(vc::device::Camera));
        ui->cbxMcFrameType->addItem(
            vc::device::mc::McFrameTypeToString(vc::device::mc::McFrameType::Frame_3E));
        ui->cbxMcCode->addItem(
            vc::device::mc::McDataCodeToString(vc::device::mc::McDataCode::Binary));
        ui->cbxVisionType->addItems(m_manager->getSubDeviceTypeList(vc::device::VisionOutput));
    }

    ui->adwInfoIcon->setPixmap(
        QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(14, 14));

    ui->adwAddBtn->setEnabled(false);

    initCards();

    connect(ui->adwNameEdit, &QLineEdit::textChanged, this, &AddDeviceWizard::onNameChanged);
    connect(ui->adwCancelBtn, &QPushButton::clicked, this, &AddDeviceWizard::reject);
    connect(ui->adwAddBtn,    &QPushButton::clicked, this, &AddDeviceWizard::onAddClicked);

    reloadStyleSheet();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &, bool) {
        reloadStyleSheet();
        refreshCardIcons();
    });

    selectCard(vc::device::DeviceType::Camera);
}

AddDeviceWizard::~AddDeviceWizard() {
    delete ui;
}

void AddDeviceWizard::initCards() {
    auto bind = [&](vc::device::DeviceType type, CardRefs refs) {
        refs.card->installEventFilter(this);
        refs.icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        refs.name->setAttribute(Qt::WA_TransparentForMouseEvents);
        refs.desc->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_cards.insert(type, refs);
    };

    bind(vc::device::DeviceType::Camera, {
        ui->adwCard_camera,
        ui->adwCardIcon_camera, ui->adwCardName_camera, ui->adwCardDesc_camera,
        0, "camera", "Basler_cam_01", ":/resrc/icon/camera.svg"
    });

    bind(vc::device::DeviceType::PLC, {
        ui->adwCard_plc,
        ui->adwCardIcon_plc, ui->adwCardName_plc, ui->adwCardDesc_plc,
        1, "plc", "PLC_Mitsu_01", ":/resrc/icon/plc_icon.svg"
    });

    bind(vc::device::DeviceType::VisionOutput, {
        ui->adwCard_visionOutput,
        ui->adwCardIcon_visionOutput, ui->adwCardName_visionOutput, ui->adwCardDesc_visionOutput,
        2, "visionOutput", "VisionOut_01", ":/resrc/icon/plug_connected.svg"
    });

    refreshCardIcons();
}

void AddDeviceWizard::refreshCardIcons()
{
    for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it) {
        const CardRefs &refs = it.value();
        if (refs.icon)
            refs.icon->setPixmap(svgIcon(refs.iconPath).pixmap(22, 22));
    }
}

void AddDeviceWizard::selectCard(vc::device::DeviceType type) {
    m_selectedType = type;

    for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it) {
        const bool active = (it.key() == type);
        QFrame *card = it.value().card;
        QLabel *name = it.value().name;
        card->setProperty("selected", active);
        name->setProperty("selected", active);
        repolish(card);
        repolish(name);
    }

    const CardRefs &sel = m_cards.value(type);

    if (sel.stackPage < 0) {
        ui->adwConfigStack->hide();
    } else {
        ui->adwConfigStack->show();
        ui->adwConfigStack->setCurrentIndex(sel.stackPage);
    }

    ui->adwNameEdit->setText(sel.defaultName);

    ui->adwAddBtn->setProperty("deviceColor", sel.colorKey);
    repolish(ui->adwAddBtn);
}

void AddDeviceWizard::reloadStyleSheet() {
    const QString path = ThemeManager::instance()->isDark()
        ? QStringLiteral(":/styles/add_device_wizard_dark.qss")
        : QStringLiteral(":/styles/add_device_wizard_light.qss");

    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        setStyleSheet(ThemeManager::instance()->resolveTokens(
            QString::fromUtf8(f.readAll())));
    } else {
        LOG_USER_WARN << QString("AddDeviceWizard: cannot load stylesheet %1").arg(path);
    }
}

void AddDeviceWizard::repolish(QWidget *w) {
    if (!w) return;
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

int AddDeviceWizard::showWizard() {
    if (!m_manager) return QDialog::Rejected;
    m_pendingDeviceId = m_manager->allocatePendingId();
    ui->adwNameEdit->setFocus();
    return exec();
}

QString AddDeviceWizard::getDeviceName() const {
    return ui->adwNameEdit->text().trimmed();
}

QString AddDeviceWizard::getDeviceType() const {
    return vc::device::DeviceTypeToString(m_selectedType);
}

void AddDeviceWizard::reject() {
    if (m_manager && !m_pendingDeviceId.isEmpty())
        m_manager->releasePendingId(m_pendingDeviceId);
    m_pendingDeviceId.clear();
    QDialog::reject();
}

void AddDeviceWizard::onCardClicked(vc::device::DeviceType type) {
    selectCard(type);
}

void AddDeviceWizard::onNameChanged(const QString &text) {
    ui->adwAddBtn->setEnabled(!text.trimmed().isEmpty());
}

void AddDeviceWizard::onAddClicked() {
    const QString name = ui->adwNameEdit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Add Device"), tr("Please enter a device name."));
        ui->adwNameEdit->setFocus();
        return;
    }

    if (m_manager->isNameExists(name)) {
        QMessageBox::warning(this, tr("Add Device"), tr("Device name already exists."));
        ui->adwNameEdit->selectAll();
        return;
    }

    QJsonObject jobj = buildDeviceJson(m_selectedType);
    jobj[DEVICE_JSK_ID]   = m_pendingDeviceId;
    jobj[DEVICE_JSK_NAME] = name;
    jobj[DEVICE_JSK_TYPE] = vc::device::DeviceTypeToString(m_selectedType);

    std::shared_ptr<vc::device::IDevice> dv(
        vc::device::DeviceFactory::create(m_selectedType, jobj));

    if (m_manager->commitDevice(m_pendingDeviceId, name, dv)) {
        LOG_USER_INFO << QString("Device added: %1 (%2)").arg(name, m_pendingDeviceId);
        accept();
    } else {
        QMessageBox::critical(this, tr("Add Device"),
            tr("Failed to register device. Please close and try again."));
    }
}

QJsonObject AddDeviceWizard::buildDeviceJson(vc::device::DeviceType type) {
    QJsonObject obj;

    switch (type) {
    case vc::device::DeviceType::Camera:
        if (ui->cbxCameraType->count() > 0)
            obj[DEVICE_JSK_CAM_TYPE] = ui->cbxCameraType->currentText();
        break;

    case vc::device::DeviceType::PLC: {
        vc::device::McProtocolConfig config;
        auto frameType = (ui->cbxMcFrameType->count() > 0)
            ? vc::device::mc::McFrameTypeFromString(ui->cbxMcFrameType->currentText())
            : vc::device::mc::McFrameType::Frame_3E;
        auto dataCode = (ui->cbxMcCode->count() > 0)
            ? vc::device::mc::McDataCodeFromString(ui->cbxMcCode->currentText())
            : vc::device::mc::McDataCode::Binary;
        config.configMcProtocol(frameType, dataCode);
        obj[DEVICE_JSK_CONFIG] = config.toJson();
        break;
    }

    case vc::device::DeviceType::VisionOutput: {
        auto subType = (ui->cbxVisionType->count() > 0)
            ? vc::device::VisionOutputTypeFromString(ui->cbxVisionType->currentText())
            : vc::device::VisionOutputType::VisionTCPIP;
        switch (subType) {
        case vc::device::VisionOutputType::VisionTCPIP: {
            vc::device::VisionTcpipDeviceCfg config;
            obj[DEVICE_JSK_CONFIG] = config.toJson();
            break;
        }
        case vc::device::VisionOutputType::VisionTcpipClient: {
            vc::device::VisionTcpipClientDeviceCfg config;
            obj[DEVICE_JSK_CONFIG] = config.toJson();
            break;
        }
        case vc::device::VisionOutputType::VisionSerial:
        case vc::device::VisionOutputType::VisionOutputTypeNone:
            // No concrete impl yet — leave config empty; factory will reject.
            break;
        }
        break;
    }

    default:
        break;
    }

    return obj;
}

bool AddDeviceWizard::eventFilter(QObject *obj, QEvent *ev) {
    if (ev->type() == QEvent::MouseButtonPress) {
        for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it) {
            if (obj == it.value().card) {
                onCardClicked(it.key());
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, ev);
}

void AddDeviceWizard::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        event->ignore();
    } else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
               && ui->adwAddBtn->isEnabled()) {
        onAddClicked();
    } else {
        QDialog::keyPressEvent(event);
    }
}
