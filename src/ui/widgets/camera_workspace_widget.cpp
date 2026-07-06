#include "ui/widgets/camera_workspace_widget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>

CameraWorkspaceWidget::CameraWorkspaceWidget(QWidget *parent)
    : QWidget(parent)
{
    m_rowsLayout = new QVBoxLayout(this);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(4);
    m_rowsLayout->addStretch(1);   // keep rows top-aligned
}

void CameraWorkspaceWidget::clearRows()
{
    for (auto it = m_rows.cbegin(); it != m_rows.cend(); ++it) {
        m_rowsLayout->removeWidget(it.value().container);
        it.value().container->deleteLater();
    }
    m_rows.clear();
}

void CameraWorkspaceWidget::setCameras(const QMap<QString, QString> &idToName)
{
    m_idToName = idToName;
    clearRows();

    // Stable order: by display name, then by id as a tie-breaker.
    QList<QString> ids = idToName.keys();
    std::sort(ids.begin(), ids.end(), [&](const QString &a, const QString &b) {
        const QString na = idToName.value(a);
        const QString nb = idToName.value(b);
        if (na == nb) return a < b;
        return na < nb;
    });

    int insertAt = 0;   // insert before the trailing stretch
    for (const QString &id : ids) {
        Row row;
        row.container = new QWidget(this);
        auto *v = new QVBoxLayout(row.container);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);

        // ── Working-workspace line: camera name + ROI status + Set button ──
        auto *top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 0);
        top->setSpacing(8);

        row.useCheck = new QCheckBox(idToName.value(id), row.container);
        row.useCheck->setObjectName(QStringLiteral("chk_use_workspace"));
        row.useCheck->setMinimumWidth(160);

        row.statusLabel = new QLabel(tr("Not set"), row.container);
        row.statusLabel->setObjectName(QStringLiteral("lbl_workspace_status"));

        row.setButton = new QPushButton(tr("Set workspace…"), row.container);
        row.setButton->setObjectName(QStringLiteral("btn_set_workspace"));
        row.setButton->setFocusPolicy(Qt::NoFocus);

        top->addWidget(row.useCheck);
        top->addWidget(row.statusLabel, 1);
        top->addWidget(row.setButton);

        // ── Condition-workspace line: indented checkbox + ROI status ───────
        auto *bottom = new QHBoxLayout;
        bottom->setContentsMargins(0, 0, 0, 0);
        bottom->setSpacing(8);

        row.conditionCheck = new QCheckBox(tr("Use as condition"), row.container);
        row.conditionCheck->setObjectName(QStringLiteral("chk_use_condition"));
        row.conditionCheck->setMinimumWidth(160);

        row.conditionStatus = new QLabel(tr("Not set"), row.container);
        row.conditionStatus->setObjectName(QStringLiteral("lbl_condition_status"));

        bottom->addSpacing(20);   // align under the camera name
        bottom->addWidget(row.conditionCheck);
        bottom->addWidget(row.conditionStatus, 1);

        v->addLayout(top);
        v->addLayout(bottom);

        const QString cameraId = id;
        connect(row.useCheck, &QCheckBox::toggled, this,
                [this, cameraId](bool on) { emit useWorkspaceToggled(cameraId, on); });
        connect(row.conditionCheck, &QCheckBox::toggled, this,
                [this, cameraId](bool on) { emit useConditionToggled(cameraId, on); });
        connect(row.setButton, &QPushButton::clicked, this,
                [this, cameraId]() { emit setWorkspaceRequested(cameraId); });

        m_rows.insert(id, row);
        m_rowsLayout->insertWidget(insertAt++, row.container);
    }
}

void CameraWorkspaceWidget::setWorkspaceState(const QString &cameraId,
                                              bool useWorkspace, const QRectF &roi,
                                              bool useCondition, const QRectF &conditionRoi,
                                              bool hasImage)
{
    auto it = m_rows.find(cameraId);
    if (it == m_rows.end())
        return;

    Row &row = it.value();

    // Render an ROI rect as a short status string, or "Not set" when empty.
    auto roiText = [&](const QRectF &r) {
        if (r.width() <= 0.0 || r.height() <= 0.0)
            return tr("Not set");
        QString text = tr("ROI %1×%2 @ (%3, %4)")
                           .arg(qRound(r.width())).arg(qRound(r.height()))
                           .arg(qRound(r.x())).arg(qRound(r.y()));
        if (!hasImage)
            text += tr("  (no image)");
        return text;
    };

    {
        QSignalBlocker block(row.useCheck);
        row.useCheck->setChecked(useWorkspace);
    }
    row.statusLabel->setText(roiText(roi));

    {
        QSignalBlocker block(row.conditionCheck);
        row.conditionCheck->setChecked(useCondition);
    }
    row.conditionStatus->setText(roiText(conditionRoi));
}
