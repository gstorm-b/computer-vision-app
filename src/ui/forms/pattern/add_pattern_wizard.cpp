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

constexpr int CW = 560;         ///< Fixed width (px) of the image/crop/pick/box canvases.
constexpr int CH = 380;         ///< Fixed height (px) of the image/crop/pick/box canvases.
constexpr int DIALOG_W = 920;   ///< Fixed overall wizard dialog width (px).
constexpr int DIALOG_H = 640;   ///< Fixed overall wizard dialog height (px).

/// Builds a labeled "section title" QLabel with a uniform uppercase, muted,
/// letter-spaced style; `hint` (if given) is appended after the label text.
QLabel *makeFieldLabel(const QString &text, const QString &hint = {}) {
    auto *lbl = new QLabel(text + (hint.isEmpty() ? "" : "  " + hint));
    lbl->setStyleSheet(QString(
        "color: %1; font: 700 9pt 'Segoe UI'; "
        "letter-spacing: 1.2px; text-transform: uppercase;"
    ).arg(ptn::TXT3));
    return lbl;
}

/// Creates a 1px-tall QFrame styled as a horizontal divider line.
QFrame *makeHSeparator() {
    auto *f = new QFrame; f->setFixedHeight(1);
    f->setStyleSheet(QString("background: %1;").arg(ptn::BD));
    return f;
}

/// Creates a 1px-wide QFrame styled as a vertical divider line.
QFrame *makeVSeparator() {
    auto *f = new QFrame; f->setFixedWidth(1);
    f->setStyleSheet(QString("background: %1;").arg(ptn::BD));
    return f;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  AddPatternWizard
// ─────────────────────────────────────────────────────────────────────────────

/// Constructs the wizard fixed-size, modal, applies the pattern theme
/// stylesheet, builds every step page, and jumps to Step 1 (Image).
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

/// Assembles the dialog's top-level layout (header, step rail, stacked step
/// pages, footer) and forces the crop canvas into its initial "no crop" mode.
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

/// Builds the fixed-height title bar: wizard title, subtitle label
/// (updated per-step by goToStep()), and a close button wired to onCancel().
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

/// Creates a single round numbered "step bubble" label for the step rail,
/// initially styled as not-done and current only when `idx` is 0.
QLabel *AddPatternWizard::makeStepBubble(int idx) {
    auto *b = new QLabel(QString::number(idx + 1));
    b->setAlignment(Qt::AlignCenter);
    b->setFixedSize(22, 22);
    b->setStyleSheet(ptn::stepBubbleStyle(false, idx == 0));
    return b;
}

/// Builds the horizontal 5-cell step rail (bubble + title + subline per
/// step) and populates m_stepBubbles / m_stepLabels for later restyling by
/// updateStepRail().
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

/// Builds the fixed-height footer bar: status label (updated by
/// updateFooterStatus()) plus Cancel/Back/Next buttons wired to their slots.
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

/// Builds Step 1 ("Image"): left-side image preview label plus a right-hand
/// column for the pattern name/number fields and the camera/file capture
/// buttons.
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

/// Builds Step 2 ("Crop"): the crop canvas plus a right-hand column with the
/// "use original frame" checkbox, X/Y/W/H spin boxes mirroring the crop
/// rect, and Reset / 1:1 Center shortcut buttons.
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

/// Builds Step 3 ("Pick Point"): the pick canvas plus a right-hand column
/// with X/Y spin boxes mirroring the pick position, a Center shortcut, and
/// an info note explaining how the pick offset is used at match time.
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

/// Builds Step 4 ("Picking Box"): the box canvas plus a right-hand column
/// with width/height and distance/angle spin boxes for the symmetric jaw
/// pair, and Reset / Rotate +90° shortcut buttons. Wires the canvas'
/// boxChanged/pickChanged signals back into the spin boxes and m_box*/m_pick
/// state (see inline comments below for the exact coordinate-frame handling).
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

/// Builds Step 5 ("Finish"): the read-only finish canvas plus a right-hand
/// column with the HTML summary label (refreshed by refreshFinishSummary())
/// and a note describing what happens on Apply.
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

/// Navigates the wizard to `step` (0-4): rejects moves forward past an
/// invalid step (see currentStepValid()), switches the visible stack page,
/// pushes the accumulated state (image/crop/pick/box) into the canvas being
/// entered, refreshes the step rail, footer status, subtitle, and Back/Next
/// button state, and rewires Next to Apply on the final step.
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

/// Restyles every step bubble/label to reflect the current step: done
/// (checkmark), current (highlighted), or upcoming (muted).
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

/// Checks whether the current step's inputs are complete enough to advance.
/// Only Step 1 (Image) is gated: requires a non-empty, unused-in-group name,
/// an unused-in-group number >= 1, and a captured/loaded image. Steps 2-5
/// have no blocking requirement.
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

/// Recomputes the footer status text for the current step: an actionable
/// hint when the step is invalid, or a "✓ ..." summary of the current
/// image/crop/pick/box state when it is valid.
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

/// Rebuilds the Step 5 HTML summary (name/number, image source, crop,
/// pick point, box size, and symmetric offset/angle) and sets it on
/// m_finishSummary.
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

/// Advances the wizard to the next step (Next button; no-op past Step 5).
void AddPatternWizard::onNext()    { goToStep(m_currentStep + 1); }
/// Moves the wizard back to the previous step (Back button; no-op before Step 1).
void AddPatternWizard::onBack()    { goToStep(m_currentStep - 1); }
/// Cancels the wizard, closing the dialog with QDialog::Rejected.
void AddPatternWizard::onCancel()  { reject(); }
/// Confirms the wizard (Apply on Step 5), closing the dialog with QDialog::Accepted.
void AddPatternWizard::onApply()   { accept(); }

/// Records "camera" as the image source and emits requestCameraImage() so
/// the host can capture a frame and feed it back via setCameraImage().
void AddPatternWizard::onPickFromCameraClicked() {
    m_imageSource = "camera";
    emit requestCameraImage();
}

/// Prompts the user for an image file via QFileDialog, loads it with
/// cv::imread (unchanged channel layout), and on success feeds it into
/// Step 1 via setLoadedImage(). Silently does nothing if the dialog is
/// cancelled or the file fails to decode.
void AddPatternWizard::onPickFromFileClicked() {
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Open pattern image"), QString(),
        tr("Image files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
    if (file.isEmpty()) return;

    cv::Mat mat = cv::imread(file.toStdString(), cv::IMREAD_UNCHANGED);
    if (mat.empty()) return;
    setLoadedImage(mat, QFileInfo(file).fileName());
}

/// Clears the currently captured/loaded image and its source/filename,
/// resets the Step 1 preview to its empty placeholder, and hides the
/// discard button.
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

/// Stores a clone of `image` as the captured source (marking the source as
/// "camera"), converts it to a QImage for preview (BGR888->RGB888 for 3-channel,
/// direct copy for 8-bit grayscale; other formats are left unpreviewed),
/// updates the Step 1 preview/status, and re-derives the geometry spin-box
/// ranges and default crop/pick via onImageSizeChanged(). Does nothing if
/// `image` is empty.
/// @param image captured frame in OpenCV BGR (CV_8UC3) or grayscale (CV_8UC1) format
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

/// Feeds `image` through setCameraImage() (for the preview/geometry setup),
/// then overrides the recorded source to "file" and stores `filename`,
/// updating the status label to show the loaded filename.
/// @param image the loaded image data (OpenCV format, see setCameraImage())
/// @param filename display name of the source file, shown in the status label
void AddPatternWizard::setLoadedImage(const cv::Mat &image, const QString &filename) {
    setCameraImage(image);
    m_imageSource   = "file";
    m_imageFilename = filename;
    m_lblImageStatus->setText(QString("● %1 · %2")
                                  .arg(tr("LOADED")).arg(filename));
}

/// Widens the crop/pick/box spin-box ranges to span the newly-(re)loaded
/// image (in image pixels), then clamps or re-centers the current crop rect
/// and pick point so both stay inside the new image bounds, pushing the
/// clamped values back into their spin boxes. All range/value changes are
/// signal-blocked to avoid re-triggering onCropChanged() etc. No-op if
/// either dimension is non-positive.
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

/// Updates the stored pattern name from the name field, shows/clears the
/// "already exists" error label depending on whether the trimmed name
/// collides with m_usedNames, and refreshes the footer status.
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

/// Updates the stored pattern number from the number field, shows/clears the
/// "already exists" error label depending on whether `v` collides with
/// m_usedNumbers, and refreshes the footer status.
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

/// Toggles whether the original (uncropped) frame is used as the pattern
/// image: switches the crop canvas between None/Crop interaction modes and,
/// when reverting to "keep original", resets the box canvas' crop overlay to
/// the full captured frame.
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

/// Updates the stored crop rect, mirrors it into the X/Y/W/H spin boxes
/// (signal-blocked to avoid feedback) and back into the crop canvas, then
/// refreshes the footer status.
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

/// Resets the crop rect to a centered region inset by 1/8 of the image
/// dimensions on each side (falling back to the canvas size CW x CH if no
/// image is loaded), then applies it via onCropChanged().
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

/// Resets the crop rect to a centered square (1:1 aspect ratio) sized to the
/// smaller image dimension minus a 40px margin, then applies it via
/// onCropChanged().
void AddPatternWizard::onCenter1to1Crop() {
    const int iw = m_capturedMat.empty() ? CW : m_capturedMat.cols;
    const int ih = m_capturedMat.empty() ? CH : m_capturedMat.rows;
    const int s = qMax(20, qMin(iw, ih) - 40);
    onCropChanged(QRect((iw - s) / 2, (ih - s) / 2, s, s));
}

/// Updates the stored pick point from whichever coordinate the current mode
/// expects (full-frame `p` when using the original frame, crop-relative
/// `imgp` when cropped), mirrors it into the pick spin boxes (signal-blocked)
/// and back into the pick canvas, then refreshes the footer status.
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

/// Centers the pick point: within the crop rect when cropping is active, or
/// within the full captured frame (falling back to CW x CH) when using the
/// original frame; applies the result via onPickChanged().
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

/// Pulls the current width/height/distance/angle values out of the Step 4
/// spin boxes into m_boxW/H/Dist/Angle, pushes them into the box canvas, and
/// refreshes the footer status.
void AddPatternWizard::onBoxChanged() {
    if (m_boxWSpin)     m_boxW     = m_boxWSpin->value();
    if (m_boxHSpin)     m_boxH     = m_boxHSpin->value();
    if (m_boxDistSpin)  m_boxDist  = m_boxDistSpin->value();
    if (m_boxAngleSpin) m_boxAngle = m_boxAngleSpin->value();
    if (m_boxCanvas)
        m_boxCanvas->setBoxConfig(m_boxW, m_boxH, m_boxDist, m_boxAngle);
    updateFooterStatus();
}

/// Resets the picking box to its default size/offset (120x80, distance 90,
/// angle 0), writes the values into the Step 4 spin boxes, and applies them
/// via onBoxChanged().
void AddPatternWizard::onBoxReset() {
    m_boxW = 120; m_boxH = 80; m_boxDist = 90; m_boxAngle = 0;
    if (m_boxWSpin)     m_boxWSpin->setValue(m_boxW);
    if (m_boxHSpin)     m_boxHSpin->setValue(m_boxH);
    if (m_boxDistSpin)  m_boxDistSpin->setValue(m_boxDist);
    if (m_boxAngleSpin) m_boxAngleSpin->setValue(m_boxAngle);
    onBoxChanged();
}

/// Rotates the picking box by +90 degrees, wrapping the result back into
/// the [-180, 180] range, writes it into the angle spin box, and applies it
/// via onBoxChanged().
void AddPatternWizard::onBoxRotate90() {
    m_boxAngle += 90;
    while (m_boxAngle > 180)  m_boxAngle -= 360;
    while (m_boxAngle < -180) m_boxAngle += 360;
    if (m_boxAngleSpin) m_boxAngleSpin->setValue(m_boxAngle);
    onBoxChanged();
}
