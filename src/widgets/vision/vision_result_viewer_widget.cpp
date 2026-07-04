#include "widgets/vision/vision_result_viewer_widget.h"

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

#include "widgets/vision/vision_canvas.h"
#include "widgets/vision/vision_tool_palette.h"

namespace {

QAction *makeToggleAction(QObject *parent, const QString &text, bool checked = true)
{
    auto *action = new QAction(text, parent);
    action->setCheckable(true);
    action->setChecked(checked);
    return action;
}

QToolButton *makeToolbarButton(const QString &text, const QString &toolTip)
{
    auto *button = new QToolButton;
    button->setText(text);
    button->setToolTip(toolTip);
    button->setAutoRaise(true);
    button->setMinimumHeight(24);
    return button;
}

} // namespace

VisionResultViewerWidget::VisionResultViewerWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(4);

    m_fitButton = makeToolbarButton(tr("Fit"), tr("Fit image to view"));
    m_overlayButton = makeToolbarButton(tr("Overlays"), tr("Show or hide overlay details"));
    m_overlayButton->setPopupMode(QToolButton::InstantPopup);

    auto *overlayMenu = new QMenu(m_overlayButton);
    m_scoreAction = makeToggleAction(this, tr("Score"));
    m_angleAction = makeToggleAction(this, tr("Angle"));
    m_patternAction = makeToggleAction(this, tr("Pattern"));
    m_rejectedAction = makeToggleAction(this, tr("Rejected"));
    m_faultAction = makeToggleAction(this, tr("Fault"));
    m_outputAction = makeToggleAction(this, tr("Output"));
    m_pickingBoxAction = makeToggleAction(this, tr("Picking Box"), false);
    m_signalsAction = makeToggleAction(this, tr("Signals"), false);

    for (QAction *action : {m_scoreAction, m_angleAction, m_patternAction,
                            m_rejectedAction, m_faultAction, m_outputAction,
                            m_pickingBoxAction,
                            m_signalsAction}) {
        overlayMenu->addAction(action);
    }
    m_overlayButton->setMenu(overlayMenu);

    toolbar->addWidget(m_fitButton);
    toolbar->addWidget(m_overlayButton);
    toolbar->addStretch(1);

    m_canvas = new VisionCanvas(this);
    m_canvas->setReadOnly(true);
    m_canvas->setToolMode(VisionToolPalette::ToolMode::Pan);

    layout->addLayout(toolbar);
    layout->addWidget(m_canvas, 1);

    connect(m_fitButton, &QToolButton::clicked, m_canvas, &VisionCanvas::fitImageToView);
    connect(m_canvas, &VisionCanvas::resultObjectSelectionChanged,
            this, &VisionResultViewerWidget::resultObjectSelectionChanged);
    for (QAction *action : {m_scoreAction, m_angleAction, m_patternAction,
                            m_rejectedAction, m_faultAction, m_outputAction,
                            m_pickingBoxAction,
                            m_signalsAction}) {
        connect(action, &QAction::toggled, this, [this](bool) {
            m_canvas->setOverlayVisibility(visibilityFromActions());
        });
    }
}

void VisionResultViewerWidget::setImage(const cv::Mat &image)
{
    m_canvas->setImage(image);
}

void VisionResultViewerWidget::setImage(const QPixmap &pixmap)
{
    m_canvas->setImage(pixmap);
}

void VisionResultViewerWidget::setOverlay(const VisionResultOverlay &overlay)
{
    m_overlay = overlay;
    // The toolbar toggles are the source of truth for overlay visibility. A
    // freshly produced result overlay carries default visibility, so keep the
    // user's current selection instead of resetting the toolbar (which would
    // force the user to re-toggle to see the overlay items again).
    m_overlay.visibility = visibilityFromActions();
    m_canvas->clearSelectedResultObject();
    m_canvas->setResultOverlay(m_overlay);
}

void VisionResultViewerWidget::setOverlayVisibility(const VisionOverlayVisibility &visibility)
{
    m_overlay.visibility = visibility;
    syncActions();
    m_canvas->setOverlayVisibility(visibility);
}

void VisionResultViewerWidget::setSelectedResultObject(int objectIndex)
{
    m_canvas->setSelectedResultObject(objectIndex);
}

void VisionResultViewerWidget::clearSelectedResultObject()
{
    m_canvas->clearSelectedResultObject();
}

void VisionResultViewerWidget::clearResult()
{
    m_canvas->clearSelectedResultObject();
    m_canvas->clearImage();
    m_overlay = VisionResultOverlay();
}

void VisionResultViewerWidget::syncActions()
{
    m_scoreAction->setChecked(m_overlay.visibility.showScore);
    m_angleAction->setChecked(m_overlay.visibility.showAngle);
    m_patternAction->setChecked(m_overlay.visibility.showPatternId);
    m_rejectedAction->setChecked(m_overlay.visibility.showRejectedCandidates);
    m_faultAction->setChecked(m_overlay.visibility.showFaultMarkers);
    m_outputAction->setChecked(m_overlay.visibility.showSentToOutputMarkers);
    m_pickingBoxAction->setChecked(m_overlay.visibility.showPickingBoxes);
    m_signalsAction->setChecked(m_overlay.visibility.showRuntimeSignalValues);
}

VisionOverlayVisibility VisionResultViewerWidget::visibilityFromActions() const
{
    VisionOverlayVisibility visibility;
    visibility.showScore = m_scoreAction->isChecked();
    visibility.showAngle = m_angleAction->isChecked();
    visibility.showPatternId = m_patternAction->isChecked();
    visibility.showRejectedCandidates = m_rejectedAction->isChecked();
    visibility.showFaultMarkers = m_faultAction->isChecked();
    visibility.showSentToOutputMarkers = m_outputAction->isChecked();
    visibility.showPickingBoxes = m_pickingBoxAction->isChecked();
    visibility.showRuntimeSignalValues = m_signalsAction->isChecked();
    return visibility;
}
