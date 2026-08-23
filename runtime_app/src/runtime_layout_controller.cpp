#include "runtime_app/src/runtime_layout_controller.h"

#include "core/logger/app_logger.h"

/// Returns the smallest supported tile count that fits `taskCount`, clamping above
/// kMaxTiles and never returning 0 (an empty runtime still needs a valid layout).
int RuntimeLayoutController::smallestFittingTileCount(int taskCount)
{
    const QList<int> counts = supportedTileCounts();
    for (int candidate : counts) {
        if (taskCount <= candidate) {
            return candidate;
        }
    }
    return kMaxTiles;
}

/// Maps a tile count to its column/row grid, falling back to the smallest grid that fits
/// an unsupported value.
QSize RuntimeLayoutController::gridFor(int tileCount)
{
    switch (tileCount) {
    case 1:
        return QSize(1, 1);
    case 2:
        return QSize(2, 1);
    case 4:
        return QSize(2, 2);
    case 6:
        return QSize(3, 2);
    case 8:
        return QSize(4, 2);
    default:
        return gridFor(smallestFittingTileCount(tileCount));
    }
}

/// Places `docks` into the grid for `tileCount`: the first dock anchors the centre area,
/// each later dock opens a new column to its right until the row is full, then the next
/// row is opened below the first cell of the previous one. Docks past the tile count are
/// tabbed into the last occupied cell.
void RuntimeLayoutController::applyLayout(ads::CDockManager *manager,
                                          const QList<ads::CDockWidget *> &docks,
                                          int tileCount)
{
    if (manager == nullptr || docks.isEmpty()) {
        return;
    }

    const QSize grid = gridFor(tileCount);
    const int columns = grid.width();
    const int rows = grid.height();

    // Floating is an operator decision about where they want to watch a task. Re-docking
    // one here would silently undo it, so a floated dock is left alone and does not
    // consume a tile.
    QList<ads::CDockWidget *> docked;
    for (ads::CDockWidget *dock : docks) {
        if (dock != nullptr && !dock->isFloating()) {
            docked.append(dock);
        }
    }
    if (docked.isEmpty()) {
        return;
    }

    // One dock area per cell, indexed row-major. rowAnchors keeps the first area of each
    // row so the next row can be opened below it rather than below the last column.
    QList<ads::CDockAreaWidget *> rowAnchors;
    ads::CDockAreaWidget *previousInRow = nullptr;

    for (int index = 0; index < docked.size(); ++index) {
        ads::CDockWidget *dock = docked.at(index);

        const int cell = qMin(index, columns * rows - 1);
        const int row = cell / columns;
        const int column = cell % columns;

        ads::CDockAreaWidget *area = nullptr;

        if (index >= columns * rows) {
            // Past the last cell: tab into it instead of hiding the dock. A running task
            // nobody can see is worse than a crowded tab bar — nothing on screen would
            // say it is still cycling.
            area = manager->addDockWidget(ads::CenterDockWidgetArea, dock, previousInRow);
        } else if (row == 0 && column == 0) {
            area = manager->addDockWidget(ads::CenterDockWidgetArea, dock);
            rowAnchors.append(area);
        } else if (column == 0) {
            // First cell of a new row: open it below the row above's anchor.
            area = manager->addDockWidget(ads::BottomDockWidgetArea, dock,
                                          rowAnchors.at(row - 1));
            rowAnchors.append(area);
        } else {
            area = manager->addDockWidget(ads::RightDockWidgetArea, dock, previousInRow);
        }

        previousInRow = area;
    }

    LOG_DEV_INFO << "Runtime layout applied."
                 << "tiles=" << tileCount
                 << "grid=" << columns << "x" << rows
                 << "docked=" << docked.size()
                 << "floating=" << (docks.size() - docked.size());
}
