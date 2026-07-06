#ifndef SIGNALS_MONITOR_WIDGET_H
#define SIGNALS_MONITOR_WIDGET_H

#include "ui/widgets/controls/flat_list_widget.h"

#include <QWidget>
#include <QVariant>
#include <QList>
#include <QString>

class QListWidgetItem;

namespace vc::widgets::sm_internal { class RowWidget; }

// =====================================================================
// SignalsMonitorWidget
// =====================================================================
//
// Read-only monitor of "task signals" already mapped via SignalsMapWidget.
// One row per logical signal slot, four columns per row:
//
//   <signal display name> | <type chip> | <current value> | [Modify]
//
// The widget is task-agnostic and device-agnostic. The owner page is
// responsible for:
//   • appendRow() the schema (mirrors SignalsMapWidget's setup).
//   • setRowTag() with the tag bound to each signal from the task config.
//   • refreshBool() / refreshNumber() with live values keyed by tag-name.
//   • setDeviceConnected() to gate the Modify button.
//   • Listening to requestWriteValue() and routing to the device runner.
//
// Container is a QListWidget with one custom row widget per item. Column
// widths are auto-sized to the widest content of each column across all
// rows, so every column visually aligns even though each row is its own
// widget. See relayoutColumns().
//
// Bool rows track OFF→ON→OFF transitions that complete inside 200 ms and
// surface a "Rising edge" chip next to the ON/OFF chip. The chip auto-
// hides after 2 s; a new rising-edge event while it is still visible
// triggers a single blink (hide 500 ms, then show again, hide timer
// restarts).
//
class SignalsMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    enum class Type { Number, Bool };
    Q_ENUM(Type)

    explicit SignalsMonitorWidget(QWidget *parent = nullptr);
    ~SignalsMonitorWidget() override;

    // Row management. internalName is the unique row key. append/insert
    // are no-ops (log warning) when internalName already exists.
    void appendRow(const QString &internalName,
                   const QString &displayName,
                   Type type);
    void insertRowAt(int row,
                     const QString &internalName,
                     const QString &displayName,
                     Type type);
    void removeRow(const QString &internalName);
    void clearRows();

    // Tag binding (from the task config / SignalsMapWidget output).
    // Empty tag → row renders "(not mapped)" and disables Modify.
    void setRowTag(const QString &internalName, const QString &tag);
    QString rowTag(const QString &internalName) const;

    // Value refresh — KEY by signal name (internalName), not PLC tag.
    // Unknown signal name → log warning + ignore.
    void refreshBool(const QString &internalName, bool value);
    void refreshNumber(const QString &internalName, int value);

    // Device connection state. Gates every row's Modify button.
    // Last-received values are preserved across reconnect cycles.
    void setDeviceConnected(bool connected);
    bool isDeviceConnected() const { return m_deviceConnected; }

signals:
    // Emitted after the user confirms a new value in the Modify dialog.
    // Owner routes the write to the device runner; the widget does not
    // self-update — call refreshBool/refreshNumber when the device echoes
    // the new value.
    void requestWriteValue(const QString &internalName, const QVariant &value);

private:
    void reloadStyleSheet();
    void relayoutColumns();
    int  rowIndexOf(const QString &internalName) const;
    void openModifyDialog(vc::widgets::sm_internal::RowWidget *row);

    FlatListWidget *m_list{nullptr};
    QList<vc::widgets::sm_internal::RowWidget *> m_rows;

    bool m_deviceConnected{false};
};

#endif // SIGNALS_MONITOR_WIDGET_H
