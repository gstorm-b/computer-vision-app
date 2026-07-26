#include "theme_manager.h"
#include "core/app_settings/app_settings.h"
#include "core/logger/app_logger.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStyle>
#include <QStyleFactory>

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance() {
    if (!s_instance)
        s_instance = new ThemeManager(qApp);
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {
    qApp->setStyle(QStyleFactory::create("Fusion"));

    ThemeStyle light;
    light.id          = "light";
    light.displayName = tr("Light");
    light.isDark      = false;
    light.palette     = buildLightPalette();
    light.qssPath     = ":/styles/light.qss";
    registerStyle(light);

    ThemeStyle dark;
    dark.id          = "dark";
    dark.displayName = tr("Dark");
    dark.isDark      = true;
    dark.palette     = buildDarkPalette();
    dark.qssPath     = ":/styles/dark.qss";
    registerStyle(dark);

    applyStyle(AppSettings::instance()->theme());
}

void ThemeManager::registerStyle(const ThemeStyle& style) {
    for (auto& s : m_styles) {
        if (s.id == style.id) {
            s = style;
            if (m_currentId == style.id)
                apply(style);
            return;
        }
    }
    m_styles.append(style);
    emit styleRegistered(style);
}

void ThemeManager::applyStyle(const QString& id) {
    if (m_currentId == id) return;
    for (const auto& s : m_styles) {
        if (s.id == id) {
            m_currentId = id;
            apply(s);
            AppSettings::instance()->setTheme(id);
            emit themeChanged(id, s.isDark);
            return;
        }
    }
}

ThemeStyle ThemeManager::currentStyle() const {
    for (const auto& s : m_styles)
        if (s.id == m_currentId) return s;
    return {};
}

bool ThemeManager::isDark() const {
    return currentStyle().isDark;
}

/// Internal storage for the §5 QSS design-token table used by resolveTokens()/tokenValue().
namespace {

/// Canonical §5 design tokens (ui_design_rules.md). The {dark, light} value pair
/// for each token is the single source of truth for QSS colour resolution; it must
/// stay in sync with the §5 table and with buildDark/LightPalette() per §11.2.
struct TokenValue { const char* dark; const char* light; };

/// Returns the static, lazily-initialized table mapping each §5 token name to
/// its {dark, light} hex/rgba value pair.
const QHash<QString, TokenValue>& tokenTable() {
    static const QHash<QString, TokenValue> table = {
        // §5.1 Background
        {"bg.window",             {"#1a1c20", "#f2f3f5"}},
        {"bg.surface",            {"#22252a", "#ffffff"}},
        {"bg.elevated",           {"#2c3038", "#e8eaee"}},
        {"bg.input",              {"#2c3038", "#ffffff"}},
        {"bg.disabled",           {"#1e2024", "#eeeff1"}},
        // §5.2 Border
        {"border.default",        {"#3a3f48", "#c8ccd4"}},
        {"border.strong",         {"#4e5562", "#a8adb8"}},
        {"border.focus",          {"#e87c00", "#c06400"}},
        {"border.disabled",       {"#2e3238", "#dddfe3"}},
        // §5.3 Text
        {"text.primary",          {"#e0e4ea", "#1a1c20"}},
        {"text.muted",            {"#7a8898", "#5a6878"}},
        {"text.disabled",         {"#4a5260", "#a0a8b0"}},
        {"text.on-accent",        {"#ffffff", "#ffffff"}},
        {"text.link",             {"#ffa040", "#9a5000"}},
        // §5.4 Accent + Selection
        {"accent.primary",        {"#e87c00", "#c06400"}},
        {"accent.bright",         {"#ffa040", "#e08020"}},
        {"accent.pressed",        {"#b05a00", "#8a4400"}},
        {"accent.pressed.deep",   {"#7a3c00", "#5a3800"}},
        {"accent.subtle",         {"rgba(232,124,0,0.12)", "rgba(192,100,0,0.09)"}},
        {"selection.bg",          {"#3d2e10", "#fde8c0"}},
        {"selection.text",        {"#e0e4ea", "#1a1c20"}},
        // §5.5 State + State surface
        {"state.error",           {"#ff5252", "#c0282a"}},
        {"state.warning",         {"#ffb020", "#9a6800"}},
        {"state.success",         {"#40c870", "#1a7a40"}},
        {"state.info",            {"#40a8e0", "#0060a0"}},
        {"state.error.surface",   {"rgba(255,82,82,0.10)",  "rgba(192,40,42,0.08)"}},
        {"state.warning.surface", {"rgba(255,176,32,0.10)", "rgba(154,104,0,0.08)"}},
        {"state.success.surface", {"rgba(64,200,112,0.10)", "rgba(26,122,64,0.08)"}},
        {"state.info.surface",    {"rgba(64,168,224,0.10)", "rgba(0,96,160,0.08)"}},
        // §5.6 Panel accent
        {"panel.accent.deep",     {"#1e1c16", "#f5ede0"}},
        {"panel.accent.mid",      {"#252118", "#faebd0"}},
        {"panel.accent.surface",  {"#2e2a1e", "#f5e4c0"}},
        {"panel.accent.border",   {"#403a28", "#d8c89a"}},
        // §5.7 Device identity
        {"device.camera",         {"#3a9bd9", "#1f6fb8"}},
        {"device.camera.bright",  {"#5cb3e8", "#3a8ad0"}},
        {"device.camera.deep",    {"#2278b8", "#155a98"}},
        {"device.camera.tint.bg", {"rgba(58,155,217,0.12)", "rgba(31,111,184,0.08)"}},
        {"device.camera.tint.bd", {"rgba(58,155,217,0.40)", "rgba(31,111,184,0.36)"}},
        {"device.plc",            {"#3ac98a", "#1a8a55"}},
        {"device.plc.bright",     {"#5cdca2", "#28a468"}},
        {"device.plc.deep",       {"#22a070", "#0e6a40"}},
        {"device.plc.tint.bg",    {"rgba(58,201,138,0.12)", "rgba(26,138,85,0.08)"}},
        {"device.plc.tint.bd",    {"rgba(58,201,138,0.40)", "rgba(26,138,85,0.36)"}},
        {"device.output",         {"#a87de0", "#7a4ec0"}},
        {"device.output.bright",  {"#bf98ea", "#9168d0"}},
        {"device.output.deep",    {"#8458c0", "#5e389a"}},
        {"device.output.tint.bg", {"rgba(168,125,224,0.12)", "rgba(122,78,192,0.08)"}},
        {"device.output.tint.bd", {"rgba(168,125,224,0.40)", "rgba(122,78,192,0.36)"}},
        {"device.robot",          {"#e0b020", "#a07000"}},
        {"device.robot.bright",   {"#f0c850", "#b88810"}},
        {"device.robot.deep",     {"#b88a10", "#7a5400"}},
        {"device.robot.tint.bg",  {"rgba(224,176,32,0.12)", "rgba(160,112,0,0.08)"}},
        {"device.robot.tint.bd",  {"rgba(224,176,32,0.40)", "rgba(160,112,0,0.36)"}},
        {"device.default",        {"#7a8ca8", "#4a5868"}},
        {"device.default.bright", {"#94a4be", "#5e6c80"}},
        {"device.default.deep",   {"#5a6c88", "#36404e"}},
        {"device.default.tint.bg",{"rgba(122,140,168,0.12)", "rgba(74,88,104,0.08)"}},
        {"device.default.tint.bd",{"rgba(122,140,168,0.40)", "rgba(74,88,104,0.36)"}},
        // §5.8 State bright/deep stops
        {"state.success.bright",  {"#2eff98", "#44c878"}},
        {"state.error.bright",    {"#ff8a8a", "#e84848"}},
        {"state.warning.bright",  {"#ffd060", "#d09020"}},
        {"state.error.deep",      {"#922015", "#6e1010"}},
        // §5.9 Overlay helpers
        {"overlay.scrim.weak",    {"rgba(0,0,0,0.08)", "rgba(0,0,0,0.08)"}},
        {"overlay.scrim.med",     {"rgba(0,0,0,0.16)", "rgba(0,0,0,0.16)"}},
        {"overlay.scrim.strong",  {"rgba(0,0,0,0.32)", "rgba(0,0,0,0.32)"}},
        {"overlay.tint.weak",     {"rgba(255,255,255,0.04)", "rgba(255,255,255,0.04)"}},
        {"overlay.tint.med",      {"rgba(255,255,255,0.10)", "rgba(255,255,255,0.10)"}},
        {"overlay.tint.strong",   {"rgba(255,255,255,0.24)", "rgba(255,255,255,0.24)"}},
    };
    return table;
}

} // namespace

QString ThemeManager::tokenValue(const QString& name, bool dark) {
    const auto it = tokenTable().constFind(name);
    if (it == tokenTable().cend())
        return QString();
    return QString::fromLatin1(dark ? it->dark : it->light);
}

QString ThemeManager::resolveTokens(const QString& qss, bool dark) {
    // Match "@{group.token}" placeholders. Token names use lower-case words joined
    // by '.' or '-'; the brace boundary keeps the match unambiguous next to QSS
    // punctuation (';', ')', '}').
    static const QRegularExpression rx(QStringLiteral(R"(@\{([a-z0-9.\-]+)\})"));
    static QSet<QString> warnedUnknown;

    QString out;
    out.reserve(qss.size());
    qsizetype last = 0;
    auto it = rx.globalMatch(qss);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += qss.mid(last, m.capturedStart() - last);
        const QString name = m.captured(1);
        const QString value = tokenValue(name, dark);
        if (value.isEmpty()) {
            // Unknown token: leave the placeholder verbatim so the defect is
            // visible rather than silently rendering a wrong colour.
            out += m.captured(0);
            if (!warnedUnknown.contains(name)) {
                warnedUnknown.insert(name);
                LOG_DEV_ERR << "ThemeManager: unknown QSS design token" << name;
            }
        } else {
            out += value;
        }
        last = m.capturedEnd();
    }
    out += qss.mid(last);
    return out;
}

QString ThemeManager::resolveTokens(const QString& qss) const {
    return resolveTokens(qss, isDark());
}

QString ThemeManager::themedIcon(const QString& basePath) const {
    if (isDark()) return basePath;
    const int dot = basePath.lastIndexOf('.');
    if (dot < 0) return basePath;
    const QString darkPath = basePath.left(dot) + "_dark" + basePath.mid(dot);
    return QFile::exists(darkPath) ? darkPath : basePath;
}

void ThemeManager::apply(const ThemeStyle& style) {
    if (style.palette.has_value())
        qApp->setPalette(*style.palette);

    QString qss;
    if (!style.qssPath.isEmpty()) {
        QFile f(style.qssPath);
        if (f.open(QFile::ReadOnly | QFile::Text))
            qss = QString::fromUtf8(f.readAll());
    }
    // Resolve §5 design tokens for the style being installed. style.isDark is used
    // (not isDark()) because m_currentId may not yet point at this style when
    // registerStyle() applies an updated definition.
    qApp->setStyleSheet(resolveTokens(qss, style.isDark));
}

QPalette ThemeManager::buildLightPalette() {
    QPalette p;
    // §5 design tokens — light theme
    const QColor bgWindow   {0xf2, 0xf3, 0xf5};  // bg.window
    const QColor bgSurface  {0xff, 0xff, 0xff};  // bg.surface
    const QColor bgElevated {0xe8, 0xea, 0xee};  // bg.elevated
    const QColor textPrimary{0x1a, 0x1c, 0x20};  // text.primary
    const QColor accent     {0xc0, 0x64, 0x00};  // accent.primary
    const QColor border     {0xc8, 0xcc, 0xd4};  // border.default
    const QColor disabled   {0xaa, 0xb0, 0xbc};  // text.disabled

    p.setColor(QPalette::Window,          bgWindow);
    p.setColor(QPalette::WindowText,      textPrimary);
    p.setColor(QPalette::Base,            bgSurface);
    p.setColor(QPalette::AlternateBase,   bgElevated);
    p.setColor(QPalette::Text,            textPrimary);
    p.setColor(QPalette::BrightText,      Qt::black);
    p.setColor(QPalette::Button,          bgElevated);
    p.setColor(QPalette::ButtonText,      textPrimary);
    p.setColor(QPalette::Highlight,       accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Light,           bgSurface);   // ADS focus_highlighting.css: palette(light) → bg.surface
    p.setColor(QPalette::Midlight,        bgElevated);
    p.setColor(QPalette::Dark,            border);      // ADS focus_highlighting.css: palette(dark) → border.default
    p.setColor(QPalette::Mid,             border);
    p.setColor(QPalette::Shadow,          QColor{0x80, 0x80, 0x80});
    p.setColor(QPalette::Link,            QColor{0x00, 0x66, 0xcc});
    p.setColor(QPalette::LinkVisited,     QColor{0x88, 0x00, 0xcc});
    p.setColor(QPalette::ToolTipBase,     bgSurface);
    p.setColor(QPalette::ToolTipText,     textPrimary);

    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text,       disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight,  border);

    return p;
}

QPalette ThemeManager::buildDarkPalette() {
    QPalette p;
    // §5 design tokens — dark theme
    const QColor bgWindow   {0x1a, 0x1c, 0x20};  // bg.window
    const QColor bgSurface  {0x22, 0x25, 0x2a};  // bg.surface
    const QColor bgElevated {0x2c, 0x30, 0x38};  // bg.elevated
    const QColor textPrimary{0xe0, 0xe4, 0xea};  // text.primary
    const QColor accent     {0xe8, 0x7c, 0x00};  // accent.primary
    const QColor border     {0x3a, 0x3f, 0x48};  // border.default
    const QColor disabled   {0x4a, 0x52, 0x60};  // text.disabled

    p.setColor(QPalette::Window,          bgWindow);
    p.setColor(QPalette::WindowText,      textPrimary);
    p.setColor(QPalette::Base,            bgWindow);
    p.setColor(QPalette::AlternateBase,   bgElevated);
    p.setColor(QPalette::Text,            textPrimary);
    p.setColor(QPalette::BrightText,      Qt::white);
    p.setColor(QPalette::Button,          bgElevated);
    p.setColor(QPalette::ButtonText,      textPrimary);
    p.setColor(QPalette::Highlight,       accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Light,           bgSurface);   // ADS focus_highlighting.css: palette(light) → bg.surface
    p.setColor(QPalette::Midlight,        bgElevated);
    p.setColor(QPalette::Dark,            border);      // ADS focus_highlighting.css: palette(dark) → border.default
    p.setColor(QPalette::Mid,             border);
    p.setColor(QPalette::Shadow,          QColor{0x11, 0x13, 0x16});
    p.setColor(QPalette::Link,            QColor{0x4f, 0xc1, 0xff});
    p.setColor(QPalette::LinkVisited,     QColor{0xb5, 0x89, 0xff});
    p.setColor(QPalette::ToolTipBase,     bgSurface);
    p.setColor(QPalette::ToolTipText,     textPrimary);

    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Text,       disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight,  QColor{0x55, 0x55, 0x55});

    return p;
}
