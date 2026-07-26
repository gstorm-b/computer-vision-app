#ifndef MATCH_CONFIG_PROPERTY_ADAPTER_H
#define MATCH_CONFIG_PROPERTY_ADAPTER_H

#include <QObject>
#include <QMap>
#include <QList>

class QtVariantPropertyManager;
class QtVariantProperty;
class QtProperty;

/// Property-browser adapters for editing matching configuration (group/algorithm
/// parameters, pattern trees) via QtVariantPropertyManager/Browser.
namespace mtc {

class MatchGroupConfig;

/// Bridges a group's algorithm config (MatchGroupConfig::typeConfig) to a
/// QtVariantPropertyManager/Browser. The edge/algorithm parameters are shared
/// by every pattern in the group, so they are edited at the group level; the
/// currently-bound config's matching type determines which property spec table
/// (e.g. kEdgeSpecs) drives the generated property tree.
///
/// Usage:
///   auto* adapter = new MatchConfigPropertyAdapter(variantMgr, this);
///   adapter->bind(&myGroupConfig);
///   for (auto* p : adapter->rootProperties())
///       browser->addProperty(p);
///   // property edits are automatically committed back to the group config
///   // and configModified() is emitted
///
/// Extending — adding a new EdgeBased setting requires only two steps:
///   1. Add the field to EdgeMatchConfig.
///   2. Add ONE entry to kEdgeSpecs[] in match_config_property_adapter.cpp.
///   Nothing else changes.
///
/// Extending — adding a new algorithm type:
///   1. Implement IMatchTypeConfig + matching_types.h as documented there.
///   2. Add a PropSpec table for the new type in the .cpp.
///   3. Add a branch in buildTypeGroup() to use that table.
class MatchConfigPropertyAdapter : public QObject {
    Q_OBJECT

public:
    /// Constructs the adapter, connecting to `mgr`'s valueChanged signal so
    /// edits made in the property browser are routed to onPropertyValueChanged().
    /// @param mgr shared QtVariantPropertyManager this adapter adds its properties to
    /// @param parent optional QObject owner
    explicit MatchConfigPropertyAdapter(QtVariantPropertyManager* mgr,
                                        QObject* parent = nullptr);
    /// Destroys the adapter, tearing down any properties it owns via destroy().
    ~MatchConfigPropertyAdapter() override;

    /// Binds to a group config object, rebuilding the algorithm-type property
    /// group for the config's matching type. Pass nullptr to unbind. No-op if
    /// `cfg` is already the bound config.
    void bind(MatchGroupConfig* cfg);

    /// Re-reads all property values from the currently bound config, updating
    /// the property tree to match. No-op if nothing is bound.
    void refresh();

    /// Returns the top-level QtProperty* items to add to a browser widget
    /// (currently just the single algorithm-type group, if built).
    QList<QtProperty*> rootProperties() const;

signals:
    /// Emitted after a property edit has been committed to the bound config.
    void configModified();

private slots:
    /// Handles a value change from the property browser: looks up the changed
    /// `prop`'s key and, if the bound config's matching type has a matching spec
    /// table, dispatches the new value into the config and emits configModified().
    void onPropertyValueChanged(QtProperty* prop, const QVariant& val);

private:
    /// Builds the property tree for the currently bound config (dispatches to
    /// buildTypeGroup()).
    void build();

    /// Creates the algorithm-type group property (named after the config's
    /// matching type) and, for MatchingType::EdgeBased, populates it from
    /// kEdgeSpecs via PropSpecHelper::buildGroup(). No-op branches exist for
    /// other matching types until their spec tables are added.
    void buildTypeGroup();

    /// Releases the properties this adapter created (m_grpType and its
    /// children) and clears the key maps. Does not touch m_mgr's other
    /// properties, since the manager is shared with the host widget.
    void destroy();

    QtVariantPropertyManager* m_mgr{nullptr};  ///< Shared property manager this adapter adds properties to; not owned.
    MatchGroupConfig*          m_cfg{nullptr}; ///< Currently bound group config; not owned. Null when unbound.

    QtVariantProperty* m_grpType{nullptr};    ///< Top-level "Edge-Based" / "Correlation" algorithm-type group property.

    QMap<QString, QtVariantProperty *> m_props;     ///< Maps spec key to its generated property.
    QMap<QtProperty *, QString>        m_propKeys;  ///< Reverse map: property to spec key, for change dispatch.
};

} // namespace mtc
#endif // MATCH_CONFIG_PROPERTY_ADAPTER_H
