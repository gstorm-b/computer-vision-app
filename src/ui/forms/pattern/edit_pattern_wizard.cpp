#include "edit_pattern_wizard.h"
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

namespace {

constexpr int CW = 560;
constexpr int CH = 380;
constexpr int DIALOG_W = 920;
constexpr int DIALOG_H = 620;

QLabel *makeFieldLabel(const QString &text) {
    auto *lbl = new QLabel(text);
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

} // namespace

EditPatternWizard::EditPatternWizard(const QString &groupName,
                                     const Pattern &pattern,
                                     const QStringList &usedNames,
                                     const QList<int>  &usedNumbers,
                                     QWidget *parent)
    : QDialog(parent),
      m_groupName(groupName),
      m_usedNames(usedNames),
      m_usedNumbers(usedNumbers),
      m_old(pattern),
      m_new(pattern)
{
    setWindowTitle(tr("Edit Pattern Wizard"));
    setModal(true);
    setFixedSize(DIALOG_W, DIALOG_H);
    setStyleSheet(ptn::baseStyleSheet());
    buildUi();
    goToStep(0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI construction
// ─────────────────────────────────────────────────────────────────────────────

void EditPatternWizard::buildUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);

    root->addWidget(buildHeader());
    root->addWidget(buildStepRail());

    m_stack = new QStackedWidget;
    m_stack->setContentsMargins(18, 18, 18, 18);
    m_stack->setStyleSheet(QString("background: %1;").arg(ptn::SURF2));
    m_stack->addWidget(buildStepIdentity());
    m_stack->addWidget(buildStepPick());
    m_stack->addWidget(buildStepBox());
    m_stack->addWidget(buildStepFinish());
    root->addWidget(m_stack, 1);

    root->addWidget(buildFooter());
}

QWidget *EditPatternWizard::buildHeader() {
    auto *w = new QFrame;
    w->setFixedHeight(54);
    w->setStyleSheet(QString(
        "QFrame { background: %1; border-bottom: 1px solid %2; }"
    ).arg(ptn::HD, ptn::BD));

    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(18, 8, 18, 8);

    auto *titles = new QVBoxLayout; titles->setSpacing(2);
    auto *t = new QLabel(tr("Edit Pattern Wizard"));
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
    close->setStyleSheet(QString(
        "QPushButton { color: %1; background: transparent; border: none;"
        "  font: 14pt 'Segoe UI'; padding: 4px 8px; }"
        "QPushButton:hover { color: %2; }"
    ).arg(ptn::TXT2, ptn::TXT));
    connect(close, &QPushButton::clicked, this, &EditPatternWizard::onCancel);
    lay->addWidget(close);

    return w;
}

QLabel *EditPatternWizard::makeStepBubble(int idx) {
    auto *b = new QLabel(QString::number(idx + 1));
    b->setAlignment(Qt::AlignCenter);
    b->setFixedSize(22, 22);
    b->setStyleSheet(ptn::stepBubbleStyle(false, idx == 0));
    return b;
}

QWidget *EditPatternWizard::buildStepRail() {
    auto *w = new QFrame;
    w->setFixedHeight(50);
    w->setStyleSheet(QString(
        "QFrame { background: %1; border-bottom: 1px solid %2; }"
    ).arg(ptn::BG, ptn::BD));

    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(18, 0, 18, 0);
    lay->setSpacing(0);

    const QStringList labels   = {tr("Identity"), tr("Pick Point"),
                                   tr("Picking Box"), tr("Finish")};
    const QStringList sublines = {tr("Image locked · edit name/number"),
                                   tr("Adjust pick position"),
                                   tr("Tweak gripper bounds"),
                                   tr("Review changes")};
    for (int i = 0; i < 4; ++i) {
        auto *cell = new QWidget;
        auto *cellLay = new QHBoxLayout(cell);
        cellLay->setContentsMargins(8, 10, 8, 10); cellLay->setSpacing(8);

        auto *bubble = makeStepBubble(i);
        m_stepBubbles.push_back(bubble);
        cellLay->addWidget(bubble);

        auto *texts = new QVBoxLayout; texts->setSpacing(1);
        auto *l1 = new QLabel(labels[i]);
        l1->setStyleSheet(QString("color: %1; font: 600 10pt 'Segoe UI';")
                              .arg(i == 0 ? ptn::TXT : ptn::TXT3));
        m_stepLabels.push_back(l1);
        auto *l2 = new QLabel(sublines[i]);
        l2->setStyleSheet(QString("color: %1; font: 8pt 'Segoe UI';").arg(ptn::TXT3));
        texts->addWidget(l1); texts->addWidget(l2);
        cellLay->addLayout(texts, 1);

        lay->addWidget(cell, 1);
    }
    return w;
}

QWidget *EditPatternWizard::buildFooter() {
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
    connect(m_btnCancel, &QPushButton::clicked, this, &EditPatternWizard::onCancel);

    m_btnBack = new QPushButton("← " + tr("Back"));
    m_btnBack->setStyleSheet(ptn::ghostButtonStyle());
    connect(m_btnBack, &QPushButton::clicked, this, &EditPatternWizard::onBack);

    m_btnNext = new QPushButton(tr("Next") + " →");
    m_btnNext->setStyleSheet(ptn::primaryButtonStyle());
    connect(m_btnNext, &QPushButton::clicked, this, &EditPatternWizard::onNext);

    lay->addWidget(m_btnCancel);
    lay->addWidget(m_btnBack);
    lay->addWidget(m_btnNext);
    return w;
}

// ── Step 1 — Identity ───────────────────────────────────────────────────────

QWidget *EditPatternWizard::buildStepIdentity() {
    auto *page = new QWidget;
    auto *lay  = new QHBoxLayout(page);
    lay->setSpacing(18); lay->setContentsMargins(0, 0, 0, 0);

    // Locked image canvas
    m_lockedCanvas = new AddPatternImageCanvas;
    m_lockedCanvas->setMode(AddPatternImageCanvas::None);
    m_lockedCanvas->setMinimumSize(CW, CH);
    m_lockedCanvas->setLocked(true);
    m_lockedCanvas->setImage(m_old.image);
    lay->addWidget(m_lockedCanvas, 1);

    // Right column: name/number
    auto *col = new QWidget; col->setFixedWidth(280);
    auto *right = new QVBoxLayout(col); right->setSpacing(14);

    auto *info = new QLabel(QString(
        "🔒  %1\n%2").arg(tr("Image locked")).arg(
                            tr("Re-learn the pattern to capture a new image.")));
    info->setStyleSheet(QString(
        "QLabel { background: rgba(106,90,205,30); border: 1px solid %1;"
        "  border-radius: 5px; padding: 9px 11px; color: %2; font: 9pt 'Segoe UI'; }"
    ).arg(ptn::LOCK, ptn::TXT2));
    info->setWordWrap(true);
    right->addWidget(info);

    right->addWidget(makeFieldLabel(tr("Pattern Name")));
    m_inputName = new QLineEdit(m_old.name);
    m_inputName->setStyleSheet(ptn::inputStyle());
    connect(m_inputName, &QLineEdit::textChanged,
            this, &EditPatternWizard::onNameChanged);
    right->addWidget(m_inputName);
    auto *prevName = new QLabel(tr("was: %1").arg(m_old.name));
    prevName->setStyleSheet(QString("color: %1; font: 9pt '%2';")
                                .arg(ptn::TXT3, "JetBrains Mono"));
    right->addWidget(prevName);
    m_lblNameError = new QLabel;
    m_lblNameError->setStyleSheet(QString("color: %1; font: 9pt 'Segoe UI';").arg(ptn::ERR));
    right->addWidget(m_lblNameError);

    right->addWidget(makeFieldLabel(tr("Pattern Number")));
    m_inputNumber = new QSpinBox; m_inputNumber->setRange(1, 9999);
    m_inputNumber->setValue(m_old.number);
    m_inputNumber->setStyleSheet(ptn::inputStyle());
    connect(m_inputNumber, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EditPatternWizard::onNumberChanged);
    right->addWidget(m_inputNumber);
    auto *prevNum = new QLabel(tr("was: #%1").arg(m_old.number));
    prevNum->setStyleSheet(QString("color: %1; font: 9pt '%2';")
                              .arg(ptn::TXT3, "JetBrains Mono"));
    right->addWidget(prevNum);
    m_lblNumberError = new QLabel;
    m_lblNumberError->setStyleSheet(QString("color: %1; font: 9pt 'Segoe UI';").arg(ptn::ERR));
    right->addWidget(m_lblNumberError);

    right->addStretch();
    lay->addWidget(col);
    return page;
}

// ── Step 2 — Pick Point ─────────────────────────────────────────────────────

QWidget *EditPatternWizard::buildStepPick() {
    auto *page = new QWidget;
    auto *lay  = new QHBoxLayout(page);
    lay->setSpacing(18); lay->setContentsMargins(0, 0, 0, 0);

    m_pickCanvas = new AddPatternImageCanvas;
    m_pickCanvas->setMode(AddPatternImageCanvas::Pick);
    m_pickCanvas->setMinimumSize(CW, CH);
    m_pickCanvas->setImage(m_old.image);
    m_pickCanvas->setPick({m_old.pickX, m_old.pickY});
    connect(m_pickCanvas, &AddPatternImageCanvas::pickChanged,
            this, &EditPatternWizard::onPickChanged);
    lay->addWidget(m_pickCanvas, 1);

    auto *col = new QWidget; col->setFixedWidth(280);
    auto *right = new QVBoxLayout(col); right->setSpacing(14);

    right->addWidget(makeFieldLabel(tr("Picking Position")));

    auto *grid = new QGridLayout; grid->setSpacing(6);
    auto addAxis = [&](const QString &label, QSpinBox *&sb, int max,
                       int initial, int row) {
        auto *l = new QLabel(label);
        l->setStyleSheet(QString("color: %1; font: 700 11pt '%2';")
                              .arg(ptn::ACC, "JetBrains Mono"));
        sb = new QSpinBox; sb->setRange(0, max); sb->setValue(initial);
        sb->setStyleSheet(ptn::inputStyle());
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int){ onPickChanged({m_pickXSpin->value(), m_pickYSpin->value()}); });
        grid->addWidget(l, row, 0); grid->addWidget(sb, row, 1);
    };
    addAxis("X", m_pickXSpin, CW, m_old.pickX, 0);
    addAxis("Y", m_pickYSpin, CH, m_old.pickY, 1);
    right->addLayout(grid);

    // Widen the pick spin ranges to the locked image so dragging the pick on
    // the canvas (which can reach any pixel) is reflected exactly.
    if (!m_old.image.empty()) {
        QSignalBlocker bx(m_pickXSpin), by(m_pickYSpin);
        m_pickXSpin->setRange(0, m_old.image.cols - 1);
        m_pickYSpin->setRange(0, m_old.image.rows - 1);
        m_pickXSpin->setValue(m_new.pickX);
        m_pickYSpin->setValue(m_new.pickY);
    }

    auto *bCenter = new QPushButton(tr("Center"));
    bCenter->setStyleSheet(ptn::ghostButtonStyle());
    connect(bCenter, &QPushButton::clicked, this, &EditPatternWizard::onPickCenter);
    right->addWidget(bCenter);

    auto *prev = new QLabel(QString(tr("was: (%1, %2)"))
                                .arg(m_old.pickX).arg(m_old.pickY));
    prev->setStyleSheet(QString("color: %1; font: 9pt '%2';")
                            .arg(ptn::TXT3, "JetBrains Mono"));
    right->addWidget(prev);

    right->addWidget(makeHSeparator());
    auto *info = new QLabel(tr(
        "Click anywhere on the locked image to set a new picking position."));
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

// ── Step 3 — Box ────────────────────────────────────────────────────────────

QWidget *EditPatternWizard::buildStepBox() {
    auto *page = new QWidget;
    auto *lay  = new QHBoxLayout(page);
    lay->setSpacing(18); lay->setContentsMargins(0, 0, 0, 0);

    m_boxCanvas = new AddPatternImageCanvas;
    m_boxCanvas->setMode(AddPatternImageCanvas::Box);
    m_boxCanvas->setMinimumSize(CW, CH);
    m_boxCanvas->setImage(m_old.image);
    m_boxCanvas->setPick({m_new.pickX, m_new.pickY});
    m_boxCanvas->setBoxConfig(m_new.pickBoxW, m_new.pickBoxH,
                              m_new.pickBoxDist, m_new.pickBoxAngle);
    // Canvas-driven box edits (drag body / corners / rotation handle) feed the
    // spin boxes, which then echo into m_new via onBoxChanged().
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
                m_new.pickBoxW = w; m_new.pickBoxH = h;
                m_new.pickBoxDist = d; m_new.pickBoxAngle = a;
                updateFooterStatus();
            });
    // The picking centre is draggable on the box canvas too (image is locked,
    // so coords are plain image pixels).  Echo the Pick-step spin boxes.
    connect(m_boxCanvas, &AddPatternImageCanvas::pickChanged,
            this, [this](const QPoint &p, const QPoint &) {
                m_new.pickX = p.x(); m_new.pickY = p.y();
                if (m_pickXSpin) {
                    QSignalBlocker b1(m_pickXSpin), b2(m_pickYSpin);
                    m_pickXSpin->setValue(p.x());
                    m_pickYSpin->setValue(p.y());
                }
                updateFooterStatus();
            });
    lay->addWidget(m_boxCanvas, 1);

    auto *col = new QWidget; col->setFixedWidth(280);
    auto *right = new QVBoxLayout(col); right->setSpacing(12);

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
    addD(tr("Width"),  m_boxWSpin,  CW, m_old.pickBoxW, 0);
    addD(tr("Height"), m_boxHSpin,  CH, m_old.pickBoxH, 1);
    right->addLayout(sg);

    right->addWidget(makeFieldLabel(tr("Offset from Pick Point")));
    auto *og = new QGridLayout; og->setSpacing(6);
    auto addO = [&](const QString &label, QDoubleSpinBox *&sb,
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
    addO(tr("Distance"), m_boxDistSpin,    0,  400, m_old.pickBoxDist,  0);
    addO(tr("Angle"),    m_boxAngleSpin, -180, 180, m_old.pickBoxAngle, 1);
    right->addLayout(og);

    // Match the Add wizard: size the geometry spin ranges to the locked image
    // so canvas drags aren't silently clamped by the default CW/CH caps.
    if (!m_old.image.empty()) {
        const double sizeCap = qMax<double>(qMax(m_old.image.cols, m_old.image.rows), 5000.0);
        const double distCap = qMax<double>(m_old.image.cols + m_old.image.rows, 1000.0);
        QSignalBlocker b1(m_boxWSpin), b2(m_boxHSpin), b3(m_boxDistSpin);
        m_boxWSpin->setRange(1.0, sizeCap);
        m_boxHSpin->setRange(1.0, sizeCap);
        m_boxDistSpin->setRange(0.0, distCap);
        m_boxWSpin->setValue(m_new.pickBoxW);
        m_boxHSpin->setValue(m_new.pickBoxH);
        m_boxDistSpin->setValue(m_new.pickBoxDist);
    }

    auto *btns = new QHBoxLayout;
    auto *bReset = new QPushButton(tr("Reset"));
    bReset->setStyleSheet(ptn::ghostButtonStyle());
    connect(bReset, &QPushButton::clicked, this, &EditPatternWizard::onBoxReset);
    auto *bRot = new QPushButton(tr("Rotate +90°"));
    bRot->setStyleSheet(ptn::ghostButtonStyle());
    connect(bRot, &QPushButton::clicked, this, &EditPatternWizard::onBoxRotate90);
    btns->addWidget(bReset); btns->addWidget(bRot); btns->addStretch();
    right->addLayout(btns);

    auto *prev = new QLabel(QString(tr("was: %1×%2 · d=%3 · %4°"))
                                .arg(m_old.pickBoxW).arg(m_old.pickBoxH)
                                .arg(m_old.pickBoxDist).arg(m_old.pickBoxAngle));
    prev->setStyleSheet(QString("color: %1; font: 9pt '%2';")
                            .arg(ptn::TXT3, "JetBrains Mono"));
    right->addWidget(prev);

    right->addStretch();
    lay->addWidget(col);
    return page;
}

// ── Step 4 — Finish (diff view) ─────────────────────────────────────────────

QWidget *EditPatternWizard::buildStepFinish() {
    auto *page = new QWidget;
    auto *lay  = new QHBoxLayout(page);
    lay->setSpacing(18); lay->setContentsMargins(0, 0, 0, 0);

    m_finishCanvas = new AddPatternImageCanvas;
    m_finishCanvas->setMode(AddPatternImageCanvas::Finish);
    m_finishCanvas->setMinimumSize(CW, CH);
    m_finishCanvas->setImage(m_old.image);
    lay->addWidget(m_finishCanvas, 1);

    auto *col = new QWidget; col->setFixedWidth(280);
    auto *right = new QVBoxLayout(col); right->setSpacing(8);

    auto *header = new QLabel(tr("CHANGES"));
    header->setStyleSheet(QString(
        "color: %1; font: 700 9pt 'Segoe UI'; letter-spacing: 1.2px;"
    ).arg(ptn::TXT3));
    right->addWidget(header);

    m_diffSummary = new QLabel;
    m_diffSummary->setWordWrap(true);
    m_diffSummary->setTextFormat(Qt::RichText);
    m_diffSummary->setStyleSheet(QString(
        "QLabel { color: %1; font: 9pt 'Segoe UI'; }"
    ).arg(ptn::TXT));
    right->addWidget(m_diffSummary);

    right->addStretch();
    lay->addWidget(col);
    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Step rail / nav
// ─────────────────────────────────────────────────────────────────────────────

void EditPatternWizard::goToStep(int step) {
    if (step < 0 || step > 3) return;
    if (step > m_currentStep && !currentStepValid()) return;

    m_currentStep = step;
    m_stack->setCurrentIndex(step);

    if (step == 1 && m_pickCanvas) {
        m_pickCanvas->setPick({m_new.pickX, m_new.pickY});
    }
    if (step == 2 && m_boxCanvas) {
        m_boxCanvas->setPick({m_new.pickX, m_new.pickY});
        m_boxCanvas->setBoxConfig(m_new.pickBoxW, m_new.pickBoxH,
                                   m_new.pickBoxDist, m_new.pickBoxAngle);
    }
    if (step == 3 && m_finishCanvas) {
        m_finishCanvas->setPick({m_new.pickX, m_new.pickY});
        m_finishCanvas->setBoxConfig(m_new.pickBoxW, m_new.pickBoxH,
                                      m_new.pickBoxDist, m_new.pickBoxAngle);
        refreshDiff();
    }

    updateStepRail();
    updateFooterStatus();
    m_btnBack->setEnabled(step > 0);

    if (step == 3) {
        m_btnNext->setText("✓  " + tr("Apply Changes"));
        disconnect(m_btnNext, nullptr, this, nullptr);
        connect(m_btnNext, &QPushButton::clicked, this, &EditPatternWizard::onApply);
    } else {
        m_btnNext->setText(tr("Next") + "  →");
        disconnect(m_btnNext, nullptr, this, nullptr);
        connect(m_btnNext, &QPushButton::clicked, this, &EditPatternWizard::onNext);
    }

    if (m_subtitleLabel) {
        const QStringList subs = {tr("Image locked · edit name/number"),
                                   tr("Adjust pick position"),
                                   tr("Tweak gripper bounds"),
                                   tr("Review changes")};
        m_subtitleLabel->setText(QString("%1  ·  %2  %3 of 4  —  %4")
                                     .arg(tr("Group:"), m_groupName)
                                     .arg(step + 1)
                                     .arg(subs[step]));
    }
}

void EditPatternWizard::updateStepRail() {
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

bool EditPatternWizard::currentStepValid() const {
    if (m_currentStep == 0) {
        const bool nameOk = !m_new.name.trimmed().isEmpty()
                            && (m_new.name == m_old.name
                                || !m_usedNames.contains(m_new.name.trimmed()));
        const bool numOk  = m_new.number >= 1
                            && (m_new.number == m_old.number
                                || !m_usedNumbers.contains(m_new.number));
        return nameOk && numOk;
    }
    return true;
}

void EditPatternWizard::updateFooterStatus() {
    if (!m_footerStatus) return;
    QString s;
    switch (m_currentStep) {
    case 0:
        s = currentStepValid()
            ? "✓ " + tr("Identity ready")
            : tr("Resolve the highlighted errors.");
        break;
    case 1:
        s = QString("✓ ") + tr("Pick point at (%1, %2)")
                                .arg(m_new.pickX).arg(m_new.pickY);
        break;
    case 2:
        s = QString("✓ ") + tr("Box %1×%2 · d=%3 · ±%4°")
                                .arg(m_new.pickBoxW).arg(m_new.pickBoxH)
                                .arg(m_new.pickBoxDist).arg(m_new.pickBoxAngle);
        break;
    case 3:
        s = "✓ " + tr("Review changes — Apply to commit");
        break;
    }
    m_footerStatus->setText(s);
}

void EditPatternWizard::refreshDiff() {
    if (!m_diffSummary) return;

    auto row = [&](const QString &label, const QString &oldV, const QString &newV) {
        const bool changed = (oldV != newV);
        const QString labelColor = changed ? ptn::WARN : ptn::TXT3;
        const QString valueLine  = changed
            ? QString("<span style='color:%1;text-decoration:line-through;'>%2</span> "
                      "<span style='color:%3;'>→</span> "
                      "<span style='color:%4;font-weight:700;'>%5</span>")
                  .arg(ptn::TXT4, oldV, ptn::WARN, ptn::OK, newV)
            : QString("<span style='color:%1;'>%2</span>").arg(ptn::TXT2, newV);
        return QString(
            "<div style='padding: 4px 0; border-bottom: 1px dashed %1;'>"
            "<b style='color:%2; font-size: 8pt; letter-spacing: 1px;'>%3</b><br/>"
            "<span style='font-family: \"JetBrains Mono\";'>%4</span></div>"
        ).arg(ptn::BD, labelColor, label, valueLine);
    };

    QString html;
    html += row(tr("NAME"),    m_old.name, m_new.name);
    html += row(tr("NUMBER"),  QString("#%1").arg(m_old.number),
                                QString("#%1").arg(m_new.number));
    html += row(tr("PICK"),    QString("(%1, %2)").arg(m_old.pickX).arg(m_old.pickY),
                                QString("(%1, %2)").arg(m_new.pickX).arg(m_new.pickY));
    html += row(tr("BOX W×H"), QString("%1×%2").arg(m_old.pickBoxW).arg(m_old.pickBoxH),
                                QString("%1×%2").arg(m_new.pickBoxW).arg(m_new.pickBoxH));
    html += row(tr("OFFSET"),  QString("d=%1 · %2°")
                                  .arg(m_old.pickBoxDist).arg(m_old.pickBoxAngle),
                                QString("d=%1 · %2°")
                                  .arg(m_new.pickBoxDist).arg(m_new.pickBoxAngle));
    m_diffSummary->setText(html);
}

// ── Slots ───────────────────────────────────────────────────────────────────

void EditPatternWizard::onNext()    { goToStep(m_currentStep + 1); }
void EditPatternWizard::onBack()    { goToStep(m_currentStep - 1); }
void EditPatternWizard::onCancel()  { reject(); }
void EditPatternWizard::onApply()   { accept(); }

void EditPatternWizard::onNameChanged(const QString &v) {
    m_new.name = v;
    if (m_lblNameError) {
        const bool dup = !v.trimmed().isEmpty()
                         && v.trimmed() != m_old.name
                         && m_usedNames.contains(v.trimmed());
        m_lblNameError->setText(dup ? tr("Name already exists in group") : QString());
    }
    updateFooterStatus();
}

void EditPatternWizard::onNumberChanged(int v) {
    m_new.number = v;
    if (m_lblNumberError) {
        const bool dup = (v != m_old.number) && m_usedNumbers.contains(v);
        m_lblNumberError->setText(dup ? tr("Number already exists in group") : QString());
    }
    updateFooterStatus();
}

void EditPatternWizard::onPickChanged(const QPoint &p) {
    m_new.pickX = p.x(); m_new.pickY = p.y();
    if (m_pickXSpin) {
        QSignalBlocker b1(m_pickXSpin), b2(m_pickYSpin);
        m_pickXSpin->setValue(p.x()); m_pickYSpin->setValue(p.y());
    }
    if (m_pickCanvas) m_pickCanvas->setPick(p);
    updateFooterStatus();
}

void EditPatternWizard::onPickCenter() {
    const int iw = m_old.image.empty() ? CW : m_old.image.cols;
    const int ih = m_old.image.empty() ? CH : m_old.image.rows;
    onPickChanged({iw / 2, ih / 2});
}

void EditPatternWizard::onBoxChanged() {
    if (m_boxWSpin)     m_new.pickBoxW     = m_boxWSpin->value();
    if (m_boxHSpin)     m_new.pickBoxH     = m_boxHSpin->value();
    if (m_boxDistSpin)  m_new.pickBoxDist  = m_boxDistSpin->value();
    if (m_boxAngleSpin) m_new.pickBoxAngle = m_boxAngleSpin->value();
    if (m_boxCanvas)
        m_boxCanvas->setBoxConfig(m_new.pickBoxW, m_new.pickBoxH,
                                  m_new.pickBoxDist, m_new.pickBoxAngle);
    updateFooterStatus();
}

void EditPatternWizard::onBoxReset() {
    // Mirror the Add wizard: reset to the default jaw geometry.
    m_new.pickBoxW = 120; m_new.pickBoxH = 80;
    m_new.pickBoxDist = 90; m_new.pickBoxAngle = 0;
    if (m_boxWSpin)     m_boxWSpin->setValue(m_new.pickBoxW);
    if (m_boxHSpin)     m_boxHSpin->setValue(m_new.pickBoxH);
    if (m_boxDistSpin)  m_boxDistSpin->setValue(m_new.pickBoxDist);
    if (m_boxAngleSpin) m_boxAngleSpin->setValue(m_new.pickBoxAngle);
    onBoxChanged();
}

void EditPatternWizard::onBoxRotate90() {
    m_new.pickBoxAngle += 90;
    while (m_new.pickBoxAngle > 180)  m_new.pickBoxAngle -= 360;
    while (m_new.pickBoxAngle < -180) m_new.pickBoxAngle += 360;
    if (m_boxAngleSpin) m_boxAngleSpin->setValue(m_new.pickBoxAngle);
    onBoxChanged();
}
