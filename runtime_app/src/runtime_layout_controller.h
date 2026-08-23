#ifndef RUNTIME_LAYOUT_CONTROLLER_H
#define RUNTIME_LAYOUT_CONTROLLER_H

#include <QList>
#include <QSize>
#include <QString>

#include "DockManager.h"
#include "DockWidget.h"

/**
 * @file runtime_layout_controller.h
 * @brief RuntimeLayoutController — arranges the operator runtime's task docks into a fixed
 *        tile grid (1, 2, 4, 6 or 8 tiles).
 */

/**
 * @class RuntimeLayoutController
 * @brief Arranges task docks into one of the supported tile grids.
 *
 * ADS gives docking and floating for free but has no "tile into N" primitive, so the grid
 * is built as an explicit sequence of addDockWidget() placements. Keeping that sequence
 * here rather than in the shell window means the layout rules are one small testable unit
 * and the window stays a window.
 *
 * Supported tile counts and their grids, written here as **rows x columns**. Note that
 * gridFor() returns the transpose, `QSize(columns, rows)`, because QSize is (width,
 * height); docs/domains/runtime_app/runtime_shell.md tabulates it that way too. Same
 * layouts, two axis orders — read the label, not the numbers.
 * @code
 *   1 -> 1x1     2 -> 1x2 (side by side)     4 -> 2x2
 *   6 -> 2x3     8 -> 2x4
 * @endcode
 *
 * @note Applying a layout never touches task lifecycle. Docks are re-parented, not
 *       recreated, so a running task keeps running — and a task the operator has floated
 *       out is left floating rather than being yanked back into the grid.
 */
class RuntimeLayoutController {
public:
    /// Maximum tasks the runtime view shows at once; also the largest supported tile count.
    static constexpr int kMaxTiles = 8;

    /// The tile counts the operator can choose from, ascending.
    static QList<int> supportedTileCounts() { return {1, 2, 4, 6, 8}; }

    /**
     * @brief Returns the smallest supported tile count that fits `taskCount`.
     * @param[in] taskCount number of task docks to show (values above kMaxTiles clamp to it)
     * @return one of 1, 2, 4, 6, 8 — never 0, so an empty runtime still has a valid layout
     */
    static int smallestFittingTileCount(int taskCount);

    /**
     * @brief Returns the row/column grid used for `tileCount`.
     * @param[in] tileCount a value from supportedTileCounts(); anything else falls back to
     *            the smallest grid that fits it
     * @return QSize(columns, rows)
     */
    static QSize gridFor(int tileCount);

    /**
     * @brief Arranges `docks` into the grid for `tileCount` inside `manager`.
     *
     * Docks beyond the tile count are tabbed into the last cell rather than hidden — a
     * running task that has become invisible is worse than a crowded tab bar, because
     * nothing on screen would say it is still cycling.
     *
     * @param[in] manager   the dock manager owning the docks
     * @param[in] docks     task docks in display order
     * @param[in] tileCount desired tile count (from supportedTileCounts())
     * @note Floating docks are skipped: floating is an operator decision and re-docking
     *       one on a layout change would undo it without being asked.
     */
    static void applyLayout(ads::CDockManager *manager,
                            const QList<ads::CDockWidget *> &docks,
                            int tileCount);
};

#endif // RUNTIME_LAYOUT_CONTROLLER_H
