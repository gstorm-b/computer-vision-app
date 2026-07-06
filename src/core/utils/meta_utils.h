#ifndef META_UTILS_H
#define META_UTILS_H

#include <qtmetamacros.h>
#include <QtTypes>
#include <QString>
#include <QMetaEnum>

template<typename T>
QString qenumToString(T value) {
    QMetaEnum metaEnum = QMetaEnum::fromType<T>();
    const char* key = metaEnum.valueToKey(static_cast<int>(value));
    return key ? QString::fromUtf8(key) : QString::number(static_cast<int>(value));
}

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

namespace vc::device {
Q_NAMESPACE

}


#endif // META_UTILS_H
