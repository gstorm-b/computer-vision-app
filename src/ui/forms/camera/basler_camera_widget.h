#ifndef BASLER_CAMERA_WIDGET_H
#define BASLER_CAMERA_WIDGET_H

#include <QWidget>
#include "DockWidget.h"
#include "ui/forms/camera/basler_cam_select_dialog.h"

#include "qtpropertybrowser/qtpropertymanager.h"
#include "qtpropertybrowser/qtvariantproperty.h"
#include "qtpropertybrowser/qttreepropertybrowser.h"

#include "device/idevice.h"
#include "device/device_manager.h"
#include "device/camera/camera_basler_gige.h"

#include "runtime/camera_runner.h"
#include "ui/forms/device_widget.h"

#include "calibration/calibration_board.h"

#include <memory>

namespace Ui {
/// Forward declaration of the uic-generated form class for basler_camera_widget.ui.
class BaslerCameraWidget;
}

class CalibrationThresholdDialog;

/// Device widget for a Basler GigE camera: exposes device/camera parameters through
/// a QtPropertyBrowser, drives connect/trigger/save actions via CameraRunner, and
/// hosts the board-calibration workflow (board setup, threshold tuning, corner
/// detection, and calibration apply) for this camera.
class BaslerCameraWidget : public IDeviceWidget
{
    Q_OBJECT

public:
    /// Runner is owned by TaskRunner (not by this widget).  It must outlive
    /// this widget — TaskRunner guarantees this as long as the device stays
    /// assigned to the task.  Pass nullptr only if the caller explicitly
    /// disables thread-routed access (rare; status lamp will stay disabled).
    /// @param dv the Basler camera device this widget will control (expected to be
    ///        castable to vc::device::BaslerGigECamera)
    /// @param runner thread-routed access point to the camera; nullptr disables control
    /// @param dock optional dock widget whose title is kept in sync with the device name
    explicit BaslerCameraWidget(std::shared_ptr<vc::device::IDevice> dv,
                                vc::runtime::CameraRunner *runner,
                                ads::CDockWidget *dock = nullptr,
                                QWidget *parent = nullptr);
    /// Destroys the widget and its generated UI (ui).
    ~BaslerCameraWidget();

    /// Returns the id of the underlying device.
    QString deviceId() override;
    /// Writes the currently-edited BaslerGigeCfg (m_params) onto the camera.
    void loadConfigToDevice() override;
    /// Repopulates the property browser from the device and camera config (calls populateBrowser()).
    void loadConfigToWidget() override;
    /// Replaces the dock widget associated with this camera widget (used for title updates).
    void setDockWidget(ads::CDockWidget *dock);

private:
    /// One-time setup run from the constructor: initializes the property browser,
    /// wires all button/table signals, resolves m_camera from m_device, loads the
    /// initial config, connects the calibration UI, and wires the CameraRunner
    /// signals (connect status / grab finished) onto this widget.
    void initCameraWiget();
    /// Adds a "Device Information" group to browser populated from gadget's
    /// Q_PROPERTYs (via the IDevice meta-object), skipping objectName.
    void populateBrowser_Device(vc::device::IDevice *gadget, QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser);
    /// Adds a "Camera basler" group to browser populated from gadget's Q_GADGET
    /// properties, then applies the exposure/gain min-max attributes and the
    /// autoBacklightLine output-line completer list from gadget.
    void populateBrowser_BaslerConfig(vc::device::BaslerGigeCfg *gadget, QtVariantPropertyManager *manager, QtTreePropertyBrowser *browser);
    /// Sets the "minimum"/"maximum" attributes of a double-valued property.
    void setDoublePropertyLimit(QtVariantProperty *variant, double &max, double &min);
    /// Sets the "minimum"/"maximum" attributes of an int-valued property.
    void setIntPropertyLimit(QtVariantProperty *variant, int &max, int &min);
    /// Sets the dock widget's window title to the device's current name; no-op when m_dock is null.
    void updateDockTitle();

private slots:
    /// Handles edits made in the property browser: renames the device (via
    /// DeviceManager, rejecting duplicate names) when the "name" property changes,
    /// or writes the edited value into m_params and pushes it to the camera
    /// (requesting a runner-side apply when connected) for any BaslerGigeCfg property.
    void onPropertyValueChanged(QtProperty *property, const QVariant &variant);
    /// Dispatches on the camera's connection status: runs onCameraConnected()/
    /// onCameraDisconnected(), or shows a warning dialog on ConnectFailed.
    void onCameraConnectStatusChanged(vc::device::ConnectStatus status);

    /// Called when the camera-selection dialog closes; on accept, copies the chosen
    /// Pylon device's identity fields into m_params and applies them to the camera.
    void cameraSelectionFinished(bool isAccept, Pylon::CDeviceInfo device);
    /// Opens the Basler camera selection dialog.
    void btn_choose_camera_clicked();
    /// Connects or disconnects the camera via the runner, after validating the
    /// configured IP address; shows a warning if no runner is available or the IP is invalid.
    void btn_connect_clicked();
    /// Requests a single-shot grab from the runner, if the camera is connected.
    void btn_trigger_clicked();
    // void btn_auto_shot_clicked();
    // void btn_backlight_toggle_clicked();
    /// Currently unimplemented (empty body).
    void btn_save_image_clicked();

    /// Opens the calibration-board selection dialog and, on a changed preset,
    /// rebuilds m_board while keeping the existing table data.
    void btn_setup_board_clicked();
    /// Opens the threshold-tuning dialog (CalibrationThresholdDialog), triggers an
    /// initial grab (and further grabs on Re-grab), and on accept stores the chosen
    /// binarization threshold into m_params/m_board.
    void btn_calib_threshold_clicked();
    /// Requests a single-shot grab flagged for calibration-corner detection
    /// (handled in onCameraGrabFinished()) if the camera and board are ready.
    void btn_calib_detect_clicked();
    /// Runs calibration from the table's image/world point pairs (after validating
    /// there are at least 4 pairs and the image points aren't all zero) and stores
    /// the resulting Calibrator into m_params.
    void btn_calib_apply_clicked();
    /// Marks calibration as needing re-apply after the user edits a calibration point.
    void onCalibPointsEdited();

private:
    /// Updates the connection-status label/icon and reloads m_params/the browser
    /// from the now-connected camera.
    void onCameraConnected();
    /// Updates the connection-status label/icon to reflect the disconnected camera.
    void onCameraDisconnected();
    /// Displays a successful grab in the image view, then routes the frame to
    /// whichever pending operation requested it: calibration corner detection
    /// (m_pendingCalibDetect) or the threshold-tuning dialog (m_pendingThresholdGrab).
    void onCameraGrabFinished(vc::device::GrabResult result);
    /// Clears and rebuilds the property browser from the device and Basler config
    /// while guarding against re-entrant valueChanged signals (m_populating_browser).
    void populateBrowser();

    /// Sets up the calibration board/table from the stored preset and refreshes the status labels.
    void initCalibrationUi();
    /// Rebuilds m_board from `preset`, seeds the table's world XY corners from the
    /// board, and stores the preset in m_params. Clears the table if the preset is unknown.
    void setCalibBoardPreset(const QString &preset);
    /// Updates the board-info label with the current preset name and dot count (or "(none)").
    void refreshBoardInfoLabel();
    /// Updates the calibration status/reprojection-error/rotation labels from
    /// m_params.calibrator(), optionally appending `extraSuffix` to the status text.
    void refreshCalibrationStatusLabels(const QString &extraSuffix = QString());

private:
    Ui::BaslerCameraWidget *ui;  ///< uic-generated form (owns the child widgets/layout).
    std::shared_ptr<vc::device::IDevice> m_device;  ///< The device this widget was constructed with.
    vc::device::BaslerGigECamera *m_camera{nullptr};  ///< m_device qobject_cast to the concrete camera type; not owned.
    ads::CDockWidget *m_dock{nullptr};  ///< Dock hosting this widget, kept in title-sync by updateDockTitle(); not owned.

    vc::device::BaslerGigeCfg m_params;  ///< Working copy of the camera's configuration edited via the property browser.
    QList<vc::device::BaslerGigeCfg> m_cam_list;  ///< Reserved for a multi-camera-config list; currently unused.

    BaslerCamSelectDialog *m_camera_select_dialog{nullptr};  ///< Dialog for picking a Basler device on the network; owned by this widget (child of `this`).

    /// Not owned — provided by TaskRunner.  Widget never creates a thread.
    vc::runtime::CameraRunner *m_runner{nullptr};

    bool m_populating_browser{false};  ///< Guards onPropertyValueChanged() against reacting to programmatic updates from populateBrowser().

    /// Calibration board the user picked; rebuilt on preset change.
    std::unique_ptr<calib::CalibrationBoard> m_board;
    /// True for the single grabFinished that follows btn_calib_detect_clicked();
    /// tells onCameraGrabFinished() to run board detection on that frame.
    bool m_pendingCalibDetect{false};

    /// Threshold-tuning dialog (modal, open only while tuning). Frames grabbed
    /// while m_pendingThresholdGrab is set are routed into this dialog.
    CalibrationThresholdDialog *m_thresholdDlg{nullptr};
    bool m_pendingThresholdGrab{false};  ///< True while a grab is pending for delivery to m_thresholdDlg.
};

#endif // BASLER_CAMERA_WIDGET_H
