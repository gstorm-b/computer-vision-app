#include "gripper_register_dialog.h"
#include "ui_gripper_register_dialog.h"

#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QSet>
#include <QStyle>
#include <QTableWidgetItem>

namespace {

/// Table layout: the name is a plain editable item, the three numeric axes are spin boxes.
/// There is no angle column — the jaw angle is per-pattern, not part of a preset.
constexpr int kColName     = 0;
constexpr int kColWidth    = 1;
constexpr int kColHeight   = 2;
constexpr int kColDistance = 3;
constexpr int kColumnCount = 4;

/// Builds a configured spin box for one numeric preset column.
/// @param[in] value   initial value
/// @param[in] minimum lower bound
/// @param[in] maximum upper bound
/// @return the spin box, ready to be installed as a cell widget
QDoubleSpinBox *makeSpin(double value, double minimum, double maximum) {
    auto *spin = new QDoubleSpinBox;
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setDecimals(2);
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    return spin;
}

/// Reads a numeric cell, tolerating a missing/foreign widget.
/// @return the spin box value, or 0.0 when the cell holds no spin box
double spinValue(QTableWidget *table, int row, int col) {
    auto *spin = qobject_cast<QDoubleSpinBox *>(table->cellWidget(row, col));
    return spin ? spin->value() : 0.0;
}

} // namespace

GripperRegisterDialog::GripperRegisterDialog(const vc::model::GripperPresetStore &presets,
                                             QWidget *parent)
    : QDialog(parent), ui(new Ui::GripperRegisterDialog), m_presets(presets) {
    ui->setupUi(this);

    ui->tbl_presets->setColumnCount(kColumnCount);
    ui->tbl_presets->setHorizontalHeaderLabels(
        { tr("Name"), tr("Width (px)"), tr("Height (px)"), tr("Distance (px)") });
    ui->tbl_presets->verticalHeader()->setVisible(false);
    ui->tbl_presets->horizontalHeader()->setSectionResizeMode(kColName, QHeaderView::Stretch);

    connect(ui->btn_add,    &QPushButton::clicked, this, &GripperRegisterDialog::onAdd);
    connect(ui->btn_remove, &QPushButton::clicked, this, &GripperRegisterDialog::onRemove);
    connect(ui->btn_save,   &QPushButton::clicked, this, &GripperRegisterDialog::onSave);
    connect(ui->btn_cancel, &QPushButton::clicked, this, &QDialog::reject);

    populate();
    showMessage(QString());
}

GripperRegisterDialog::~GripperRegisterDialog() {
    delete ui;
}

void GripperRegisterDialog::populate() {
    ui->tbl_presets->setRowCount(0);
    for (const vc::model::GripperPreset &p : m_presets.presets())
        appendRow(p);
}

void GripperRegisterDialog::appendRow(const vc::model::GripperPreset &preset) {
    auto *table = ui->tbl_presets;
    const int row = table->rowCount();
    table->insertRow(row);

    table->setItem(row, kColName, new QTableWidgetItem(preset.name));
    table->setCellWidget(row, kColWidth,
                         makeSpin(preset.boxes.size.width, 0.0, 100000.0));
    table->setCellWidget(row, kColHeight,
                         makeSpin(preset.boxes.size.height, 0.0, 100000.0));
    table->setCellWidget(row, kColDistance,
                         makeSpin(preset.boxes.distance, 0.0, 100000.0));
}

QString GripperRegisterDialog::uniqueDefaultName() const {
    QSet<QString> used;
    for (int row = 0; row < ui->tbl_presets->rowCount(); ++row) {
        if (auto *item = ui->tbl_presets->item(row, kColName))
            used.insert(item->text().trimmed());
    }
    for (int n = 1;; ++n) {
        const QString candidate = tr("Gripper %1").arg(n);
        if (!used.contains(candidate))
            return candidate;
    }
}

void GripperRegisterDialog::onAdd() {
    vc::model::GripperPreset preset;
    preset.name = uniqueDefaultName();
    appendRow(preset);

    const int row = ui->tbl_presets->rowCount() - 1;
    ui->tbl_presets->selectRow(row);
    ui->tbl_presets->editItem(ui->tbl_presets->item(row, kColName));
    showMessage(QString());
}

void GripperRegisterDialog::onRemove() {
    const int row = ui->tbl_presets->currentRow();
    if (row < 0) {
        showMessage(tr("Select a row to remove."));
        return;
    }
    ui->tbl_presets->removeRow(row);
    showMessage(QString());
}

void GripperRegisterDialog::onSave() {
    // Rebuild from scratch so the store's own uniqueness rule is what decides, rather
    // than a second copy of that rule living here.
    vc::model::GripperPresetStore rebuilt;

    for (int row = 0; row < ui->tbl_presets->rowCount(); ++row) {
        auto *nameItem = ui->tbl_presets->item(row, kColName);
        const QString name = nameItem ? nameItem->text().trimmed() : QString();

        if (name.isEmpty()) {
            ui->tbl_presets->selectRow(row);
            showMessage(tr("Row %1 has no name. Every gripper needs a unique name.")
                            .arg(row + 1));
            return;
        }

        vc::model::GripperPreset preset;
        preset.name = name;
        preset.boxes.size = cv::Size2f(
            static_cast<float>(spinValue(ui->tbl_presets, row, kColWidth)),
            static_cast<float>(spinValue(ui->tbl_presets, row, kColHeight)));
        preset.boxes.distance = spinValue(ui->tbl_presets, row, kColDistance);

        if (!rebuilt.add(preset)) {
            ui->tbl_presets->selectRow(row);
            showMessage(tr("Row %1 repeats the name \"%2\". Names must be unique.")
                            .arg(row + 1).arg(name));
            return;
        }
    }

    m_presets = rebuilt;
    accept();
}

void GripperRegisterDialog::showMessage(const QString &text, bool error) {
    ui->lbl_message->setText(text);
    // Styling comes from the parent's QSS via this property, not from an inline sheet.
    ui->lbl_message->setProperty("messageKind", error ? QStringLiteral("error")
                                                      : QStringLiteral("note"));
    ui->lbl_message->style()->unpolish(ui->lbl_message);
    ui->lbl_message->style()->polish(ui->lbl_message);
}
