#ifndef MC_FAME_3E_H
#define MC_FAME_3E_H

/**
 * @file mc_fame_3e.h
 * @brief Concrete MC-protocol frame codec for the 3E frame (Frame3E).
 */

#include "device/plc/mc_frame_abstract.h"
#include "device/plc/mc_request.h"
#include "device/plc/mc_context_3e.h"

namespace vc::device {

/**
 * @class Frame3E
 * @brief Concrete MC-protocol frame codec for the 3E frame: builds/parses the binary 3E
 *        read/write (bit and word) request and response frames described by Context_Mc3E.
 */
class Frame3E : public MCFrameAbstract {
public:
    /// Constructs the frame codec with an empty last-error string.
    Frame3E();

    /**
     * @brief Builds a 3E request frame for `request` (dispatches by MCRequest::RqType to the
     *        matching build_* helper). For ReadBit requests of more than 8 bits on an X/Y/M device,
     *        transparently switches to a word-aligned read via build_read_word.
     * @param[in]  request the read/write request to encode
     * @param[in]  ctx the frame context (addressing + timing parameters) for the target frame type
     * @param[out] data output buffer; overwritten with the encoded frame on success
     * @return RequestFrameOK on success, RequestFrameError/ObjectError on failure
     */
    FrameReturnCode makeSendFrame(vc::device::MCRequest *request, McContext* ctx, QByteArray &data) override;
    /**
     * @brief Parses a 3E response frame for `request` (dispatches by MCRequest::RqType to the
     *        matching parse_* helper).
     * @param[in]     request the original read/write request the response corresponds to
     * @param[in]     ctx the frame context used to resolve the device map
     * @param[in,out] data the raw bytes received so far
     * @return ResponseOk on success; WaitingReceive if fewer than 9 bytes have arrived yet;
     *         ResponseError/ResponseInvalid on a bad response; ObjectError if request/ctx are null
     */
    FrameReturnCode parseReceiveFrame(vc::device::MCRequest *request, McContext* ctx, QByteArray &data) override;
    /// Returns the description of the most recent build/parse error recorded in m_last_error.
    QString lastErrorDescription() override;

private:
    /// Reads the 3E end-code at the fixed status offset and records it via m_last_error;
    /// true when the end code is 0x0000 (success).
    bool checkErrorStatus(QByteArray &data);
    /**
     * @brief Wraps `data` in the full 3E send envelope (sub-header, network/PC/module-IO/station
     *        numbers, request length, monitoring time) and replaces `data` with the complete frame.
     * @param[in]     ctx frame context supplying the envelope fields (addressing + timing)
     * @param[in,out] byte in: the request payload; out: the complete frame ready to send
     */
    void make_send_data(Context_Mc3E* ctx, QByteArray &byte);

    /**
     * @brief Builds a 3E bit-read request frame (command 0x0401/0x0001).
     * @param[in]  device the device type letter (e.g. 'X', 'Y', 'M')
     * @param[in]  start the starting device address
     * @param[in]  amount the number of bits to read
     * @param[in]  ctx frame context used to build the send envelope
     * @param[out] data output buffer; overwritten with the encoded frame
     * @return always RequestFrameOK
     */
    FrameReturnCode build_read_bit(char &device, int &start, int &amount, Context_Mc3E *ctx, QByteArray &data);
    /**
     * @brief Builds a 3E bit-write request frame (command 0x1401/0x0001), packing pairs of
     *        values from request->m_value into single bytes.
     * @param[in]  request the write request
     * @param[in]  ctx frame context used to build the send envelope
     * @param[out] data output buffer; overwritten with the encoded frame
     * @return always RequestFrameOK
     */
    FrameReturnCode build_write_bit(vc::device::MCRequest *request, Context_Mc3E *ctx, QByteArray &data);

    /**
     * @brief Builds a 3E word-read request frame (command 0x0401/0x0000).
     * @param[in]  device the device type letter (e.g. 'D')
     * @param[in]  start the starting device address
     * @param[in]  amount the number of words to read
     * @param[in]  ctx frame context used to build the send envelope
     * @param[out] data output buffer; overwritten with the encoded frame
     * @return always RequestFrameOK
     */
    FrameReturnCode build_read_word(char &device, int &start, int &amount, Context_Mc3E *ctx, QByteArray &data);
    /**
     * @brief Builds a 3E word-write request frame (command 0x1401/0x0000), appending each value
     *        as a little-endian 16-bit word.
     * @param[in]  request the write request
     * @param[in]  ctx frame context used to build the send envelope
     * @param[out] data output buffer; overwritten with the encoded frame
     * @return always RequestFrameOK
     */
    FrameReturnCode build_write_word(vc::device::MCRequest *request, Context_Mc3E *ctx, QByteArray &data);

    /**
     * @brief Parses a 3E bit-read response for reads of ≤8 bits; stores decoded M-device values
     *        into ctx's device map.
     * @param[in]     request the original read-bit request
     * @param[in]     ctx frame context; supplies the device map
     * @param[in,out] data the received bytes so far
     * @return ResponseOk on success; WaitingReceive/ResponseInvalid/ResponseError on failure
     */
    FrameReturnCode parse_read_bit(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data);
    /**
     * @brief Parses a 3E bit-read response for reads of >8 bits (sent as word-aligned read).
     * @param[in]     request the original read-bit request
     * @param[in]     ctx frame context; supplies the device map
     * @param[in,out] data the received bytes so far
     * @return ResponseOk on success; WaitingReceive/ResponseInvalid/ResponseError on failure
     */
    FrameReturnCode parse_read_bit_from_word(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data);

    /**
     * @brief Parses a 3E word-read response; stores decoded D-device values into ctx's device map.
     * @param[in]     request the original read-word request
     * @param[in]     ctx frame context; supplies the device map
     * @param[in,out] data the received bytes so far
     * @return ResponseOk on success; WaitingReceive/ResponseInvalid/ResponseError on failure
     */
    FrameReturnCode parse_read_word(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data);

    /**
     * @brief Parses a response to any write request (bit or word); only checks the end code.
     * @param[in]     request unused (kept for uniform signature)
     * @param[in]     ctx unused (kept for uniform signature)
     * @param[in,out] data the received bytes so far; must be at least 11 bytes
     * @return ResponseOk on success; ResponseInvalid if <11 bytes; ResponseError on bad end code
     */
    FrameReturnCode parse_write(vc::device::MCRequest *request, Context_Mc3E* ctx, QByteArray &data);

private:
    QString m_last_error;  ///< Description of the most recent build/parse error.
    int m_total_bit_word_len;  ///< Word-read length (ceil(amount/16)) computed by makeSendFrame()
                               ///< when a >8-bit X/Y/M bit-read is redirected through build_read_word().
};

}

#endif // MC_FAME_3E_H
