#pragma once

#include <QObject>

/// QtTest suite covering ToolRegistry and FrameRegistry add/get/default lookup behavior,
/// including the not-found status returned for unknown ids.
class FrameToolTests : public QObject
{
    Q_OBJECT

private slots:
    /// Verifies ToolRegistry::add()/get() round-trip a tool's fields and that setDefault()
    /// plus getDefault() retrieve the designated default tool.
    void toolRegistryAddGetAndDefault();
    /// Verifies ToolRegistry::get() on an unregistered id returns a failed Result with
    /// KinematicsStatus::ToolNotFound.
    void toolRegistryMissingReturnsToolNotFound();
    /// Verifies FrameRegistry::add()/get() round-trip a user frame's parentLinkId and that
    /// contains() reports the frame as present.
    void frameRegistryAddAndGet();
    /// Verifies FrameRegistry::get() on an unregistered id returns a failed Result with
    /// KinematicsStatus::FrameNotFound.
    void frameRegistryMissingReturnsFrameNotFound();
};
