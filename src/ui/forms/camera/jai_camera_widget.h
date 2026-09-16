#ifndef JAI_CAMERA_WIDGET_H
#define JAI_CAMERA_WIDGET_H

#include <QWidget>
#include "DockWidget.h"
#include "ui/forms/camera/jai_cam_select_dialog.h"

#include "qtpropertybrowser/qtpropertymanager.h"
#include "qtpropertybrowser/qtvariantproperty.h"
#include "qtpropertybrowser/qttreepropertybrowser.h"

#include "device/idevice.h"
#include "device/device_manager.h"
#include "device/camera/camera_jai_gige.h"

#include "runtime/camera_runner.h"
#include "ui/forms/device_widget.h"

#include "calibration/calibration_board.h"

#include <memory>

namespace Ui {
/// Forward declaration of the uic-generated form class for jai_camera_widget.ui.
class JaiCameraWidget;
}

class CalibrationThresholdDialog;

/**
 * @file jai_camera_widget.h
 * @brief JaiCameraWidget — device widget for a JAI GigE camera.
 */

/**
 * @class JaiCameraWidget
 * @brief Device widget for a JAI GigE camera: exposes device/camera parameters through a
 *        QtPropertyBrowser, drives connect/trigger actions via CameraRunner, and hosts the
 *        board-calibration workflow for this camera.
 *
 * @note **A near-sibling of `BaslerCameraWidget`, deliberately, for now.** The two differ only
 *       in the config type, the exposure enum and the camera-select dialog; the device-info
 *       browser, the connect/trigger controls and the entire calibration workflow are generic
 *       over `CameraDevice`/`CameraCfg`. Unifying them is the right end state and is recorded in
 *       `docs/backlog/technical_debt_and_next_steps.md` — it is not done here because it would
 *       rewrite the panel of a camera already running in production, with no widget-level test
 *       to catch a regression, in the same change that introduces a second camera family.
 */
class JaiCameraWidget : public IDeviceWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the widget for @p dv, wiring it to @p runner for thread-routed camera
     *        access and to @p dock for title sync.
     * @param[in] dv the JAI camera device (expected to be a vc::device::JaiGigECamera)
     * @param[in] runner thread-routed access point to the camera; nullptr disables control
     * @param[in] dock optional dock whose title tracks the device name
     * @param[in] parent parent widget
     * @note @p runner is owned by TaskRunner, not by this widget, and must outlive it.
     */
    explicit JaiCameraWidget(std::shared_ptr<vc::device::IDevice> dv,
                             vc::runtime::CameraRunner *runner,
                             ads::CDockWidget *dock = nullptr,
                             QWidget *parent = nullptr);
    ~JaiCameraWidget();

    /// Returns the id of the underlying device.
    QString deviceId() override;
    /// Writes the currently-edited JaiGigeCfg (m_params) onto the camera.
    void loadConfigToDevice() override;
    /// Repopulates the property browser from the device and camera config.
    void loadConfigToWidget() override;
    /// Replaces the dock widget associated with this camera widget.
    void setDockWidget(ads::CDockWidget *dock);

private:
    /// One-time setup run from the constructor.
    void initCameraWidget();
    /// Adds a "Device Information" group populated from the IDevice meta-object.
    void populateBrowser_Device(vc::device::IDevice *gadget,
                                QtVariantPropertyManager *manager,
                                QtTreePropertyBrowser *browser);
    /// Adds a "Camera JAI" group populated from the config's Q_GADGET properties, then applies
    /// the exposure/gain limits and the backlight-line completer read from the camera.
    void populateBrowser_JaiConfig(vc::device::JaiGigeCfg *gadget,
                                   QtVariantPropertyManager *manager,
                                   QtTreePropertyBrowser *browser);
    /// Sets the dock's window title to the device's current name; no-op when m_dock is null.
    void updateDockTitle();

private slots:
    /// Handles edits in the property browser: renames the device (rejecting duplicates) for the
    /// "name" property, or writes the value into m_params and pushes it to the camera.
    void onPropertyValueChanged(QtProperty *property, const QVariant &variant);
    /// Dispatches on the camera's connection status.
    void onCameraConnectStatusChanged(vc::device::ConnectStatus status);

    /// Refreshes the exposure/gain limit attributes after the device applied parameters.
    /// @note GenICam ranges are not constants — raising the acquisition frame rate lowers the
    ///       exposure maximum, and vice versa. Without this the browser keeps showing the
    ///       limits read at connect, which is one of the ways "the exposure limit is wrong"
    ///       happens. Only the two attributes are touched: rebuilding the tree on every applied
    ///       edit would fight the operator's cursor.
    void onParametersApplied(bool ok);

    /// Called when the camera-selection dialog closes; on accept copies the chosen camera's
    /// identity into m_params and applies it.
    void cameraSelectionFinished(bool isAccept, JaiCameraInfo camera);
    /// Opens the JAI camera selection dialog.
    void btn_choose_camera_clicked();
    /// Connects or disconnects the camera via the runner, after validating the configured IP.
    void btn_connect_clicked();
    /// Requests a single-shot grab from the runner, if the camera is connected.
    void btn_trigger_clicked();
    /// Starts or stops continuous (live-view) acquisition, based on the device's current state.
    void btn_auto_shot_clicked();
    /// Forces the backlight on, or releases it, based on the lamp's reported level.
    void btn_backlight_toggle_clicked();
    /// Repaints the backlight button from the lamp's actual level.
    /// @note Driven by the device, like onContinuousStateChanged(): the auto-backlight sequence
    ///       switches the lamp around every grab, so the button cannot track its own last click.
    void onBacklightStateChanged(bool on);
    /// Displays a continuous frame, dropping it if the previous one is still being painted.
    void onContinuousFrameReady(vc::device::GrabResult result);
    /// Repaints the live-view button from the camera's actual streaming state.
    /// @note Driven by the device, not by the last click: streaming also stops for reasons this
    ///       widget did not cause — a single shot pre-empting it, a disconnect, a pulled cable.
    void onContinuousStateChanged(bool active);
    /// Saves the last grabbed frame to a file chosen by the operator.
    void btn_save_image_clicked();

    /// Opens the calibration-board selection dialog.
    void btn_setup_board_clicked();
    /// Opens the threshold-tuning dialog and feeds it freshly grabbed frames.
    void btn_calib_threshold_clicked();
    /// Requests a grab flagged for calibration-corner detection.
    void btn_calib_detect_clicked();
    /// Runs calibration from the table's image/world point pairs.
    void btn_calib_apply_clicked();
    /// Marks calibration as needing re-apply after the user edits a point.
    void onCalibPointsEdited();

private:
    /// Updates the status label and reloads m_params/the browser from the connected camera.
    void onCameraConnected();
    /// Updates the status label to reflect the disconnected camera.
    void onCameraDisconnected();
    /// Displays a successful grab, then routes the frame to whichever operation requested it.
    void onCameraGrabFinished(vc::device::GrabResult result);
    /// Clears and rebuilds the property browser, guarding against re-entrant valueChanged.
    void populateBrowser();

    /// Sets up the calibration board/table from the stored preset and refreshes the labels.
    void initCalibrationUi();
    /// Rebuilds m_board from `preset` and seeds the table's world XY corners from it.
    void setCalibBoardPreset(const QString &preset);
    /// Updates the board-info label with the current preset name and dot count.
    void refreshBoardInfoLabel();
    /// Updates the calibration status/reprojection/rotation labels from m_params.calibrator().
    void refreshCalibrationStatusLabels(const QString &extraSuffix = QString());
    /// Repaints the connection label for `connected` through its `connectionState` property.
    void applyConnectionVisual(bool connected);

private:
    Ui::JaiCameraWidget *ui;                          ///< uic-generated form.
    std::shared_ptr<vc::device::IDevice> m_device;    ///< The device this widget was built with.
    vc::device::JaiGigECamera *m_camera{nullptr};     ///< m_device cast to the concrete type; not owned.
    ads::CDockWidget *m_dock{nullptr};                ///< Dock hosting this widget; not owned.

    vc::device::JaiGigeCfg m_params;                  ///< Working copy edited via the property browser.

    JaiCamSelectDialog *m_camera_select_dialog{nullptr};  ///< Owned (child of this).

    vc::runtime::CameraRunner *m_runner{nullptr};     ///< Not owned — provided by TaskRunner.

    bool m_populating_browser{false};                 ///< Guards onPropertyValueChanged() during populateBrowser().

    std::unique_ptr<calib::CalibrationBoard> m_board; ///< Calibration board; rebuilt on preset change.
    bool m_pendingCalibDetect{false};                 ///< True for the grab that follows Detect.

    CalibrationThresholdDialog *m_thresholdDlg{nullptr};  ///< Open only while tuning the threshold.
    bool m_pendingThresholdGrab{false};               ///< True while a grab is pending for m_thresholdDlg.

    cv::Mat m_lastFrame;                              ///< Last successful grab, for Save image.

    QtVariantProperty *m_exposureProperty{nullptr};   ///< Kept so its limits can be refreshed in place.
    QtVariantProperty *m_gainProperty{nullptr};       ///< Kept so its limits can be refreshed in place.

    bool m_continuousActive{false};   ///< Mirror of the camera's streaming state, for the button.
    bool m_backlightOn{false};        ///< Mirror of the camera's backlight level, for the button.
    /// True while a continuous frame is being converted and painted.
    ///
    /// The drop flag. Frames arrive on a queued connection, so if painting a 5 MP frame takes
    /// longer than the frame interval the events pile up in the GUI thread's queue without bound
    /// and the view falls further behind real time the longer live view runs. Dropping is the
    /// right trade for a focus aid: the operator wants *now*, not every frame.
    bool m_paintingContinuousFrame{false};
};

#endif // JAI_CAMERA_WIDGET_H
