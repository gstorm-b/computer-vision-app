#ifndef MC_DEVICE_MAP_H
#define MC_DEVICE_MAP_H

/**
 * @file mc_device_map.h
 * @brief MC-protocol device subscription ranges and polled register value map.
 */

#include <vector>
#include <QByteArray>
#include <QSet>
#include <map>
#include "device/plc/plc_device.h"

/// Upper bound accepted for a single SubscribeDevice()/Subscribe_deivce() amount.
#define MC_MAXIMUM_DEVICE_AMOUNT   256

namespace vc::device {

/**
 * @class McDeviceRange
 * @brief Tracks the individual device addresses subscribed for one MC device type
 *        (X/Y/M/D) and can coalesce them into contiguous DeviceRange spans so a
 *        poll cycle can read multiple devices with a single MC-protocol command.
 */
class McDeviceRange {
public:
    /// A contiguous, inclusive span of subscribed addresses.
    struct DeviceRange {
        int start;   ///< First address in the span.
        int end;     ///< Last address in the span (inclusive).
        int amount;  ///< Number of addresses in the span (end - start + 1).
    };

    /// Constructs an empty, unoptimized range set.
    McDeviceRange();
    /// Destructor; no owned resources to release.
    ~McDeviceRange();

    /**
     * @brief Appends `amount` consecutive addresses starting at `address` to
     *        subscribed_devices and marks the set as unoptimized.
     * @param[in] address start address
     * @param[in] amount amount of devices
     * @param[in] optimizal if true, calls OptimizeRange() immediately after adding
     * @note The amount guard (`amount < 1 && amount > MC_MAXIMUM_DEVICE_AMOUNT`)
     *       can never be true for a single value, so out-of-range amounts are not
     *       actually rejected here.
     */
    void SubscribeDevice(int address, int amount, bool optimizal = false);

    /// Sorts and de-duplicates subscribed_devices, then groups consecutive
    /// addresses into `ranges` so multiple devices can be queried in a single
    /// command. No-op if already optimized or if there are no subscriptions.
    void OptimizeRange();

    /// @return true once OptimizeRange() has run since the last modification
    /// that reset the flag (SubscribeDevice() clears it again).
    inline const bool IsOptimzed() {
        return this->has_optimized;
    }

    /**
     * @brief Clears subscribed_devices and the computed ranges.
     * @note Does not reset has_optimized.
     */
    void clearRanges() {
        subscribed_devices.clear();
        ranges.clear();
    }

    std::vector<int> subscribed_devices;  ///< Raw (possibly unsorted/duplicated) subscribed addresses.
    std::vector<DeviceRange> ranges;      ///< Contiguous spans computed by OptimizeRange().

private:
    bool has_optimized;  ///< True once `ranges` reflects the current subscribed_devices.
};

/**
 * @class McDeviceMap
 * @brief MC-protocol PlcValueMap: holds the X/Y/M/D device subscription ranges
 *        used to plan poll requests, plus the resulting M (bit) and D (word)
 *        value maps populated by Frame3E's response parsers.
 */
class McDeviceMap : public PlcValueMap {
public:
    /// Constructs an empty device map (all ranges and value maps default-empty).
    McDeviceMap();
    /// Destructor; McDeviceRange/std::map members clean themselves up.
    ~McDeviceMap();

    /// Implements PlcValueMap::clone() via the copy constructor.
    /// @return a new McDeviceMap holding a copy of this map's ranges and values
    std::shared_ptr<PlcValueMap> clone() const {
        return std::make_shared<McDeviceMap>(*this);
    }

    /// Reads a polled `M<addr>` (bit) or `D<addr>` (word) value; see PlcValueMap::valueForTag().
    ///
    /// Only M and D are answered, because only those two are what a task's signal map can bind:
    /// X and Y are subscription ranges the poll planner uses, and no tag names them.
    bool valueForTag(const QString &tag, QVariant *value) const override;

    /**
     * @brief Adds a range of devices of the given type to that type's subscription list.
     * @param[in] device device type in uppercase: 'X', 'Y', 'M', or 'D' (others are ignored)
     * @param[in] address start address
     * @param[in] amount amount of devices
     * @param[in] optimal optimize the target range immediately after adding
     */
    void Subscribe_deivce(char device, int address, int amount, bool optimal = true);

    /// Optimizes the X, Y, M, and D device ranges so each type can be queried
    /// with as few MC-protocol commands as possible.
    void OptimizeRanges();

    /**
     * @brief Retrieves the subscription ranges for a specific device type.
     * @param[in] device device type in uppercase: 'X', 'Y', 'M', or 'D'
     * @return pointer to the matching McDeviceRange, or nullptr if `device` is unrecognized
     */
    McDeviceRange* GetDeviceRange(char device);

    /// Clears all device ranges and the M/D value maps.
    /// @note does not reset each range's has_optimized flag.
    void clearMap() {
        x_devices.clearRanges();
        y_devices.clearRanges();
        m_devices.clearRanges();
        d_devices.clearRanges();

        device_map_m.clear();
        device_map_d.clear();
    }

    McDeviceRange x_devices;  ///< Subscribed ranges for X (input) devices.
    McDeviceRange y_devices;  ///< Subscribed ranges for Y (output) devices.
    McDeviceRange m_devices;  ///< Subscribed ranges for M (internal relay/bit) devices.
    McDeviceRange d_devices;  ///< Subscribed ranges for D (data register/word) devices.

    std::map<int, quint8> device_map_m;  ///< Latest polled bit value per M device address.
    std::map<int, qint16> device_map_d;  ///< Latest polled word value per D device address.
};

}; // namespace vc::device

#endif // MC_DEVICE_MAP_H
