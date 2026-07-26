#ifndef CAMERAMAPPINGWIDGET_H
#define CAMERAMAPPINGWIDGET_H

#include "ui/widgets/controls/flat_list_widget.h"

#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMap>
#include <QStringList>
#include <QEvent>

/// Label that becomes an editable QComboBox when clicked ("Component 1").
/// Carries an optional "userData" string that travels separately from the
/// visible label text. Callers that need to decouple a stable identifier
/// (e.g. a device id) from the human-readable label set both via
/// setOptions(displayNames, values, currentValue) and read the underlying
/// id back via userData().
class EditableComboWidget : public QStackedWidget {
    Q_OBJECT
public:
    /// Builds the stacked label/combo-box pair, starting in label (display) mode.
    explicit EditableComboWidget(QWidget *parent = nullptr);
    /// Sets the text shown on the display label; does not affect userData().
    void setText(const QString &text);
    /// Returns the text currently shown on the display label.
    QString text() const;
    /// Returns the stored value, falling back to the label text when no value
    /// was set via setUserData() or the 3-argument setOptions().
    QString userData() const;
    /// Sets the underlying value returned by userData(), independent of the displayed label.
    void setUserData(const QString &value);

    /// Legacy overload: forwards to the 3-argument setOptions() using `options`
    /// as both the display names and the underlying values.
    void setOptions(const QStringList &options, const QString &currentSelection);
    /// Populates the combo box so the i-th entry shows displayNames[i] while storing
    /// values[i] as its hidden item data; selects the entry whose value equals
    /// currentValue (not by label).
    void setOptions(const QStringList &displayNames,
                    const QStringList &values,
                    const QString &currentValue);

signals:
    /// Emitted when the combo-box selection changes.
    /// @param newValue the underlying value (userData) of the newly selected item, not its display label
    void valueChanged(const QString &newValue);
    /// Emitted when the label is clicked, just before switching into edit (combo) mode.
    void editRequested();

protected:
    /// Switches the label to combo (and opens its popup) on a label click, and
    /// switches back to the label on the combo box gaining focus; defers to the
    /// base class implementation otherwise.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLabel *m_label;       ///< The label shown in display mode.
    QComboBox *m_comboBox; ///< The combo box shown while editing; its popup drives selection.
    QString    m_userData; ///< Cached value set via setUserData()/setOptions(); empty means "use the label text".
};

/// A single mapping row: camera-name combo, number combo, and a delete button
/// ("Component 2" — a normal row, as opposed to the trailing Add row).
class CameraRowWidget : public QWidget {
    Q_OBJECT
public:
    /// Builds the row's layout: "Camera:" label + nameWidget, "Number:" label +
    /// numberWidget, and a Delete button.
    explicit CameraRowWidget(QWidget *parent = nullptr);
    EditableComboWidget *nameWidget;   ///< Editable combo holding the camera id (userData) / display name (text) for this row.
    EditableComboWidget *numberWidget; ///< Editable combo holding the assigned slot number, stored as text/userData.
    QPushButton *btnDelete;            ///< Removes this row when clicked; wired up by the owning CameraMappingWidget.
};

/// Trailing "Add" row ("Component 3"): shows a "+ Add New Row" button that flips
/// to a camera-selection combo + Cancel button when clicked.
class AddRowWidget : public QStackedWidget {
    Q_OBJECT
public:
    /// Builds both stacked pages (add button, select-camera) and starts on the add-button page.
    explicit AddRowWidget(QWidget *parent = nullptr);
    /// Repopulates the camera-selection combo from parallel `displayNames`/`ids`
    /// arrays and disables the add button (with an explanatory tooltip) when
    /// `ids` is empty.
    /// @param displayNames labels shown in the selection combo
    /// @param ids the id emitted by addRequested() for each corresponding entry
    void setAvailableCameras(const QStringList &displayNames,
                             const QStringList &ids);

signals:
    /// Emitted with the chosen camera's id when the user picks one from the selection combo.
    void addRequested(const QString &cameraId);

private:
    /// Builds the page showing the "+ Add New Row" button; clicking it switches
    /// to the select-camera page.
    QWidget *createAddButtonPage();
    /// Builds the page with the camera-selection combo and Cancel button;
    /// selecting a camera emits addRequested() and returns to the add-button
    /// page, Cancel just returns without emitting.
    QWidget *createSelectCameraPage();

    QPushButton *btnAddRow;       ///< "+ Add New Row" button on the first stacked page.
    QComboBox *cbCameraSelect;    ///< Camera-selection combo on the second stacked page; item data holds the camera id.
    QPushButton *btnCancel;       ///< Returns to the add-button page without emitting addRequested().
};

/// List item ("Component 4") that sorts numerically by its Qt::UserRole data
/// instead of alphabetically by text, so mapping rows sort by assigned slot number.
class SortableCameraItem : public QListWidgetItem {
public:
    /// Constructs the item, optionally attaching it to `view`.
    explicit SortableCameraItem(QListWidget *view = nullptr) : QListWidgetItem(view) {}

    /// Compares items by their Qt::UserRole integer data (the slot number)
    /// rather than by display text, so the list sorts numerically.
    bool operator<(const QListWidgetItem &other) const override {
        return data(Qt::UserRole).toInt() < other.data(Qt::UserRole).toInt();
    }
};

/// Main manager widget ("Component 5"): a FlatListWidget of camera-to-number
/// mapping rows plus a trailing Add row, keeping ids unique, numbers within
/// setNumberLimit(), and the list sorted/re-themed automatically.
class CameraMappingWidget : public FlatListWidget {
    Q_OBJECT
public:
    /// Sets up the list (no selection, Add row wired in), applies the current
    /// theme's stylesheet, and reapplies it whenever the theme changes.
    explicit CameraMappingWidget(QWidget *parent = nullptr);

    /// Legacy entry point: sets the available camera ids, using each string as
    /// both the stable id and its display label. Use setCameraOptions() when
    /// those need to differ.
    void setCameraList(const QStringList &cameras);
    /// Preferred entry point: sets the available cameras from an id -> display-name
    /// map. The mapping stored/returned by this widget still uses ids; the UI
    /// shows the matching display name.
    void setCameraOptions(const QMap<QString, QString> &idToName);

    /// Sets the highest assignable slot number (falls back to 8 when `limit` is <= 0).
    void setNumberLimit(int limit);

    /// Reads the current rows into a number -> camera-id map, skipping the Add
    /// row and any row with an empty id or non-positive number.
    QMap<int, QString> getCurrentMapping() const;
    /// Replaces all rows with one per entry in `mapping` (number -> camera id),
    /// rebuilding sort/duplicate state, without emitting intermediate signals.
    void setCurrentMapping(const QMap<int, QString> &mapping);

signals:
    /// Emitted after any row add/delete/edit with the newly recomputed
    /// number -> camera-id mapping.
    void mappingChanged(const QMap<int, QString> &mapping);

private slots:
    /// Adds a new row for `cameraId` at the smallest available slot number,
    /// unless that number would exceed setNumberLimit().
    void onRowAdded(const QString &cameraId);
    /// Removes `item` from the list and refreshes derived state.
    void onRowDeleted(QListWidgetItem *item);
    /// Re-sorts the list, refreshes the Add row's available-camera list,
    /// reapplies duplicate-camera warnings, and emits mappingChanged().
    void onDataChanged();

    /// Populates `widget`'s combo with the cameras still available for
    /// selection (plus its own current id, ordered per m_allCameras) just
    /// before it opens for editing.
    void provideNameOptions(EditableComboWidget *widget);
    /// Populates `widget`'s combo with the slot numbers still available (plus
    /// its own current number) just before it opens for editing.
    void provideNumberOptions(EditableComboWidget *widget);

private:
    /// Creates the sentinel Add-row item/widget (sorted to the bottom via
    /// INT_MAX) and wires its addRequested signal.
    void setupAddRow();
    /// Inserts a new mapping row for `cameraId`/`number` directly above the
    /// Add row and wires its signals.
    void addMappingRow(const QString &cameraId, int number);
    /// Connects `row`'s delete button and both editable combos
    /// (editRequested -> populate options, valueChanged -> onDataChanged())
    /// for the list item `item`.
    void wireRow(CameraRowWidget *row, SortableCameraItem *item);
    /// Scans all rows in order and flags every row after the first with a
    /// given camera id as a duplicate via setDuplicateWarning().
    void applyDuplicateWarnings();
    /// Sets/clears the "duplicateCamera" style property and tooltip on `row`
    /// (and its name label), then repolishes them.
    void setDuplicateWarning(CameraRowWidget *row, bool on, const QString &tooltip);
    /// Returns the smallest slot number not currently in use, or
    /// setNumberLimit()+1 when all numbers up to the limit are taken.
    int getSmallestAvailableNumber() const;
    /// Returns the ids from m_allCameras not already used by an existing mapping row.
    QStringList getAvailableCameraIds() const;
    /// Returns the slot numbers from 1..limit not already used by an existing mapping row.
    QList<int> getAvailableNumbers() const;
    /// Stores each row's current slot number into its item's Qt::UserRole so
    /// SortableCameraItem::operator< sorts by it.
    void updateSortingData();
    /// Returns the display name mapped to `id` via m_camIdToName, or `id` itself when unmapped.
    QString displayFor(const QString &id) const;
    /// Maps each id in `ids` through displayFor(), preserving order.
    QStringList displaysFor(const QStringList &ids) const;
    /// Loads the dark or light QSS resource matching the current theme and
    /// applies it with theme tokens resolved.
    void reloadStyleSheet();

    QStringList m_allCameras;                  ///< All known camera ids, in the order supplied to setCameraList()/setCameraOptions().
    QMap<QString, QString> m_camIdToName;      ///< Maps camera id to display name; empty when set via the legacy setCameraList().
    int m_limitNumber = 8;                     ///< Highest assignable slot number (see setNumberLimit()).
    SortableCameraItem *m_addRowItem = nullptr;   ///< The sentinel list item hosting m_addRowWidget, sorted last via INT_MAX.
    AddRowWidget *m_addRowWidget = nullptr;       ///< The trailing "Add" row widget shown by m_addRowItem.
};

#endif // CAMERAMAPPINGWIDGET_H
