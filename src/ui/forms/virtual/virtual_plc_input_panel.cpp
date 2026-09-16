#include "ui/forms/virtual/virtual_plc_input_panel.h"
#include "ui_virtual_plc_input_panel.h"

#include <QHeaderView>
#include <QTableWidgetItem>

#include "core/logger/app_logger.h"

/// Builds the panel against `runner`, offering `digitalTags` then `wordTags` in the tag box.
VirtualPlcInputPanel::VirtualPlcInputPanel(vc::runtime::PlcRunner *runner,
                                           const QStringList &digitalTags,
                                           const QStringList &wordTags,
                                           QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VirtualPlcInputPanel)
    , m_runner(runner)
    , m_digitalTags(digitalTags)
{
    ui->setupUi(this);

    ui->cbx_tag->addItems(digitalTags);
    ui->cbx_tag->addItems(wordTags);
    ui->tbl_inputs->horizontalHeader()->setStretchLastSection(true);

    connect(ui->cbx_tag, &QComboBox::currentTextChanged,
            this, &VirtualPlcInputPanel::onTagChanged);
    connect(ui->btn_set, &QPushButton::clicked,
            this, &VirtualPlcInputPanel::onSetClicked);

    if (m_runner) {
        // Wired to the runner, never to the device: the runner forwards the device's signals onto
        // this thread. Listening to what the device PUBLISHED — rather than echoing what was typed
        // — is what makes the table evidence of delivery instead of a restatement of the request.
        connect(m_runner, &vc::runtime::PlcRunner::valueChanged,
                this, &VirtualPlcInputPanel::onValuesPublished);
    }

    // Asked of the runner, not of the device's sub-type. A device that would silently ignore the
    // controls must not be given them.
    const bool drivable = m_runner && m_runner->supportsInputSimulation();
    ui->cbx_tag->setEnabled(drivable);
    ui->spin_value->setEnabled(drivable);
    ui->btn_set->setEnabled(drivable);
    if (!drivable) {
        ui->lbl_hint->setText(tr("This device's inputs cannot be driven from here."));
    }

    onTagChanged(ui->cbx_tag->currentText());
}

VirtualPlcInputPanel::~VirtualPlcInputPanel()
{
    delete ui;
}

/// Retunes the value editor's range for the kind of tag now selected.
void VirtualPlcInputPanel::onTagChanged(const QString &tag)
{
    // The range IS the type indicator. A separate bit/word control would be a second thing to keep
    // in sync with the tag, and a spinbox offering -32768..32767 for a coil invites "M10 = 5".
    if (m_digitalTags.contains(tag, Qt::CaseInsensitive)
        || tag.startsWith(QLatin1Char('M'), Qt::CaseInsensitive)) {
        ui->spin_value->setRange(0, 1);
    } else {
        ui->spin_value->setRange(-32768, 32767);
    }
}

/// Sends the current tag/value to the device through the runner.
void VirtualPlcInputPanel::onSetClicked()
{
    const QString tag = ui->cbx_tag->currentText().trimmed();
    if (tag.isEmpty()) {
        return;
    }

    if (!m_runner) {
        LOG_USER_ERR << tr("Input not delivered: no runner to carry it to the device thread.");
        return;
    }

    // Queued onto the device thread. The device validates the tag and reports a refusal through
    // the runner's errorOccurred(), so a bad tag is rejected in one place rather than two.
    m_runner->requestInjectInputValue(tag, ui->spin_value->value());
}

/// Adds or updates the row for each tag the device published.
void VirtualPlcInputPanel::onValuesPublished(const QMap<QString, QVariant> &values)
{
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        int row = -1;
        for (int i = 0; i < ui->tbl_inputs->rowCount(); ++i) {
            if (ui->tbl_inputs->item(i, 0)->text() == it.key()) {
                row = i;
                break;
            }
        }
        if (row < 0) {
            row = ui->tbl_inputs->rowCount();
            ui->tbl_inputs->insertRow(row);
            ui->tbl_inputs->setItem(row, 0, new QTableWidgetItem(it.key()));
            ui->tbl_inputs->setItem(row, 1, new QTableWidgetItem());
        }
        ui->tbl_inputs->item(row, 1)->setText(it.value().toString());
    }
}
