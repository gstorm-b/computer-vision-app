#include "basler_camera_widget.h"
#include "ui_basler_camera_widget.h"

#include <QMessageBox>
#include <QHostAddress>

#include "core/utils/windows_helper.h"

#include "calibration/calibration_board_factory.h"
#include "ui/widgets/calibration/calibration_board_dialog.h"
#include "ui/widgets/calibration/calibration_threshold_dialog.h"
#include "ui/widgets/calibration/calibration_points_table.h"

/// Converts an OpenCV Mat (8-bit grayscale, BGR, or BGRA) to a QPixmap for display.
/// @param mat source image; must be CV_8UC1, CV_8UC3 (BGR), or CV_8UC4 (BGRA)
/// @return the converted pixmap, or a null QPixmap if `mat`'s type is unsupported
inline QPixmap cvMatToQPixmap(const cv::Mat& mat) {
    QImage qimg;
    if (mat.type() == CV_8UC1) {
        // Grayscale
        qimg = QImage(mat.data,
                      mat.cols,
                      mat.rows,
                      static_cast<int>(mat.step),
                      QImage::Format_Grayscale8);
    } else if (mat.type() == CV_8UC3) {
        // OpenCV uses BGR, Qt uses RGB
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        qimg = QImage(rgb.data,
                      rgb.cols,
                      rgb.rows,
                      static_cast<int>(rgb.step),
                      QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        // BGRA → RGBA
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        qimg = QImage(rgba.data,
                      rgba.cols,
                      rgba.rows,
                      static_cast<int>(rgba.step),
                      QImage::Format_RGBA8888).copy();
    } else {
        return QPixmap();
    }
    return QPixmap::fromImage(qimg);
}

/// Creates and registers a QtVariantProperty for a single Qt meta-property, handling
/// enum types specially (registers the enum's key names) and setting the display
/// name / minimum / maximum attributes from the class-info entries
/// "<prop>_name", "<prop>_min", "<prop>_max" when present. The "objectName"
/// property is skipped.
/// @param meta meta-object of the class owning `prop` (used to read class-info and enum names)
/// @param prop the meta-property being converted
/// @param value the property's current value
/// @param manager property manager the new property is registered with
/// @param browser unused for creation itself but kept for signature symmetry with callers
/// @return the created property, or nullptr for objectName / on failure to create a property
static QtVariantProperty* addPropertyToBrowser(const QMetaObject &meta, QMetaProperty &prop, QVariant &value,
                                        QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser) {

    QString propName = prop.name();
    if (propName == "objectName") {
        // ignore objectName
        return nullptr;
    }

    QtVariantProperty *variantProp = nullptr;
    // enum type Check
    if (prop.isEnumType()) {
        variantProp = manager->addProperty(QtVariantPropertyManager::enumTypeId(), propName);

        // get enum names
        QStringList enumNames;
        QMetaEnum metaEnum = prop.enumerator();
        for (int j = 0; j < metaEnum.keyCount(); ++j) {
            const char* key = metaEnum.key(j);
            QString translatedName = QCoreApplication::translate(meta.className(), key);
            enumNames << translatedName;
        }
        variantProp->setAttribute(QLatin1String("enumNames"), enumNames);
        // set enum value
        variantProp->setValue(value.toInt());

    } else {
        // normal data types handle (int, QString, bool, QColor, v.v.)
        int typeId = value.userType();
        variantProp = manager->addProperty(typeId, propName);
        if (variantProp) {
            variantProp->setValue(value);
        }
    }

    if (!variantProp) {
        return nullptr;
    }

    int displayNameIdx = meta.indexOfClassInfo(QString("%1_name").arg(propName).toUtf8());
    if (displayNameIdx != -1) {
        const QString displayName = meta.classInfo(displayNameIdx).value();
        if (!displayName.isEmpty()) {
            variantProp->setDisplayName(displayName);
        }
    }

    // --- set attributes---
    int typeId = value.userType();
    int minIdx = meta.indexOfClassInfo(QString("%1_min").arg(propName).toUtf8());
    if (minIdx != -1) {
        if (typeId == QMetaType::Int) {
            variantProp->setAttribute("minimum", QString(meta.classInfo(minIdx).value()).toInt());
        } else if (typeId == QMetaType::Double) {
            variantProp->setAttribute("minimum", QString(meta.classInfo(minIdx).value()).toDouble());
        }
    }

    int maxIdx = meta.indexOfClassInfo(QString("%1_max").arg(propName).toUtf8());
    if (maxIdx != -1) {
        if (typeId == QMetaType::Int) {
            variantProp->setAttribute("maximum", QString(meta.classInfo(maxIdx).value()).toInt());
        } else if (typeId == QMetaType::Double) {
            variantProp->setAttribute("maximum", QString(meta.classInfo(maxIdx).value()).toDouble());
        }
    }

    if (!prop.isWritable()) {
        variantProp->setEnabled(false);
    }

    return variantProp;
}

void BaslerCameraWidget::populateBrowser_Device(vc::device::IDevice *gadget, QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser) {
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Device Information"));
    browser->addProperty(topItem);
    const QMetaObject *meta = gadget->metaObject();

    int propCount = meta->propertyCount();
    for (int i = 0; i < propCount; ++i) {
        QMetaProperty prop = meta->property(i);
        // QVariant value = prop.readOnGadget(gadget);
        QVariant value = gadget->property(prop.name());
        QtVariantProperty *variantProp = addPropertyToBrowser(*meta, prop, value, manager, browser);
        if (variantProp) {
            topItem->addSubProperty(variantProp);
        }
    }
}

void BaslerCameraWidget::populateBrowser_BaslerConfig(vc::device::BaslerGigeCfg *gadget, QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser) {
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Camera basler"));
    browser->addProperty(topItem);
    const QMetaObject &meta = gadget->getMetaObject();

    int propCount = meta.propertyCount();
    for (int i = 0; i < propCount; ++i) {
        QMetaProperty prop = meta.property(i);
        QVariant value = prop.readOnGadget(gadget);

        QtVariantProperty *variantProp = addPropertyToBrowser(meta, prop, value, manager, browser);
        if (variantProp) {
            topItem->addSubProperty(variantProp);
        }

        if (variantProp->propertyName() == "paramsExposureTime") {
            variantProp->setAttribute("minimum", gadget->m_paramsExposureMin);
            variantProp->setAttribute("maximum",  gadget->m_paramsExposureMax);
        } else if (variantProp->propertyName() == "paramsGain") {
            variantProp->setAttribute("minimum", gadget->m_paramsGainMin);
            variantProp->setAttribute("maximum",  gadget->m_paramsGainMax);
        } else if (variantProp->propertyName() == "autoBacklightLine") {
            QStringList str_list;
            for (BaslerIOLine io : gadget->m_ioCapabilities) {
                if (io.can_be_output) {
                    str_list.append(io.name);
                }
            }
            variantProp->setAttribute("completer", str_list);
        }
    }
}

BaslerCameraWidget::BaslerCameraWidget(std::shared_ptr<vc::device::IDevice> dv,
                                       vc::runtime::CameraRunner *runner,
                                       ads::CDockWidget *dock, QWidget *parent)
    : IDeviceWidget(parent),
    ui(new Ui::BaslerCameraWidget),
    m_device(dv),
    m_dock(dock),
    m_runner(runner) {

    ui->setupUi(this);
    initCameraWiget();
}

BaslerCameraWidget::~BaslerCameraWidget() {
    delete ui;
}

QString BaslerCameraWidget::deviceId() {
    return m_device->id();
}

void BaslerCameraWidget::loadConfigToDevice() {
    if(!m_camera) {
        return;
    }
    m_camera->setBaslerGigeConfig(m_params);
}

void BaslerCameraWidget::loadConfigToWidget() {
    populateBrowser();
}

void BaslerCameraWidget::setDockWidget(ads::CDockWidget *dock) {
    m_dock = dock;
}

void BaslerCameraWidget::initCameraWiget() {
    initPropertyBrowser();

    ui->btn_connect->setIcon(svgIcon(":/resrc/icon/plug_disconnected.svg"));
    ui->btn_trigger->setIcon(svgIcon(":/resrc/icon/capture.svg"));

    connect(ui->btn_select_camera, &QPushButton::clicked,
            this, &BaslerCameraWidget::btn_choose_camera_clicked);
    connect(ui->btn_connect, &QPushButton::clicked,
            this, &BaslerCameraWidget::btn_connect_clicked);
    connect(ui->btn_trigger, &QPushButton::clicked,
            this, &BaslerCameraWidget::btn_trigger_clicked);
    connect(ui->btn_save_image, &QPushButton::clicked,
            this, &BaslerCameraWidget::btn_save_image_clicked);

    connect(ui->btn_setup_board, &QPushButton::clicked,
            this, &BaslerCameraWidget::btn_setup_board_clicked);
    connect(ui->btn_calib_threshold, &QPushButton::clicked,
            this, &BaslerCameraWidget::btn_calib_threshold_clicked);
    connect(ui->btn_calib_detect, &QPushButton::clicked,
            this, &BaslerCameraWidget::btn_calib_detect_clicked);
    connect(ui->btn_calib_apply, &QPushButton::clicked,
            this, &BaslerCameraWidget::btn_calib_apply_clicked);
    connect(ui->table_calibration_points, &CalibrationPointsTable::pointsEdited,
            this, &BaslerCameraWidget::onCalibPointsEdited);
    // connect(ui->btn_auto_shot, &QPushButton::clicked,
    //         this, &BaslerCameraWidget::btn_auto_shot_clicked);

    // connect(ui->btn_baklight_toggle, &QPushButton::clicked,
    //         this, &BaslerCameraWidget::btn_backlight_toggle_clicked);

    if (m_device) {
        m_camera = qobject_cast<vc::device::BaslerGigECamera*>(m_device.get());
        if (!m_camera) {
            LOG_DEV_ERR << "BaslerCameraWidget: expected BaslerGigECamera but got"
                        << m_device->id();
            setEnabled(false);
        } else {
            m_params = m_camera->baslerGigeConfig();
            loadConfigToWidget();
            connect(m_variantManager, &QtVariantPropertyManager::valueChanged,
                    this, &BaslerCameraWidget::onPropertyValueChanged);

            initCalibrationUi();
        }
    }

    ui->splitter_main->setStretchFactor(0, 7);
    ui->splitter_main->setStretchFactor(0, 3);

    m_camera_select_dialog = new BaslerCamSelectDialog(this);

    connect(m_camera_select_dialog, &BaslerCamSelectDialog::userSelectionFinished,
            this, &BaslerCameraWidget::cameraSelectionFinished);

    // ── Wire to runner (NOT to device directly) ──────────────────────────────
    // The runner forwards device signals onto the GUI thread; that is the
    // whole point of routing through TaskRunner.  The Widget never touches
    // QThread or moveToThread().
    if (m_runner && m_camera) {
        connect(m_runner, &vc::runtime::CameraRunner::connectStatusChanged,
                this,     &BaslerCameraWidget::onCameraConnectStatusChanged);
        connect(m_runner, &vc::runtime::CameraRunner::grabFinished,
                this,     &BaslerCameraWidget::onCameraGrabFinished);
    } else {
        LOG_DEV_ERR << "BaslerCameraWidget: no runner provided — control disabled";
    }

    onCameraConnectStatusChanged(m_device && m_device->isDeviceConnected()
                               ? vc::device::ConnectStatus::Connected
                               : vc::device::ConnectStatus::Disconnected);
}

void BaslerCameraWidget::setDoublePropertyLimit(QtVariantProperty *variant, double &max, double &min) {
    variant->setAttribute("minimum", min);
    variant->setAttribute("maximum", max);
}

void BaslerCameraWidget::setIntPropertyLimit(QtVariantProperty *variant, int &max, int &min) {
    variant->setAttribute("minimum", min);
    variant->setAttribute("maximum", max);
}

void BaslerCameraWidget::updateDockTitle() {
    if (m_dock == nullptr) {
        return;
    }
    m_dock->setWindowTitle(QString("%1").arg(m_device->name()));
}

void BaslerCameraWidget::onPropertyValueChanged(QtProperty *property, const QVariant &variant) {
    if (m_populating_browser) {
        return;
    }

    QString propName = property->propertyName();

    const QMetaObject *idevice_meta = m_device->metaObject();
    const QMetaObject &meta_cfg = m_params.getMetaObject();

    // abstract device
    int index = idevice_meta->indexOfProperty(propName.toUtf8());
    if (index != -1) {
        if (propName == "name") {
            QString new_name = variant.toString();
            // avoid loop made by signal valueChanged
            if (m_device->name() == new_name) {
                return;
            }

            if (!m_device->deviceManager()->changeDeviceName(m_device->id(), new_name)) {
                LOG_USER_WARN << tr("Cannot rename device to \"%1\": the name is "
                                    "already in use.").arg(new_name);
                m_variantManager->setValue(property, m_device->name());
            }
        }
        return;
    }

    // parameters
    index = meta_cfg.indexOfProperty(propName.toUtf8());
    if (index != -1) {
        QMetaProperty mProp = meta_cfg.property(index);
        mProp.writeOnGadget(&m_params, variant);
        qDebug() << "Updated context: " << propName << "to" << variant;
        this->m_camera->setBaslerGigeConfig(m_params);
        if (this->m_camera->isDeviceConnected() && m_runner) {
            m_runner->requestApplyParams();
        }
        return;
    }
}

void BaslerCameraWidget::onCameraConnectStatusChanged(vc::device::ConnectStatus status) {
    switch (status) {
    case vc::device::ConnectStatus::Connected:
        onCameraConnected();
        break;
    case vc::device::ConnectStatus::Disconnected:
        onCameraDisconnected();
        break;
    case vc::device::ConnectStatus::ConnectFailed:
        QMessageBox::warning(this,
                             tr("Connect error"),
                             m_camera->lastMsg());
        break;
    // Enumerate the rest explicitly (no default:) so adding a ConnectStatus
    // value surfaces a -Wswitch / C4062 warning here.
    case vc::device::ConnectStatus::NoConnection:
    case vc::device::ConnectStatus::LostConnected:
    case vc::device::ConnectStatus::Connecting:
        break;
    }

    // error on connect failed, check on runner/

    // if (status == vc::device::ConnectStatus::Connected) {
    //     onCameraConnected();
    // } else {
    //     onCameraDisconnected();
    // }
}

void BaslerCameraWidget::cameraSelectionFinished(bool isAccept,
                                                 Pylon::CDeviceInfo device) {
    if (isAccept) {
        m_params.m_deviceInfo = device;
        m_params.m_modelName = device.GetModelName();
        m_params.m_serialNumber = device.GetSerialNumber();
        m_params.m_userDefinedName = device.GetUserDefinedName();
        m_params.m_ipAddress = device.GetIpAddress();
        loadConfigToWidget();
        m_camera->setBaslerGigeConfig(m_params);
    }
}

void BaslerCameraWidget::btn_choose_camera_clicked() {
    m_camera_select_dialog->showCameraSelectForm();
}

void BaslerCameraWidget::btn_connect_clicked() {
    if (!m_runner) {
        QMessageBox::warning(this, tr("Connect error"),
                             tr("No camera runner available."));
        return;
    }

    if (!m_camera->isDeviceConnected()) {
        QHostAddress address;
        if (!address.setAddress(m_params.ipAddress())) {
            QMessageBox::warning(this,
                                 tr("Connect error"),
                                 tr("IP address invalid format."));
            return;
        }

        m_runner->requestConnect();
    } else {
        m_runner->requestDisconnect();
    }
}

void BaslerCameraWidget::btn_trigger_clicked() {
    if (!m_camera->isDeviceConnected() || !m_runner) {
        return;
    }

    m_runner->requestSingleShot();
}

void BaslerCameraWidget::btn_save_image_clicked() {

}

void BaslerCameraWidget::onCameraConnected() {
    ui->lb_connection_status->setText(tr("Connected"));
    ui->lb_connection_status->setProperty("connectionState", "disconnected");
    ui->lb_connection_status->style()->unpolish(ui->lb_connection_status);
    ui->lb_connection_status->style()->polish(ui->lb_connection_status);
    ui->lb_connection_status->update();
    m_params = m_camera->baslerGigeConfig();
    LOG_DEV_DEBUG << m_params.m_paramsExposureMin << m_params.m_paramsExposureMax;
    LOG_DEV_DEBUG << m_params.m_paramsGainMin << m_params.m_paramsGainMax;
    loadConfigToWidget();
    ui->btn_connect->setIcon(svgIcon(":/resrc/icon/plug_connected.svg"));
}

void BaslerCameraWidget::onCameraDisconnected() {
    ui->lb_connection_status->setText(tr("Disconnected"));
    ui->lb_connection_status->setProperty("connectionState", "disconnected");
    ui->lb_connection_status->style()->unpolish(ui->lb_connection_status);
    ui->lb_connection_status->style()->polish(ui->lb_connection_status);
    ui->lb_connection_status->update();
    ui->btn_connect->setIcon(svgIcon(":/resrc/icon/plug_disconnected.svg"));
}

void BaslerCameraWidget::onCameraGrabFinished(vc::device::GrabResult result) {
    if (result.isGrabSuccess) {
        QPixmap new_frame = cvMatToQPixmap(result.frame);
        ui->image_view->loadImage(new_frame);
    }

    // Detect request initiated by btn_calib_detect: run board detection on the
    // freshly-grabbed frame and push the 4 corner points into the table.
    if (m_pendingCalibDetect) {
        m_pendingCalibDetect = false;

        if (!result.isGrabSuccess) {
            ui->label_calib_status->setText(tr("Calibration status: grab failed"));
            return;
        }
        if (!m_board) {
            ui->label_calib_status->setText(tr("Calibration status: no board set"));
            return;
        }

        std::vector<cv::Point2f> allPts;
        std::vector<cv::Point2f> cornerPts;
        cv::Mat debug_image;
        bool ok = m_board->detect(result.frame, allPts, &cornerPts, &debug_image);
        if (!ok || cornerPts.empty()) {
            ui->label_calib_status->setText(
                tr("Calibration status: board detect failed"));
            return;
        }

        ui->table_calibration_points->setImagePoints(cornerPts);
        ui->btn_calib_apply->setEnabled(true);
        ui->label_calib_status->setText(
            tr("Calibration status: detected (point changed)"));
    }

    // Frame requested by the threshold-tuning dialog (initial open or Re-grab):
    // push it into the open dialog so the preview refreshes.
    if (m_pendingThresholdGrab) {
        m_pendingThresholdGrab = false;
        if (m_thresholdDlg && result.isGrabSuccess) {
            m_thresholdDlg->setImage(result.frame);
        }
    }
}

void BaslerCameraWidget::populateBrowser() {
    m_populating_browser = true;
    m_variantEditor->blockSignals(true);
    m_variantManager->clear();

    populateBrowser_Device(m_device.get(), m_variantManager, m_variantEditor);
    populateBrowser_BaslerConfig(&m_params, m_variantManager, m_variantEditor);

    m_variantEditor->blockSignals(false);
    m_populating_browser = false    ;
}

// =============================================================================
//  Calibration
// =============================================================================

void BaslerCameraWidget::initCalibrationUi() {
    setCalibBoardPreset(m_params.calibBoardPreset());
    refreshCalibrationStatusLabels();
    ui->btn_calib_apply->setEnabled(false);

    // init table here
    ui->table_calibration_points->setImagePoints(m_params.calibrator().getImagePts());
    ui->table_calibration_points->setWorldPoints(m_params.calibrator().getRobotPts());
}

void BaslerCameraWidget::setCalibBoardPreset(const QString &preset) {
    m_board = calib::CalibrationBoardFactory::createFromPreset(preset.toStdString());
    if (!m_board) {
        LOG_DEV_ERR << "Calibration board preset unknown:" << preset;
        ui->table_calibration_points->setRowCount(0);
        refreshBoardInfoLabel();
        return;
    }

    m_params.setCalibBoardPreset(preset);
    m_board->setBinarizeThreshold(m_params.calibThreshold());

    // Default world XY for the 4 corner points comes straight from the board
    // (board-frame mm, z=0). The user edits these to map board frame -> robot
    // frame before pressing Apply.
    const auto corners = m_board->cornerObjectPointsXY();
    ui->table_calibration_points->setRowCount(static_cast<int>(corners.size()));
    ui->table_calibration_points->setWorldPointsXY(corners);
    refreshBoardInfoLabel();
}

void BaslerCameraWidget::refreshBoardInfoLabel() {
    if (!m_board) {
        ui->lb_board_info->setText(tr("Board: (none)"));
        return;
    }
    ui->lb_board_info->setText(
        tr("Board: %1 (%2 dots)")
            .arg(m_params.calibBoardPreset())
            .arg(m_board->totalDots()));
}

void BaslerCameraWidget::refreshCalibrationStatusLabels(const QString &extraSuffix) {
    const calib::Calibrator &c = m_params.calibrator();
    QString status = c.isCalibrated() ? tr("calibrated") : tr("not calibrated");
    if (!extraSuffix.isEmpty()) {
        status += QStringLiteral(" %1").arg(extraSuffix);
    }
    ui->label_calib_status->setText(tr("Calibration status: %1").arg(status));

    if (c.isCalibrated()) {
        ui->label_calib_error_mm->setText(
            tr("Reproject error (mm): %1").arg(c.reprojectionErrorMm(), 0, 'f', 4));
        ui->label_calib_error_pixel->setText(
            tr("Reproject error (pixel): %1").arg(c.reprojectionErrorPx(), 0, 'f', 4));
        double rotDeg = c.rotateImageToRobot(0.0);
        ui->label_calib_rotate_image2world->setText(
            tr("Z axis rotate (Image to World): %1 deg").arg(rotDeg, 0, 'f', 4));
    } else {
        ui->label_calib_error_mm->setText(tr("Reproject error (mm):"));
        ui->label_calib_error_pixel->setText(tr("Reproject error (pixel):"));
        ui->label_calib_rotate_image2world->setText(
            tr("Z axis rotate (Image to World):"));
    }
}

void BaslerCameraWidget::btn_setup_board_clicked() {
    CalibrationBoardDialog dlg(m_params.calibBoardPreset(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QString preset = dlg.selectedPreset();
    if (preset.isEmpty() || preset == m_params.calibBoardPreset()) {
        return;
    }
    // Per requirement: keep existing table data (user already entered world
    // coords against the previous board); only refresh board info and mark the
    // calibration as needing re-apply.
    m_board = calib::CalibrationBoardFactory::createFromPreset(preset.toStdString());
    if (!m_board) {
        QMessageBox::warning(this, tr("Calibration board"),
                             tr("Unknown preset: %1").arg(preset));
        return;
    }
    m_params.setCalibBoardPreset(preset);
    m_board->setBinarizeThreshold(m_params.calibThreshold());
    m_camera->setBaslerGigeConfig(m_params);
    refreshBoardInfoLabel();
    ui->btn_calib_apply->setEnabled(true);
    refreshCalibrationStatusLabels(tr("(point changed)"));
}

void BaslerCameraWidget::btn_calib_threshold_clicked() {
    if (!m_runner || !m_camera || !m_camera->isDeviceConnected()) {
        QMessageBox::warning(this, tr("Calibration"),
                             tr("Camera is not connected."));
        return;
    }
    if (!m_board) {
        QMessageBox::warning(this, tr("Calibration"),
                             tr("No calibration board configured."));
        return;
    }

    m_thresholdDlg =
        new CalibrationThresholdDialog(m_board.get(), m_params.calibThreshold(), this);

    // Re-grab button and the initial open both pull a fresh frame from the
    // camera; the result is routed back via onCameraGrabFinished().
    connect(m_thresholdDlg, &CalibrationThresholdDialog::regrabRequested,
            this, [this]() {
                if (m_runner && m_camera && m_camera->isDeviceConnected()) {
                    m_pendingThresholdGrab = true;
                    m_runner->requestSingleShot();
                }
            });

    m_pendingThresholdGrab = true;
    m_runner->requestSingleShot();

    const int rc = m_thresholdDlg->exec();

    if (rc == QDialog::Accepted) {
        const int threshold = m_thresholdDlg->threshold();
        m_params.setCalibThreshold(threshold);
        m_board->setBinarizeThreshold(threshold);
        m_camera->setBaslerGigeConfig(m_params);
        refreshCalibrationStatusLabels(tr("(threshold changed)"));
    } else {
        // Preview mutated the board's threshold; restore the stored value.
        m_board->setBinarizeThreshold(m_params.calibThreshold());
    }

    // Drop the dialog; clear the pointer first so a late grab is ignored.
    m_pendingThresholdGrab = false;
    CalibrationThresholdDialog *dlg = m_thresholdDlg;
    m_thresholdDlg = nullptr;
    dlg->deleteLater();
}

void BaslerCameraWidget::btn_calib_detect_clicked() {
    if (!m_runner || !m_camera || !m_camera->isDeviceConnected()) {
        QMessageBox::warning(this, tr("Calibration"),
                             tr("Camera is not connected."));
        return;
    }
    if (!m_board) {
        QMessageBox::warning(this, tr("Calibration"),
                             tr("No calibration board configured."));
        return;
    }
    m_pendingCalibDetect = true;
    m_runner->requestSingleShot();
}

void BaslerCameraWidget::btn_calib_apply_clicked() {
    if (!m_board) {
        QMessageBox::warning(this, tr("Calibration"),
                             tr("No calibration board configured."));
        return;
    }

    const auto imgPts   = ui->table_calibration_points->imagePoints();
    const auto worldPts = ui->table_calibration_points->worldPoints();

    if (imgPts.size() < 4 || imgPts.size() != worldPts.size()) {
        QMessageBox::warning(
            this, tr("Calibration"),
            tr("Need at least 4 valid image/world point pairs. "
               "Run Detect and fill in the world coordinates first."));
        return;
    }

    // Basic sanity: reject the all-zero image-points case that happens before
    // a detect has populated the table.
    bool anyImgNonZero = false;
    for (const auto &p : imgPts) {
        if (p.x != 0.f || p.y != 0.f) { anyImgNonZero = true; break; }
    }
    if (!anyImgNonZero) {
        QMessageBox::warning(this, tr("Calibration"),
                             tr("Image points are empty — run Detect first."));
        return;
    }

    calib::Calibrator calib;
    calib.addCorrespondences(imgPts, worldPts);
    if (!calib.calibrate()) {
        ui->label_calib_status->setText(tr("Calibration status: calibrate failed"));
        return;
    }

    m_params.setCalibrator(calib);
    m_camera->setBaslerGigeConfig(m_params);

    refreshCalibrationStatusLabels();
    ui->btn_calib_apply->setEnabled(false);
}

void BaslerCameraWidget::onCalibPointsEdited() {
    ui->btn_calib_apply->setEnabled(true);
    refreshCalibrationStatusLabels(tr("(point changed)"));
}
