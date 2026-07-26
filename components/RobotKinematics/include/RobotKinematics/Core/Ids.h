#pragma once

#include <string>
#include <utility>

/// Root namespace for the RobotKinematics library (poses, joint vectors, kinematics
/// solvers, and collision geometry).
namespace RobotKinematics {

/// Strongly-typed identifier for a reference frame (base, flange, or a user frame).
struct FrameId {
    std::string value;  ///< Underlying frame name.

    /// Constructs an empty (unset) frame id.
    FrameId() = default;
    /// Wraps an existing string as a frame id (implicit conversion for convenient literals/returns).
    FrameId(std::string v) : value(std::move(v)) {}
    /// Wraps a C string as a frame id (implicit conversion).
    FrameId(const char* v) : value(v) {}

    /// Returns true if the id holds no name.
    bool empty() const { return value.empty(); }
};

/// Compares two frame ids by their underlying string value.
inline bool operator==(const FrameId& a, const FrameId& b) { return a.value == b.value; }
/// Compares two frame ids for inequality by their underlying string value.
inline bool operator!=(const FrameId& a, const FrameId& b) { return !(a == b); }

/// Strongly-typed identifier for a tool / TCP definition.
struct ToolId {
    std::string value;  ///< Underlying tool name.

    /// Constructs an empty (unset) tool id.
    ToolId() = default;
    /// Wraps an existing string as a tool id (implicit conversion for convenient literals/returns).
    ToolId(std::string v) : value(std::move(v)) {}
    /// Wraps a C string as a tool id (implicit conversion).
    ToolId(const char* v) : value(v) {}

    /// Returns true if the id holds no name.
    bool empty() const { return value.empty(); }
};

/// Compares two tool ids by their underlying string value.
inline bool operator==(const ToolId& a, const ToolId& b) { return a.value == b.value; }
/// Compares two tool ids for inequality by their underlying string value.
inline bool operator!=(const ToolId& a, const ToolId& b) { return !(a == b); }

} // namespace RobotKinematics
