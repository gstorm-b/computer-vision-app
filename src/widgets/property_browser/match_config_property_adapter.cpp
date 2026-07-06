#include "match_config_property_adapter.h"
#include "matching/match_group.h"
#include "matching/edge_match_config.h"

#include "widgets/property_browser/prop_spec.h"

#include <QMetaType>

namespace mtc {

// ============================================================================
//  Property spec table — EdgeBased algorithm parameters (group-level)
//
//  Each entry fully describes one editable field.
//  Adding a new setting = adding ONE entry here. Nothing else changes.
//  See prop_spec.h for the complete PropSpec<Config> field reference.
//  (Pattern-level "Common" params live in the patterns widget — they are
//   per-pattern, not shared at the group level.)
// ============================================================================

// ── EdgeBased parameters ──────────────────────────────────────────────────────

static const QList<PropSpec<EdgeMatchConfig>> kEdgeSpecs = {
    { "threshLower",     "Canny Low Thresh",    "Lower threshold for the Canny edge detector.",
      QMetaType::Double, 0.0,  255.0,  1.0,   1, false,
      [](const auto &c){ return QVariant(c.threshLower); },
      [](auto &c, const auto &v){ c.threshLower = v.toDouble(); } },

    { "threshUpper",     "Canny High Thresh",   "Upper threshold for the Canny edge detector.",
      QMetaType::Double, 0.0,  255.0,  1.0,   1, false,
      [](const auto &c){ return QVariant(c.threshUpper); },
      [](auto &c, const auto &v){ c.threshUpper = v.toDouble(); } },

    { "kernelSize",      "Kernel Size",         "Canny kernel size (odd, 1 – 7).",
      QMetaType::Int,    1,    7,      2,      -1, false,
      [](const auto &c){ return QVariant(c.kernelSize); },
      [](auto &c, const auto &v){ c.kernelSize = v.toInt(); } },

    { "blurWidth",       "Blur Width",          "Gaussian blur kernel width (odd, 1 – 31).",
      QMetaType::Int,    1,    31,     2,      -1, false,
      [](const auto &c){ return QVariant(c.blurWidth); },
      [](auto &c, const auto &v){ c.blurWidth = v.toInt(); } },

    { "blurHeight",      "Blur Height",         "Gaussian blur kernel height (odd, 1 – 31).",
      QMetaType::Int,    1,    31,     2,      -1, false,
      [](const auto &c){ return QVariant(c.blurHeight); },
      [](auto &c, const auto &v){ c.blurHeight = v.toInt(); } },

    { "greediness",      "Greediness",          "Matching greediness — higher values speed up at some cost to accuracy.",
      QMetaType::Double, 0.0,  1.0,    0.01,  3, false,
      [](const auto &c){ return QVariant(c.greediness); },
      [](auto &c, const auto &v){ c.greediness = v.toDouble(); } },

    { "minReduceLength", "Min Reduce Length",   "Minimum edge contour length kept after reduction.",
      QMetaType::Int,    8,    512,    8,      -1, false,
      [](const auto &c){ return QVariant(c.minReduceLength); },
      [](auto &c, const auto &v){ c.minReduceLength = v.toInt(); } },

    { "tSamples",        "T Samples",           "Number of template sampling points.",
      QMetaType::Int,    1,    20,     1,      -1, false,
      [](const auto &c){ return QVariant(c.tSamples); },
      [](auto &c, const auto &v){ c.tSamples = v.toInt(); } },

    { "invertBinary",    "Invert Binary Thresh", "Invert the binary threshold before matching.",
      QMetaType::Bool,   {},{},{},               -1, false,
      [](const auto &c){ return QVariant(c.invertBinaryThreshold); },
      [](auto &c, const auto &v){ c.invertBinaryThreshold = v.toBool(); } },

    { "binaryThreshold", "Binary Threshold",     "Source binarization threshold: -1 = auto (Otsu), 0–255 = fixed value.",
      QMetaType::Int,    -1,   255,    1,      -1, false,
      [](const auto &c){ return QVariant(c.binaryThreshold); },
      [](auto &c, const auto &v){ c.binaryThreshold = v.toInt(); } },

    { "binaryMaxValue",  "Binary Max Value",     "Intensity assigned to pixels above the binary threshold (0–255).",
      QMetaType::Int,    0,    255,    1,      -1, false,
      [](const auto &c){ return QVariant(c.binaryMaxValue); },
      [](auto &c, const auto &v){ c.binaryMaxValue = v.toInt(); } },

    { "subPixel",        "Sub-Pixel Estimation", "Enable sub-pixel accuracy for pose estimation.",
      QMetaType::Bool,   {},{},{},               -1, false,
      [](const auto &c){ return QVariant(c.subPixelEstimation); },
      [](auto &c, const auto &v){ c.subPixelEstimation = v.toBool(); } },

    { "stopLayer1",      "Stop at Layer 1",      "Stop the pyramid search at layer 1 (faster, less accurate).",
      QMetaType::Bool,   {},{},{},               -1, false,
      [](const auto &c){ return QVariant(c.stopAtLayer1); },
      [](auto &c, const auto &v){ c.stopAtLayer1 = v.toBool(); } },
};

// ============================================================================
//  MatchConfigPropertyAdapter
// ============================================================================

MatchConfigPropertyAdapter::MatchConfigPropertyAdapter(QtVariantPropertyManager *mgr,
                                                       QObject *parent)
    : QObject(parent), m_mgr(mgr)
{
    connect(m_mgr, &QtVariantPropertyManager::valueChanged,
            this,  &MatchConfigPropertyAdapter::onPropertyValueChanged);
}

MatchConfigPropertyAdapter::~MatchConfigPropertyAdapter()
{
    destroy();
}

void MatchConfigPropertyAdapter::bind(MatchGroupConfig *cfg)
{
    if (m_cfg == cfg) return;
    destroy();
    m_cfg = cfg;
    if (m_cfg) build();
}

QList<QtProperty *> MatchConfigPropertyAdapter::rootProperties() const
{
    QList<QtProperty *> list;
    if (m_grpType)   list.append(m_grpType);
    return list;
}

// ── Build ────────────────────────────────────────────────────────────────────

void MatchConfigPropertyAdapter::build()
{
    buildTypeGroup();
}

void MatchConfigPropertyAdapter::buildTypeGroup()
{
    const MatchingType t = m_cfg->matchingType();
    m_grpType = m_mgr->addProperty(QtVariantPropertyManager::groupTypeId(),
                                   tr(matchingTypeName(t)));

    if (t == MatchingType::EdgeBased) {
        const EdgeMatchConfig *ecfg = m_cfg->edgeConfig();
        if (!ecfg) return;

        auto sub = PropSpecHelper::buildGroup(m_mgr, m_grpType,
                                              kEdgeSpecs, *ecfg, m_propKeys);
        for (auto it = sub.cbegin(); it != sub.cend(); ++it)
            m_props[it.key()] = it.value();
    }
    // Future: add branches for MatchingType::Correlation etc.
}

// ── Refresh ──────────────────────────────────────────────────────────────────

void MatchConfigPropertyAdapter::refresh()
{
    if (!m_cfg) return;

    if (m_cfg->matchingType() == MatchingType::EdgeBased) {
        const EdgeMatchConfig *ecfg = m_cfg->edgeConfig();
        if (ecfg) PropSpecHelper::refresh(m_mgr, kEdgeSpecs, *ecfg, m_props);
    }
}

// ── Destroy ──────────────────────────────────────────────────────────────────

void MatchConfigPropertyAdapter::destroy()
{
    // The variant manager is SHARED with the host widget (group-level props,
    // other adapters, …).  m_mgr->clear() would wipe everyone's properties.
    // Delete only the top-level groups this adapter created. Their child
    // properties are owned by the QtProperty tree and are released together
    // with the parent group.
    m_props.clear();
    m_propKeys.clear();

    delete m_grpType;    m_grpType   = nullptr;

    m_cfg = nullptr;
}

// ── Slot: property changed ────────────────────────────────────────────────────

void MatchConfigPropertyAdapter::onPropertyValueChanged(QtProperty *prop,
                                                         const QVariant &val)
{
    if (!m_cfg) return;

    const QString key = m_propKeys.value(prop);
    if (key.isEmpty()) return;

    if (m_cfg->matchingType() == MatchingType::EdgeBased) {
        EdgeMatchConfig *ecfg = m_cfg->edgeConfig();
        if (ecfg && PropSpecHelper::dispatch(kEdgeSpecs, key, val, *ecfg)) {
            emit configModified();
        }
    }
}

} // namespace mtc
