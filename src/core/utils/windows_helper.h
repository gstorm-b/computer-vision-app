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

/**
 * Returns a random number from 0 to highest - 1
 */
static int randomNumberBounded(int highest)
{
    return QRandomGenerator::global()->bounded(highest);
}


/**
 * Function returns a features string with closable (c), movable (m) and floatable (f)
 * features. i.e. The following string is for a not closable but movable and floatable
 * widget: c- m+ f+
 */
static QString featuresString(ads::CDockWidget* DockWidget)
{
    auto f = DockWidget->features();
    return QString("c%1 m%2 f%3")
        .arg(f.testFlag(ads::CDockWidget::DockWidgetClosable) ? "+" : "-")
        .arg(f.testFlag(ads::CDockWidget::DockWidgetMovable) ? "+" : "-")
        .arg(f.testFlag(ads::CDockWidget::DockWidgetFloatable) ? "+" : "-");
}


/**
 * Appends the string returned by featuresString() to the window title of
 * the given DockWidget
 */
static void appendFeaturStringToWindowTitle(ads::CDockWidget* DockWidget)
{
    DockWidget->setWindowTitle(DockWidget->windowTitle()
                               +  QString(" (%1)").arg(featuresString(DockWidget)));
}

/**
 * Helper function to create an SVG icon
 */
class ThemedSvgIconEngine final : public QIconEngine
{
public:
    explicit ThemedSvgIconEngine(QString basePath, int intent)
        : m_basePath(std::move(basePath)), m_intent(intent)
    {
    }

    QIconEngine *clone() const override
    {
        return new ThemedSvgIconEngine(m_basePath, m_intent);
    }

    QString key() const override
    {
        return QStringLiteral("ThemedSvgIconEngine");
    }

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

    void paint(QPainter *painter, const QRect &rect,
               QIcon::Mode mode, QIcon::State state) override
    {
        if (!painter)
            return;
        painter->drawPixmap(rect, pixmap(rect.size(), mode, state));
    }

    QSize actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        const QIcon icon(themedPath());
        return icon.actualSize(size, mode, state);
    }

private:
    QString themedPath() const
    {
        if (!qApp)
            return m_basePath;
        return ThemeManager::instance()->themedIcon(m_basePath);
    }

    QString styleKey() const
    {
        if (!qApp)
            return QStringLiteral("no-app");
        const QString currentId = ThemeManager::instance()->currentStyleId();
        return currentId.isEmpty() ? QStringLiteral("uninitialized") : currentId;
    }

private:
    QString m_basePath;
    int m_intent{92};
};

static QIcon svgIcon(const QString& File, int intent = 92)
{
    return QIcon(new ThemedSvgIconEngine(File, intent));
}


#endif // WINDOWS_HELPER_H
