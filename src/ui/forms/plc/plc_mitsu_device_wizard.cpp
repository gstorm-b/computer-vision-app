#include "plc_mitsu_device_wizard.h"
#include "ui_plc_mitsu_device_wizard.h"

PlcMitsuDeviceWizard::PlcMitsuDeviceWizard(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlcMitsuDeviceWizard) {
    ui->setupUi(this);
    setupWidget();
}

PlcMitsuDeviceWizard::~PlcMitsuDeviceWizard() {
    delete ui;
}

// void PlcMitsuDeviceWizard::getWizardJson(QJsonObject &obj) {
//     // obj[DEVICE_JSK_PLC_TYPE] = cbx_plc_brand->currentText();
// }

void PlcMitsuDeviceWizard::setupWidget() {
    ui->cbb_protocol_type->addItem(tr("MC Protocol"));

    ui->cbb_interface_type->addItems({tr("Ethernet TCP/IP")});
    ui->cbb_frame_type->addItems({tr("3E")});

    connect(ui->cbb_interface_type, &QComboBox::currentIndexChanged, this, [this](int index) {
        ui->cbb_frame_type->clear();
        if (index == 0) {
            ui->cbb_frame_type->addItems({tr("3E")});
            ui->stack_wg->setCurrentIndex(0);
        } else if (index == 1) {
            ui->cbb_frame_type->addItems({tr("1C, 3C")});
            ui->stack_wg->setCurrentIndex(1);
        }
    });

    fixedAllRowWidth();
}

void PlcMitsuDeviceWizard::fixedAllRowWidth() {
    QList<QFormLayout*> layouts = this->findChildren<QFormLayout*>();
    if (layouts.empty()) {
        return;
    }
    syncFormLabelsWidth(layouts);
}

void PlcMitsuDeviceWizard::syncFormLabelsWidth(QList<QFormLayout*> layouts) {
    int maxWidth = 0;
    QList<QLabel*> allLabels;

    for (QFormLayout* layout : layouts) {
        if (!layout) continue;
        for (int i = 0; i < layout->rowCount(); ++i) {
            QLayoutItem* item = layout->itemAt(i, QFormLayout::LabelRole);
            if (item && item->widget()) {
                QLabel* label = qobject_cast<QLabel*>(item->widget());
                if (label) {
                    allLabels.append(label);
                    int labelWidth = label->sizeHint().width();
                    if (labelWidth > maxWidth) {
                        maxWidth = labelWidth;
                    }
                }
            }
        }
    }

    for (QLabel* label : allLabels) {
        label->setFixedWidth(maxWidth);
    }
}
