#ifndef ADD_PATTERN_WIZARD_H
#define ADD_PATTERN_WIZARD_H

#include <QDialog>
#include <QStringList>
#include <QString>
#include <QPoint>
#include <QSize>
#include <QRect>
#include <opencv2/core.hpp>

class QStackedWidget;
class QLabel;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QToolButton;
class QCheckBox;
class QHBoxLayout;
class QVBoxLayout;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsView;

class AddPatternWizardStepRail;   // forward
class AddPatternImageCanvas;      // forward — canvas widget for crop / pick / box

// ─────────────────────────────────────────────────────────────────────────────
//  AddPatternWizard
//
//  5-step modal dialog reimplementation of `PatternWizard.jsx`.
//
//      1. Image       — capture from camera or open file + name + number
//      2. Crop        — drag rect on canvas, or "use original frame"
//      3. Pick Point  — click canvas to set pick X/Y
//      4. Picking Box — symmetric jaw pair (size + distance + angle)
//      5. Finish      — preview + summary
//
//  All design tokens (colors, fonts, spacings) come from `pattern_theme.h`,
//  which mirrors `ui_scratch/design_handoff_full_project/README.md`.
//
//  Signals:
//      requestCameraImage()   — host should capture from active camera
//                               then call setCameraImage().
//
//  Result fields (read after exec() == Accepted):
//      patternName(), patternNumber(), patternImage() (cv::Mat),
//      pickX/Y(), pickBoxW/H/Dist/Angle().
// ─────────────────────────────────────────────────────────────────────────────

class AddPatternWizard : public QDialog {
    Q_OBJECT
public:
    explicit AddPatternWizard(const QString &groupName,
                              const QStringList &usedNames,
                              const QList<int>  &usedNumbers,
                              QWidget *parent = nullptr);
    ~AddPatternWizard() override = default;

    // Result accessors (valid only after Accepted) ─────────────────────────
    QString patternName()   const { return m_name; }
    int     patternNumber() const { return m_number; }
    cv::Mat rawImage()      const { return m_capturedMat; }
    cv::Mat patternImage()  const {
        cv::Mat patternImage = m_capturedMat;
        if (!m_keepOriginal) {
            const QRect r = m_crop;
            if (r.width() > 0 && r.height() > 0
                && r.x() >= 0 && r.y() >= 0
                && r.x() + r.width()  <= patternImage.cols
                && r.y() + r.height() <= patternImage.rows)
            {
                patternImage = patternImage(cv::Rect(r.x(), r.y(),
                                                     r.width(), r.height())).clone();
            }
        }
        return patternImage;
    }
    bool    keepOriginal()  const { return m_keepOriginal; }
    QRect   cropRect()      const { return m_crop; }

    // Pick coordinates — relative to the crop origin when the user cropped,
    // otherwise raw source-image coordinates.  m_pick is always stored in
    // source-image pixels; we translate at the accessor boundary so callers
    // get the pick in the same frame as the pattern image they receive.
    // int     pickX()         const { return m_keepOriginal ? m_pick.x()
    //                                                       : m_pick.x() - m_crop.x(); }
    // int     pickY()         const { return m_keepOriginal ? m_pick.y()
    //                                                       : m_pick.y() - m_crop.y(); }
    int     pickX()         const { return m_pick.x(); }
    int     pickY()         const { return m_pick.y(); }
    double  pickBoxW()      const { return m_boxW;     }
    double  pickBoxH()      const { return m_boxH;     }
    double  pickBoxDist()   const { return m_boxDist;  }
    double  pickBoxAngle()  const { return m_boxAngle; }

public slots:
    /// Called by host when camera capture finishes (in response to
    /// requestCameraImage()).  Feeds the image into Step 1 preview.
    void setCameraImage(const cv::Mat &image);

    /// Allow host to set initial image (eg. when file is loaded externally).
    void setLoadedImage(const cv::Mat &image, const QString &filename);

signals:
    /// User clicked "Capture from Camera" on Step 1.  Host should grab
    /// from the active camera and feed back via setCameraImage().
    void requestCameraImage();

private slots:
    // Step rail / nav ─────────────────────────────────────────────────────
    void onNext();
    void onBack();
    void onCancel();
    void onApply();

    // Step 1 ──────────────────────────────────────────────────────────────
    void onPickFromCameraClicked();
    void onPickFromFileClicked();
    void onDiscardImageClicked();
    void onNameChanged(const QString &v);
    void onNumberChanged(int v);

    // Step 2 ──────────────────────────────────────────────────────────────
    void onKeepOriginalToggled(bool on);
    void onCropChanged(const QRect &r);
    void onResetCrop();
    void onCenter1to1Crop();

    // Step 3 ──────────────────────────────────────────────────────────────
    void onPickChanged(const QPoint &p, const QPoint &imgp);
    void onPickCenter();

    // Step 4 ──────────────────────────────────────────────────────────────
    void onBoxChanged();
    void onBoxReset();
    void onBoxRotate90();

private:
    // ── UI build ──────────────────────────────────────────────────────────
    void buildUi();
    QWidget *buildHeader();
    QWidget *buildStepRail();
    QWidget *buildFooter();
    QWidget *buildStepImage();
    QWidget *buildStepCrop();
    QWidget *buildStepPick();
    QWidget *buildStepBox();
    QWidget *buildStepFinish();

    // Validation / state ──────────────────────────────────────────────────
    void updateStepRail();
    void updateFooterStatus();
    void refreshFinishSummary();
    bool currentStepValid() const;
    void goToStep(int step);

    // Resize all geometry spin boxes so they can mirror the canvas for the
    // currently-loaded image, and clamp the default crop/pick to fit.  Called
    // from setCameraImage() / setLoadedImage().
    void onImageSizeChanged(int imageW, int imageH);

    // Helpers ─────────────────────────────────────────────────────────────
    QLabel *makeStepBubble(int idx);

private:
    // Inputs ─────────────────────────────────────────────────────────────
    QString     m_groupName;
    QStringList m_usedNames;
    QList<int>  m_usedNumbers;

    // Result state ───────────────────────────────────────────────────────
    QString m_name;
    int     m_number{1};
    cv::Mat m_capturedMat;
    QString m_imageSource;       // "camera" | "file" | empty
    QString m_imageFilename;
    bool    m_keepOriginal{true};
    QRect   m_crop{80, 60, 400, 260};
    QPoint  m_pick{280, 195};
    double  m_boxW{120}, m_boxH{80};
    double  m_boxDist{90}, m_boxAngle{0};

    // UI ─────────────────────────────────────────────────────────────────
    int                  m_currentStep{0};
    QStackedWidget      *m_stack{nullptr};
    QList<QLabel*>       m_stepBubbles;
    QList<QLabel*>       m_stepLabels;
    QLabel              *m_subtitleLabel{nullptr};
    QLabel              *m_footerStatus{nullptr};
    QPushButton         *m_btnBack{nullptr};
    QPushButton         *m_btnNext{nullptr};
    QPushButton         *m_btnCancel{nullptr};

    // Step 1
    QLineEdit           *m_inputName{nullptr};
    QSpinBox            *m_inputNumber{nullptr};
    QLabel              *m_lblNameError{nullptr};
    QLabel              *m_lblNumberError{nullptr};
    QLabel              *m_lblImageStatus{nullptr};
    QLabel              *m_imagePreviewLabel{nullptr};
    QPushButton         *m_btnFromCamera{nullptr};
    QPushButton         *m_btnFromFile{nullptr};
    QPushButton         *m_btnDiscardImage{nullptr};

    // Step 2
    QCheckBox           *m_chkKeepOriginal{nullptr};
    AddPatternImageCanvas *m_cropCanvas{nullptr};
    QSpinBox            *m_cropX{nullptr};
    QSpinBox            *m_cropY{nullptr};
    QSpinBox            *m_cropW{nullptr};
    QSpinBox            *m_cropH{nullptr};

    // Step 3
    AddPatternImageCanvas *m_pickCanvas{nullptr};
    QSpinBox            *m_pickXSpin{nullptr};
    QSpinBox            *m_pickYSpin{nullptr};

    // Step 4
    AddPatternImageCanvas *m_boxCanvas{nullptr};
    QDoubleSpinBox      *m_boxWSpin{nullptr};
    QDoubleSpinBox      *m_boxHSpin{nullptr};
    QDoubleSpinBox      *m_boxDistSpin{nullptr};
    QDoubleSpinBox      *m_boxAngleSpin{nullptr};

    // Step 5
    QLabel              *m_finishSummary{nullptr};
    AddPatternImageCanvas *m_finishCanvas{nullptr};
};

#endif // ADD_PATTERN_WIZARD_H
