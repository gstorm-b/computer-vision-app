#ifndef TASK_WIDGET_H
#define TASK_WIDGET_H

#include <QWidget>
#include <QObject>
#include <QFile>
#include <QHBoxLayout>
#include <QList>
#include <QStackedWidget>
#include <QCoreApplication>

#include "DockWidget.h"
#include "core/utils/theme_manager.h"

#include "qtpropertybrowser/qtpropertymanager.h"
#include "qtpropertybrowser/qtvariantproperty.h"
#include "qtpropertybrowser/qttreepropertybrowser.h"

#include "ui/widgets/property_browser/property_browser_widget.h"
#include "ui/widgets/property_browser/prop_spec.h"

#include "model/itask.h"

/// Base class for all task-configuration widgets.
///
/// Property browser setup
/// ─────────────────────
/// Call initPropertyBrowser(container) once in the subclass constructor.
/// This creates a PropertyBrowserWidget (search + tree + description) and
/// embeds it in `container`.
///
/// The three low-level pointers (m_variantManager, m_variantFactory,
/// m_variantEditor) are kept for backward compatibility with existing code
/// that accesses them directly.  New code should prefer m_propBrowser.
///
/// Completer on a string property
/// ───────────────────────────────
///   auto *p = m_variantManager->addProperty(QMetaType::QString, "Tag");
///   m_variantManager->setAttribute(p, "completer",
///                                  QStringList{"alpha", "beta", "gamma"});
class ITaskWidget : public QWidget {
    Q_OBJECT

public:
    /// Constructs the base widget, storing the owning dock and the task it
    /// configures; subclasses build their actual UI on top of this.
    explicit ITaskWidget(std::shared_ptr<vc::model::ITask> task,
                         ads::CDockWidget *dock = nullptr,
                         QWidget *parent = nullptr)
        : QWidget(parent),
          m_dock(dock),
          m_task(task)
    {}

    /// Writes the widget's current UI state into the underlying task/model.
    virtual void loadConfigToTask()   = 0;
    /// Populates the widget's UI controls from the underlying task/model state.
    virtual void loadConfigToWidget() = 0;

protected:
    // ── Theme reload ──────────────────────────────────────────────────────
    /// Call once from the subclass constructor when the widget has a per-form
    /// QSS pair. Stores the paths, triggers an initial load, and subscribes
    /// to ThemeManager::themeChanged for subsequent switches.
    /// @param darkPath resource path to the dark-theme QSS file
    /// @param lightPath resource path to the light-theme QSS file
    void setupThemeReload(const QString &darkPath, const QString &lightPath) {
        m_darkQssPath  = darkPath;
        m_lightQssPath = lightPath;
        reloadStyleSheet();
        connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                this, [this](const QString &, bool) { reloadStyleSheet(); });
    }

    /// Loads the per-form QSS for the active theme.  No-op when paths are
    /// empty (widget relies on the global sheet only).  Subclasses may
    /// override to run additional logic after the sheet is applied.
    virtual void reloadStyleSheet() {
        const QString path = ThemeManager::instance()->isDark()
            ? m_darkQssPath : m_lightQssPath;
        if (path.isEmpty()) return;
        QFile f(path);
        if (f.open(QFile::ReadOnly | QFile::Text))
            setStyleSheet(ThemeManager::instance()->resolveTokens(
                QString::fromUtf8(f.readAll())));
    }

    // ── Initialization ────────────────────────────────────────────────────
    /// Creates the PropertyBrowserWidget (m_propBrowser) and its manager/
    /// factory/editor pointers, then embeds it into `container` via
    /// embedBrowserInWidget(). No-op if `container` is null.
    /// @param container widget to host the property browser
    void initPropertyBrowser(QWidget *container) {
        if (container == nullptr) {
            return;
        }

        m_propBrowser = new PropertyBrowserWidget(this);

        m_variantManager = m_propBrowser->variantManager();
        m_variantFactory  = m_propBrowser->variantFactory();
        m_variantEditor   = m_propBrowser->browser();

        if (container) embedBrowserInWidget(container);
    }

    /// Embeds m_propBrowser into `container` via a zero-margin QHBoxLayout.
    /// No-op if `container` is null.
    /// @param container widget to host the property browser
    void embedBrowserInWidget(QWidget *container) {
        if (container == nullptr) {
            return;
        }

        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_propBrowser);    
    }

    /// Builds a QStackedWidget (m_browserStackWidget) inside `container`'s new
    /// zero-margin QHBoxLayout (m_browserBox), for switching between multiple
    /// PropertyBrowserWidget instances. No-op if `container` is null.
    void initBrowserInWidget(QWidget *container) {
        if (container) {
            m_browserBox = new QHBoxLayout(container);
            m_browserStackWidget = new QStackedWidget();
            m_browserBox->addWidget(m_browserStackWidget);
            m_browserBox->setContentsMargins(0, 0, 0, 0);
        }
    }

    /// Switches m_browserStackWidget to show `wg`, adding it to the stack
    /// first (and appending it to m_propertyBrowserWidgets) if it isn't
    /// already tracked.
    void changePropertyBrowserWidget(PropertyBrowserWidget *wg) {
        if (!m_propertyBrowserWidgets.contains(wg)) {
            m_propertyBrowserWidgets.append(wg);
            m_browserStackWidget->addWidget(wg);
            m_browserStackWidget->setCurrentWidget(wg);
            return; 
        }

        m_browserStackWidget->setCurrentWidget(wg);
    }

    /// Switches m_browserStackWidget back to the default m_propBrowser,
    /// lazily creating it (and adding it to the stack) on first use.
    void changePropertyBrowserDefault() {
        if (!m_propBrowser) {
            m_propBrowser = new PropertyBrowserWidget(this);
            m_browserStackWidget->addWidget(m_propBrowser);
        }
        m_browserStackWidget->setCurrentWidget(m_propBrowser);
    }

    /// Removes `wg` from m_browserStackWidget and from m_propertyBrowserWidgets.
    /// No-op if `wg` isn't currently tracked.
    void removePropertyBrowserWidget(PropertyBrowserWidget *wg) {
        if (!m_propertyBrowserWidgets.contains(wg)) {
            return;
        }

        m_propertyBrowserWidgets.removeAll(wg);
        m_browserStackWidget->removeWidget(wg);
    }

    // ── Reflection helper ─────────────────────────────────────────────────
    /// Introspects a QMetaProperty and creates the matching QtVariantProperty:
    /// builds an enum property (with translated enum names) for enum-typed
    /// properties, otherwise a plain value property; applies display name,
    /// numeric min/max, and tooltip from Q_CLASSINFO entries named
    /// `<prop>_name` / `<prop>_min` / `<prop>_max` / `<prop>_desc`; disables
    /// the property when the QMetaProperty isn't writable; adds it to
    /// `browser` if given. The `objectName` property is always skipped.
    /// Reads min/max/name from Q_CLASSINFO entries:
    ///   Q_CLASSINFO("speed_min", "0")
    ///   Q_CLASSINFO("speed_max", "100")
    ///   Q_CLASSINFO("speed_name", "Speed (m/s)")
    /// @param meta meta-object of the class owning `prop` (used for QMetaEnum/Q_CLASSINFO lookups)
    /// @param prop the reflected property to build an editor for
    /// @param value the property's current value
    /// @param manager QtVariantPropertyManager used to create the property
    /// @param browser optional tree browser to add the created property to
    /// @return the created QtVariantProperty, or nullptr if creation failed or the property is `objectName`
    static QtVariantProperty *addPropertyToBrowser(
            const QMetaObject        &meta,
            QMetaProperty            &prop,
            QVariant                 &value,
            QtVariantPropertyManager *manager,
            QtTreePropertyBrowser    *browser)
    {
        const QString propName = QString::fromLatin1(prop.name());
        if (propName == QLatin1String("objectName")) return nullptr;

        QtVariantProperty *variantProp = nullptr;

        if (prop.isEnumType()) {
            variantProp = manager->addProperty(QtVariantPropertyManager::enumTypeId(), propName);

            QStringList enumNames;
            const QMetaEnum metaEnum = prop.enumerator();
            enumNames.reserve(metaEnum.keyCount());
            for (int j = 0; j < metaEnum.keyCount(); ++j) {
                enumNames << QCoreApplication::translate(meta.className(),
                                                         metaEnum.key(j));
            }
            variantProp->setAttribute(QLatin1String("enumNames"), enumNames);
            variantProp->setValue(value.toInt());
        } else {
            variantProp = manager->addProperty(value.userType(), propName);
            if (variantProp) variantProp->setValue(value);
        }

        if (!variantProp) return nullptr;

        // Display name override
        const int nameIdx = meta.indexOfClassInfo(
            QStringLiteral("%1_name").arg(propName).toUtf8());
        if (nameIdx != -1) {
            const QString displayName = QString::fromUtf8(meta.classInfo(nameIdx).value());
            if (!displayName.isEmpty())
                variantProp->setDisplayName(displayName);
        }

        // Numeric range (stored as strings in Q_CLASSINFO)
        const int minIdx = meta.indexOfClassInfo(
            QStringLiteral("%1_min").arg(propName).toUtf8());
        if (minIdx != -1)
            variantProp->setAttribute(QStringLiteral("minimum"),
                                      QString::fromUtf8(meta.classInfo(minIdx).value()).toDouble());

        const int maxIdx = meta.indexOfClassInfo(
            QStringLiteral("%1_max").arg(propName).toUtf8());
        if (maxIdx != -1)
            variantProp->setAttribute(QStringLiteral("maximum"),
                                      QString::fromUtf8(meta.classInfo(maxIdx).value()).toDouble());

        // Tooltip / description
        const int tipIdx = meta.indexOfClassInfo(
            QStringLiteral("%1_desc").arg(propName).toUtf8());
        if (tipIdx != -1)
            variantProp->setDescriptionToolTip(
                QString::fromUtf8(meta.classInfo(tipIdx).value()));

        if (!prop.isWritable())
            variantProp->setEnabled(false);

        if (browser)
            browser->addProperty(variantProp);

        return variantProp;
    }

protected:
    ads::CDockWidget                *m_dock{nullptr};  ///< Owning dock widget, if any; not owned by this widget.
    std::shared_ptr<vc::model::ITask> m_task;  ///< The task this widget configures.

    // High-level wrapper (created by initPropertyBrowser)
    PropertyBrowserWidget          *m_propBrowser{nullptr};  ///< Default property browser (search + tree + description), created by initPropertyBrowser().

    // Low-level pointers — kept for backward compatibility.
    // They point into m_propBrowser's internals; do not delete them.
    QtVariantPropertyManager *m_variantManager{nullptr};  ///< Alias into m_propBrowser's variant manager; do not delete.
    QtVariantEditorFactory   *m_variantFactory{nullptr};  ///< Alias into m_propBrowser's variant factory; do not delete.
    QtTreePropertyBrowser    *m_variantEditor {nullptr};  ///< Alias into m_propBrowser's tree browser; do not delete.

    QHBoxLayout *m_browserBox{nullptr};  ///< Zero-margin layout created by initBrowserInWidget() to host m_browserStackWidget.
    QStackedWidget *m_browserStackWidget{nullptr};  ///< Stack switched between multiple PropertyBrowserWidget instances by changePropertyBrowserWidget()/changePropertyBrowserDefault().
    QList<PropertyBrowserWidget*> m_propertyBrowserWidgets;  ///< Non-default property browsers currently tracked in m_browserStackWidget.

private:
    QString m_darkQssPath;  ///< Resource path to the dark-theme QSS file passed to setupThemeReload().
    QString m_lightQssPath;  ///< Resource path to the light-theme QSS file passed to setupThemeReload().
};

#endif // TASK_WIDGET_H
