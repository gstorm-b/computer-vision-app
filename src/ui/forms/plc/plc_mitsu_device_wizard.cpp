#include "plc_mitsu_device_wizard.h"
#include "ui_plc_mitsu_device_wizard.h"

/// Constructs the wizard, sets up the generated UI, and calls setupWidget() to populate combo
/// boxes and align form labels.
PlcMitsuDeviceWizard::PlcMitsuDeviceWizard(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlcMitsuDeviceWizard) {
    ui->setupUi(this);
    setupWidget();
}

/// Deletes the generated UI object.
PlcMitsuDeviceWizard::~PlcMitsuDeviceWizard() {
    delete ui;
}

// void PlcMitsuDeviceWizard::getWizardJson(QJsonObject &obj) {
//     // obj[DEVICE_JSK_PLC_TYPE] = cbx_plc_brand->currentText();
// }

/// Populates cbb_protocol_type with "MC Protocol" and cbb_interface_type with "Ethernet TCP/IP";
/// connects cbb_interface_type's index change to refresh cbb_frame_type's options ("3E" for index
/// 0, "1C, 3C" for index 1) and switch stack_wg to the matching page; then aligns all form row
/// label widths.
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

/// Finds all QFormLayout children of this widget and, if any are found, forwards them to
/// syncFormLabelsWidth() to align their row-label widths; does nothing if there are none.
void PlcMitsuDeviceWizard::fixedAllRowWidth() {
    QList<QFormLayout*> layouts = this->findChildren<QFormLayout*>();
    if (layouts.empty()) {
        return;
    }
    syncFormLabelsWidth(layouts);
}

/// Scans every row's label in `layouts`, finds the widest label's size-hint width, and fixes
/// every label to that common width so label columns line up across all the given layouts.
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
