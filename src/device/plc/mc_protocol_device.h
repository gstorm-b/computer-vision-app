#ifndef MC_PROTOCOL_DEVICE_H
#define MC_PROTOCOL_DEVICE_H

#include "device/plc/plc_device.h"
#include "device/plc/mc_protocol_config.h"
#include "device/plc/mc_fame_3e.h"
#include "device/device_capabilities.h"

#include <QTimer>
#include <chrono>

#define MC_PROTOCOL_DEVICE_STR      "MC protocol device"  ///< Human-readable display label for this device type.

/// PLC device family (config, protocol devices, and MC-protocol support types).
namespace vc::device {

/// Mitsubishi MC-protocol PLC device: owns the transport (McMsgInterface) and frame codec
/// (MCFrameAbstract), round-robins polling reads over the configured M/D device ranges, and
/// serves ad-hoc digital/word writes via IPlcTagProvider/IPlcIoWriter.
class McProtocolDevice : public PlcDevice, public IPlcTagProvider, public IPlcIoWriter {
    Q_OBJECT

public:
    /// Constructs the device; connection, transport, and polling state are set up later by
    /// deviceConnect()/initialize_mc_device().
    explicit McProtocolDevice(QString id, QString name, QObject* parent = nullptr);
    /// Destroys the device. Actual disconnect/teardown happens via deviceTerminate()/
    /// deviceDisconnect() before destruction; this destructor may run on another thread.
    ~McProtocolDevice();

    /// Establishes the MC transport and frame codec and starts polling.
    /// @return true if already connected (re-publishes Connected and returns early) or if the
    /// transport connects successfully; false on initialization or transport connect failure
    bool deviceConnect() override;
    /// Tears down the transport/frame and publishes ConnectStatus::Disconnected.
    /// @return always true
    bool deviceDisconnect() override;
    /// Returns whether the device's current connect status is ConnectStatus::Connected.
    bool isDeviceConnected() const override {
        return connectStatus() == device::ConnectStatus::Connected;
    }

    /// Returns PlcType::MitsubishiMc, identifying this as the Mitsubishi MC PLC family device.
    PlcType plcType() const override {
        return PlcType::MitsubishiMc;
    }

    /// Returns the MC frame type of the current configuration (delegates to
    /// McProtocolConfig::currentFrameType()).
    McFrameType currentFrameType() const {
        return m_config.currentFrameType();
    }

    /// Applies `cfg` (must be a McProtocolConfig) as the new configuration, rebuilds the
    /// polling device map, and re-emits the config-changed signal. No-op if `cfg` is null or
    /// the device is currently connected.
    void setDeviceConfig(IDeviceCfg *cfg) override;
    /// Applies `cfg`'s context as the new configuration, rebuilds the polling device map, and
    /// re-emits the config-changed signal. No-op if the device is currently connected.
    void setMcProtocolConfig(McProtocolConfig& cfg);
    /// Returns a copy of the current MC protocol configuration.
    McProtocolConfig mcProtocolConfig() const;

    /// Returns the names of the digital (M) device tags covered by the current device map.
    QStringList availableDigitalIoNames() const override;
    /// Returns the names of the word (D) device tags covered by the current device map.
    QStringList availableWordIoNames() const override;
    /// Parses `tag` as an "M<address>" digital tag and queues a single-bit write request for
    /// `value`.
    /// @return false if `tag` doesn't match the M-device pattern or the request is invalid
    bool writeDigitalIoByName(const QString &tag, bool value) override;
    /// Parses `tag` as a "D<address>" word tag and queues a single-word write request for
    /// `value`.
    /// @return false if `tag` doesn't match the D-device pattern or the request is invalid
    bool writeWordIoByName(const QString &tag, qint16 value) override;

    /// Clones `request` (must be an MCRequest, i.e. RequestType::Request_MC) and appends it to
    /// the ad-hoc request queue for the next polling tick to send.
    /// @return false if `request`'s type isn't RequestType::Request_MC
    bool pushRequest(IRequest *request) override;

    /// Restores the device from JSON via IDevice::fromJson(), then rebuilds the polling
    /// device map to match the restored configuration.
    bool fromJson(const QJsonObject &obj) override;

public slots:
    /// Disconnects the device if currently connected, in preparation for destruction.
    void deviceTerminate() override;

private slots:
    /// Polling timer tick: stops the timer if the device is no longer connected, otherwise
    /// drives one step of the round-robin poll via polling_query().
    void onPollingTimerTimeOut();
    /// Transport readyRead handler: forwards to response_handle() to parse the pending reply.
    void onMsgInterfaceReadReady();
    /// Queues a bit-write toggling the configured "comm active" M-device between 0 and 1, to
    /// signal liveness to the PLC each polling round.
    void onSetCommActiveDevice();

private:
    /// Phase of the current polling round, driving whether the next tick resumes the same
    /// round-robin pass or waits for the timer.
    enum DataQueryState {
        QueryTriggerByTimer,  ///< Waiting for the next timer tick to start/continue polling.
        QueryContinue,        ///< A response just completed; immediately issue the next request.
        QueryFinished         ///< The round-robin pass over all polling requests has completed.
    };

    /// (Re)builds the transport and frame codec from m_config, resets polling/queue state, and
    /// (re)creates the polling timer with the configured refresh interval.
    /// @return false if the configured frame type or message interface type is unsupported
    bool initialize_mc_device();
    /// Frees the transport + frame and stops (but keeps) the polling timer. Does not change
    /// connection status.
    /// @note caller must hold m_mutex.
    void releaseConnectionResources();
    /// Releases transport resources and publishes the given terminal status.
    /// @note locks m_mutex; safe to call from the polling/response handlers.
    void teardownConnection(ConnectStatus finalStatus);
    /// Unused/placeholder request executor (declared, not defined/called elsewhere in this
    /// class).
    void excute_request();
    /// Drives one polling step: handles a pending response timeout via retry_request_handle(),
    /// otherwise pops the next ad-hoc request or advances the round-robin polling queue (and,
    /// on completing a full pass, emits pollingUpdate() and queues the comm-active toggle)
    /// before dispatching it with request_handle().
    void polling_query();

    /// Encodes m_current_request into a frame via m_frame->makeSendFrame() and sends it
    /// through m_msg_interface; on send failure retries up to 5 times before disconnecting.
    void request_handle();
    /// Reads and parses the pending response frame; on success updates the device map via
    /// check_device_changed(), on a still-incomplete frame stashes the partial bytes in
    /// m_read_buffer for the next call, and on completion (ok/error/invalid) clears state and,
    /// if mid-round, continues polling via polling_query().
    void response_handle();
    /// Handles a response timeout: gives up (via setDeviceLostConnect()) after more than 5
    /// retries, otherwise clears the transport buffer/read buffer and resends the current
    /// request via request_handle().
    void retry_request_handle();

    /// Rebuilds the device map's M/D address ranges and polling request queue from the
    /// current context's configured start/amount addresses.
    void optimizeDeviceMap();
    /// Rebuilds the M-device value/name maps and appends one ReadBit polling request per
    /// optimized M address range.
    void update_m_map();
    /// Rebuilds the D-device value/name maps and appends one ReadWord polling request per
    /// optimized D address range.
    void update_d_map();
    /// Diffs the freshly-polled M/D device maps against their "last" snapshots, emits
    /// deviceMChanged()/deviceDChanged()/valueChanged() for each changed address (suppressed
    /// on the very first polling pass), and updates the snapshots in place.
    void check_device_changed();
    /// Fills in any M-device map entries missing from the "last" snapshot (used after a size
    /// change) without disturbing existing entries.
    void update_last_m_map();
    /// Fills in any D-device map entries missing from the "last" snapshot (used after a size
    /// change) without disturbing existing entries.
    void update_last_d_map();

    /// Tears down the connection and publishes ConnectStatus::LostConnected after repeated
    /// response timeouts.
    void setDeviceLostConnect();

signals:
    /// Emitted when a queued MC request completes, carrying its result.
    void requestFinished(vc::device::McResult result);
    /// Emitted when a polled M-device bit's value changes.
    /// @param number M-device address
    /// @param last_state previous bit value
    /// @param new_state new bit value
    void deviceMChanged(int number, quint8 last_state, quint8 new_state);
    /// Emitted when a polled D-device word's value changes.
    /// @param number D-device address
    /// @param last_val previous word value
    /// @param new_val new word value
    void deviceDChanged(int number, qint16 last_val, qint16 new_val);

private:
    McProtocolConfig m_config;  ///< Current MC frame type and addressing/timing configuration.

    std::unique_ptr<McMsgInterface> m_msg_interface;  ///< Active transport (e.g. TCP socket); null until connected.
    std::unique_ptr<MCFrameAbstract> m_frame;  ///< Frame codec matching the configured frame type; null until connected.

    QList<std::shared_ptr<MCRequest>> m_request_queue;  ///< Ad-hoc requests (writes, comm-active toggle) awaiting send.
    QList<std::shared_ptr<MCRequest>> m_polling_request_queue;  ///< Round-robin read requests covering the configured device map.

    std::shared_ptr<MCRequest> m_current_request;  ///< Request currently in flight (sent, awaiting response), or null.

    McDeviceMap m_device_map;  ///< Current M/D device values and address-range subscriptions.
    QTimer *m_polling_timer{nullptr};  ///< Drives periodic polling; created once and reused across connect cycles.

    std::chrono::high_resolution_clock::time_point m_sent_time_point;  ///< Timestamp the current request was sent, for timeout detection.

    bool m_comm_active_m_device_value{false};  ///< Next value to write to the comm-active M-device (toggles each round).
    int m_comm_active_m_device;  ///< Address of the M-device used as the comm-active heartbeat bit.
    DataQueryState m_data_update_state;  ///< Current phase of the round-robin polling pass.
    int m_update_command_index;  ///< Index of the next polling request to issue in m_polling_request_queue.
    bool is_first_time_polling;  ///< True until the first full polling pass completes; suppresses change signals until then.
    bool m_wait_for_response;  ///< True while a request has been sent and its response is still pending.
    // bool m_retry_by_timeout{false};
    int m_retry_count{0};  ///< Number of consecutive send/response-timeout retries for the current request.

    std::map<int, quint8> m_last_device_M_map;  ///< Previous-poll snapshot of M-device values, for change detection.
    std::map<int, qint16> m_last_device_D_map;  ///< Previous-poll snapshot of D-device values, for change detection.

    QStringList m_m_device_names;  ///< Display names ("M####") for all subscribed M-device addresses.
    QStringList m_d_device_names;  ///< Display names ("D####") for all subscribed D-device addresses.

    QByteArray m_read_buffer;  ///< Bytes of a partially-received frame, carried over to the next receive attempt.
};


} // namespace vc::device

#endif // MC_PROTOCOL_DEVICE_H
