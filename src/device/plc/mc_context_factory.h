#ifndef MC_CONTEXT_FACTORY_H
#define MC_CONTEXT_FACTORY_H

#include "mc_context_3e.h"

/// PLC device family (config, protocol devices, and MC-protocol support types).
namespace vc::device {

/// Factory helpers for allocating MC-protocol frame context instances.
namespace Factory {

/// Allocates the McContext subclass matching `frame_type` (only Frame_3E is
/// currently implemented; other frame types return nullptr).
/// @param frame_type the MC frame variant to build a context for
/// @param data_code data encoding (binary/ASCII) passed to the Frame_3E context
/// @return a shared Context_Mc3E for Frame_3E, otherwise nullptr
[[maybe_unused]] static std::shared_ptr<McContext> contextFactory(McFrameType frame_type, McDataCode data_code = McDataCode::Binary) {
    switch (frame_type) {
    case McFrameType::Frame_1C:
        return nullptr;
    case McFrameType::Frame_1E:
        return nullptr;
    case McFrameType::Frame_3E:
        return std::make_shared<Context_Mc3E>(data_code);
    case McFrameType::Frame_3C:
        return nullptr;
    default:
        return nullptr;
    }
}

}

}


#endif // MC_CONTEXT_FACTORY_H
