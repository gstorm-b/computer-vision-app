#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//
// The HMI. It owns the worker QThread and the ModbusRunner living inside it,
// but it never touches a Modbus object itself: everything travels through
// queued signals, so a slow or busy Modbus client can never freeze the UI.
//
// All widgets come from mainwindow.ui - nothing is created in C++ except the
// table rows, whose number depends on ModbusDemo::kAddressCount and on the
// display format selected for the area.
//

#include <QList>
#include <QMainWindow>
#include <QPointer>
#include <QString>

#include "modbusdefs.h"

class ModbusRunner;

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;
class QThread;
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

//! One typed value watched on top of the raw register map: the Data Monitor
//! tab reads and writes it as INT32 / FLOAT32 / BCD / ASCII / ... across as
//! many consecutive registers as the type needs.
struct MonitorEntry
{
    QString name;
    int area = ModbusDemo::HoldingRegisters;
    int address = 0;
    int type = 0;      //!< DataCodec::Type
    int order = 0;     //!< DataCodec::ByteOrder
    int length = 1;    //!< registers, only meaningful for ASCII text
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

Q_SIGNALS:
    // GUI -> worker thread (queued)
    void startServerRequested(const QString &address, int port, int unitId);
    void stopServerRequested();
    void writeValueRequested(int area, int address, int value);
    void writeBlockRequested(const ModbusBlock &block);
    void fillAreaRequested(int area, int value);
    void randomizeAreaRequested(int area);
    void acceptConnectionsRequested(bool accept);
    void logLevelChanged(int level);

protected:
    void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
    // worker thread -> GUI (queued)
    void onLogMessage(int level, const QString &message);
    void onServerStateChanged(int state, const QString &text, bool running);
    void onBlockChanged(const ModbusBlock &block);
    void onStatsChanged(const ModbusStats &stats);

    // widget reactions
    void onStartClicked();
    void onStopClicked();
    void onTableItemChanged(QTableWidgetItem *item);
    void onSaveLog();
    void onClearLog();
    void onAbout();

    // data monitor
    void onMonitorTypeChanged();
    void onMonitorAdd();
    void onMonitorRemove();
    void onMonitorClear();
    void onMonitorWrite();

private:
    void setupTables();
    void setupMonitor();
    void setupConnections();
    void startWorkerThread();
    void shutdownWorker();

    // register tables
    int registersPerRow(int area) const;
    int rowsForArea(int area) const;
    void rebuildAreaTable(int area);
    void updateAreaRow(int area, int row);
    void refreshAreaRange(int area, int firstAddress, int lastAddress);
    void updatePlcReferences(int area);
    QList<quint16> valuesFor(int area, int address, int count) const;

    // data monitor
    void addMonitorRow(const MonitorEntry &entry);
    void refreshMonitorRow(int row);
    void refreshMonitorRange(int area, int firstAddress, int lastAddress);

    void appendLogLine(int level, const QString &message);
    void logLocal(int level, const QString &message);
    void showLocalAddresses();

    Ui::MainWindow *ui;

    QThread *m_thread = nullptr;              //!< deleted here once it has stopped
    QPointer<ModbusRunner> m_runner;          //!< NOT owned: the worker thread deletes it

    QList<QTableWidget *> m_tables;
    QCheckBox *m_plcRefChecks[ModbusDemo::AreaCount] = {};
    QSpinBox *m_plcBaseSpins[ModbusDemo::AreaCount] = {};

    //! Display format of the two register areas (null for the bit areas, which
    //! are always shown as ON/OFF check boxes).
    QComboBox *m_formatCombos[ModbusDemo::AreaCount] = {};
    QComboBox *m_orderCombos[ModbusDemo::AreaCount] = {};
    int m_format[ModbusDemo::AreaCount] = {};   //!< DataCodec::Type
    int m_order[ModbusDemo::AreaCount] = {};    //!< DataCodec::ByteOrder

    //! Mirror of the register map, so formatting and the Data Monitor need no
    //! round trip to the worker thread.
    QList<quint16> m_values[ModbusDemo::AreaCount];
    QList<MonitorEntry> m_monitor;

    bool m_updatingTables = false;   //!< blocks the itemChanged feedback loop
    bool m_shutDown = false;         //!< makes shutdownWorker() idempotent
};

#endif // MAINWINDOW_H
