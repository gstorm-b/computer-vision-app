#ifndef META_UTILS_H
#define META_UTILS_H

#include <qtmetamacros.h>
#include <QtTypes>
#include <QString>
#include <QMetaEnum>

/**
 * @file meta_utils.h
 * @brief Template helpers for converting Q_ENUM/Q_ENUM_NS values to/from their
 *        declared key names via QMetaEnum (qenumToString / stringToQEnum).
 */

/**
 * @brief Converts a Q_ENUM/Q_ENUM_NS-registered enum value to its declared key name via QMetaEnum.
 * @param[in] value the enum value to convert
 * @return the enum's key name (e.g. "Foo" for `T::Foo`), or the numeric value as a string if
 *         `T` has no matching key (e.g. a value outside the declared enumerators)
 */
template<typename T>
QString qenumToString(T value) {
    QMetaEnum metaEnum = QMetaEnum::fromType<T>();
    const char* key = metaEnum.valueToKey(static_cast<int>(value));
    return key ? QString::fromUtf8(key) : QString::number(static_cast<int>(value));
}

/**
 * @brief Parses a Q_ENUM/Q_ENUM_NS key name back into its enum value via QMetaEnum.
 * @param[in] str          the key name to look up (e.g. "Foo" for `T::Foo`)
 * @param[in] defaultValue value returned when `str` does not match any key of `T`
 * @return the matching enum value, or `defaultValue` if `str` is not a valid key
 */
template<typename T>
T stringToQEnum(const QString& str, T defaultValue) {
    QMetaEnum metaEnum = QMetaEnum::fromType<T>();
    bool ok;
    int value = metaEnum.keyToValue(str.toUtf8().constData(), &ok);
    if (ok) {
        return static_cast<T>(value);
    }
    return defaultValue;
}

/// Namespace holder for device-related enums that need Qt meta-object enum reflection
/// (Q_ENUM_NS) without belonging to any QObject-derived class.
namespace vc::device {
Q_NAMESPACE

}


#endif // META_UTILS_H
