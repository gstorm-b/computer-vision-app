#include "mc_device_map.h"

/// PLC device family (config, protocol devices, and MC-protocol support types).
namespace vc::device {

//// McDeviceRange

/// Constructs an empty range set with has_optimized initialized to false.
McDeviceRange::McDeviceRange() {
    this->has_optimized = false;
}

/// Destructor. Body is intentionally empty; no owned resources to release.
McDeviceRange::~McDeviceRange() {

}

/// Appends `amount` consecutive addresses starting at `address` to
/// subscribed_devices, marks the set as unoptimized, then optimizes
/// immediately if `optimizal` is true.
/// @note the guard below can never reject any single `amount` value (it
/// requires amount < 1 AND amount > MC_MAXIMUM_DEVICE_AMOUNT simultaneously),
/// so out-of-range amounts are not actually filtered out here.
void McDeviceRange::SubscribeDevice(int address, int amount, bool optimizal) {
    if ((amount < 1) && (amount > MC_MAXIMUM_DEVICE_AMOUNT)) {
        return;
    }

    for (int idx=0;idx<amount;idx++) {
        subscribed_devices.push_back(address+idx);
    }
    // none optimized ranges mark
    has_optimized = false;

    if (optimizal) {
        OptimizeRange();
    }
}

/// Sorts and de-duplicates subscribed_devices, then walks the result to build
/// contiguous DeviceRange spans in `ranges`, merging consecutive addresses
/// into a single span. No-op if already optimized or with no subscriptions.
void McDeviceRange::OptimizeRange() {
    if (has_optimized) {
        return;
    }

    if (subscribed_devices.empty()) {
        return;
    }

    std::sort(subscribed_devices.begin(), subscribed_devices.end());
    auto last = std::unique(subscribed_devices.begin(), subscribed_devices.end());
    subscribed_devices.erase(last, subscribed_devices.end());

    // QSet<int> uniqueDevices;
    // for (int device : subscribed_devices) {
    //     uniqueDevices.insert(device);
    // }

    // QVector<int> sortedDevices = uniqueDevices.values().toVector();
    // std::sort(sortedDevices.begin(), sortedDevices.end());
    // subsribed_devices = sortedDevices;

    ranges.clear();
    DeviceRange currentRange = { subscribed_devices[0], subscribed_devices[0] };

    for (int i = 1; i < subscribed_devices.size(); ++i) {
        if (subscribed_devices[i] == currentRange.end + 1) {
            currentRange.end = subscribed_devices[i];
        } else {
            currentRange.amount = currentRange.end - currentRange.start + 1;
            // ranges.append(currentRange);
            ranges.push_back(currentRange);
            currentRange = {subscribed_devices[i], subscribed_devices[i]};
        }
    }
    currentRange.amount = currentRange.end - currentRange.start + 1;
    // ranges.append(currentRange);
    ranges.push_back(currentRange);

    has_optimized = true;
}

//// END McDeviceRange


//// McDeviceMap
/// Constructs an empty device map (all ranges and value maps default-empty).
McDeviceMap::McDeviceMap() {

}

/// Destructor. Body is intentionally empty; McDeviceRange/std::map members
/// clean themselves up.
McDeviceMap::~McDeviceMap() {

}

/// Dispatches to the SubscribeDevice() call of the McDeviceRange matching
/// `device`; unrecognized device letters are silently ignored.
void McDeviceMap::Subscribe_deivce(char device, int address, int amount, bool optimal) {
    switch (device) {
    case 'X':
        x_devices.SubscribeDevice(address, amount, optimal);
        break;
    case 'Y':
        y_devices.SubscribeDevice(address, amount, optimal);
        break;
    case 'M':
        m_devices.SubscribeDevice(address, amount, optimal);
        break;
    case 'D':
        d_devices.SubscribeDevice(address, amount, optimal);
        break;
    }
}

/// Optimizes the X, Y, D, and M device ranges in turn.
void McDeviceMap::OptimizeRanges() {
    x_devices.OptimizeRange();
    y_devices.OptimizeRange();
    d_devices.OptimizeRange();
    m_devices.OptimizeRange();
}

/// Returns the McDeviceRange member matching `device` ('X'/'Y'/'M'/'D'), or
/// nullptr if `device` is not one of those four letters.
McDeviceRange* McDeviceMap::GetDeviceRange(char device) {
    switch (device) {
    case 'X':
        return &x_devices;
    case 'Y':
        return &y_devices;
    case 'M':
        return &m_devices;
    case 'D':
        return &d_devices;
    }
    return nullptr;
}

//// END McDeviceMap


}
