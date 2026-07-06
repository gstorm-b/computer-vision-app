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

class PlcMitsuDeviceWizard : public QWidget {
    Q_OBJECT

public:
    explicit PlcMitsuDeviceWizard(QWidget *parent = nullptr);
    ~PlcMitsuDeviceWizard();

    // QJsonObject getWizardJson(QJsonObject &obj);

private:
    void setupWidget();
    void fixedAllRowWidth();
    void syncFormLabelsWidth(QList<QFormLayout*> layouts);

private:
    Ui::PlcMitsuDeviceWizard *ui;
};

#endif // PLC_MITSU_DEVICE_WIZARD_H
