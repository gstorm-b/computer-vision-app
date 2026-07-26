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

/// 5-step modal dialog reimplementation of `PatternWizard.jsx`: Image (capture from
/// camera or open file + name + number), Crop (drag rect on canvas, or "use original
/// frame"), Pick Point (click canvas to set pick X/Y), Picking Box (symmetric jaw pair —
/// size + distance + angle), and Finish (preview + summary). All design tokens (colors,
/// fonts, spacings) come from `pattern_theme.h`, which mirrors
/// `ui_scratch/design_handoff_full_project/README.md`.
/// @note Emits requestCameraImage() when the user picks "Capture from Camera" on Step 1;
/// the host must capture a frame and feed it back via setCameraImage().
/// @note Result accessors (patternName(), patternNumber(), patternImage(), pickX()/Y(),
/// pickBoxW()/H()/Dist()/Angle()) are only meaningful after exec() returns Accepted.
class AddPatternWizard : public QDialog {
    Q_OBJECT
public:
    /// Constructs the wizard for group `groupName`, seeded with the names/numbers
    /// already used in that group so Step 1 can flag duplicates.
    /// @param groupName pattern group the new pattern will be added to
    /// @param usedNames existing pattern names in the group, rejected as duplicates
    /// @param usedNumbers existing pattern numbers in the group, rejected as duplicates
    explicit AddPatternWizard(const QString &groupName,
                              const QStringList &usedNames,
                              const QList<int>  &usedNumbers,
                              QWidget *parent = nullptr);
    /// Default destructor; no resources owned beyond normal Qt parent-child cleanup.
    ~AddPatternWizard() override = default;

    // Result accessors (valid only after Accepted) ─────────────────────────
    /// Returns the pattern name entered on Step 1.
    QString patternName()   const { return m_name; }
    /// Returns the pattern number entered on Step 1.
    int     patternNumber() const { return m_number; }
    /// Returns the full, uncropped captured/loaded source image.
    cv::Mat rawImage()      const { return m_capturedMat; }
    /// Returns the image to persist as the pattern: the raw captured/loaded frame when
    /// keepOriginal() is true, otherwise `rawImage()` cropped to `cropRect()` (only if
    /// the crop rect is non-empty and fully inside the image bounds; falls back to the
    /// uncropped image otherwise).
    /// @return a cloned cv::Mat, independent of the wizard's internal buffer
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
    /// Returns whether Step 2's "use original frame" toggle is enabled (crop skipped).
    bool    keepOriginal()  const { return m_keepOriginal; }
    /// Returns the crop rectangle selected on Step 2, in source-image pixel coordinates.
    QRect   cropRect()      const { return m_crop; }

    // Pick coordinates — relative to the crop origin when the user cropped,
    // otherwise raw source-image coordinates.  m_pick is always stored in
    // source-image pixels; we translate at the accessor boundary so callers
    // get the pick in the same frame as the pattern image they receive.
    // int     pickX()         const { return m_keepOriginal ? m_pick.x()
    //                                                       : m_pick.x() - m_crop.x(); }
    // int     pickY()         const { return m_keepOriginal ? m_pick.y()
    //                                                       : m_pick.y() - m_crop.y(); }
    /// Returns the pick point's X coordinate, in source-image pixel coordinates.
    int     pickX()         const { return m_pick.x(); }
    /// Returns the pick point's Y coordinate, in source-image pixel coordinates.
    int     pickY()         const { return m_pick.y(); }
    /// Returns the picking-box width (image pixels) set on Step 4.
    double  pickBoxW()      const { return m_boxW;     }
    /// Returns the picking-box height (image pixels) set on Step 4.
    double  pickBoxH()      const { return m_boxH;     }
    /// Returns the jaw-pair centre-to-centre distance (image pixels) set on Step 4.
    double  pickBoxDist()   const { return m_boxDist;  }
    /// Returns the jaw-pair angle (degrees) set on Step 4.
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
    /// Handles the footer "Next" button: validates the current step via
    /// currentStepValid() and, if it passes, advances via goToStep().
    void onNext();
    /// Handles the footer "Back" button: returns to the previous step via goToStep().
    void onBack();
    /// Handles the footer "Cancel" button / dialog close: rejects the dialog.
    void onCancel();
    /// Handles the Step 5 "Apply"/"Finish" action: accepts the dialog so the caller can
    /// read back the result accessors.
    void onApply();

    // Step 1 ──────────────────────────────────────────────────────────────
    /// Handles the "Capture from Camera" button on Step 1: emits requestCameraImage()
    /// so the host can grab a frame and feed it back via setCameraImage().
    void onPickFromCameraClicked();
    /// Handles the "Open File" button on Step 1: prompts the user to choose an image
    /// file and loads it into the wizard.
    void onPickFromFileClicked();
    /// Handles the "Discard" button on Step 1: clears the currently loaded/captured
    /// image so the user can pick a new one.
    void onDiscardImageClicked();
    /// Handles edits to the Step 1 name field: validates `v` against m_usedNames and
    /// updates the name-error label / Next-button enabled state.
    /// @param v the candidate pattern name
    void onNameChanged(const QString &v);
    /// Handles edits to the Step 1 number field: validates `v` against m_usedNumbers
    /// and updates the number-error label / Next-button enabled state.
    /// @param v the candidate pattern number
    void onNumberChanged(int v);

    // Step 2 ──────────────────────────────────────────────────────────────
    /// Handles the "use original frame" checkbox on Step 2: sets m_keepOriginal and
    /// enables/disables the crop canvas and spin boxes accordingly.
    /// @param on new checkbox state
    void onKeepOriginalToggled(bool on);
    /// Handles crop-rectangle changes from the crop canvas or the X/Y/W/H spin boxes:
    /// updates m_crop and keeps canvas and spin boxes in sync with each other.
    /// @param r the new crop rectangle, in source-image pixel coordinates
    void onCropChanged(const QRect &r);
    /// Handles the "Reset" button on Step 2: restores the crop rectangle to its default.
    void onResetCrop();
    /// Handles the "Center 1:1" button on Step 2: recentres the crop rectangle over the
    /// image at its current size.
    void onCenter1to1Crop();

    // Step 3 ──────────────────────────────────────────────────────────────
    /// Handles pick-point changes from the pick canvas or the X/Y spin boxes: updates
    /// m_pick and keeps canvas and spin boxes in sync with each other.
    /// @param p pick point in absolute source-image pixel coordinates
    /// @param imgp pick point relative to the crop origin (equal to p when uncropped)
    void onPickChanged(const QPoint &p, const QPoint &imgp);
    /// Handles the "Center" button on Step 3: moves the pick point to the center of the
    /// crop rectangle (or of the image, when uncropped).
    void onPickCenter();

    // Step 4 ──────────────────────────────────────────────────────────────
    /// Handles picking-box parameter changes from the Step 4 spin boxes or canvas drag:
    /// keeps the box canvas and width/height/distance/angle spin boxes in sync.
    void onBoxChanged();
    /// Handles the "Reset" button on Step 4: restores the picking box to its default
    /// size, distance, and angle.
    void onBoxReset();
    /// Handles the "Rotate 90°" button on Step 4: rotates the picking-box angle by 90
    /// degrees.
    void onBoxRotate90();

private:
    // ── UI build ──────────────────────────────────────────────────────────
    /// Builds the full wizard UI: header, step rail, stacked step pages, and footer.
    void buildUi();
    /// Builds the dialog header widget (title + group-name subtitle).
    /// @return the constructed header widget, ready to insert into the layout
    QWidget *buildHeader();
    /// Builds the left-hand step rail: numbered bubbles and labels for all 5 steps.
    /// @return the constructed step-rail widget
    QWidget *buildStepRail();
    /// Builds the dialog footer: Back/Next/Cancel buttons and the status label.
    /// @return the constructed footer widget
    QWidget *buildFooter();
    /// Builds the Step 1 (Image) page: name/number fields, camera/file capture buttons,
    /// and the image preview.
    /// @return the constructed page widget
    QWidget *buildStepImage();
    /// Builds the Step 2 (Crop) page: crop canvas, X/Y/W/H spin boxes, and the
    /// keep-original checkbox.
    /// @return the constructed page widget
    QWidget *buildStepCrop();
    /// Builds the Step 3 (Pick Point) page: pick canvas plus X/Y spin boxes.
    /// @return the constructed page widget
    QWidget *buildStepPick();
    /// Builds the Step 4 (Picking Box) page: box canvas plus width/height/distance/angle
    /// spin boxes.
    /// @return the constructed page widget
    QWidget *buildStepBox();
    /// Builds the Step 5 (Finish) page: summary label and read-only preview canvas.
    /// @return the constructed page widget
    QWidget *buildStepFinish();

    // Validation / state ──────────────────────────────────────────────────
    /// Refreshes the step-rail bubbles/labels to reflect m_currentStep (active,
    /// completed, and upcoming styling).
    void updateStepRail();
    /// Refreshes the footer status label and the Back/Next buttons' enabled state for
    /// the current step.
    void updateFooterStatus();
    /// Rebuilds the Step 5 summary text and preview canvas from the current result
    /// state (name, number, crop, pick, box).
    void refreshFinishSummary();
    /// Returns whether the current step's inputs are valid enough to advance (eg. name
    /// and number uniqueness on Step 1).
    bool currentStepValid() const;
    /// Switches the stacked widget to `step`, updating the step rail and footer.
    /// @param step zero-based step index to switch to
    void goToStep(int step);

    /// Resizes all geometry spin boxes so they can mirror the canvas for the
    /// currently-loaded image, and clamps the default crop/pick to fit.
    /// @param imageW loaded image width, in pixels
    /// @param imageH loaded image height, in pixels
    /// @note Called from setCameraImage() / setLoadedImage().
    void onImageSizeChanged(int imageW, int imageH);

    // Helpers ─────────────────────────────────────────────────────────────
    /// Creates the numbered circular bubble label used for step `idx` in the step rail.
    /// @param idx zero-based step index
    /// @return the newly created label
    QLabel *makeStepBubble(int idx);

private:
    // Inputs ─────────────────────────────────────────────────────────────
    QString     m_groupName;   ///< Name of the pattern group this wizard adds to.
    QStringList m_usedNames;   ///< Existing pattern names in the group (Step 1 duplicate check).
    QList<int>  m_usedNumbers; ///< Existing pattern numbers in the group (Step 1 duplicate check).

    // Result state ───────────────────────────────────────────────────────
    QString m_name;                    ///< Pattern name entered on Step 1.
    int     m_number{1};               ///< Pattern number entered on Step 1.
    cv::Mat m_capturedMat;             ///< Full captured/loaded source image (uncropped).
    QString m_imageSource;       ///< Image origin: "camera", "file", or empty when none loaded.
    QString m_imageFilename;           ///< Filename of the loaded image, when loaded from file.
    bool    m_keepOriginal{true};      ///< Whether Step 2's "use original frame" toggle is on.
    QRect   m_crop{80, 60, 400, 260};  ///< Crop rectangle selected on Step 2 (source-image px).
    QPoint  m_pick{280, 195};          ///< Pick point, in source-image pixel coordinates.
    double  m_boxW{120}, m_boxH{80};   ///< Picking-box width/height (image pixels), Step 4.
    double  m_boxDist{90}, m_boxAngle{0}; ///< Jaw-pair distance (image px) / angle (degrees), Step 4.

    // UI ─────────────────────────────────────────────────────────────────
    int                  m_currentStep{0};        ///< Zero-based index of the visible step.
    QStackedWidget      *m_stack{nullptr};         ///< Stacked widget holding the 5 step pages.
    QList<QLabel*>       m_stepBubbles;            ///< Numbered circular labels in the step rail.
    QList<QLabel*>       m_stepLabels;              ///< Step-name labels in the step rail.
    QLabel              *m_subtitleLabel{nullptr};  ///< Header subtitle label (group name).
    QLabel              *m_footerStatus{nullptr};   ///< Footer status/validation message label.
    QPushButton         *m_btnBack{nullptr};        ///< Footer "Back" button.
    QPushButton         *m_btnNext{nullptr};        ///< Footer "Next" button.
    QPushButton         *m_btnCancel{nullptr};      ///< Footer "Cancel" button.

    // Step 1
    QLineEdit           *m_inputName{nullptr};        ///< Step 1 pattern-name text field.
    QSpinBox            *m_inputNumber{nullptr};      ///< Step 1 pattern-number spin box.
    QLabel              *m_lblNameError{nullptr};     ///< Step 1 name-validation error label.
    QLabel              *m_lblNumberError{nullptr};   ///< Step 1 number-validation error label.
    QLabel              *m_lblImageStatus{nullptr};   ///< Step 1 image-loaded status label.
    QLabel              *m_imagePreviewLabel{nullptr}; ///< Step 1 thumbnail preview of the image.
    QPushButton         *m_btnFromCamera{nullptr};    ///< Step 1 "Capture from Camera" button.
    QPushButton         *m_btnFromFile{nullptr};      ///< Step 1 "Open File" button.
    QPushButton         *m_btnDiscardImage{nullptr};  ///< Step 1 "Discard" button.

    // Step 2
    QCheckBox           *m_chkKeepOriginal{nullptr};  ///< Step 2 "use original frame" checkbox.
    AddPatternImageCanvas *m_cropCanvas{nullptr};     ///< Step 2 drag-to-crop canvas.
    QSpinBox            *m_cropX{nullptr};            ///< Step 2 crop-rectangle X spin box.
    QSpinBox            *m_cropY{nullptr};            ///< Step 2 crop-rectangle Y spin box.
    QSpinBox            *m_cropW{nullptr};            ///< Step 2 crop-rectangle width spin box.
    QSpinBox            *m_cropH{nullptr};            ///< Step 2 crop-rectangle height spin box.

    // Step 3
    AddPatternImageCanvas *m_pickCanvas{nullptr};  ///< Step 3 click-to-pick canvas.
    QSpinBox            *m_pickXSpin{nullptr};     ///< Step 3 pick-X spin box.
    QSpinBox            *m_pickYSpin{nullptr};     ///< Step 3 pick-Y spin box.

    // Step 4
    AddPatternImageCanvas *m_boxCanvas{nullptr};      ///< Step 4 picking-box overlay canvas.
    QDoubleSpinBox      *m_boxWSpin{nullptr};         ///< Step 4 box-width spin box.
    QDoubleSpinBox      *m_boxHSpin{nullptr};         ///< Step 4 box-height spin box.
    QDoubleSpinBox      *m_boxDistSpin{nullptr};      ///< Step 4 jaw-pair distance spin box.
    QDoubleSpinBox      *m_boxAngleSpin{nullptr};     ///< Step 4 jaw-pair angle spin box.

    // Step 5
    QLabel              *m_finishSummary{nullptr};    ///< Step 5 read-only result summary text.
    AddPatternImageCanvas *m_finishCanvas{nullptr};   ///< Step 5 read-only preview canvas (locked).
};

#endif // ADD_PATTERN_WIZARD_H
