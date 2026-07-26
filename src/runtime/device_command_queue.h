#ifndef DEVICE_COMMAND_QUEUE_H
#define DEVICE_COMMAND_QUEUE_H

#include <QList>
#include <QQueue>

#include "runtime/device_command.h"

namespace vc::runtime {

/// Behavior applied by DeviceCommandQueue::enqueue() when a new command arrives while the
/// runner is busy.
enum class DeviceCommandQueuePolicy {
    RejectWhenBusy,         ///< Reject the new command outright (DeviceCommandResultCode::Busy).
    QueueWhenBusy,          ///< Append the new command to the pending queue (FIFO), if room allows.
    ReplacePendingSameKind, ///< Drop the first pending command of the same kind, then enqueue the new one.
};

/// Result of a single DeviceCommandQueue::enqueue() call: whether the command was accepted,
/// whether it should run immediately, and any rejection detail.
struct DeviceCommandEnqueueDecision {
    bool accepted{false};        ///< True if the command was accepted (either to run now or queued).
    bool shouldRunNow{false};    ///< True if the caller should execute the command immediately (runner was not busy).
    bool replacedPending{false}; ///< True if ReplacePendingSameKind actually removed a pending command of the same kind.
    int pendingCount{0};         ///< Number of commands left pending after this call (only meaningful when queued).
    DeviceCommandResult rejection; ///< Populated with a Rejected result when accepted is false.
};

/// FIFO queue of pending DeviceCommand entries for a single device runner, applying a
/// DeviceCommandQueuePolicy to decide whether/how to accept a new command while busy.
class DeviceCommandQueue {
public:
    /// Constructs an empty queue capped at `maxPending` pending commands.
    explicit DeviceCommandQueue(int maxPending = 16)
        : m_maxPending(maxPending)
    {
    }

    /// Decides how to handle `command` given whether the runner is currently `busy`, per
    /// `policy`. When not busy, the command is accepted to run immediately (nothing is queued).
    /// When busy, applies RejectWhenBusy/QueueWhenBusy/ReplacePendingSameKind, rejecting with
    /// DeviceCommandResultCode::Busy or QueueFull as appropriate.
    /// @param command command being submitted
    /// @param policy how to handle the command if the runner is busy
    /// @param busy whether the runner is currently executing another command
    /// @return the enqueue outcome; check `accepted`/`shouldRunNow`, or `rejection` on failure
    DeviceCommandEnqueueDecision enqueue(const DeviceCommand &command,
                                         DeviceCommandQueuePolicy policy,
                                         bool busy)
    {
        DeviceCommandEnqueueDecision decision;
        if (!busy) {
            decision.accepted = true;
            decision.shouldRunNow = true;
            return decision;
        }

        if (policy == DeviceCommandQueuePolicy::RejectWhenBusy) {
            decision.rejection = DeviceCommandResult::rejected(
                command,
                DeviceCommandResultCode::Busy,
                QStringLiteral("Runner is busy with another command."));
            return decision;
        }

        if (policy == DeviceCommandQueuePolicy::ReplacePendingSameKind) {
            QList<DeviceCommand> rewritten;
            rewritten.reserve(m_pending.size());
            bool replaced = false;
            for (const DeviceCommand &pending : m_pending) {
                if (!replaced && pending.kind == command.kind) {
                    replaced = true;
                    continue;
                }
                rewritten.push_back(pending);
            }
            m_pending.clear();
            for (const DeviceCommand &item : rewritten) {
                m_pending.enqueue(item);
            }
            decision.replacedPending = replaced;
        }

        if (!decision.replacedPending && m_pending.size() >= m_maxPending) {
            decision.rejection = DeviceCommandResult::rejected(
                command,
                DeviceCommandResultCode::QueueFull,
                QStringLiteral("Command queue is full."));
            return decision;
        }

        m_pending.enqueue(command);
        decision.accepted = true;
        decision.pendingCount = m_pending.size();
        return decision;
    }

    /// @return true if there is at least one command waiting in the queue.
    bool hasPending() const
    {
        return !m_pending.isEmpty();
    }

    /// Dequeues and returns the oldest pending command (FIFO order).
    /// @note Caller must check hasPending() first; dequeuing an empty QQueue is undefined.
    DeviceCommand takeNext()
    {
        return m_pending.dequeue();
    }

    /// Discards all pending commands.
    void clear()
    {
        m_pending.clear();
    }

    /// @return the number of commands currently waiting in the queue.
    int pendingCount() const
    {
        return m_pending.size();
    }

private:
    int m_maxPending{16};        ///< Maximum number of commands allowed to wait in m_pending.
    QQueue<DeviceCommand> m_pending; ///< FIFO of commands waiting for the runner to become free.
};

} // namespace vc::runtime

#endif // DEVICE_COMMAND_QUEUE_H
