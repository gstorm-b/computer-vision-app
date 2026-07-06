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

// --- Component 1: Label có thể click để chuyển thành ComboBox ---
//
// Carries an optional "userData" string that travels separately from the
// visible label text. Callers that need to decouple a stable identifier
// (e.g. a device id) from the human-readable label set both via
// setOptions(displayNames, values, currentValue) and read the underlying
// id back via userData().
class EditableComboWidget : public QStackedWidget {
    Q_OBJECT
public:
    explicit EditableComboWidget(QWidget *parent = nullptr);
    void setText(const QString &text);
    QString text() const;
    QString userData() const;
    void setUserData(const QString &value);

    // Legacy: display label == value. Used by the number column.
    void setOptions(const QStringList &options, const QString &currentSelection);
    // Decoupled: i-th item shows displayNames[i] with hidden value values[i].
    // currentValue selects by value (not by label).
    void setOptions(const QStringList &displayNames,
                    const QStringList &values,
                    const QString &currentValue);

signals:
    // newValue is the underlying value (userData), not the display label.
    void valueChanged(const QString &newValue);
    void editRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLabel *m_label;
    QComboBox *m_comboBox;
    QString    m_userData;
};

// --- Component 2: Widget cho một Row bình thường ---
class CameraRowWidget : public QWidget {
    Q_OBJECT
public:
    explicit CameraRowWidget(QWidget *parent = nullptr);
    EditableComboWidget *nameWidget;
    EditableComboWidget *numberWidget;
    QPushButton *btnDelete;
};

// --- Component 3: Widget cho Row cuối cùng (Add) ---
class AddRowWidget : public QStackedWidget {
    Q_OBJECT
public:
    explicit AddRowWidget(QWidget *parent = nullptr);
    // displayNames / ids are parallel arrays; addRequested emits the id.
    // When ids is empty the add button is disabled (no cameras to add).
    void setAvailableCameras(const QStringList &displayNames,
                             const QStringList &ids);

signals:
    void addRequested(const QString &cameraId);

private:
    QWidget *createAddButtonPage();
    QWidget *createSelectCameraPage();

    QPushButton *btnAddRow;
    QComboBox *cbCameraSelect;
    QPushButton *btnCancel;
};

// --- Component 4: Custom Item để hỗ trợ Sorting ---
class SortableCameraItem : public QListWidgetItem {
public:
    explicit SortableCameraItem(QListWidget *view = nullptr) : QListWidgetItem(view) {}

    // Override toán tử so sánh để sort theo giá trị số (lưu trong Qt::UserRole)
    bool operator<(const QListWidgetItem &other) const override {
        return data(Qt::UserRole).toInt() < other.data(Qt::UserRole).toInt();
    }
};

// --- Component 5: Widget Chính (Manager) ---
class CameraMappingWidget : public FlatListWidget {
    Q_OBJECT
public:
    explicit CameraMappingWidget(QWidget *parent = nullptr);

    // Legacy entry point: each string is used as both the stable id and the
    // display label. Use setCameraOptions() when those need to differ.
    void setCameraList(const QStringList &cameras);
    // Preferred: id -> displayName. The mapping stored / returned uses ids;
    // the UI shows the matching display name.
    void setCameraOptions(const QMap<QString, QString> &idToName);

    void setNumberLimit(int limit);

    // Mapping value is the camera ID.
    QMap<int, QString> getCurrentMapping() const;
    void setCurrentMapping(const QMap<int, QString> &mapping);

signals:
    void mappingChanged(const QMap<int, QString> &mapping);

private slots:
    void onRowAdded(const QString &cameraId);
    void onRowDeleted(QListWidgetItem *item);
    void onDataChanged();

    void provideNameOptions(EditableComboWidget *widget);
    void provideNumberOptions(EditableComboWidget *widget);

private:
    void setupAddRow();
    void addMappingRow(const QString &cameraId, int number);
    void wireRow(CameraRowWidget *row, SortableCameraItem *item);
    void applyDuplicateWarnings();
    void setDuplicateWarning(CameraRowWidget *row, bool on, const QString &tooltip);
    int getSmallestAvailableNumber() const;
    QStringList getAvailableCameraIds() const;
    QList<int> getAvailableNumbers() const;
    void updateSortingData();
    QString displayFor(const QString &id) const;
    QStringList displaysFor(const QStringList &ids) const;
    void reloadStyleSheet();

    QStringList m_allCameras;                  // ids
    QMap<QString, QString> m_camIdToName;      // id -> display name
    int m_limitNumber = 8;
    SortableCameraItem *m_addRowItem = nullptr;
    AddRowWidget *m_addRowWidget = nullptr;
};

#endif // CAMERAMAPPINGWIDGET_H
