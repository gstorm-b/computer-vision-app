#ifndef ADD_DEVICE_WIZARD_H
#define ADD_DEVICE_WIZARD_H

#include <QDialog>
#include <QMap>

#include "device/device_manager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class AddDeviceWizard; }
class QFrame;
class QLabel;
QT_END_NAMESPACE

/// Modal wizard dialog for adding a new device (Camera, PLC, or VisionOutput) to a
/// DeviceManager: presents selectable type cards, a per-type configuration stack, and
/// validates/commits the resulting device through the manager.
class AddDeviceWizard : public QDialog {
    Q_OBJECT

public:
    /// Constructs the wizard, populates the per-type combo boxes from `mng`, builds the
    /// device-type selector cards, and applies the current theme stylesheet.
    /// @param mng device manager used to enumerate sub-device types and later commit the device
    /// @param taskName optional task name shown as a subtitle; the subtitle is hidden when empty
    /// @param parent parent widget
    explicit AddDeviceWizard(std::shared_ptr<vc::device::DeviceManager> mng,
                             const QString &taskName = QString(),
                             QWidget *parent = nullptr);
    /// Destroys the wizard, deleting the generated UI object.
    ~AddDeviceWizard() override;

    /// Reserves a pending device id from the manager, focuses the name field, and runs the
    /// dialog modally.
    /// @return QDialog::Accepted if a device was committed, QDialog::Rejected if cancelled or
    /// if no manager was supplied
    int showWizard();

    /// Returns the device id reserved for this wizard session (empty until showWizard() runs).
    QString getDeviceId()   const { return m_pendingDeviceId; }
    /// Returns the trimmed device name entered by the user.
    QString getDeviceName() const;
    /// Returns the string name of the currently selected device type.
    QString getDeviceType() const;

    /// Releases the pending device id (if any) back to the manager, then defers to
    /// QDialog::reject().
    void reject() override;

protected:
    /// Intercepts mouse-press events on the type-selector card frames and routes them to
    /// onCardClicked(); all other events fall through to the base class.
    /// @param obj event source
    /// @param ev event to filter
    /// @return true if the event was a card click and was consumed, otherwise the base result
    bool eventFilter(QObject *obj, QEvent *ev) override;
    /// Ignores Escape (blocks accidental dialog close), triggers the Add action on Enter/Return
    /// when it is enabled, and otherwise defers to QDialog::keyPressEvent().
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    /// Handles a card click by making `type` the active selection (see selectCard()).
    void onCardClicked(vc::device::DeviceType type);
    /// Validates the entered device name (non-empty, unique), builds the type-specific config
    /// JSON, creates the device via DeviceFactory, and commits it through the manager; shows a
    /// warning/critical message box and returns without closing the dialog on failure.
    void onAddClicked();
    /// Enables the Add button only while the trimmed name text is non-empty.
    /// @param text current contents of the device-name edit field
    void onNameChanged(const QString &text);

private:
    /// Widget references and layout metadata for one device-type selector card.
    struct CardRefs {
        QFrame *card{nullptr};  ///< Clickable card frame widget for this device type.
        QLabel *icon{nullptr};  ///< Card's icon label, repainted by refreshCardIcons().
        QLabel *name{nullptr};  ///< Card's device-type name label.
        QLabel *desc{nullptr};  ///< Card's description label.
        int     stackPage{-1};      ///< Index into adwConfigStack for this type; -1 hides the config stack.
        QString colorKey;           ///< QSS deviceColor value applied to the Add button when selected (matches [deviceColor="..."]).
        QString defaultName;        ///< Device name pre-filled into the name field when this card is selected.
        QString iconPath;           ///< Resource path of the SVG icon rendered on the card.
    };

    /// Builds the CardRefs entry for each device type (Camera/PLC/VisionOutput), installs this
    /// dialog as their event filter, and renders their icons.
    void initCards();
    /// Re-renders every card's icon pixmap via svgIcon(), e.g. after a theme change.
    void refreshCardIcons();
    /// Marks `type`'s card/name as the active selection (repolishing their style), shows or
    /// hides the config stack and switches to that type's page, and resets the name field to
    /// the type's default name.
    /// @param type device type to make the active selection
    void selectCard(vc::device::DeviceType type);
    /// Loads the light/dark QSS resource matching the current theme, resolves its tokens, and
    /// applies it as this dialog's stylesheet.
    void reloadStyleSheet();
    /// Forces Qt to re-evaluate stylesheet selectors on `w` (unpolish, polish, update); used
    /// after changing a dynamic property such as "selected".
    /// @param w widget to repolish; no-op if null
    void repolish(QWidget *w);
    /// Builds the type-specific "config" JSON payload (camera sub-type, MC protocol config, or
    /// vision-output config) for the selected device type.
    /// @param type device type to build configuration for
    /// @return JSON object with the type's config fields; empty for unhandled/default cases
    QJsonObject buildDeviceJson(vc::device::DeviceType type);

    Ui::AddDeviceWizard *ui{nullptr};  ///< Generated UI form for this dialog (owned; deleted in the destructor).

    std::shared_ptr<vc::device::DeviceManager> m_manager;  ///< Device manager used to enumerate types and commit the finished device.
    QString                m_pendingDeviceId;  ///< Device id reserved via allocatePendingId() for the in-progress session; released on reject().
    vc::device::DeviceType m_selectedType{vc::device::DeviceType::Camera};  ///< Currently selected device type.

    QMap<vc::device::DeviceType, CardRefs> m_cards;  ///< Selector card widgets/metadata keyed by device type.
};

#endif // ADD_DEVICE_WIZARD_H
