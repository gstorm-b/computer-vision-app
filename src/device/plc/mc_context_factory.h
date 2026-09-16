#ifndef MC_CONTEXT_FACTORY_H
#define MC_CONTEXT_FACTORY_H

/**
 * @file mc_context_factory.h
 * @brief Factory helpers for allocating MC-protocol frame context instances.
 */

#include "mc_context_1c.h"
#include "mc_context_3c.h"
#include "mc_context_3e.h"

namespace vc::device {

namespace Factory {

/**
 * @brief Allocates the McContext subclass matching `frame_type`.
 *
 * Frame_1E is still unimplemented and returns nullptr; every caller treats a null context as
 * "unsupported frame type" and refuses to connect, which is the intended behaviour for it.
 *
 * @param[in] frame_type the MC frame variant to build a context for
 * @param[in] data_code data encoding (binary/ASCII) passed to the created context. The
 *        computer-link frames (1C/3C) are ASCII on the wire regardless of this argument's
 *        value, so they are constructed with their own default rather than with `data_code`.
 * @return a shared context for 1C, 3C and 3E; nullptr for 1E and anything unrecognized
 */
[[maybe_unused]] static std::shared_ptr<McContext> contextFactory(McFrameType frame_type, McDataCode data_code = McDataCode::Binary) {
    switch (frame_type) {
    case McFrameType::Frame_1C:
        return std::make_shared<Context_Mc1C>();
    case McFrameType::Frame_1E:
        return nullptr;
    case McFrameType::Frame_3E:
        return std::make_shared<Context_Mc3E>(data_code);
    case McFrameType::Frame_3C:
        return std::make_shared<Context_Mc3C>();
    default:
        return nullptr;
    }
}

}

}


#endif // MC_CONTEXT_FACTORY_H
