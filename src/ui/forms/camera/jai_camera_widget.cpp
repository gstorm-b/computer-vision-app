#include "jai_camera_widget.h"
#include "ui_jai_camera_widget.h"

#include <QFileDialog>
#include <QHostAddress>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStyle>

#include "core/qgadget_macro.h"
#include "core/utils/windows_helper.h"

#include "calibration/calibration_board_factory.h"
#include "ui/widgets/calibration/calibration_board_dialog.h"
#include "ui/widgets/calibration/calibration_threshold_dialog.h"
#include "ui/widgets/calibration/calibration_points_table.h"

using vc::device::jai::JaiIOLine;

namespace {

/// Converts an OpenCV Mat (8-bit grayscale, BGR, or BGRA) to a QPixmap for display.
/// @return the converted pixmap, or a null QPixmap for an unsupported type
QPixmap cvMatToQPixmap(const cv::Mat &mat)
{
    QImage qimg;
    if (mat.type() == CV_8UC1) {
        qimg = QImage(mat.data, mat.cols, mat.rows,
                      static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        qimg = QImage(rgb.data, rgb.cols, rgb.rows,
                      static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        qimg = QImage(rgba.data, rgba.cols, rgba.rows,
                      static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    } else {
        return QPixmap();
    }
    return QPixmap::fromImage(qimg);
}

/// Creates and registers a QtVariantProperty for one Qt meta-property, handling enum types
/// specially and applying the display name / minimum / maximum class-info attributes.
/// @return the created property, or nullptr for objectName / on failure
QtVariantProperty *addPropertyToBrowser(const QMetaObject &meta, QMetaProperty &prop,
                                        QVariant &value, QtVariantPropertyManager *manager)
{
    const QString propName = QString::fromLatin1(prop.name());
    if (propName == QLatin1String("objectName")) {
        return nullptr;
    }

    QtVariantProperty *variantProp = nullptr;
    if (prop.isEnumType()) {
        variantProp = manager->addProperty(QtVariantPropertyManager::enumTypeId(), propName);
        if (!variantProp) {
            return nullptr;
        }
        // Enum labels, translated in the enum's own scope — see vc::gadget_meta::enumKeyNames.
        variantProp->setAttribute(QLatin1String("enumNames"),
                                  vc::gadget_meta::enumKeyNames(prop));
        variantProp->setValue(value.toInt());
    } else {
        variantProp = manager->addProperty(value.userType(), propName);
        if (!variantProp) {
            return nullptr;
        }
        variantProp->setValue(value);
    }

    variantProp->setDisplayName(vc::gadget_meta::displayName(meta, prop.name()));

    const int typeId = value.userType();
    const int minIdx = meta.indexOfClassInfo(QStringLiteral("%1_min").arg(propName).toUtf8());
    if (minIdx != -1) {
        if (typeId == QMetaType::Int) {
            variantProp->setAttribute("minimum", QString(meta.classInfo(minIdx).value()).toInt());
        } else if (typeId == QMetaType::Double) {
            variantProp->setAttribute("minimum",
                                      QString(meta.classInfo(minIdx).value()).toDouble());
        }
    }

    const int maxIdx = meta.indexOfClassInfo(QStringLiteral("%1_max").arg(propName).toUtf8());
    if (maxIdx != -1) {
        if (typeId == QMetaType::Int) {
            variantProp->setAttribute("maximum", QString(meta.classInfo(maxIdx).value()).toInt());
        } else if (typeId == QMetaType::Double) {
            variantProp->setAttribute("maximum",
                                      QString(meta.classInfo(maxIdx).value()).toDouble());
        }
    }

    if (!prop.isWritable()) {
        variantProp->setEnabled(false);
    }
    return variantProp;
}

} // namespace

// ── Property browser ─────────────────────────────────────────────────────────────────────────

void JaiCameraWidget::populateBrowser_Device(vc::device::IDevice *gadget,
                                             QtVariantPropertyManager *manager,
                                             QtTreePropertyBrowser *browser)
{
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Device Information"));
    browser->addProperty(topItem);

    const QMetaObject *meta = gadget->metaObject();
    for (int i = 0; i < meta->propertyCount(); ++i) {
        QMetaProperty prop = meta->property(i);
        QVariant value = gadget->property(prop.name());
        if (QtVariantProperty *variantProp = addPropertyToBrowser(*meta, prop, value, manager)) {
            topItem->addSubProperty(variantProp);
        }
    }
}

void JaiCameraWidget::populateBrowser_JaiConfig(vc::device::JaiGigeCfg *gadget,
                                                QtVariantPropertyManager *manager,
                                                QtTreePropertyBrowser *browser)
{
    QtProperty *topItem = manager->addProperty(QtVariantPropertyManager::groupTypeId(),
                                               QLatin1String("Camera JAI"));
    browser->addProperty(topItem);

    const QMetaObject &meta = gadget->getMetaObject();
    for (int i = 0; i < meta.propertyCount(); ++i) {
        QMetaProperty prop = meta.property(i);
        QVariant value = prop.readOnGadget(gadget);

        QtVariantProperty *variantProp = addPropertyToBrowser(meta, prop, value, manager);
        if (!variantProp) {
            // Null-checked before use, unlike the Basler widget, which dereferences the result
            // and would crash on any property the manager refuses to create.
            continue;
        }
        topItem->addSubProperty(variantProp);

        // Limits from the CAMERA, overriding the static class-info bounds the macro declared:
        // those are a sane envelope for the property editor, these are what this model accepts.
        //
        // Only when the camera reported a usable range. A degenerate min==max (which is what
        // a failed range read leaves behind) reaches the browser as a field pinned to one value,
        // and the operator sees an exposure control that will not move with nothing to explain
        // it. Falling back to the declared envelope is wrong-but-usable; pinning it is neither.
        const QString name = variantProp->propertyName();
        if (name == QLatin1String("paramsExposureTime")) {
            m_exposureProperty = variantProp;
            if (gadget->m_paramsExposureMin < gadget->m_paramsExposureMax) {
                variantProp->setAttribute("minimum", gadget->m_paramsExposureMin);
                variantProp->setAttribute("maximum", gadget->m_paramsExposureMax);
            }
        } else if (name == QLatin1String("paramsGain")) {
            m_gainProperty = variantProp;
            if (gadget->m_paramsGainMin < gadget->m_paramsGainMax) {
                variantProp->setAttribute("minimum", double(gadget->m_paramsGainMin));
                variantProp->setAttribute("maximum", double(gadget->m_paramsGainMax));
            }
        } else if (name == QLatin1String("autoBacklightLine")) {
            QStringList outputs;
            for (const JaiIOLine &io : gadget->m_ioCapabilities) {
                if (io.can_be_output) {
                    outputs.append(io.name);
                }
            }
            variantProp->setAttribute("completer", outputs);
        }
    }
}

// ── Construction ─────────────────────────────────────────────────────────────────────────────

JaiCameraWidget::JaiCameraWidget(std::shared_ptr<vc::device::IDevice> dv,
                                 vc::runtime::CameraRunner *runner,
                                 ads::CDockWidget *dock, QWidget *parent)
    : IDeviceWidget(parent)
    , ui(new Ui::JaiCameraWidget)
    , m_device(dv)
    , m_dock(dock)
    , m_runner(runner)
{
    ui->setupUi(this);
    initCameraWidget();
}

JaiCameraWidget::~JaiCameraWidget()
{
    // A camera left streaming into a destroyed widget keeps acquiring with nobody reading it, and
    // the runner would keep re-emitting frames at a dangling receiver. The device also stops on
    // disconnect and on task teardown; this covers the panel simply being closed.
    if (m_runner && m_continuousActive) {
        m_runner->requestContinuousStop();
    }
    delete ui;
}

QString JaiCameraWidget::deviceId()
{
    return m_device->id();
}

void JaiCameraWidget::loadConfigToDevice()
{
    if (!m_camera) {
        return;
    }
    m_camera->setJaiGigeConfig(m_params);
}

void JaiCameraWidget::loadConfigToWidget()
{
    populateBrowser();
}

void JaiCameraWidget::setDockWidget(ads::CDockWidget *dock)
{
    m_dock = dock;
}

void JaiCameraWidget::initCameraWidget()
{
    initPropertyBrowser();

    ui->btn_connect->setIcon(svgIcon(":/resrc/icon/plug_disconnected.svg"));
    ui->btn_trigger->setIcon(svgIcon(":/resrc/icon/capture.svg"));

    connect(ui->btn_select_camera, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_choose_camera_clicked);
    connect(ui->btn_connect, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_connect_clicked);
    connect(ui->btn_trigger, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_trigger_clicked);
    connect(ui->btn_auto_shot, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_auto_shot_clicked);
    connect(ui->btn_backlight_toggle, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_backlight_toggle_clicked);
    connect(ui->btn_save_image, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_save_image_clicked);

    connect(ui->btn_setup_board, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_setup_board_clicked);
    connect(ui->btn_calib_threshold, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_calib_threshold_clicked);
    connect(ui->btn_calib_detect, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_calib_detect_clicked);
    connect(ui->btn_calib_apply, &QPushButton::clicked,
            this, &JaiCameraWidget::btn_calib_apply_clicked);
    connect(ui->table_calibration_points, &CalibrationPointsTable::pointsEdited,
            this, &JaiCameraWidget::onCalibPointsEdited);

    if (m_device) {
        m_camera = qobject_cast<vc::device::JaiGigECamera *>(m_device.get());
        if (!m_camera) {
            LOG_DEV_ERR << "JaiCameraWidget: expected JaiGigECamera but got" << m_device->id();
            setEnabled(false);
        } else {
            m_params = m_camera->jaiGigeConfig();
            loadConfigToWidget();
            connect(m_variantManager, &QtVariantPropertyManager::valueChanged,
                    this, &JaiCameraWidget::onPropertyValueChanged);
            initCalibrationUi();
        }
    }

    ui->splitter_main->setStretchFactor(0, 7);
    ui->splitter_main->setStretchFactor(1, 3);

    m_camera_select_dialog = new JaiCamSelectDialog(this);
    connect(m_camera_select_dialog, &JaiCamSelectDialog::userSelectionFinished,
            this, &JaiCameraWidget::cameraSelectionFinished);

    // ── Wire to the runner, NOT to the device ────────────────────────────────────────────────
    // The runner forwards device signals onto the GUI thread; that is the whole point of routing
    // through TaskRunner. The widget never touches QThread or moveToThread().
    if (m_runner && m_camera) {
        connect(m_runner, &vc::runtime::CameraRunner::connectStatusChanged,
                this, &JaiCameraWidget::onCameraConnectStatusChanged);
        connect(m_runner, &vc::runtime::CameraRunner::grabFinished,
                this, &JaiCameraWidget::onCameraGrabFinished);
        connect(m_runner, &vc::runtime::CameraRunner::parametersApplied,
                this, &JaiCameraWidget::onParametersApplied);
        connect(m_runner, &vc::runtime::CameraRunner::continuousFrameReady,
                this, &JaiCameraWidget::onContinuousFrameReady);
        connect(m_runner, &vc::runtime::CameraRunner::continuousStateChanged,
                this, &JaiCameraWidget::onContinuousStateChanged);
        connect(m_runner, &vc::runtime::CameraRunner::backlightStateChanged,
                this, &JaiCameraWidget::onBacklightStateChanged);
    } else {
        LOG_DEV_ERR << "JaiCameraWidget: no runner provided — control disabled";
    }

    onCameraConnectStatusChanged(m_device && m_device->isDeviceConnected()
                                     ? vc::device::ConnectStatus::Connected
                                     : vc::device::ConnectStatus::Disconnected);
}

void JaiCameraWidget::updateDockTitle()
{
    if (m_dock == nullptr) {
        return;
    }
    m_dock->setWindowTitle(m_device->name());
}

// ── Property edits ───────────────────────────────────────────────────────────────────────────

void JaiCameraWidget::onPropertyValueChanged(QtProperty *property, const QVariant &variant)
{
    if (m_populating_browser) {
        return;
    }

    const QString propName = property->propertyName();
    const QMetaObject *deviceMeta = m_device->metaObject();
    const QMetaObject &configMeta = m_params.getMetaObject();

    if (deviceMeta->indexOfProperty(propName.toUtf8()) != -1) {
        if (propName == QLatin1String("name")) {
            const QString newName = variant.toString();
            if (m_device->name() == newName) {
                return;   // avoid the loop the valueChanged signal would otherwise make
            }
            if (!m_device->deviceManager()->changeDeviceName(m_device->id(), newName)) {
                LOG_USER_WARN << tr("Cannot rename device to \"%1\": the name is already in use.")
                                     .arg(newName);
                m_variantManager->setValue(property, m_device->name());
            } else {
                updateDockTitle();
            }
        }
        return;
    }

    const int index = configMeta.indexOfProperty(propName.toUtf8());
    if (index != -1) {
        QMetaProperty configProp = configMeta.property(index);
        configProp.writeOnGadget(&m_params, variant);
        m_camera->setJaiGigeConfig(m_params);
        if (m_camera->isDeviceConnected() && m_runner) {
            m_runner->requestApplyParams();
        }
    }
}

// ── Connection ───────────────────────────────────────────────────────────────────────────────

void JaiCameraWidget::onCameraConnectStatusChanged(vc::device::ConnectStatus status)
{
    switch (status) {
    case vc::device::ConnectStatus::Connected:
        onCameraConnected();
        break;
    case vc::device::ConnectStatus::Disconnected:
        onCameraDisconnected();
        break;
    case vc::device::ConnectStatus::LostConnected:
        // Distinct from a clean disconnect, and shown as such: the runtime is reconnecting, and
        // an operator who reads "Disconnected" reaches for the Connect button instead of the
        // cable. The Basler widget shows nothing at all for this case.
        onCameraDisconnected();
        ui->lb_connection_status->setText(tr("Link lost — reconnecting"));
        break;
    case vc::device::ConnectStatus::ConnectFailed:
        QMessageBox::warning(this, tr("Connect error"), m_camera->lastMsg());
        onCameraDisconnected();
        break;
    // Enumerated explicitly (no default:) so adding a ConnectStatus value surfaces a warning here.
    case vc::device::ConnectStatus::NoConnection:
    case vc::device::ConnectStatus::Connecting:
        break;
    }
}

void JaiCameraWidget::applyConnectionVisual(bool connected)
{
    ui->lb_connection_status->setProperty("connectionState",
                                          connected ? "connected" : "disconnected");
    ui->lb_connection_status->style()->unpolish(ui->lb_connection_status);
    ui->lb_connection_status->style()->polish(ui->lb_connection_status);
    ui->lb_connection_status->update();
    ui->btn_connect->setIcon(svgIcon(connected ? ":/resrc/icon/plug_connected.svg"
                                               : ":/resrc/icon/plug_disconnected.svg"));

    // Disabled rather than left clickable-and-inert. The sibling action buttons guard inside
    // their slots instead, which is why this one was found dead by hand rather than by the eye:
    // an enabled button that does nothing is indistinguishable from a broken lamp.
    ui->btn_backlight_toggle->setEnabled(connected);
}

void JaiCameraWidget::onCameraConnected()
{
    ui->lb_connection_status->setText(tr("Connected"));
    // "connected", not "disconnected". The Basler widget sets the disconnected state string in
    // both handlers, so its lamp never turns green however the QSS is written.
    applyConnectionVisual(true);

    // Re-read: connect is what fills in the model, serial, pixel format and the exposure/gain
    // limits this camera actually reports.
    m_params = m_camera->jaiGigeConfig();
    loadConfigToWidget();
}

void JaiCameraWidget::onCameraDisconnected()
{
    ui->lb_connection_status->setText(tr("Disconnected"));
    applyConnectionVisual(false);
    // The device stops streaming as part of its own teardown, but a disconnect that happens
    // without a state report — a link lost while the widget was hidden, say — would otherwise
    // leave the button reading "Stop live view" on a camera that is gone.
    onContinuousStateChanged(false);
}

void JaiCameraWidget::cameraSelectionFinished(bool isAccept, JaiCameraInfo camera)
{
    if (!isAccept) {
        return;
    }
    if (!camera.configurationValid) {
        QMessageBox::warning(this, tr("Select camera"),
                             tr("Camera %1 has no valid IP configuration on this subnet and "
                                "cannot be opened. Assign it an address first.")
                                 .arg(camera.modelName));
        return;
    }

    m_params.m_modelName       = camera.modelName;
    m_params.m_serialNumber    = camera.serialNumber;
    m_params.m_userDefinedName = camera.userDefinedName;
    m_params.m_ipAddress       = camera.ipAddress;

    loadConfigToWidget();
    m_camera->setJaiGigeConfig(m_params);
}

void JaiCameraWidget::btn_choose_camera_clicked()
{
    m_camera_select_dialog->showCameraSelectForm();
}

void JaiCameraWidget::btn_connect_clicked()
{
    if (!m_runner) {
        QMessageBox::warning(this, tr("Connect error"), tr("No camera runner available."));
        return;
    }

    if (!m_camera->isDeviceConnected()) {
        QHostAddress address;
        if (!address.setAddress(m_params.ipAddress())) {
            QMessageBox::warning(this, tr("Connect error"), tr("IP address invalid format."));
            return;
        }
        m_runner->requestConnect();
    } else {
        m_runner->requestDisconnect();
    }
}

void JaiCameraWidget::btn_trigger_clicked()
{
    if (!m_camera->isDeviceConnected() || !m_runner) {
        return;
    }
    m_runner->requestSingleShot();
}

void JaiCameraWidget::btn_auto_shot_clicked()
{
    if (!m_camera->isDeviceConnected() || !m_runner) {
        return;
    }
    // Asks for the opposite of what the CAMERA reports, not of what this button shows. The two
    // can disagree — a single shot pre-empts streaming, and so does a pulled cable — and keying
    // off the label would then send the wrong request.
    if (m_continuousActive) {
        m_runner->requestContinuousStop();
    } else {
        m_runner->requestContinuousStart();
    }
}

void JaiCameraWidget::onContinuousFrameReady(vc::device::GrabResult result)
{
    if (!result.isGrabSuccess || result.frame.empty()) {
        return;
    }
    if (m_paintingContinuousFrame) {
        return;   // still painting the previous one — see m_paintingContinuousFrame
    }

    m_paintingContinuousFrame = true;
    m_lastFrame = result.frame;   // so Save image works on what the operator is looking at
    QPixmap frame = cvMatToQPixmap(result.frame);
    ui->image_view->loadImage(frame);
    m_paintingContinuousFrame = false;
}

void JaiCameraWidget::onContinuousStateChanged(bool active)
{
    m_continuousActive = active;
    ui->btn_auto_shot->setText(active ? tr("Stop live view") : tr("Continuous shot"));
}

void JaiCameraWidget::btn_backlight_toggle_clicked()
{
    if (!m_camera->isDeviceConnected() || !m_runner) {
        return;
    }
    // Asks for the opposite of the LAMP's reported level, not of this button's label — the same
    // rule btn_auto_shot_clicked() follows. The auto-backlight sequence switches the lamp around
    // every grab, so the label can be stale through no fault of the operator.
    m_runner->requestBacklight(!m_backlightOn);
}

void JaiCameraWidget::onBacklightStateChanged(bool on)
{
    m_backlightOn = on;
    ui->btn_backlight_toggle->setText(on ? tr("Backlight off") : tr("Backlight on"));
}

void JaiCameraWidget::btn_save_image_clicked()
{
    // Implemented rather than left empty (the Basler widget's is an empty body, so its Save
    // button does nothing at all): during commissioning the grabbed frame is the evidence.
    if (m_lastFrame.empty()) {
        QMessageBox::information(this, tr("Save image"),
                                 tr("No image to save — trigger a grab first."));
        return;
    }

    const QString suggested =
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        + QStringLiteral("/%1.png").arg(m_device->name());
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save image"), suggested, tr("Images (*.png *.bmp *.jpg)"));
    if (path.isEmpty()) {
        return;
    }

    if (!cv::imwrite(path.toStdString(), m_lastFrame)) {
        QMessageBox::warning(this, tr("Save image"),
                             tr("Could not write %1.").arg(path));
        return;
    }
    LOG_USER_INFO << tr("Image saved to %1").arg(path);
}

void JaiCameraWidget::onCameraGrabFinished(vc::device::GrabResult result)
{
    if (result.isGrabSuccess) {
        m_lastFrame = result.frame;
        // Named, not a temporary: ImageViewOnly::loadImage takes a non-const reference.
        QPixmap frame = cvMatToQPixmap(result.frame);
        ui->image_view->loadImage(frame);
    }

    // Detect requested by btn_calib_detect: run board detection on the fresh frame and push the
    // corner points into the table.
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
        cv::Mat debugImage;
        const bool ok = m_board->detect(result.frame, allPts, &cornerPts, &debugImage);
        if (!ok || cornerPts.empty()) {
            ui->label_calib_status->setText(tr("Calibration status: board detect failed"));
            return;
        }

        ui->table_calibration_points->setImagePoints(cornerPts);
        ui->btn_calib_apply->setEnabled(true);
        ui->label_calib_status->setText(tr("Calibration status: detected (point changed)"));
    }

    // Frame requested by the threshold-tuning dialog.
    if (m_pendingThresholdGrab) {
        m_pendingThresholdGrab = false;
        if (m_thresholdDlg && result.isGrabSuccess) {
            m_thresholdDlg->setImage(result.frame);
        }
    }
}

void JaiCameraWidget::onParametersApplied(bool ok)
{
    Q_UNUSED(ok);
    if (!m_camera) {
        return;
    }

    // The device re-reads the camera's ranges after every apply, because they move: raising the
    // acquisition frame rate lowers the exposure maximum. Pull the fresh limits and update just
    // those two attributes — a full populateBrowser() here would collapse the tree and steal
    // focus while the operator is still editing.
    const vc::device::JaiGigeCfg fresh = m_camera->jaiGigeConfig();
    m_params.m_paramsExposureMin = fresh.m_paramsExposureMin;
    m_params.m_paramsExposureMax = fresh.m_paramsExposureMax;
    m_params.m_paramsGainMin     = fresh.m_paramsGainMin;
    m_params.m_paramsGainMax     = fresh.m_paramsGainMax;

    const QSignalBlocker blocker(m_variantManager);
    if (m_exposureProperty && (fresh.m_paramsExposureMin < fresh.m_paramsExposureMax)) {
        m_exposureProperty->setAttribute("minimum", fresh.m_paramsExposureMin);
        m_exposureProperty->setAttribute("maximum", fresh.m_paramsExposureMax);
    }
    if (m_gainProperty && (fresh.m_paramsGainMin < fresh.m_paramsGainMax)) {
        m_gainProperty->setAttribute("minimum", double(fresh.m_paramsGainMin));
        m_gainProperty->setAttribute("maximum", double(fresh.m_paramsGainMax));
    }
}

void JaiCameraWidget::populateBrowser()
{
    m_populating_browser = true;
    m_variantEditor->blockSignals(true);
    m_variantManager->clear();
    // Cleared with the manager: the properties they point at are about to be destroyed.
    m_exposureProperty = nullptr;
    m_gainProperty = nullptr;

    populateBrowser_Device(m_device.get(), m_variantManager, m_variantEditor);
    populateBrowser_JaiConfig(&m_params, m_variantManager, m_variantEditor);

    m_variantEditor->blockSignals(false);
    m_populating_browser = false;
}

// ── Calibration ──────────────────────────────────────────────────────────────────────────────

void JaiCameraWidget::initCalibrationUi()
{
    setCalibBoardPreset(m_params.calibBoardPreset());
    refreshCalibrationStatusLabels();
    ui->btn_calib_apply->setEnabled(false);

    ui->table_calibration_points->setImagePoints(m_params.calibrator().getImagePts());
    ui->table_calibration_points->setWorldPoints(m_params.calibrator().getRobotPts());
}

void JaiCameraWidget::setCalibBoardPreset(const QString &preset)
{
    m_board = calib::CalibrationBoardFactory::createFromPreset(preset.toStdString());
    if (!m_board) {
        LOG_DEV_ERR << "Calibration board preset unknown:" << preset;
        ui->table_calibration_points->setRowCount(0);
        refreshBoardInfoLabel();
        return;
    }

    m_params.setCalibBoardPreset(preset);
    m_board->setBinarizeThreshold(m_params.calibThreshold());

    // Default world XY for the 4 corner points comes from the board (board-frame mm, z=0). The
    // operator edits these to map board frame -> robot frame before pressing Apply.
    const auto corners = m_board->cornerObjectPointsXY();
    ui->table_calibration_points->setRowCount(static_cast<int>(corners.size()));
    ui->table_calibration_points->setWorldPointsXY(corners);
    refreshBoardInfoLabel();
}

void JaiCameraWidget::refreshBoardInfoLabel()
{
    if (!m_board) {
        ui->lb_board_info->setText(tr("Board: (none)"));
        return;
    }
    ui->lb_board_info->setText(tr("Board: %1 (%2 dots)")
                                   .arg(m_params.calibBoardPreset())
                                   .arg(m_board->totalDots()));
}

void JaiCameraWidget::refreshCalibrationStatusLabels(const QString &extraSuffix)
{
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
        ui->label_calib_rotate_image2world->setText(
            tr("Z axis rotate (Image to World): %1 deg")
                .arg(c.rotateImageToRobot(0.0), 0, 'f', 4));
    } else {
        ui->label_calib_error_mm->setText(tr("Reproject error (mm):"));
        ui->label_calib_error_pixel->setText(tr("Reproject error (pixel):"));
        ui->label_calib_rotate_image2world->setText(tr("Z axis rotate (Image to World):"));
    }
}

void JaiCameraWidget::btn_setup_board_clicked()
{
    CalibrationBoardDialog dlg(m_params.calibBoardPreset(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QString preset = dlg.selectedPreset();
    if (preset.isEmpty() || preset == m_params.calibBoardPreset()) {
        return;
    }

    // Existing table data is kept: the operator has already entered world coordinates against
    // the previous board. Only the board info refreshes, and the calibration is marked stale.
    m_board = calib::CalibrationBoardFactory::createFromPreset(preset.toStdString());
    if (!m_board) {
        QMessageBox::warning(this, tr("Calibration board"),
                             tr("Unknown preset: %1").arg(preset));
        return;
    }
    m_params.setCalibBoardPreset(preset);
    m_board->setBinarizeThreshold(m_params.calibThreshold());
    m_camera->setJaiGigeConfig(m_params);
    refreshBoardInfoLabel();
    ui->btn_calib_apply->setEnabled(true);
    refreshCalibrationStatusLabels(tr("(point changed)"));
}

void JaiCameraWidget::btn_calib_threshold_clicked()
{
    if (!m_runner || !m_camera || !m_camera->isDeviceConnected()) {
        QMessageBox::warning(this, tr("Calibration"), tr("Camera is not connected."));
        return;
    }
    if (!m_board) {
        QMessageBox::warning(this, tr("Calibration"), tr("No calibration board configured."));
        return;
    }

    m_thresholdDlg =
        new CalibrationThresholdDialog(m_board.get(), m_params.calibThreshold(), this);

    // Re-grab and the initial open both pull a fresh frame; the result comes back through
    // onCameraGrabFinished().
    connect(m_thresholdDlg, &CalibrationThresholdDialog::regrabRequested, this, [this]() {
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
        m_camera->setJaiGigeConfig(m_params);
        refreshCalibrationStatusLabels(tr("(threshold changed)"));
    } else {
        // The preview mutated the board's threshold; restore the stored value.
        m_board->setBinarizeThreshold(m_params.calibThreshold());
    }

    // Drop the dialog, clearing the pointer first so a late grab is ignored.
    m_pendingThresholdGrab = false;
    CalibrationThresholdDialog *dlg = m_thresholdDlg;
    m_thresholdDlg = nullptr;
    dlg->deleteLater();
}

void JaiCameraWidget::btn_calib_detect_clicked()
{
    if (!m_runner || !m_camera || !m_camera->isDeviceConnected()) {
        QMessageBox::warning(this, tr("Calibration"), tr("Camera is not connected."));
        return;
    }
    if (!m_board) {
        QMessageBox::warning(this, tr("Calibration"), tr("No calibration board configured."));
        return;
    }
    m_pendingCalibDetect = true;
    m_runner->requestSingleShot();
}

void JaiCameraWidget::btn_calib_apply_clicked()
{
    if (!m_board) {
        QMessageBox::warning(this, tr("Calibration"), tr("No calibration board configured."));
        return;
    }

    const auto imgPts = ui->table_calibration_points->imagePoints();
    const auto worldPts = ui->table_calibration_points->worldPoints();

    if (imgPts.size() < 4 || imgPts.size() != worldPts.size()) {
        QMessageBox::warning(this, tr("Calibration"),
                             tr("Need at least 4 valid image/world point pairs. "
                                "Run Detect and fill in the world coordinates first."));
        return;
    }

    // Rejects the all-zero image-points case that exists before a detect has filled the table.
    bool anyImgNonZero = false;
    for (const auto &p : imgPts) {
        if (p.x != 0.f || p.y != 0.f) {
            anyImgNonZero = true;
            break;
        }
    }
    if (!anyImgNonZero) {
        QMessageBox::warning(this, tr("Calibration"),
                             tr("Image points are empty — run Detect first."));
        return;
    }

    calib::Calibrator calibrator;
    calibrator.addCorrespondences(imgPts, worldPts);
    if (!calibrator.calibrate()) {
        ui->label_calib_status->setText(tr("Calibration status: calibrate failed"));
        return;
    }

    m_params.setCalibrator(calibrator);
    m_camera->setJaiGigeConfig(m_params);

    refreshCalibrationStatusLabels();
    ui->btn_calib_apply->setEnabled(false);
}

void JaiCameraWidget::onCalibPointsEdited()
{
    ui->btn_calib_apply->setEnabled(true);
    refreshCalibrationStatusLabels(tr("(point changed)"));
}
