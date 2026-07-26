#include "mc_protocol_device.h"
#include "mc_msg_tcp_client.h"
#include "mc_request.h"

#include <memory>
#include <QRegularExpression>
#include <QThread>

namespace vc::device {

namespace {

/// Parses a "<prefix><digits>" device tag (e.g. "M12", case-insensitive prefix, whitespace
/// trimmed) into a numeric address, requiring the prefix to match `expectedPrefix`.
/// @param tag the raw tag text to parse
/// @param expectedPrefix the required device-type prefix ('M' or 'D'); a different prefix is rejected
/// @param address output; set to the parsed non-negative address on success, left untouched on failure
/// @return true if `tag` matched the expected prefix and a valid non-negative number
bool parseMcTag(const QString &tag, QChar expectedPrefix, int *address)
{
    if (!address) {
        return false;
    }

    static const QRegularExpression re(QStringLiteral("^([MD])(\\d+)$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto match = re.match(tag.trimmed());
    if (!match.hasMatch()) {
        return false;
    }

    const QChar prefix = match.captured(1).at(0).toUpper();
    if (prefix != expectedPrefix) {
        return false;
    }

    bool ok = false;
    const int parsed = match.captured(2).toInt(&ok, 10);
    if (!ok || parsed < 0) {
        return false;
    }

    *address = parsed;
    return true;
}

} // namespace

/// Constructs the device, forwarding id/name/parent to PlcDevice and registering m_config as
/// the active configuration via IDevice::setDeviceConfig.
/// @param id unique device identifier
/// @param name display name
/// @param parent optional QObject parent for lifetime management
McProtocolDevice::McProtocolDevice(QString id, QString name, QObject* parent)
    : PlcDevice(id, name, parent) {

    IDevice::setDeviceConfig(&m_config);
}

/// Destructor. Body is intentionally empty; connection teardown happens via
/// deviceTerminate()/deviceDisconnect() before destruction.
/// @note May run on a different thread than the one that created the device.
McProtocolDevice::~McProtocolDevice() {
    // Destructor call from another thread
}

/// Terminates the device: logs the request and, if still connected, disconnects (stopping the
/// polling timer and releasing the transport) via deviceDisconnect().
void McProtocolDevice::deviceTerminate() {
    LOG_DEV_DEBUG << "MC protocol device terminate, device name" << name()
                  << ", id" << id();
    if (isDeviceConnected()) {
        LOG_DEV_DEBUG << "MC protocol disconnect, destroy timer";
        deviceDisconnect();
    }
}

/// Establishes the MC protocol connection: an idempotent no-op re-publish of Connected if
/// already connected; otherwise (re)initializes the transport/frame codec, opens the socket
/// connection, and starts the polling timer on success.
/// @return true if already connected or newly connected; false if initialization or the socket
/// connect failed (status published as ConnectFailed)
/// @note Locks m_mutex for the duration of the call.
bool McProtocolDevice::deviceConnect() {
    QMutexLocker locker(&m_mutex);

    // Connect requests are idempotent. Re-running the full init while already
    // connected would build a second transport over the live one, leaking the
    // open socket and desyncing the single-connection MC session — exactly the
    // "old connection not closed, cannot reconnect" failure on phase re-entry.
    // Re-publish Connected so a runner that issued the redundant request still
    // sees a status event and clears its busy flag.
    if (connectStatus() == ConnectStatus::Connected) {
        this->setConnectionStatus(ConnectStatus::Connected);
        return true;
    }

    if (!this->initialize_mc_device()) {
        return false;
    }

    McMsgInterface::MsgIfState msg_port_state = m_msg_interface->ConnectToPort();
    if (msg_port_state != McMsgInterface::MsgIfState::Connected) {
        const QString err = m_msg_interface->GetErrorDescription();
        LOG_DEV_INFO << "connect error occurred," << err;
        // Free the half-open transport so a later retry starts from a clean
        // state instead of leaking the failed socket.
        releaseConnectionResources();
        this->setConnectionStatus(ConnectStatus::ConnectFailed, err);
        return false;
    }

    LOG_USER_INFO << "MC Device connected";
    this->setConnectionStatus(ConnectStatus::Connected);
    m_polling_timer->start();
    return true;
}

/// Tears down the connection (releases the transport and publishes Disconnected status) and
/// logs the disconnect.
/// @return always true.
bool McProtocolDevice::deviceDisconnect() {
    teardownConnection(ConnectStatus::Disconnected);
    LOG_USER_INFO << "MC Device disconnected";
    return true;
}

/// Stops (without deleting) the reused polling timer and destroys/releases the message-interface
/// transport and frame codec, leaving the device ready for a clean reconnect.
/// @note Caller must hold m_mutex; safe to call from within the timer's own slot chain since the
/// timer is only stopped, never deleted.
void McProtocolDevice::releaseConnectionResources() {
    // Caller holds m_mutex. The polling timer is created once and reused across
    // connect cycles; only stop it here (never delete) because this can run
    // from inside the timer's own slot chain (e.g. send/receive failure paths).
    if (m_polling_timer && m_polling_timer->isActive()) {
        m_polling_timer->stop();
    }

    if (m_msg_interface) {
        // DestroyMsgPort() closes and deletes the socket synchronously on the
        // device's own thread (see McEthernetTcpPort). reset() then frees the
        // interface; both are deterministic and run on the correct thread.
        m_msg_interface->DestroyMsgPort();
        m_msg_interface.reset();
    }
    m_frame.reset();
}

/// Releases connection resources under m_mutex and publishes `finalStatus` as the new
/// connection state.
/// @param finalStatus the terminal ConnectStatus to publish (e.g. Disconnected, LostConnected)
void McProtocolDevice::teardownConnection(ConnectStatus finalStatus) {
    QMutexLocker locker(&m_mutex);
    releaseConnectionResources();
    this->setConnectionStatus(finalStatus);
}

/// Applies a new device configuration (expected to be a McProtocolConfig) if non-null and the
/// device is not currently connected: updates the MC context, recomputes the polling device
/// map, and republishes the config via IDevice::setDeviceConfig.
/// @param cfg the new device configuration; ignored if null or if the device is currently connected
/// @note Locks m_mutex while applying the change.
void McProtocolDevice::setDeviceConfig(IDeviceCfg *cfg) {
    if (!cfg) {
        return;
    }

    if (this->isDeviceConnected()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    McProtocolConfig *mc_config = static_cast<McProtocolConfig*>(cfg);
    m_config.setContext(mc_config->context());
    optimizeDeviceMap();
    // change and emit signal
    IDevice::setDeviceConfig(&m_config);
}

/// Applies a new MC protocol configuration directly (bypassing the polymorphic IDeviceCfg
/// interface), ignored while the device is connected. Otherwise behaves like setDeviceConfig().
/// @param cfg the new MC protocol configuration to adopt
void McProtocolDevice::setMcProtocolConfig(McProtocolConfig& cfg) {
    if (this->isDeviceConnected()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_config.setContext(cfg.context());
    optimizeDeviceMap();
    // change and emit signal
    IDevice::setDeviceConfig(&m_config);
}

/// Returns a copy of the current MC protocol configuration.
McProtocolConfig McProtocolDevice::mcProtocolConfig() const {
    return m_config;
}

/// Returns the list of digital ("M") device tag names currently subscribed in the device map.
QStringList McProtocolDevice::availableDigitalIoNames() const {
    return m_m_device_names;
}

/// Returns the list of word ("D") device tag names currently subscribed in the device map.
QStringList McProtocolDevice::availableWordIoNames() const {
    return m_d_device_names;
}

/// Parses `tag` as an "M<address>" digital IO tag, builds a single-bit WriteBit MC request for
/// it, and queues the request via pushRequest().
/// @param tag digital IO tag, e.g. "M12"
/// @param value bit value to write (mapped to 0x01/0x00)
/// @return true if the tag parsed, the request was valid, and it was queued; false otherwise (logged)
bool McProtocolDevice::writeDigitalIoByName(const QString &tag, bool value)
{
    int address = 0;
    if (!parseMcTag(tag, QLatin1Char('M'), &address)) {
        LOG_DEV_ERR << "McProtocolDevice: invalid digital IO tag" << tag;
        return false;
    }

    MCRequest request(MCRequest::WriteBit, 'M', address, 1);
    if (!request.isValid()) {
        LOG_DEV_ERR << "McProtocolDevice: invalid digital IO write request" << tag;
        return false;
    }

    request.buildWriteData_Bit_Device(value ? 0x01 : 0x00);
    return pushRequest(&request);
}

/// Parses `tag` as a "D<address>" word IO tag, builds a single-word WriteWord MC request for
/// it, and queues the request via pushRequest().
/// @param tag word IO tag, e.g. "D34"
/// @param value word value to write
/// @return true if the tag parsed, the request was valid, and it was queued; false otherwise (logged)
bool McProtocolDevice::writeWordIoByName(const QString &tag, qint16 value)
{
    int address = 0;
    if (!parseMcTag(tag, QLatin1Char('D'), &address)) {
        LOG_DEV_ERR << "McProtocolDevice: invalid word IO tag" << tag;
        return false;
    }

    MCRequest request(MCRequest::WriteWord, 'D', address, 1);
    if (!request.isValid()) {
        LOG_DEV_ERR << "McProtocolDevice: invalid word IO write request" << tag;
        return false;
    }

    request.buildWriteData_Word_Device_Word(value);
    return pushRequest(&request);
}

/// Clones `request` (must be of type Request_MC) into an MCRequest, detaches its value payload,
/// and appends it to the ad-hoc request queue for the next polling cycle.
/// @param request the request to enqueue; rejected if its type() is not RequestType::Request_MC
/// @return true if enqueued, false if the type check failed
/// @note Locks m_mutex while mutating the queue.
bool McProtocolDevice::pushRequest(IRequest *request) {
    if (request->type() != RequestType::Request_MC) {
        return false;
    }

    m_mutex.lock();
    std::shared_ptr<MCRequest> mc_request = std::static_pointer_cast<MCRequest>(request->clone());
    mc_request->m_value.detach();
    m_request_queue.push_back(mc_request);
    m_mutex.unlock();
    return true;
}

/// Restores device state from `obj` via IDevice::fromJson(), then recomputes the polling
/// device map to match the restored configuration.
/// @param obj serialized device state
/// @return the result of IDevice::fromJson(obj)
/// @note Locks m_mutex for the duration.
bool McProtocolDevice::fromJson(const QJsonObject &obj) {
    QMutexLocker locker(&m_mutex);
    bool state = IDevice::fromJson(obj);
    optimizeDeviceMap();
    return state;
}

/// Polling timer slot; stops the timer if the device is no longer connected, otherwise drives
/// the next polling_query() step.
void McProtocolDevice::onPollingTimerTimeOut() {
    if (!this->isDeviceConnected()) {
        if(m_polling_timer) {
            if (m_polling_timer->isActive()) {
                m_polling_timer->stop();
            }
        }
        return;
    }

    // LOG_DEV_DEBUG << "Timeout...";
    polling_query();
}

/// Slot for the message interface's readyRead signal; forwards to response_handle() to process
/// the pending reply.
void McProtocolDevice::onMsgInterfaceReadReady() {
    response_handle();
}

/// Queues a heartbeat write toggling the configured "active" M-bit device, then flips the
/// cached value so the next call sends the opposite state.
/// @note Appends directly to m_request_queue without the m_mutex locking pushRequest() uses;
/// only safe because this runs on the same (polling) call path as the queue drain.
void McProtocolDevice::onSetCommActiveDevice() {
    std::shared_ptr<MCRequest> request = std::make_shared<MCRequest>(
        MCRequest::WriteBit, 'M', m_comm_active_m_device, 1);
    request->buildWriteData_Bit_Device((m_comm_active_m_device_value) ? 0x01 : 0x00);
    m_request_queue.push_back(request);

    m_comm_active_m_device_value = !m_comm_active_m_device_value;
}

/// (Re)builds the MC transport stack for the currently configured protocol: releases any
/// previous transport, then constructs the frame codec and message interface matching the
/// config's frame/message types, wires up the read-ready signal, (re)creates the polling timer
/// on first use, refreshes its interval, recomputes the polling device map, and resets all
/// per-session state (queues, counters, buffers).
/// @return true if both the frame type and message-interface type are recognized/supported;
/// false if either is unsupported, leaving the device unusable for connect.
bool McProtocolDevice::initialize_mc_device() {
    // Clear any transport left over from a previous (possibly failed) session
    // before building a fresh one, so connect attempts never stack sockets.
    releaseConnectionResources();

    McFrameType frame_type = m_config.context()->frameType();
    McMsgItfType msg_type = m_config.context()->msgConfig()->type();

    // setup frame builder
    switch (frame_type) {
    case vc::device::mc::McFrameType::Frame_3E:
        m_config.context()->setDeviceMap(&m_device_map);
        m_frame = std::make_unique<Frame3E>();
        LOG_DEV_INFO << "Initialize MC Protocol Frame 3E.";
        break;
    default:
        return false;
    }

    // setup msg interface
    switch (msg_type) {
    case vc::device::mc::McMsgItfType::EthernetTCPIP:
        m_msg_interface = std::make_unique<McEthernetTcpPort>();
        m_msg_interface->SetConfig(m_config.context()->msgConfig());
        LOG_DEV_INFO << "Initialize Ethernet TCP/IP message interface.";
        break;
    default:
        return false;
    }
    connect(m_msg_interface->ioDevice(), &QIODevice::readyRead,
            this, &McProtocolDevice::onMsgInterfaceReadReady);


    McContext *ctx = m_config.context();
    // Setup polling timer. Created once and reused across connect cycles so it
    // is never deleted from inside its own slot chain (see
    // releaseConnectionResources). Only the interval is refreshed per session.
    if (!m_polling_timer) {
        m_polling_timer = new QTimer(this);
        m_polling_timer->setTimerType(Qt::PreciseTimer);
        connect(m_polling_timer, &QTimer::timeout,
                this, &McProtocolDevice::onPollingTimerTimeOut);
    }
    m_polling_timer->setInterval(ctx->m_refreshInterval);
    LOG_DEV_INFO << "MC Protocol Device refresh interval:" << ctx->m_refreshInterval;

    // setup polling device map
    optimizeDeviceMap();

    // setup state flag
    m_wait_for_response = false;
    is_first_time_polling = true;
    m_update_command_index = 0;
    m_data_update_state = DataQueryState::QueryTriggerByTimer;
    // m_retry_by_timeout = false;
    m_retry_count = 0;

    m_current_request.reset();
    m_request_queue.clear();
    m_read_buffer.clear();

    m_comm_active_m_device = m_config.context()->m_activeMDevice;

    return true;
}

/// Drives one step of the polling state machine: if a response is still outstanding, checks for
/// timeout and retries; otherwise pops the next ad-hoc request (if any) or the next scheduled
/// polling request round-robin, updates m_data_update_state accordingly (emitting
/// pollingUpdate() and queuing the heartbeat write when a full round completes), and dispatches
/// it via request_handle().
/// @note Locks m_mutex only while draining m_request_queue.
void McProtocolDevice::polling_query() {
    if (m_wait_for_response) {
        auto current_time_point = std::chrono::high_resolution_clock::now();
        auto time_lasp = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_point - m_sent_time_point);
        if (time_lasp.count() > m_config.context()->msgConfig()->m_responseTimeout) {
            // response timeout handle;
            retry_request_handle();
        }
        return;
    }

    m_mutex.lock();
    if (!m_request_queue.empty()) {
        m_current_request = m_request_queue.takeFirst();
        m_mutex.unlock();
        // Ad-hoc commands (the heartbeat M-bit toggle) are queued exactly once
        // per completed round, so the round-robin reads must resume right after
        // this response, in the same tick, instead of waiting for the next
        // timer tick.
        m_data_update_state = DataQueryState::QueryContinue;
    } else {
        m_mutex.unlock();

        if (m_polling_request_queue.isEmpty()) {
            return;
        }

        m_current_request = m_polling_request_queue.at(m_update_command_index++);
        if (m_update_command_index >= m_polling_request_queue.count()) {
            m_update_command_index = 0;
            m_data_update_state = DataQueryState::QueryFinished;
            is_first_time_polling = false;
            // emit device's value change signal
            // LOG_DEV_DEBUG << "Device map updated";
            emit pollingUpdate(m_device_map.clone());
            onSetCommActiveDevice();
        } else {
            m_data_update_state = DataQueryState::QueryContinue;
        }
    }

    request_handle();
}

/// Encodes m_current_request into a frame and sends it over the message interface; on send
/// failure, retries up to 5 times before disconnecting; on success, records the send timestamp
/// and marks the device as waiting for a response.
void McProtocolDevice::request_handle() {
    if (m_current_request != nullptr) {
        QByteArray send_frame;

        m_mutex.lock();
        MCFrameAbstract::FrameReturnCode rt_code = m_frame->makeSendFrame(m_current_request.get(), m_config.context(), send_frame);
        m_mutex.unlock();

        if (rt_code == MCFrameAbstract::RequestFrameOK) {
            McMsgInterface::MsgErrorState send_state = m_msg_interface->SendMsg(send_frame);
            // LOG_DEV_DEBUG << "MC Protocol Device sent message.";

            if (send_state != McMsgInterface::MsgErrorState::NoError) {

                LOG_USER_ERR << tr("MC Device error, cannot send request. Retry %1.").arg(m_retry_count);
                m_current_request.reset();
                m_retry_count += 1;

                if (m_retry_count >= 5) {
                    LOG_USER_ERR << tr("Mc Device error, retry send request over 5 times, disconnect.");
                    deviceDisconnect();
                }
                return;
            }

            m_sent_time_point = std::chrono::high_resolution_clock::now();
            m_wait_for_response = true;
            // LOG_USER_INFO << "Send frame to PLC:" << send_frame.toHex(' ');
        } else {
            m_current_request.reset();
            LOG_USER_ERR << "make request frame error";
        }
    }
}

/// Reads and parses the next reply for m_current_request, carrying over any partial bytes from
/// a previous split read; on a complete response (ok/error/invalid) clears the read buffer and
/// outstanding-request state, and on a still-incomplete frame stores the accumulated bytes back
/// into m_read_buffer for the next call.
/// @note Continues the polling round via polling_query() when the state machine is mid-round
/// (QueryContinue).
void McProtocolDevice::response_handle() {
    if (!m_wait_for_response) {
        return;
    }

    if (m_current_request == nullptr) {
        return;
    }

    QByteArray receive_bytes = m_read_buffer; // carry over any partial frame from a prior split read
    McMsgInterface::MsgErrorState recieve_state = m_msg_interface->ReceiveMsg(receive_bytes, 1);
    // OLOG_INFO << "Received frame from PLC:" << receive_bytes.toHex(' ');

    if (recieve_state != McMsgInterface::MsgErrorState::NoError) {
        if (recieve_state == McMsgInterface::MsgErrorState::BufferEmpty) {
            LOG_USER_ERR << "empty frame";
            return;
        }
        LOG_USER_ERR << "error while receive frame, error code" << recieve_state;
        return;
    }

    m_mutex.lock();
    MCFrameAbstract::FrameReturnCode rt_code = m_frame->parseReceiveFrame(m_current_request.get(), m_config.context(), receive_bytes);
    m_mutex.unlock();

    if (rt_code == MCFrameAbstract::ResponseOk) {
        // OLOG_INFO << "reponse frame status error," << m_frame->lastErrorDescription();
        check_device_changed();

    } else if  (rt_code == MCFrameAbstract::ResponseError) {
        LOG_USER_ERR << "reponse frame status error," << m_frame->lastErrorDescription();
    } else if  (rt_code == MCFrameAbstract::ResponseInvalid) {
        LOG_USER_ERR << "reponse frame invalid," << m_frame->lastErrorDescription();
    } else if (rt_code == MCFrameAbstract::WaitingReceive) {
        m_read_buffer = receive_bytes; // still incomplete; keep the accumulated bytes for the next read
        return;
    }

    m_read_buffer.clear(); // frame fully consumed (ok/error/invalid) - start clean next time
    m_retry_count = 0;
    // m_retry_by_timeout = false;
    m_wait_for_response = false;
    m_current_request.reset();

    if (m_data_update_state == DataQueryState::QueryContinue) {
        // LOG_DEV_DEBUG << "Continue query ...";
        polling_query();
    }
}

/// Handles a response timeout: gives up (marks the device lost) after more than 5 retries,
/// otherwise clears the transport's receive buffer and any partial frame, and resends the
/// current request via request_handle().
void McProtocolDevice::retry_request_handle() {
    m_retry_count += 1;
    if (m_retry_count > 5) {
        // LOG_USER_ERR << "MC Device Error, disconnect to device";
        setDeviceLostConnect();
        return;
    }

    // if (m_current_request) {
    //     LOG_USER_ERR << "MC Device Error, request timeout" << m_current_request->type();
    // }
    LOG_USER_INFO << "MC Device Error, timeout, retry" << m_retry_count;
    m_msg_interface->clearBuffer();
    m_read_buffer.clear(); // discard any partial frame from the abandoned attempt
    m_wait_for_response = false;
    request_handle();
}


/// Rebuilds the device map's subscribed M/D address ranges from the current context, merges
/// them into optimal contiguous ranges, and regenerates the polling request queue and per-tag
/// name lists.
void McProtocolDevice::optimizeDeviceMap() {
    m_device_map.clearMap();
    McContext *ctx = m_config.context();
    m_device_map.Subscribe_deivce('M', ctx->m_startMAddress, ctx->m_amountMAddress);
    m_device_map.Subscribe_deivce('D', ctx->m_startDAddress, ctx->m_amountDAddress);
    m_device_map.OptimizeRanges();
    m_polling_request_queue.clear();
    update_m_map();
    update_d_map();
}

/// Rebuilds the digital (M) device-value map, its "last known value" shadow map, and the
/// exposed tag-name list from the device map's subscribed M ranges, and appends one ReadBit
/// polling request per range.
void McProtocolDevice::update_m_map() {
    if (m_device_map.m_devices.ranges.empty()) {
        return;
    }

    m_device_map.device_map_m.clear();
    m_last_device_M_map.clear();
    m_m_device_names.clear();

    std::vector<McDeviceRange::DeviceRange> *m_ranges_ptr = &m_device_map.m_devices.ranges;
    int r_size = m_device_map.m_devices.ranges.size();
    // LOG_DEV_DEBUG << "RSize" << r_size;
    for (int index=0;index<r_size;index++) {
        int start_address = (m_ranges_ptr->begin())[index].start;
        int amount = (m_ranges_ptr->begin())[index].amount;
        // init map
        for (int i=0;i<amount;i++) {
            m_device_map.device_map_m[start_address+i] = 0x00;
            m_last_device_M_map[start_address+i] = 0x00;

            QString device_name = QString("M%1").arg(start_address + i, 4, 10, QChar('0'));
            m_m_device_names.append(device_name);
        }

        std::shared_ptr<MCRequest> request = std::make_shared<MCRequest>(
            MCRequest::ReadBit, 'M', start_address, amount);
        m_polling_request_queue.push_back(request);
    }
}

/// Rebuilds the word (D) device-value map, its "last known value" shadow map, and the exposed
/// tag-name list from the device map's subscribed D ranges, and appends one ReadWord polling
/// request per range.
void McProtocolDevice::update_d_map() {
    if (m_device_map.d_devices.ranges.empty()) {
        return;
    }

    m_device_map.device_map_d.clear();
    m_last_device_D_map.clear();
    m_d_device_names.clear();

    std::vector<McDeviceRange::DeviceRange> *d_ranges_ptr = &m_device_map.d_devices.ranges;
    int r_size = m_device_map.d_devices.ranges.size();
    // LOG_DEV_DEBUG << "RSize" << r_size;
    for (int index=0;index<r_size;index++) {
        int start_address = (d_ranges_ptr->begin())[index].start;
        int amount  = (d_ranges_ptr->begin())[index].amount;
        // init map
        for (int i=0;i<amount;i++) {
            m_device_map.device_map_d[start_address+i] = 0x0000;
            m_last_device_D_map[start_address+i] = 0x0000;

            QString device_name = QString("D%1").arg(start_address + i, 4, 10, QChar('0'));
            m_d_device_names.append(device_name);
        }

        std::shared_ptr<MCRequest> request = std::make_shared<MCRequest>(
            MCRequest::ReadWord, 'D', start_address, amount);
        m_polling_request_queue.push_back(request);
    }
}

/// Compares the freshly polled M and D device maps against their "last known value" shadows,
/// emits deviceMChanged()/deviceDChanged() for each changed address (suppressed during the very
/// first polling round), accumulates the changes into a single valueChanged(QMap) signal, and
/// updates the shadow maps in place.
/// @note If a map's size changed since the last comparison, its shadow map is first resynced via
/// update_last_m_map()/update_last_d_map() before the value diff.
void McProtocolDevice::check_device_changed() {
    QMap<QString, QVariant> changedValues;
    if ((!m_device_map.device_map_m.empty()) && (!m_last_device_M_map.empty())) {

        if (m_device_map.device_map_m.size() != m_last_device_M_map.size()) {
            update_last_m_map();
            LOG_DEV_INFO << "Device M map size change:" << m_last_device_M_map.size();
        }

        std::map<int, quint8>::iterator it_new_m = m_device_map.device_map_m.begin();
        std::map<int, quint8>::iterator it_last_m = m_last_device_M_map.begin();
        for (; it_new_m != m_device_map.device_map_m.end(); ++it_new_m, ++it_last_m) {
            if (it_new_m->second != it_last_m->second) {
                if (!is_first_time_polling) {
                    // append to changed maps;
                    changedValues.insert(QString("M%1").arg(it_new_m->first, 4, 10, QChar('0')), it_new_m->second);

                    emit deviceMChanged(it_new_m->first, it_last_m->second, it_new_m->second);
                }
                it_last_m->second = it_new_m->second;
            }
        }
    }

    if ((m_device_map.device_map_d.empty()) || (m_last_device_D_map.empty())) {
        return;
    }
    if (m_device_map.device_map_d.size() != m_last_device_D_map.size()) {
        update_last_d_map();
        LOG_DEV_INFO << "Device D map size change:" << m_last_device_D_map.size();
    }

    std::map<int, qint16>::iterator it_new_d = m_device_map.device_map_d.begin();
    std::map<int, qint16>::iterator it_last_d = m_last_device_D_map.begin();
    for (; it_new_d != m_device_map.device_map_d.end(); ++it_new_d, ++it_last_d) {
        if (it_new_d->second != it_last_d->second) {
            if (!is_first_time_polling) {
                // append to changed maps;
                changedValues.insert(QString("D%1").arg(it_new_d->first, 4, 10, QChar('0')), it_new_d->second);

                emit deviceDChanged(it_new_d->first, it_last_d->second, it_new_d->second);                
            }
            it_last_d->second = it_new_d->second;
        }
    }

    if (changedValues.size() > 0) {
        emit valueChanged(changedValues);
    }
}

/// Adds any address present in the current M device map but missing from m_last_device_M_map,
/// seeding it with the current value (used to resync the shadow map after a size change).
void McProtocolDevice::update_last_m_map() {
    for (std::map<int, quint8>::iterator it = m_device_map.device_map_m.begin();
         it != m_device_map.device_map_m.end();
         ++it) {

        if (m_last_device_M_map.find(it->first) == m_last_device_M_map.end()){
            m_last_device_M_map[it->first] = it->second;
        }
    }
}

/// Adds any address present in the current D device map but missing from m_last_device_D_map,
/// seeding it with the current value (used to resync the shadow map after a size change).
void McProtocolDevice::update_last_d_map() {
    for (std::map<int, qint16>::iterator it = m_device_map.device_map_d.begin();
         it != m_device_map.device_map_d.end();
         ++it) {

        if (m_last_device_D_map.find(it->first) == m_last_device_D_map.end()){
            m_last_device_D_map[it->first] = it->second;
        }
    }
}

/// Tears down the connection and publishes LostConnected status after repeated response
/// timeouts.
void McProtocolDevice::setDeviceLostConnect() {
    teardownConnection(ConnectStatus::LostConnected);
    LOG_USER_ERR << "MC Device lost connect.";
}

}
