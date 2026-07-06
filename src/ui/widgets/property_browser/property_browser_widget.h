#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include "qtpropertybrowser/qttreepropertybrowser.h"
#include "qtpropertybrowser/qtvariantproperty.h"
#include "qtpropertybrowser/qtpropertymanager.h"
#include "qtpropertybrowser/qteditorfactory.h"

#include "custom_property_managers.h"

// ============================================================================
//  PropertyBrowserWidget
//
//  A self-contained property inspector composed of three zones:
//
//    ┌─────────────────────────────────────────────────────┐
//    │ [🔍 Filter properties...                          ✕]│  ← searchBar
//    ├─────────────────────────────────────────────────────┤
//    │                                                     │
//    │  QtTreePropertyBrowser                              │
//    │                                                     │
//    ├─────────────────────────────────────────────────────┤
//    │  Description: <tooltip of selected property>        │  ← descFrame
//    └─────────────────────────────────────────────────────┘
//
//  All layout widgets are named (see objectName) for full QSS theming:
//    #searchBar         — the top filter frame
//    #searchEdit        — the QLineEdit inside the search bar
//    #descFrame         — the bottom description frame
//    #descLabel         — the QLabel showing the description
//
//  Built-in managers (pre-wired, ready to use):
//    variantManager()  — QtVariantPropertyManager (int/double/bool/string …)
//    positionManager() — PositionPropertyManager  (XY / XYZ / XYZRPY)
//    sizeManager()     — SizePropertyManager      (WH / WHD)
//    pointManager()    — PointPropertyManager     (int XY / XYZ — QPoint, cv::Point, cv::Point3i)
//    pointFManager()   — PointFPropertyManager    (double XY / XYZ — QPointF, cv::Point2f/d, cv::Point3f/d)
//
//  String properties with completer:
//    auto *p = variantManager()->addProperty(QMetaType::QString, "File path");
//    variantManager()->setAttribute(p, "completer", QStringList{"/path/a", "/path/b"});
//
//  Usage pattern:
//    auto *w = new PropertyBrowserWidget(parentWidget);
//    w->setSearchVisible(false);       // hide search if panel is tiny
//    auto *grp = w->variantManager()->addProperty(
//                    QtVariantPropertyManager::groupTypeId(), "Settings");
//    auto *p = w->variantManager()->addProperty(QMetaType::Double, "Speed");
//    grp->addSubProperty(p);
//    w->addProperty(grp);
//
// ============================================================================

class PropertyBrowserWidget : public QWidget {
    Q_OBJECT

    Q_PROPERTY(bool searchVisible
               READ  isSearchVisible
               WRITE setSearchVisible)
    Q_PROPERTY(bool descriptionVisible
               READ  isDescriptionVisible
               WRITE setDescriptionVisible)

public:
    explicit PropertyBrowserWidget(QWidget *parent = nullptr);
    ~PropertyBrowserWidget() override = default;

    // ── Core accessors ─────────────────────────────────────────────────────
    QtTreePropertyBrowser    *browser()         const { return m_browser; }
    QtVariantPropertyManager *variantManager()  const { return m_variantManager; }
    QtVariantEditorFactory   *variantFactory()  const { return m_variantFactory; }
    PositionPropertyManager  *positionManager() const { return m_positionManager; }
    SizePropertyManager      *sizeManager()     const { return m_sizeManager; }
    PointPropertyManager     *pointManager()    const { return m_pointManager; }
    PointFPropertyManager    *pointFManager()   const { return m_pointFManager; }

    // ── Search bar ─────────────────────────────────────────────────────────
    void setSearchVisible(bool visible);
    bool isSearchVisible()   const;
    void clearSearch();
    QString searchText()     const;

    // ── Description panel ──────────────────────────────────────────────────
    void setDescriptionVisible(bool visible);
    bool isDescriptionVisible() const;

    // ── Browser forwarding methods ─────────────────────────────────────────
    QtBrowserItem *addProperty(QtProperty *property);
    void           removeProperty(QtProperty *property);
    void           clear();

    void setExpanded(QtBrowserItem *item, bool expanded);
    void setAlternatingRowColors(bool enable);
    void setSplitterPosition(int pos);
    void setResizeMode(QtTreePropertyBrowser::ResizeMode mode);
    void setRootIsDecorated(bool show);
    void setHeaderVisible(bool visible);
    void setPropertiesWithoutValueMarked(bool mark);

    // ── Indentation / splitter ─────────────────────────────────────────────
    void setIndentation(int i);
    int  splitterPosition() const;

signals:
    void currentItemChanged(QtBrowserItem *item);

private slots:
    void onSearchChanged(const QString &text);
    void onCurrentItemChanged(QtBrowserItem *item);
    void applyFilter();

private:
    void setupUi();
    void setupManagers();
    bool itemOrChildMatchesFilter(QtBrowserItem *item, const QString &lowerText) const;
    void filterTopLevel(const QString &lowerText);

    // ── Widgets ────────────────────────────────────────────────────────────
    QFrame               *m_searchBar      {nullptr};
    QLineEdit            *m_searchEdit     {nullptr};
    QtTreePropertyBrowser *m_browser       {nullptr};
    QFrame               *m_descFrame      {nullptr};
    QLabel               *m_descLabel      {nullptr};

    // ── Managers / factories ───────────────────────────────────────────────
    QtVariantPropertyManager *m_variantManager  {nullptr};
    QtVariantEditorFactory   *m_variantFactory  {nullptr};
    PositionPropertyManager  *m_positionManager {nullptr};
    SizePropertyManager      *m_sizeManager     {nullptr};
    PointPropertyManager     *m_pointManager    {nullptr};
    PointFPropertyManager    *m_pointFManager   {nullptr};
    QtDoubleSpinBoxFactory   *m_dblFactory      {nullptr};  // shared by custom managers
    QtSpinBoxFactory         *m_intFactory      {nullptr};  // for integer point manager

    QTimer  m_filterTimer;   // debounce rapid key input
    QString m_filterText;
};
