#pragma once

// ============================================================================
//  prop_spec.h  —  Generic property-spec framework for QtPropertyBrowser
//
//  Design goal: define a new editable parameter by adding ONE entry to a
//  spec list. Building, refreshing, and dispatching changes are handled
//  entirely by the helper functions below. Nothing else needs to change.
//
//  Typical usage pattern:
//
//    // 1. Define specs (usually as a static / file-level variable)
//    static const QList<PropSpec<MyConfig>> kMySpecs = {
//        { "speed", "Speed (m/s)", "Maximum linear velocity",
//          QMetaType::Double, 0.0, 10.0, 0.01, 2, false,
//          [](const MyConfig &c){ return c.speed; },
//          [](MyConfig &c, const QVariant &v){ c.speed = v.toDouble(); } },
//        { "enabled", "Enabled", nullptr,
//          QMetaType::Bool, {}, {}, {}, -1, false,
//          [](const MyConfig &c){ return c.enabled; },
//          [](MyConfig &c, const QVariant &v){ c.enabled = v.toBool(); } },
//    };
//
//    // 2. Build (once, inside your "build" method)
//    auto *grp = mgr->addProperty(QtVariantPropertyManager::groupTypeId(), "My Group");
//    m_propMap = PropSpecHelper::buildGroup(mgr, grp, kMySpecs, cfg, m_propKeys);
//    browser->addProperty(grp);
//
//    // 3. Refresh (on external cfg changes)
//    PropSpecHelper::refresh(mgr, kMySpecs, cfg, m_propMap);
//
//    // 4. Dispatch (in valueChanged slot)
//    if (PropSpecHelper::dispatch(kMySpecs, key, val, cfg))
//        emit configModified();
//
// ============================================================================

#include <functional>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QSignalBlocker>

#include "qtpropertybrowser/qtvariantproperty.h"

/// One row in a property spec table. Fields:
///   key         — unique internal id (ASCII, used as map key)
///   label       — user-visible name in the property browser
///   description — shown in the description panel; nullptr → no tooltip
///   propType    — QMetaType type id: QMetaType::Double, Int, Bool, QString …
///   min/max/step — range and step attributes (invalid QVariant = skip)
///   decimals    — precision for Double; -1 = use manager default
///   readOnly    — disables the editor widget for this property
///   read        — extract the value from the config as a QVariant
///   write       — apply a changed QVariant value back to the config
template<typename Config>
struct PropSpec {
    const char *key;             ///< Unique internal id (ASCII, used as the lookup-map key).
    const char *label;           ///< User-visible name shown in the property browser.
    const char *description;    ///< Shown in the description panel; nullptr means no tooltip.
    int         propType;       ///< QMetaType type id: QMetaType::Double / Int / Bool / QString …
    QVariant    min, max, step;  ///< Range/step attributes; an invalid QVariant means "skip this attribute".
    int         decimals{-1};    ///< Precision for Double properties; -1 = use the manager default.
    bool        readOnly{false}; ///< When true, disables the editor widget for this property.

    std::function<QVariant(const Config &)>         read;   ///< Extracts this property's current value from the config.
    std::function<void(Config &, const QVariant &)> write;  ///< Applies a changed value back to the config.
};

/// A labelled collection of PropSpec entries that forms one collapsible group
/// in the property browser.
template<typename Config>
struct PropGroup {
    const char              *label;   ///< Group label shown in the property browser.
    QList<PropSpec<Config>>  specs;   ///< Specs built as sub-properties of this group.
};

/// Free functions that build, refresh, and dispatch value changes for
/// PropSpec/PropGroup tables against a QtVariantPropertyManager-based browser.
namespace PropSpecHelper {

/// Applies a spec's optional min/max/step/decimals/description attributes to
/// `prop` (skipping any that are unset) and sets its enabled state from
/// spec.readOnly.
template<typename Config>
inline void applyAttributes(QtVariantPropertyManager       *mgr,
                             QtVariantProperty              *prop,
                             const PropSpec<Config>         &spec)
{
    if (spec.min.isValid())  mgr->setAttribute(prop, QStringLiteral("minimum"),    spec.min);
    if (spec.max.isValid())  mgr->setAttribute(prop, QStringLiteral("maximum"),    spec.max);
    if (spec.step.isValid()) mgr->setAttribute(prop, QStringLiteral("singleStep"), spec.step);
    if (spec.decimals >= 0)  mgr->setAttribute(prop, QStringLiteral("decimals"),   spec.decimals);
    if (spec.description)    prop->setDescriptionToolTip(QString::fromUtf8(spec.description));
    prop->setEnabled(!spec.readOnly);
}

/// Creates one standalone QtVariantProperty from `spec` (applying its
/// attributes and initial value from `cfg`), and records it in `propMap`/
/// `propKeyMap` when those are provided.
/// @param propMap optional {key → prop} map to populate for later lookup
/// @param propKeyMap optional {prop → key} reverse map to populate
/// @return the created property, or nullptr if the manager failed to create it
template<typename Config>
inline QtVariantProperty *buildOne(QtVariantPropertyManager          *mgr,
                                    const PropSpec<Config>             &spec,
                                    const Config                       &cfg,
                                    QMap<QString, QtVariantProperty *> *propMap    = nullptr,
                                    QMap<QtProperty *, QString>        *propKeyMap = nullptr)
{
    auto *prop = mgr->addProperty(spec.propType, QString::fromUtf8(spec.label));
    if (!prop) return nullptr;

    applyAttributes(mgr, prop, spec);
    prop->setValue(spec.read(cfg));

    const QString key = QString::fromUtf8(spec.key);
    if (propMap)    (*propMap)[key]    = prop;
    if (propKeyMap) (*propKeyMap)[prop] = key;
    return prop;
}

/// Builds every spec in `specs` (via buildOne) as a sub-property of `group`
/// (pass nullptr for no group).
/// @return the {key → prop} map for all built specs; also fills propKeyMap for reverse lookups
template<typename Config>
inline QMap<QString, QtVariantProperty *>
buildGroup(QtVariantPropertyManager            *mgr,
           QtVariantProperty                   *group,
           const QList<PropSpec<Config>>       &specs,
           const Config                        &cfg,
           QMap<QtProperty *, QString>         &propKeyMap)
{
    QMap<QString, QtVariantProperty *> propMap;
    for (const auto &spec : specs) {
        auto *prop = buildOne(mgr, spec, cfg, &propMap, &propKeyMap);
        if (prop && group) group->addSubProperty(prop);
    }
    return propMap;
}

/// Builds every group in `groups` (via buildGroup) and adds each resulting
/// group property as a top-level property of `browser`.
/// @return the merged {key → prop} map across all groups; also fills propKeyMap
template<typename Config>
inline QMap<QString, QtVariantProperty *>
buildGroups(QtVariantPropertyManager              *mgr,
            QtAbstractPropertyBrowser             *browser,
            const QList<PropGroup<Config>>        &groups,
            const Config                          &cfg,
            QMap<QtProperty *, QString>           &propKeyMap)
{
    QMap<QString, QtVariantProperty *> combined;
    for (const auto &grp : groups) {
        auto *groupProp = mgr->addProperty(QtVariantPropertyManager::groupTypeId(),
                                           QString::fromUtf8(grp.label));
        auto sub = buildGroup(mgr, groupProp, grp.specs, cfg, propKeyMap);
        combined.insert(sub);
        browser->addProperty(groupProp);
    }
    return combined;
}

/// Pushes each spec's current value (via spec.read(cfg)) into its matching
/// existing property in `propMap`, wrapping the update in a QSignalBlocker on
/// `mgr` so it does not trigger valueChanged.
template<typename Config>
inline void refresh(QtVariantPropertyManager                   *mgr,
                    const QList<PropSpec<Config>>              &specs,
                    const Config                              &cfg,
                    const QMap<QString, QtVariantProperty *> &propMap)
{
    const QSignalBlocker blocker(mgr);
    for (const auto &spec : specs) {
        const QString key = QString::fromUtf8(spec.key);
        if (auto *p = propMap.value(key)) p->setValue(spec.read(cfg));
    }
}

/// Calls refresh() for every group in `groups` against the shared `propMap`.
template<typename Config>
inline void refreshGroups(QtVariantPropertyManager                   *mgr,
                           const QList<PropGroup<Config>>            &groups,
                           const Config                              &cfg,
                           const QMap<QString, QtVariantProperty *> &propMap)
{
    for (const auto &grp : groups)
        refresh(mgr, grp.specs, cfg, propMap);
}

/// Finds the spec in `specs` whose key matches `key` and, if found, calls its
/// write() to apply `val` to `cfg`.
/// @return true if a matching spec was found and its write() was invoked
template<typename Config>
inline bool dispatch(const QList<PropSpec<Config>> &specs,
                     const QString                  &key,
                     const QVariant                 &val,
                     Config                         &cfg)
{
    for (const auto &spec : specs) {
        if (QLatin1String(spec.key) == key) {
            spec.write(cfg, val);
            return true;
        }
    }
    return false;
}

/// Calls dispatch() against each group's specs in turn, stopping at (and
/// returning true from) the first group whose dispatch() succeeds.
/// @return true if any group's dispatch() matched and wrote `key`
template<typename Config>
inline bool dispatchGroups(const QList<PropGroup<Config>> &groups,
                            const QString                  &key,
                            const QVariant                 &val,
                            Config                         &cfg)
{
    for (const auto &grp : groups)
        if (dispatch(grp.specs, key, val, cfg)) return true;
    return false;
}

} // namespace PropSpecHelper
