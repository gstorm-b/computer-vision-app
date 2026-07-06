#ifndef DEVICE_WIDGET_H
#define DEVICE_WIDGET_H

#include <QWidget>
#include <QObject>
#include <QFile>
#include "core/utils/theme_manager.h"
#include "widgets/property_browser/property_browser_widget.h"

class IDeviceWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(IDeviceWidget)

public:
    explicit IDeviceWidget(QWidget *parent = nullptr) : QWidget(parent) {}
    ~IDeviceWidget() override = default;

    virtual PropertyBrowserWidget* getPropertyBrowser() const {
        return m_propBrowser;
    }

    virtual QString deviceId() = 0;
    virtual void loadConfigToDevice() = 0;
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

    // ── Initialisation ────────────────────────────────────────────────────
    void initPropertyBrowser() {
        m_propBrowser = new PropertyBrowserWidget(this);

        m_variantManager = m_propBrowser->variantManager();
        m_variantFactory  = m_propBrowser->variantFactory();
        m_variantEditor   = m_propBrowser->browser();
    }

    // High-level wrapper (created by initPropertyBrowser)
    PropertyBrowserWidget          *m_propBrowser{nullptr};

    // Low-level pointers — kept for backward compatibility.
    // They point into m_propBrowser's internals; do not delete them.
    QtVariantPropertyManager *m_variantManager{nullptr};
    QtVariantEditorFactory   *m_variantFactory{nullptr};
    QtTreePropertyBrowser    *m_variantEditor {nullptr};

private:
    QString m_darkQssPath;
    QString m_lightQssPath;
};

#endif // DEVICE_WIDGET_H
