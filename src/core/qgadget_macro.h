#ifndef QGADGET_MARCO_H
#define QGADGET_MARCO_H

#include <qtmetamacros.h>
#include <QtTypes>
#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QMetaEnum>
#include <QMetaObject>
#include <QMetaProperty>

/// Declares a read-write numeric Q_GADGET property `name` of `type`, backed by member
/// `m_##name`, plus Q_CLASSINFO entries recording its display name and its [minVal,
/// maxVal] range (consumed by property-browser widgets for validation/UI bounds).
#define G_PROPERTY_NUMBER_READWRITE(type, name, minVal, maxVal, displayName) \
    Q_PROPERTY(type name READ name WRITE set##name) \
    Q_CLASSINFO(#name "_min", #minVal) \
    Q_CLASSINFO(#name "_max", #maxVal) \
    Q_CLASSINFO(#name "_name", displayName) \
    public: \
    type name() const { return m_##name; } \
    void set##name(type val) { m_##name = val; } \

/// Declares a read-write string-like Q_GADGET property `name` of `type`, backed by
/// member `m_##name`, plus a Q_CLASSINFO entry recording its display name.
#define G_PROPERTY_STRING_READWRITE(type, name, displayName) \
    Q_PROPERTY(type name READ name WRITE set##name) \
    Q_CLASSINFO(#name "_name", displayName) \
    public: \
    type name() const { return m_##name; } \
    void set##name(type val) { m_##name = val; } \

/// Declares a read-only (CONSTANT) string-like Q_GADGET property `name` of `type`,
/// backed by member `m_##name`, plus a Q_CLASSINFO entry recording its display name.
/// No setter is generated.
#define G_PROPERTY_STRING_READ(type, name, displayName) \
    Q_PROPERTY(type name READ name CONSTANT) \
    Q_CLASSINFO(#name "_name", displayName) \
    public: \
    type name() const { return m_##name; } \
    // void set##name(type val) { m_##name = val; } \

/// Declares a read-write boolean Q_GADGET property `name` of `type`, backed by member
/// `m_##name`, plus a Q_CLASSINFO entry recording its display name.
#define G_PROPERTY_BOOL_READWRITE(type, name, displayName) \
Q_PROPERTY(type name READ name WRITE set##name) \
    Q_CLASSINFO(#name "_name", displayName) \
    public: \
    type name() const { return m_##name; } \
    void set##name(type val) { m_##name = val; } \


/// Declares a read-write enum Q_GADGET property `name` of `type`, backed by member
/// `m_##name`, plus a Q_CLASSINFO entry recording its display name.
#define G_PROPERTY_ENUM_READWRITE(type, name, displayName) \
Q_PROPERTY(type name READ name WRITE set##name) \
    Q_CLASSINFO(#name "_name", displayName) \
    public: \
    type name() const { return m_##name; } \
    void set##name(type val) { m_##name = val; } \

/// Like G_PROPERTY_STRING_READWRITE, but for a d-pointer (PIMPL) class: the
/// getter/setter access `d->m_##name` instead of a direct member, so the class must
/// have an accessible `d` pointer with that member.
#define P_PROPERTY_STRING_READWRITE(type, name, displayName) \
Q_PROPERTY(type name READ name WRITE set##name) \
    Q_CLASSINFO(#name "_name", displayName) \
    public: \
    type name() const { return d->m_##name; } \
    void set##name(type val) { d->m_##name = val; } \

/// Shared gadget meta-property helpers. Property-browser widgets dispatch edits
/// through the Q_PROPERTY system of a Q_GADGET config; these helpers centralize the
/// "indexOfProperty → read/write OnGadget" pattern and the "<prop>_name" Q_CLASSINFO
/// display-name lookup emitted by the macros above, so each widget no longer
/// re-implements it.
namespace vc::gadget_meta {

/// Resolves the display name registered via Q_CLASSINFO("<prop>_name", "…").
/// @return the registered display name, or `propName` itself if no class-info entry exists.
inline QString displayName(const QMetaObject &meta, const char *propName) {
    const QByteArray key = QByteArray(propName) + "_name";
    const int idx = meta.indexOfClassInfo(key.constData());
    if (idx >= 0) {
        const QString v = QString::fromUtf8(meta.classInfo(idx).value());
        if (!v.isEmpty()) return v;
    }
    return QString::fromUtf8(propName);
}

/// Writes `value` into the gadget's property `propName`.
/// @return false if the property is unknown or the meta-write fails.
inline bool writeProperty(const QMetaObject &meta, void *gadget,
                          const QString &propName, const QVariant &value) {
    const int idx = meta.indexOfProperty(propName.toUtf8().constData());
    if (idx < 0) return false;
    return meta.property(idx).writeOnGadget(gadget, value);
}

/// Reads the gadget's property `propName`.
/// @return the property's current value, or an invalid QVariant if unknown.
inline QVariant readProperty(const QMetaObject &meta, const void *gadget,
                             const QString &propName) {
    const int idx = meta.indexOfProperty(propName.toUtf8().constData());
    if (idx < 0) return {};
    return meta.property(idx).readOnGadget(gadget);
}

} // namespace vc::gadget_meta

#endif // QGADGET_MARCO_H
