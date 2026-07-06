#include "custom_property_managers.h"
#include <QLocale>

// ============================================================================
//  PositionPropertyManager
// ============================================================================

// ── Static helpers ───────────────────────────────────────────────────────────

int PositionPropertyManager::modeComponentCount(Mode m)
{
    switch (m) {
    case XY:     return 2;
    case XYZ:    return 3;
    case XYZRPY: return 6;
    }
    return 2;
}

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

PositionPropertyManager::PositionPropertyManager(QObject *parent)
    : QtAbstractPropertyManager(parent)
    , m_doubleManager(new QtDoublePropertyManager(this))
{
    connect(m_doubleManager, &QtDoublePropertyManager::valueChanged,
            this, &PositionPropertyManager::slotDoubleChanged);
    connect(m_doubleManager, &QtAbstractPropertyManager::propertyDestroyed,
            this, &PositionPropertyManager::slotPropertyDestroyed);
}

PositionPropertyManager::~PositionPropertyManager() = default;

QtDoublePropertyManager *PositionPropertyManager::subDoubleManager() const
{
    return m_doubleManager;
}

// ── Value accessors ──────────────────────────────────────────────────────────

QVector<double> PositionPropertyManager::value(const QtProperty *property) const
{
    return m_values.value(property).values;
}

PositionPropertyManager::Mode PositionPropertyManager::mode(const QtProperty *property) const
{
    return m_values.value(property).mode;
}

int PositionPropertyManager::decimals(const QtProperty *property) const
{
    return m_values.value(property).decimals;
}

double PositionPropertyManager::minimum(const QtProperty *property) const
{
    return m_values.value(property).minimum;
}

double PositionPropertyManager::maximum(const QtProperty *property) const
{
    return m_values.value(property).maximum;
}

double PositionPropertyManager::singleStep(const QtProperty *property) const
{
    return m_values.value(property).singleStep;
}

// ── Mutators ─────────────────────────────────────────────────────────────────

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

void PositionPropertyManager::setRange(QtProperty *property, double minVal, double maxVal)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->minimum = minVal;
    it->maximum = maxVal;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setRange(sub, minVal, maxVal);
}

void PositionPropertyManager::setSingleStep(QtProperty *property, double step)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->singleStep = step;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setSingleStep(sub, step);
}

// ── Display text ─────────────────────────────────────────────────────────────

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

void PositionPropertyManager::initializeProperty(QtProperty *property)
{
    Data data;
    data.values = QVector<double>(modeComponentCount(data.mode), 0.0);
    m_values[property] = data;
    createSubProperties(property, m_values[property]);
}

void PositionPropertyManager::uninitializeProperty(QtProperty *property)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    destroySubProperties(property, it.value());
    m_values.erase(it);
}

// ── Private helpers ───────────────────────────────────────────────────────────

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

void PositionPropertyManager::slotPropertyDestroyed(QtProperty *sub)
{
    m_subToParent.remove(sub);
}

// ============================================================================
//  SizePropertyManager
// ============================================================================

// ── Static helpers ───────────────────────────────────────────────────────────

int SizePropertyManager::modeComponentCount(Mode m)
{
    return (m == WH) ? 2 : 3;
}

QStringList SizePropertyManager::modeLabels(Mode m)
{
    if (m == WH)
        return {QStringLiteral("Width"), QStringLiteral("Height")};
    return {QStringLiteral("Width"), QStringLiteral("Height"), QStringLiteral("Depth")};
}

// ── Construction ─────────────────────────────────────────────────────────────

SizePropertyManager::SizePropertyManager(QObject *parent)
    : QtAbstractPropertyManager(parent)
    , m_doubleManager(new QtDoublePropertyManager(this))
{
    connect(m_doubleManager, &QtDoublePropertyManager::valueChanged,
            this, &SizePropertyManager::slotDoubleChanged);
    connect(m_doubleManager, &QtAbstractPropertyManager::propertyDestroyed,
            this, &SizePropertyManager::slotPropertyDestroyed);
}

SizePropertyManager::~SizePropertyManager() = default;

QtDoublePropertyManager *SizePropertyManager::subDoubleManager() const
{
    return m_doubleManager;
}

// ── Value accessors ──────────────────────────────────────────────────────────

QVector<double> SizePropertyManager::value(const QtProperty *property) const
{
    return m_values.value(property).values;
}

SizePropertyManager::Mode SizePropertyManager::mode(const QtProperty *property) const
{
    return m_values.value(property).mode;
}

int SizePropertyManager::decimals(const QtProperty *property) const
{
    return m_values.value(property).decimals;
}

double SizePropertyManager::minimum(const QtProperty *property) const
{
    return m_values.value(property).minimum;
}

double SizePropertyManager::maximum(const QtProperty *property) const
{
    return m_values.value(property).maximum;
}

double SizePropertyManager::singleStep(const QtProperty *property) const
{
    return m_values.value(property).singleStep;
}

// ── Mutators ─────────────────────────────────────────────────────────────────

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

void SizePropertyManager::setRange(QtProperty *property, double minVal, double maxVal)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->minimum = minVal;
    it->maximum = maxVal;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setRange(sub, minVal, maxVal);
}

void SizePropertyManager::setSingleStep(QtProperty *property, double step)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->singleStep = step;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setSingleStep(sub, step);
}

// ── Display text ─────────────────────────────────────────────────────────────

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

void SizePropertyManager::initializeProperty(QtProperty *property)
{
    Data data;
    data.values = QVector<double>(modeComponentCount(data.mode), 0.0);
    m_values[property] = data;
    createSubProperties(property, m_values[property]);
}

void SizePropertyManager::uninitializeProperty(QtProperty *property)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    destroySubProperties(property, it.value());
    m_values.erase(it);
}

// ── Private helpers ───────────────────────────────────────────────────────────

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

void SizePropertyManager::slotPropertyDestroyed(QtProperty *sub)
{
    m_subToParent.remove(sub);
}

// ============================================================================
//  PointPropertyManager
// ============================================================================

// ── Static helpers ───────────────────────────────────────────────────────────

int PointPropertyManager::modeComponentCount(Mode m)
{
    return (m == XY) ? 2 : 3;
}

QStringList PointPropertyManager::modeLabels(Mode m)
{
    if (m == XY)
        return {QStringLiteral("X"), QStringLiteral("Y")};
    return {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
}

// ── Construction ─────────────────────────────────────────────────────────────

PointPropertyManager::PointPropertyManager(QObject *parent)
    : QtAbstractPropertyManager(parent)
    , m_intManager(new QtIntPropertyManager(this))
{
    connect(m_intManager, &QtIntPropertyManager::valueChanged,
            this, &PointPropertyManager::slotIntChanged);
    connect(m_intManager, &QtAbstractPropertyManager::propertyDestroyed,
            this, &PointPropertyManager::slotPropertyDestroyed);
}

PointPropertyManager::~PointPropertyManager() = default;

QtIntPropertyManager *PointPropertyManager::subIntManager() const
{
    return m_intManager;
}

// ── Value accessors ──────────────────────────────────────────────────────────

QVector<int> PointPropertyManager::value(const QtProperty *property) const
{
    return m_values.value(property).values;
}

QPoint PointPropertyManager::valueAsQPoint(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return QPoint(v.value(0, 0), v.value(1, 0));
}

#ifdef NCR_PROP_HAS_OPENCV
cv::Point PointPropertyManager::valueAsCvPoint(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point(v.value(0, 0), v.value(1, 0));
}

cv::Point3i PointPropertyManager::valueAsCvPoint3i(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point3i(v.value(0, 0), v.value(1, 0), v.value(2, 0));
}
#endif

PointPropertyManager::Mode PointPropertyManager::mode(const QtProperty *property) const
{
    return m_values.value(property).mode;
}

int PointPropertyManager::minimum(const QtProperty *property) const
{
    return m_values.value(property).minimum;
}

int PointPropertyManager::maximum(const QtProperty *property) const
{
    return m_values.value(property).maximum;
}

int PointPropertyManager::singleStep(const QtProperty *property) const
{
    return m_values.value(property).singleStep;
}

// ── Mutators ─────────────────────────────────────────────────────────────────

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

void PointPropertyManager::setValue(QtProperty *property, const QPoint &val)
{
    setValue(property, QVector<int>{val.x(), val.y()});
}

#ifdef NCR_PROP_HAS_OPENCV
void PointPropertyManager::setValue(QtProperty *property, const cv::Point &val)
{
    setValue(property, QVector<int>{val.x, val.y});
}

void PointPropertyManager::setValue(QtProperty *property, const cv::Point3i &val)
{
    setValue(property, QVector<int>{val.x, val.y, val.z});
}
#endif

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

void PointPropertyManager::setRange(QtProperty *property, int minVal, int maxVal)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->minimum = minVal;
    it->maximum = maxVal;
    for (auto *sub : std::as_const(it->subProps))
        m_intManager->setRange(sub, minVal, maxVal);
}

void PointPropertyManager::setSingleStep(QtProperty *property, int step)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->singleStep = step;
    for (auto *sub : std::as_const(it->subProps))
        m_intManager->setSingleStep(sub, step);
}

// ── Display text ─────────────────────────────────────────────────────────────

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

void PointPropertyManager::initializeProperty(QtProperty *property)
{
    Data data;
    data.values = QVector<int>(modeComponentCount(data.mode), 0);
    m_values[property] = data;
    createSubProperties(property, m_values[property]);
}

void PointPropertyManager::uninitializeProperty(QtProperty *property)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    destroySubProperties(property, it.value());
    m_values.erase(it);
}

// ── Private helpers ───────────────────────────────────────────────────────────

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

void PointPropertyManager::slotPropertyDestroyed(QtProperty *sub)
{
    m_subToParent.remove(sub);
}

// ============================================================================
//  PointFPropertyManager
// ============================================================================

// ── Static helpers ───────────────────────────────────────────────────────────

int PointFPropertyManager::modeComponentCount(Mode m)
{
    return (m == XY) ? 2 : 3;
}

QStringList PointFPropertyManager::modeLabels(Mode m)
{
    if (m == XY)
        return {QStringLiteral("X"), QStringLiteral("Y")};
    return {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
}

// ── Construction ─────────────────────────────────────────────────────────────

PointFPropertyManager::PointFPropertyManager(QObject *parent)
    : QtAbstractPropertyManager(parent)
    , m_doubleManager(new QtDoublePropertyManager(this))
{
    connect(m_doubleManager, &QtDoublePropertyManager::valueChanged,
            this, &PointFPropertyManager::slotDoubleChanged);
    connect(m_doubleManager, &QtAbstractPropertyManager::propertyDestroyed,
            this, &PointFPropertyManager::slotPropertyDestroyed);
}

PointFPropertyManager::~PointFPropertyManager() = default;

QtDoublePropertyManager *PointFPropertyManager::subDoubleManager() const
{
    return m_doubleManager;
}

// ── Value accessors ──────────────────────────────────────────────────────────

QVector<double> PointFPropertyManager::value(const QtProperty *property) const
{
    return m_values.value(property).values;
}

QPointF PointFPropertyManager::valueAsQPointF(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return QPointF(v.value(0, 0.0), v.value(1, 0.0));
}

#ifdef NCR_PROP_HAS_OPENCV
cv::Point2f PointFPropertyManager::valueAsCvPoint2f(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point2f(static_cast<float>(v.value(0, 0.0)),
                       static_cast<float>(v.value(1, 0.0)));
}

cv::Point2d PointFPropertyManager::valueAsCvPoint2d(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point2d(v.value(0, 0.0), v.value(1, 0.0));
}

cv::Point3f PointFPropertyManager::valueAsCvPoint3f(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point3f(static_cast<float>(v.value(0, 0.0)),
                       static_cast<float>(v.value(1, 0.0)),
                       static_cast<float>(v.value(2, 0.0)));
}

cv::Point3d PointFPropertyManager::valueAsCvPoint3d(const QtProperty *property) const
{
    const auto v = m_values.value(property).values;
    return cv::Point3d(v.value(0, 0.0), v.value(1, 0.0), v.value(2, 0.0));
}
#endif

PointFPropertyManager::Mode PointFPropertyManager::mode(const QtProperty *property) const
{
    return m_values.value(property).mode;
}

int PointFPropertyManager::decimals(const QtProperty *property) const
{
    return m_values.value(property).decimals;
}

double PointFPropertyManager::minimum(const QtProperty *property) const
{
    return m_values.value(property).minimum;
}

double PointFPropertyManager::maximum(const QtProperty *property) const
{
    return m_values.value(property).maximum;
}

double PointFPropertyManager::singleStep(const QtProperty *property) const
{
    return m_values.value(property).singleStep;
}

// ── Mutators ─────────────────────────────────────────────────────────────────

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

void PointFPropertyManager::setValue(QtProperty *property, const QPointF &val)
{
    setValue(property, QVector<double>{val.x(), val.y()});
}

#ifdef NCR_PROP_HAS_OPENCV
void PointFPropertyManager::setValue(QtProperty *property, const cv::Point2f &val)
{
    setValue(property, QVector<double>{static_cast<double>(val.x),
                                       static_cast<double>(val.y)});
}

void PointFPropertyManager::setValue(QtProperty *property, const cv::Point2d &val)
{
    setValue(property, QVector<double>{val.x, val.y});
}

void PointFPropertyManager::setValue(QtProperty *property, const cv::Point3f &val)
{
    setValue(property, QVector<double>{static_cast<double>(val.x),
                                       static_cast<double>(val.y),
                                       static_cast<double>(val.z)});
}

void PointFPropertyManager::setValue(QtProperty *property, const cv::Point3d &val)
{
    setValue(property, QVector<double>{val.x, val.y, val.z});
}
#endif

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

void PointFPropertyManager::setRange(QtProperty *property, double minVal, double maxVal)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->minimum = minVal;
    it->maximum = maxVal;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setRange(sub, minVal, maxVal);
}

void PointFPropertyManager::setSingleStep(QtProperty *property, double step)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    it->singleStep = step;
    for (auto *sub : std::as_const(it->subProps))
        m_doubleManager->setSingleStep(sub, step);
}

// ── Display text ─────────────────────────────────────────────────────────────

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

void PointFPropertyManager::initializeProperty(QtProperty *property)
{
    Data data;
    data.values = QVector<double>(modeComponentCount(data.mode), 0.0);
    m_values[property] = data;
    createSubProperties(property, m_values[property]);
}

void PointFPropertyManager::uninitializeProperty(QtProperty *property)
{
    auto it = m_values.find(property);
    if (it == m_values.end()) return;

    destroySubProperties(property, it.value());
    m_values.erase(it);
}

// ── Private helpers ───────────────────────────────────────────────────────────

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

void PointFPropertyManager::slotPropertyDestroyed(QtProperty *sub)
{
    m_subToParent.remove(sub);
}
