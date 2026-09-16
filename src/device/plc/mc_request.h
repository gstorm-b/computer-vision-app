#ifndef MC_REQUEST_H
#define MC_REQUEST_H

/**
 * @file mc_request.h
 * @brief MC-protocol request object, register-value builders, and the McResult payload.
 */

#include <memory>
#include <QString>
#include <QList>
#include "device/irequest.h"
#include "device/plc/memory_utils.h"

namespace vc::device {

/**
 * @struct McResult
 * @brief Outcome of a single MC-protocol read/write request, as reported to
 *        listeners of MCProtocolDevice::requestFinished.
 */
struct McResult {
    bool isOk;                ///< True when the request completed successfully.
    int startAddress;         ///< Starting register address the result corresponds to.
    QString register_type;    ///< Device/register type of the result (e.g. "D", "M").
    int register_amount;      ///< Number of registers/bits covered by the result.
    QString msg;               ///< Human-readable status or error message.
    QByteArray data;           ///< Raw response payload bytes.
    /// Correlation id of the request this result completes; matches the value
    /// McProtocolDevice::pushTrackedRequest() returned. Never 0 on a delivered result —
    /// uncorrelated traffic (polling reads, the comm-active heartbeat) produces no result at all.
    quint64 correlationId{0};
};

/**
 * @class MCRequest
 * @brief Concrete IRequest for the Mitsubishi MC protocol: describes a single bit or
 *        word read/write against a PLC device (device-type letter + start address +
 *        amount) and can encode the write payload into `m_value`.
 */
class MCRequest : public IRequest {
public:
    /**
     * @enum RqType
     * @brief Kind of MC-protocol operation this request performs.
     */
    enum RqType {
        ReadBit,
        WriteBit,
        ReadWord,
        WriteWord,
    };

    /// Default-constructs an unconfigured request; isValid() is false until
    /// one of the parameterized constructors below succeeds.
    MCRequest() {

    }

    /// @return Request_MC, identifying this as an MC-protocol request for
    /// IDevice::pushRequest dispatch.
    RequestType type() const override {
        return RequestType::Request_MC;
    }

    /// Builds a request from a textual PLC device address, e.g. "D100": the
    /// first character is the device-type letter, the remainder is parsed as
    /// the start address. Marks the request invalid (isValid() == false) if
    /// `head_device` is empty, the address fails to parse, or `amount` is not
    /// in (0, 64].
    /// @param head_device device-type letter followed by the start address, e.g. "D100"
    MCRequest(RqType rqtype, QString head_device, int amount) {
        m_type = rqtype;
        if (head_device.isEmpty()) {
            is_config = false;
            return;
        }
        m_device_type = head_device[0].toLatin1();
        bool success;
        m_start_address = head_device.mid(1).toInt(&success, 10);
        if (!success) {
            is_config = false;
            return;
        }

        if ((amount <= 0) || (amount > 64)) {
            is_config = false;
            return;
        }

        m_amount = amount;
        is_config = true;
    }

    /// Builds a request directly from device-type letter, start address, and
    /// amount. Marks the request invalid (isValid() == false) when `amount`
    /// is not in (0, 128].
    MCRequest(RqType rqtype, char dv_type, int start_address, int amount) {
        m_type = rqtype;
        m_device_type = dv_type;
        m_start_address = start_address;

        if ((amount <= 0) || (amount > 128)) {
            is_config = false;
            return;
        }

        m_amount = amount;
        is_config = true;
    }

    /// @return true once a constructor above has produced a valid request
    /// (parsed address, in-range amount); false otherwise.
    const bool isValid() {
        return is_config;
    }

    /// Encodes a list of bit values into `m_value`, normalizing every entry
    /// to 0x00 (off) or 0x01 (on).
    void buildWriteData_Bit_Device(QList<quint8> &value) {
        m_value.clear();
        for (int idx=0;idx<value.size();idx++) {
            m_value.append(static_cast<quint16>((value.at(idx) == 0x00) ? 0x00 : 0x01));
        }
    }

    /// Encodes a single bit value into `m_value` as-is (no 0/1 normalization,
    /// unlike the QList overload above).
    void buildWriteData_Bit_Device(quint8 value) {
        m_value.clear();
        m_value.append(value);
    }

    /// Encodes a list of signed 16-bit words into `m_value` (raw bit pattern,
    /// reinterpreted as quint16).
    void buildWriteData_Word_Device_Word(QList<qint16> &value) {
        m_value.clear();
        for (int idx=0;idx<value.size();idx++) {
            m_value.append(static_cast<quint16>(value.at(idx)));
        }
    }

    /// Encodes a single signed 16-bit word into `m_value`.
    void buildWriteData_Word_Device_Word(qint16 value) {
        m_value.clear();
        m_value.append(static_cast<quint16>(value));
    }

    /// Encodes a list of signed 32-bit values into `m_value`, splitting each
    /// into two 16-bit words (low word first, then high word).
    void buildWriteData_Word_Device_DoubleWord(QList<qint32> &value) {
        m_value.clear();
        for (int idx=0;idx<value.size();idx++) {
            m_value.append(static_cast<quint16>(value.at(idx) & 0xFFFF));
            m_value.append(static_cast<quint16>((value.at(idx) >> 16) & 0xFFFF));
        }
    }

    /// Encodes a list of floats into `m_value` by converting each to its
    /// IEEE-754 bit pattern (floatToReal32) and splitting it into two 16-bit
    /// words (low word first, then high word).
    void buildWriteData_Word_Device_Float(QList<float> &value) {
        m_value.clear();
        for (int idx=0;idx<value.size();idx++) {
            quint32 fnum = floatToReal32(value.at(idx));
            m_value.append(static_cast<quint16>(fnum & 0xFFFF));
            m_value.append(static_cast<quint16>((fnum >> 16) & 0xFFFF));
        }
    }

    /// Encodes a list of doubles into `m_value` by converting each to its
    /// IEEE-754 bit pattern (doubleToReal64) and splitting it into four
    /// 16-bit words, least-significant word first.
    void buildWriteData_Word_Device_DoubleFloat(QList<double> &value) {
        m_value.clear();
        for (int idx=0;idx<value.size();idx++) {
            quint64 fnum = doubleToReal64(value.at(idx));
            m_value.append(static_cast<quint16>(fnum & 0xFFFF));
            m_value.append(static_cast<quint16>((fnum >> 16) & 0xFFFF));
            m_value.append(static_cast<quint16>((fnum >> 32) & 0xFFFF));
            m_value.append(static_cast<quint16>((fnum >> 48) & 0xFFFF));
        }
    }

    /// @return a deep copy of this request (via the copy constructor), boxed
    /// as the abstract IRequest for queueing/dispatch.
    std::shared_ptr<IRequest> clone() const override {
        return std::make_shared<MCRequest>(*this);
    }

    /// Assigns the correlation id this request reports when it completes. 0 means uncorrelated:
    /// the request produces no completion at all.
    void setCorrelationId(quint64 id) { m_correlation_id = id; }
    /// @return the correlation id, or 0 for uncorrelated traffic (polling reads, the
    /// comm-active heartbeat).
    quint64 correlationId() const { return m_correlation_id; }
    /// @return true if this request still owes a completion — correlated and not yet resolved.
    bool needsResolution() const { return m_correlation_id != 0 && !m_resolved; }
    /// Marks the request resolved so no later path can emit a second completion for it.
    ///
    /// The "exactly once" guarantee lives HERE, on the request object, rather than in a map of
    /// outstanding writes keyed by id. That is deliberate: a pending map would be written by
    /// pushRequest() (which holds m_mutex) and read by the response/abandon paths (which run on
    /// the polling call chain), and onSetCommActiveDevice() already pushes to the same queue
    /// **without** the mutex, documented as safe only because it shares that call path. A flag
    /// that travels with the request never crosses that boundary.
    void markResolved() { m_resolved = true; }

public:
    MCRequest::RqType m_type;      ///< Requested operation kind (read/write, bit/word).
    char m_device_type;            ///< PLC device/register type letter (e.g. 'D', 'M', 'X', 'Y').
    int m_start_address;           ///< Start address within the device_type's register range.
    int m_amount;                  ///< Number of registers/bits covered by the request.
    QList<quint16> m_value;        ///< Encoded write payload (words), built by the buildWriteData_* helpers.

private:
    bool is_config = false;   ///< True once a constructor has produced a valid request; backs isValid().
    quint64 m_correlation_id{0};  ///< Completion id; 0 = uncorrelated, reports nothing.
    bool m_resolved{false};       ///< True once a completion has been emitted for this request.
};

} // namespace vc::device

/// Required now that requestFinished(McResult) is actually emitted (Phase 9 / E1). The signal was
/// declared and never fired, so the missing registration cost nothing and nobody noticed; without
/// it a queued connection — which is the only kind PlcRunner makes — silently drops the argument.
Q_DECLARE_METATYPE(vc::device::McResult)

#endif // MC_REQUEST_H
