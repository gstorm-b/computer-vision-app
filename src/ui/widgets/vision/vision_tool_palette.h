#ifndef VISION_TOOL_PALETTE_H
#define VISION_TOOL_PALETTE_H

#include <QWidget>

class QAbstractButton;
class QButtonGroup;
class QToolButton;

class VisionToolPalette : public QWidget {
    Q_OBJECT

public:
    enum class ToolMode {
        SelectMove,
        Pan,
        DrawAxisAlignedRect,
        DrawRotatedRect,
    };

    explicit VisionToolPalette(QWidget *parent = nullptr);

    void setActiveMode(ToolMode mode);
    void setReadOnly(bool readOnly);
    void setUndoAvailable(bool enabled);
    void setRedoAvailable(bool enabled);

signals:
    void toolModeRequested(VisionToolPalette::ToolMode mode);
    void deleteRequested();
    void fitRequested();
    void undoRequested();
    void redoRequested();

private slots:
    void onToolButtonClicked(QAbstractButton *button, bool checked);

private:
    QToolButton *buttonForMode(ToolMode mode) const;

private:
    QButtonGroup *m_modeGroup{nullptr};
    QToolButton *m_selectButton{nullptr};
    QToolButton *m_panButton{nullptr};
    QToolButton *m_rectButton{nullptr};
    QToolButton *m_rotatedButton{nullptr};
    QToolButton *m_deleteButton{nullptr};
    QToolButton *m_fitButton{nullptr};
    QToolButton *m_undoButton{nullptr};
    QToolButton *m_redoButton{nullptr};
};

#endif // VISION_TOOL_PALETTE_H
