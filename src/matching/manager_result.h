#ifndef MANAGER_RESULT_H
#define MANAGER_RESULT_H

#include <string>

/**
 * @file manager_result.h
 * @brief ManagerResult — lightweight ok/error result returned by pattern/group manager operations.
 */

namespace mtc {

/**
 * @struct ManagerResult
 * @brief Lightweight ok/error result: `ok` reports success, `error` carries a human-readable
 *        failure message when `ok` is false.
 */
struct ManagerResult {
    bool ok = true;         ///< True on success; false when the operation failed.
    std::wstring error;     ///< Failure message; empty when `ok` is true.

    /// Default-constructs a successful result (ok = true, empty error).
    ManagerResult() = default;

    /**
     * @brief Constructs a result with an explicit success flag and optional error message.
     * @param[in] ok    true for success, false for failure
     * @param[in] error failure description; empty by default
     */
    explicit ManagerResult(bool ok, const std::wstring &error = {})
        : ok(ok), error(error) {}

    /// Returns a successful result.
    static ManagerResult success()
    { return ManagerResult{true}; }

    /**
     * @brief Returns a failed result carrying `reason` as the error message.
     * @param[in] reason failure description stored in the error field
     * @return ManagerResult with ok = false and error = reason
     */
    static ManagerResult fail(const std::wstring &reason)
    { return ManagerResult{false, reason}; }

    /// Implicit bool — lets you write:  if (!result) { ... }
    explicit operator bool() const { return ok; }
};

}

#endif // MANAGER_RESULT_H
