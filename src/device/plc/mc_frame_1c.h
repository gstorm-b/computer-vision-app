#ifndef MC_FRAME_1C_H
#define MC_FRAME_1C_H

/**
 * @file mc_frame_1c.h
 * @brief Concrete MC-protocol frame codec for the 1C computer-link frame (Frame1C).
 */

#include "device/plc/mc_frame_abstract.h"
#include "device/plc/mc_request.h"
#include "device/plc/mc_context_1c.h"

namespace vc::device {

/**
 * @class Frame1C
 * @brief Concrete MC-protocol frame codec for the 1C computer-link frame: builds and parses the
 *        ASCII BR/BW/WR/WW request and response frames described by Context_Mc1C.
 *
 * Frame layout, from `reference source/1C_frame/fx3communicator.cpp`:
 *
 *     request  : ENQ station(2) plc(2) cmd(2) wait(1) area [sum(2)] [CR LF]
 *     read resp: STX station(2) plc(2) data ETX [sum(2)] [CR LF]
 *     write resp: ACK station(2) plc(2) [CR LF]
 *     error    : NAK station(2) plc(2) errcode(2) [CR LF]
 *
 * Numbers are ASCII: station, PLC number, device count and word values in upper-case
 * hexadecimal, the device address in decimal. The CR LF terminator belongs to format 4;
 * format 1 is the same frame without it. The sum check is present only when the context
 * enables it, and it must match the PLC port's own setting — there is no negotiation.
 */
class Frame1C : public MCFrameAbstract {
public:
    /// Constructs the frame codec with an empty last-error string.
    Frame1C();

    /**
     * @brief Builds a 1C request frame for `request` (BR/WR/BW/WW by MCRequest::RqType).
     * @param[in]  request the read/write request to encode
     * @param[in]  ctx the frame context; must be a Context_Mc1C
     * @param[out] data output buffer; overwritten with the encoded frame on success
     * @return RequestFrameOK on success, RequestFrameError on an unusable request,
     *         ObjectError when `request` or `ctx` is null
     */
    FrameReturnCode makeSendFrame(vc::device::MCRequest *request, McContext *ctx, QByteArray &data) override;

    /**
     * @brief Parses a 1C response frame for `request`, storing read values into the context's
     *        device map.
     * @param[in]     request the original request the response answers
     * @param[in]     ctx the frame context; supplies the station/PLC numbers and the device map
     * @param[in,out] data the raw bytes received so far; not consumed, the caller owns the buffer
     * @return ResponseOk, ResponseError (NAK or a PLC error code), ResponseInvalid (malformed,
     *         wrong station, or bad sum check), WaitingReceive while the frame is incomplete, or
     *         ObjectError when `request` or `ctx` is null
     */
    FrameReturnCode parseReceiveFrame(vc::device::MCRequest *request, McContext *ctx, QByteArray &data) override;

    /// Returns the description of the most recent build/parse error recorded in m_last_error.
    QString lastErrorDescription() override;

    /// Maps a 1C NAK error code to a human-readable description.
    /// @return the description, or "unknown error code" for a value the protocol does not define
    static QString nakErrorDescription(quint8 code);

private:
    /// Appends the two-character command code ("BR"/"BW"/"WR"/"WW") for `type` to `out`.
    /// @return false when the request type has no 1C command
    bool appendCommand(MCRequest::RqType type, QByteArray &out);

    /// Appends the area field: device letter, 4-digit decimal address and 2-digit hex count.
    void appendArea(QByteArray &out, char device, int address, int amount);

    /// Wraps `area` in the 1C envelope (ENQ, station, PLC, command, wait time, optional sum
    /// check, optional CR LF) and writes the complete frame to `out`.
    void buildEnvelope(Context_Mc1C *ctx, const QByteArray &command, const QByteArray &area,
                       QByteArray &out);

    /**
     * @brief Locates one complete response frame in `data`.
     * @param[in]  ctx the context supplying the format and sum-check settings
     * @param[out] control the control code found (STX/ACK/NAK)
     * @param[out] start index of the control code within `data`
     * @param[out] payload the frame's data field (between the station/PLC header and ETX);
     *             empty for ACK and NAK frames
     * @param[out] errorCode the NAK error code, when `control` is NAK
     * @return ResponseOk when a complete, well-formed frame was found; WaitingReceive when more
     *         bytes are needed; ResponseInvalid when the frame is malformed, addressed to another
     *         station, or fails its sum check
     */
    FrameReturnCode locateFrame(Context_Mc1C *ctx, const QByteArray &data, quint8 &control,
                                int &start, QByteArray &payload, quint8 &errorCode);

    /// Decodes an ASCII bit-read payload ('0'/'1' per device) into the context's M-device map.
    FrameReturnCode parseReadBit(vc::device::MCRequest *request, Context_Mc1C *ctx,
                                 const QByteArray &payload);
    /// Decodes an ASCII word-read payload (4 hex characters per word) into the context's
    /// D-device map.
    FrameReturnCode parseReadWord(vc::device::MCRequest *request, Context_Mc1C *ctx,
                                  const QByteArray &payload);

private:
    QString m_last_error;  ///< Description of the most recent build/parse error.
};

}

#endif // MC_FRAME_1C_H
