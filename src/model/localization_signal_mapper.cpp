#include "model/localization_signal_mapper.h"

#include <QMetaProperty>

#include "core/logger/app_logger.h"

/// Application data-model types: tasks, device bindings, and localization signal mapping.
namespace vc::model {

/// Translation-unit-local helpers backing LocalizationSignalMapper::configure().
namespace {

/// TaskLocalizeConfig gadget property names eligible for PLC-tag mapping; configure() looks up
/// each by name via QMetaObject so the list order does not affect behavior.
constexpr const char *kSignalFields[] = {
    "nActiveCamera",
    "nActivePatternGroup",
    "nDetectedNumber",
    "nFaultCode",
    "bCameraValid",
    "bPatternValid",
    "bTaskReady",
    "bExecuteTrigger",
    "bMatchingFinished",
    "bMatchingBusy",
    "bMatchingDetected",
    "bMatchingLowArea",
    "bTaskFault",
};

/// Reads the trimmed-nothing string value of gadget property `fieldName` from `config` via
/// QMetaObject reflection.
/// @param config the localization config gadget to read from
/// @param fieldName name of a TaskLocalizeConfig property (see kSignalFields)
/// @return the property's string value, or an empty string if `fieldName` is not a known property
QString readSignalTag(const TaskLocalizeConfig &config, const char *fieldName)
{
    const QMetaObject &meta = TaskLocalizeConfig::staticMetaObject;
    const int idx = meta.indexOfProperty(fieldName);
    if (idx < 0)
        return QString();

    return meta.property(idx).readOnGadget(&config).toString();
}

} // namespace

void LocalizationSignalMapper::configure(const TaskLocalizeConfig &config)
{
    m_tagToSignalName.clear();
    m_signalNameToTag.clear();

    for (const char *fieldName : kSignalFields) {
        const QString tag = readSignalTag(config, fieldName).trimmed();
        if (tag.isEmpty())
            continue;

        if (m_tagToSignalName.contains(tag)) {
            LOG_DEV_ERR << "LocalizationSignalMapper: duplicate PLC tag"
                        << tag
                        << "existing signal"
                        << m_tagToSignalName.value(tag)
                        << "new signal"
                        << fieldName;
        }

        const QString signalName = QString::fromUtf8(fieldName);
        m_tagToSignalName.insert(tag, signalName);
        m_signalNameToTag.insert(signalName, tag);
    }
}

void LocalizationSignalMapper::clear()
{
    m_tagToSignalName.clear();
    m_signalNameToTag.clear();
}

QString LocalizationSignalMapper::signalNameForTag(const QString &tag) const
{
    return m_tagToSignalName.value(tag);
}

QString LocalizationSignalMapper::tagForSignalName(const QString &name) const
{
    return m_signalNameToTag.value(name);
}

QList<LocalizationSignalEvent> LocalizationSignalMapper::mapValues(
    const QMap<QString, QVariant> &values) const
{
    QList<LocalizationSignalEvent> events;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const QString signalName = signalNameForTag(it.key());
        if (signalName.isEmpty()) {
            // LOG_DEV_ERR << "LocalizationSignalMapper: unmapped PLC tag" << it.key();
            continue;
        }

        events.append({it.key(), signalName, it.value()});
    }
    return events;
}

} // namespace vc::model
