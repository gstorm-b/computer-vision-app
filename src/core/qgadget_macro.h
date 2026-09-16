#ifndef QGADGET_MARCO_H
#define QGADGET_MARCO_H

#include <qtmetamacros.h>
#include <QtTypes>
#include <QCoreApplication>
#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QMetaEnum>
#include <QMetaObject>
#include <QMetaProperty>
#include <QStringList>

/**
 * @file qgadget_macro.h
 * @brief Q_GADGET property-declaration macros and gadget meta-property helpers
 *        (displayName, writeProperty, readProperty) for Q_GADGET config types.
 *
 * Property-browser widgets dispatch edits through the Q_PROPERTY system of a
 * Q_GADGET config; the helpers in vc::gadget_meta centralise the
 * "indexOfProperty → read/writeOnGadget" pattern and the "<prop>_name"
 * Q_CLASSINFO display-name lookup emitted by the macros below, so each widget
 * no longer re-implements it.
 */

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

namespace vc::gadget_meta {

/// Resolves the display name registered via Q_CLASSINFO("<prop>_name", "…"), translated.
///
/// This is the ONE place that reads "<prop>_name". It used to be one of six — the property
/// browsers each carried their own copy of the same six lines, and every copy translated the
/// enum keys beside it while none translated the label. That is not five oversights, it is
/// one block that was copied five times, so the lookup lives here now and the widgets call
/// it.
///
/// The translation context is meta.className(), matching what those same widgets already
/// pass for enum keys. The source strings themselves are invisible to lupdate, because
/// Q_CLASSINFO is not something it reads — each config header carries a QT_TRANSLATE_NOOP
/// marker table beside its properties so they can be extracted, and the architecture
/// contract test asserts the two agree.
///
/// @return the translated display name, or `propName` itself if no class-info entry exists.
inline QString displayName(const QMetaObject &meta, const char *propName) {
    const QByteArray key = QByteArray(propName) + "_name";
    const int idx = meta.indexOfClassInfo(key.constData());
    if (idx >= 0) {
        const char *raw = meta.classInfo(idx).value();
        if (raw && *raw)
            return QCoreApplication::translate(meta.className(), raw);
    }
    return QString::fromUtf8(propName);
}

/// Resolves the translated labels for an enum property's keys, in declaration order.
///
/// The context is QMetaEnum::scope() — the class or namespace that REGISTERED the enum —
/// because that is the context lupdate records for a QT_TR_NOOP marker sitting in that same
/// scope (see enum_keys_basler_defines in device/camera/basler_define.h).
///
/// The five property browsers each used to pass `meta.className()` here, which is the
/// *config* class, not the enum's scope. For every enum in this project those differ
/// ("vc::device::BaslerGigeCfg" vs "vc::device::basler"), so a translated key could never
/// have been found. Nothing looked broken only because all 33 keys were still untranslated —
/// filling them in would have changed nothing on screen.
inline QStringList enumKeyNames(const QMetaProperty &prop) {
    const QMetaEnum metaEnum = prop.enumerator();
    QStringList names;
    names.reserve(metaEnum.keyCount());
    for (int i = 0; i < metaEnum.keyCount(); ++i)
        names << QCoreApplication::translate(metaEnum.scope(), metaEnum.key(i));
    return names;
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
