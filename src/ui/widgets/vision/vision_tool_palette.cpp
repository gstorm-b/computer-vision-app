#include "ui/widgets/vision/vision_tool_palette.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QToolButton>

namespace {

QToolButton *makeButton(const QString &text,
                        const QString &toolTip,
                        bool checkable = false)
{
    auto *button = new QToolButton;
    button->setText(text);
    button->setToolTip(toolTip);
    button->setCheckable(checkable);
    button->setAutoRaise(true);
    button->setMinimumHeight(24);
    return button;
}

} // namespace

VisionToolPalette::VisionToolPalette(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);

    m_selectButton = makeButton(tr("Sel"), tr("Select and move ROI"), true);
    m_panButton = makeButton(tr("Pan"), tr("Pan the image"), true);
    m_rectButton = makeButton(tr("Rect"), tr("Draw axis-aligned ROI"), true);
    m_rotatedButton = makeButton(tr("Rot"), tr("Draw rotated ROI"), true);
    m_deleteButton = makeButton(tr("Del"), tr("Delete selected ROI"));
    m_fitButton = makeButton(tr("Fit"), tr("Fit image to view"));
    m_undoButton = makeButton(tr("Undo"), tr("Undo ROI change"));
    m_redoButton = makeButton(tr("Redo"), tr("Redo ROI change"));

    m_modeGroup->addButton(m_selectButton, static_cast<int>(ToolMode::SelectMove));
    m_modeGroup->addButton(m_panButton, static_cast<int>(ToolMode::Pan));
    m_modeGroup->addButton(m_rectButton, static_cast<int>(ToolMode::DrawAxisAlignedRect));
    m_modeGroup->addButton(m_rotatedButton, static_cast<int>(ToolMode::DrawRotatedRect));

    layout->addWidget(m_selectButton);
    layout->addWidget(m_panButton);
    layout->addWidget(m_rectButton);
    layout->addWidget(m_rotatedButton);
    layout->addSpacing(8);
    layout->addWidget(m_deleteButton);
    layout->addWidget(m_fitButton);
    layout->addStretch(1);
    layout->addWidget(m_undoButton);
    layout->addWidget(m_redoButton);

    connect(m_modeGroup, &QButtonGroup::buttonToggled,
            this, &VisionToolPalette::onToolButtonClicked);
    connect(m_deleteButton, &QToolButton::clicked, this, &VisionToolPalette::deleteRequested);
    connect(m_fitButton, &QToolButton::clicked, this, &VisionToolPalette::fitRequested);
    connect(m_undoButton, &QToolButton::clicked, this, &VisionToolPalette::undoRequested);
    connect(m_redoButton, &QToolButton::clicked, this, &VisionToolPalette::redoRequested);

    setActiveMode(ToolMode::SelectMove);
}

void VisionToolPalette::setActiveMode(ToolMode mode)
{
    if (auto *button = buttonForMode(mode)) {
        button->setChecked(true);
    }
}

void VisionToolPalette::setReadOnly(bool readOnly)
{
    m_rectButton->setEnabled(!readOnly);
    m_rotatedButton->setEnabled(!readOnly);
    m_deleteButton->setEnabled(!readOnly);
}

void VisionToolPalette::setUndoAvailable(bool enabled)
{
    m_undoButton->setEnabled(enabled);
}

void VisionToolPalette::setRedoAvailable(bool enabled)
{
    m_redoButton->setEnabled(enabled);
}

void VisionToolPalette::onToolButtonClicked(QAbstractButton *button, bool checked)
{
    if (!checked || !button) return;
    emit toolModeRequested(static_cast<ToolMode>(m_modeGroup->id(button)));
}

QToolButton *VisionToolPalette::buttonForMode(ToolMode mode) const
{
    return qobject_cast<QToolButton *>(m_modeGroup->button(static_cast<int>(mode)));
}
