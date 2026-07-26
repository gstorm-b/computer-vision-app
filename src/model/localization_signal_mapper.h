#ifndef LOCALIZATION_SIGNAL_MAPPER_H
#define LOCALIZATION_SIGNAL_MAPPER_H

#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>

#include "model/task_localization_config.h"

/// Application data-model types: tasks, device bindings, and localization signal mapping.
namespace vc::model {

/// One mapped PLC signal update, ready to be dispatched under its TaskLocalizeConfig property name.
struct LocalizationSignalEvent {
    QString tag;    ///< PLC tag/address the value was read from.
    QString name;   ///< Matching TaskLocalizeConfig gadget property (signal) name.
    QVariant value; ///< Value read from the PLC for this tag.
};

/// Bidirectional lookup between PLC tags and the TaskLocalizeConfig signal names they are
/// configured to (see kSignalFields in the .cpp), used to translate raw PLC reads into
/// named localization signal events.
class LocalizationSignalMapper {
public:
    /// Rebuilds the tag<->signal maps from `config`'s gadget properties (kSignalFields); empty
    /// tags are skipped and duplicate tags are logged (LOG_DEV_ERR) but keep the first mapping.
    void configure(const TaskLocalizeConfig &config);
    /// Clears both the tag->signal and signal->tag maps.
    void clear();

    /// @return the configured signal name for `tag`, or an empty string if `tag` is unmapped.
    QString signalNameForTag(const QString &tag) const;
    /// @return the PLC tag configured for signal `name`, or an empty string if `name` is unmapped.
    QString tagForSignalName(const QString &name) const;
    /// Translates raw tag->value PLC reads into LocalizationSignalEvent entries, dropping any
    /// tag that has no configured signal name.
    /// @param values raw PLC tag->value reads
    /// @return one event per recognized tag (unmapped tags are silently skipped)
    QList<LocalizationSignalEvent> mapValues(const QMap<QString, QVariant> &values) const;
    /// @return true if configure() has not been called (or clear() was), i.e. no tags are mapped.
    bool isEmpty() const { return m_tagToSignalName.isEmpty(); }

private:
    QMap<QString, QString> m_tagToSignalName; ///< PLC tag -> TaskLocalizeConfig signal name.
    QMap<QString, QString> m_signalNameToTag; ///< TaskLocalizeConfig signal name -> PLC tag.
};

} // namespace vc::model

#endif // LOCALIZATION_SIGNAL_MAPPER_H
