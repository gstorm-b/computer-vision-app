#ifndef MC_FAME_3E_H
#define MC_FAME_3E_H

#include "device/plc/mc_frame_abstract.h"
#include "device/plc/mc_request.h"
#include "device/plc/mc_context_3e.h"

/// PLC device family (config, protocol devices, and MC-protocol support types).
namespace vc::device {

/// Concrete MC-protocol frame codec for the 3E frame: builds/parses the binary 3E
/// read/write (bit and word) request and response frames described by Context_Mc3E.
class Frame3E : public MCFrameAbstract {
public:
    /// Constructs the frame codec with an empty last-error string.
    Frame3E();

    /// Builds a 3E request frame for `request` (dispatches by MCRequest::RqType to the
    /// matching build_* helper). For ReadBit requests of more than 8 bits on an X/Y/M device,
    /// transparently switches to a word-aligned read via build_read_word and records the
    /// equivalent word length in m_total_bit_word_len.
    /// @param request the read/write request to encode
    /// @param ctx the frame context (addressing + timing parameters) for the target frame type
    /// @param data output buffer; overwritten with the encoded frame on success
    /// @return RequestFrameOK on success, RequestFrameError/ObjectError on failure
    FrameReturnCode makeSendFrame(vc::device::MCRequest *request, McContext* ctx, QByteArray &data) override;
    /// Parses a 3E response frame for `request` (dispatches by MCRequest::RqType to the matching
    /// parse_* helper; ReadBit further routes to parse_read_bit_from_word when more than 8 bits of
    /// an X/Y/M device were requested, mirroring the word-aligned build in makeSendFrame).
    /// @param request the original read/write request the response corresponds to
    /// @param ctx the frame context used to resolve the device map that decoded values are written to
    /// @param data the raw bytes received so far
    /// @return ResponseOk on success; WaitingReceive if fewer than 9 bytes have arrived yet;
    ///         ResponseError/ResponseInvalid on a bad response; ObjectError if request/ctx are null
    FrameReturnCode parseReceiveFrame(vc::device::MCRequest *request, McContext* ctx, QByteArray &data) override;
    /// Returns the description of the most recent build/parse error recorded in m_last_error.
    QString lastErrorDescription() override;

private:
    /// Reads the 3E end-code at the fixed status offset and records it via m_last_error;
    /// true when the end code is 0x0000 (success).
    bool checkErrorStatus(QByteArray &data);
    /// Wraps `data` (an already-built request payload) in the full 3E send envelope — sub-header,
    /// network, PC, destination module IO number, station number, request length, and monitoring
    /// time, all read from `ctx` — then replaces `data` with the resulting complete frame.
    /// @param ctx frame context supplying the envelope fields (addressing + timing)
    /// @param byte in: the request payload; out: the complete frame ready to send
    void make_send_data(Context_Mc3E* ctx, QByteArray &byte);

    /// Builds a 3E bit-read request frame (command 0x0401/0x0001) for `amount` bits of `device`
    /// starting at `start`.
    /// @param device the device type letter (e.g. 'X', 'Y', 'M')
    /// @param start the starting device address
    /// @param amount the number of bits to read
    /// @param ctx frame context used to build the send envelope
    /// @param data output buffer; overwritten with the encoded frame
    /// @return always RequestFrameOK
    FrameReturnCode build_read_bit(char &device, int &start, int &amount, Context_Mc3E *ctx, QByteArray &data);
    /// Builds a 3E bit-write request frame (command 0x1401/0x0001) for `request`, packing each
    /// pair of values from request->m_value into a single byte (the first value in bit 4, the
    /// second in bit 0; a trailing odd value gets its own byte with bit 0 unset).
    /// @param request the write request; the caller is expected to have set m_amount to
    ///        request->m_value.size() before calling
    /// @param ctx frame context used to build the send envelope
    /// @param data output buffer; overwritten with the encoded frame
    /// @return always RequestFrameOK
    FrameReturnCode build_write_bit(vc::device::MCRequest *request, Context_Mc3E *ctx, QByteArray &data);

    /// Builds a 3E word-read request frame (command 0x0401/0x0000) for `amount` words of `device`
    /// starting at `start`.
    /// @param device the device type letter (e.g. 'D')
    /// @param start the starting device address
    /// @param amount the number of words to read
    /// @param ctx frame context used to build the send envelope
    /// @param data output buffer; overwritten with the encoded frame
    /// @return always RequestFrameOK
    FrameReturnCode build_read_word(char &device, int &start, int &amount, Context_Mc3E *ctx, QByteArray &data);
    /// Builds a 3E word-write request frame (command 0x1401/0x0000) for `request`, appending each
    /// value in request->m_value as a little-endian 16-bit word.
    /// @param request the write request; the caller is expected to have set m_amount to
    ///        request->m_value.size() before calling
    /// @param ctx frame context used to build the send envelope
    /// @param data output buffer; overwritten with the encoded frame
    /// @return always RequestFrameOK
    FrameReturnCode build_write_word(vc::device::MCRequest *request, Context_Mc3E *ctx, QByteArray &data);

    /// Parses a 3E bit-read response for reads of 8 or fewer bits per device: after validating the
    /// end code, unpacks each response byte into up to two bit values (the 0x10 nibble bit, then
    /// the 0x01 bit) until request->m_amount values are collected, then — only for device_type
    /// 'M' — stores them into ctx's device map starting at request->m_start_address.
    /// @param request the original read-bit request (device type, start address, amount)
    /// @param ctx frame context; supplies the device map that decoded bit values are written into
    /// @param data the received bytes so far
    /// @return ResponseOk on success; WaitingReceive if the full frame hasn't arrived yet;
    ///         ResponseInvalid if data is under 9 bytes; ResponseError if the end code indicates failure
    FrameReturnCode parse_read_bit(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data);
    /// Parses a 3E bit-read response for reads of more than 8 bits from an X/Y/M device (the
    /// request was sent word-aligned via build_read_word): after validating the end code, unpacks
    /// each response byte into 8 bit values (LSB first), trims the result to request->m_amount,
    /// then — only for device_type 'M' — stores them into ctx's device map starting at
    /// request->m_start_address.
    /// @param request the original read-bit request (device type, start address, amount)
    /// @param ctx frame context; supplies the device map that decoded bit values are written into
    /// @param data the received bytes so far
    /// @return ResponseOk on success; WaitingReceive if the full frame hasn't arrived yet;
    ///         ResponseInvalid if data is under 9 bytes; ResponseError if the end code indicates failure
    FrameReturnCode parse_read_bit_from_word(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data);

    /// Parses a 3E word-read response: after validating the end code, decodes each little-endian
    /// 16-bit word from the response payload, trims the result to request->m_amount, then — only
    /// for device_type 'D' — stores them into ctx's device map starting at request->m_start_address.
    /// @param request the original read-word request (device type, start address, amount)
    /// @param ctx frame context; supplies the device map that decoded word values are written into
    /// @param data the received bytes so far
    /// @return ResponseOk on success; WaitingReceive if the full frame hasn't arrived yet;
    ///         ResponseInvalid if data is under 9 bytes; ResponseError if the end code indicates failure
    FrameReturnCode parse_read_word(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data);

    /// Parses a response to any write request (bit or word) — both share the same fixed-length
    /// envelope, so only the end-code check below is needed; `request` and `ctx` are unused,
    /// kept only for a uniform parse_* signature.
    /// @param data the received bytes so far; must be at least 11 bytes (9-byte header + 2-byte
    ///        end code) to avoid misreading a still-arriving frame as a successful response
    /// @return ResponseOk on success; ResponseInvalid if fewer than 11 bytes have arrived;
    ///         ResponseError if the end code indicates failure
    FrameReturnCode parse_write(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data);

private:
    QString m_last_error;  ///< Description of the most recent build/parse error.
    int m_total_bit_word_len;  ///< Word-read length (ceil(amount/16)) computed by makeSendFrame()
                               ///< when a >8-bit X/Y/M bit-read is redirected through build_read_word().
};

}

#endif // MC_FAME_3E_H
