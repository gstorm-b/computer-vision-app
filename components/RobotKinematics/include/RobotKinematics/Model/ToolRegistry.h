#pragma once

#include <RobotKinematics/Core/Ids.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Model/RobotModelConfig.h>

#include <string>
#include <vector>

namespace RobotKinematics {

/// Lookup registry for tool/TCP definitions with a configurable default tool.
class ToolRegistry {
public:
    /// Constructs an empty registry with no tools and no default.
    ToolRegistry() = default;

    /// Builds a registry from a SerialRobotConfig: adds every tool in `config.tools` and
    /// sets the default to `config.defaultToolId` (may be empty, meaning no default).
    static ToolRegistry fromConfig(const SerialRobotConfig& config)
    {
        ToolRegistry registry;
        for (const Tool& tool : config.tools) {
            registry.add(tool);
        }
        registry.setDefault(ToolId{config.defaultToolId});
        return registry;
    }

    /// Appends `tool` to the registry (no uniqueness check; later entries with a
    /// duplicate id shadow earlier ones since lookup returns the first match).
    void add(const Tool& tool) { tools_.push_back(tool); }
    /// Sets which tool id `getDefault()` resolves to.
    void setDefault(const ToolId& id) { defaultId_ = id.value; }

    /// True if a tool with the given id has been added.
    bool contains(const ToolId& id) const { return find(id) != nullptr; }

    /// Looks up a tool by id.
    /// @return success with a copy of the matching Tool, or failure with
    /// KinematicsStatus::ToolNotFound when no tool with that id was added.
    Result<Tool> get(const ToolId& id) const
    {
        if (const Tool* tool = find(id)) {
            return Result<Tool>::success(*tool);
        }
        return Result<Tool>::failure(KinematicsStatus::ToolNotFound, "tool not found: " + id.value);
    }

    /// Looks up the tool configured via `setDefault`/`fromConfig`.
    /// @return failure with KinematicsStatus::ToolNotFound when no default id was set, or
    /// when the configured default id does not match any added tool.
    Result<Tool> getDefault() const
    {
        if (defaultId_.empty()) {
            return Result<Tool>::failure(KinematicsStatus::ToolNotFound, "no default tool configured");
        }
        return get(ToolId{defaultId_});
    }

    /// Returns all tools added to the registry, in insertion order.
    const std::vector<Tool>& tools() const { return tools_; }
    /// Returns the configured default tool id, or an empty string if none is set.
    const std::string& defaultId() const { return defaultId_; }

private:
    /// Linear search for the first added tool whose id matches `id`.
    /// @return pointer into `tools_`, or nullptr if not found.
    const Tool* find(const ToolId& id) const
    {
        for (const Tool& tool : tools_) {
            if (tool.id == id.value) {
                return &tool;
            }
        }
        return nullptr;
    }

    std::vector<Tool> tools_;    ///< All tools added so far, in insertion order.
    std::string defaultId_;      ///< Id of the default tool, empty when none is configured.
};

} // namespace RobotKinematics
