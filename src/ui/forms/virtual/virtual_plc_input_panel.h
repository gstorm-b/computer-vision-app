#ifndef VIRTUAL_PLC_INPUT_PANEL_H
#define VIRTUAL_PLC_INPUT_PANEL_H

/**
 * @file virtual_plc_input_panel.h
 * @brief VirtualPlcInputPanel — drives a hardware-free PLC's input tags from the device page.
 */

#include <QPointer>
#include <QWidget>

#include "runtime/plc_runner.h"

QT_BEGIN_NAMESPACE
namespace Ui { class VirtualPlcInputPanel; }
QT_END_NAMESPACE

/**
 * @class VirtualPlcInputPanel
 * @brief The controls that let a virtual PLC's inputs be driven, so a project with no PLC on the
 *        network can be taken past Ready.
 *
 * Every runtime state after Ready begins with the PLC changing an input — `bExecuteTrigger`,
 * `bErrorReset`, `nActiveCamera`, `nActivePatternGroup`. Without a way to produce those, a
 * hardware-free project could be built, opened, configured and taken to Ready, and then nothing:
 * every transition the task state machine has was unreachable, including the fault paths that
 * matter most.
 *
 * @note **Every write goes through PlcRunner, never to the device.** The device lives on the
 *       runner's worker thread and injection emits a signal from it. Calling across threads is the
 *       trap ModbusDeviceWidget already paid for: Qt reported nothing, the frame never reached the
 *       wire, and it failed intermittently.
 *
 * @note The table shows what the device actually **published**, not what was typed. It is filled
 *       from PlcRunner::valueChanged(), which is the same signal the runtime consumes — so a value
 *       appearing here is evidence it was delivered, not merely requested.
 */
class VirtualPlcInputPanel : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Builds the panel against `runner`.
     * @param[in] runner the PLC runner carrying values to the device thread; the panel disables
     *            itself when this is null or the device cannot have its inputs driven.
     * @param[in] digitalTags bit tag names offered in the tag box.
     * @param[in] wordTags word tag names offered in the tag box.
     */
    explicit VirtualPlcInputPanel(vc::runtime::PlcRunner *runner,
                                  const QStringList &digitalTags,
                                  const QStringList &wordTags,
                                  QWidget *parent = nullptr);
    ~VirtualPlcInputPanel() override;

private slots:
    /// Retunes the value editor for the tag now selected: a bit tag is 0..1, a word tag is the
    /// full signed 16-bit range. The editor's range is what tells the operator which kind of tag
    /// they picked, which is why there is no separate type control.
    void onTagChanged(const QString &tag);
    /// Sends the current tag/value to the device through the runner.
    void onSetClicked();
    /// Adds or updates the row for each tag the device published.
    void onValuesPublished(const QMap<QString, QVariant> &values);

private:
    Ui::VirtualPlcInputPanel *ui;
    /// Held as a QPointer because the runner is owned by TaskRunner, which outlives this panel in
    /// normal use but is not guaranteed to.
    QPointer<vc::runtime::PlcRunner> m_runner;
    QStringList m_digitalTags;  ///< Bit tag names offered in the tag box.
};

#endif // VIRTUAL_PLC_INPUT_PANEL_H
