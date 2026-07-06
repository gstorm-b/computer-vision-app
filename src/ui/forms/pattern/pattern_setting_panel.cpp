#include "pattern_setting_panel.h"
#include "pattern_theme.h"

#include "matching/match_pattern.h"
#include "matching/match_group.h"
#include "matching/edge_match_config.h"

#include <QLabel>
#include <QStackedWidget>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPainter>
#include <QScrollArea>

namespace {

// ── Tiny helper: draw the "reticle on grid" thumbnail placeholder ─────────────
// Mirrors `PatternThumb` from the design handoff so the empty/learned state
// reads identically when no real image has been loaded yet.
QPixmap makePlaceholderPixmap(int w, int h, bool learned) {
    QPixmap pm(w, h); pm.fill(QColor("#181818"));
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    // grid
    QPen gridPen;
    gridPen.setColor(QColor(ptn::BD));
    gridPen.setWidthF(0.5);
    p.setPen(gridPen);
    for (int i = 1; i < 5; ++i) {
        const int gx = w * i / 5;
        const int gy = h * i / 5;
        p.drawLine(gx, 0, gx, h);
        p.drawLine(0, gy, w, gy);
    }

    if (learned) {
        // dashed reticle box
        QPen rect;
        rect.setColor(QColor(ptn::ACC));
        rect.setStyle(Qt::DashLine);
        rect.setWidthF(1.2);
        p.setPen(rect);
        const int rw = w * 7 / 10, rh = h * 7 / 10;
        const int rx = (w - rw) / 2, ry = (h - rh) / 2;
        p.drawRect(QRect(rx, ry, rw, rh));

        // crosshairs
        QColor mid(ptn::ACC); mid.setAlpha(68);
        p.setPen(QPen(mid, 0.8));
        p.drawLine(w / 2, 0, w / 2, h);
        p.drawLine(0, h / 2, w, h / 2);

        // inner ring + dot
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(ptn::ACC), 1.5));
        p.drawEllipse(QPointF(w / 2.0, h / 2.0), 5, 5);
        p.setBrush(QColor(ptn::ACC)); p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(w / 2.0, h / 2.0), 1.5, 1.5);

        // bottom legend
        p.setPen(QColor(ptn::OK));
        p.setFont(QFont("JetBrains Mono", 7, QFont::Bold));
        p.drawText(QRect(0, h - 16, w, 14), Qt::AlignCenter, "● LEARNED");
    } else {
        p.setPen(QColor(ptn::TXT3));
        p.setFont(QFont("JetBrains Mono", 8));
        p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, "Not learned");
    }
    return pm;
}

QString sectionHeaderQss() {
    return QString(
        "QLabel { background: %1; color: %2; "
        "  font: 700 8pt 'Segoe UI'; letter-spacing: 1.2px; "
        "  padding: 4px 10px; border-bottom: 1px solid #252526;"
        "  border-top: 1px solid %3; }"
    ).arg(ptn::HD, ptn::TXT3, ptn::BD);
}

QString propLabelQss() {
    return QString(
        "color: %1; font: 500 9.5pt 'Segoe UI'; padding-left: 2px;"
    ).arg(ptn::TXT3);
}

QString inputQssCompact() {
    return QString(
        "QSpinBox, QDoubleSpinBox, QComboBox {"
        "  background: %1; color: %2; "
        "  border: 1px solid %3; border-radius: 3px; "
        "  padding: 1px 6px; font: 9.5pt 'JetBrains Mono';"
        "  min-height: 18px; }"
        "QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {"
        "  border: 1px solid %4; }"
    ).arg(ptn::BG, ptn::TXT, ptn::BD, ptn::ACC);
}

// Reusable ghost button style — small variant for under-tree actions
QString smallGhostBtnQss() {
    return QString(
        "QPushButton {"
        "  background: transparent; color: %1; "
        "  border: 1px solid %2; border-radius: 3px;"
        "  padding: 4px 9px; font: 600 9pt 'Segoe UI'; }"
        "QPushButton:hover { background: %3; color: %4; border-color: %5; }"
    ).arg(ptn::TXT2, ptn::BD, ptn::SURF2, ptn::TXT, ptn::BD2);
}

QFrame *thinSeparator() {
    auto *f = new QFrame; f->setFixedHeight(1);
    f->setStyleSheet(QString("background: %1;").arg(ptn::BD));
    return f;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────

PatternSettingPanel::PatternSettingPanel(QWidget *parent) : QWidget(parent) {
    setStyleSheet(QString(
        "QWidget#PatternSettingPanel { background: %1; }"
        "QToolTip { background: %2; color: %3; border: 1px solid %4; }"
    ).arg(ptn::BG, ptn::SURF2, ptn::TXT, ptn::BD));
    setObjectName("PatternSettingPanel");
    buildUi();
    clearSelection();
}

// ── Public API ──────────────────────────────────────────────────────────────

void PatternSettingPanel::setSelection(mtc::MatchGroup *group,
                                        mtc::MatchPattern *pattern) {
    m_group   = group;
    m_pattern = pattern;

    if (!group)       { m_stack->setCurrentIndex(kEmpty);   return; }
    if (!pattern)     { m_stack->setCurrentIndex(kGroup);   refreshFromGroup();   return; }
    /* both */          m_stack->setCurrentIndex(kPattern); refreshFromPattern();
}

void PatternSettingPanel::clearSelection() {
    m_group = nullptr; m_pattern = nullptr;
    m_stack->setCurrentIndex(kEmpty);
}

void PatternSettingPanel::setPatternThumbnail(const QPixmap &pix) {
    if (!m_lblThumb) return;
    if (pix.isNull()) {
        m_lblThumb->setPixmap(makePlaceholderPixmap(
            m_lblThumb->width()  > 0 ? m_lblThumb->width()  : 240,
            m_lblThumb->height() > 0 ? m_lblThumb->height() : 140,
            m_pattern && !m_pattern->config().m_rawImage.empty()));
        return;
    }
    m_lblThumb->setPixmap(pix.scaled(m_lblThumb->size(), Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
}

void PatternSettingPanel::setShowInlineThumbnail(bool show) {
    // Hide the wrapper so the entire strip — thumbnail + Trigger / Open
    // buttons — collapses to zero height.  Hiding only the QLabel would
    // leave the wrapper's padding + button row taking up vertical space.
    if (m_thumbWrap) m_thumbWrap->setVisible(show);
    if (m_lblThumb)  m_lblThumb->setVisible(show);
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI construction
// ─────────────────────────────────────────────────────────────────────────────

// Helper — wrap any page in a vertically-scrollable QScrollArea so the
// property rows always reach the user even when the panel is short.
static QWidget *wrapInScroll(QWidget *page) {
    auto *sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sa->setStyleSheet(QString(
        "QScrollArea { background: %1; border: none; }"
        "QScrollBar:vertical { background: %1; width: 8px; }"
        "QScrollBar::handle:vertical { background: %2; border-radius: 3px; min-height: 18px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0; }"
    ).arg(ptn::BG, ptn::BD2));
    sa->setWidget(page);
    return sa;
}

void PatternSettingPanel::buildUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget;
    m_stack->addWidget(buildEmptyPage());                      // index 0 (no scroll needed)
    m_stack->addWidget(wrapInScroll(buildGroupPage()));         // index 1
    m_stack->addWidget(wrapInScroll(buildPatternPage()));       // index 2
    root->addWidget(m_stack, 1);
}

// ── Empty page ─────────────────────────────────────────────────────────────

QWidget *PatternSettingPanel::buildEmptyPage() {
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setAlignment(Qt::AlignCenter);
    auto *lbl = new QLabel(tr("Select a group or pattern"));
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet(QString(
        "color: %1; font: 9.5pt 'Segoe UI'; padding: 16px;"
    ).arg(ptn::TXT3));
    lay->addWidget(lbl);
    return w;
}

// ── Group page ─────────────────────────────────────────────────────────────

QWidget *PatternSettingPanel::buildSectionHeader(const QString &label) {
    auto *l = new QLabel(label.toUpper());
    l->setStyleSheet(sectionHeaderQss());
    return l;
}

QWidget *PatternSettingPanel::buildPropRow(const QString &label, QWidget *editor) {
    auto *row = new QWidget;
    row->setStyleSheet(QString(
        "QWidget { background: transparent; border-bottom: 1px solid #252526; }"
    ));
    auto *lay = new QGridLayout(row);
    lay->setContentsMargins(12, 3, 10, 3);
    lay->setHorizontalSpacing(8); lay->setVerticalSpacing(0);
    lay->setColumnStretch(0, 1); lay->setColumnStretch(1, 1);

    auto *l = new QLabel(label);
    l->setStyleSheet(propLabelQss());
    l->setMinimumHeight(20);
    lay->addWidget(l, 0, 0);
    if (editor) {
        editor->setStyleSheet(editor->styleSheet() + inputQssCompact());
        lay->addWidget(editor, 0, 1);
    }
    return row;
}

QSpinBox *PatternSettingPanel::makeIntSpin(int min, int max, int initial) {
    auto *s = new QSpinBox; s->setRange(min, max); s->setValue(initial);
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    return s;
}

QDoubleSpinBox *PatternSettingPanel::makeDoubleSpin(double min, double max,
                                                      double step, int decimals,
                                                      double initial) {
    auto *s = new QDoubleSpinBox;
    s->setRange(min, max); s->setSingleStep(step);
    s->setDecimals(decimals); s->setValue(initial);
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    return s;
}

QComboBox *PatternSettingPanel::makeKernelCombo() {
    auto *c = new QComboBox;
    for (int k : { 1, 3, 5, 7 }) c->addItem(QString::number(k), k);
    return c;
}

QCheckBox *PatternSettingPanel::makeToggle() {
    auto *c = new QCheckBox;
    c->setStyleSheet(QString(
        "QCheckBox::indicator { width: 28px; height: 14px; border-radius: 7px;"
        "  background: %1; border: 1px solid %2; }"
        "QCheckBox::indicator:checked {"
        "  background: %3; border: 1px solid %3;"
        "  image: none; }"
    ).arg(ptn::BD, ptn::BD2, ptn::ACC));
    return c;
}

QWidget *PatternSettingPanel::buildGroupPage() {
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0); lay->setSpacing(0);

    lay->addWidget(buildSectionHeader(tr("Group Config")));

    m_lblGroupName = new QLabel;
    m_lblGroupName->setStyleSheet(QString(
        "color: %1; font: 9.5pt 'JetBrains Mono'; padding: 2px 6px;"
    ).arg(ptn::TXT2));
    lay->addWidget(buildPropRow(tr("Group Name"), m_lblGroupName));

    auto *numWrap = new QWidget;
    auto *numLay  = new QHBoxLayout(numWrap);
    numLay->setContentsMargins(0, 0, 0, 0); numLay->setSpacing(6);
    m_lblGroupNumber = new QLabel;
    m_lblGroupNumber->setStyleSheet(QString(
        "color: %1; font: 700 9.5pt 'JetBrains Mono'; padding: 2px 6px;"
    ).arg(ptn::WARN));
    auto *numHint = new QLabel(tr("runtime selector"));
    numHint->setStyleSheet(QString("color: %1; font: 8pt 'Segoe UI';").arg(ptn::TXT3));
    numLay->addWidget(m_lblGroupNumber); numLay->addWidget(numHint); numLay->addStretch();
    lay->addWidget(buildPropRow(tr("Group Number"), numWrap));

    lay->addWidget(buildSectionHeader(tr("Picking Box")));

    m_spinLowRatio = makeDoubleSpin(0.0, 100.0, 0.1, 2, 1.5);
    connect(m_spinLowRatio, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){
        if (m_blockSignals) return;
        emit groupFieldChanged(QStringLiteral("lowWorkpieceRatio"), v);
    });
    lay->addWidget(buildPropRow(tr("Low Workpiece Ratio"), m_spinLowRatio));

    m_spinPickBoxW = makeDoubleSpin(0.0, 10000.0, 1.0, 1, 50.0);
    connect(m_spinPickBoxW, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){
        if (m_blockSignals) return;
        emit groupFieldChanged(QStringLiteral("pickBoxW"), v);
    });
    lay->addWidget(buildPropRow(tr("Box Width (px)"), m_spinPickBoxW));

    m_spinPickBoxH = makeDoubleSpin(0.0, 10000.0, 1.0, 1, 50.0);
    connect(m_spinPickBoxH, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){
        if (m_blockSignals) return;
        emit groupFieldChanged(QStringLiteral("pickBoxH"), v);
    });
    lay->addWidget(buildPropRow(tr("Box Height (px)"), m_spinPickBoxH));

    m_spinPickBoxDist = makeDoubleSpin(0.0, 10000.0, 1.0, 1, 0.0);
    connect(m_spinPickBoxDist, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){
        if (m_blockSignals) return;
        emit groupFieldChanged(QStringLiteral("pickBoxDist"), v);
    });
    lay->addWidget(buildPropRow(tr("Distance (px)"), m_spinPickBoxDist));

    m_spinPickBoxAngle = makeDoubleSpin(-360.0, 360.0, 1.0, 1, 0.0);
    connect(m_spinPickBoxAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){
        if (m_blockSignals) return;
        emit groupFieldChanged(QStringLiteral("pickBoxAngle"), v);
    });
    lay->addWidget(buildPropRow(tr("Angle (°)"), m_spinPickBoxAngle));

    lay->addStretch();
    return w;
}

// ── Pattern page ───────────────────────────────────────────────────────────

QWidget *PatternSettingPanel::buildPatternPage() {
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0); lay->setSpacing(0);

    // Title row: "#N name"  +  LEARNED / NOT LEARNED
    auto *titleRow = new QWidget;
    titleRow->setStyleSheet(QString(
        "background: %1; border-bottom: 1px solid %2;"
    ).arg(ptn::HD, ptn::BD));
    auto *titleLay = new QHBoxLayout(titleRow);
    titleLay->setContentsMargins(10, 6, 10, 6); titleLay->setSpacing(8);

    m_lblPatternTitle = new QLabel;
    m_lblPatternTitle->setStyleSheet(QString(
        "color: %1; font: 700 8.5pt 'Segoe UI'; letter-spacing: 1.2px;"
    ).arg(ptn::TXT3));
    titleLay->addWidget(m_lblPatternTitle, 1);

    m_lblLearnedBadge = new QLabel;
    m_lblLearnedBadge->setStyleSheet(QString(
        "font: 800 8pt 'Segoe UI'; padding: 1px 6px; border-radius: 3px;"
    ));
    titleLay->addWidget(m_lblLearnedBadge);
    lay->addWidget(titleRow);

    // Thumbnail
    auto *thumbWrap = new QWidget;
    m_thumbWrap = thumbWrap;
    thumbWrap->setStyleSheet(QString("background: %1;").arg(ptn::BG));
    auto *thumbLay = new QVBoxLayout(thumbWrap);
    thumbLay->setContentsMargins(10, 8, 10, 4); thumbLay->setSpacing(6);

    m_lblThumb = new QLabel;
    m_lblThumb->setMinimumHeight(120);
    m_lblThumb->setMaximumHeight(160);
    m_lblThumb->setAlignment(Qt::AlignCenter);
    m_lblThumb->setStyleSheet(QString(
        "QLabel { background: #181818; border: 1px solid %1; border-radius: 4px; }"
    ).arg(ptn::BD));
    thumbLay->addWidget(m_lblThumb);

    // Buttons row
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    m_btnLearn = new QPushButton(tr("▶  Trigger & Learn"));
    m_btnLearn->setStyleSheet(QString(
        "QPushButton {"
        "  background: rgba(34,209,122,21); color: %1; "
        "  border: 1px solid %1; border-radius: 3px;"
        "  padding: 4px 10px; font: 600 9pt 'Segoe UI'; }"
        "QPushButton:hover { background: rgba(34,209,122,40); }"
    ).arg(ptn::OK));
    connect(m_btnLearn, &QPushButton::clicked,
            this, &PatternSettingPanel::learnRequested);

    m_btnOpenImage = new QPushButton(tr("📁  Open Image"));
    m_btnOpenImage->setStyleSheet(smallGhostBtnQss());
    connect(m_btnOpenImage, &QPushButton::clicked,
            this, &PatternSettingPanel::openImageRequested);

    btnRow->addWidget(m_btnLearn); btnRow->addWidget(m_btnOpenImage);
    btnRow->addStretch();
    thumbLay->addLayout(btnRow);
    lay->addWidget(thumbWrap);

    // ── Match Settings ─────────────────────────────────────────────────────
    lay->addWidget(buildSectionHeader(tr("Match Settings")));

    auto *patNumWrap = new QWidget;
    auto *patNumLay  = new QHBoxLayout(patNumWrap);
    patNumLay->setContentsMargins(0, 0, 0, 0); patNumLay->setSpacing(6);
    m_lblPatternNumber = new QLabel;
    m_lblPatternNumber->setStyleSheet(QString(
        "color: %1; font: 700 9.5pt 'JetBrains Mono'; padding: 2px 6px;"
    ).arg(ptn::OUTPUT));
    auto *outputHint = new QLabel(tr("output ID"));
    outputHint->setStyleSheet(QString("color: %1; font: 8pt 'Segoe UI';").arg(ptn::TXT3));
    patNumLay->addWidget(m_lblPatternNumber); patNumLay->addWidget(outputHint); patNumLay->addStretch();
    lay->addWidget(buildPropRow(tr("Pattern Number"), patNumWrap));

    m_spinMinScore = makeDoubleSpin(0.0, 1.0, 0.01, 3, 0.85);
    connect(m_spinMinScore, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){ if(!m_blockSignals) emit patternFieldChanged("minScore", v); });
    lay->addWidget(buildPropRow(tr("Min Score"), m_spinMinScore));

    m_spinAngle = makeDoubleSpin(-360.0, 360.0, 1.0, 1, 0.0);
    connect(m_spinAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){ if(!m_blockSignals) emit patternFieldChanged("angle", v); });
    lay->addWidget(buildPropRow(tr("Angle (°)"), m_spinAngle));

    m_spinTolAngle = makeDoubleSpin(0.0, 360.0, 1.0, 1, 180.0);
    connect(m_spinTolAngle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){ if(!m_blockSignals) emit patternFieldChanged("toleranceAngle", v); });
    lay->addWidget(buildPropRow(tr("Tolerance Angle (°)"), m_spinTolAngle));

    m_spinMaxOverlap = makeDoubleSpin(0.0, 1.0, 0.01, 3, 0.1);
    connect(m_spinMaxOverlap, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){ if(!m_blockSignals) emit patternFieldChanged("maxOverlap", v); });
    lay->addWidget(buildPropRow(tr("Max Overlap"), m_spinMaxOverlap));

    m_spinPickX = makeIntSpin(-100000, 100000, 0);
    connect(m_spinPickX, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){ if(!m_blockSignals) emit patternFieldChanged("pickX", v); });
    lay->addWidget(buildPropRow(tr("Pick X (px)"), m_spinPickX));

    m_spinPickY = makeIntSpin(-100000, 100000, 0);
    connect(m_spinPickY, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){ if(!m_blockSignals) emit patternFieldChanged("pickY", v); });
    lay->addWidget(buildPropRow(tr("Pick Y (px)"), m_spinPickY));

    // ── Edge Match Config ──────────────────────────────────────────────────
    lay->addWidget(buildSectionHeader(tr("Edge Match Config")));

    m_spinThreshLower = makeIntSpin(0, 1000, 100);
    connect(m_spinThreshLower, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){ if(!m_blockSignals) emit patternFieldChanged("edge.threshLower", v); });
    lay->addWidget(buildPropRow(tr("Thresh Lower"), m_spinThreshLower));

    m_spinThreshUpper = makeIntSpin(0, 1000, 200);
    connect(m_spinThreshUpper, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){ if(!m_blockSignals) emit patternFieldChanged("edge.threshUpper", v); });
    lay->addWidget(buildPropRow(tr("Thresh Upper"), m_spinThreshUpper));

    m_cmbKernelSize = makeKernelCombo();
    connect(m_cmbKernelSize, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){
        if (m_blockSignals) return;
        emit patternFieldChanged("edge.kernelSize", m_cmbKernelSize->currentData().toInt());
    });
    lay->addWidget(buildPropRow(tr("Kernel Size"), m_cmbKernelSize));

    auto *blurWrap = new QWidget;
    auto *blurLay  = new QHBoxLayout(blurWrap);
    blurLay->setContentsMargins(0, 0, 0, 0); blurLay->setSpacing(4);
    m_spinBlurW = makeIntSpin(1, 99, 5);
    m_spinBlurH = makeIntSpin(1, 99, 5);
    connect(m_spinBlurW, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){ if(!m_blockSignals) emit patternFieldChanged("edge.blurW", v); });
    connect(m_spinBlurH, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){ if(!m_blockSignals) emit patternFieldChanged("edge.blurH", v); });
    blurLay->addWidget(m_spinBlurW); blurLay->addWidget(m_spinBlurH);
    lay->addWidget(buildPropRow(tr("Blur W × H"), blurWrap));

    m_spinGreediness = makeDoubleSpin(0.0, 1.0, 0.01, 3, 0.0);
    connect(m_spinGreediness, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){ if(!m_blockSignals) emit patternFieldChanged("edge.greediness", v); });
    lay->addWidget(buildPropRow(tr("Greediness"), m_spinGreediness));

    m_spinMinReduceLen = makeIntSpin(1, 10000, 32);
    connect(m_spinMinReduceLen, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){ if(!m_blockSignals) emit patternFieldChanged("edge.minReduceLength", v); });
    lay->addWidget(buildPropRow(tr("Min Reduce Len (px)"), m_spinMinReduceLen));

    m_spinTSamples = makeIntSpin(1, 100, 3);
    connect(m_spinTSamples, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v){ if(!m_blockSignals) emit patternFieldChanged("edge.tSamples", v); });
    lay->addWidget(buildPropRow(tr("T Samples"), m_spinTSamples));

    m_chkInvertBinary = makeToggle();
    connect(m_chkInvertBinary, &QCheckBox::toggled,
            this, [this](bool v){ if(!m_blockSignals) emit patternFieldChanged("edge.invertBinary", v); });
    lay->addWidget(buildPropRow(tr("Invert Binary"), m_chkInvertBinary));

    m_chkSubPixel = makeToggle();
    connect(m_chkSubPixel, &QCheckBox::toggled,
            this, [this](bool v){ if(!m_blockSignals) emit patternFieldChanged("edge.subPixel", v); });
    lay->addWidget(buildPropRow(tr("Sub-pixel"), m_chkSubPixel));

    m_chkStopAtL1 = makeToggle();
    connect(m_chkStopAtL1, &QCheckBox::toggled,
            this, [this](bool v){ if(!m_blockSignals) emit patternFieldChanged("edge.stopAtLayer1", v); });
    lay->addWidget(buildPropRow(tr("Stop at Layer 1"), m_chkStopAtL1));

    lay->addStretch();
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Refresh from model
// ─────────────────────────────────────────────────────────────────────────────

void PatternSettingPanel::blockEditorSignals(bool block) { m_blockSignals = block; }

void PatternSettingPanel::refreshFromGroup() {
    if (!m_group) return;
    const auto &g = m_group->config();

    blockEditorSignals(true);
    m_lblGroupName->setText(QString::fromStdWString(g.m_groupName));
    m_lblGroupNumber->setText(QString::number(g.m_groupIndex));
    m_spinLowRatio    ->setValue(g.m_lowWorkpieceRatio);
    m_spinPickBoxW    ->setValue(g.m_pickingBoxSize.width);
    m_spinPickBoxH    ->setValue(g.m_pickingBoxSize.height);
    m_spinPickBoxDist ->setValue(g.m_pickingBoxDistance);
    m_spinPickBoxAngle->setValue(g.m_pickingBoxAngle);
    blockEditorSignals(false);
}

void PatternSettingPanel::refreshFromPattern() {
    if (!m_pattern) return;
    const auto &p = m_pattern->config();
    const bool learned = !p.m_rawImage.empty();

    blockEditorSignals(true);

    // Title row
    m_lblPatternTitle->setText(QString("#%1  %2")
                                   .arg(p.m_patternIndex)
                                   .arg(QString::fromStdWString(p.m_patternName)));
    m_lblLearnedBadge->setText(learned ? tr("● LEARNED") : tr("● NOT LEARNED"));
    m_lblLearnedBadge->setStyleSheet(QString(
        "font: 800 8pt 'Segoe UI'; padding: 1px 6px; border-radius: 3px; "
        "color: %1; background: rgba(%2, 30);"
    ).arg(learned ? ptn::OK : ptn::ERR,
          learned ? "34,209,122" : "232,64,64"));

    // Thumbnail — placeholder if no real image
    setPatternThumbnail(QPixmap{});

    // Match Settings
    m_lblPatternNumber->setText(QString("#%1").arg(p.m_patternIndex));
    m_spinMinScore   ->setValue(p.m_minScore);
    m_spinAngle      ->setValue(p.m_angle);
    m_spinTolAngle   ->setValue(p.m_toleranceAngle);
    m_spinMaxOverlap ->setValue(p.m_maxOverlap);
    m_spinPickX      ->setValue(static_cast<int>(p.m_pickPosition.x));
    m_spinPickY      ->setValue(static_cast<int>(p.m_pickPosition.y));

    // Edge config — only if EdgeBased
    if (auto *e = p.edgeConfig()) {
        m_spinThreshLower  ->setValue(static_cast<int>(e->threshLower));
        m_spinThreshUpper  ->setValue(static_cast<int>(e->threshUpper));
        // kernel combo: find index
        const int targetIdx = m_cmbKernelSize->findData(e->kernelSize);
        if (targetIdx >= 0) m_cmbKernelSize->setCurrentIndex(targetIdx);
        m_spinBlurW        ->setValue(e->blurWidth);
        m_spinBlurH        ->setValue(e->blurHeight);
        m_spinGreediness   ->setValue(e->greediness);
        m_spinMinReduceLen ->setValue(e->minReduceLength);
        m_spinTSamples     ->setValue(e->tSamples);
        m_chkInvertBinary  ->setChecked(e->invertBinaryThreshold);
        m_chkSubPixel      ->setChecked(e->subPixelEstimation);
        m_chkStopAtL1      ->setChecked(e->stopAtLayer1);
    }

    blockEditorSignals(false);
}
