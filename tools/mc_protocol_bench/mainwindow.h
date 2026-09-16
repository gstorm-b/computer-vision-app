#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/**
 * @file mainwindow.h
 * @brief MainWindow — parameter entry, run control and reporting for the MC protocol bench.
 */

#include <QMainWindow>
#include <QThread>

#include "bench_runner.h"
#include "device/plc/mc_context.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class GadgetTable;

/**
 * @class MainWindow
 * @brief The bench window: pick a frame, edit its parameters and its transport's, choose the
 *        command mix, run against the PLC, read the numbers.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /// Builds the window, restores the last-used settings and selects the default frame.
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    /// Saves the current settings before closing, and refuses to close mid-run.
    void closeEvent(QCloseEvent *event) override;

private slots:
    /// Rebuilds the context for the newly selected frame and repopulates both parameter tables.
    void onFrameChanged();
    /// Starts a run on the worker thread.
    void onRun();
    /// Asks the current run to stop at the next request boundary.
    void onStop();
    /// Appends a line to the log view.
    void onLogLine(const QString &line);
    /// Updates the progress bar.
    void onProgress(int done, int total);
    /// Renders the finished report and re-enables the controls.
    void onFinished(BenchReport report);

private:
    /// Creates the context for `frame`, seeded with this bench's defaults for that frame.
    void buildContext(vc::device::mc::McFrameType frame);
    /// Repopulates the context and transport parameter tables from the current context.
    void refreshTables();
    /// Reads both parameter tables back into the context.
    void commitTables();
    /// Renders `report` into the results table and the summary line.
    void renderReport(const BenchReport &report);
    /// Enables or disables the controls that must not change during a run.
    void setRunning(bool running);
    /// Persists the current frame selection, command mix and iteration count.
    void saveSettings();
    /// Restores what saveSettings() wrote.
    void loadSettings();
    /// Persists the current context's parameters under the selected frame's own key, so each
    /// frame keeps its own line settings between sessions.
    void saveContext();
    /// Restores the stored parameters for the current frame, if any were saved.
    void loadContext();

    Ui::MainWindow *ui;                                 ///< Generated form.
    GadgetTable *m_contextTable{nullptr};               ///< Editor for the McContext properties.
    GadgetTable *m_transportTable{nullptr};             ///< Editor for the message-interface config.
    std::shared_ptr<vc::device::McContext> m_context;   ///< The context being edited and benched.
    QThread *m_thread{nullptr};                         ///< Worker thread hosting the runner.
    BenchRunner *m_runner{nullptr};                     ///< Runner; lives on m_thread.
};

#endif // MAINWINDOW_H
