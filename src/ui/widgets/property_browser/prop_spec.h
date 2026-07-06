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

// ─────────────────────────────────────────────────────────────────────────────
//  PropSpec<Config>
//
//  One row in a property spec table. Fields:
//    key         — unique internal id (ASCII, used as map key)
//    label       — user-visible name in the property browser
//    description — shown in the description panel; nullptr → no tooltip
//    propType    — QMetaType type id: QMetaType::Double, Int, Bool, QString …
//    min/max/step — range and step attributes (invalid QVariant = skip)
//    decimals    — precision for Double; -1 = use manager default
//    readOnly    — disables the editor widget for this property
//    read        — extract the value from the config as a QVariant
//    write       — apply a changed QVariant value back to the config
// ─────────────────────────────────────────────────────────────────────────────

template<typename Config>
struct PropSpec {
    const char *key;
    const char *label;
    const char *description;    // may be nullptr
    int         propType;       // QMetaType::Double / Int / Bool / QString …
    QVariant    min, max, step;
    int         decimals{-1};
    bool        readOnly{false};

    std::function<QVariant(const Config &)>         read;
    std::function<void(Config &, const QVariant &)> write;
};

// ─────────────────────────────────────────────────────────────────────────────
//  PropGroup<Config>
//
//  A labelled collection of specs that forms one collapsible group in the
//  property browser.
// ─────────────────────────────────────────────────────────────────────────────

template<typename Config>
struct PropGroup {
    const char              *label;
    QList<PropSpec<Config>>  specs;
};

// ─────────────────────────────────────────────────────────────────────────────
//  PropSpecHelper  —  free functions operating on spec lists
// ─────────────────────────────────────────────────────────────────────────────

namespace PropSpecHelper {

// Apply numeric attributes from a spec to a freshly created property.
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

// Build one spec as a standalone property and optionally populate lookup maps.
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

// Build all specs as sub-properties of `group` (may be nullptr for no group).
// Returns the {key → prop} map; fills propKeyMap for reverse lookups.
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

// Build multiple PropGroups, adding each as a top-level property in the browser.
// Returns merged {key → prop} map; fills propKeyMap.
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

// Refresh all values from cfg into existing properties (suppresses valueChanged).
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

// Refresh multiple groups.
template<typename Config>
inline void refreshGroups(QtVariantPropertyManager                   *mgr,
                           const QList<PropGroup<Config>>            &groups,
                           const Config                              &cfg,
                           const QMap<QString, QtVariantProperty *> &propMap)
{
    for (const auto &grp : groups)
        refresh(mgr, grp.specs, cfg, propMap);
}

// Dispatch: if `key` matches any spec, write val to cfg and return true.
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

// Dispatch across multiple groups.
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
