#ifndef VISION_TOOL_PALETTE_H
#define VISION_TOOL_PALETTE_H

#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QToolButton;

/// Toolbar of mode/action buttons (select, pan, draw rect, draw rotated rect, delete, fit,
/// undo, redo) for the ROI editor; the four mode buttons are mutually exclusive via a
/// QButtonGroup and emit toolModeRequested, while the action buttons each emit their own
/// one-shot signal.
class VisionToolPalette : public QWidget {
    Q_OBJECT

public:
    /// Interaction mode for the ROI canvas, selected by the mutually-exclusive mode buttons.
    enum class ToolMode {
        SelectMove,          ///< Select and move/resize existing ROIs.
        Pan,                 ///< Pan the image instead of editing ROIs.
        DrawAxisAlignedRect, ///< Draw a new axis-aligned rectangular ROI.
        DrawRotatedRect,     ///< Draw a new rotated rectangular ROI.
    };

    explicit VisionToolPalette(QWidget *parent = nullptr);

    /// Checks the mode button corresponding to `mode` (does not itself emit toolModeRequested).
    void setActiveMode(ToolMode mode);
    /// Enables/disables the draw-rect, draw-rotated-rect, and delete buttons for `readOnly`;
    /// select/pan/fit/undo/redo remain usable.
    void setReadOnly(bool readOnly);
    /// Enables/disables the undo button.
    void setUndoAvailable(bool enabled);
    /// Enables/disables the redo button.
    void setRedoAvailable(bool enabled);

signals:
    void toolModeRequested(VisionToolPalette::ToolMode mode); ///< Emitted when a mode button is checked by the user.
    void deleteRequested();  ///< Emitted when the delete button is clicked.
    void fitRequested();     ///< Emitted when the fit button is clicked.
    void undoRequested();    ///< Emitted when the undo button is clicked.
    void redoRequested();    ///< Emitted when the redo button is clicked.

private slots:
    /// Handles QButtonGroup::buttonToggled for the mode group: ignores untoggle events and
    /// button==nullptr, otherwise emits toolModeRequested with the mode mapped to `button`'s
    /// group id.
    void onToolButtonClicked(QAbstractButton *button, bool checked);

private:
    /// @return the mode button registered under `mode`'s group id, or nullptr if none.
    QToolButton *buttonForMode(ToolMode mode) const;

private:
    QButtonGroup *m_modeGroup{nullptr};    ///< Exclusive group tying the four mode buttons together, keyed by ToolMode value.
    QToolButton *m_selectButton{nullptr};  ///< "Sel" button for ToolMode::SelectMove.
    QToolButton *m_panButton{nullptr};     ///< "Pan" button for ToolMode::Pan.
    QToolButton *m_rectButton{nullptr};    ///< "Rect" button for ToolMode::DrawAxisAlignedRect.
    QToolButton *m_rotatedButton{nullptr}; ///< "Rot" button for ToolMode::DrawRotatedRect.
    QToolButton *m_deleteButton{nullptr};  ///< "Del" action button; disabled when read-only.
    QToolButton *m_fitButton{nullptr};     ///< "Fit" action button; fits the image to the view.
    QToolButton *m_undoButton{nullptr};    ///< "Undo" action button; enabled via setUndoAvailable.
    QToolButton *m_redoButton{nullptr};    ///< "Redo" action button; enabled via setRedoAvailable.
};

#endif // VISION_TOOL_PALETTE_H
