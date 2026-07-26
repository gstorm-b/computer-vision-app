#ifndef DEVICE_WIDGET_H
#define DEVICE_WIDGET_H

#include <QWidget>
#include <QObject>
#include <QFile>
#include "core/utils/theme_manager.h"
#include "ui/widgets/property_browser/property_browser_widget.h"

/// Abstract base for device-configuration widgets (one per connected device type,
/// e.g. camera, robot). Owns the shared PropertyBrowserWidget plumbing and the
/// optional per-form QSS reload wiring; concrete subclasses supply the actual
/// device id/config load behaviour.
class IDeviceWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(IDeviceWidget)

public:
    /// Constructs the widget without any device attached yet.
    explicit IDeviceWidget(QWidget *parent = nullptr) : QWidget(parent) {}
    /// Default destructor; subclasses own/delete their own UI members.
    ~IDeviceWidget() override = default;

    /// Returns the property browser created by initPropertyBrowser(), or
    /// nullptr if initPropertyBrowser() was never called.
    virtual PropertyBrowserWidget* getPropertyBrowser() const {
        return m_propBrowser;
    }

    /// Returns the identifier of the device this widget is bound to.
    virtual QString deviceId() = 0;
    /// Pushes the widget's currently-edited configuration values down onto the device.
    virtual void loadConfigToDevice() = 0;
    /// Refreshes the widget's displayed values from the device's current configuration.
    virtual void loadConfigToWidget() = 0;

protected:
    // ── Theme reload ──────────────────────────────────────────────────────
    /// Call once from the subclass constructor when the widget has a per-form
    /// QSS pair. Stores the paths, triggers an initial load, and subscribes
    /// to ThemeManager::themeChanged for subsequent switches.
    /// @param darkPath per-form QSS file used when the dark theme is active
    /// @param lightPath per-form QSS file used when the light theme is active
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

    // ── Initialisation ────────────────────────────────────────────────────
    /// Creates the PropertyBrowserWidget for this widget and caches its
    /// manager/factory/editor pointers into the members below.
    void initPropertyBrowser() {
        m_propBrowser = new PropertyBrowserWidget(this);

        m_variantManager = m_propBrowser->variantManager();
        m_variantFactory  = m_propBrowser->variantFactory();
        m_variantEditor   = m_propBrowser->browser();
    }

    // High-level wrapper (created by initPropertyBrowser)
    PropertyBrowserWidget          *m_propBrowser{nullptr};  ///< Property-browser wrapper; owns the objects below.

    /// Low-level pointers — kept for backward compatibility.
    /// They point into m_propBrowser's internals; do not delete them.
    QtVariantPropertyManager *m_variantManager{nullptr};  ///< Variant property manager owned by m_propBrowser.
    QtVariantEditorFactory   *m_variantFactory{nullptr};  ///< Variant editor factory owned by m_propBrowser.
    QtTreePropertyBrowser    *m_variantEditor {nullptr};  ///< Tree property browser view owned by m_propBrowser.

private:
    QString m_darkQssPath;  ///< Path to the per-form QSS file used in dark theme (empty if none).
    QString m_lightQssPath;  ///< Path to the per-form QSS file used in light theme (empty if none).
};

#endif // DEVICE_WIDGET_H
