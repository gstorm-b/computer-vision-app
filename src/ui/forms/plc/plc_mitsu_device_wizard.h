#ifndef PLC_MITSU_DEVICE_WIZARD_H
#define PLC_MITSU_DEVICE_WIZARD_H

#include <QWidget>
#include <QFormLayout>
#include <QLabel>
#include <QList>
#include <algorithm>

#include "device/idevice.h"

namespace Ui {
class PlcMitsuDeviceWizard;
}

/// Wizard page for configuring a Mitsubishi PLC device (MC Protocol, Ethernet TCP/IP): lets the
/// user pick interface/frame type and keeps the generated form's row labels aligned.
class PlcMitsuDeviceWizard : public QWidget {
    Q_OBJECT

public:
    /// Loads the .ui form and runs setupWidget() to populate combo boxes and align labels.
    explicit PlcMitsuDeviceWizard(QWidget *parent = nullptr);
    /// Destroys the generated UI object.
    ~PlcMitsuDeviceWizard();

    // QJsonObject getWizardJson(QJsonObject &obj);

private:
    /// Populates the protocol/interface/frame-type combo boxes with their supported values, wires
    /// the interface-type combo to switch the frame-type options and the stacked widget page, and
    /// aligns all form-row label widths via fixedAllRowWidth().
    void setupWidget();
    /// Collects every QFormLayout in this widget and, if any exist, aligns their row-label widths
    /// via syncFormLabelsWidth().
    void fixedAllRowWidth();
    /// Sets every row label across `layouts` to a common fixed width equal to the widest label's
    /// size hint, so form rows line up visually across multiple form layouts.
    /// @param layouts the form layouts whose label columns should be width-synced
    void syncFormLabelsWidth(QList<QFormLayout*> layouts);

private:
    Ui::PlcMitsuDeviceWizard *ui;  ///< Generated UI object owning this wizard's widgets; destroyed in the destructor.
};

#endif // PLC_MITSU_DEVICE_WIZARD_H
