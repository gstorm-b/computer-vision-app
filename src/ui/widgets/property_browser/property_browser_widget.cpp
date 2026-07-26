#include "property_browser_widget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QStyle>

// ── Construction ─────────────────────────────────────────────────────────────

/// Builds the search bar / tree browser / description panel layout, wires up
/// the built-in property managers, and starts a 120ms debounce timer that
/// applies the search filter after typing settles.
PropertyBrowserWidget::PropertyBrowserWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupManagers();

    // Debounce the filter so fast typing doesn't thrash the tree.
    m_filterTimer.setSingleShot(true);
    m_filterTimer.setInterval(120);
    connect(&m_filterTimer, &QTimer::timeout, this, &PropertyBrowserWidget::applyFilter);
}

// ── UI layout ─────────────────────────────────────────────────────────────────

/// Creates and lays out the search bar (icon + line edit), the
/// QtTreePropertyBrowser, and the description frame in a zero-margin vertical
/// layout; assigns objectName to each part for QSS theming.
void PropertyBrowserWidget::setupUi()
{
    setObjectName(QStringLiteral("PropertyBrowserWidget"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Search bar ───────────────────────────────────────────────────────────
    m_searchBar = new QFrame(this);
    m_searchBar->setObjectName(QStringLiteral("searchBar"));
    m_searchBar->setFrameShape(QFrame::NoFrame);

    auto *searchLayout = new QHBoxLayout(m_searchBar);
    searchLayout->setContentsMargins(4, 3, 4, 3);
    searchLayout->setSpacing(4);

    // Magnifier label — purely decorative; swap for an icon via QSS if desired.
    auto *iconLabel = new QLabel(QStringLiteral("🔍"), m_searchBar);
    iconLabel->setObjectName(QStringLiteral("searchIcon"));
    iconLabel->setFixedWidth(18);
    searchLayout->addWidget(iconLabel);

    m_searchEdit = new QLineEdit(m_searchBar);
    m_searchEdit->setObjectName(QStringLiteral("searchEdit"));
    m_searchEdit->setPlaceholderText(tr("Filter properties…"));
    m_searchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(m_searchEdit);

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &PropertyBrowserWidget::onSearchChanged);

    root->addWidget(m_searchBar);

    // ── Property browser ─────────────────────────────────────────────────────
    m_browser = new QtTreePropertyBrowser(this);
    m_browser->setObjectName(QStringLiteral("propertyBrowser"));
    m_browser->setAlternatingRowColors(false);
    m_browser->setPropertiesWithoutValueMarked(true);
    m_browser->setRootIsDecorated(false);
    m_browser->setResizeMode(QtTreePropertyBrowser::Stretch);
    m_browser->setIndentation(12);

    connect(m_browser, &QtTreePropertyBrowser::currentItemChanged,
            this, &PropertyBrowserWidget::onCurrentItemChanged);

    root->addWidget(m_browser, 1);

    // ── Description frame ─────────────────────────────────────────────────────
    m_descFrame = new QFrame(this);
    m_descFrame->setObjectName(QStringLiteral("descFrame"));
    m_descFrame->setFrameShape(QFrame::StyledPanel);
    m_descFrame->setMinimumHeight(48);
    m_descFrame->setMaximumHeight(80);

    auto *descLayout = new QVBoxLayout(m_descFrame);
    descLayout->setContentsMargins(6, 4, 6, 4);

    m_descLabel = new QLabel(m_descFrame);
    m_descLabel->setObjectName(QStringLiteral("descLabel"));
    m_descLabel->setWordWrap(true);
    m_descLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_descLabel->setText(tr("Select a property to see its description."));
    descLayout->addWidget(m_descLabel);

    root->addWidget(m_descFrame);
}

// ── Manager / factory wiring ──────────────────────────────────────────────────

/// Instantiates the variant/position/size/point/pointF property managers and
/// registers the shared double/int spin-box factories against each manager's
/// sub-manager on the tree browser.
void PropertyBrowserWidget::setupManagers()
{
    // ── QtVariantPropertyManager (handles int, double, bool, string …) ───────
    m_variantManager = new QtVariantPropertyManager(this);
    m_variantFactory  = new QtVariantEditorFactory(this);
    m_browser->setFactoryForManager(m_variantManager, m_variantFactory);

    // ── PositionPropertyManager ───────────────────────────────────────────────
    m_positionManager = new PositionPropertyManager(this);

    // ── SizePropertyManager ───────────────────────────────────────────────────
    m_sizeManager = new SizePropertyManager(this);

    // ── PointPropertyManager (int XY/XYZ) ─────────────────────────────────────
    m_pointManager = new PointPropertyManager(this);

    // ── PointFPropertyManager (double XY/XYZ) ─────────────────────────────────
    m_pointFManager = new PointFPropertyManager(this);

    // Shared factories — each custom manager owns its own sub-manager, but
    // they all need the matching editor factory registered on the browser.
    m_dblFactory = new QtDoubleSpinBoxFactory(this);
    m_intFactory = new QtSpinBoxFactory(this);
    m_browser->setFactoryForManager(m_positionManager->subDoubleManager(), m_dblFactory);
    m_browser->setFactoryForManager(m_sizeManager->subDoubleManager(),     m_dblFactory);
    m_browser->setFactoryForManager(m_pointFManager->subDoubleManager(),   m_dblFactory);
    m_browser->setFactoryForManager(m_pointManager->subIntManager(),       m_intFactory);
}

// ── Search bar ────────────────────────────────────────────────────────────────

/// Toggles search bar visibility; clears any active filter text when hidden.
void PropertyBrowserWidget::setSearchVisible(bool visible)
{
    m_searchBar->setVisible(visible);
    if (!visible) clearSearch();
}

/// Returns whether the search bar frame is currently visible.
bool PropertyBrowserWidget::isSearchVisible() const
{
    return m_searchBar->isVisible();
}

/// Clears the search line edit's text.
void PropertyBrowserWidget::clearSearch()
{
    m_searchEdit->clear();
}

/// Returns the search line edit's current text.
QString PropertyBrowserWidget::searchText() const
{
    return m_searchEdit->text();
}

// ── Description panel ─────────────────────────────────────────────────────────

/// Toggles description panel visibility.
void PropertyBrowserWidget::setDescriptionVisible(bool visible)
{
    m_descFrame->setVisible(visible);
}

/// Returns whether the description frame is currently visible.
bool PropertyBrowserWidget::isDescriptionVisible() const
{
    return m_descFrame->isVisible();
}

// ── Browser forwarding ────────────────────────────────────────────────────────

/// Forwards to QtTreePropertyBrowser::addProperty().
QtBrowserItem *PropertyBrowserWidget::addProperty(QtProperty *property)
{
    return m_browser->addProperty(property);
}

/// Forwards to QtTreePropertyBrowser::removeProperty().
void PropertyBrowserWidget::removeProperty(QtProperty *property)
{
    m_browser->removeProperty(property);
}

/// Clears all properties from the tree browser and resets the description
/// label to its placeholder text.
void PropertyBrowserWidget::clear()
{
    m_browser->clear();
    m_descLabel->setText(tr("Select a property to see its description."));
}

/// Forwards to QtTreePropertyBrowser::setExpanded().
void PropertyBrowserWidget::setExpanded(QtBrowserItem *item, bool expanded)
{
    m_browser->setExpanded(item, expanded);
}

/// Forwards to QtTreePropertyBrowser::setAlternatingRowColors().
void PropertyBrowserWidget::setAlternatingRowColors(bool enable)
{
    m_browser->setAlternatingRowColors(enable);
}

/// Forwards to QtTreePropertyBrowser::setSplitterPosition().
void PropertyBrowserWidget::setSplitterPosition(int pos)
{
    m_browser->setSplitterPosition(pos);
}

/// Forwards to QtTreePropertyBrowser::setResizeMode().
void PropertyBrowserWidget::setResizeMode(QtTreePropertyBrowser::ResizeMode mode)
{
    m_browser->setResizeMode(mode);
}

/// Forwards to QtTreePropertyBrowser::setRootIsDecorated().
void PropertyBrowserWidget::setRootIsDecorated(bool show)
{
    m_browser->setRootIsDecorated(show);
}

/// Forwards to QtTreePropertyBrowser::setHeaderVisible().
void PropertyBrowserWidget::setHeaderVisible(bool visible)
{
    m_browser->setHeaderVisible(visible);
}

/// Forwards to QtTreePropertyBrowser::setPropertiesWithoutValueMarked().
void PropertyBrowserWidget::setPropertiesWithoutValueMarked(bool mark)
{
    m_browser->setPropertiesWithoutValueMarked(mark);
}

/// Forwards to QtTreePropertyBrowser::setIndentation().
void PropertyBrowserWidget::setIndentation(int i)
{
    m_browser->setIndentation(i);
}

/// Forwards to QtTreePropertyBrowser::splitterPosition().
int PropertyBrowserWidget::splitterPosition() const
{
    return m_browser->splitterPosition();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

/// Records the new search text and (re)starts the debounce timer; the actual
/// filtering happens later in applyFilter() once typing settles.
void PropertyBrowserWidget::onSearchChanged(const QString &text)
{
    m_filterText = text;
    m_filterTimer.start();   // debounce
}

/// Updates the description label with the newly current item's name and
/// tooltip (or resets to the placeholder text when `item` is null), then
/// re-emits currentItemChanged() to this widget's own listeners.
void PropertyBrowserWidget::onCurrentItemChanged(QtBrowserItem *item)
{
    if (item) {
        const QString desc = item->property()->descriptionToolTip();
        m_descLabel->setText(desc.isEmpty()
                             ? item->property()->propertyName()
                             : QStringLiteral("<b>%1</b><br>%2")
                                   .arg(item->property()->propertyName(), desc));
    } else {
        m_descLabel->setText(tr("Select a property to see its description."));
    }
    emit currentItemChanged(item);
}

/// Lower-cases and trims the pending filter text, logs it, and applies it to
/// the tree browser's top-level items via filterTopLevel(). Called when the
/// debounce timer (m_filterTimer) fires.
void PropertyBrowserWidget::applyFilter()
{
    const QString lower = m_filterText.trimmed().toLower();
    qDebug() << "Applying filter:" << lower;
    filterTopLevel(lower);
}

// ── Filter implementation ─────────────────────────────────────────────────────

/// Recursively checks whether `item`'s display name contains `lowerText`, or
/// whether any of its descendants do.
/// @return true if `lowerText` is empty, or a match is found in `item` or a child
bool PropertyBrowserWidget::itemOrChildMatchesFilter(QtBrowserItem *item,
                                                     const QString &lowerText) const
{
    if (lowerText.isEmpty()) return true;

    const QString name = item->property()->displayName().toLower();
    if (name.contains(lowerText)) {
        return true;
    }

    for (auto *child : item->children())
        if (itemOrChildMatchesFilter(child, lowerText)) {
            return true;
        }

    return false;
}

/// Shows or hides each top-level browser item depending on whether it or any
/// of its descendants matches `lowerText` (via itemOrChildMatchesFilter()).
void PropertyBrowserWidget::filterTopLevel(const QString &lowerText)
{
    // not only shows matching items, but also shows same-level siblings
    for (auto *item : m_browser->topLevelItems()) {
        const bool visible = itemOrChildMatchesFilter(item, lowerText);
        m_browser->setItemVisible(item, visible);
    }
}
