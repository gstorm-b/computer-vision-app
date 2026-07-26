#ifndef PLC_VALUE_H
#define PLC_VALUE_H

#include <vector>

/// Generic typed value-container abstraction for PLC point values.
namespace vc::device {

/// Runtime tag for the element type held by a ValueContainer, as reported by
/// ValueContainerAbstract::type().
enum class PLCValueType {
    Int,
    Float,
    String
};

/// Type-erased interface over a ValueContainer<T> so heterogeneous containers
/// can be stored/queried without knowing `T` at the call site.
class ValueContainerAbstract {
public:
    virtual ~ValueContainerAbstract() = default;
    /// @return the runtime type tag for the concrete element type `T`.
    virtual PLCValueType type() const = 0;
    /// Prints all held values (implementation-defined format/destination).
    virtual void printAll() const = 0;
};

/// Typed, append-only list of PLC point values of type `T` (int, float, or
/// treated as String for any other type).
template<typename T>
class ValueContainer : public ValueContainerAbstract {
public:
    /// Appends `value` to the container.
    void add(const T& value) {
        data.push_back(value);
    }

    /// @return all values added so far, in insertion order.
    const std::vector<T>& values() const {
        return data;
    }

    /// @return PLCValueType::Int/Float for T = int/float, PLCValueType::String otherwise.
    PLCValueType type() const override {
        if constexpr (std::is_same_v<T, int>) return PLCValueType::Int;
        else if constexpr (std::is_same_v<T, float>) return PLCValueType::Float;
        else return PLCValueType::String;
    }

private:
    std::vector<T> data;   ///< Backing storage for the appended values.
};

} // namespace vc::device

#endif // PLC_VALUE_H
