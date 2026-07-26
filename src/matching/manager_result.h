#ifndef MANAGER_RESULT_H
#define MANAGER_RESULT_H

#include <string>

/// Vision/matching module: ManagerResult, a lightweight ok/error result type returned by
/// pattern/group manager mutation operations.
namespace mtc {

/// Lightweight ok/error result: `ok` reports success, `error` carries a human-readable
/// failure message when `ok` is false.
struct ManagerResult {
    bool ok = true;         ///< True on success; false when the operation failed.
    std::wstring error;     ///< Failure message; empty when `ok` is true.

    /// Default-constructs a successful result (ok = true, empty error).
    ManagerResult() = default;

    /// Constructs a result with an explicit success flag and optional error message.
    explicit ManagerResult(bool ok, const std::wstring &error = {})
        : ok(ok), error(error) {}

    /// Returns a successful result.
    static ManagerResult success()
    { return ManagerResult{true}; }

    /// Returns a failed result carrying `reason` as the error message.
    static ManagerResult fail(const std::wstring &reason)
    { return ManagerResult{false, reason}; }

    /// Implicit bool — lets you write:  if (!result) { ... }
    explicit operator bool() const { return ok; }
};

}

#endif // MANAGER_RESULT_H
