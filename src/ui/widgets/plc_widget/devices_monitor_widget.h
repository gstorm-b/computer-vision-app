#ifndef DEVICES_MONITOR_WIDGET_H
#define DEVICES_MONITOR_WIDGET_H

#include <QPointer>
#include <QStringList>
#include <QWidget>

class QFrame;
class QLabel;
class QLineEdit;
class QTableWidget;

namespace vc::widgets {

class DeviceRowDelegate;

// ─────────────────────────────────────────────────────────────────────────────
//  DevicesMonitorWidget
//
//  Self-contained monitor for either Mitsubishi M-bit or D-word registers,
//  matching the design handoff `PLC Panel` register table:
//
//    ┌──────────────────────────────────────────────────────────────────┐
//    │ M DEVICES — BIT REGISTERS         16 / 64 ACTIVE   [ filter… ]   │  header
//    ├──────────┬──────────────────────────┬──────────┬─────────────────┤
//    │ ADDRESS  │ DESCRIPTION              │ STATE    │ ACTION          │
//    ├──────────┼──────────────────────────┼──────────┼─────────────────┤
//    │ M2000    │ trigger input            │ [ ON ]   │ ON  OFF  TOGGLE │
//    │ M2001    │ vision busy              │ [ OFF ]  │ ON  OFF  TOGGLE │
//    └──────────┴──────────────────────────┴──────────┴─────────────────┘
//
//  Mode::Bit  → State chip + ON/OFF/TOGGLE per row (emits bitWriteRequested).
//  Mode::Word → Numeric value + value box + WRITE per row
//                (emits wordWriteRequested).
//
//  Rendering is owned by `DeviceRowDelegate` — every interactive control is
//  painted by the delegate itself, no per-row `setCellWidget()` widgets.
//  This guarantees the row sizes (44 px) cleanly match the painted geometry
//  with no native QPushButton bevel-clipping.
// ─────────────────────────────────────────────────────────────────────────────

class DevicesMonitorWidget : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Bit, Word };

    explicit DevicesMonitorWidget(Mode mode, QWidget *parent = nullptr);
    ~DevicesMonitorWidget() override;

    Mode mode() const { return m_mode; }

    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);

    void setRange(int start_address, int amount);
    int  startAddress() const { return m_start; }
    int  amount()       const { return m_amount; }
    void clearRows();

    // Live updates from polling.
    void setBitState(int address, quint8 value);
    void setWordValue(int address, qint16 value);
    void clearAllStatuses();

    // Per-row description / comment.
    void        setCommentEditable(bool editable);
    QString     comment(int address) const;
    void        setComment(int address, const QString &text);
    QStringList allDeviceNames() const;

signals:
    void bitWriteRequested(int address, quint8 value);
    void wordWriteRequested(int address, qint16 value);

private slots:
    void onFilterTextChanged(const QString &text);

private:
    void setupUi();
    void reloadStyleSheet();
    void rebuildRows();
    void buildRow(int row, int address);
    void recountActive();

    int     rowOfAddress(int address) const;
    int     addressOfRow(int row)     const;
    QString formatName(int address)   const;

    Mode m_mode;
    int  m_start{0};
    int  m_amount{0};
    bool m_commentEditable{true};

    QFrame              *m_header        {nullptr};
    QLabel              *m_titleLabel    {nullptr};
    QLabel              *m_subtitleLabel {nullptr};
    QLabel              *m_countLabel    {nullptr};
    QLineEdit           *m_search        {nullptr};
    QTableWidget        *m_table         {nullptr};
    DeviceRowDelegate   *m_delegate      {nullptr};
};

} // namespace vc::widgets

#endif // DEVICES_MONITOR_WIDGET_H
