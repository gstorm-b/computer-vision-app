#include "camera_mapping_widget.h"

#include "core/utils/theme_manager.h"

#include <QFile>
#include <QMouseEvent>
#include <QSet>
#include <algorithm>

namespace {
constexpr int kRowHeight = 40;  ///< Fixed pixel height applied to every list row's size hint.
constexpr const char *kAddRowToken = "__cmw_add_row__";  ///< Sentinel token; never shown in the UI.

/// Forces `widget` to re-evaluate its stylesheet (unpolish + polish + update); used to refresh
/// dynamic-property-based QSS selectors (e.g. the duplicate-camera warning styling) after a
/// property changes.
/// @param widget target widget; no-op when null
void repolish(QWidget *widget)
{
    if (!widget)
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
} // namespace

// ==========================================
// EditableComboWidget Implementation
// ==========================================
/// Constructs the stacked label/combo pair: builds the label and combo pages, shows the label
/// by default, and wires the combo's activation to commit the selected value, update the
/// label text, and switch back to the label view.
EditableComboWidget::EditableComboWidget(QWidget *parent) : QStackedWidget(parent) {
    setObjectName(QStringLiteral("cmwEditableWrap"));

    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("cmwEditableLabel"));
    m_label->installEventFilter(this);

    m_comboBox = new QComboBox(this);
    m_comboBox->setObjectName(QStringLiteral("cmwEditableCombo"));

    addWidget(m_label);
    addWidget(m_comboBox);
    setCurrentWidget(m_label);

    connect(m_comboBox, &QComboBox::activated, this, [this](int index) {
        if (index >= 0) {
            const QString display = m_comboBox->itemText(index);
            const QString value   = m_comboBox->itemData(index).toString();
            setText(display);
            m_userData = value.isEmpty() ? display : value;
            emit valueChanged(m_userData);
        }
        setCurrentWidget(m_label);
    });

    m_comboBox->installEventFilter(this);
}

/// Intercepts clicks on the label to switch to combo-edit mode and open the popup, and hides
/// the combo behind the label again as soon as it gains focus (the combo is a popup host only,
/// never shown as the visible page).
/// @param watched either m_label or m_comboBox
/// @param event the event being filtered
/// @return true when the event was consumed (label click); otherwise defers to the base class
bool EditableComboWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_label && event->type() == QEvent::MouseButtonPress) {
        emit editRequested();
        setCurrentWidget(m_comboBox);
        m_comboBox->showPopup();
        return true;
    }
    if (watched == m_comboBox && event->type() == QEvent::FocusIn) {
        // Combobox is purely a popup host — its visual rep stays hidden.
        setCurrentWidget(m_label);
    }
    return QStackedWidget::eventFilter(watched, event);
}

/// Sets the visible label text (does not affect userData()).
void EditableComboWidget::setText(const QString &text) {
    m_label->setText(text);
}

/// Returns the current label text.
QString EditableComboWidget::text() const {
    return m_label->text();
}

/// Returns the stored value identifier, falling back to the label text when no explicit
/// userData has been set.
QString EditableComboWidget::userData() const {
    return m_userData.isEmpty() ? m_label->text() : m_userData;
}

/// Overwrites the stored value identifier without touching the visible label text.
void EditableComboWidget::setUserData(const QString &value) {
    m_userData = value;
}

/// Legacy overload where each option's display text and stored value are the same string;
/// forwards to the (displayNames, values, currentValue) overload.
void EditableComboWidget::setOptions(const QStringList &options,
                                     const QString &currentSelection) {
    setOptions(options, options, currentSelection);
}

/// Repopulates the combo box from parallel `displayNames`/`values` arrays (i-th display name
/// paired with the i-th value) and selects the entry whose value matches `currentValue`; combo
/// signals are blocked while rebuilding so no valueChanged() fires. Extra entries beyond the
/// shorter of the two lists are ignored.
/// @param displayNames text shown for each combo entry
/// @param values underlying value returned by userData() when that entry is chosen
/// @param currentValue value to pre-select; no selection change if not found among `values`
void EditableComboWidget::setOptions(const QStringList &displayNames,
                                     const QStringList &values,
                                     const QString &currentValue) {
    m_comboBox->blockSignals(true);
    m_comboBox->clear();
    const int n = qMin(displayNames.size(), values.size());
    for (int i = 0; i < n; ++i) {
        m_comboBox->addItem(displayNames[i], values[i]);
    }
    int idx = -1;
    for (int i = 0; i < n; ++i) {
        if (values[i] == currentValue) { idx = i; break; }
    }
    if (idx >= 0) m_comboBox->setCurrentIndex(idx);
    m_comboBox->blockSignals(false);
}

// ==========================================
// CameraRowWidget Implementation
// ==========================================
/// Builds one mapping row: "Camera:" + name combo, "Number:" + number combo, and a Delete
/// button, laid out horizontally.
CameraRowWidget::CameraRowWidget(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("cmwRow"));

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    nameWidget   = new EditableComboWidget(this);
    numberWidget = new EditableComboWidget(this);

    btnDelete = new QPushButton(tr("Delete"), this);
    btnDelete->setObjectName(QStringLiteral("cmwDeleteBtn"));

    auto *lbCam = new QLabel(tr("Camera:"), this);
    lbCam->setObjectName(QStringLiteral("cmwRowFieldLabel"));
    auto *lbNum = new QLabel(tr("Number:"), this);
    lbNum->setObjectName(QStringLiteral("cmwRowFieldLabel"));

    layout->addWidget(lbCam);
    layout->addWidget(nameWidget, 1);
    layout->addWidget(lbNum);
    layout->addWidget(numberWidget, 1);
    layout->addWidget(btnDelete);
}

// ==========================================
// AddRowWidget Implementation
// ==========================================
/// Constructs the two-page Add row (the "+ Add New Row" button page and the camera-selection
/// page) and starts on the button page.
AddRowWidget::AddRowWidget(QWidget *parent) : QStackedWidget(parent) {
    setObjectName(QStringLiteral("cmwAddRow"));
    addWidget(createAddButtonPage());
    addWidget(createSelectCameraPage());
    setCurrentIndex(0);
}

/// Builds the first page: the "+ Add New Row" button, which switches to the
/// camera-selection page (index 1) when clicked.
/// @return the constructed page widget (also stored via btnAddRow)
QWidget* AddRowWidget::createAddButtonPage() {
    QWidget *w = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    btnAddRow = new QPushButton(tr("+ Add New Row"), this);
    btnAddRow->setObjectName(QStringLiteral("cmwAddRowBtn"));
    layout->addWidget(btnAddRow);

    connect(btnAddRow, &QPushButton::clicked, this, [this]() {
        setCurrentIndex(1);
    });
    return w;
}

/// Builds the second page: the camera-selection combo plus a Cancel button. Cancel returns to
/// the button page; choosing a real camera (index > 0, skipping the placeholder hint) emits
/// addRequested() with that camera's id and returns to the button page.
/// @return the constructed page widget
QWidget* AddRowWidget::createSelectCameraPage() {
    QWidget *w = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    cbCameraSelect = new QComboBox(this);
    cbCameraSelect->setObjectName(QStringLiteral("cmwAddSelectCombo"));

    btnCancel = new QPushButton(tr("Cancel"), this);
    btnCancel->setObjectName(QStringLiteral("cmwCancelBtn"));

    layout->addWidget(cbCameraSelect, 1);
    layout->addWidget(btnCancel);

    connect(btnCancel, &QPushButton::clicked, this, [this]() {
        setCurrentIndex(0);
    });

    connect(cbCameraSelect, &QComboBox::activated, this, [this](int index) {
        // Index 0 is the placeholder hint, ignore.
        if (index > 0) {
            const QString id = cbCameraSelect->itemData(index).toString();
            emit addRequested(id);
            setCurrentIndex(0);
        }
    });

    return w;
}

/// Repopulates the camera-selection combo with a leading placeholder hint followed by the
/// parallel `displayNames`/`ids` entries (combo signals blocked while rebuilding), and
/// disables/tooltips the Add button when no cameras are available to add.
void AddRowWidget::setAvailableCameras(const QStringList &displayNames,
                                       const QStringList &ids) {
    cbCameraSelect->blockSignals(true);
    cbCameraSelect->clear();
    cbCameraSelect->addItem(tr("-- Select Camera to Add --"));
    const int n = qMin(displayNames.size(), ids.size());
    for (int i = 0; i < n; ++i) {
        cbCameraSelect->addItem(displayNames[i], ids[i]);
    }
    cbCameraSelect->setCurrentIndex(0);
    cbCameraSelect->blockSignals(false);

    btnAddRow->setEnabled(n > 0);
    btnAddRow->setToolTip(n > 0 ? QString()
                                : tr("No more cameras available to map."));
}

// ==========================================
// CameraMappingWidget Implementation
// ==========================================
/// Constructs the list widget: disables item selection, adds the trailing Add row, loads the
/// theme-appropriate stylesheet, and re-loads it whenever ThemeManager::themeChanged fires.
CameraMappingWidget::CameraMappingWidget(QWidget *parent) : FlatListWidget(parent) {
    setObjectName(QStringLiteral("cmwList"));
    setSelectionMode(QAbstractItemView::NoSelection);
    setupAddRow();

    reloadStyleSheet();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &, bool) { reloadStyleSheet(); });
}

/// Loads the light or dark QSS resource (per ThemeManager::isDark()), resolves its theme
/// tokens, and applies it as this widget's stylesheet.
void CameraMappingWidget::reloadStyleSheet() {
    const QString path = ThemeManager::instance()->isDark()
        ? QStringLiteral(":/styles/camera_mapping_widget_dark.qss")
        : QStringLiteral(":/styles/camera_mapping_widget_light.qss");
    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        setStyleSheet(ThemeManager::instance()->resolveTokens(
            QString::fromUtf8(f.readAll())));
    }
}

/// Returns the display name registered for camera `id` via setCameraOptions(), or `id` itself
/// when no mapping is registered (legacy id-as-display mode).
QString CameraMappingWidget::displayFor(const QString &id) const {
    auto it = m_camIdToName.constFind(id);
    return it != m_camIdToName.constEnd() ? it.value() : id;
}

/// Maps each entry of `ids` through displayFor(), preserving order.
QStringList CameraMappingWidget::displaysFor(const QStringList &ids) const {
    QStringList out;
    out.reserve(ids.size());
    for (const QString &id : ids) out << displayFor(id);
    return out;
}

/// Legacy entry point: sets the full candidate camera list using each string as both id and
/// display name (clears any id->name mapping), then refreshes derived UI state.
/// @param cameras camera identifiers, used verbatim as display labels too
void CameraMappingWidget::setCameraList(const QStringList &cameras) {
    m_allCameras = cameras;
    m_camIdToName.clear();             // legacy: id == display
    onDataChanged();
}

/// Preferred entry point: sets the full candidate camera list from an id->displayName map, so
/// the UI can show a friendly label while the mapping is keyed by stable id.
/// @param idToName camera id mapped to its display name
void CameraMappingWidget::setCameraOptions(const QMap<QString, QString> &idToName) {
    m_allCameras = idToName.keys();
    m_camIdToName = idToName;
    onDataChanged();
}

/// Sets the maximum assignable camera number; non-positive values fall back to 8.
void CameraMappingWidget::setNumberLimit(int limit) {
    m_limitNumber = limit > 0 ? limit : 8;
}

/// Creates the trailing "Add row" list item (sorted last via an INT_MAX sentinel key) hosting
/// an AddRowWidget, and connects its addRequested() signal to onRowAdded().
void CameraMappingWidget::setupAddRow() {
    m_addRowItem = new SortableCameraItem(this);
    // Sentinel sort key so the Add row stays at the bottom.
    m_addRowItem->setData(Qt::UserRole, INT_MAX);

    m_addRowWidget = new AddRowWidget(this);
    m_addRowItem->setSizeHint(QSize(0, kRowHeight));
    setItemWidget(m_addRowItem, m_addRowWidget);

    connect(m_addRowWidget, &AddRowWidget::addRequested,
            this, &CameraMappingWidget::onRowAdded);
}

/// Connects one mapping row's signals: Delete removes `item` via onRowDeleted(); each
/// EditableComboWidget's editRequested() populates its dropdown options on demand
/// (provideNameOptions()/provideNumberOptions()) and its valueChanged() triggers
/// onDataChanged().
void CameraMappingWidget::wireRow(CameraRowWidget *row, SortableCameraItem *item) {
    connect(row->btnDelete, &QPushButton::clicked, this, [this, item]() {
        onRowDeleted(item);
    });
    connect(row->nameWidget, &EditableComboWidget::editRequested, this, [this, row]() {
        provideNameOptions(row->nameWidget);
    });
    connect(row->nameWidget, &EditableComboWidget::valueChanged,
            this, &CameraMappingWidget::onDataChanged);
    connect(row->numberWidget, &EditableComboWidget::editRequested, this, [this, row]() {
        provideNumberOptions(row->numberWidget);
    });
    connect(row->numberWidget, &EditableComboWidget::valueChanged,
            this, &CameraMappingWidget::onDataChanged);
}

/// Creates and inserts one mapping row for `cameraId`/`number` directly above the trailing Add
/// row, pre-filling both combo widgets' text and userData and wiring its signals.
void CameraMappingWidget::addMappingRow(const QString &cameraId, int number) {
    SortableCameraItem *newItem = new SortableCameraItem();
    newItem->setSizeHint(QSize(0, kRowHeight));

    CameraRowWidget *row = new CameraRowWidget(this);
    row->nameWidget->setUserData(cameraId);
    row->nameWidget->setText(displayFor(cameraId));
    row->numberWidget->setUserData(QString::number(number));
    row->numberWidget->setText(QString::number(number));

    wireRow(row, newItem);

    // Always insert above the Add row (currently at count()-1).
    insertItem(count() - 1, newItem);
    setItemWidget(newItem, row);
}

/// Slot for AddRowWidget::addRequested(): assigns `cameraId` the smallest available number and
/// inserts a new row for it, unless even the smallest available number would exceed
/// m_limitNumber (in which case nothing is added).
void CameraMappingWidget::onRowAdded(const QString &cameraId) {
    int autoNumber = getSmallestAvailableNumber();
    if (autoNumber > m_limitNumber) return;

    addMappingRow(cameraId, autoNumber);
    onDataChanged();
}

/// Removes and deletes the row at `item`'s position, then refreshes derived UI state.
void CameraMappingWidget::onRowDeleted(QListWidgetItem *item) {
    int r = row(item);
    delete takeItem(r);
    onDataChanged();
}

/// Refreshes each row's Qt::UserRole sort key from its current number combo value, so
/// sortItems() orders rows numerically (the Add row is skipped and keeps its INT_MAX key).
void CameraMappingWidget::updateSortingData() {
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *item = this->item(i);
        if (item == m_addRowItem) continue;

        CameraRowWidget *rowWidget = qobject_cast<CameraRowWidget*>(itemWidget(item));
        if (rowWidget) {
            int num = rowWidget->numberWidget->userData().toInt();
            item->setData(Qt::UserRole, num);
        }
    }
}

/// Re-sorts rows by their current numbers, refreshes the Add row's list of still-available
/// cameras, re-applies duplicate-camera warnings, and emits mappingChanged() with the latest
/// mapping. Called after any edit that could change the mapping.
void CameraMappingWidget::onDataChanged() {
    updateSortingData();
    sortItems(Qt::AscendingOrder);

    const QStringList availableIds = getAvailableCameraIds();
    m_addRowWidget->setAvailableCameras(displaysFor(availableIds), availableIds);
    applyDuplicateWarnings();

    emit mappingChanged(getCurrentMapping());
}

/// Builds the camera-name dropdown options for `widget`: available (unmapped) camera ids plus
/// the row's own current selection (so it isn't lost), ordered to match m_allCameras.
/// @param widget the row's name combo being populated just before it opens its popup
void CameraMappingWidget::provideNameOptions(EditableComboWidget *widget) {
    QStringList availableIds = getAvailableCameraIds();
    const QString currentId  = widget->userData();
    if (!currentId.isEmpty() && !availableIds.contains(currentId)) {
        availableIds.append(currentId);
    }

    // Keep order stable with m_allCameras.
    QStringList orderedIds;
    for (const QString &id : m_allCameras) {
        if (availableIds.contains(id)) orderedIds.append(id);
    }
    if (!currentId.isEmpty() && !orderedIds.contains(currentId)) {
        orderedIds.append(currentId);
    }

    widget->setOptions(displaysFor(orderedIds), orderedIds, currentId);
}

/// Builds the number dropdown options for `widget`: available (unused) numbers up to
/// m_limitNumber plus the row's own current number (so it isn't lost), sorted ascending.
/// @param widget the row's number combo being populated just before it opens its popup
void CameraMappingWidget::provideNumberOptions(EditableComboWidget *widget) {
    QList<int> availableNums = getAvailableNumbers();
    int currentNum = widget->userData().toInt();
    if (currentNum > 0 && !availableNums.contains(currentNum)) {
        availableNums.append(currentNum);
    }
    std::sort(availableNums.begin(), availableNums.end());

    QStringList options;
    for (int n : availableNums) options.append(QString::number(n));
    widget->setOptions(options, QString::number(currentNum));
}

/// Reads the current number->cameraId mapping directly off the row widgets (skipping the Add
/// row), omitting any row with an empty camera id or a non-positive number.
/// @return the assembled mapping, keyed by camera number
QMap<int, QString> CameraMappingWidget::getCurrentMapping() const {
    QMap<int, QString> map;
    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *item = this->item(i);
        if (item == m_addRowItem) continue;

        CameraRowWidget *rowWidget = qobject_cast<CameraRowWidget*>(itemWidget(item));
        if (rowWidget) {
            QString id = rowWidget->nameWidget->userData();
            int num    = rowWidget->numberWidget->userData().toInt();
            if (!id.isEmpty() && num > 0) {
                map.insert(num, id);
            }
        }
    }
    return map;
}

/// Replaces all mapping rows with ones built from `mapping` (number -> cameraId), rebuilding
/// them in map key order; signals are blocked during the rebuild and onDataChanged() is called
/// once at the end.
void CameraMappingWidget::setCurrentMapping(const QMap<int, QString> &mapping) {
    this->blockSignals(true);

    while (count() > 1) {
        QListWidgetItem *item = takeItem(0);
        delete item;
    }

    for (auto it = mapping.constBegin(); it != mapping.constEnd(); ++it) {
        addMappingRow(it.value(), it.key());
    }

    this->blockSignals(false);
    onDataChanged();
}

/// Scans all mapping rows in list order and flags every row after the first occurrence of a
/// given camera id as a duplicate, applying a warning tooltip/style via setDuplicateWarning().
void CameraMappingWidget::applyDuplicateWarnings()
{
    QSet<QString> seenCameraIds;

    for (int i = 0; i < count(); ++i) {
        QListWidgetItem *item = this->item(i);
        if (item == m_addRowItem) continue;

        CameraRowWidget *rowWidget = qobject_cast<CameraRowWidget*>(itemWidget(item));
        if (!rowWidget) continue;

        const QString cameraId = rowWidget->nameWidget->userData();
        const bool duplicate = !cameraId.isEmpty() && seenCameraIds.contains(cameraId);
        const QString tooltip = duplicate
            ? tr("This camera is already mapped by an earlier row. "
                 "Choose a different camera or remove the duplicate entry.")
            : QString();

        setDuplicateWarning(rowWidget, duplicate, tooltip);

        if (!cameraId.isEmpty() && !duplicate)
            seenCameraIds.insert(cameraId);
    }
}

/// Sets or clears the "duplicateCamera" dynamic property and tooltip on `row` (and the
/// matching "mappingWarning" property on its name label), then repolishes the affected widgets
/// so the QSS duplicate-warning styling takes effect immediately.
/// @param row target row; no-op when null
/// @param on true to mark/style the row as a duplicate, false to clear the warning
/// @param tooltip tooltip text applied when `on` is true; ignored (cleared) otherwise
void CameraMappingWidget::setDuplicateWarning(CameraRowWidget *row,
                                              bool on,
                                              const QString &tooltip)
{
    if (!row)
        return;

    row->setProperty("duplicateCamera", on);
    row->setToolTip(tooltip);
    repolish(row);

    if (row->nameWidget) {
        row->nameWidget->setToolTip(tooltip);
        if (QLabel *nameLabel =
                row->nameWidget->findChild<QLabel*>(QStringLiteral("cmwEditableLabel"))) {
            nameLabel->setProperty("mappingWarning", on ? QStringLiteral("duplicate")
                                                        : QString());
            nameLabel->setToolTip(tooltip);
            repolish(nameLabel);
        }
    }
}

/// Returns the camera ids from m_allCameras that are not currently assigned to any row.
QStringList CameraMappingWidget::getAvailableCameraIds() const {
    QStringList available = m_allCameras;
    QMap<int, QString> currentMap = getCurrentMapping();
    for (auto it = currentMap.constBegin(); it != currentMap.constEnd(); ++it) {
        available.removeOne(it.value());
    }
    return available;
}

/// Returns the numbers in [1, m_limitNumber] that are not currently assigned to any row, in
/// ascending order.
QList<int> CameraMappingWidget::getAvailableNumbers() const {
    QList<int> available;
    QMap<int, QString> currentMap = getCurrentMapping();
    QSet<int> usedNumbers;
    for (auto it = currentMap.constBegin(); it != currentMap.constEnd(); ++it) {
        usedNumbers.insert(it.key());
    }

    for (int i = 1; i <= m_limitNumber; ++i) {
        if (!usedNumbers.contains(i)) {
            available.append(i);
        }
    }
    return available;
}

/// Returns the smallest unused number in [1, m_limitNumber], or m_limitNumber + 1 if every
/// number in range is already taken.
int CameraMappingWidget::getSmallestAvailableNumber() const {
    QList<int> available = getAvailableNumbers();
    if (available.isEmpty()) return m_limitNumber + 1;
    std::sort(available.begin(), available.end());
    return available.first();
}
