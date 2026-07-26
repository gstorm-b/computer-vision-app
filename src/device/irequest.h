#ifndef IREQUEST_H
#define IREQUEST_H

#include <memory>

/// Device abstraction layer: the abstract request contract devices process via IDevice::pushRequest().
namespace vc::device {

/// Identifies which device-family protocol a request targets.
enum RequestType {
    Request_MC,
    Request_PLC,
    Request_VisionOutput
};

/// Abstract request object pushed to a device via IDevice::pushRequest(); concrete requests
/// carry protocol-specific payload/addressing data.
class IRequest {
public:
    /// Default virtual destructor; concrete requests own no extra resources here.
    virtual ~IRequest() = default;
    /// Returns the RequestType identifying which device-family protocol this request targets.
    virtual RequestType type() const = 0;
    /// Returns a heap-allocated copy of this request (caller owns the returned instance).
    virtual std::shared_ptr<IRequest> clone() const = 0;
};

}

#endif // IREQUEST_H
