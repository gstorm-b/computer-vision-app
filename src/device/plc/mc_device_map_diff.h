#ifndef MC_DEVICE_MAP_DIFF_H
#define MC_DEVICE_MAP_DIFF_H

/**
 * @file mc_device_map_diff.h
 * @brief Pure shadow-map diff for the MC protocol's polled M/D device maps.
 *
 * Header-only and device-free on purpose. `mc_frame_test.pro` compiles four `src/` files and
 * `mc_protocol_device.cpp` is not one of them, and the device builds its own transport with no
 * injection point — so nothing in this repository has ever executed a `McProtocolDevice` method.
 * Keeping the diff here is what makes it testable at all.
 */

#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

#include <map>

namespace vc::device {

/**
 * @struct McMapChange
 * @brief One address whose polled value differs from the shadow copy.
 */
struct McMapChange {
    int address{0};         ///< Device address (the number after the M/D prefix).
    int previousValue{0};   ///< Value held in the shadow map; meaningless when hasPrevious is false.
    int currentValue{0};    ///< Freshly polled value.
    /// False when the address was absent from the shadow entirely — a newly visible address rather
    /// than a changed one. The caller must not emit a per-address changed signal for these, because
    /// there is no honest previous value to report.
    bool hasPrevious{true};
};

/**
 * @struct McDeviceMapDiff
 * @brief Everything one polling round changed, ready for the device to publish.
 */
struct McDeviceMapDiff {
    /// Tag-keyed values for PlcDevice::valueChanged(), covering **both** areas.
    QMap<QString, QVariant> changedValues;
    QList<McMapChange> mChanges;  ///< Per-address M changes, in ascending address order.
    QList<McMapChange> dChanges;  ///< Per-address D changes, in ascending address order.
};

/// Formats an address the way the signal mapper expects: prefix + 4 zero-padded digits.
inline QString mcDeviceTag(QChar prefix, int address)
{
    return QStringLiteral("%1%2").arg(prefix).arg(address, 4, 10, QLatin1Char('0'));
}

/**
 * @brief Diffs both polled maps against their shadows, updating the shadows in place.
 *
 * @param[in]     liveM             freshly polled M (bit) values, keyed by address.
 * @param[in,out] shadowM           last-known M values; updated to match `liveM`.
 * @param[in]     liveD             freshly polled D (word) values, keyed by address.
 * @param[in,out] shadowD           last-known D values; updated to match `liveD`.
 * @param[in]     suppressReporting when true, shadows are still updated but nothing is reported —
 *                this is the first-polling-round behaviour, preserved deliberately.
 * @return every change from both areas, accumulated together.
 *
 * @note **Both areas are always processed.** The two areas are independent: a station with no D
 *       ranges configured is a normal configuration, not a reason to stop.
 */
McDeviceMapDiff diffMcDeviceMaps(const std::map<int, quint8> &liveM,
                                 std::map<int, quint8> &shadowM,
                                 const std::map<int, qint16> &liveD,
                                 std::map<int, qint16> &shadowD,
                                 bool suppressReporting);

// ── Implementation ────────────────────────────────────────────────────────────

namespace detail {

/// Diffs one area by KEY, seeding any address the shadow has never seen.
///
/// Keyed lookup, not a lockstep iterator pair. The pairwise form the device used assumed both maps
/// carried identical key sets, which nothing guarantees: the shadow is only ever added to, never
/// erased, so once the polled range shrinks the two walk out of step and every subsequent address
/// is compared against a neighbour's value.
template <typename ValueT>
void diffOneArea(const std::map<int, ValueT> &live,
                 std::map<int, ValueT> &shadow,
                 QChar prefix,
                 bool suppressReporting,
                 QMap<QString, QVariant> *changedValues,
                 QList<McMapChange> *changes)
{
    for (typename std::map<int, ValueT>::const_iterator it = live.begin(); it != live.end(); ++it) {
        const int address = it->first;
        const ValueT current = it->second;

        typename std::map<int, ValueT>::iterator shadowIt = shadow.find(address);
        const bool hasPrevious = (shadowIt != shadow.end());
        if (hasPrevious && shadowIt->second == current) {
            continue;
        }

        if (!suppressReporting) {
            changedValues->insert(mcDeviceTag(prefix, address), current);
            McMapChange change;
            change.address = address;
            change.previousValue = hasPrevious ? static_cast<int>(shadowIt->second) : 0;
            change.currentValue = static_cast<int>(current);
            change.hasPrevious = hasPrevious;
            changes->append(change);
        }

        if (hasPrevious) {
            shadowIt->second = current;
        } else {
            shadow.insert(std::make_pair(address, current));
        }
    }
}

} // namespace detail

inline McDeviceMapDiff diffMcDeviceMaps(const std::map<int, quint8> &liveM,
                                        std::map<int, quint8> &shadowM,
                                        const std::map<int, qint16> &liveD,
                                        std::map<int, qint16> &shadowD,
                                        bool suppressReporting)
{
    McDeviceMapDiff diff;
    detail::diffOneArea(liveM, shadowM, QLatin1Char('M'), suppressReporting,
                        &diff.changedValues, &diff.mChanges);

    // No early return between the two areas. The device had one here — it returned whenever either
    // D map was empty, BEFORE emitting valueChanged — so on a station configured with M ranges and
    // no D ranges every accumulated M change was discarded, while deviceMChanged still fired and
    // the device widget looked alive. Backlog 59.3b; pinned by
    // test_m_changes_survive_a_station_with_no_d_ranges.
    detail::diffOneArea(liveD, shadowD, QLatin1Char('D'), suppressReporting,
                        &diff.changedValues, &diff.dChanges);
    return diff;
}

} // namespace vc::device

#endif // MC_DEVICE_MAP_DIFF_H
