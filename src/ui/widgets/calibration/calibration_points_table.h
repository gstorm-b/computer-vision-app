#ifndef CALIBRATION_POINTS_TABLE_H
#define CALIBRATION_POINTS_TABLE_H

#include <QTableWidget>
#include <opencv2/core.hpp>
#include <vector>

// =====================================================================
// CalibrationPointsTable
// =====================================================================
//
// 6-column table used by camera calibration UI:
//   #  |  Img X (px)  |  Img Y (px)  |  World X (mm)  |  World Y (mm)  |  World Z (mm)
//
// Each row holds one 2D image -> 3D world correspondence pair.
// Image columns are populated by the board detector and are read-only.
// World columns are user-editable (or filled programmatically from a robot
// teach pendant via setWorldPointsFromRobot()).
//
// Emits pointsEdited() when any user edit lands in a world column. Programmatic
// fills (setImagePoints / setWorldPoints*) are guarded by a signal blocker and
// do not emit.
//
class CalibrationPointsTable : public QTableWidget
{
    Q_OBJECT

public:
    explicit CalibrationPointsTable(QWidget *parent = nullptr);

    // Resize the table to n rows. Existing rows are kept up to n; new rows are
    // initialised with empty image cells and world = (0, 0, 0).
    void setRowCount(int n);

    // Fill image columns. Size mismatch is logged and clamped.
    void setImagePoints(const std::vector<cv::Point2f> &pts);
    std::vector<cv::Point2f> imagePoints() const;

    // Fill world columns. Z defaults to 0 when only XY is supplied.
    void setWorldPointsXY(const std::vector<cv::Point2f> &xy);
    void setWorldPoints(const std::vector<cv::Point3f> &pts);
    std::vector<cv::Point3f> worldPoints() const;

    // Ready-made entry point for direct robot teach-pendant input. Currently
    // delegates to setWorldPoints(); kept as a distinct slot so the robot
    // integration can wire to it without changing call sites here.
    void setWorldPointsFromRobot(const std::vector<cv::Point3f> &pts);

    void clearAll();

signals:
    // Emitted when the user finishes editing any world (X/Y/Z) cell.
    void pointsEdited();

private slots:
    void onItemChanged(QTableWidgetItem *item);

private:
    void initHeader();
    void initRow(int row);
    static bool isWorldColumn(int col);

    bool m_blockEditSignal{false};
};

#endif // CALIBRATION_POINTS_TABLE_H
