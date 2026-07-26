#ifndef WINDOWS_HELPER_H
#define WINDOWS_HELPER_H

#include <QTime>
#include <QLabel>
#include <QTextEdit>
#include <QCalendarWidget>
#include <QFrame>
#include <QTreeView>
#include <QFileSystemModel>
#include <QBoxLayout>
#include <QSettings>
#include <QDockWidget>
#include <QDebug>
#include <QResizeEvent>
#include <QAction>
#include <QWidgetAction>
#include <QComboBox>
#include <QInputDialog>
#include <QRubberBand>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QScreen>
#include <QStyle>
#include <QMessageBox>
#include <QMenu>
#include <QToolButton>
#include <QToolBar>
#include <QIconEngine>
#include <QPainter>
#include <QPointer>
#include <QMap>
#include <QElapsedTimer>
#include <QPixmapCache>
#include <QRandomGenerator>

// #include "DockAreaTabBar.h"
// #include "DockAreaTitleBar.h"
// #include "DockAreaWidget.h"
// #include "DockComponentsFactory.h"
// #include "DockManager.h"
// #include "DockSplitter.h"
// #include "FloatingDockContainer.h"
// #include "ImageViewer.h"
// #include "MyDockAreaTitleBar.h"
// #include "StatusDialog.h"

#include "DockWidget.h"
// #include "ads_globals.h"
#include "core/utils/theme_manager.h"

/// Returns a pseudo-random integer in the range [0, highest) using the global
/// QRandomGenerator.
static int randomNumberBounded(int highest)
{
    return QRandomGenerator::global()->bounded(highest);
}


/// Builds a compact feature-flags string for `DockWidget` showing its closable (c),
/// movable (m), and floatable (f) features as +/- suffixes, e.g. a not-closable but
/// movable and floatable widget yields "c- m+ f+".
static QString featuresString(ads::CDockWidget* DockWidget)
{
    auto f = DockWidget->features();
    return QString("c%1 m%2 f%3")
        .arg(f.testFlag(ads::CDockWidget::DockWidgetClosable) ? "+" : "-")
        .arg(f.testFlag(ads::CDockWidget::DockWidgetMovable) ? "+" : "-")
        .arg(f.testFlag(ads::CDockWidget::DockWidgetFloatable) ? "+" : "-");
}


/// Appends the string returned by featuresString() (in parentheses) to the current
/// window title of the given DockWidget.
static void appendFeaturStringToWindowTitle(ads::CDockWidget* DockWidget)
{
    DockWidget->setWindowTitle(DockWidget->windowTitle()
                               +  QString(" (%1)").arg(featuresString(DockWidget)));
}

/// QIconEngine that resolves an SVG icon path through ThemeManager (so the icon follows
/// the active light/dark theme/style) and caches rendered pixmaps in QPixmapCache keyed
/// by style, path, mode, state, and size to avoid re-rendering the SVG each time.
class ThemedSvgIconEngine final : public QIconEngine
{
public:
    /// Constructs the engine for the themed SVG at `basePath`, rendered at `intent`
    /// pixels square when pixmap()/actualSize() are asked for an invalid size.
    explicit ThemedSvgIconEngine(QString basePath, int intent)
        : m_basePath(std::move(basePath)), m_intent(intent)
    {
    }

    /// Returns a new engine with the same base path and intent size (QIconEngine
    /// polymorphic-copy contract).
    QIconEngine *clone() const override
    {
        return new ThemedSvgIconEngine(m_basePath, m_intent);
    }

    /// Returns the plugin key identifying this QIconEngine implementation.
    QString key() const override
    {
        return QStringLiteral("ThemedSvgIconEngine");
    }

    /// Renders (or fetches from QPixmapCache) the themed SVG at `size` for the given
    /// icon `mode`/`state`, caching the result keyed by style, path, mode, state, and size.
    /// @param size desired pixmap size; falls back to an m_intent square when invalid
    /// @return the rendered or cached pixmap
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        const QString resolvedPath = themedPath();
        const QSize requestedSize = size.isValid() ? size : QSize(m_intent, m_intent);
        const QString cacheKey = QStringLiteral("themed-svg:%1:%2:%3:%4:%5x%6")
                                     .arg(styleKey(),
                                          resolvedPath,
                                          QString::number(static_cast<int>(mode)),
                                          QString::number(static_cast<int>(state)),
                                          QString::number(requestedSize.width()),
                                          QString::number(requestedSize.height()));

        QPixmap cached;
        if (QPixmapCache::find(cacheKey, &cached))
            return cached;

        QIcon icon(resolvedPath);
        icon.addPixmap(icon.pixmap(m_intent));
        const QPixmap pm = icon.pixmap(requestedSize, mode, state);
        QPixmapCache::insert(cacheKey, pm);
        return pm;
    }

    /// Paints the themed pixmap rendered at `rect`'s size into `rect`; does nothing if
    /// `painter` is null.
    void paint(QPainter *painter, const QRect &rect,
               QIcon::Mode mode, QIcon::State state) override
    {
        if (!painter)
            return;
        painter->drawPixmap(rect, pixmap(rect.size(), mode, state));
    }

    /// Returns the actual (unscaled) size of the themed SVG icon for the given
    /// mode/state, delegating to a temporary QIcon built from themedPath().
    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        const QIcon icon(themedPath());
        return icon.actualSize(size, mode, state);
    }

private:
    /// Resolves m_basePath through ThemeManager for the currently active theme; returns
    /// m_basePath unchanged if no QApplication instance exists yet.
    QString themedPath() const
    {
        if (!qApp)
            return m_basePath;
        return ThemeManager::instance()->themedIcon(m_basePath);
    }

    /// Returns an identifier for the currently active style, used as part of the
    /// pixmap-cache key; returns a placeholder string if there is no QApplication or the
    /// style id has not been set yet.
    QString styleKey() const
    {
        if (!qApp)
            return QStringLiteral("no-app");
        const QString currentId = ThemeManager::instance()->currentStyleId();
        return currentId.isEmpty() ? QStringLiteral("uninitialized") : currentId;
    }

private:
    QString m_basePath;  ///< Untheme-resolved base path to the SVG icon file.
    int m_intent{92};    ///< Default square render size (px) used when no valid size is requested.
};

/// Creates a QIcon backed by a ThemedSvgIconEngine for `File`, rendered at `intent` pixels.
static QIcon svgIcon(const QString& File, int intent = 92)
{
    return QIcon(new ThemedSvgIconEngine(File, intent));
}


#endif // WINDOWS_HELPER_H
