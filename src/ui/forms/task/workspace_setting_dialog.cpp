#include "ui/forms/task/workspace_setting_dialog.h"
#include "ui_workspace_setting_dialog.h"

#include <QColor>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

/// ROI bounding colours — working = success green, condition = accent orange (see
/// ui_design_rules §5). ItemRoi is a painted surface; colours are applied in code because
/// QSS cannot reach a QGraphicsItem.
namespace {
const QColor kWorkingNormal(0x40, 0xc8, 0x70);      ///< Working-ROI border colour, unselected.
const QColor kWorkingSelected(0x8a, 0xf0, 0xb0);    ///< Working-ROI border colour, selected.
const QColor kConditionNormal(0xe8, 0x7c, 0x00);    ///< Condition-ROI border colour, unselected.
const QColor kConditionSelected(0xff, 0xc0, 0x70);  ///< Condition-ROI border colour, selected.
} // namespace

/// Constructs the dialog: embeds the ImageWidget into the .ui's host stack, builds the
/// ROI colour legend overlay, and wires all button/ROI signals.
WorkspaceSettingDialog::WorkspaceSettingDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::WorkspaceSettingDialog)
{
    ui->setupUi(this);

    m_view = new ImageWidget(this);
    m_view->setEnableMouseMenu(false);
    ui->stack_wg->addWidget(m_view);
    ui->stack_wg->setCurrentWidget(m_view);

    auto *legendFrame = new QFrame(m_view->viewport());
    legendFrame->setObjectName(QStringLiteral("workspaceRoiLegend"));
    legendFrame->setStyleSheet(QStringLiteral(
        "QFrame#workspaceRoiLegend {"
        "background: rgba(8, 12, 18, 180);"
        "border: 1px solid rgba(255, 255, 255, 36);"
        "border-radius: 8px;"
        "}"
        "QFrame#workspaceRoiLegend QLabel { color: white; }"));
    auto *legendLayout = new QVBoxLayout(legendFrame);
    legendLayout->setContentsMargins(10, 8, 10, 8);
    legendLayout->setSpacing(4);

    auto addLegendRow = [legendLayout](const QString &text, const QColor &color) {
        auto *row = new QHBoxLayout;
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(6);

        auto *swatch = new QLabel;
        swatch->setFixedSize(12, 12);
        swatch->setStyleSheet(QStringLiteral(
            "background:%1; border:1px solid rgba(255,255,255,96); border-radius:3px;")
                                  .arg(color.name()));
        auto *label = new QLabel(text);

        row->addWidget(swatch);
        row->addWidget(label);
        row->addStretch(1);
        legendLayout->addLayout(row);
    };

    addLegendRow(tr("Workspace ROI"), kWorkingNormal);
    addLegendRow(tr("Condition ROI"), kConditionNormal);
    legendFrame->adjustSize();
    legendFrame->move(12, 12);
    legendFrame->raise();

    // Both a freshly drawn ROI and a preloaded ROI surface through these
    // signals; onRoiCreated assigns the new item to whichever kind is pending.
    connect(m_view, &ImageWidget::signal_draw_roi_finished, this,
            [this](QGraphicsItem *item, ImageWidget::ItemAddType) { onRoiCreated(item); });
    connect(m_view, &ImageWidget::signal_new_roi_added, this,
            [this](QGraphicsItem *item, ImageWidget::ItemAddType) { onRoiCreated(item); });

    connect(ui->btn_choose_image, &QPushButton::clicked,
            this, &WorkspaceSettingDialog::onChooseImageClicked);
    connect(ui->btn_grab, &QPushButton::clicked,
            this, &WorkspaceSettingDialog::onGrabClicked);
    connect(ui->btn_set_working, &QPushButton::clicked,
            this, &WorkspaceSettingDialog::onSetWorkingRoiClicked);
    connect(ui->btn_clear_working, &QPushButton::clicked,
            this, &WorkspaceSettingDialog::onClearWorkingRoiClicked);
    connect(ui->btn_set_condition, &QPushButton::clicked,
            this, &WorkspaceSettingDialog::onSetConditionRoiClicked);
    connect(ui->btn_clear_condition, &QPushButton::clicked,
            this, &WorkspaceSettingDialog::onClearConditionRoiClicked);
    connect(ui->btn_save, &QPushButton::clicked,
            this, &WorkspaceSettingDialog::onSaveClicked);
    connect(ui->btn_cancel, &QPushButton::clicked,
            this, &QDialog::reject);

    m_lastDir = QDir::currentPath();
    updateButtons();
}

/// Destroys the dialog and deletes the generated UI.
WorkspaceSettingDialog::~WorkspaceSettingDialog()
{
    delete ui;
}

/// Sets the text shown in the dialog's title label.
void WorkspaceSettingDialog::setTitleText(const QString &title)
{
    ui->label_title->setText(title);
}

/// Preloads an existing workspace: loads `image` into the view and draws `workingRoi`/
/// `conditionRoi` if non-empty.
/// @param image reference image to display; ignored if null
/// @param workingRoi initial working ROI in image-pixel coordinates; skipped if empty
/// @param conditionRoi initial condition ROI in image-pixel coordinates; skipped if empty
void WorkspaceSettingDialog::setInitial(const QPixmap &image, const QRectF &workingRoi,
                                        const QRectF &conditionRoi)
{
    if (!image.isNull()) {
        QPixmap p = image;
        m_view->loadImage(p);
        resetRois();

        if (workingRoi.width() > 0.0 && workingRoi.height() > 0.0) {
            m_pendingKind = RoiKind::Working;
            m_view->addRoi(ImageWidget::NormalROI, workingRoi);
        }
        if (conditionRoi.width() > 0.0 && conditionRoi.height() > 0.0) {
            m_pendingKind = RoiKind::Condition;
            m_view->addRoi(ImageWidget::NormalROI, conditionRoi);
        }
        m_pendingKind = RoiKind::None;

        m_view->fitImageView();
    }
    updateButtons();
}

/// Loads `image` into the view as the reference image, supplied by the owner after a
/// requestGrab() (or any external image source); resets any existing ROIs.
void WorkspaceSettingDialog::setMainViewImage(QPixmap image)
{
    if (image.isNull())
        return;

    m_view->loadImage(image);
    resetRois();
    m_view->fitImageView();
    updateButtons();
}

/// Swallows the Escape key so a stray press does not close the dialog while editing an ROI.
void WorkspaceSettingDialog::keyPressEvent(QKeyEvent *event)
{
    // Avoid closing the dialog on a stray Escape while editing the ROI.
    if (event->key() == Qt::Key_Escape) {
        event->ignore();
        return;
    }
    QDialog::keyPressEvent(event);
}

/// Opens a file dialog and loads the chosen image as the reference image, resetting ROIs.
void WorkspaceSettingDialog::onChooseImageClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select workspace image"), m_lastDir,
        tr("Images (*.png *.bmp *.jpg *.jpeg *.tif *.tiff)"));
    if (filePath.isEmpty())
        return;

    m_lastDir = QFileInfo(filePath).absolutePath();
    m_view->loadImage(filePath);
    resetRois();
    m_view->fitImageView();
    updateButtons();
}

/// Requests a camera grab by emitting requestGrab().
void WorkspaceSettingDialog::onGrabClicked()
{
    emit requestGrab();
}

/// Drops any existing working ROI and starts drawing a new one.
void WorkspaceSettingDialog::onSetWorkingRoiClicked()
{
    if (!m_view->hadImage())
        return;
    // One ROI per kind: drop the existing working ROI before drawing a new one.
    if (m_workingRoi) {
        m_view->removeRoi(m_workingRoi);
        m_workingRoi = nullptr;
    }
    m_pendingKind = RoiKind::Working;
    m_view->startDrawROI(ImageWidget::NormalROI);
}

/// Removes the current working ROI, if any.
void WorkspaceSettingDialog::onClearWorkingRoiClicked()
{
    if (m_workingRoi) {
        m_view->removeRoi(m_workingRoi);
        m_workingRoi = nullptr;
    }
    updateButtons();
}

/// Drops any existing condition ROI and starts drawing a new one.
void WorkspaceSettingDialog::onSetConditionRoiClicked()
{
    if (!m_view->hadImage())
        return;
    if (m_conditionRoi) {
        m_view->removeRoi(m_conditionRoi);
        m_conditionRoi = nullptr;
    }
    m_pendingKind = RoiKind::Condition;
    m_view->startDrawROI(ImageWidget::NormalROI);
}

/// Removes the current condition ROI, if any.
void WorkspaceSettingDialog::onClearConditionRoiClicked()
{
    if (m_conditionRoi) {
        m_view->removeRoi(m_conditionRoi);
        m_conditionRoi = nullptr;
    }
    updateButtons();
}

/// Capture a freshly created ROI (drawn or preloaded) into the pending kind.
void WorkspaceSettingDialog::onRoiCreated(QGraphicsItem *item)
{
    auto *roi = dynamic_cast<ItemRoi *>(item);
    if (!roi)
        return;

    switch (m_pendingKind) {
    case RoiKind::Working:
        m_workingRoi = roi;
        styleRoi(roi, RoiKind::Working);
        break;
    case RoiKind::Condition:
        m_conditionRoi = roi;
        styleRoi(roi, RoiKind::Condition);
        break;
    case RoiKind::None:
    default:
        // Not expected: an ROI appeared without a pending kind. Leave it as-is.
        break;
    }

    m_pendingKind = RoiKind::None;
    updateButtons();
}

/// Apply the kind-specific bounding colour (working = green, condition = orange).
void WorkspaceSettingDialog::styleRoi(ItemRoi *roi, RoiKind kind)
{
    if (!roi)
        return;
    if (kind == RoiKind::Condition) {
        roi->setBoundingColorNormal(kConditionNormal);
        roi->setBoundingColorSelected(kConditionSelected);
    } else {
        roi->setBoundingColorNormal(kWorkingNormal);
        roi->setBoundingColorSelected(kWorkingSelected);
    }
    roi->update();
}

/// Drop all ROIs and reset tracked pointers (used when the image changes).
void WorkspaceSettingDialog::resetRois()
{
    m_view->removeAllRoi();
    m_workingRoi   = nullptr;
    m_conditionRoi = nullptr;
    m_pendingKind  = RoiKind::None;
}

/// Validates that at least one ROI was drawn, captures the result (image + ROIs), and
/// accepts the dialog; shows a warning message box instead if nothing can be saved.
void WorkspaceSettingDialog::onSaveClicked()
{
    if (!m_view->hadImage()) {
        QMessageBox::warning(this, tr("Workspace"),
                             tr("Load or grab an image first."));
        return;
    }

    const QRectF working   = m_workingRoi   ? m_workingRoi->getRoi()   : QRectF();
    const QRectF condition = m_conditionRoi ? m_conditionRoi->getRoi() : QRectF();

    const bool hasWorking   = working.width()   > 0.0 && working.height()   > 0.0;
    const bool hasCondition = condition.width() > 0.0 && condition.height() > 0.0;

    if (!hasWorking && !hasCondition) {
        QMessageBox::warning(this, tr("Workspace"),
                             tr("Draw a working or condition ROI first."));
        return;
    }

    m_resultRoi          = hasWorking   ? working   : QRectF();
    m_resultConditionRoi = hasCondition ? condition : QRectF();
    m_resultImage        = m_view->getImage();
    m_hasResult          = true;
    accept();
}

/// Enables/disables the set/clear/save buttons based on whether an image is loaded and
/// which ROIs currently exist.
void WorkspaceSettingDialog::updateButtons()
{
    const bool hasImage = m_view->hadImage();

    // "Set" is available only when that kind has no ROI yet (so a reopened
    // workspace cannot stack a second ROI of the same kind); "Clear" removes the
    // current one to draw a fresh ROI.
    ui->btn_set_working->setEnabled(hasImage && m_workingRoi == nullptr);
    ui->btn_clear_working->setEnabled(hasImage && m_workingRoi != nullptr);
    ui->btn_set_condition->setEnabled(hasImage && m_conditionRoi == nullptr);
    ui->btn_clear_condition->setEnabled(hasImage && m_conditionRoi != nullptr);
    ui->btn_save->setEnabled(hasImage);
}
