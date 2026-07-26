#ifndef MC_FRAME_ABSTRACT_H
#define MC_FRAME_ABSTRACT_H

#include <QByteArray>
#include "device/plc/mc_context.h"
#include "device/plc/mc_request.h"

/// Device-family classes for the MC (Mitsubishi) protocol PLC integration.
namespace vc::device {

/// Abstract MC-protocol frame codec: builds outgoing request frames and parses incoming
/// response frames for a specific MC frame variant (1E/3E/1C/3C). Concrete subclasses (e.g.
/// Frame3E) implement the actual binary/ASCII encoding.
class MCFrameAbstract {
public:
    /// Outcome of a makeSendFrame()/parseReceiveFrame() call.
    enum FrameReturnCode {
        RequestFrameOK,     ///< makeSendFrame() built the request frame successfully.
        RequestFrameError,  ///< makeSendFrame() failed to build the request frame.
        WaitingReceive,     ///< parseReceiveFrame() needs more bytes; the frame is not yet complete.
        ResponseOk,         ///< parseReceiveFrame() parsed a valid, successful response frame.
        ResponseError,      ///< parseReceiveFrame() parsed a response frame reporting an error end-code.
        ResponseInvalid,    ///< parseReceiveFrame() found the response frame malformed/unrecognized.
        ObjectError         ///< The request/context passed in was null or otherwise unusable.
    };

    /// Virtual destructor so concrete frame codecs can be destroyed through a MCFrameAbstract pointer.
    virtual ~MCFrameAbstract() = default;

    /// Builds the outgoing request frame for `request` into `byte`.
    /// @param request the read/write request to encode
    /// @param ctx the frame context (addressing + timing parameters) for the target frame type
    /// @param byte output buffer; overwritten with the encoded frame on success
    /// @return RequestFrameOK on success, RequestFrameError/ObjectError on failure
    virtual FrameReturnCode makeSendFrame(vc::device::MCRequest *request, McContext* ctx, QByteArray &byte) = 0;

    /// Parses a received response frame in `byte` against the outstanding `request`.
    /// @param request the request the response is expected to answer
    /// @param ctx the frame context (addressing + timing parameters) for the target frame type
    /// @param byte input buffer of bytes received so far; implementations may accumulate partial
    /// reads across calls until a full frame is available
    /// @return ResponseOk/ResponseError/ResponseInvalid once a full frame is parsed, WaitingReceive
    /// if more bytes are needed, ObjectError on invalid arguments
    virtual FrameReturnCode parseReceiveFrame(vc::device::MCRequest *request, McContext* ctx, QByteArray &byte) = 0;

    /// Returns a human-readable description of the most recent build/parse failure.
    virtual QString lastErrorDescription() = 0;
};

}

#endif // MC_FRAME_ABSTRACT_H
