#include "devices_monitor_widget.h"
#include "device_row_delegate.h"
#include "core/utils/theme_manager.h"

#include <QFile>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QToolTip>
#include <QVBoxLayout>

/// DevicesMonitorWidget implementation: builds the header/table UI, applies the
/// active theme's stylesheet, and maps live PLC polling updates (bit/word values)
/// and user filter/edit actions onto the table rows painted by DeviceRowDelegate.
namespace vc::widgets {


/// Constructs the widget for the given register kind, builds the header/table
/// UI, and applies the current theme's stylesheet; re-applies it whenever
/// ThemeManager reports a theme change.
DevicesMonitorWidget::DevicesMonitorWidget(Mode mode, QWidget *parent)
    : QWidget(parent), m_mode(mode) {
    setupUi();
    reloadStyleSheet();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &, bool) { reloadStyleSheet(); });
}

/// Uses the compiler-generated destructor; Qt parent/child ownership tears
/// down all child widgets.
DevicesMonitorWidget::~DevicesMonitorWidget() = default;

/// Builds the header bar (title/subtitle labels, active-count label, and filter
/// box) and the device table (4 fixed/stretch columns, 44 px rows, no grid,
/// alternating row colors), installs DeviceRowDelegate as the item delegate, and
/// relays the delegate's bitWriteRequested/wordWriteRequested signals through to
/// this widget's own signals of the same name.
void DevicesMonitorWidget::setupUi() {
    setObjectName(QStringLiteral("DevicesMonitorWidget"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────
    m_header = new QFrame(this);
    m_header->setObjectName("dmHeader");
    m_header->setFrameShape(QFrame::NoFrame);

    auto *hLay = new QHBoxLayout(m_header);
    hLay->setContentsMargins(14, 10, 14, 10);
    hLay->setSpacing(12);

    auto *titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(2);
    titleBlock->setContentsMargins(0, 0, 0, 0);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("dmTitle"));
    m_titleLabel->setText(m_mode == Mode::Bit
                              ? tr("M DEVICES — BIT REGISTERS")
                              : tr("D DEVICES — WORD REGISTERS"));

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName(QStringLiteral("dmSubtitle"));
    m_subtitleLabel->setText(m_mode == Mode::Bit
                                 ? tr("ON/OFF flags polled live from PLC")
                                 : tr("16-bit signed values polled live from PLC"));

    titleBlock->addWidget(m_titleLabel);
    titleBlock->addWidget(m_subtitleLabel);
    hLay->addLayout(titleBlock);
    hLay->addStretch();

    m_countLabel = new QLabel(this);
    m_countLabel->setObjectName(QStringLiteral("dmCount"));
    m_countLabel->setText("0 / 0");
    hLay->addWidget(m_countLabel);

    m_search = new QLineEdit(this);
    m_search->setObjectName("dmSearch");
    m_search->setPlaceholderText(tr("Filter address or description…"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged,
            this, &DevicesMonitorWidget::onFilterTextChanged);
    hLay->addWidget(m_search);

    root->addWidget(m_header);

    // ── Table ─────────────────────────────────────────────────────────────
    m_table = new QTableWidget(this);
    m_table->setObjectName("dmTable");
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        tr("Address"),
        tr("Description"),
        m_mode == Mode::Bit ? tr("State") : tr("Value"),
        tr("Action"),
    });
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
                             | QAbstractItemView::EditKeyPressed);
    m_table->setMouseTracking(true);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(0, 96);
    m_table->setColumnWidth(2, m_mode == Mode::Bit ? 100 : 130);
    m_table->setColumnWidth(3, m_mode == Mode::Bit ? 280 : 220);
    m_table->verticalHeader()->setDefaultSectionSize(44);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // ── Delegate paints chip / buttons / value / write strip ─────────────
    m_delegate = new DeviceRowDelegate(
        m_mode == Mode::Bit ? DeviceRowDelegate::Bit
                            : DeviceRowDelegate::Word,
        m_table);
    m_table->setItemDelegate(m_delegate);

    connect(m_delegate, &DeviceRowDelegate::bitWriteRequested,
            this,        &DevicesMonitorWidget::bitWriteRequested);
    connect(m_delegate, &DeviceRowDelegate::wordWriteRequested,
            this,        &DevicesMonitorWidget::wordWriteRequested);

    root->addWidget(m_table, 1);
}

/// Reloads the light/dark QSS resource matching ThemeManager's current theme,
/// resolves its design tokens, applies it as this widget's stylesheet, and
/// forces the table viewport to repaint.
void DevicesMonitorWidget::reloadStyleSheet() {
    const QString path = ThemeManager::instance()->isDark()
        ? QStringLiteral(":/styles/devices_monitor_widget_dark.qss")
        : QStringLiteral(":/styles/devices_monitor_widget_light.qss");
    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        setStyleSheet(ThemeManager::instance()->resolveTokens(
            QString::fromUtf8(f.readAll())));
    }
    if (m_table) {
        m_table->viewport()->update();
        m_table->update();
    }
}

/// Sets the header title label text.
void DevicesMonitorWidget::setTitle(const QString &title) {
    m_titleLabel->setText(title);
}

/// Sets the header subtitle label text.
void DevicesMonitorWidget::setSubtitle(const QString &subtitle) {
    m_subtitleLabel->setText(subtitle);
}

/// Configures the contiguous device range shown and rebuilds all table rows.
/// @param start_address first device address in the range
/// @param amount number of consecutive devices in the range
void DevicesMonitorWidget::setRange(int start_address, int amount) {
    m_start  = start_address;
    m_amount = amount;
    rebuildRows();
}

/// Empties the table, resets the configured amount to 0, and refreshes the
/// active-count label.
void DevicesMonitorWidget::clearRows() {
    m_table->clearContents();
    m_table->setRowCount(0);
    m_amount = 0;
    recountActive();
}

/// Builds the display name for `address`: 'M' prefix in Bit mode or 'D' prefix
/// in Word mode, followed by the zero-padded 4-digit address.
QString DevicesMonitorWidget::formatName(int address) const {
    const QChar prefix = m_mode == Mode::Bit ? QLatin1Char('M') : QLatin1Char('D');
    return QString("%1%2").arg(prefix).arg(address, 4, 10, QChar('0'));
}

/// Converts a device address to its row index (address - m_start), or -1 if
/// the address falls outside the currently configured range.
int DevicesMonitorWidget::rowOfAddress(int address) const {
    const int row = address - m_start;
    if (row < 0 || row >= m_amount) return -1;
    return row;
}

/// Converts a row index back to its device address (m_start + row).
int DevicesMonitorWidget::addressOfRow(int row) const {
    return m_start + row;
}

/// Returns the formatted device name for every row in the currently configured
/// range, in row order.
QStringList DevicesMonitorWidget::allDeviceNames() const {
    QStringList names;
    names.reserve(m_amount);
    for (int i = 0; i < m_amount; ++i)
        names << formatName(addressOfRow(i));
    return names;
}

/// Clears and rebuilds every row for the currently configured range (m_start /
/// m_amount), fixing each row's height at 44 px, then refreshes the
/// active-count label.
void DevicesMonitorWidget::rebuildRows() {
    m_table->clearContents();
    m_table->setRowCount(m_amount);
    for (int i = 0; i < m_amount; ++i) {
        buildRow(i, addressOfRow(i));
        m_table->setRowHeight(i, 44);
    }
    recountActive();
}

/// Creates the four QTableWidgetItems for `row` at `address`: Address (non-
/// editable, carries AddressRole), Description (editable iff m_commentEditable),
/// State-or-Value (non-editable/non-selectable, carries BitStateRole in Bit mode
/// or WordValueRole in Word mode), and Action (editable only in Word mode, where
/// it also carries the pending-write value via PendingWriteRole). Actual
/// painting of these columns is done by DeviceRowDelegate.
void DevicesMonitorWidget::buildRow(int row, int address) {
    using R = DeviceRowDelegate;

    // Column 0 — Address (read-only, painted by delegate).  We still need
    // a QTableWidgetItem so the model has somewhere to store the role.
    auto *addrItem = new QTableWidgetItem;
    addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsEditable);
    addrItem->setData(R::AddressRole, address);
    m_table->setItem(row, 0, addrItem);

    // Column 1 — Description (editable text via Qt::DisplayRole).
    auto *descItem = new QTableWidgetItem;
    auto descFlags = descItem->flags() & ~Qt::ItemIsSelectable;
    if (m_commentEditable) descFlags |= Qt::ItemIsEditable;
    else                   descFlags &= ~Qt::ItemIsEditable;
    descItem->setFlags(descFlags);
    m_table->setItem(row, 1, descItem);

    // Column 2 — State (Bit) or Value (Word) — painted from role data.
    auto *stateItem = new QTableWidgetItem;
    stateItem->setFlags(stateItem->flags() & ~Qt::ItemIsEditable
                                            & ~Qt::ItemIsSelectable);
    if (m_mode == Mode::Bit) stateItem->setData(R::BitStateRole,  false);
    else                     stateItem->setData(R::WordValueRole, 0);
    m_table->setItem(row, 2, stateItem);

    // Column 3 — Action: painted by the delegate; the item only carries the
    // pending-write value for Word mode.
    auto *actItem = new QTableWidgetItem;
    Qt::ItemFlags actFlags = actItem->flags() & ~Qt::ItemIsSelectable;
    if (m_mode == Mode::Word) actFlags |=  Qt::ItemIsEditable;
    else                      actFlags &= ~Qt::ItemIsEditable;
    actItem->setFlags(actFlags);
    if (m_mode == Mode::Word) actItem->setData(R::PendingWriteRole, 0);
    m_table->setItem(row, 3, actItem);
}

/// Updates the ON/OFF chip state for `address` if it maps to a currently
/// visible row; no-op if the row is missing or the on/off value is unchanged.
/// Refreshes the active-count label when the state actually changes.
void DevicesMonitorWidget::setBitState(int address, quint8 value) {
    const int row = rowOfAddress(address);
    if (row < 0) return;
    auto *item = m_table->item(row, DeviceRowDelegate::ColState);
    if (!item) return;
    const bool on = value != 0;
    if (item->data(DeviceRowDelegate::BitStateRole).toBool() == on) return;
    item->setData(DeviceRowDelegate::BitStateRole, on);
    recountActive();
}

/// Updates the numeric value shown for `address` if it maps to a currently
/// visible row; no-op if the address is out of the configured range.
void DevicesMonitorWidget::setWordValue(int address, qint16 value) {
    const int row = rowOfAddress(address);
    if (row < 0) return;
    auto *item = m_table->item(row, DeviceRowDelegate::ColState);
    if (!item) return;
    item->setData(DeviceRowDelegate::WordValueRole, static_cast<int>(value));
}

/// Resets every row's state (Bit mode: OFF) or value (Word mode: 0) and
/// refreshes the active-count label.
void DevicesMonitorWidget::clearAllStatuses() {
    using R = DeviceRowDelegate;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto *item = m_table->item(i, R::ColState);
        if (!item) continue;
        if (m_mode == Mode::Bit) item->setData(R::BitStateRole,  false);
        else                     item->setData(R::WordValueRole, 0);
    }
    recountActive();
}

/// Enables or disables in-place editing of the Description column, applying
/// the new edit flag to every row currently in the table (later-built rows
/// pick it up from the stored m_commentEditable in buildRow).
void DevicesMonitorWidget::setCommentEditable(bool editable) {
    m_commentEditable = editable;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        if (auto *it = m_table->item(i, 1)) {
            auto flags = it->flags();
            if (editable) flags |=  Qt::ItemIsEditable;
            else          flags &= ~Qt::ItemIsEditable;
            it->setFlags(flags);
        }
    }
}

/// Returns the Description text for `address`, or an empty string if the
/// address is out of range or its Description item is missing.
QString DevicesMonitorWidget::comment(int address) const {
    const int row = rowOfAddress(address);
    if (row < 0) return QString();
    auto *it = m_table->item(row, 1);
    return it ? it->text() : QString();
}

/// Sets the Description text and matching tooltip for the row at `address`;
/// no-op if the address is out of range.
void DevicesMonitorWidget::setComment(int address, const QString &text) {
    const int row = rowOfAddress(address);
    if (row < 0) return;
    if (auto *it = m_table->item(row, 1)) {
        it->setText(text);
        it->setToolTip(text);
    }
}

/// Recomputes the header count label: in Bit mode, "active / total ACTIVE"
/// counted from rows whose BitStateRole is true; in Word mode, "total WORDS".
void DevicesMonitorWidget::recountActive() {
    if (m_mode == Mode::Bit) {
        int active = 0;
        for (int i = 0; i < m_table->rowCount(); ++i) {
            auto *item = m_table->item(i, DeviceRowDelegate::ColState);
            if (item && item->data(DeviceRowDelegate::BitStateRole).toBool())
                ++active;
        }
        m_countLabel->setText(QString("%1 / %2  %3")
                                  .arg(active).arg(m_amount).arg(tr("ACTIVE")));
    } else {
        m_countLabel->setText(QString("%1  %2")
                                  .arg(m_amount).arg(tr("WORDS")));
    }
}

/// Filters visible rows to those whose formatted address name or Description
/// text contains the trimmed `text` (case-insensitive); shows every row again
/// once the filter text is empty.
void DevicesMonitorWidget::onFilterTextChanged(const QString &text) {
    using R = DeviceRowDelegate;
    const QString needle = text.trimmed();
    for (int i = 0; i < m_table->rowCount(); ++i) {
        if (needle.isEmpty()) {
            m_table->setRowHidden(i, false);
            continue;
        }
        const int address = m_table->item(i, R::ColAddress)
                                ? m_table->item(i, R::ColAddress)
                                      ->data(R::AddressRole).toInt()
                                : -1;
        const QString name = (address >= 0) ? formatName(address) : QString();
        const QString desc = m_table->item(i, R::ColDescription)
                                 ? m_table->item(i, R::ColDescription)->text()
                                 : QString();
        const bool match = name.contains(needle, Qt::CaseInsensitive)
                           || desc.contains(needle, Qt::CaseInsensitive);
        m_table->setRowHidden(i, !match);
    }
}

} // namespace vc::widgets
