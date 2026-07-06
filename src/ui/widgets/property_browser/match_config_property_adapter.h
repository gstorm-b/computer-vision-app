#ifndef MATCH_CONFIG_PROPERTY_ADAPTER_H
#define MATCH_CONFIG_PROPERTY_ADAPTER_H

#include <QObject>
#include <QMap>
#include <QList>

class QtVariantPropertyManager;
class QtVariantProperty;
class QtProperty;

namespace mtc {

class MatchGroupConfig;

// ---------------------------------------------------------------------------
// MatchConfigPropertyAdapter
//
// Bridges a group's algorithm config (MatchGroupConfig::typeConfig) ↔
// QtVariantPropertyManager/Browser.  The edge/algorithm parameters are shared
// by every pattern in the group, so they are edited at the group level.
//
// Usage:
//   auto* adapter = new MatchConfigPropertyAdapter(variantMgr, this);
//   adapter->bind(&myGroupConfig);
//   for (auto* p : adapter->rootProperties())
//       browser->addProperty(p);
//   // property edits are automatically committed back to the group config
//   // and configModified() is emitted
//
// Extending — adding a new EdgeBased setting requires only two steps:
//   1. Add the field to EdgeMatchConfig.
//   2. Add ONE entry to kEdgeSpecs[] in match_config_property_adapter.cpp.
//   Nothing else changes.
//
// Extending — adding a new algorithm type:
//   1. Implement IMatchTypeConfig + matching_types.h as documented there.
//   2. Add a PropSpec table for the new type in the .cpp.
//   3. Add a branch in buildTypeGroup() to use that table.
// ---------------------------------------------------------------------------
class MatchConfigPropertyAdapter : public QObject {
    Q_OBJECT

public:
    explicit MatchConfigPropertyAdapter(QtVariantPropertyManager* mgr,
                                        QObject* parent = nullptr);
    ~MatchConfigPropertyAdapter() override;

    // Bind to a group config object.  Pass nullptr to unbind.
    // Rebuilds the algorithm-type property group for the config's matching type.
    void bind(MatchGroupConfig* cfg);

    // Re-read all property values from the currently bound config.
    void refresh();

    // Top-level QtProperty* items to add to a browser widget.
    QList<QtProperty*> rootProperties() const;

signals:
    // Emitted after any property change has been committed to the config.
    void configModified();

private slots:
    void onPropertyValueChanged(QtProperty* prop, const QVariant& val);

private:
    void build();
    void buildTypeGroup();
    void destroy();

    QtVariantPropertyManager* m_mgr{nullptr};
    MatchGroupConfig*          m_cfg{nullptr};

    QtVariantProperty* m_grpType{nullptr};    // "Edge-Based" / "Correlation" group

    QMap<QString, QtVariantProperty *> m_props;     // key  → property
    QMap<QtProperty *, QString>        m_propKeys;  // property → key (reverse map)
};

} // namespace mtc
#endif // MATCH_CONFIG_PROPERTY_ADAPTER_H
