#include "custom_property_managers.h"
#include <QLocale>

// ============================================================================
//  PositionPropertyManager
// ============================================================================

// ── Static helpers ───────────────────────────────────────────────────────────

/// Returns the number of numeric sub-properties for `m` (2 for XY, 3 for XYZ, 6 for XYZRPY;
/// defaults to 2 for any unhandled value).
int PositionPropertyManager::modeComponentCount(Mode m)
{
    switch (m) {
    case XY:     return 2;
    case XYZ:    return 3;
    case XYZRPY: return 6;
    }
    return 2;
}

/// Returns the display label for each component of `m` (e.g. "X"/"Y" for XY, plus "Roll"/
/// "Pitch"/"Yaw" for XYZRPY).
QStringList PositionPropertyManager::modeLabels(Mode m)
{
    switch (m) {
    case XY:     return {QStringLiteral("X"),    QStringLiteral("Y")};
    case XYZ:    return {QStringLiteral("X"),    QStringLiteral("Y"),     QStringLiteral("Z")};
    case XYZRPY: return {QStringLiteral("X"),    QStringLiteral("Y"),     QStringLiteral("Z"),
                         QStringLiteral("Roll"), QStringLiteral("Pitch"), QStringLiteral("Yaw")};
    }
    return {};
}

// ── Construction ─────────────────────────────────────────────────────────────

/// Constructs the manager and its internal QtDoublePropertyManager (used for the X/Y/Z/... sub-
/// properties), connecting its valueChanged/propertyDestroyed signals to slotDoubleChanged() and
/// slotPropertyDestroyed() so component edits and sub-property teardown propagate back here.
PositionPropertyManager::PositionPropertyManager(QObject *parent)
    : QtAbstractPropertyManager(parent)
    , m_doubleManager(new QtDoublePropertyManager(this))
{
    connect(m_doubleManager, &QtDoublePropertyManager::valueChanged,
            this, &PositionPropertyManager::slotDoubleChanged);
    connect(m_doubleManager, &QtAbstractPropertyManager::propertyDestroyed,
            this, &PositionPropertyManager::slotPropertyDestroyed);
}

/// Default destructor.
PositionPropertyManager::~PositionPropertyManager() = default;

/// Returns the internal QtDoublePropertyManager that owns the component sub-properties; callers
/// must register it with an editor factory on their QtAbstractPropertyBrowser.
QtDoublePropertyManager *PositionPropertyManager::subDoubleManager() const
{
    return m_doubleManager;
}

// ── Value accessors ──────────────────────────────────────────────────────────

/// Returns the current component values for `property` (empty vector if `property` is not
/// managed by this instance).
QVector<double> PositionPropertyManager::value(const QtProperty *property) const
{
    return m_values.value(property).values;
}

/// Returns the display mode currently set for `property` (defaults to XY if unmanaged).
PositionPropertyManager::Mode PositionPropertyManager::mode(const QtProperty *property) const
{
    return m_values.value(property).mode;
}

/// Returns the decimal precision used to format and edit `property`'s component values.
int PositionPropertyManager::decimals(const QtProperty *property) const
{
    return m_values.value(property).decimals;
}

/// Returns the minimum value allowed for each of `property`'s components.
double PositionPropertyManager::minimum(const QtProperty *property) const
{
    return m_values.value(property).minimum;
}

/// Returns the maximum value allowed for each of `property`'s components.
double PositionPropertyManager::maximum(const QtProperty *property) const
{
    return m_values.value(property).maximum;
}

/// Returns the spin/step increment used when editing each of `property`'s components.
double PositionPropertyManager::singleStep(const QtProperty *property) const
{
    return m_values.value(property).singleStep;
}

// ── Mutators ─────────────────────────────────────────────────────────────────

/// Sets `property`'s component values (resizing to the current mode's component count, zero-
/// filling any missing entries) and pushes each value to its corresponding sub-property; no-op
/// if `property` is not managed here. Emits propertyChanged() and valueChanged() on success.
/// @param val the new component values; extra entries beyond the mode's count are dropped
void PositionPropertyManager::setValue(QtProperty *property, const QVector<double> &val)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    Data &data = it.value();
    const int n = modeComponentCount(data.mode);
    data.values.resize(n);
    for (int i = 0; i < n; ++i)
        data.values[i] = (i < val.size()) ? val[i] : 0.0;

    for (int i = 0; i < qMin(n, data.subProps.size()); ++i)
        m_doubleManager->setValue(data.subProps[i], data.values[i]);

    emit propertyChanged(property);
    emit valueChanged(property, data.values);
}

/// Switches `property` to `newMode`: destroys the existing sub-properties, resizes the stored
/// values (zero-filled) to the new component count, and rebuilds the sub-properties. No-op if
/// `property` is unmanaged or already in `newMode`. Emits modeChanged() and propertyChanged().
void PositionPropertyManager::setMode(QtProperty *property, Mode newMode)
{
    auto it = m_values.find(property);
    if (it == m_values.end() || it->mode == newMode) return;

    Data &data = it.value();
    destroySubProperties(property, data);

    data.mode = newMode;
    const int n = modeComponentCount(newMode);
    data.values.resize(n, 0.0);
    createSubProperties(property, data);

    emit modeChanged(property, newMode);
    emit propertyChanged(property);
}

/// Sets the decimal precision for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged. Emits decimalsChanged() and propertyChanged().
void PositionPropertyManager::setDecimals(QtProperty *property, int prec)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->decimals = prec;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setDecimals(sub, prec);

    emit decimalsChanged(property, prec);
    emit propertyChanged(property);
}

/// Sets the min/max range for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged. Does not emit any signal itself (relies on the sub-managers' own
/// range-change notifications, if any).
void PositionPropertyManager::setRange(QtProperty *property, double minVal, double maxVal)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->minimum = minVal;
    it->maximum = maxVal;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setRange(sub, minVal, maxVal);
}

/// Sets the spin/step increment for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged.
void PositionPropertyManager::setSingleStep(QtProperty *property, double step)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->singleStep = step;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setSingleStep(sub, step);
}

// ── Display text ─────────────────────────────────────────────────────────────

/// Formats `property`'s component values as "Label:value" pairs (fixed-point, using the stored
/// decimals), joined with a double space, e.g. "X:1.00  Y:2.00". Empty string if unmanaged.
QString PositionPropertyManager::valueText(const QtProperty *property) const
{
    const auto it = m_values.constFind(property);
    if (it == m_values.constEnd()) return {};

    const Data &data = it.value();
    const QStringList labels = modeLabels(data.mode);
    QStringList parts;
    parts.reserve(data.values.size());
    for (int i = 0; i < data.values.size(); ++i)
        parts << QStringLiteral("%1:%2").arg(labels.value(i))
                                        .arg(data.values[i], 0, 'f', data.decimals);
    return parts.join(QStringLiteral("  "));
}

// ── QtAbstractPropertyManager overrides ──────────────────────────────────────

/// QtAbstractPropertyManager hook invoked when `property` is added to this manager: seeds a
/// default XY Data entry (zero-filled) and creates its sub-properties.
void PositionPropertyManager::initializeProperty(QtProperty *property)
{
    Data data;
    data.values = QVector<double>(modeComponentCount(data.mode), 0.0);
    m_values[property] = data;
    createSubProperties(property, m_values[property]);
}

/// QtAbstractPropertyManager hook invoked when `property` is removed from this manager: destroys
/// its sub-properties and erases its stored Data. No-op if `property` is unmanaged.
void PositionPropertyManager::uninitializeProperty(QtProperty *property)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    destroySubProperties(property, it.value());
    m_values.erase(it);
}

// ── Private helpers ───────────────────────────────────────────────────────────

/// Creates one QtDoubleProperty sub-property per component of `data.mode` under `parent`,
/// configuring each from `data` (decimals/range/step/value), adding it to `parent`, and
/// registering it in m_subToParent and `data.subProps`.
void PositionPropertyManager::createSubProperties(QtProperty *parent, Data &data)
{
    const QStringList labels = modeLabels(data.mode);
    const int n = modeComponentCount(data.mode);

    data.subProps.clear();
    for (int i = 0; i < n; ++i) {
        auto *sub = m_doubleManager->addProperty(labels[i]);
        m_doubleManager->setDecimals  (sub, data.decimals);
        m_doubleManager->setRange     (sub, data.minimum, data.maximum);
        m_doubleManager->setSingleStep(sub, data.singleStep);
        m_doubleManager->setValue     (sub, data.values.value(i, 0.0));
        parent->addSubProperty(sub);
        m_subToParent[sub] = parent;
        data.subProps.append(sub);
    }
}

/// Removes and deletes every sub-property in `data.subProps` from `parent`, unregistering each
/// from m_subToParent, then clears `data.subProps`.
void PositionPropertyManager::destroySubProperties(QtProperty *parent, Data &data)
{
    for (auto *sub : std::as_const(data.subProps)) {
        m_subToParent.remove(sub);
        parent->removeSubProperty(sub);
        delete sub;
    }
    data.subProps.clear();
}

// ── Private slots ─────────────────────────────────────────────────────────────

/// Handles a value change on a component sub-property `sub`: looks up its parent compound
/// property, writes the new value back into the parent's stored Data at the matching index, and
/// emits propertyChanged()/valueChanged() for the parent. No-op if `sub` is untracked.
void PositionPropertyManager::slotDoubleChanged(QtProperty *sub, double /*value*/)
{
    auto parentIt = m_subToParent.find(sub);
    if (parentIt == m_subToParent.end()) return;

    QtProperty *parent = parentIt.value();
    auto dataIt = m_values.find(parent);
    if (dataIt == m_values.end()) return;

    Data &data = dataIt.value();
    const int idx = data.subProps.indexOf(sub);
    if (idx < 0) return;

    data.values[idx] = m_doubleManager->value(sub);
    emit propertyChanged(parent);
    emit valueChanged(parent, data.values);
}

/// Removes `sub` from m_subToParent when the sub-property manager destroys it (e.g. during
/// uninitializeProperty()/destroySubProperties(), or external destruction).
void PositionPropertyManager::slotPropertyDestroyed(QtProperty *sub)
{
    m_subToParent.remove(sub);
}

// ============================================================================
//  SizePropertyManager
// ============================================================================

// ── Static helpers ───────────────────────────────────────────────────────────

/// Returns the number of numeric sub-properties for `m` (2 for WH, 3 for WHD).
int SizePropertyManager::modeComponentCount(Mode m)
{
    return (m == WH) ? 2 : 3;
}

/// Returns the display label for each component of `m` ("Width"/"Height", plus "Depth" for WHD).
QStringList SizePropertyManager::modeLabels(Mode m)
{
    if (m == WH)
        return {QStringLiteral("Width"), QStringLiteral("Height")};
    return {QStringLiteral("Width"), QStringLiteral("Height"), QStringLiteral("Depth")};
}

// ── Construction ─────────────────────────────────────────────────────────────

/// Constructs the manager and its internal QtDoublePropertyManager (used for the Width/Height/
/// Depth sub-properties), connecting its valueChanged/propertyDestroyed signals to
/// slotDoubleChanged() and slotPropertyDestroyed().
SizePropertyManager::SizePropertyManager(QObject *parent)
    : QtAbstractPropertyManager(parent)
    , m_doubleManager(new QtDoublePropertyManager(this))
{
    connect(m_doubleManager, &QtDoublePropertyManager::valueChanged,
            this, &SizePropertyManager::slotDoubleChanged);
    connect(m_doubleManager, &QtAbstractPropertyManager::propertyDestroyed,
            this, &SizePropertyManager::slotPropertyDestroyed);
}

/// Default destructor.
SizePropertyManager::~SizePropertyManager() = default;

/// Returns the internal QtDoublePropertyManager that owns the component sub-properties; callers
/// must register it with an editor factory on their QtAbstractPropertyBrowser.
QtDoublePropertyManager *SizePropertyManager::subDoubleManager() const
{
    return m_doubleManager;
}

// ── Value accessors ──────────────────────────────────────────────────────────

/// Returns the current component values for `property` (empty vector if `property` is not
/// managed by this instance).
QVector<double> SizePropertyManager::value(const QtProperty *property) const
{
    return m_values.value(property).values;
}

/// Returns the display mode currently set for `property` (defaults to WH if unmanaged).
SizePropertyManager::Mode SizePropertyManager::mode(const QtProperty *property) const
{
    return m_values.value(property).mode;
}

/// Returns the decimal precision used to format and edit `property`'s component values.
int SizePropertyManager::decimals(const QtProperty *property) const
{
    return m_values.value(property).decimals;
}

/// Returns the minimum value allowed for each of `property`'s components.
double SizePropertyManager::minimum(const QtProperty *property) const
{
    return m_values.value(property).minimum;
}

/// Returns the maximum value allowed for each of `property`'s components.
double SizePropertyManager::maximum(const QtProperty *property) const
{
    return m_values.value(property).maximum;
}

/// Returns the spin/step increment used when editing each of `property`'s components.
double SizePropertyManager::singleStep(const QtProperty *property) const
{
    return m_values.value(property).singleStep;
}

// ── Mutators ─────────────────────────────────────────────────────────────────

/// Sets `property`'s component values (resizing to the current mode's component count, zero-
/// filling any missing entries) and pushes each value to its corresponding sub-property; no-op
/// if `property` is not managed here. Emits propertyChanged() and valueChanged() on success.
/// @param val the new component values; extra entries beyond the mode's count are dropped
void SizePropertyManager::setValue(QtProperty *property, const QVector<double> &val)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    Data &data = it.value();
    const int n = modeComponentCount(data.mode);
    data.values.resize(n);
    for (int i = 0; i < n; ++i)
        data.values[i] = (i < val.size()) ? val[i] : 0.0;

    for (int i = 0; i < qMin(n, data.subProps.size()); ++i)
        m_doubleManager->setValue(data.subProps[i], data.values[i]);

    emit propertyChanged(property);
    emit valueChanged(property, data.values);
}

/// Switches `property` to `newMode`: destroys the existing sub-properties, resizes the stored
/// values (zero-filled) to the new component count, and rebuilds the sub-properties. No-op if
/// `property` is unmanaged or already in `newMode`. Emits modeChanged() and propertyChanged().
void SizePropertyManager::setMode(QtProperty *property, Mode newMode)
{
    auto it = m_values.find(property);
    if (it == m_values.end() || it->mode == newMode) return;

    Data &data = it.value();
    destroySubProperties(property, data);

    data.mode = newMode;
    const int n = modeComponentCount(newMode);
    data.values.resize(n, 0.0);
    createSubProperties(property, data);

    emit modeChanged(property, newMode);
    emit propertyChanged(property);
}

/// Sets the decimal precision for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged. Emits decimalsChanged() and propertyChanged().
void SizePropertyManager::setDecimals(QtProperty *property, int prec)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->decimals = prec;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setDecimals(sub, prec);

    emit decimalsChanged(property, prec);
    emit propertyChanged(property);
}

/// Sets the min/max range for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged.
void SizePropertyManager::setRange(QtProperty *property, double minVal, double maxVal)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->minimum = minVal;
    it->maximum = maxVal;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setRange(sub, minVal, maxVal);
}

/// Sets the spin/step increment for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged.
void SizePropertyManager::setSingleStep(QtProperty *property, double step)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->singleStep = step;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setSingleStep(sub, step);
}

// ── Display text ─────────────────────────────────────────────────────────────

/// Formats `property`'s component values as "Label:value" pairs (fixed-point, using the stored
/// decimals), joined with " × ", e.g. "Width:1.00 × Height:2.00". Empty string if unmanaged.
QString SizePropertyManager::valueText(const QtProperty *property) const
{
    const auto it = m_values.constFind(property);
    if (it == m_values.constEnd()) return {};

    const Data &data = it.value();
    const QStringList labels = modeLabels(data.mode);
    QStringList parts;
    parts.reserve(data.values.size());
    for (int i = 0; i < data.values.size(); ++i)
        parts << QStringLiteral("%1:%2").arg(labels.value(i))
                                        .arg(data.values[i], 0, 'f', data.decimals);
    return parts.join(QStringLiteral(" × "));
}

// ── QtAbstractPropertyManager overrides ──────────────────────────────────────

/// QtAbstractPropertyManager hook invoked when `property` is added to this manager: seeds a
/// default WH Data entry (zero-filled) and creates its sub-properties.
void SizePropertyManager::initializeProperty(QtProperty *property)
{
    Data data;
    data.values = QVector<double>(modeComponentCount(data.mode), 0.0);
    m_values[property] = data;
    createSubProperties(property, m_values[property]);
}

/// QtAbstractPropertyManager hook invoked when `property` is removed from this manager: destroys
/// its sub-properties and erases its stored Data. No-op if `property` is unmanaged.
void SizePropertyManager::uninitializeProperty(QtProperty *property)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    destroySubProperties(property, it.value());
    m_values.erase(it);
}

// ── Private helpers ───────────────────────────────────────────────────────────

/// Creates one QtDoubleProperty sub-property per component of `data.mode` under `parent`,
/// configuring each from `data` (decimals/range/step/value), adding it to `parent`, and
/// registering it in m_subToParent and `data.subProps`.
void SizePropertyManager::createSubProperties(QtProperty *parent, Data &data)
{
    const QStringList labels = modeLabels(data.mode);
    const int n = modeComponentCount(data.mode);

    data.subProps.clear();
    for (int i = 0; i < n; ++i) {
        auto *sub = m_doubleManager->addProperty(labels[i]);
        m_doubleManager->setDecimals  (sub, data.decimals);
        m_doubleManager->setRange     (sub, data.minimum, data.maximum);
        m_doubleManager->setSingleStep(sub, data.singleStep);
        m_doubleManager->setValue     (sub, data.values.value(i, 0.0));
        parent->addSubProperty(sub);
        m_subToParent[sub] = parent;
        data.subProps.append(sub);
    }
}

/// Removes and deletes every sub-property in `data.subProps` from `parent`, unregistering each
/// from m_subToParent, then clears `data.subProps`.
void SizePropertyManager::destroySubProperties(QtProperty *parent, Data &data)
{
    for (auto *sub : std::as_const(data.subProps)) {
        m_subToParent.remove(sub);
        parent->removeSubProperty(sub);
        delete sub;
    }
    data.subProps.clear();
}

// ── Private slots ─────────────────────────────────────────────────────────────

/// Handles a value change on a component sub-property `sub`: looks up its parent compound
/// property, writes the new value back into the parent's stored Data at the matching index, and
/// emits propertyChanged()/valueChanged() for the parent. No-op if `sub` is untracked.
void SizePropertyManager::slotDoubleChanged(QtProperty *sub, double /*value*/)
{
    auto parentIt = m_subToParent.find(sub);
    if (parentIt == m_subToParent.end()) return;

    QtProperty *parent = parentIt.value();
    auto dataIt = m_values.find(parent);
    if (dataIt == m_values.end()) return;

    Data &data = dataIt.value();
    const int idx = data.subProps.indexOf(sub);
    if (idx < 0) return;

    data.values[idx] = m_doubleManager->value(sub);
    emit propertyChanged(parent);
    emit valueChanged(parent, data.values);
}

/// Removes `sub` from m_subToParent when the sub-property manager destroys it (e.g. during
/// uninitializeProperty()/destroySubProperties(), or external destruction).
void SizePropertyManager::slotPropertyDestroyed(QtProperty *sub)
{
    m_subToParent.remove(sub);
}

// ============================================================================
//  PointPropertyManager
// ============================================================================

// ── Static helpers ───────────────────────────────────────────────────────────

/// Returns the number of numeric sub-properties for `m` (2 for XY, 3 for XYZ).
int PointPropertyManager::modeComponentCount(Mode m)
{
    return (m == XY) ? 2 : 3;
}

/// Returns the display label for each component of `m` ("X"/"Y", plus "Z" for XYZ).
QStringList PointPropertyManager::modeLabels(Mode m)
{
    if (m == XY)
        return {QStringLiteral("X"), QStringLiteral("Y")};
    return {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
}

// ── Construction ─────────────────────────────────────────────────────────────

/// Constructs the manager and its internal QtIntPropertyManager (used for the X/Y/Z sub-
/// properties), connecting its valueChanged/propertyDestroyed signals to slotIntChanged() and
/// slotPropertyDestroyed().
PointPropertyManager::PointPropertyManager(QObject *parent)
    : QtAbstractPropertyManager(parent)
    , m_intManager(new QtIntPropertyManager(this))
{
    connect(m_intManager, &QtIntPropertyManager::valueChanged,
            this, &PointPropertyManager::slotIntChanged);
    connect(m_intManager, &QtAbstractPropertyManager::propertyDestroyed,
            this, &PointPropertyManager::slotPropertyDestroyed);
}

/// Default destructor.
PointPropertyManager::~PointPropertyManager() = default;

/// Returns the internal QtIntPropertyManager that owns the component sub-properties; callers
/// must register it with an editor factory on their QtAbstractPropertyBrowser.
QtIntPropertyManager *PointPropertyManager::subIntManager() const
{
    return m_intManager;
}

// ── Value accessors ──────────────────────────────────────────────────────────

/// Returns the current component values for `property` (empty vector if `property` is not
/// managed by this instance).
QVector<int> PointPropertyManager::value(const QtProperty *property) const
{
    return m_values.value(property).values;
}

/// Returns `property`'s first two components as a QPoint (0 for any missing component); valid
/// for both XY and XYZ modes.
QPoint PointPropertyManager::valueAsQPoint(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return QPoint(v.value(0, 0), v.value(1, 0));
}

#ifdef NCR_PROP_HAS_OPENCV
/// Returns `property`'s first two components as a cv::Point (0 for any missing component);
/// intended for XY mode.
cv::Point PointPropertyManager::valueAsCvPoint(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point(v.value(0, 0), v.value(1, 0));
}

/// Returns `property`'s three components as a cv::Point3i (0 for any missing component);
/// intended for XYZ mode.
cv::Point3i PointPropertyManager::valueAsCvPoint3i(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point3i(v.value(0, 0), v.value(1, 0), v.value(2, 0));
}
#endif

/// Returns the display mode currently set for `property` (defaults to XY if unmanaged).
PointPropertyManager::Mode PointPropertyManager::mode(const QtProperty *property) const
{
    return m_values.value(property).mode;
}

/// Returns the minimum value allowed for each of `property`'s components.
int PointPropertyManager::minimum(const QtProperty *property) const
{
    return m_values.value(property).minimum;
}

/// Returns the maximum value allowed for each of `property`'s components.
int PointPropertyManager::maximum(const QtProperty *property) const
{
    return m_values.value(property).maximum;
}

/// Returns the spin/step increment used when editing each of `property`'s components.
int PointPropertyManager::singleStep(const QtProperty *property) const
{
    return m_values.value(property).singleStep;
}

// ── Mutators ─────────────────────────────────────────────────────────────────

/// Sets `property`'s component values (resizing to the current mode's component count, zero-
/// filling any missing entries) and pushes each value to its corresponding sub-property; no-op
/// if `property` is not managed here. Emits propertyChanged() and valueChanged() on success.
/// @param val the new component values; extra entries beyond the mode's count are dropped
void PointPropertyManager::setValue(QtProperty *property, const QVector<int> &val)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    Data &data = it.value();
    const int n = modeComponentCount(data.mode);
    data.values.resize(n);
    for (int i = 0; i < n; ++i)
        data.values[i] = (i < val.size()) ? val[i] : 0;

    for (int i = 0; i < qMin(n, data.subProps.size()); ++i)
        m_intManager->setValue(data.subProps[i], data.values[i]);

    emit propertyChanged(property);
    emit valueChanged(property, data.values);
}

/// Convenience overload: sets `property`'s value from a QPoint's x/y components.
void PointPropertyManager::setValue(QtProperty *property, const QPoint &val)
{
    setValue(property, QVector<int>{val.x(), val.y()});
}

#ifdef NCR_PROP_HAS_OPENCV
/// Convenience overload: sets `property`'s value from a cv::Point's x/y components.
void PointPropertyManager::setValue(QtProperty *property, const cv::Point &val)
{
    setValue(property, QVector<int>{val.x, val.y});
}

/// Convenience overload: sets `property`'s value from a cv::Point3i's x/y/z components.
void PointPropertyManager::setValue(QtProperty *property, const cv::Point3i &val)
{
    setValue(property, QVector<int>{val.x, val.y, val.z});
}
#endif

/// Switches `property` to `newMode`: destroys the existing sub-properties, resizes the stored
/// values (zero-filled) to the new component count, and rebuilds the sub-properties. No-op if
/// `property` is unmanaged or already in `newMode`. Emits modeChanged() and propertyChanged().
void PointPropertyManager::setMode(QtProperty *property, Mode newMode)
{
    auto it = m_values.find(property);
    if (it == m_values.end() || it->mode == newMode) return;

    Data &data = it.value();
    destroySubProperties(property, data);

    data.mode = newMode;
    const int n = modeComponentCount(newMode);
    data.values.resize(n, 0);
    createSubProperties(property, data);

    emit modeChanged(property, newMode);
    emit propertyChanged(property);
}

/// Sets the min/max range for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged.
void PointPropertyManager::setRange(QtProperty *property, int minVal, int maxVal)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->minimum = minVal;
    it->maximum = maxVal;
    for (auto *sub : std::as_const(it->subProps))
        m_intManager->setRange(sub, minVal, maxVal);
}

/// Sets the spin/step increment for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged.
void PointPropertyManager::setSingleStep(QtProperty *property, int step)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->singleStep = step;
    for (auto *sub : std::as_const(it->subProps))
        m_intManager->setSingleStep(sub, step);
}

// ── Display text ─────────────────────────────────────────────────────────────

/// Formats `property`'s component values as "Label:value" pairs joined with ", " and wrapped in
/// parentheses, e.g. "(X:1, Y:2)". Empty string if unmanaged.
QString PointPropertyManager::valueText(const QtProperty *property) const
{
    const auto it = m_values.constFind(property);
    if (it == m_values.constEnd()) return {};

    const Data &data = it.value();
    const QStringList labels = modeLabels(data.mode);
    QStringList parts;
    parts.reserve(data.values.size());
    for (int i = 0; i < data.values.size(); ++i)
        parts << QStringLiteral("%1:%2").arg(labels.value(i)).arg(data.values[i]);
    return QStringLiteral("(") + parts.join(QStringLiteral(", ")) + QStringLiteral(")");
}

// ── QtAbstractPropertyManager overrides ──────────────────────────────────────

/// QtAbstractPropertyManager hook invoked when `property` is added to this manager: seeds a
/// default XY Data entry (zero-filled) and creates its sub-properties.
void PointPropertyManager::initializeProperty(QtProperty *property)
{
    Data data;
    data.values = QVector<int>(modeComponentCount(data.mode), 0);
    m_values[property] = data;
    createSubProperties(property, m_values[property]);
}

/// QtAbstractPropertyManager hook invoked when `property` is removed from this manager: destroys
/// its sub-properties and erases its stored Data. No-op if `property` is unmanaged.
void PointPropertyManager::uninitializeProperty(QtProperty *property)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    destroySubProperties(property, it.value());
    m_values.erase(it);
}

// ── Private helpers ───────────────────────────────────────────────────────────

/// Creates one QtIntProperty sub-property per component of `data.mode` under `parent`,
/// configuring each from `data` (range/step/value), adding it to `parent`, and registering it in
/// m_subToParent and `data.subProps`.
void PointPropertyManager::createSubProperties(QtProperty *parent, Data &data)
{
    const QStringList labels = modeLabels(data.mode);
    const int n = modeComponentCount(data.mode);

    data.subProps.clear();
    for (int i = 0; i < n; ++i) {
        auto *sub = m_intManager->addProperty(labels[i]);
        m_intManager->setRange     (sub, data.minimum, data.maximum);
        m_intManager->setSingleStep(sub, data.singleStep);
        m_intManager->setValue     (sub, data.values.value(i, 0));
        parent->addSubProperty(sub);
        m_subToParent[sub] = parent;
        data.subProps.append(sub);
    }
}

/// Removes and deletes every sub-property in `data.subProps` from `parent`, unregistering each
/// from m_subToParent, then clears `data.subProps`.
void PointPropertyManager::destroySubProperties(QtProperty *parent, Data &data)
{
    for (auto *sub : std::as_const(data.subProps)) {
        m_subToParent.remove(sub);
        parent->removeSubProperty(sub);
        delete sub;
    }
    data.subProps.clear();
}

// ── Private slots ─────────────────────────────────────────────────────────────

/// Handles a value change on a component sub-property `sub`: looks up its parent compound
/// property, writes the new value back into the parent's stored Data at the matching index, and
/// emits propertyChanged()/valueChanged() for the parent. No-op if `sub` is untracked.
void PointPropertyManager::slotIntChanged(QtProperty *sub, int /*value*/)
{
    auto parentIt = m_subToParent.find(sub);
    if (parentIt == m_subToParent.end()) return;

    QtProperty *parent = parentIt.value();
    auto dataIt = m_values.find(parent);
    if (dataIt == m_values.end()) return;

    Data &data = dataIt.value();
    const int idx = data.subProps.indexOf(sub);
    if (idx < 0) return;

    data.values[idx] = m_intManager->value(sub);
    emit propertyChanged(parent);
    emit valueChanged(parent, data.values);
}

/// Removes `sub` from m_subToParent when the sub-property manager destroys it (e.g. during
/// uninitializeProperty()/destroySubProperties(), or external destruction).
void PointPropertyManager::slotPropertyDestroyed(QtProperty *sub)
{
    m_subToParent.remove(sub);
}

// ============================================================================
//  PointFPropertyManager
// ============================================================================

// ── Static helpers ───────────────────────────────────────────────────────────

/// Returns the number of numeric sub-properties for `m` (2 for XY, 3 for XYZ).
int PointFPropertyManager::modeComponentCount(Mode m)
{
    return (m == XY) ? 2 : 3;
}

/// Returns the display label for each component of `m` ("X"/"Y", plus "Z" for XYZ).
QStringList PointFPropertyManager::modeLabels(Mode m)
{
    if (m == XY)
        return {QStringLiteral("X"), QStringLiteral("Y")};
    return {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
}

// ── Construction ─────────────────────────────────────────────────────────────

/// Constructs the manager and its internal QtDoublePropertyManager (used for the X/Y/Z sub-
/// properties), connecting its valueChanged/propertyDestroyed signals to slotDoubleChanged() and
/// slotPropertyDestroyed().
PointFPropertyManager::PointFPropertyManager(QObject *parent)
    : QtAbstractPropertyManager(parent)
    , m_doubleManager(new QtDoublePropertyManager(this))
{
    connect(m_doubleManager, &QtDoublePropertyManager::valueChanged,
            this, &PointFPropertyManager::slotDoubleChanged);
    connect(m_doubleManager, &QtAbstractPropertyManager::propertyDestroyed,
            this, &PointFPropertyManager::slotPropertyDestroyed);
}

/// Default destructor.
PointFPropertyManager::~PointFPropertyManager() = default;

/// Returns the internal QtDoublePropertyManager that owns the component sub-properties; callers
/// must register it with an editor factory on their QtAbstractPropertyBrowser.
QtDoublePropertyManager *PointFPropertyManager::subDoubleManager() const
{
    return m_doubleManager;
}

// ── Value accessors ──────────────────────────────────────────────────────────

/// Returns the current component values for `property` (empty vector if `property` is not
/// managed by this instance).
QVector<double> PointFPropertyManager::value(const QtProperty *property) const
{
    return m_values.value(property).values;
}

/// Returns `property`'s first two components as a QPointF (0.0 for any missing component);
/// valid for both XY and XYZ modes.
QPointF PointFPropertyManager::valueAsQPointF(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return QPointF(v.value(0, 0.0), v.value(1, 0.0));
}

#ifdef NCR_PROP_HAS_OPENCV
/// Returns `property`'s first two components as a cv::Point2f (0.0 for any missing component,
/// narrowed to float); intended for XY mode.
cv::Point2f PointFPropertyManager::valueAsCvPoint2f(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point2f(static_cast<float>(v.value(0, 0.0)),
                       static_cast<float>(v.value(1, 0.0)));
}

/// Returns `property`'s first two components as a cv::Point2d (0.0 for any missing component);
/// intended for XY mode.
cv::Point2d PointFPropertyManager::valueAsCvPoint2d(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point2d(v.value(0, 0.0), v.value(1, 0.0));
}

/// Returns `property`'s three components as a cv::Point3f (0.0 for any missing component,
/// narrowed to float); intended for XYZ mode.
cv::Point3f PointFPropertyManager::valueAsCvPoint3f(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point3f(static_cast<float>(v.value(0, 0.0)),
                       static_cast<float>(v.value(1, 0.0)),
                       static_cast<float>(v.value(2, 0.0)));
}

/// Returns `property`'s three components as a cv::Point3d (0.0 for any missing component);
/// intended for XYZ mode.
cv::Point3d PointFPropertyManager::valueAsCvPoint3d(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point3d(v.value(0, 0.0), v.value(1, 0.0), v.value(2, 0.0));
}
#endif

/// Returns the display mode currently set for `property` (defaults to XY if unmanaged).
PointFPropertyManager::Mode PointFPropertyManager::mode(const QtProperty *property) const
{
    return m_values.value(property).mode;
}

/// Returns the decimal precision used to format and edit `property`'s component values.
int PointFPropertyManager::decimals(const QtProperty *property) const
{
    return m_values.value(property).decimals;
}

/// Returns the minimum value allowed for each of `property`'s components.
double PointFPropertyManager::minimum(const QtProperty *property) const
{
    return m_values.value(property).minimum;
}

/// Returns the maximum value allowed for each of `property`'s components.
double PointFPropertyManager::maximum(const QtProperty *property) const
{
    return m_values.value(property).maximum;
}

/// Returns the spin/step increment used when editing each of `property`'s components.
double PointFPropertyManager::singleStep(const QtProperty *property) const
{
    return m_values.value(property).singleStep;
}

// ── Mutators ─────────────────────────────────────────────────────────────────

/// Sets `property`'s component values (resizing to the current mode's component count, zero-
/// filling any missing entries) and pushes each value to its corresponding sub-property; no-op
/// if `property` is not managed here. Emits propertyChanged() and valueChanged() on success.
/// @param val the new component values; extra entries beyond the mode's count are dropped
void PointFPropertyManager::setValue(QtProperty *property, const QVector<double> &val)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    Data &data = it.value();
    const int n = modeComponentCount(data.mode);
    data.values.resize(n);
    for (int i = 0; i < n; ++i)
        data.values[i] = (i < val.size()) ? val[i] : 0.0;

    for (int i = 0; i < qMin(n, data.subProps.size()); ++i)
        m_doubleManager->setValue(data.subProps[i], data.values[i]);

    emit propertyChanged(property);
    emit valueChanged(property, data.values);
}

/// Convenience overload: sets `property`'s value from a QPointF's x/y components.
void PointFPropertyManager::setValue(QtProperty *property, const QPointF &val)
{
    setValue(property, QVector<double>{val.x(), val.y()});
}

#ifdef NCR_PROP_HAS_OPENCV
/// Convenience overload: sets `property`'s value from a cv::Point2f's x/y components (widened
/// to double).
void PointFPropertyManager::setValue(QtProperty *property, const cv::Point2f &val)
{
    setValue(property, QVector<double>{static_cast<double>(val.x),
                                       static_cast<double>(val.y)});
}

/// Convenience overload: sets `property`'s value from a cv::Point2d's x/y components.
void PointFPropertyManager::setValue(QtProperty *property, const cv::Point2d &val)
{
    setValue(property, QVector<double>{val.x, val.y});
}

/// Convenience overload: sets `property`'s value from a cv::Point3f's x/y/z components (widened
/// to double).
void PointFPropertyManager::setValue(QtProperty *property, const cv::Point3f &val)
{
    setValue(property, QVector<double>{static_cast<double>(val.x),
                                       static_cast<double>(val.y),
                                       static_cast<double>(val.z)});
}

/// Convenience overload: sets `property`'s value from a cv::Point3d's x/y/z components.
void PointFPropertyManager::setValue(QtProperty *property, const cv::Point3d &val)
{
    setValue(property, QVector<double>{val.x, val.y, val.z});
}
#endif

/// Switches `property` to `newMode`: destroys the existing sub-properties, resizes the stored
/// values (zero-filled) to the new component count, and rebuilds the sub-properties. No-op if
/// `property` is unmanaged or already in `newMode`. Emits modeChanged() and propertyChanged().
void PointFPropertyManager::setMode(QtProperty *property, Mode newMode)
{
    auto it = m_values.find(property);
    if (it == m_values.end() || it->mode == newMode) return;

    Data &data = it.value();
    destroySubProperties(property, data);

    data.mode = newMode;
    const int n = modeComponentCount(newMode);
    data.values.resize(n, 0.0);
    createSubProperties(property, data);

    emit modeChanged(property, newMode);
    emit propertyChanged(property);
}

/// Sets the decimal precision for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged. Emits decimalsChanged() and propertyChanged().
void PointFPropertyManager::setDecimals(QtProperty *property, int prec)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->decimals = prec;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setDecimals(sub, prec);

    emit decimalsChanged(property, prec);
    emit propertyChanged(property);
}

/// Sets the min/max range for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged.
void PointFPropertyManager::setRange(QtProperty *property, double minVal, double maxVal)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->minimum = minVal;
    it->maximum = maxVal;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setRange(sub, minVal, maxVal);
}

/// Sets the spin/step increment for `property` and propagates it to every sub-property. No-op if
/// `property` is unmanaged.
void PointFPropertyManager::setSingleStep(QtProperty *property, double step)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->singleStep = step;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setSingleStep(sub, step);
}

// ── Display text ─────────────────────────────────────────────────────────────

/// Formats `property`'s component values as "Label:value" pairs (fixed-point, using the stored
/// decimals) joined with ", " and wrapped in parentheses, e.g. "(X:1.00, Y:2.00)". Empty string
/// if unmanaged.
QString PointFPropertyManager::valueText(const QtProperty *property) const
{
    const auto it = m_values.constFind(property);
    if (it == m_values.constEnd()) return {};

    const Data &data = it.value();
    const QStringList labels = modeLabels(data.mode);
    QStringList parts;
    parts.reserve(data.values.size());
    for (int i = 0; i < data.values.size(); ++i)
        parts << QStringLiteral("%1:%2").arg(labels.value(i))
                                        .arg(data.values[i], 0, 'f', data.decimals);
    return QStringLiteral("(") + parts.join(QStringLiteral(", ")) + QStringLiteral(")");
}

// ── QtAbstractPropertyManager overrides ──────────────────────────────────────

/// QtAbstractPropertyManager hook invoked when `property` is added to this manager: seeds a
/// default XY Data entry (zero-filled) and creates its sub-properties.
void PointFPropertyManager::initializeProperty(QtProperty *property)
{
    Data data;
    data.values = QVector<double>(modeComponentCount(data.mode), 0.0);
    m_values[property] = data;
    createSubProperties(property, m_values[property]);
}

/// QtAbstractPropertyManager hook invoked when `property` is removed from this manager: destroys
/// its sub-properties and erases its stored Data. No-op if `property` is unmanaged.
void PointFPropertyManager::uninitializeProperty(QtProperty *property)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    destroySubProperties(property, it.value());
    m_values.erase(it);
}

// ── Private helpers ───────────────────────────────────────────────────────────

/// Creates one QtDoubleProperty sub-property per component of `data.mode` under `parent`,
/// configuring each from `data` (decimals/range/step/value), adding it to `parent`, and
/// registering it in m_subToParent and `data.subProps`.
void PointFPropertyManager::createSubProperties(QtProperty *parent, Data &data)
{
    const QStringList labels = modeLabels(data.mode);
    const int n = modeComponentCount(data.mode);

    data.subProps.clear();
    for (int i = 0; i < n; ++i) {
        auto *sub = m_doubleManager->addProperty(labels[i]);
        m_doubleManager->setDecimals  (sub, data.decimals);
        m_doubleManager->setRange     (sub, data.minimum, data.maximum);
        m_doubleManager->setSingleStep(sub, data.singleStep);
        m_doubleManager->setValue     (sub, data.values.value(i, 0.0));
        parent->addSubProperty(sub);
        m_subToParent[sub] = parent;
        data.subProps.append(sub);
    }
}

/// Removes and deletes every sub-property in `data.subProps` from `parent`, unregistering each
/// from m_subToParent, then clears `data.subProps`.
void PointFPropertyManager::destroySubProperties(QtProperty *parent, Data &data)
{
    for (auto *sub : std::as_const(data.subProps)) {
        m_subToParent.remove(sub);
        parent->removeSubProperty(sub);
        delete sub;
    }
    data.subProps.clear();
}

// ── Private slots ─────────────────────────────────────────────────────────────

/// Handles a value change on a component sub-property `sub`: looks up its parent compound
/// property, writes the new value back into the parent's stored Data at the matching index, and
/// emits propertyChanged()/valueChanged() for the parent. No-op if `sub` is untracked.
void PointFPropertyManager::slotDoubleChanged(QtProperty *sub, double /*value*/)
{
    auto parentIt = m_subToParent.find(sub);
    if (parentIt == m_subToParent.end()) return;

    QtProperty *parent = parentIt.value();
    auto dataIt = m_values.find(parent);
    if (dataIt == m_values.end()) return;

    Data &data = dataIt.value();
    const int idx = data.subProps.indexOf(sub);
    if (idx < 0) return;

    data.values[idx] = m_doubleManager->value(sub);
    emit propertyChanged(parent);
    emit valueChanged(parent, data.values);
}

/// Removes `sub` from m_subToParent when the sub-property manager destroys it (e.g. during
/// uninitializeProperty()/destroySubProperties(), or external destruction).
void PointFPropertyManager::slotPropertyDestroyed(QtProperty *sub)
{
    m_subToParent.remove(sub);
}
