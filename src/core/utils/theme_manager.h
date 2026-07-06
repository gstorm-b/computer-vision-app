#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <optional>

#include <QObject>
#include <QPalette>
#include <QString>
#include <QVector>

// Describes a named visual style that can be registered with ThemeManager.
// Built-in styles "light" and "dark" are always pre-registered.
// Additional styles can be registered at runtime via ThemeManager::registerStyle().
struct ThemeStyle {
    QString id;                          // unique key, e.g. "dark", "high_contrast"
    QString displayName;                 // shown in the Theme menu
    bool    isDark{false};               // controls themedIcon() icon variant selection
    std::optional<QPalette> palette;     // nullopt = keep current palette unchanged
    QString qssPath;                     // Qt resource path to .qss file, empty = no stylesheet
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager* instance();

    // Register or replace a style. Emits styleRegistered if it is a new entry.
    // If the style id matches the active style, the new definition is applied immediately.
    void registerStyle(const ThemeStyle& style);

    QVector<ThemeStyle> styles() const { return m_styles; }

    // Switch to a registered style by id. No-op if id is unknown.
    void applyStyle(const QString& id);

    QString    currentStyleId() const { return m_currentId; }
    ThemeStyle currentStyle()   const;
    bool       isDark()         const;

    // Returns the appropriate icon resource path for the current theme.
    // Icons named "foo.svg" carry WHITE paths (for dark bg).
    // Icons named "foo_dark.svg" carry BLACK paths (for light bg).
    QString themedIcon(const QString& basePath) const;

    // QSS design-token resolution.
    //
    // Qt has no native QSS variables, so stylesheets reference design tokens by
    // name through the placeholder syntax "@{group.token}" (e.g. "@{accent.primary}",
    // "@{bg.surface}"). resolveTokens() substitutes every placeholder with the hex /
    // rgba value the §5 token table (ui_design_rules.md) assigns it for the given
    // theme. This is the one place tokens turn into literals; renaming a token is a
    // single-table edit. Unknown tokens are left untouched (and logged once).
    //
    // The no-argument overload resolves for the active theme; the bool overload is
    // used by ThemeManager::apply() while a specific style is being installed.
    QString resolveTokens(const QString& qss) const;
    static QString resolveTokens(const QString& qss, bool dark);

    // Raw token lookup. Returns the value string for a token in the requested
    // theme, or an empty string if the token name is not in the §5 table.
    static QString tokenValue(const QString& name, bool dark);

signals:
    void themeChanged(QString styleId, bool isDark);
    void styleRegistered(ThemeStyle style);

private:
    explicit ThemeManager(QObject* parent = nullptr);
    void apply(const ThemeStyle& style);

    static QPalette buildLightPalette();
    static QPalette buildDarkPalette();

    QVector<ThemeStyle> m_styles;
    QString             m_currentId;
    static ThemeManager* s_instance;
};

#endif // THEME_MANAGER_H
