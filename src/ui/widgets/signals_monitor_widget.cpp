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

/// Internal implementation detail types for SignalsMonitorWidget: the
/// per-row widget and the "Modify value" dialog, neither of which is part
/// of the public widget API.
namespace vc::widgets::sm_internal {

namespace {

/// Re-evaluates the QSS after toggling a dynamic property. Mirrors the helper
/// in add_device_wizard.cpp (design_rules 15.3).
void repolish(QWidget *w) {
    if (!w) return;
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

constexpr int kRisingEdgeHideMs = 2000;    ///< How long the "RE" chip stays visible after a rising edge.
constexpr int kRisingEdgeBlinkMs = 500;    ///< Hide duration of the blink when a new rising edge arrives while the chip is still shown.
constexpr int kRisingEdgeWindowMs = 200;   ///< Max OFF→ON→OFF duration counted as a rising-edge pulse.

// Column hint floor — keeps a freshly added empty row from collapsing.
constexpr int kModifyBtnWidth = 32;        ///< Minimum width reserved for the Modify column.
constexpr int kRowHMargin = 8;             ///< Row layout horizontal content margin, in pixels.
constexpr int kRowVMargin = 4;             ///< Row layout vertical content margin, in pixels.
constexpr int kRowSpacing = 8;             ///< Spacing between the name/type/value/modify cells.
constexpr int kValueInnerSpacing = 6;      ///< Spacing between the stacked value-cell widgets.

} // namespace

/// One entry in the monitor list: shows a signal's display name, type chip,
/// current value (ON/OFF pill + rising-edge chip for Bool, numeric label for
/// Number, or "(not mapped)" when unmapped), and a Modify button.
class RowWidget : public QFrame {
    Q_OBJECT
public:
    using Type = SignalsMonitorWidget::Type;

    /// Builds the row's layout (name label, type chip, value cell holding
    /// all 3 possible value representations, modify button) and the
    /// rising-edge hide/blink timers; renders in the initial unmapped state.
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
    /// Returns the row's internalName (its unique key).
    QString internalName() const { return m_internalName; }
    /// Returns the user-facing display name shown in the name column.
    QString displayName()  const { return m_displayName; }
    /// Returns the row's value kind (Number or Bool).
    Type    type()         const { return m_type; }
    /// Returns the currently bound PLC tag, or empty if unmapped.
    QString tag()          const { return m_tag; }

    /// Rebinds the row to `tag` (empty clears the mapping): updates which
    /// value widgets are visible and whether the Modify button is enabled.
    void setTag(const QString &tag) {
        m_tag = tag;
        applyValueVisibility();
        updateModifyEnabled();
    }

    /// Updates the row's device-connected state, which gates the Modify
    /// button, and refreshes the button's tooltip to explain why it is
    /// disabled when the device is not connected.
    void setDeviceConnected(bool connected) {
        m_deviceConnected = connected;
        updateModifyEnabled();
        // Keep tooltip informative when disabled-due-to-disconnect.
        m_btnModify->setToolTip(m_deviceConnected
                                    ? tr("Modify value")
                                    : tr("Device disconnected"));
    }

    /// Updates the ON/OFF pill for a Bool row and detects a rising-edge
    /// pulse: if the value transitions ON→OFF within kRisingEdgeWindowMs of
    /// having transitioned OFF→ON, triggerRisingEdge() is invoked. The first
    /// value received after construction is recorded but never treated as a
    /// transition.
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

    /// Updates the numeric value label for a Number row.
    void setNumberValue(int v) {
        m_hasNum = true;
        m_lastNum = v;
        m_lblNumber->setText(QString::number(v));
    }

    // Snapshot of last value — used to seed the Modify dialog.
    /// Returns the last known value as a QVariant (bool or int, per type),
    /// used to seed the Modify dialog. Falls back to false/0 if no value
    /// has been received yet.
    QVariant currentValueForModify() const {
        if (m_type == Type::Bool) {
            return m_hasBool ? QVariant(m_lastBool) : QVariant(false);
        }
        return m_hasNum ? QVariant(m_lastNum) : QVariant(0);
    }

    // ── Column width contract ──────────────────────────────────────────
    /// Column width hints (in pixels) for one row: name, type chip, value
    /// cell, and modify button.
    struct ColumnHints { int name, type, value, modify; };
    /// Computes this row's natural column widths, taking the max size hint
    /// across every candidate value widget (ON/OFF pill + rising-edge chip,
    /// number label, "(not mapped)" label) so the value column never
    /// jitters as the row's mapped/value state changes.
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

    /// Applies externally-computed column widths (the max across all rows)
    /// to this row's name/type/value/modify widgets so columns align.
    void applyColumnWidths(int name, int type, int value, int modify) {
        m_lblName->setFixedWidth(name);
        m_lblType->setFixedWidth(type);
        m_valueCell->setFixedWidth(value);
        m_btnModify->setFixedWidth(modify);
    }

signals:
    /// Emitted when the user clicks the row's Modify button.
    void modifyRequested();

private:
    /// Shows/hides the value-cell widgets for the current tag/type state:
    /// "(not mapped)" when m_tag is empty, otherwise the ON/OFF pill for
    /// Bool rows or the numeric label for Number rows. Leaves the
    /// rising-edge chip's visibility to the RE state machine.
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

    /// Enables the Modify button only when the row is mapped to a tag and
    /// the device is connected.
    void updateModifyEnabled() {
        m_btnModify->setEnabled(!m_tag.isEmpty() && m_deviceConnected);
    }

    /// Shows the "RE" rising-edge chip; if it is already visible, blinks it
    /// (hide for kRisingEdgeBlinkMs, then show again and restart the
    /// kRisingEdgeHideMs auto-hide), otherwise shows it and starts the
    /// auto-hide timer.
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

    QString m_internalName; ///< Unique row key (matches the task-config field).
    QString m_displayName;  ///< User-facing label shown in the name column.
    Type    m_type;         ///< Number or Bool; selects which value widget is shown.
    QString m_tag;          ///< Bound PLC tag; empty means unmapped.

    bool m_deviceConnected{false}; ///< Gates the Modify button.

    QLabel      *m_lblName{nullptr};
    QLabel      *m_lblType{nullptr};
    QWidget     *m_valueCell{nullptr};      ///< Container holding all 3 value representations, stacked.
    QLabel      *m_lblOnOff{nullptr};
    QLabel      *m_lblRising{nullptr};      ///< "RE" rising-edge chip; visibility owned by the RE state machine.
    QLabel      *m_lblNumber{nullptr};
    QLabel      *m_lblUnmapped{nullptr};
    QToolButton *m_btnModify{nullptr};

    bool         m_hasBool{false};   ///< True once a Bool value has been received (guards the "first value" case).
    bool         m_lastBool{false};  ///< Last Bool value received.
    QElapsedTimer m_tRising;         ///< Measures the OFF→ON→OFF interval for rising-edge detection.
    QTimer      *m_hideReTimer{nullptr};  ///< Auto-hides the RE chip after kRisingEdgeHideMs.
    QTimer      *m_blinkReTimer{nullptr}; ///< Re-shows the RE chip after kRisingEdgeBlinkMs during a blink.
    bool         m_reVisible{false};      ///< Tracks whether the RE chip is currently shown (drives blink vs. show).

    bool         m_hasNum{false};  ///< True once a Number value has been received.
    int          m_lastNum{0};     ///< Last Number value received.
};

/// Modal dialog that gathers a new value from the user for the Modify
/// action: a Bool row gets an ON/OFF radio choice, a Number row gets a
/// signed/unsigned int16 interpretation combo backed by a stacked spinbox.
class ModifyValueDialog : public QDialog {
    Q_OBJECT
public:
    using Type = SignalsMonitorWidget::Type;

    /// Builds the dialog for `type`, seeded with `initial`, titled with
    /// `displayName`, plus the OK/Cancel button box.
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

    /// Returns the value the user chose: the ON radio state for Bool, or the
    /// active page's spinbox value (signed or unsigned int16) for Number.
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
    /// Adds ON/OFF radio buttons to `root`, pre-checking the one matching
    /// `initial`.
    void buildBool(QVBoxLayout *root, bool initial) {
        m_rbOn  = new QRadioButton(tr("ON"),  this);
        m_rbOff = new QRadioButton(tr("OFF"), this);
        (initial ? m_rbOn : m_rbOff)->setChecked(true);
        root->addWidget(m_rbOn);
        root->addWidget(m_rbOff);
    }

    /// Adds the signed/unsigned int16 interpretation combo and its stacked
    /// spinbox pages to `root`, defaulting to the unsigned page only when
    /// `initial` cannot fit the signed int16 range; wires the combo so
    /// switching pages carries the current value over, clamped to the
    /// destination range.
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

    Type m_type;   ///< Which of the Bool/Number widget sets is active.
    QRadioButton *m_rbOn{nullptr};
    QRadioButton *m_rbOff{nullptr};
    QComboBox    *m_cbInterp{nullptr};      ///< Signed/unsigned int16 interpretation picker.
    QStackedWidget *m_stack{nullptr};       ///< Swaps between the signed and unsigned spinbox pages.
    QSpinBox     *m_spSigned{nullptr};      ///< Signed int16 page (-32768..32767).
    QSpinBox     *m_spUnsigned{nullptr};    ///< Unsigned int16 page (0..65535).
};

} // namespace vc::widgets::sm_internal

using vc::widgets::sm_internal::RowWidget;
using vc::widgets::sm_internal::ModifyValueDialog;

// =====================================================================
// SignalsMonitorWidget
// =====================================================================

/// Builds the widget's layout: a single FlatListWidget (no selection, no
/// keyboard focus, per-pixel vertical scrolling) that will hold one
/// RowWidget per signal, loads the theme-appropriate stylesheet, and
/// reloads it whenever ThemeManager reports a theme change.
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

/// Default destructor; RowWidget children are owned/destroyed via m_list.
SignalsMonitorWidget::~SignalsMonitorWidget() = default;

/// Loads the dark or light QSS resource (per ThemeManager::isDark()),
/// resolves its design-token placeholders, and applies it as the widget's
/// stylesheet. No-op if the resource file cannot be opened.
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

/// Returns the index into m_rows whose RowWidget::internalName() matches, or
/// -1 if not found.
int SignalsMonitorWidget::rowIndexOf(const QString &internalName) const {
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i]->internalName() == internalName) return i;
    }
    return -1;
}

/// Appends a new row for `internalName` at the end of the list.
/// @note internalName is the unique row key; this is a no-op (with a
/// logged warning) if a row for internalName already exists.
void SignalsMonitorWidget::appendRow(const QString &internalName,
                                     const QString &displayName,
                                     Type type) {
    insertRowAt(m_rows.size(), internalName, displayName, type);
}

/// Inserts a new RowWidget for `internalName` at `row` (clamped to the
/// valid range), seeded with the widget's current device-connected state,
/// and re-runs relayoutColumns() to account for the new row.
/// @note internalName is the unique row key; this is a no-op (with a
/// logged warning) if a row for internalName already exists.
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

/// Removes the row identified by `internalName`, if present, and reflows
/// column widths for the remaining rows.
void SignalsMonitorWidget::removeRow(const QString &internalName) {
    int row = rowIndexOf(internalName);
    if (row < 0) return;
    m_rows.removeAt(row);
    delete m_list->takeItem(row);   // takeItem returns ownership
    relayoutColumns();
}

/// Removes every row from the list.
void SignalsMonitorWidget::clearRows() {
    m_rows.clear();
    m_list->clear();
}

/// Binds the row identified by `internalName` to `tag` (empty means
/// unmapped) and reflows column widths, since the value cell's visible
/// content changes.
void SignalsMonitorWidget::setRowTag(const QString &internalName,
                                     const QString &tag) {
    int row = rowIndexOf(internalName);
    if (row < 0) return;
    m_rows[row]->setTag(tag);
    relayoutColumns();
}

/// Returns the tag currently bound to the row identified by `internalName`,
/// or an empty string if the row does not exist.
QString SignalsMonitorWidget::rowTag(const QString &internalName) const {
    int row = rowIndexOf(internalName);
    return row < 0 ? QString() : m_rows[row]->tag();
}

/// Pushes a fresh Bool value to the row identified by `internalName`.
/// Logs a warning and does nothing if the signal is unknown or is not a
/// Bool-type row.
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

/// Pushes a fresh Number value to the row identified by `internalName` and
/// reflows column widths (the number's digit count may have changed).
/// Logs a warning and does nothing if the signal is unknown or is not a
/// Number-type row.
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

/// Propagates the device-connected state to every row (gates each row's
/// Modify button); last-received values are left untouched.
void SignalsMonitorWidget::setDeviceConnected(bool connected) {
    m_deviceConnected = connected;
    for (auto *r : m_rows) r->setDeviceConnected(connected);
}

/// Recomputes the name/type/value/modify column widths as the max size hint
/// across every row, applies them to every row so columns visually align,
/// and refreshes each list item's size hint to match its row widget.
/// No-op when there are no rows.
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

/// Opens the modal ModifyValueDialog seeded with `row`'s current value; on
/// acceptance, emits requestWriteValue() with the row's internalName and the
/// chosen value (the widget does not apply the value itself). No-op if
/// `row` is null or the dialog is cancelled.
void SignalsMonitorWidget::openModifyDialog(RowWidget *row) {
    if (!row) return;
    ModifyValueDialog dlg(row->type(), row->displayName(),
                          row->currentValueForModify(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    emit requestWriteValue(row->internalName(), dlg.value());
}

#include "signals_monitor_widget.moc"
