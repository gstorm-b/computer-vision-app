#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <optional>

#include <QObject>
#include <QPalette>
#include <QString>
#include <QVector>

/// Describes a named visual style that can be registered with ThemeManager.
/// Built-in styles "light" and "dark" are always pre-registered.
/// Additional styles can be registered at runtime via ThemeManager::registerStyle().
struct ThemeStyle {
    QString id;                          ///< Unique key, e.g. "dark", "high_contrast".
    QString displayName;                 ///< Shown in the Theme menu.
    bool    isDark{false};                ///< Controls themedIcon() icon variant selection.
    std::optional<QPalette> palette;     ///< nullopt = keep current palette unchanged.
    QString qssPath;                     ///< Qt resource path to .qss file, empty = no stylesheet.
};

/// Application-wide theme/style manager: owns the registry of ThemeStyle
/// entries (built-in "light"/"dark" plus any registered at runtime), applies
/// the active style's QPalette and stylesheet to the QApplication, and
/// resolves the §5 design-token placeholders used in the project's QSS files.
/// Singleton, accessed via instance(); the instance is parented to qApp.
class ThemeManager : public QObject {
    Q_OBJECT
public:
    /// Returns the process-wide ThemeManager, lazily creating it (parented to
    /// qApp) on first call.
    static ThemeManager* instance();

    /// Register or replace a style. Emits styleRegistered if it is a new entry.
    /// If the style id matches the active style, the new definition is applied immediately.
    void registerStyle(const ThemeStyle& style);

    /// Returns a copy of all currently registered styles.
    QVector<ThemeStyle> styles() const { return m_styles; }

    /// Switch to a registered style by id. No-op if id is unknown.
    void applyStyle(const QString& id);

    /// Returns the id of the currently active style.
    QString    currentStyleId() const { return m_currentId; }
    /// Returns the full ThemeStyle currently active, or a default-constructed
    /// ThemeStyle if the current id does not match any registered style.
    ThemeStyle currentStyle()   const;
    /// Returns whether the currently active style is a dark theme.
    bool       isDark()         const;

    /// Returns the appropriate icon resource path for the current theme.
    /// Icons named "foo.svg" carry WHITE paths (for dark bg).
    /// Icons named "foo_dark.svg" carry BLACK paths (for light bg).
    QString themedIcon(const QString& basePath) const;

    /// QSS design-token resolution.
    ///
    /// Qt has no native QSS variables, so stylesheets reference design tokens by
    /// name through the placeholder syntax "@{group.token}" (e.g. "@{accent.primary}",
    /// "@{bg.surface}"). resolveTokens() substitutes every placeholder with the hex /
    /// rgba value the §5 token table (ui_design_rules.md) assigns it for the given
    /// theme. This is the one place tokens turn into literals; renaming a token is a
    /// single-table edit. Unknown tokens are left untouched (and logged once).
    ///
    /// The no-argument overload resolves for the active theme; the bool overload is
    /// used by ThemeManager::apply() while a specific style is being installed.
    QString resolveTokens(const QString& qss) const;
    /// @copydoc ThemeManager::resolveTokens(const QString&) const
    /// @param dark true to resolve against the dark-theme values, false for light
    static QString resolveTokens(const QString& qss, bool dark);

    /// Raw token lookup. Returns the value string for a token in the requested
    /// theme, or an empty string if the token name is not in the §5 table.
    static QString tokenValue(const QString& name, bool dark);

signals:
    /// Emitted after applyStyle() successfully switches the active style.
    void themeChanged(QString styleId, bool isDark);
    /// Emitted by registerStyle() when `style` is a newly registered id (not
    /// emitted when replacing an existing id's definition).
    void styleRegistered(ThemeStyle style);

private:
    /// Constructs the manager: forces the "Fusion" QStyle, pre-registers the
    /// built-in "light" and "dark" ThemeStyle entries, then applies the theme
    /// id last persisted in AppSettings.
    explicit ThemeManager(QObject* parent = nullptr);
    /// Installs `style` as the live look: sets the QApplication palette (if the
    /// style provides one) and the resolved, token-substituted stylesheet.
    void apply(const ThemeStyle& style);

    /// Builds the QPalette used for the built-in "light" style from the §5
    /// light-theme design tokens.
    static QPalette buildLightPalette();
    /// Builds the QPalette used for the built-in "dark" style from the §5
    /// dark-theme design tokens.
    static QPalette buildDarkPalette();

    QVector<ThemeStyle> m_styles;      ///< All registered styles, in registration order.
    QString             m_currentId;   ///< Id of the currently active style in m_styles.
    static ThemeManager* s_instance;   ///< Process-wide singleton instance, created lazily by instance().
};

#endif // THEME_MANAGER_H
