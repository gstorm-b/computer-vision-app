#ifndef IREQUEST_H
#define IREQUEST_H

/**
 * @file irequest.h
 * @brief Device abstraction layer: the abstract request contract devices process via IDevice::pushRequest().
 */

#include <memory>

namespace vc::device {

/**
 * @enum RequestType
 * @brief Identifies which device-family protocol a request targets.
 */
enum RequestType {
    Request_MC,
    Request_PLC,
    Request_VisionOutput
};

/**
 * @class IRequest
 * @brief Abstract request object pushed to a device via IDevice::pushRequest(); concrete requests
 *        carry protocol-specific payload/addressing data.
 */
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
