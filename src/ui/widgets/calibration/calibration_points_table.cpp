#include "calibration_points_table.h"

#include <QHeaderView>
#include <QSignalBlocker>
#include <QStringList>

/// Fixed column indices for the 6-column layout (# | Img X | Img Y | World X | World Y | World Z).
namespace {
constexpr int kColIndex   = 0;
constexpr int kColImgX    = 1;
constexpr int kColImgY    = 2;
constexpr int kColWorldX  = 3;
constexpr int kColWorldY  = 4;
constexpr int kColWorldZ  = 5;
constexpr int kColCount   = 6;
} // namespace

/// Builds the header and subscribes onItemChanged() to QTableWidget::itemChanged.
CalibrationPointsTable::CalibrationPointsTable(QWidget *parent)
    : QTableWidget(parent) {
    initHeader();
    connect(this, &QTableWidget::itemChanged,
            this, &CalibrationPointsTable::onItemChanged);
}

/// Sets the fixed 6-column count and header labels, hides the vertical header, stretches
/// all columns except the index column, and configures row-selection/edit triggers.
void CalibrationPointsTable::initHeader() {
    QTableWidget::setColumnCount(kColCount);
    setHorizontalHeaderLabels(QStringList{
        tr("#"),
        tr("Img X (px)"), tr("Img Y (px)"),
        tr("World X (mm)"), tr("World Y (mm)"), tr("World Z (mm)")
    });
    verticalHeader()->setVisible(false);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(kColIndex, QHeaderView::ResizeToContents);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
}

/// Returns true if `col` is one of the three world (X/Y/Z) columns.
bool CalibrationPointsTable::isWorldColumn(int col) {
    return col == kColWorldX || col == kColWorldY || col == kColWorldZ;
}

/// Creates all 6 cells for `row`: the index cell (read-only, centered, 1-based row number),
/// the two image cells (read-only, right-aligned, initially empty), and the three world
/// cells (editable, right-aligned, seeded to "0.000"). Runs under a signal blocker so
/// populating a fresh row never emits itemChanged/pointsEdited.
void CalibrationPointsTable::initRow(int row) {
    QSignalBlocker blocker(this);
    for (int c = 0; c < kColCount; ++c) {
        auto *item = new QTableWidgetItem();
        if (c == kColIndex) {
            item->setText(QString::number(row + 1));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setTextAlignment(Qt::AlignCenter);
        } else if (c == kColImgX || c == kColImgY) {
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        } else {
            item->setText(QStringLiteral("0.000"));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        setItem(row, c, item);
    }
}

/// Resizes the table to n rows. Existing rows are kept up to n; new rows are
/// initialised via initRow() with empty image cells and world = (0, 0, 0).
/// Also renumbers the index column of any kept rows under a signal blocker.
void CalibrationPointsTable::setRowCount(int n) {
    int old = rowCount();
    QTableWidget::setRowCount(n);
    for (int r = old; r < n; ++r) {
        initRow(r);
    }
    // Refresh index column numbering for any kept rows (defensive — rowCount
    // shrinking does not touch existing items, growing covered above).
    QSignalBlocker blocker(this);
    for (int r = 0; r < n; ++r) {
        if (auto *it = item(r, kColIndex)) {
            it->setText(QString::number(r + 1));
        }
    }
}

/// Fills the image (X, Y) columns from `pts`, one row per point, clamped to rowCount().
/// Any remaining rows beyond `pts.size()` have their image cells cleared instead, so
/// stale data from a previous fill isn't left showing. Runs under a signal blocker.
void CalibrationPointsTable::setImagePoints(const std::vector<cv::Point2f> &pts) {
    QSignalBlocker blocker(this);
    int n = std::min<int>(static_cast<int>(pts.size()), rowCount());
    for (int r = 0; r < n; ++r) {
        item(r, kColImgX)->setText(QString::number(pts[r].x, 'f', 3));
        item(r, kColImgY)->setText(QString::number(pts[r].y, 'f', 3));
    }
    // Clear any image cells past the supplied set so stale data is not shown.
    for (int r = n; r < rowCount(); ++r) {
        item(r, kColImgX)->setText(QString());
        item(r, kColImgY)->setText(QString());
    }
}

/// Reads back the image (X, Y) columns; a row whose X or Y text fails to parse as a float
/// is returned as (0, 0).
/// @return one point per row, in row order
std::vector<cv::Point2f> CalibrationPointsTable::imagePoints() const {
    std::vector<cv::Point2f> out;
    out.reserve(rowCount());
    for (int r = 0; r < rowCount(); ++r) {
        bool okX = false, okY = false;
        float x = item(r, kColImgX)->text().toFloat(&okX);
        float y = item(r, kColImgY)->text().toFloat(&okY);
        if (okX && okY) {
            out.emplace_back(x, y);
        } else {
            out.emplace_back(0.f, 0.f);
        }
    }
    return out;
}

/// Fills the world X/Y columns from `xy` (clamped to rowCount()) and forces World Z to
/// "0.000" for each filled row. Runs under a signal blocker.
void CalibrationPointsTable::setWorldPointsXY(const std::vector<cv::Point2f> &xy) {
    QSignalBlocker blocker(this);
    int n = std::min<int>(static_cast<int>(xy.size()), rowCount());
    for (int r = 0; r < n; ++r) {
        item(r, kColWorldX)->setText(QString::number(xy[r].x, 'f', 3));
        item(r, kColWorldY)->setText(QString::number(xy[r].y, 'f', 3));
        item(r, kColWorldZ)->setText(QStringLiteral("0.000"));
    }
}

/// Fills the world X/Y/Z columns from `pts`, one row per point, clamped to rowCount().
/// Runs under a signal blocker.
void CalibrationPointsTable::setWorldPoints(const std::vector<cv::Point3f> &pts) {
    QSignalBlocker blocker(this);
    int n = std::min<int>(static_cast<int>(pts.size()), rowCount());
    for (int r = 0; r < n; ++r) {
        item(r, kColWorldX)->setText(QString::number(pts[r].x, 'f', 3));
        item(r, kColWorldY)->setText(QString::number(pts[r].y, 'f', 3));
        item(r, kColWorldZ)->setText(QString::number(pts[r].z, 'f', 3));
    }
}

/// Reads back the world (X, Y, Z) columns (unparsable text yields 0.0f per QString::toFloat()).
/// @return one point per row, in row order
std::vector<cv::Point3f> CalibrationPointsTable::worldPoints() const {
    std::vector<cv::Point3f> out;
    out.reserve(rowCount());
    for (int r = 0; r < rowCount(); ++r) {
        float x = item(r, kColWorldX)->text().toFloat();
        float y = item(r, kColWorldY)->text().toFloat();
        float z = item(r, kColWorldZ)->text().toFloat();
        out.emplace_back(x, y, z);
    }
    return out;
}

/// Entry point for direct robot teach-pendant input: fills the world columns via
/// setWorldPoints() and then emits pointsEdited(), unlike the other setWorldPoints*()
/// setters (which fill silently).
void CalibrationPointsTable::setWorldPointsFromRobot(const std::vector<cv::Point3f> &pts) {
    setWorldPoints(pts);
    emit pointsEdited();
}

/// Clears the image columns and resets the world columns to "0.000" for every row,
/// under a signal blocker so no pointsEdited() is emitted.
void CalibrationPointsTable::clearAll() {
    QSignalBlocker blocker(this);
    int n = rowCount();
    for (int r = 0; r < n; ++r) {
        if (auto *it = item(r, kColImgX))   it->setText(QString());
        if (auto *it = item(r, kColImgY))   it->setText(QString());
        if (auto *it = item(r, kColWorldX)) it->setText(QStringLiteral("0.000"));
        if (auto *it = item(r, kColWorldY)) it->setText(QStringLiteral("0.000"));
        if (auto *it = item(r, kColWorldZ)) it->setText(QStringLiteral("0.000"));
    }
}

/// Handler for QTableWidget::itemChanged; re-emits pointsEdited() when `item` is non-null
/// and in a world column, and m_blockEditSignal isn't set (in practice it never is —
/// programmatic fills are guarded by QSignalBlocker instead, so this only fires for
/// direct user edits).
void CalibrationPointsTable::onItemChanged(QTableWidgetItem *item) {
    if (m_blockEditSignal || !item) {
        return;
    }
    if (isWorldColumn(item->column())) {
        emit pointsEdited();
    }
}
