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

// ============================================================================
//  ITaskWidget
//
//  Base class for all task-configuration widgets.
//
//  Property browser setup
//  ─────────────────────
//  Call initPropertyBrowser(container) once in the subclass constructor.
//  This creates a PropertyBrowserWidget (search + tree + description) and
//  embeds it in `container`.
//
//  The three low-level pointers (m_variantManager, m_variantFactory,
//  m_variantEditor) are kept for backward compatibility with existing code
//  that accesses them directly.  New code should prefer m_propBrowser.
//
//  Completer on a string property
//  ───────────────────────────────
//    auto *p = m_variantManager->addProperty(QMetaType::QString, "Tag");
//    m_variantManager->setAttribute(p, "completer",
//                                   QStringList{"alpha", "beta", "gamma"});
// ============================================================================

class ITaskWidget : public QWidget {
    Q_OBJECT

public:
    explicit ITaskWidget(std::shared_ptr<vc::model::ITask> task,
                         ads::CDockWidget *dock = nullptr,
                         QWidget *parent = nullptr)
        : QWidget(parent),
          m_dock(dock),
          m_task(task)
    {}

    virtual void loadConfigToTask()   = 0;
    virtual void loadConfigToWidget() = 0;

protected:
    // ── Theme reload ──────────────────────────────────────────────────────
    // Call once from the subclass constructor when the widget has a per-form
    // QSS pair. Stores the paths, triggers an initial load, and subscribes
    // to ThemeManager::themeChanged for subsequent switches.
    void setupThemeReload(const QString &darkPath, const QString &lightPath) {
        m_darkQssPath  = darkPath;
        m_lightQssPath = lightPath;
        reloadStyleSheet();
        connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                this, [this](const QString &, bool) { reloadStyleSheet(); });
    }

    // Loads the per-form QSS for the active theme.  No-op when paths are
    // empty (widget relies on the global sheet only).  Subclasses may
    // override to run additional logic after the sheet is applied.
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
    /**
     * @brief initPropertyBrowser: create property widget
     * @param container
     */
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

    /**
     * @brief embedBrowserInWidget: embed property widget into container widget
     * @param container
     */
    void embedBrowserInWidget(QWidget *container) {
        if (container == nullptr) {
            return;
        }

        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(m_propBrowser);    
    }

    void initBrowserInWidget(QWidget *container) {
        if (container) {
            m_browserBox = new QHBoxLayout(container);
            m_browserStackWidget = new QStackedWidget();
            m_browserBox->addWidget(m_browserStackWidget);
            m_browserBox->setContentsMargins(0, 0, 0, 0);
        }
    }

    void changePropertyBrowserWidget(PropertyBrowserWidget *wg) {
        if (!m_propertyBrowserWidgets.contains(wg)) {
            m_propertyBrowserWidgets.append(wg);
            m_browserStackWidget->addWidget(wg);
            m_browserStackWidget->setCurrentWidget(wg);
            return; 
        }

        m_browserStackWidget->setCurrentWidget(wg);
    }

    void changePropertyBrowserDefault() {
        if (!m_propBrowser) {
            m_propBrowser = new PropertyBrowserWidget(this);
            m_browserStackWidget->addWidget(m_propBrowser);
        }
        m_browserStackWidget->setCurrentWidget(m_propBrowser);
    }

    void removePropertyBrowserWidget(PropertyBrowserWidget *wg) {
        if (!m_propertyBrowserWidgets.contains(wg)) {
            return;
        }

        m_propertyBrowserWidgets.removeAll(wg);
        m_browserStackWidget->removeWidget(wg);
    }

    // ── Reflection helper ─────────────────────────────────────────────────
    // Introspects a QMetaProperty and creates the matching QtVariantProperty.
    // Reads min/max/name from Q_CLASSINFO entries:
    //   Q_CLASSINFO("speed_min", "0")
    //   Q_CLASSINFO("speed_max", "100")
    //   Q_CLASSINFO("speed_name", "Speed (m/s)")
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
    ads::CDockWidget                *m_dock{nullptr};
    std::shared_ptr<vc::model::ITask> m_task;

    // High-level wrapper (created by initPropertyBrowser)
    PropertyBrowserWidget          *m_propBrowser{nullptr};

    // Low-level pointers — kept for backward compatibility.
    // They point into m_propBrowser's internals; do not delete them.
    QtVariantPropertyManager *m_variantManager{nullptr};
    QtVariantEditorFactory   *m_variantFactory{nullptr};
    QtTreePropertyBrowser    *m_variantEditor {nullptr};

    QHBoxLayout *m_browserBox{nullptr};
    QStackedWidget *m_browserStackWidget{nullptr};
    QList<PropertyBrowserWidget*> m_propertyBrowserWidgets;

private:
    QString m_darkQssPath;
    QString m_lightQssPath;
};

#endif // TASK_WIDGET_H
