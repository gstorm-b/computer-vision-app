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

/// ============================================================================
///  PropertyBrowserWidget
///
///  A self-contained property inspector composed of three zones:
///
///    ┌─────────────────────────────────────────────────────┐
///    │ [🔍 Filter properties...                          ✕]│  ← searchBar
///    ├─────────────────────────────────────────────────────┤
///    │                                                     │
///    │  QtTreePropertyBrowser                              │
///    │                                                     │
///    ├─────────────────────────────────────────────────────┤
///    │  Description: `<tooltip of selected property>`      │  ← descFrame
///    └─────────────────────────────────────────────────────┘
///
///  All layout widgets are named (see objectName) for full QSS theming:
///    #searchBar         — the top filter frame
///    #searchEdit        — the QLineEdit inside the search bar
///    #descFrame         — the bottom description frame
///    #descLabel         — the QLabel showing the description
///
///  Built-in managers (pre-wired, ready to use):
///    variantManager()  — QtVariantPropertyManager (int/double/bool/string …)
///    positionManager() — PositionPropertyManager  (XY / XYZ / XYZRPY)
///    sizeManager()     — SizePropertyManager      (WH / WHD)
///    pointManager()    — PointPropertyManager     (int XY / XYZ — QPoint, cv::Point, cv::Point3i)
///    pointFManager()   — PointFPropertyManager    (double XY / XYZ — QPointF, cv::Point2f/d, cv::Point3f/d)
///
///  String properties with completer:
///    auto *p = variantManager()->addProperty(QMetaType::QString, "File path");
///    variantManager()->setAttribute(p, "completer", QStringList{"/path/a", "/path/b"});
///
///  Usage pattern:
///    auto *w = new PropertyBrowserWidget(parentWidget);
///    w->setSearchVisible(false);       // hide search if panel is tiny
///    auto *grp = w->variantManager()->addProperty(
///                    QtVariantPropertyManager::groupTypeId(), "Settings");
///    auto *p = w->variantManager()->addProperty(QMetaType::Double, "Speed");
///    grp->addSubProperty(p);
///    w->addProperty(grp);
///
/// ============================================================================

class PropertyBrowserWidget : public QWidget {
    Q_OBJECT

    Q_PROPERTY(bool searchVisible
               READ  isSearchVisible
               WRITE setSearchVisible)
    Q_PROPERTY(bool descriptionVisible
               READ  isDescriptionVisible
               WRITE setDescriptionVisible)

public:
    /// Constructs the widget and builds the search bar, tree browser, and
    /// description panel, then wires up the built-in variant/position/size/
    /// point property managers with their editor factories.
    explicit PropertyBrowserWidget(QWidget *parent = nullptr);
    /// Default destructor; child widgets/managers are destroyed via Qt's
    /// parent-child ownership.
    ~PropertyBrowserWidget() override = default;

    // ── Core accessors ─────────────────────────────────────────────────────
    /// Returns the underlying QtTreePropertyBrowser used to host properties.
    QtTreePropertyBrowser    *browser()         const { return m_browser; }
    /// Returns the pre-wired manager for built-in variant types (int/double/bool/string …).
    QtVariantPropertyManager *variantManager()  const { return m_variantManager; }
    /// Returns the editor factory registered for variantManager().
    QtVariantEditorFactory   *variantFactory()  const { return m_variantFactory; }
    /// Returns the pre-wired manager for position properties (XY / XYZ / XYZRPY).
    PositionPropertyManager  *positionManager() const { return m_positionManager; }
    /// Returns the pre-wired manager for size properties (WH / WHD).
    SizePropertyManager      *sizeManager()     const { return m_sizeManager; }
    /// Returns the pre-wired manager for integer point properties (QPoint, cv::Point, cv::Point3i).
    PointPropertyManager     *pointManager()    const { return m_pointManager; }
    /// Returns the pre-wired manager for floating-point point properties (QPointF, cv::Point2f/d, cv::Point3f/d).
    PointFPropertyManager    *pointFManager()   const { return m_pointFManager; }

    // ── Search bar ─────────────────────────────────────────────────────────
    /// Shows or hides the search bar; hiding it also clears any active filter text.
    void setSearchVisible(bool visible);
    /// Returns whether the search bar is currently visible.
    bool isSearchVisible()   const;
    /// Clears the search edit's text (triggers a debounced filter refresh).
    void clearSearch();
    /// Returns the current raw text typed in the search edit.
    QString searchText()     const;

    // ── Description panel ──────────────────────────────────────────────────
    /// Shows or hides the bottom description panel.
    void setDescriptionVisible(bool visible);
    /// Returns whether the description panel is currently visible.
    bool isDescriptionVisible() const;

    // ── Browser forwarding methods ─────────────────────────────────────────
    /// Adds `property` as a top-level item of the tree browser.
    /// @return the created QtBrowserItem
    QtBrowserItem *addProperty(QtProperty *property);
    /// Removes `property` (and its browser item) from the tree browser.
    void           removeProperty(QtProperty *property);
    /// Removes all properties from the tree browser and resets the description
    /// label back to its placeholder text.
    void           clear();

    /// Expands or collapses `item` in the tree browser.
    void setExpanded(QtBrowserItem *item, bool expanded);
    /// Enables/disables alternating row background colors on the tree browser.
    void setAlternatingRowColors(bool enable);
    /// Moves the browser's name/value column splitter to `pos`.
    void setSplitterPosition(int pos);
    /// Sets how the tree browser resizes its columns.
    void setResizeMode(QtTreePropertyBrowser::ResizeMode mode);
    /// Shows/hides the tree decoration (expand/collapse arrows) on root items.
    void setRootIsDecorated(bool show);
    /// Shows/hides the tree browser's header row.
    void setHeaderVisible(bool visible);
    /// Enables/disables visual marking of properties that have no value set.
    void setPropertiesWithoutValueMarked(bool mark);

    // ── Indentation / splitter ─────────────────────────────────────────────
    /// Sets the per-level indentation (in pixels) of the tree browser.
    void setIndentation(int i);
    /// Returns the current position of the browser's splitter.
    int  splitterPosition() const;

signals:
    /// Emitted whenever the tree browser's current item changes (forwarded
    /// from QtTreePropertyBrowser::currentItemChanged); `item` is null when
    /// selection is cleared.
    void currentItemChanged(QtBrowserItem *item);

private slots:
    /// Stores the new filter text and (re)starts the debounce timer that
    /// eventually calls applyFilter().
    void onSearchChanged(const QString &text);
    /// Updates the description label to show the newly selected property's
    /// name/tooltip (or the placeholder text if `item` is null), then re-emits
    /// currentItemChanged().
    void onCurrentItemChanged(QtBrowserItem *item);
    /// Applies the pending filter text (lower-cased, trimmed) to the tree
    /// browser's top-level items; called when the debounce timer fires.
    void applyFilter();

private:
    /// Builds the search bar, tree browser, and description panel and lays
    /// them out in a vertical stack; sets objectName on each part for QSS.
    void setupUi();
    /// Creates the variant/position/size/point/pointF property managers and
    /// registers their editor factories on the tree browser.
    void setupManagers();
    /// Returns true if `item`'s display name contains `lowerText`, or if any
    /// descendant (recursively) does.
    bool itemOrChildMatchesFilter(QtBrowserItem *item, const QString &lowerText) const;
    /// Shows/hides each top-level browser item depending on whether it or any
    /// of its children matches `lowerText`.
    void filterTopLevel(const QString &lowerText);

    // ── Widgets ────────────────────────────────────────────────────────────
    QFrame               *m_searchBar      {nullptr};  ///< Top filter bar frame (objectName "searchBar").
    QLineEdit            *m_searchEdit     {nullptr};  ///< Filter text field inside m_searchBar (objectName "searchEdit").
    QtTreePropertyBrowser *m_browser       {nullptr};  ///< The tree property browser hosting all properties.
    QFrame               *m_descFrame      {nullptr};  ///< Bottom description frame (objectName "descFrame").
    QLabel               *m_descLabel      {nullptr};  ///< Label showing the selected property's name/tooltip.

    // ── Managers / factories ───────────────────────────────────────────────
    QtVariantPropertyManager *m_variantManager  {nullptr};  ///< Manager for built-in variant-typed properties.
    QtVariantEditorFactory   *m_variantFactory  {nullptr};  ///< Editor factory registered for m_variantManager.
    PositionPropertyManager  *m_positionManager {nullptr};  ///< Manager for position (XY/XYZ/XYZRPY) properties.
    SizePropertyManager      *m_sizeManager     {nullptr};  ///< Manager for size (WH/WHD) properties.
    PointPropertyManager     *m_pointManager    {nullptr};  ///< Manager for integer point properties.
    PointFPropertyManager    *m_pointFManager   {nullptr};  ///< Manager for floating-point point properties.
    QtDoubleSpinBoxFactory   *m_dblFactory      {nullptr};  ///< shared by custom managers
    QtSpinBoxFactory         *m_intFactory      {nullptr};  ///< for integer point manager

    QTimer  m_filterTimer;   ///< debounce rapid key input
    QString m_filterText;    ///< Pending filter text set by onSearchChanged(), applied by applyFilter().
};
