#include "ui/widgets/vision/vision_tool_palette.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QToolButton>

/// Anonymous namespace holding a small local factory helper for constructing the palette's
/// tool buttons.
namespace {

/// Creates an auto-raised QToolButton with the given label/tooltip, a fixed minimum height
/// of 24px, and no parent (caller/layout takes ownership).
/// @param checkable whether the button toggles (used for the mutually-exclusive mode buttons).
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

/// Builds the eight tool buttons, registers the four mode buttons in an exclusive
/// QButtonGroup keyed by their ToolMode value, lays them out horizontally (mode buttons,
/// delete/fit, a stretch, then undo/redo), wires each button's signal to the corresponding
/// palette signal/slot, and defaults the active mode to ToolMode::SelectMove.
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

/// Checks the button for `mode` if one is registered; does nothing otherwise.
void VisionToolPalette::setActiveMode(ToolMode mode)
{
    if (auto *button = buttonForMode(mode)) {
        button->setChecked(true);
    }
}

/// Disables the draw-rect, draw-rotated-rect, and delete buttons when `readOnly` is true
/// (re-enables them otherwise); the select/pan/fit/undo/redo buttons are unaffected.
void VisionToolPalette::setReadOnly(bool readOnly)
{
    m_rectButton->setEnabled(!readOnly);
    m_rotatedButton->setEnabled(!readOnly);
    m_deleteButton->setEnabled(!readOnly);
}

/// Enables/disables the undo button.
void VisionToolPalette::setUndoAvailable(bool enabled)
{
    m_undoButton->setEnabled(enabled);
}

/// Enables/disables the redo button.
void VisionToolPalette::setRedoAvailable(bool enabled)
{
    m_redoButton->setEnabled(enabled);
}

/// Slot for the mode QButtonGroup's buttonToggled signal: ignores the "unchecked" toggle
/// event and a null button, otherwise emits toolModeRequested with the ToolMode mapped from
/// the button's group id.
void VisionToolPalette::onToolButtonClicked(QAbstractButton *button, bool checked)
{
    if (!checked || !button) return;
    emit toolModeRequested(static_cast<ToolMode>(m_modeGroup->id(button)));
}

/// @return the QToolButton registered in m_modeGroup under `mode`'s integer value, or
/// nullptr if the cast/lookup fails.
QToolButton *VisionToolPalette::buttonForMode(ToolMode mode) const
{
    return qobject_cast<QToolButton *>(m_modeGroup->button(static_cast<int>(mode)));
}
