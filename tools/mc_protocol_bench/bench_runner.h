#ifndef BENCH_RUNNER_H
#define BENCH_RUNNER_H

/**
 * @file bench_runner.h
 * @brief BenchRunner — drives a scripted sequence of MC requests against a real PLC and
 *        measures the round trip, using the product's own frame codecs and transports.
 */

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

#include "device/plc/mc_context.h"
#include "device/plc/mc_request.h"

namespace vc::device {
class MCFrameAbstract;
class McMsgInterface;
}

/**
 * @enum BenchStage
 * @brief The step of one request/response cycle that failed.
 *
 * Reported separately because they mean completely different things on a commissioning bench:
 * a Build failure is a configuration mistake, a Send failure is the cable or the port, a
 * Timeout is the PLC not answering (wrong station, wrong line settings), and a Parse failure
 * means the PLC answered something this codec did not expect — the only one of the four that
 * points at the codec itself.
 */
enum class BenchStage {
    Connect,
    Build,
    Send,
    Timeout,
    Parse,
    Ok
};

/// Human-readable name of a bench stage, used in the report and the log.
QString benchStageName(BenchStage stage);

/**
 * @struct BenchFailure
 * @brief One recorded failure, with the bytes that produced it.
 */
struct BenchFailure {
    int iteration{0};        ///< Iteration the failure happened on.
    BenchStage stage{BenchStage::Ok};  ///< Step that failed.
    QString detail;          ///< The codec's or transport's own error description.
    QByteArray request;      ///< Bytes sent (empty when the failure was at Build).
    QByteArray response;     ///< Bytes received so far (empty on Send/Timeout with nothing back).
};

/**
 * @struct BenchCommandStat
 * @brief Accumulated result for one command across every iteration.
 */
struct BenchCommandStat {
    QString name;                       ///< Command label, e.g. "Read bit M100 x16".
    int sent{0};                        ///< Requests attempted.
    int ok{0};                          ///< Requests that completed and parsed.
    QMap<QString, int> failuresByStage; ///< Failure count per stage name.
    QVector<qint64> latenciesUs;        ///< Round-trip time per successful request, microseconds.
    QList<BenchFailure> samples;        ///< First few failures, with their bytes.

    /// @return the latency at the given percentile (0..100), or 0 when nothing succeeded.
    qint64 percentileUs(int percentile) const;
    /// @return the mean latency in microseconds, or 0 when nothing succeeded.
    qint64 meanUs() const;
    /// @return the lowest recorded latency, or 0 when nothing succeeded.
    qint64 minUs() const;
    /// @return the highest recorded latency, or 0 when nothing succeeded.
    qint64 maxUs() const;
};

/**
 * @struct BenchReport
 * @brief Everything one run produced.
 */
struct BenchReport {
    QList<BenchCommandStat> commands;  ///< One entry per enabled command.
    qint64 wallClockMs{0};             ///< Total run time.
    int totalRequests{0};              ///< Requests attempted across all commands.
    int totalOk{0};                    ///< Requests that completed.
    bool connected{false};             ///< Whether the transport opened at all.
    QString connectError;              ///< Why it did not, when it did not.
};

Q_DECLARE_METATYPE(BenchReport)

/**
 * @struct BenchConfig
 * @brief Everything one run needs. The protocol parameters are not duplicated here: the
 *        context IS the product's own McContext, already carrying the frame type, the transport
 *        config and the M/D poll ranges.
 */
struct BenchConfig {
    /// Owned by the caller; must outlive the run. Cloned onto the worker thread before use.
    vc::device::McContext *context{nullptr};

    bool readBit{true};      ///< Read the configured M range each iteration.
    bool readWord{true};     ///< Read the configured D range each iteration.
    bool writeBit{false};    ///< Write one bit at the M range's start address.
    bool writeWord{false};   ///< Write one word at the D range's start address.

    quint8 writeBitValue{0};   ///< Value written when writeBit is enabled.
    qint16 writeWordValue{0};  ///< Value written when writeWord is enabled.

    int iterations{100};     ///< How many times to run the enabled command set.
};

/**
 * @class BenchRunner
 * @brief Runs a BenchConfig against a real PLC on its own thread and reports timings.
 *
 * Lives on a worker thread: the transports are synchronous (`waitForReadyRead` and friends), so
 * running on the GUI thread would freeze the window for the whole benchmark. The transport is
 * also created inside run(), because a QSerialPort must be created on the thread that uses it.
 */
class BenchRunner : public QObject {
    Q_OBJECT

public:
    /// Constructs the runner; nothing is opened until run() executes on the worker thread.
    explicit BenchRunner(QObject *parent = nullptr);
    ~BenchRunner() override;

    /// Sets the configuration for the next run. Call before starting the thread.
    void setConfig(const BenchConfig &config);

public slots:
    /// Opens the transport, runs the configured iterations, and emits finished().
    /// Emits finished() on every exit path, including a failed connect — the caller re-enables
    /// its Run button from that signal and would otherwise be stuck.
    void run();
    /// Asks the run to stop at the next request boundary. Safe to call from another thread.
    void requestStop();

signals:
    /// Emitted as iterations complete, for a progress bar.
    void progress(int done, int total);
    /// Emitted for a line of human-readable running commentary.
    void logLine(const QString &line);
    /// Emitted once when the run ends, successfully or not.
    void finished(BenchReport report);

private:
    /// Builds the frame codec matching the context's frame type.
    /// @return the codec, or nullptr when the frame type has no implementation
    std::unique_ptr<vc::device::MCFrameAbstract> makeFrame() const;
    /// Builds the transport matching the context's message-interface type.
    /// @return the transport, or nullptr when the interface type has no implementation
    std::unique_ptr<vc::device::McMsgInterface> makeTransport() const;

    /**
     * @brief Runs one request to completion and records the outcome into `stat`.
     * @param[in]     request the request to send
     * @param[in]     iteration the iteration number, for failure records
     * @param[in,out] stat the command's accumulating statistics
     * @return the stage reached; BenchStage::Ok when the response parsed
     */
    BenchStage runOne(vc::device::MCRequest &request, int iteration, BenchCommandStat &stat);

    BenchConfig m_config;                                 ///< Configuration for the current run.
    std::shared_ptr<vc::device::McContext> m_context;     ///< Worker-thread copy of the context.
    std::unique_ptr<vc::device::MCFrameAbstract> m_frame; ///< Frame codec for this run.
    std::unique_ptr<vc::device::McMsgInterface> m_transport; ///< Transport for this run.
    vc::device::McDeviceMap m_deviceMap;                  ///< Somewhere for read values to land.
    QAtomicInt m_stopRequested{0};                        ///< Set by requestStop(), read between requests.
};

#endif // BENCH_RUNNER_H
