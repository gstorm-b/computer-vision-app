#include "mc_protocol_device.h"
#include "mc_device_map_diff.h"
#include "mc_frame_1c.h"
#include "mc_frame_3c.h"
#include "mc_msg_serial_port.h"
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
        // Must publish a status even here. PlcRunner::requestConnect() sets a busy flag
        // that is only cleared by a status/failure signal, so returning false silently
        // wedged the runner: every later reconnect request — automatic or manual — was
        // dropped without a trace.
        this->setConnectionStatus(ConnectStatus::ConnectFailed,
                                  QStringLiteral("MC device initialization failed."));
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
    // BEFORE the locker, and it has to be: failOutstandingRequests() takes m_mutex itself, and
    // QMutex is not recursive. Both terminal paths — deviceDisconnect() and
    // setDeviceLostConnect() — reach the queue through here, so this is the one place that has
    // to remember, instead of two.
    failOutstandingRequests(
        finalStatus == ConnectStatus::LostConnected
            ? QStringLiteral("Abandoned: the PLC connection was lost before the write was sent.")
            : QStringLiteral("Abandoned: the device was disconnected before the write was sent."));

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

/// Tracked bit write: same parsing and validation as writeDigitalIoByName(), but queued
/// correlated so the caller learns whether it reached the PLC.
/// @return the completion id, or 0 if the tag or request was rejected (no completion follows)
void McProtocolDevice::writeDigitalIoTracked(quint64 id, const QString &tag, bool value) {
    int address = 0;
    if (!parseMcTag(tag, QLatin1Char('M'), &address)) {
        LOG_DEV_ERR << "McProtocolDevice: invalid digital IO tag" << tag;
        // Resolved, not dropped: a rejection the caller never hears about is the same silence
        // this whole phase exists to remove.
        resolveIoWrite(id, false, QStringLiteral("\"%1\" is not an M-device tag.").arg(tag));
        return;
    }

    MCRequest request(MCRequest::WriteBit, 'M', address, 1);
    if (!request.isValid()) {
        LOG_DEV_ERR << "McProtocolDevice: invalid digital IO write request" << tag;
        resolveIoWrite(id, false,
                       QStringLiteral("A write request for \"%1\" could not be built.").arg(tag));
        return;
    }

    request.buildWriteData_Bit_Device(value ? 0x01 : 0x00);
    pushTrackedRequest(&request, id);
}

/// Tracked word write; see writeDigitalIoTracked().
void McProtocolDevice::writeWordIoTracked(quint64 id, const QString &tag, qint16 value) {
    int address = 0;
    if (!parseMcTag(tag, QLatin1Char('D'), &address)) {
        LOG_DEV_ERR << "McProtocolDevice: invalid word IO tag" << tag;
        resolveIoWrite(id, false, QStringLiteral("\"%1\" is not a D-device tag.").arg(tag));
        return;
    }

    MCRequest request(MCRequest::WriteWord, 'D', address, 1);
    if (!request.isValid()) {
        LOG_DEV_ERR << "McProtocolDevice: invalid word IO write request" << tag;
        resolveIoWrite(id, false,
                       QStringLiteral("A write request for \"%1\" could not be built.").arg(tag));
        return;
    }

    request.buildWriteData_Word_Device_Word(value);
    pushTrackedRequest(&request, id);
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

/// Queues `request` correlated, so exactly one requestFinished() reports its outcome.
/// @param request request to clone and queue
/// @return the correlation id, or 0 if the request was not of MC type
/// @note Locks m_mutex; the id is allocated under the same lock that appends the request, so two
///       threads submitting at once cannot be handed the same id.
quint64 McProtocolDevice::pushTrackedRequest(IRequest *request, quint64 id) {
    if (request == nullptr || request->type() != RequestType::Request_MC) {
        return 0;
    }

    m_mutex.lock();
    std::shared_ptr<MCRequest> mc_request = std::static_pointer_cast<MCRequest>(request->clone());
    mc_request->m_value.detach();
    // A caller that already owns an id (PlcRunner, which handed one out synchronously before
    // queueing the write) passes it in; a direct caller lets the device allocate. Either way
    // there is exactly one id space, so the two can never collide.
    const quint64 correlationId = (id != 0) ? id : nextIoWriteId();
    mc_request->setCorrelationId(correlationId);
    m_request_queue.push_back(mc_request);
    m_mutex.unlock();
    return correlationId;
}

/// Emits requestFinished() once for a correlated request, then marks it resolved.
///
/// Every terminal path funnels through here rather than emitting directly, so "exactly once" is
/// one check in one place instead of a rule each path is trusted to remember.
void McProtocolDevice::resolveRequest(const std::shared_ptr<MCRequest> &request, bool ok,
                                      const QString &message) {
    if (!request || !request->needsResolution()) {
        return;
    }
    request->markResolved();

    McResult result;
    result.isOk = ok;
    result.startAddress = request->m_start_address;
    result.register_type = QString(QChar::fromLatin1(request->m_device_type));
    result.register_amount = request->m_amount;
    result.msg = message;
    result.correlationId = request->correlationId();
    emit requestFinished(result);
    // The family-neutral form of the same completion. requestFinished() carries the MC-specific
    // detail (device type, address, amount, raw payload); this is the one shape all four PLC
    // families speak, and the one the controller policy will consume.
    resolveIoWrite(request->correlationId(), ok, message);
}

/// Fails the in-flight request and everything still queued, with `reason`.
///
/// A queue that is simply cleared leaves every correlated caller waiting forever, which is worse
/// than telling them the write failed: a failure is actionable, silence is not.
void McProtocolDevice::failOutstandingRequests(const QString &reason) {
    // The in-flight one first, so completions arrive in submission order.
    resolveRequest(m_current_request, false, reason);

    m_mutex.lock();
    const QList<std::shared_ptr<MCRequest>> abandoned = m_request_queue;
    m_mutex.unlock();

    // Resolved outside the lock: requestFinished() reaches a directly connected consumer on this
    // thread, and letting it re-enter the device while m_mutex is held would deadlock.
    for (const std::shared_ptr<MCRequest> &request : abandoned) {
        resolveRequest(request, false, reason);
    }
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
/// Builds the concrete transport for `type`; the production factory the switch always was.
/// @param type configured message-interface type
/// @return the transport, or null if `type` is not supported by this build
std::unique_ptr<McMsgInterface> McProtocolDevice::createMsgInterface(mc::McMsgItfType type) {
    switch (type) {
    case vc::device::mc::McMsgItfType::EthernetTCPIP:
        LOG_DEV_INFO << "Initialize Ethernet TCP/IP message interface.";
        return std::make_unique<McEthernetTcpPort>();
    case vc::device::mc::McMsgItfType::SerialPort:
        LOG_DEV_INFO << "Initialize serial message interface.";
        return std::make_unique<McMsgSerialPort>();
    default:
        return nullptr;
    }
}

bool McProtocolDevice::initialize_mc_device() {
    // Clear any transport left over from a previous (possibly failed) session
    // before building a fresh one, so connect attempts never stack sockets.
    releaseConnectionResources();

    // A config whose frame type has no context factory arm (Frame_1E today, or a frame token
    // this build does not know) leaves m_context null, and every line below dereferences it.
    // Loading such a project used to crash here rather than refuse to connect.
    if (m_config.context() == nullptr) {
        LOG_DEV_ERR << "MC device has no protocol context; the configured frame type is not "
                       "supported by this build.";
        return false;
    }
    if (m_config.context()->msgConfig() == nullptr) {
        LOG_DEV_ERR << "MC protocol context has no message-interface configuration.";
        return false;
    }

    McFrameType frame_type = m_config.context()->frameType();
    McMsgItfType msg_type = m_config.context()->msgConfig()->type();

    // Frame-independent, so it is done once rather than inside every frame arm: a codec whose
    // context has no device map parses a response and has nowhere to put the values.
    m_config.context()->setDeviceMap(&m_device_map);

    // setup frame builder
    switch (frame_type) {
    case vc::device::mc::McFrameType::Frame_3E:
        m_frame = std::make_unique<Frame3E>();
        LOG_DEV_INFO << "Initialize MC Protocol Frame 3E.";
        break;
    case vc::device::mc::McFrameType::Frame_1C:
        m_frame = std::make_unique<Frame1C>();
        LOG_DEV_INFO << "Initialize MC Protocol Frame 1C.";
        break;
    case vc::device::mc::McFrameType::Frame_3C:
        m_frame = std::make_unique<Frame3C>();
        LOG_DEV_INFO << "Initialize MC Protocol Frame 3C.";
        break;
    default:
        LOG_DEV_ERR << "Unsupported MC frame type:" << McFrameTypeToString(frame_type);
        return false;
    }

    // setup msg interface
    m_msg_interface = createMsgInterface(msg_type);
    if (!m_msg_interface) {
        LOG_DEV_ERR << "Unsupported MC message interface type:"
                    << McMsgItfTypeToString(msg_type);
        // The frame codec built above would otherwise survive into the next connect attempt
        // alongside a null transport.
        m_frame.reset();
        return false;
    }
    m_msg_interface->SetConfig(m_config.context()->msgConfig());
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

    // Anything correlated still sitting here is about to be discarded by the clear below —
    // pushed while the device was disconnected, most likely. Dropping it silently leaves the
    // caller waiting for a completion that can never come.
    //
    // Resolved with m_mutex HELD (deviceConnect() owns it for this whole call), which is safe
    // for the same reason the setConnectionStatus() calls a few lines up in deviceConnect() are:
    // requestFinished() reaches PlcRunner over a queued connection, so it cannot re-enter the
    // device on this thread.
    resolveRequest(m_current_request,
                   false,
                   QStringLiteral("Abandoned: the device was re-initialized before the write "
                                  "was sent."));
    for (const std::shared_ptr<MCRequest> &request : m_request_queue) {
        resolveRequest(request, false,
                       QStringLiteral("Abandoned: the device was re-initialized before the "
                                      "write was sent."));
    }

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
            // pollingUpdate() and is_first_time_polling both used to be cleared HERE, and both
            // were wrong for the same reason: this is the moment the last request of the round is
            // *selected*, before request_handle() sends it and long before its response is parsed.
            // They now live at the tail of response_handle(), where the round is genuinely over.
            // See Phase 9 / E5.
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
                // Resolved BEFORE the reset, or the request is dropped here with nothing said.
                // A send failure abandons this request outright — request_handle() does not
                // resend it, it just returns and the next tick takes the following one.
                resolveRequest(m_current_request, false,
                               QStringLiteral("Send failed: the request could not be written to "
                                              "the PLC transport."));
                m_current_request.reset();
                m_retry_count += 1;

                if (m_retry_count >= 5) {
                    LOG_USER_ERR << tr("Mc Device error, retry send request over 5 times, connection lost.");
                    // LostConnected, not Disconnected. Disconnected means "an operator
                    // asked for this" and is deliberately NOT a recoverable status, so
                    // publishing it here made a pulled cable permanent: the localization
                    // recovery policy ignored it, never scheduled a reconnect, and never
                    // withdrew bTaskReady. A failed send is an unexpected loss.
                    setDeviceLostConnect();
                }
                return;
            }

            m_sent_time_point = std::chrono::high_resolution_clock::now();
            m_wait_for_response = true;
            // LOG_USER_INFO << "Send frame to PLC:" << send_frame.toHex(' ');
        } else {
            resolveRequest(m_current_request, false,
                           QStringLiteral("Frame build failed: the request could not be encoded "
                                          "for the configured MC frame type."));
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
        resolveRequest(m_current_request, true, QStringLiteral("OK"));

    } else if  (rt_code == MCFrameAbstract::ResponseError) {
        LOG_USER_ERR << "reponse frame status error," << m_frame->lastErrorDescription();
        // The frame's own mapped error description, not a generic "failed": on a NAK it is the
        // PLC's end code, which is the only thing that says WHICH write the PLC refused and why.
        resolveRequest(m_current_request, false, m_frame->lastErrorDescription());
    } else if  (rt_code == MCFrameAbstract::ResponseInvalid) {
        LOG_USER_ERR << "reponse frame invalid," << m_frame->lastErrorDescription();
        resolveRequest(m_current_request, false, m_frame->lastErrorDescription());
    } else if (rt_code == MCFrameAbstract::WaitingReceive) {
        m_read_buffer = receive_bytes; // still incomplete; keep the accumulated bytes for the next read
        return;                        // NOT terminal - the request is still owed a completion
    }

    m_read_buffer.clear(); // frame fully consumed (ok/error/invalid) - start clean next time
    m_retry_count = 0;
    // m_retry_by_timeout = false;
    m_wait_for_response = false;
    m_current_request.reset();

    if (m_data_update_state == DataQueryState::QueryContinue) {
        // LOG_DEV_DEBUG << "Continue query ...";
        polling_query();
    } else if (m_data_update_state == DataQueryState::QueryFinished) {
        // The round is over: this response was the last one, and every subscribed range is now in
        // m_device_map. Publishing HERE is what makes the snapshot mean "what was just read",
        // which is the contract both Modbus families already keep.
        //
        // Field-reported 2026-09-09: emitting at the wrap instead meant the FIRST snapshot carried
        // the zero-fill update_d_map() had written, so setup() read nActiveCamera as 0 and the
        // runtime faulted with CameraNotRegistered on a project whose camera 1 was bound and
        // registered. See Phase 9 / E5.
        //
        // QueryFinished holds if and only if the round's last polling request is the one that just
        // answered: polling_query() assigns m_data_update_state on EVERY dispatch, and its ad-hoc
        // branch always assigns QueryContinue. response_handle() returns early on
        // !m_wait_for_response, so one round cannot publish twice.
        is_first_time_polling = false;
        emit pollingUpdate(m_device_map.clone());
    }
}

/// Handles a response timeout: gives up (marks the device lost) after more than 5 retries,
/// otherwise clears the transport's receive buffer and any partial frame, and resends the
/// current request via request_handle().
void McProtocolDevice::retry_request_handle() {
    m_retry_count += 1;
    if (m_retry_count > 5) {
        // LOG_USER_ERR << "MC Device Error, disconnect to device";
        // Resolved here rather than left to setDeviceLostConnect()'s sweep, so the reason names
        // retry exhaustion — this write WAS sent, repeatedly, and the PLC never answered. That is
        // a different fault from one abandoned in the queue, and the caller can tell them apart.
        resolveRequest(m_current_request, false,
                       QStringLiteral("No response after %1 retries; the PLC did not answer.")
                           .arg(m_retry_count - 1));
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

/// Compares the freshly polled M and D device maps against their "last known value" shadows via
/// diffMcDeviceMaps(), emits deviceMChanged()/deviceDChanged() for each changed address
/// (suppressed during the very first polling round), and publishes both areas' changes as a
/// single valueChanged(QMap).
/// @note The shadows are brought up to date even on the suppressed first round, so the round
/// after it reports only genuine changes.
void McProtocolDevice::check_device_changed() {
    // The diff itself lives in a header-only helper so it can be tested: nothing in this
    // repository has ever executed a McProtocolDevice method — the transport is built internally
    // with no injection point — and this function held two defects that no suite could reach.
    const McDeviceMapDiff diff = diffMcDeviceMaps(m_device_map.device_map_m,
                                                  m_last_device_M_map,
                                                  m_device_map.device_map_d,
                                                  m_last_device_D_map,
                                                  is_first_time_polling);

    // A newly seen address has no honest previous value, so it reaches valueChanged (the consumer
    // needs to learn the value) but not the per-address signal, whose contract is old -> new.
    for (const McMapChange &change : diff.mChanges) {
        if (change.hasPrevious) {
            emit deviceMChanged(change.address, change.previousValue, change.currentValue);
        }
    }
    for (const McMapChange &change : diff.dChanges) {
        if (change.hasPrevious) {
            emit deviceDChanged(change.address, change.previousValue, change.currentValue);
        }
    }

    if (!diff.changedValues.isEmpty()) {
        emit valueChanged(diff.changedValues);
    }
}

/// Tears down the connection and publishes LostConnected status after repeated response
/// timeouts.
void McProtocolDevice::setDeviceLostConnect() {
    // Clear the retry budget with the connection. It is a per-session counter, and
    // leaving it exhausted meant the first timeout after a successful reconnect
    // immediately declared the link lost again.
    m_retry_count = 0;
    m_wait_for_response = false;

    teardownConnection(ConnectStatus::LostConnected);
    LOG_USER_ERR << "MC Device lost connect.";
}

}
