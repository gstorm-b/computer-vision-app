#ifndef CALIBRATION_POINTS_TABLE_H
#define CALIBRATION_POINTS_TABLE_H

#include <QTableWidget>
#include <opencv2/core.hpp>
#include <vector>

/// 6-column table used by camera calibration UI:
///   #  |  Img X (px)  |  Img Y (px)  |  World X (mm)  |  World Y (mm)  |  World Z (mm)
///
/// Each row holds one 2D image -> 3D world correspondence pair.
/// Image columns are populated by the board detector and are read-only.
/// World columns are user-editable (or filled programmatically from a robot
/// teach pendant via setWorldPointsFromRobot()).
///
/// Emits pointsEdited() when any user edit lands in a world column. Programmatic
/// fills (setImagePoints / setWorldPoints*) are guarded by a signal blocker and
/// do not emit.
///
class CalibrationPointsTable : public QTableWidget
{
    Q_OBJECT

public:
    /// Constructs the table and builds its fixed 6-column header.
    explicit CalibrationPointsTable(QWidget *parent = nullptr);

    /// Resizes the table to n rows. Existing rows are kept up to n; new rows are
    /// initialised with empty image cells and world = (0, 0, 0). Also renumbers
    /// the index column of any kept rows.
    void setRowCount(int n);

    /// Fills the image (X, Y) columns from `pts`, one row per point.
    /// @note Rows past `pts.size()` (up to rowCount()) have their image cells cleared instead,
    /// so stale data from a previous fill isn't left showing.
    void setImagePoints(const std::vector<cv::Point2f> &pts);
    /// Reads back the image (X, Y) columns; a row with an unparsable/missing value is
    /// returned as (0, 0).
    /// @return one point per row, in row order
    std::vector<cv::Point2f> imagePoints() const;

    /// Fills the world X/Y columns from `xy` (clamped to rowCount()) and forces World Z to
    /// "0.000" for each filled row.
    void setWorldPointsXY(const std::vector<cv::Point2f> &xy);
    /// Fills the world X/Y/Z columns from `pts`, one row per point (clamped to rowCount()).
    void setWorldPoints(const std::vector<cv::Point3f> &pts);
    /// Reads back the world (X, Y, Z) columns.
    /// @return one point per row, in row order
    std::vector<cv::Point3f> worldPoints() const;

    /// Ready-made entry point for direct robot teach-pendant input: fills the world columns via
    /// setWorldPoints() and then emits pointsEdited(), unlike the other setWorldPoints*()
    /// setters. Kept as a distinct method so the robot integration can wire to it without
    /// changing other call sites.
    void setWorldPointsFromRobot(const std::vector<cv::Point3f> &pts);

    /// Clears the image columns and resets the world columns to "0.000" for every row,
    /// without emitting pointsEdited().
    void clearAll();

signals:
    /// Emitted when the user finishes editing any world (X/Y/Z) cell, or after
    /// setWorldPointsFromRobot() fills the world columns programmatically.
    void pointsEdited();

private slots:
    /// Handler for QTableWidget::itemChanged; re-emits pointsEdited() when `item` is in a
    /// world column and the change wasn't a programmatic fill (m_blockEditSignal is false).
    void onItemChanged(QTableWidgetItem *item);

private:
    /// Sets the fixed 6-column count and header labels, hides the vertical header, stretches
    /// all columns except the index column, and configures row-selection/edit triggers.
    void initHeader();
    /// Creates all 6 cells for `row`: the index cell (read-only, centered, 1-based row number),
    /// the two image cells (read-only, right-aligned), and the three world cells
    /// (editable, right-aligned, seeded to "0.000"). Runs under a signal blocker.
    void initRow(int row);
    /// Returns true if `col` is one of the three world (X/Y/Z) columns.
    static bool isWorldColumn(int col);

    bool m_blockEditSignal{false};  ///< Checked in onItemChanged() but never set to true elsewhere; QSignalBlocker is what actually suppresses itemChanged during programmatic fills.
};

#endif // CALIBRATION_POINTS_TABLE_H
