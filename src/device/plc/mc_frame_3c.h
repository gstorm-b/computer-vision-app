#ifndef MC_FRAME_3C_H
#define MC_FRAME_3C_H

/**
 * @file mc_frame_3c.h
 * @brief Concrete MC-protocol frame codec for the 3C computer-link frame (Frame3C).
 */

#include "device/plc/mc_frame_abstract.h"
#include "device/plc/mc_request.h"
#include "device/plc/mc_context_3c.h"

namespace vc::device {

/**
 * @class Frame3C
 * @brief Concrete MC-protocol frame codec for the 3C computer-link frame: builds and parses the
 *        ASCII QnA-compatible read/write request and response frames described by Context_Mc3C.
 *
 * Frame layout, from `reference source/3C_frame/mc_frame_3c.py`:
 *
 *     request : ENQ "F9" route(8) request-data [sum(2)] [CR LF]
 *     response: STX "F9" route(8) data ETX [sum(2)] [CR LF]
 *     error   : NAK "F9" route(8) errcode(4) [sum(2)] [CR LF]
 *
 * where `route` is station(2) network(2) PC(2) self-station(2), all upper-case ASCII hex, and
 * `request-data` is command(4) sub-command(4) device-header address count(4) [values].
 *
 * The device header and address width follow the PLC series: Q/L use `X*` plus a 6-digit
 * address, iQ-R uses `X***` plus an 8-digit address. The sub-command differs the same way,
 * which is why McPlcSeries is a context field and not a build-time constant.
 */
class Frame3C : public MCFrameAbstract {
public:
    /// Constructs the frame codec with an empty last-error string.
    Frame3C();

    /**
     * @brief Builds a 3C request frame for `request`.
     *
     * A bit read of more than 8 points on an X/Y/M device is issued as a word read of
     * ceil(amount / 16) words and unpacked again on receive — the same redirection Frame3E
     * performs, so the polling loop behaves identically whichever frame a project selects.
     *
     * @param[in]  request the read/write request to encode
     * @param[in]  ctx the frame context; must be a Context_Mc3C
     * @param[out] data output buffer; overwritten with the encoded frame on success
     * @return RequestFrameOK on success, RequestFrameError on an unusable request,
     *         ObjectError when `request` or `ctx` is null
     */
    FrameReturnCode makeSendFrame(vc::device::MCRequest *request, McContext *ctx, QByteArray &data) override;

    /**
     * @brief Parses a 3C response frame for `request`, storing read values into the context's
     *        device map.
     * @param[in]     request the original request the response answers
     * @param[in]     ctx the frame context; supplies the access route and the device map
     * @param[in,out] data the raw bytes received so far; not consumed, the caller owns the buffer
     * @return ResponseOk, ResponseError (NAK), ResponseInvalid (malformed, wrong access route,
     *         or bad sum check), WaitingReceive while incomplete, ObjectError on null arguments
     */
    FrameReturnCode parseReceiveFrame(vc::device::MCRequest *request, McContext *ctx, QByteArray &data) override;

    /// Returns the description of the most recent build/parse error recorded in m_last_error.
    QString lastErrorDescription() override;

private:
    /// Appends the frame ID ("F9") and the four access-route numbers to `out`.
    void appendRouteHeader(Context_Mc3C *ctx, QByteArray &out);

    /// Appends the device header for `device` ("M*" for Q/L, "M***" for iQ-R).
    /// @return false when the device letter is not one the protocol addresses here
    bool appendDeviceHeader(Context_Mc3C *ctx, char device, QByteArray &out);

    /// Appends the device address with the width the PLC series requires (6 or 8 decimal digits).
    void appendDeviceAddress(Context_Mc3C *ctx, int address, QByteArray &out);

    /// Appends the command and the series-dependent sub-command for a read or write of bits or
    /// words.
    void appendCommand(Context_Mc3C *ctx, bool write, bool bitUnits, QByteArray &out);

    /// Wraps `requestData` in the 3C envelope (ENQ, frame ID, access route, optional sum check,
    /// optional CR LF) and writes the complete frame to `out`.
    void buildEnvelope(Context_Mc3C *ctx, const QByteArray &requestData, QByteArray &out);

    /**
     * @brief Locates one complete response frame in `data` and validates its frame ID, access
     *        route and sum check.
     * @param[out] control the control code found (STX/ACK/NAK)
     * @param[out] payload the frame's data field, excluding the trailing ETX
     * @param[out] errorCode the NAK error code, when `control` is NAK
     * @return ResponseOk when a complete, well-formed frame was found; WaitingReceive when more
     *         bytes are needed; ResponseInvalid when malformed, misrouted, or badly summed
     */
    FrameReturnCode locateFrame(Context_Mc3C *ctx, const QByteArray &data, quint8 &control,
                                QByteArray &payload, quint16 &errorCode);

    /// Decodes an ASCII bit-read payload ('0'/'1' per device) into the context's M-device map.
    FrameReturnCode parseReadBit(vc::device::MCRequest *request, Context_Mc3C *ctx,
                                 const QByteArray &payload);
    /// Decodes a word-packed bit-read payload (4 hex characters per 16 bits) into the context's
    /// M-device map.
    FrameReturnCode parseReadBitFromWord(vc::device::MCRequest *request, Context_Mc3C *ctx,
                                         const QByteArray &payload);
    /// Decodes an ASCII word-read payload (4 hex characters per word) into the context's
    /// D-device map.
    FrameReturnCode parseReadWord(vc::device::MCRequest *request, Context_Mc3C *ctx,
                                  const QByteArray &payload);

    /// True when `request` is a bit read that must travel as a word read.
    static bool readsBitsAsWords(const vc::device::MCRequest *request);
    /// Number of words a bit read of `amount` points occupies.
    static int bitWordCount(int amount);

private:
    QString m_last_error;  ///< Description of the most recent build/parse error.
};

}

#endif // MC_FRAME_3C_H
