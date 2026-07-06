#include "camera_mapping_widget.h"

#include "core/utils/theme_manager.h"

#include <QFile>
#include <QMouseEvent>
#include <QSet>
#include <algorithm>

namespace {
constexpr int kRowHeight = 40;
constexpr const char *kAddRowToken = "__cmw_add_row__";  // sentinel, never visible

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

void EditableComboWidget::setText(const QString &text) {
    m_label->setText(text);
}

QString EditableComboWidget::text() const {
    return m_label->text();
}

QString EditableComboWidget::userData() const {
    return m_userData.isEmpty() ? m_label->text() : m_userData;
}

void EditableComboWidget::setUserData(const QString &value) {
    m_userData = value;
}

void EditableComboWidget::setOptions(const QStringList &options,
                                     const QString &currentSelection) {
    setOptions(options, options, currentSelection);
}

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
AddRowWidget::AddRowWidget(QWidget *parent) : QStackedWidget(parent) {
    setObjectName(QStringLiteral("cmwAddRow"));
    addWidget(createAddButtonPage());
    addWidget(createSelectCameraPage());
    setCurrentIndex(0);
}

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
CameraMappingWidget::CameraMappingWidget(QWidget *parent) : FlatListWidget(parent) {
    setObjectName(QStringLiteral("cmwList"));
    setSelectionMode(QAbstractItemView::NoSelection);
    setupAddRow();

    reloadStyleSheet();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](const QString &, bool) { reloadStyleSheet(); });
}

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

QString CameraMappingWidget::displayFor(const QString &id) const {
    auto it = m_camIdToName.constFind(id);
    return it != m_camIdToName.constEnd() ? it.value() : id;
}

QStringList CameraMappingWidget::displaysFor(const QStringList &ids) const {
    QStringList out;
    out.reserve(ids.size());
    for (const QString &id : ids) out << displayFor(id);
    return out;
}

void CameraMappingWidget::setCameraList(const QStringList &cameras) {
    m_allCameras = cameras;
    m_camIdToName.clear();             // legacy: id == display
    onDataChanged();
}

void CameraMappingWidget::setCameraOptions(const QMap<QString, QString> &idToName) {
    m_allCameras = idToName.keys();
    m_camIdToName = idToName;
    onDataChanged();
}

void CameraMappingWidget::setNumberLimit(int limit) {
    m_limitNumber = limit > 0 ? limit : 8;
}

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

void CameraMappingWidget::onRowAdded(const QString &cameraId) {
    int autoNumber = getSmallestAvailableNumber();
    if (autoNumber > m_limitNumber) return;

    addMappingRow(cameraId, autoNumber);
    onDataChanged();
}

void CameraMappingWidget::onRowDeleted(QListWidgetItem *item) {
    int r = row(item);
    delete takeItem(r);
    onDataChanged();
}

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

void CameraMappingWidget::onDataChanged() {
    updateSortingData();
    sortItems(Qt::AscendingOrder);

    const QStringList availableIds = getAvailableCameraIds();
    m_addRowWidget->setAvailableCameras(displaysFor(availableIds), availableIds);
    applyDuplicateWarnings();

    emit mappingChanged(getCurrentMapping());
}

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

QStringList CameraMappingWidget::getAvailableCameraIds() const {
    QStringList available = m_allCameras;
    QMap<int, QString> currentMap = getCurrentMapping();
    for (auto it = currentMap.constBegin(); it != currentMap.constEnd(); ++it) {
        available.removeOne(it.value());
    }
    return available;
}

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

int CameraMappingWidget::getSmallestAvailableNumber() const {
    QList<int> available = getAvailableNumbers();
    if (available.isEmpty()) return m_limitNumber + 1;
    std::sort(available.begin(), available.end());
    return available.first();
}
