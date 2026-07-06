#include "signals_monitor_widget.h"

#include "ui/widgets/controls/state_pill_label.h"
#include "ui/widgets/controls/type_chip_label.h"
#include "core/logger/app_logger.h"
#include "core/utils/theme_manager.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QRadioButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace vc::widgets::sm_internal {

namespace {

// Re-evaluate the QSS after toggling a dynamic property. Mirrors the helper
// in add_device_wizard.cpp (design_rules 15.3).
void repolish(QWidget *w) {
    if (!w) return;
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

constexpr int kRisingEdgeHideMs = 2000;
constexpr int kRisingEdgeBlinkMs = 500;
constexpr int kRisingEdgeWindowMs = 200;

// Column hint floor — keeps a freshly added empty row from collapsing.
constexpr int kModifyBtnWidth = 32;
constexpr int kRowHMargin = 8;
constexpr int kRowVMargin = 4;
constexpr int kRowSpacing = 8;
constexpr int kValueInnerSpacing = 6;

} // namespace

// =====================================================================
// RowWidget — one entry in the monitor list
// =====================================================================
class RowWidget : public QFrame {
    Q_OBJECT
public:
    using Type = SignalsMonitorWidget::Type;

    RowWidget(const QString &internalName,
              const QString &displayName,
              Type type,
              QWidget *parent = nullptr)
        : QFrame(parent),
          m_internalName(internalName),
          m_displayName(displayName),
          m_type(type) {
        setObjectName(QStringLiteral("smwRow"));
        setFrameShape(QFrame::NoFrame);

        auto *lay = new QHBoxLayout(this);
        lay->setContentsMargins(kRowHMargin, kRowVMargin, kRowHMargin, kRowVMargin);
        lay->setSpacing(kRowSpacing);

        // ── Name ────────────────────────────────────────────────────────
        m_lblName = new QLabel(displayName, this);
        m_lblName->setObjectName(QStringLiteral("smwName"));

        // ── Type chip ───────────────────────────────────────────────────
        m_lblType = new TypeChipLabel(this);
        m_lblType->setObjectName(QStringLiteral("smwTypeChip"));
        m_lblType->setProperty("typeKind", type == Type::Bool ? "bool" : "number");
        m_lblType->setText(type == Type::Bool ? tr("Bool") : tr("Num"));
        m_lblType->setAlignment(Qt::AlignCenter);

        // ── Value cell (holds all 3 possible representations) ──────────
        m_valueCell = new QWidget(this);
        m_valueCell->setObjectName(QStringLiteral("smwValueCell"));
        auto *vlay = new QHBoxLayout(m_valueCell);
        vlay->setContentsMargins(0, 0, 0, 0);
        vlay->setSpacing(kValueInnerSpacing);

        m_lblOnOff = new StatePillLabel(m_valueCell);
        m_lblOnOff->setObjectName(QStringLiteral("smwOnOff"));
        m_lblOnOff->setProperty("onOffState", "off");
        m_lblOnOff->setText(tr("OFF"));
        m_lblOnOff->setAlignment(Qt::AlignCenter);

        m_lblRising = new QLabel(tr("RE"), m_valueCell);
        m_lblRising->setObjectName(QStringLiteral("smwRisingEdge"));
        m_lblRising->setAlignment(Qt::AlignCenter);
        m_lblRising->setToolTip(tr("Rising edge — pulse shorter than %1 ms")
                                    .arg(kRisingEdgeWindowMs));
        // Reserve layout space even when hidden so column width never jitters.
        {
            QSizePolicy sp = m_lblRising->sizePolicy();
            sp.setRetainSizeWhenHidden(true);
            m_lblRising->setSizePolicy(sp);
        }
        m_lblRising->setVisible(false);

        m_lblNumber = new QLabel(QStringLiteral("0"), m_valueCell);
        m_lblNumber->setObjectName(QStringLiteral("smwValueNumber"));
        m_lblNumber->setAlignment(Qt::AlignCenter);

        m_lblUnmapped = new QLabel(tr("(not mapped)"), m_valueCell);
        m_lblUnmapped->setObjectName(QStringLiteral("smwValueUnmapped"));
        m_lblUnmapped->setAlignment(Qt::AlignCenter);

        vlay->addWidget(m_lblOnOff);
        vlay->addWidget(m_lblRising);
        vlay->addWidget(m_lblNumber);
        vlay->addWidget(m_lblUnmapped);
        vlay->addStretch(1);

        // ── Modify button ──────────────────────────────────────────────
        m_btnModify = new QToolButton(this);
        m_btnModify->setObjectName(QStringLiteral("smwModifyBtn"));
        m_btnModify->setText(QStringLiteral("..."));
        m_btnModify->setAutoRaise(true);
        m_btnModify->setToolTip(tr("Modify value"));
        connect(m_btnModify, &QToolButton::clicked,
                this, &RowWidget::modifyRequested);

        lay->addWidget(m_lblName);
        lay->addWidget(m_lblType);
        lay->addWidget(m_valueCell, /*stretch*/ 1);
        lay->addWidget(m_btnModify);

        // ── Rising-edge timers ─────────────────────────────────────────
        m_hideReTimer = new QTimer(this);
        m_hideReTimer->setSingleShot(true);
        m_hideReTimer->setInterval(kRisingEdgeHideMs);
        connect(m_hideReTimer, &QTimer::timeout, this, [this] {
            m_lblRising->setVisible(false);
            m_reVisible = false;
        });

        m_blinkReTimer = new QTimer(this);
        m_blinkReTimer->setSingleShot(true);
        m_blinkReTimer->setInterval(kRisingEdgeBlinkMs);
        connect(m_blinkReTimer, &QTimer::timeout, this, [this] {
            m_lblRising->setVisible(true);
            m_hideReTimer->start();
        });

        // Initial render — unmapped, value chips hidden.
        applyValueVisibility();
    }

    // ── Public accessors ───────────────────────────────────────────────
    QString internalName() const { return m_internalName; }
    QString displayName()  const { return m_displayName; }
    Type    type()         const { return m_type; }
    QString tag()          const { return m_tag; }

    void setTag(const QString &tag) {
        m_tag = tag;
        applyValueVisibility();
        updateModifyEnabled();
    }

    void setDeviceConnected(bool connected) {
        m_deviceConnected = connected;
        updateModifyEnabled();
        // Keep tooltip informative when disabled-due-to-disconnect.
        m_btnModify->setToolTip(m_deviceConnected
                                    ? tr("Modify value")
                                    : tr("Device disconnected"));
    }

    void setBoolValue(bool v) {
        const bool first = !m_hasBool;
        const bool last  = m_lastBool;
        m_hasBool = true;
        m_lastBool = v;

        // Render chip.
        m_lblOnOff->setText(v ? tr("ON") : tr("OFF"));
        m_lblOnOff->setProperty("onOffState", v ? "on" : "off");
        repolish(m_lblOnOff);

        if (first) {
            m_tRising.invalidate();
            return;
        }
        if (last == v) return;   // no transition

        if (!last && v) {        // OFF→ON
            m_tRising.start();
        } else if (last && !v) { // ON→OFF
            if (m_tRising.isValid() &&
                m_tRising.elapsed() < kRisingEdgeWindowMs) {
                triggerRisingEdge();
            }
            m_tRising.invalidate();
        }
    }

    void setNumberValue(int v) {
        m_hasNum = true;
        m_lastNum = v;
        m_lblNumber->setText(QString::number(v));
    }

    // Snapshot of last value — used to seed the Modify dialog.
    QVariant currentValueForModify() const {
        if (m_type == Type::Bool) {
            return m_hasBool ? QVariant(m_lastBool) : QVariant(false);
        }
        return m_hasNum ? QVariant(m_lastNum) : QVariant(0);
    }

    // ── Column width contract ──────────────────────────────────────────
    struct ColumnHints { int name, type, value, modify; };
    ColumnHints columnSizeHints() const {
        // Take the max of all candidate value widgets so the column reserves
        // enough room for any state without jittering.
        const int wBool = m_lblOnOff->sizeHint().width()
                          + kValueInnerSpacing
                          + m_lblRising->sizeHint().width();
        const int wNum = m_lblNumber->sizeHint().width();
        const int wUnm = m_lblUnmapped->sizeHint().width();
        int wValue = qMax(wBool, qMax(wNum, wUnm));

        return ColumnHints{
            m_lblName->sizeHint().width(),
            m_lblType->sizeHint().width(),
            wValue,
            qMax(kModifyBtnWidth, m_btnModify->sizeHint().width())
        };
    }

    void applyColumnWidths(int name, int type, int value, int modify) {
        m_lblName->setFixedWidth(name);
        m_lblType->setFixedWidth(type);
        m_valueCell->setFixedWidth(value);
        m_btnModify->setFixedWidth(modify);
    }

signals:
    void modifyRequested();

private:
    void applyValueVisibility() {
        const bool unmapped = m_tag.isEmpty();
        m_lblUnmapped->setVisible(unmapped);
        if (unmapped) {
            m_lblOnOff->setVisible(false);
            m_lblRising->setVisible(false);
            m_lblNumber->setVisible(false);
            return;
        }
        if (m_type == Type::Bool) {
            m_lblOnOff->setVisible(true);
            // m_lblRising visibility is owned by RE state machine; keep as-is.
            m_lblNumber->setVisible(false);
        } else {
            m_lblOnOff->setVisible(false);
            m_lblRising->setVisible(false);
            m_lblNumber->setVisible(true);
        }
    }

    void updateModifyEnabled() {
        m_btnModify->setEnabled(!m_tag.isEmpty() && m_deviceConnected);
    }

    void triggerRisingEdge() {
        if (!m_reVisible) {
            m_lblRising->setVisible(true);
            m_reVisible = true;
            m_hideReTimer->start();
        } else {
            // Blink: hide for 500 ms, then show again and reset the 2 s hide.
            m_hideReTimer->stop();
            m_lblRising->setVisible(false);
            m_blinkReTimer->start();
        }
    }

    QString m_internalName;
    QString m_displayName;
    Type    m_type;
    QString m_tag;

    bool m_deviceConnected{false};

    QLabel      *m_lblName{nullptr};
    QLabel      *m_lblType{nullptr};
    QWidget     *m_valueCell{nullptr};
    QLabel      *m_lblOnOff{nullptr};
    QLabel      *m_lblRising{nullptr};
    QLabel      *m_lblNumber{nullptr};
    QLabel      *m_lblUnmapped{nullptr};
    QToolButton *m_btnModify{nullptr};

    bool         m_hasBool{false};
    bool         m_lastBool{false};
    QElapsedTimer m_tRising;
    QTimer      *m_hideReTimer{nullptr};
    QTimer      *m_blinkReTimer{nullptr};
    bool         m_reVisible{false};

    bool         m_hasNum{false};
    int          m_lastNum{0};
};

// =====================================================================
// ModifyValueDialog — gathers a new value from the user
// =====================================================================
class ModifyValueDialog : public QDialog {
    Q_OBJECT
public:
    using Type = SignalsMonitorWidget::Type;

    ModifyValueDialog(Type type,
                      const QString &displayName,
                      const QVariant &initial,
                      QWidget *parent)
        : QDialog(parent), m_type(type) {
        setWindowTitle(tr("Modify — %1").arg(displayName));
        setObjectName(QStringLiteral("smwModifyDialog"));

        auto *root = new QVBoxLayout(this);

        if (type == Type::Bool) {
            buildBool(root, initial.toBool());
        } else {
            buildNumber(root, initial.toInt());
        }

        auto *bb = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        root->addWidget(bb);
        connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    QVariant value() const {
        if (m_type == Type::Bool) {
            return QVariant(m_rbOn && m_rbOn->isChecked());
        }
        // Number — read the active page's spinbox.
        return QVariant(m_stack->currentIndex() == 0
                            ? m_spSigned->value()
                            : m_spUnsigned->value());
    }

private:
    void buildBool(QVBoxLayout *root, bool initial) {
        m_rbOn  = new QRadioButton(tr("ON"),  this);
        m_rbOff = new QRadioButton(tr("OFF"), this);
        (initial ? m_rbOn : m_rbOff)->setChecked(true);
        root->addWidget(m_rbOn);
        root->addWidget(m_rbOff);
    }

    void buildNumber(QVBoxLayout *root, int initial) {
        // Combobox picks interpretation; stacked widget swaps the spinbox.
        // Default page: signed if initial fits signed range, else unsigned.
        m_cbInterp = new QComboBox(this);
        m_cbInterp->addItem(tr("Signed int16 (-32768..32767)"));
        m_cbInterp->addItem(tr("Unsigned int16 (0..65535)"));

        m_stack = new QStackedWidget(this);

        m_spSigned = new QSpinBox(this);
        m_spSigned->setRange(-32768, 32767);
        m_spUnsigned = new QSpinBox(this);
        m_spUnsigned->setRange(0, 65535);

        m_stack->addWidget(m_spSigned);
        m_stack->addWidget(m_spUnsigned);

        // Default to unsigned only when the value cannot fit signed int16.
        const int initialPage = (initial > 32767) ? 1 : 0;
        if (initialPage == 0) {
            m_spSigned->setValue(qBound(-32768, initial, 32767));
        } else {
            m_spUnsigned->setValue(qBound(0, initial, 65535));
        }
        m_cbInterp->setCurrentIndex(initialPage);
        m_stack->setCurrentIndex(initialPage);

        connect(m_cbInterp,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this](int i) {
            // When switching, carry the visible value over, clamped to the
            // destination range.
            const int v = (m_stack->currentIndex() == 0)
                              ? m_spSigned->value()
                              : m_spUnsigned->value();
            m_stack->setCurrentIndex(i);
            if (i == 0) m_spSigned->setValue(qBound(-32768, v, 32767));
            else        m_spUnsigned->setValue(qBound(0,      v, 65535));
        });

        root->addWidget(m_cbInterp);
        root->addWidget(m_stack);
    }

    Type m_type;
    QRadioButton *m_rbOn{nullptr};
    QRadioButton *m_rbOff{nullptr};
    QComboBox    *m_cbInterp{nullptr};
    QStackedWidget *m_stack{nullptr};
    QSpinBox     *m_spSigned{nullptr};
    QSpinBox     *m_spUnsigned{nullptr};
};

} // namespace vc::widgets::sm_internal

using vc::widgets::sm_internal::RowWidget;
using vc::widgets::sm_internal::ModifyValueDialog;

// =====================================================================
// SignalsMonitorWidget
// =====================================================================

SignalsMonitorWidget::SignalsMonitorWidget(QWidget *parent)
    : QWidget(parent) {
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_list = new FlatListWidget(this);
    m_list->setObjectName(QStringLiteral("smwList"));
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    lay->addWidget(m_list);

    reloadStyleSheet();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &, bool) { reloadStyleSheet(); });
}

SignalsMonitorWidget::~SignalsMonitorWidget() = default;

void SignalsMonitorWidget::reloadStyleSheet() {
    const QString path = ThemeManager::instance()->isDark()
        ? QStringLiteral(":/styles/signals_monitor_widget_dark.qss")
        : QStringLiteral(":/styles/signals_monitor_widget_light.qss");
    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        setStyleSheet(ThemeManager::instance()->resolveTokens(
            QString::fromUtf8(f.readAll())));
    }
}

int SignalsMonitorWidget::rowIndexOf(const QString &internalName) const {
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i]->internalName() == internalName) return i;
    }
    return -1;
}

void SignalsMonitorWidget::appendRow(const QString &internalName,
                                     const QString &displayName,
                                     Type type) {
    insertRowAt(m_rows.size(), internalName, displayName, type);
}

void SignalsMonitorWidget::insertRowAt(int row,
                                       const QString &internalName,
                                       const QString &displayName,
                                       Type type) {
    if (rowIndexOf(internalName) >= 0) {
        LOG_USER_WARN << "SignalsMonitorWidget: duplicate internalName"
                      << internalName;
        return;
    }
    row = qBound(0, row, m_rows.size());

    auto *w = new RowWidget(internalName, displayName, type, m_list);
    w->setDeviceConnected(m_deviceConnected);
    connect(w, &RowWidget::modifyRequested, this, [this, w] {
        openModifyDialog(w);
    });

    m_rows.insert(row, w);

    auto *item = new QListWidgetItem();
    m_list->insertItem(row, item);
    m_list->setItemWidget(item, w);

    relayoutColumns();
}

void SignalsMonitorWidget::removeRow(const QString &internalName) {
    int row = rowIndexOf(internalName);
    if (row < 0) return;
    m_rows.removeAt(row);
    delete m_list->takeItem(row);   // takeItem returns ownership
    relayoutColumns();
}

void SignalsMonitorWidget::clearRows() {
    m_rows.clear();
    m_list->clear();
}

void SignalsMonitorWidget::setRowTag(const QString &internalName,
                                     const QString &tag) {
    int row = rowIndexOf(internalName);
    if (row < 0) return;
    m_rows[row]->setTag(tag);
    relayoutColumns();
}

QString SignalsMonitorWidget::rowTag(const QString &internalName) const {
    int row = rowIndexOf(internalName);
    return row < 0 ? QString() : m_rows[row]->tag();
}

void SignalsMonitorWidget::refreshBool(const QString &internalName, bool value) {
    int row = rowIndexOf(internalName);
    if (row < 0) {
        LOG_USER_WARN << "SignalsMonitorWidget: refreshBool unknown signal" << internalName;
        return;
    }
    if (m_rows[row]->type() != Type::Bool) {
        LOG_USER_WARN << "SignalsMonitorWidget: refreshBool on non-bool signal" << internalName;
        return;
    }
    m_rows[row]->setBoolValue(value);
}

void SignalsMonitorWidget::refreshNumber(const QString &internalName, int value) {
    int row = rowIndexOf(internalName);
    if (row < 0) {
        LOG_USER_WARN << "SignalsMonitorWidget: refreshNumber unknown signal" << internalName;
        return;
    }
    if (m_rows[row]->type() != Type::Number) {
        LOG_USER_WARN << "SignalsMonitorWidget: refreshNumber on non-number signal" << internalName;
        return;
    }
    m_rows[row]->setNumberValue(value);
    relayoutColumns();   // digit count may have changed
}

void SignalsMonitorWidget::setDeviceConnected(bool connected) {
    m_deviceConnected = connected;
    for (auto *r : m_rows) r->setDeviceConnected(connected);
}

void SignalsMonitorWidget::relayoutColumns() {
    if (m_rows.isEmpty()) return;
    int wName = 0, wType = 0, wValue = 0, wModify = 0;
    for (auto *r : m_rows) {
        auto h = r->columnSizeHints();
        wName   = qMax(wName,   h.name);
        wType   = qMax(wType,   h.type);
        wValue  = qMax(wValue,  h.value);
        wModify = qMax(wModify, h.modify);
    }
    for (auto *r : m_rows) {
        r->applyColumnWidths(wName, wType, wValue, wModify);
    }
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *it = m_list->item(i);
        QWidget *w = m_list->itemWidget(it);
        if (w) it->setSizeHint(w->sizeHint());
    }
}

void SignalsMonitorWidget::openModifyDialog(RowWidget *row) {
    if (!row) return;
    ModifyValueDialog dlg(row->type(), row->displayName(),
                          row->currentValueForModify(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    emit requestWriteValue(row->internalName(), dlg.value());
}

#include "signals_monitor_widget.moc"
