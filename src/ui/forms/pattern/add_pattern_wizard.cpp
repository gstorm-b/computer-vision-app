#include "add_pattern_wizard.h"
#include "pattern_canvas.h"
#include "pattern_theme.h"

#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QPainter>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

namespace {

constexpr int CW = 560;
constexpr int CH = 380;
constexpr int DIALOG_W = 920;
constexpr int DIALOG_H = 640;

// Apply a uniform style to a labeled "section title" (uppercase muted).
QLabel *makeFieldLabel(const QString &text, const QString &hint = {}) {
    auto *lbl = new QLabel(text + (hint.isEmpty() ? "" : "  " + hint));
    lbl->setStyleSheet(QString(
        "color: %1; font: 700 9pt 'Segoe UI'; "
        "letter-spacing: 1.2px; text-transform: uppercase;"
    ).arg(ptn::TXT3));
    return lbl;
}

QFrame *makeHSeparator() {
    auto *f = new QFrame; f->setFixedHeight(1);
    f->setStyleSheet(QString("background: %1;").arg(ptn::BD));
    return f;
}

QFrame *makeVSeparator() {
    auto *f = new QFrame; f->setFixedWidth(1);
    f->setStyleSheet(QString("background: %1;").arg(ptn::BD));
    return f;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  AddPatternWizard
// ─────────────────────────────────────────────────────────────────────────────

AddPatternWizard::AddPatternWizard(const QString &groupName,
                                   const QStringList &usedNames,
                                   const QList<int>  &usedNumbers,
                                   QWidget *parent)
    : QDialog(parent),
      m_groupName(groupName),
      m_usedNames(usedNames),
      m_usedNumbers(usedNumbers)
{
    setWindowTitle(tr("New Pattern Wizard"));
    setModal(true);
    setFixedSize(DIALOG_W, DIALOG_H);
    setStyleSheet(ptn::baseStyleSheet());
    buildUi();
    goToStep(0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI construction
// ─────────────────────────────────────────────────────────────────────────────

void AddPatternWizard::buildUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(buildHeader());
    root->addWidget(buildStepRail());

    m_stack = new QStackedWidget;
    m_stack->setContentsMargins(18, 18, 18, 18);
    m_stack->setStyleSheet(QString("background: %1;").arg(ptn::SURF2));
    m_stack->addWidget(buildStepImage());
    m_stack->addWidget(buildStepCrop());
    m_stack->addWidget(buildStepPick());
    m_stack->addWidget(buildStepBox());
    m_stack->addWidget(buildStepFinish());
    root->addWidget(m_stack, 1);

    root->addWidget(buildFooter());

    m_keepOriginal = true;
    if (m_cropCanvas)
        m_cropCanvas->setMode(AddPatternImageCanvas::None);
}

QWidget *AddPatternWizard::buildHeader() {
    auto *w = new QFrame;
    w->setFixedHeight(54);
    w->setStyleSheet(QString(
        "QFrame { background: %1; border-bottom: 1px solid %2; }"
    ).arg(ptn::HD, ptn::BD));

    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(18, 8, 18, 8);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto *t = new QLabel(tr("New Pattern Wizard"));
    t->setStyleSheet(QString("color: %1; font: 700 12pt 'Segoe UI';").arg(ptn::TXT));
    m_subtitleLabel = new QLabel;
    m_subtitleLabel->setStyleSheet(QString(
        "color: %1; font: 9pt 'Segoe UI';"
    ).arg(ptn::TXT3));
    titles->addWidget(t);
    titles->addWidget(m_subtitleLabel);
    lay->addLayout(titles);
    lay->addStretch();

    auto *close = new QPushButton("✕");
    close->setFlat(true);
    close->setCursor(Qt::PointingHandCursor);
    close->setStyleSheet(QString(
        "QPushButton { color: %1; background: transparent; border: none; "
        "  font: 14pt 'Segoe UI'; padding: 4px 8px; }"
        "QPushButton:hover { color: %2; }"
    ).arg(ptn::TXT2, ptn::TXT));
    connect(close, &QPushButton::clicked, this, &AddPatternWizard::onCancel);
    lay->addWidget(close);

    return w;
}

QLabel *AddPatternWizard::makeStepBubble(int idx) {
    auto *b = new QLabel(QString::number(idx + 1));
    b->setAlignment(Qt::AlignCenter);
    b->setFixedSize(22, 22);
    b->setStyleSheet(ptn::stepBubbleStyle(false, idx == 0));
    return b;
}

QWidget *AddPatternWizard::buildStepRail() {
    auto *w = new QFrame;
    w->setFixedHeight(50);
    w->setStyleSheet(QString(
        "QFrame { background: %1; border-bottom: 1px solid %2; }"
    ).arg(ptn::BG, ptn::BD));

    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(18, 0, 18, 0);
    lay->setSpacing(0);

    const QStringList labels   = {tr("Image"), tr("Crop"), tr("Pick Point"),
                                   tr("Picking Box"), tr("Finish")};
    const QStringList sublines = {tr("Capture or load source"),
                                   tr("Trim to pattern region"),
                                   tr("Set picking position"),
                                   tr("Define gripper bounds"),
                                   tr("Review & apply")};

    for (int i = 0; i < 5; ++i) {
        auto *cell = new QWidget;
        auto *cellLay = new QHBoxLayout(cell);
        cellLay->setContentsMargins(8, 10, 8, 10);
        cellLay->setSpacing(8);

        auto *bubble = makeStepBubble(i);
        m_stepBubbles.push_back(bubble);
        cellLay->addWidget(bubble);

        auto *texts = new QVBoxLayout;
        texts->setSpacing(1);
        auto *l1 = new QLabel(labels[i]);
        l1->setStyleSheet(QString("color: %1; font: 600 10pt 'Segoe UI';")
                              .arg(i == 0 ? ptn::TXT : ptn::TXT3));
        m_stepLabels.push_back(l1);
        auto *l2 = new QLabel(sublines[i]);
        l2->setStyleSheet(QString("color: %1; font: 8pt 'Segoe UI';").arg(ptn::TXT3));
        texts->addWidget(l1);
        texts->addWidget(l2);
        cellLay->addLayout(texts, 1);

        lay->addWidget(cell, 1);
    }

    return w;
}

QWidget *AddPatternWizard::buildFooter() {
    auto *w = new QFrame;
    w->setFixedHeight(54);
    w->setStyleSheet(QString(
        "QFrame { background: %1; border-top: 1px solid %2; }"
    ).arg(ptn::HD, ptn::BD));

    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(18, 8, 18, 8);

    m_footerStatus = new QLabel;
    m_footerStatus->setStyleSheet(QString(
        "color: %1; font: 9pt '%2';"
    ).arg(ptn::TXT3, "JetBrains Mono"));
    lay->addWidget(m_footerStatus, 1);

    m_btnCancel = new QPushButton(tr("Cancel"));
    m_btnCancel->setStyleSheet(ptn::ghostButtonStyle());
    connect(m_btnCancel, &QPushButton::clicked, this, &AddPatternWizard::onCancel);

    m_btnBack = new QPushButton("← " + tr("Back"));
    m_btnBack->setStyleSheet(ptn::ghostButtonStyle());
    connect(m_btnBack, &QPushButton::clicked, this, &AddPatternWizard::onBack);

    m_btnNext = new QPushButton(tr("Next") + " →");
    m_btnNext->setStyleSheet(ptn::primaryButtonStyle());
    connect(m_btnNext, &QPushButton::clicked, this, &AddPatternWizard::onNext);

    lay->addWidget(m_btnCancel);
    lay->addWidget(m_btnBack);
    lay->addWidget(m_btnNext);
    return w;
}

// ── Step 1 ──────────────────────────────────────────────────────────────────

QWidget *AddPatternWizard::buildStepImage() {
    auto *page = new QWidget;
    auto *lay  = new QHBoxLayout(page);
    lay->setSpacing(18); lay->setContentsMargins(0, 0, 0, 0);

    // Left: image preview
    m_imagePreviewLabel = new QLabel;
    m_imagePreviewLabel->setMinimumSize(CW, CH);
    m_imagePreviewLabel->setAlignment(Qt::AlignCenter);
    m_imagePreviewLabel->setStyleSheet(QString(
        "QLabel { background: #181818; border: 1px solid %1; border-radius: 5px;"
        "  color: %2; font: 10pt '%3'; }"
    ).arg(ptn::BD, ptn::TXT3, "JetBrains Mono"));
    m_imagePreviewLabel->setText(tr("No image yet\n\nCapture from camera or open from file →"));
    lay->addWidget(m_imagePreviewLabel, 1);

    // Right: name / number / source
    auto *right = new QVBoxLayout;
    right->setSpacing(14);
    auto *col = new QWidget; col->setFixedWidth(280);
    col->setLayout(right);

    right->addWidget(makeFieldLabel(tr("Pattern Name")));
    m_inputName = new QLineEdit;
    m_inputName->setPlaceholderText("e.g. Front_face");
    m_inputName->setStyleSheet(ptn::inputStyle());
    connect(m_inputName, &QLineEdit::textChanged, this, &AddPatternWizard::onNameChanged);
    right->addWidget(m_inputName);
    m_lblNameError = new QLabel;
    m_lblNameError->setStyleSheet(QString("color: %1; font: 9pt 'Segoe UI';").arg(ptn::ERR));
    right->addWidget(m_lblNameError);

    right->addWidget(makeFieldLabel(tr("Pattern Number"),
                                     "(written to output register on match)"));
    m_inputNumber = new QSpinBox;
    m_inputNumber->setRange(1, 9999);
    m_inputNumber->setValue(1);
    m_inputNumber->setStyleSheet(ptn::inputStyle());
    connect(m_inputNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AddPatternWizard::onNumberChanged);
    right->addWidget(m_inputNumber);
    m_lblNumberError = new QLabel;
    m_lblNumberError->setStyleSheet(QString("color: %1; font: 9pt 'Segoe UI';").arg(ptn::ERR));
    right->addWidget(m_lblNumberError);

    right->addWidget(makeHSeparator());
    right->addWidget(makeFieldLabel(tr("Image Source")));

    m_btnFromCamera = new QPushButton(tr("📷  Capture from Camera"));
    m_btnFromCamera->setStyleSheet(ptn::ghostButtonStyle());
    m_btnFromCamera->setMinimumHeight(44);
    connect(m_btnFromCamera, &QPushButton::clicked,
            this, &AddPatternWizard::onPickFromCameraClicked);
    right->addWidget(m_btnFromCamera);

    m_btnFromFile = new QPushButton(tr("📁  Open from File"));
    m_btnFromFile->setStyleSheet(ptn::ghostButtonStyle());
    m_btnFromFile->setMinimumHeight(44);
    connect(m_btnFromFile, &QPushButton::clicked,
            this, &AddPatternWizard::onPickFromFileClicked);
    right->addWidget(m_btnFromFile);

    m_btnDiscardImage = new QPushButton(tr("Discard image"));
    m_btnDiscardImage->setStyleSheet(ptn::ghostButtonStyle());
    m_btnDiscardImage->hide();
    connect(m_btnDiscardImage, &QPushButton::clicked,
            this, &AddPatternWizard::onDiscardImageClicked);
    right->addWidget(m_btnDiscardImage);

    m_lblImageStatus = new QLabel;
    m_lblImageStatus->setStyleSheet(QString("color: %1; font: 9pt '%2';")
                                        .arg(ptn::OK, "JetBrains Mono"));
    right->addWidget(m_lblImageStatus);

    right->addStretch();
    lay->addWidget(col);

    return page;
}

// ── Step 2 ──────────────────────────────────────────────────────────────────

QWidget *AddPatternWizard::buildStepCrop() {
    auto *page = new QWidget;
    auto *lay  = new QHBoxLayout(page);
    lay->setSpacing(18); lay->setContentsMargins(0, 0, 0, 0);

    m_cropCanvas = new AddPatternImageCanvas;
    m_cropCanvas->setMode(AddPatternImageCanvas::Crop);
    m_cropCanvas->setMinimumSize(CW, CH);
    connect(m_cropCanvas, &AddPatternImageCanvas::cropChanged,
            this, &AddPatternWizard::onCropChanged);
    lay->addWidget(m_cropCanvas, 1);

    auto *col = new QWidget; col->setFixedWidth(280);
    auto *right = new QVBoxLayout(col); right->setSpacing(14);

    m_chkKeepOriginal = new QCheckBox(tr("Use original frame"));
    m_chkKeepOriginal->setChecked(true);
    m_chkKeepOriginal->setStyleSheet(QString(
        "QCheckBox { color: %1; font: 600 11pt 'Segoe UI'; padding: 10px; "
        "  border: 1px solid %2; border-radius: 5px; background: %3; }"
        "QCheckBox:checked { border-color: %4; background: %5; }"
    ).arg(ptn::TXT, ptn::BD2, ptn::BG, ptn::ACC,
          QString("rgba(43,140,232,40)")));
    connect(m_chkKeepOriginal, &QCheckBox::toggled,
            this, &AddPatternWizard::onKeepOriginalToggled);
    right->addWidget(m_chkKeepOriginal);

    right->addWidget(makeFieldLabel(tr("Crop Region (image px)")));

    auto *grid = new QGridLayout;
    grid->setSpacing(6);
    auto addCoord = [&](const QString &label, QSpinBox *&sb, int row, int max) {
        auto *l = new QLabel(label);
        l->setStyleSheet(QString("color: %1; font: 9pt '%2';")
                            .arg(ptn::TXT3, "JetBrains Mono"));
        sb = new QSpinBox; sb->setRange(0, max);
        sb->setStyleSheet(ptn::inputStyle());
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int){ onCropChanged(QRect(m_cropX->value(), m_cropY->value(),
                                                        m_cropW->value(), m_cropH->value())); });
        grid->addWidget(l, row, 0);
        grid->addWidget(sb, row, 1);
    };
    addCoord("X", m_cropX, 0, CW);
    addCoord("Y", m_cropY, 1, CH);
    addCoord("W", m_cropW, 2, CW);
    addCoord("H", m_cropH, 3, CH);
    right->addLayout(grid);

    auto *btns = new QHBoxLayout;
    auto *bReset = new QPushButton(tr("Reset"));
    bReset->setStyleSheet(ptn::ghostButtonStyle());
    connect(bReset, &QPushButton::clicked, this, &AddPatternWizard::onResetCrop);
    auto *bCenter = new QPushButton(tr("1:1 Center"));
    bCenter->setStyleSheet(ptn::ghostButtonStyle());
    connect(bCenter, &QPushButton::clicked, this, &AddPatternWizard::onCenter1to1Crop);
    btns->addWidget(bReset); btns->addWidget(bCenter); btns->addStretch();
    right->addLayout(btns);

    right->addStretch();
    lay->addWidget(col);
    return page;
}

// ── Step 3 ──────────────────────────────────────────────────────────────────

QWidget *AddPatternWizard::buildStepPick() {
    auto *page = new QWidget;
    auto *lay  = new QHBoxLayout(page);
    lay->setSpacing(18); lay->setContentsMargins(0, 0, 0, 0);

    m_pickCanvas = new AddPatternImageCanvas;
    m_pickCanvas->setMode(AddPatternImageCanvas::Pick);
    m_pickCanvas->setMinimumSize(CW, CH);
    connect(m_pickCanvas, &AddPatternImageCanvas::pickChanged,
            this, &AddPatternWizard::onPickChanged);
    lay->addWidget(m_pickCanvas, 1);

    auto *col = new QWidget; col->setFixedWidth(280);
    auto *right = new QVBoxLayout(col); right->setSpacing(14);

    right->addWidget(makeFieldLabel(tr("Picking Position (image coords)")));

    auto *grid = new QGridLayout; grid->setSpacing(6);
    auto addAxis = [&](const QString &label, QSpinBox *&sb, int max, int row) {
        auto *l = new QLabel(label);
        l->setStyleSheet(QString("color: %1; font: 700 11pt '%2';")
                              .arg(ptn::ACC, "JetBrains Mono"));
        sb = new QSpinBox; sb->setRange(0, max);
        sb->setStyleSheet(ptn::inputStyle());
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int){
            QPoint temp;
            onPickChanged(temp, {m_pickXSpin->value(), m_pickYSpin->value()});
        });
        grid->addWidget(l, row, 0); grid->addWidget(sb, row, 1);
    };
    addAxis("X", m_pickXSpin, CW, 0);
    addAxis("Y", m_pickYSpin, CH, 1);
    right->addLayout(grid);

    auto *bCenter = new QPushButton(tr("Center"));
    bCenter->setStyleSheet(ptn::ghostButtonStyle());
    connect(bCenter, &QPushButton::clicked, this, &AddPatternWizard::onPickCenter);
    right->addWidget(bCenter);

    right->addWidget(makeHSeparator());

    auto *info = new QLabel(tr(
        "On a match, this pattern-relative offset is transformed by the\n"
        "detected pose to produce the real-world TCP target sent to the robot."));
    info->setWordWrap(true);
    info->setStyleSheet(QString(
        "QLabel { background: %1; border: 1px solid %2; border-radius: 5px;"
        "  padding: 10px 12px; color: %3; font: 9pt 'Segoe UI'; }"
    ).arg(ptn::BG, ptn::BD, ptn::TXT2));
    right->addWidget(info);

    right->addStretch();
    lay->addWidget(col);
    return page;
}

// ── Step 4 ──────────────────────────────────────────────────────────────────

QWidget *AddPatternWizard::buildStepBox() {
    auto *page = new QWidget;
    auto *lay  = new QHBoxLayout(page);
    lay->setSpacing(18); lay->setContentsMargins(0, 0, 0, 0);

    m_boxCanvas = new AddPatternImageCanvas;
    m_boxCanvas->setMode(AddPatternImageCanvas::Box);
    m_boxCanvas->setMinimumSize(CW, CH);
    // Canvas-driven box edits (drag corner / body / rotation handle) push
    // values back into the spin boxes; the existing onBoxChanged() slot
    // then echoes them into m_box* state.
    connect(m_boxCanvas, &AddPatternImageCanvas::boxChanged,
            this, [this](double w, double h, double d, double a) {
                if (m_boxWSpin) {
                    QSignalBlocker b1(m_boxWSpin), b2(m_boxHSpin),
                                   b3(m_boxDistSpin), b4(m_boxAngleSpin);
                    m_boxWSpin->setValue(w);
                    m_boxHSpin->setValue(h);
                    m_boxDistSpin->setValue(d);
                    m_boxAngleSpin->setValue(a);
                }
                m_boxW = w; m_boxH = h; m_boxDist = d; m_boxAngle = a;
                updateFooterStatus();
            });
    // The picking centre can also be dragged on the box canvas (for
    // convenience while positioning the jaws).  The box canvas shows the full
    // frame, so `p` is full-image coords — mirror the Pick-step storage
    // semantics (crop-relative when the user cropped) and echo the pick spins.
    connect(m_boxCanvas, &AddPatternImageCanvas::pickChanged,
            this, [this](const QPoint &p, const QPoint &) {
                m_pick = m_keepOriginal ? p : (p - m_crop.topLeft());
                if (m_pickXSpin) {
                    QSignalBlocker b1(m_pickXSpin), b2(m_pickYSpin);
                    m_pickXSpin->setValue(m_pick.x());
                    m_pickYSpin->setValue(m_pick.y());
                }
                updateFooterStatus();
            });
    lay->addWidget(m_boxCanvas, 1);

    auto *col = new QWidget; col->setFixedWidth(280);
    auto *right = new QVBoxLayout(col); right->setSpacing(12);

    auto *info = new QLabel(tr(
        "Both jaws share the same dimensions and offset. Box A sits at angle, "
        "Box B at angle + 180°."));
    info->setWordWrap(true);
    info->setStyleSheet(QString(
        "QLabel { background: rgba(43,140,232,26); border: 1px solid %1;"
        "  border-radius: 5px; padding: 8px 11px; color: %2; font: 9pt 'Segoe UI'; }"
    ).arg(QString("rgba(43,140,232,85)"), ptn::TXT2));
    right->addWidget(info);

    right->addWidget(makeFieldLabel(tr("Box Size (shared)")));
    auto *sg = new QGridLayout; sg->setSpacing(6);
    auto addD = [&](const QString &label, QDoubleSpinBox *&sb, double max,
                    double init, int row) {
        auto *l = new QLabel(label);
        l->setStyleSheet(QString("color: %1; font: 9pt '%2';")
                            .arg(ptn::TXT3, "JetBrains Mono"));
        sb = new QDoubleSpinBox; sb->setRange(5, max); sb->setValue(init);
        sb->setStyleSheet(ptn::inputStyle());
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double){ onBoxChanged(); });
        sg->addWidget(l, row, 0); sg->addWidget(sb, row, 1);
    };
    addD(tr("Width"),  m_boxWSpin,  CW, m_boxW, 0);
    addD(tr("Height"), m_boxHSpin,  CH, m_boxH, 1);
    right->addLayout(sg);

    right->addWidget(makeFieldLabel(tr("Offset from Pick Point")));
    auto *og = new QGridLayout; og->setSpacing(6);
    auto addOffset = [&](const QString &label, QDoubleSpinBox *&sb,
                         double minV, double maxV, double init, int row) {
        auto *l = new QLabel(label);
        l->setStyleSheet(QString("color: %1; font: 9pt '%2';")
                            .arg(ptn::TXT3, "JetBrains Mono"));
        sb = new QDoubleSpinBox; sb->setRange(minV, maxV); sb->setValue(init);
        sb->setStyleSheet(ptn::inputStyle());
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double){ onBoxChanged(); });
        og->addWidget(l, row, 0); og->addWidget(sb, row, 1);
    };
    addOffset(tr("Distance"), m_boxDistSpin,    0,  400, m_boxDist,  0);
    addOffset(tr("Angle"),    m_boxAngleSpin, -180, 180, m_boxAngle, 1);
    right->addLayout(og);

    auto *btns = new QHBoxLayout;
    auto *bReset = new QPushButton(tr("Reset"));
    bReset->setStyleSheet(ptn::ghostButtonStyle());
    connect(bReset, &QPushButton::clicked, this, &AddPatternWizard::onBoxReset);
    auto *bRot = new QPushButton(tr("Rotate +90°"));
    bRot->setStyleSheet(ptn::ghostButtonStyle());
    connect(bRot, &QPushButton::clicked, this, &AddPatternWizard::onBoxRotate90);
    btns->addWidget(bReset); btns->addWidget(bRot); btns->addStretch();
    right->addLayout(btns);

    right->addStretch();
    lay->addWidget(col);
    return page;
}

// ── Step 5 ──────────────────────────────────────────────────────────────────

QWidget *AddPatternWizard::buildStepFinish() {
    auto *page = new QWidget;
    auto *lay  = new QHBoxLayout(page);
    lay->setSpacing(18); lay->setContentsMargins(0, 0, 0, 0);

    m_finishCanvas = new AddPatternImageCanvas;
    m_finishCanvas->setMode(AddPatternImageCanvas::Finish);
    m_finishCanvas->setMinimumSize(CW, CH);
    lay->addWidget(m_finishCanvas, 1);

    auto *col = new QWidget; col->setFixedWidth(280);
    auto *right = new QVBoxLayout(col); right->setSpacing(8);

    m_finishSummary = new QLabel;
    m_finishSummary->setWordWrap(true);
    m_finishSummary->setTextFormat(Qt::RichText);
    m_finishSummary->setStyleSheet(QString(
        "QLabel { color: %1; font: 9pt 'Segoe UI'; }"
    ).arg(ptn::TXT));
    right->addWidget(m_finishSummary);

    right->addStretch();

    auto *apply = new QLabel(tr(
        "On Apply, the pattern is added to the group and marked as learned. "
        "You can refine thresholds later in the Property panel."));
    apply->setWordWrap(true);
    apply->setStyleSheet(QString(
        "QLabel { background: rgba(34,209,122,21); border: 1px solid %1;"
        "  border-radius: 5px; padding: 9px 11px; color: %2; font: 9pt 'Segoe UI'; }"
    ).arg(QString("rgba(34,209,122,85)"), ptn::TXT2));
    right->addWidget(apply);

    lay->addWidget(col);
    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Step rail / nav
// ─────────────────────────────────────────────────────────────────────────────

void AddPatternWizard::goToStep(int step) {
    if (step < 0 || step > 4) return;

    // Validate forward moves
    if (step > m_currentStep && !currentStepValid()) return;

    int last_step = m_currentStep;
    m_currentStep = step;
    m_stack->setCurrentIndex(step);

    // Sync state into step canvases when entering them
    if (step == 1 && m_cropCanvas) {
        m_cropCanvas->setImage(m_capturedMat);
        m_cropCanvas->setCrop(m_crop);
    }
    if (step == 2 && m_pickCanvas) {
        m_pickCanvas->setImage(m_capturedMat);
        // Show the crop region as a read-only overlay on the Pick canvas so
        // the user knows the active crop area.  Empty rect = no overlay.
        m_pickCanvas->setCrop(m_keepOriginal ? QRect() : m_crop);
        m_pickCanvas->setPick(m_pick);
        if (last_step == 1) {
            onPickCenter();
        }
    }
    if (step == 3 && m_boxCanvas) {
        m_boxCanvas->setImage(m_capturedMat);
        m_boxCanvas->setPick(m_keepOriginal ? m_pick : (m_pick + m_crop.topLeft()));
        m_boxCanvas->setBoxConfig(m_boxW, m_boxH, m_boxDist, m_boxAngle);
    }
    if (step == 4 && m_finishCanvas) {
        // QPoint  final_pick = m_pick;
        // if (!m_keepOriginal) {
        //     final_pick = m_pick + m_crop.topLeft();
        // }

        m_finishCanvas->setImage(patternImage());
        m_finishCanvas->setPick(m_pick);
        // m_finishCanvas->setPick(final_pick);
        m_finishCanvas->setBoxConfig(m_boxW, m_boxH, m_boxDist, m_boxAngle);
        refreshFinishSummary();
    }

    updateStepRail();
    updateFooterStatus();

    m_btnBack->setEnabled(step > 0);
    if (step == 4) {
        m_btnNext->setText("✓  " + tr("Apply Pattern"));
        disconnect(m_btnNext, nullptr, this, nullptr);
        connect(m_btnNext, &QPushButton::clicked, this, &AddPatternWizard::onApply);
    } else {
        m_btnNext->setText(tr("Next") + "  →");
        disconnect(m_btnNext, nullptr, this, nullptr);
        connect(m_btnNext, &QPushButton::clicked, this, &AddPatternWizard::onNext);
    }

    if (m_subtitleLabel) {
        const QStringList subs = {tr("Capture or load source"),
                                   tr("Trim to pattern region"),
                                   tr("Set picking position"),
                                   tr("Define gripper bounds"),
                                   tr("Review & apply")};
        m_subtitleLabel->setText(QString("%1  ·  %2  %3 of 5  —  %4")
                                     .arg(tr("Group:"), m_groupName)
                                     .arg(step + 1)
                                     .arg(subs[step]));
    }
}

void AddPatternWizard::updateStepRail() {
    for (int i = 0; i < m_stepBubbles.size(); ++i) {
        const bool done    = i < m_currentStep;
        const bool current = i == m_currentStep;
        m_stepBubbles[i]->setText(done ? "✓" : QString::number(i + 1));
        m_stepBubbles[i]->setStyleSheet(ptn::stepBubbleStyle(done, current));
        m_stepLabels[i]->setStyleSheet(QString("color: %1; font: 600 10pt 'Segoe UI';")
                                           .arg(current ? ptn::TXT
                                                : done ? ptn::TXT2 : ptn::TXT3));
    }
}

bool AddPatternWizard::currentStepValid() const {
    switch (m_currentStep) {
    case 0: {
        const bool nameOk   = !m_name.trimmed().isEmpty()
                              && !m_usedNames.contains(m_name.trimmed());
        const bool numOk    = m_number >= 1 && !m_usedNumbers.contains(m_number);
        const bool imageOk  = !m_capturedMat.empty();
        return nameOk && numOk && imageOk;
    }
    case 1: case 2: case 3: case 4:
        return true;
    }
    return false;
}

void AddPatternWizard::updateFooterStatus() {
    if (!m_footerStatus) return;
    QString s;
    switch (m_currentStep) {
    case 0:
        if (!currentStepValid()) {
            if (m_name.trimmed().isEmpty() || m_number < 1)
                s = tr("Enter a name and number to continue.");
            else if (m_capturedMat.empty())
                s = tr("Capture or load an image to continue.");
            else
                s = tr("Resolve the highlighted errors.");
        } else {
            s = "✓ " + tr("Image ready · proceed to crop");
        }
        break;
    case 1:
        s = m_keepOriginal
            ? "✓ " + tr("Using original frame")
            : QString("✓ ") + tr("Cropped to %1×%2 px")
                                  .arg(m_crop.width()).arg(m_crop.height());
        break;
    case 2: {
        // Display crop-relative coords if the user cropped — matches the
        // semantics returned by pickX() / pickY() and the canvas PICK label.
        const QPoint p = m_keepOriginal ? m_pick : (m_pick - m_crop.topLeft());
        s = QString("✓ ") + tr("Pick point at (%1, %2)")
                                .arg(p.x()).arg(p.y());
        break;
    }
    case 3:
        s = QString("✓ ") + tr("Symmetric pair · %1×%2 · d=%3 · ±%4°")
                                .arg(m_boxW).arg(m_boxH).arg(m_boxDist).arg(m_boxAngle);
        break;
    case 4:
        s = "✓ " + tr("All steps complete — ready to apply");
        break;
    }
    m_footerStatus->setText(s);
}

void AddPatternWizard::refreshFinishSummary() {
    if (!m_finishSummary) return;
    QString html = QString(
        "<div style='font-size: 13pt; font-weight: 700; color: %1; margin-bottom: 6px;'>"
        "%2 <span style='color: %3; font-family: \"JetBrains Mono\";'>#%4</span></div>"
    ).arg(ptn::TXT, m_name.isEmpty() ? "(unnamed)" : m_name, ptn::ACC).arg(m_number);

    auto row = [&](const QString &k, const QString &v) {
        return QString(
            "<div style='display:block; padding: 5px 0; border-bottom: 1px dashed %1;'>"
            "<b style='color:%2; font-size: 8pt; letter-spacing: 1.2px;'>%3</b>"
            "&nbsp;&nbsp;<span style='font-family: \"JetBrains Mono\";'>%4</span>"
            "</div>"
        ).arg(ptn::BD, ptn::TXT3, k, v);
    };

    html += row(tr("SOURCE"),
                m_imageSource == "camera" ? tr("Camera capture")
              : m_imageSource == "file"   ? tr("File: %1").arg(m_imageFilename)
              :                              tr("(none)"));
    html += row(tr("CROP"),
                m_keepOriginal
                    ? tr("Original (no crop)")
                    : QString("%1×%2 @ (%3,%4)").arg(m_crop.width()).arg(m_crop.height())
                                                  .arg(m_crop.x()).arg(m_crop.y()));
    {
        // const QPoint p = m_keepOriginal ? m_pick : (m_pick - m_crop.topLeft());
        const QPoint p = m_pick;
        html += row(tr("PICK POINT"), QString("(%1, %2) px").arg(p.x()).arg(p.y()));
    }
    html += row(tr("BOX SIZE"),   QString("%1 × %2 px").arg(m_boxW).arg(m_boxH));
    html += row(tr("OFFSET"),     QString("d=%1 · %2° / %3°")
                                     .arg(m_boxDist).arg(m_boxAngle).arg(m_boxAngle + 180));
    m_finishSummary->setText(html);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void AddPatternWizard::onNext()    { goToStep(m_currentStep + 1); }
void AddPatternWizard::onBack()    { goToStep(m_currentStep - 1); }
void AddPatternWizard::onCancel()  { reject(); }
void AddPatternWizard::onApply()   { accept(); }

void AddPatternWizard::onPickFromCameraClicked() {
    m_imageSource = "camera";
    emit requestCameraImage();
}

void AddPatternWizard::onPickFromFileClicked() {
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Open pattern image"), QString(),
        tr("Image files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
    if (file.isEmpty()) return;

    cv::Mat mat = cv::imread(file.toStdString(), cv::IMREAD_UNCHANGED);
    if (mat.empty()) return;
    setLoadedImage(mat, QFileInfo(file).fileName());
}

void AddPatternWizard::onDiscardImageClicked() {
    m_capturedMat.release();
    m_imageSource.clear();
    m_imageFilename.clear();
    m_imagePreviewLabel->setPixmap(QPixmap());
    m_imagePreviewLabel->setText(tr("No image yet\n\nCapture from camera or open from file →"));
    m_btnDiscardImage->hide();
    m_lblImageStatus->clear();
    updateFooterStatus();
}

void AddPatternWizard::setCameraImage(const cv::Mat &image) {
    if (image.empty()) return;
    m_capturedMat = image.clone();
    m_imageSource = "camera";
    m_imageFilename.clear();

    QImage qimg;
    if (image.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
        qimg = QImage(rgb.data, rgb.cols, rgb.rows,
                      static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    } else if (image.type() == CV_8UC1) {
        qimg = QImage(image.data, image.cols, image.rows,
                      static_cast<int>(image.step), QImage::Format_Grayscale8).copy();
    }
    if (m_imagePreviewLabel) {
        m_imagePreviewLabel->setPixmap(
            QPixmap::fromImage(qimg).scaled(
                m_imagePreviewLabel->size(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    m_btnDiscardImage->show();
    m_lblImageStatus->setText(QString("● %1 · %2×%3")
                                  .arg(tr("CAPTURED")).arg(image.cols).arg(image.rows));

    // Image dimensions drive the geometry spin-box ranges and the default
    // crop / pick positions.  All canvas-side values are in image pixels, so
    // the spin boxes need to span the full image to mirror canvas edits.
    onImageSizeChanged(image.cols, image.rows);

    updateFooterStatus();
}

void AddPatternWizard::setLoadedImage(const cv::Mat &image, const QString &filename) {
    setCameraImage(image);
    m_imageSource   = "file";
    m_imageFilename = filename;
    m_lblImageStatus->setText(QString("● %1 · %2")
                                  .arg(tr("LOADED")).arg(filename));
}

void AddPatternWizard::onImageSizeChanged(int imageW, int imageH) {
    if (imageW <= 0 || imageH <= 0) return;

    // ── Spin box ranges ─────────────────────────────────────────────────
    // Block per-spin signals while we widen — setRange() can clip the
    // current value, which would otherwise re-fire onCropChanged / etc.
    auto setR = [](QSpinBox *sb, int lo, int hi) {
        if (!sb) return;
        QSignalBlocker b(sb);
        sb->setRange(lo, hi);
    };
    auto setRD = [](QDoubleSpinBox *sb, double lo, double hi) {
        if (!sb) return;
        QSignalBlocker b(sb);
        sb->setRange(lo, hi);
    };

    setR(m_cropX, 0, imageW);
    setR(m_cropY, 0, imageH);
    setR(m_cropW, 1, imageW);
    setR(m_cropH, 1, imageH);

    setR(m_pickXSpin, 0, imageW - 1);
    setR(m_pickYSpin, 0, imageH - 1);

    // Pick-box sizes can in principle exceed the image (the canvas now lets
    // jaws spill off-frame), so give a generous cap based on the image.
    const double sizeCap = qMax(double(qMax(imageW, imageH)), 5000.0);
    const double distCap = qMax(std::hypot(double(imageW), double(imageH)), 1000.0);
    setRD(m_boxWSpin,    1.0, sizeCap);
    setRD(m_boxHSpin,    1.0, sizeCap);
    setRD(m_boxDistSpin, 0.0, distCap);
    // Angle range stays as configured (-180..180).

    // ── Clamp / re-centre defaults to fit the new image ─────────────────
    QRect imgR(0, 0, imageW, imageH);
    if (!imgR.contains(m_crop) || m_crop.isEmpty()) {
        const int cw = qBound(20, m_crop.width(),  imageW);
        const int ch = qBound(20, m_crop.height(), imageH);
        m_crop = QRect((imageW - cw) / 2, (imageH - ch) / 2, cw, ch);
    }
    if (m_pick.x() < 0 || m_pick.x() >= imageW ||
        m_pick.y() < 0 || m_pick.y() >= imageH) {
        m_pick = QPoint(imageW / 2, imageH / 2);
    }

    // Push the clamped values back into the spin boxes (still blocked above
    // for the range set; values need their own block).
    auto setV = [](QSpinBox *sb, int v) {
        if (!sb) return;
        QSignalBlocker b(sb);
        sb->setValue(v);
    };
    setV(m_cropX, m_crop.x());      setV(m_cropY, m_crop.y());
    setV(m_cropW, m_crop.width());  setV(m_cropH, m_crop.height());
    setV(m_pickXSpin, m_pick.x());  setV(m_pickYSpin, m_pick.y());
}

void AddPatternWizard::onNameChanged(const QString &v) {
    m_name = v;
    if (m_lblNameError) {
        m_lblNameError->setText(
            (!v.trimmed().isEmpty() && m_usedNames.contains(v.trimmed()))
                ? tr("Name already exists in group")
                : QString());
    }
    updateFooterStatus();
}

void AddPatternWizard::onNumberChanged(int v) {
    m_number = v;
    if (m_lblNumberError) {
        m_lblNumberError->setText(
            (v >= 1 && m_usedNumbers.contains(v))
                ? tr("Number already exists in group")
                : QString());
    }
    updateFooterStatus();
}

void AddPatternWizard::onKeepOriginalToggled(bool on) {
    m_keepOriginal = on;
    if (m_cropCanvas) {
        m_cropCanvas->setMode(on ? AddPatternImageCanvas::None
                                  : AddPatternImageCanvas::Crop);
        if (m_keepOriginal) {
            m_boxCanvas->setCrop(QRect(0, 0, m_capturedMat.cols, m_capturedMat.rows));
        } else {
            // m_boxCanvas->setCrop(m_crop);
        }
    }
    updateFooterStatus();
}

void AddPatternWizard::onCropChanged(const QRect &r) {
    m_crop = r;
    if (m_cropX) {
        QSignalBlocker b1(m_cropX), b2(m_cropY), b3(m_cropW), b4(m_cropH);
        m_cropX->setValue(r.x()); m_cropY->setValue(r.y());
        m_cropW->setValue(r.width()); m_cropH->setValue(r.height());
    }
    if (m_cropCanvas) m_cropCanvas->setCrop(r);
    updateFooterStatus();
}

void AddPatternWizard::onResetCrop() {
    // Use the loaded image's dimensions — crop geometry is in image pixels.
    const int iw = m_capturedMat.empty() ? CW : m_capturedMat.cols;
    const int ih = m_capturedMat.empty() ? CH : m_capturedMat.rows;
    const int marginX = qMax(20, iw / 8);
    const int marginY = qMax(20, ih / 8);
    onCropChanged(QRect(marginX, marginY,
                        qMax(20, iw - 2 * marginX),
                        qMax(20, ih - 2 * marginY)));
}

void AddPatternWizard::onCenter1to1Crop() {
    const int iw = m_capturedMat.empty() ? CW : m_capturedMat.cols;
    const int ih = m_capturedMat.empty() ? CH : m_capturedMat.rows;
    const int s = qMax(20, qMin(iw, ih) - 40);
    onCropChanged(QRect((iw - s) / 2, (ih - s) / 2, s, s));
}

void AddPatternWizard::onPickChanged(const QPoint &p, const QPoint &imgp) {
    if (m_keepOriginal) {
        m_pick = p;
    } else {
        m_pick = imgp;
    }
    if (m_pickXSpin) {
        QSignalBlocker b1(m_pickXSpin), b2(m_pickYSpin);
        m_pickXSpin->setValue(m_pick.x()); m_pickYSpin->setValue(m_pick.y());
    }
    if (m_pickCanvas) m_pickCanvas->setPick(m_pick);
    updateFooterStatus();
}

void AddPatternWizard::onPickCenter() {
    QPoint temp;
    if (!m_keepOriginal) {
        if (!m_crop.isNull() && !m_crop.isEmpty() && !m_keepOriginal) {
            const int iw = (m_crop.width() / 2);
            const int ih = (m_crop.height() / 2);
            onPickChanged(temp, {iw, ih});
        }
    } else {
        const int iw = m_capturedMat.empty() ? CW : m_capturedMat.cols;
        const int ih = m_capturedMat.empty() ? CH : m_capturedMat.rows;
        onPickChanged({iw / 2, ih / 2}, temp);
    }
}

void AddPatternWizard::onBoxChanged() {
    if (m_boxWSpin)     m_boxW     = m_boxWSpin->value();
    if (m_boxHSpin)     m_boxH     = m_boxHSpin->value();
    if (m_boxDistSpin)  m_boxDist  = m_boxDistSpin->value();
    if (m_boxAngleSpin) m_boxAngle = m_boxAngleSpin->value();
    if (m_boxCanvas)
        m_boxCanvas->setBoxConfig(m_boxW, m_boxH, m_boxDist, m_boxAngle);
    updateFooterStatus();
}

void AddPatternWizard::onBoxReset() {
    m_boxW = 120; m_boxH = 80; m_boxDist = 90; m_boxAngle = 0;
    if (m_boxWSpin)     m_boxWSpin->setValue(m_boxW);
    if (m_boxHSpin)     m_boxHSpin->setValue(m_boxH);
    if (m_boxDistSpin)  m_boxDistSpin->setValue(m_boxDist);
    if (m_boxAngleSpin) m_boxAngleSpin->setValue(m_boxAngle);
    onBoxChanged();
}

void AddPatternWizard::onBoxRotate90() {
    m_boxAngle += 90;
    while (m_boxAngle > 180)  m_boxAngle -= 360;
    while (m_boxAngle < -180) m_boxAngle += 360;
    if (m_boxAngleSpin) m_boxAngleSpin->setValue(m_boxAngle);
    onBoxChanged();
}
