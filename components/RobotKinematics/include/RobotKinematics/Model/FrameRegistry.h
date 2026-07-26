#pragma once

#include <RobotKinematics/Core/Ids.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <vector>

namespace RobotKinematics {

/// Lookup registry for user-defined frames. Base and flange frames are intrinsic to
/// the kinematic chain and are resolved by ForwardKinematics, not stored here.
class FrameRegistry {
public:
    FrameRegistry() = default;

    /// Builds a registry populated with all user frames declared in `config`.
    static FrameRegistry fromConfig(const SerialRobotConfig& config)
    {
        FrameRegistry registry;
        for (const UserFrame& frame : config.frames.userFrames) {
            registry.add(frame);
        }
        return registry;
    }

    /// Appends `frame` to the registry. Does not check for an existing frame with the same id.
    void add(const UserFrame& frame) { frames_.push_back(frame); }

    /// True when a frame with the given id has been registered.
    bool contains(const FrameId& id) const { return find(id) != nullptr; }

    /// Looks up a registered frame by id.
    /// @return the frame on success, or a failure Result with FrameNotFound if no frame matches.
    Result<UserFrame> get(const FrameId& id) const
    {
        if (const UserFrame* frame = find(id)) {
            return Result<UserFrame>::success(*frame);
        }
        return Result<UserFrame>::failure(KinematicsStatus::FrameNotFound, "frame not found: " + id.value);
    }

    /// Returns all registered frames, in registration order.
    const std::vector<UserFrame>& frames() const { return frames_; }

private:
    /// Finds the registered frame matching `id`, or nullptr if none does.
    const UserFrame* find(const FrameId& id) const
    {
        for (const UserFrame& frame : frames_) {
            if (frame.id == id.value) {
                return &frame;
            }
        }
        return nullptr;
    }

    std::vector<UserFrame> frames_; ///< Registered user frames, in registration order.
};

} // namespace RobotKinematics
