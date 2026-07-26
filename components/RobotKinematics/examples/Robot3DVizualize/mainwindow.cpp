#include "mainwindow.h"
#include "Robot3DVisualizerLogic.h"
#include "ui_mainwindow.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QColor>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <QVTKOpenGLNativeWidget.h>

#include <RobotKinematics/Collision/BuiltInCollisionProfiles.h>
#include <RobotKinematics/Collision/CollisionProfileJsonLoader.h>
#include <RobotKinematics/Collision/CollisionProfileValidator.h>
#include <RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h>
#include <RobotKinematics/Collision/MeshCollisionProfileValidator.h>
#include <RobotKinematics/Core/Units.h>
#include <RobotKinematics/Kinematics/JointLimitValidator.h>
#include <RobotKinematics/Presets/NachiMZ04D.h>

#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkCylinderSource.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkLight.h>
#include <vtkLineSource.h>
#include <vtkMatrix4x4.h>
#include <vtkNamedColors.h>
#include <vtkOrientationMarkerWidget.h>
#include <vtkPlaneSource.h>
#include <vtkPolyDataAlgorithm.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkSTLReader.h>

#include <array>
#include <algorithm>
#include <optional>

using namespace RobotKinematics;

/// Internal helpers used only within this translation unit: asset-path resolution, VTK/Eigen
/// pose-matrix conversions, and the static part/pose constants used to build and pose the
/// Nachi MZ04D example scene.
namespace
{
/// Static descriptor for one visual part (STL mesh) of the robot model: which file to load,
/// how to display it, and which FK link drives its pose.
struct RobotVisualPartSpec
{
    const char* key;         ///< Stable identifier used to look up per-part UI/debug state (e.g. "j1", "tool").
    const char* displayName; ///< Human-readable name used in status/warning messages.
    const char* fileName;    ///< STL file name, resolved under the Nachi asset directory.
    const char* colorName;   ///< vtkNamedColors color name applied to the mesh actor.
    const char* linkId;      ///< FK link id (key into FkChain::linkPosesInBase) that drives this part's pose.
};

/// The 8 STL visual parts (base + joints 1-6 + tool) that make up the Nachi MZ04D example
/// scene, in the order loadRobotVisuals() adds them and the per-part checkbox arrays index them.
constexpr std::array<RobotVisualPartSpec, 8> kRobotParts = {{
    {"base", "Base", "MZ04-01_base.stl", "Silver", "base_link"},
    {"j1", "Joint 1", "MZ04-01_j1.stl", "SlateGray", "link_1"},
    {"j2", "Joint 2", "MZ04-01_j2.stl", "LightSteelBlue", "link_2"},
    {"j3", "Joint 3", "MZ04-01_j3.stl", "CadetBlue", "link_3"},
    {"j4", "Joint 4", "MZ04-01_j4.stl", "LightSkyBlue", "link_4"},
    {"j5", "Joint 5", "MZ04-01_j5.stl", "SteelBlue", "link_5"},
    {"j6", "Joint 6", "MZ04-01_j6.stl", "DodgerBlue", "flange"},
    {"tool", "Centering Tool Mesh", "Centering_tool.stl", "DarkOrange", "flange"},
}};

/// Joint angles (degrees) for the "Midpoint" sample-pose button: joint 2 raised 90 deg, all
/// other joints at zero.
constexpr std::array<double, 6> kMidPointDegrees = {
    0.0, 90.0, 0.0, 0.0, 0.0, 0.0,
};


/// Joint angles (degrees) for teach-pendant measurement point 1, from
/// docs/preset_references/nachi-mz04d.md.
constexpr std::array<double, 6> kTeachPoint1Degrees = {
    28.1579, -18.8069, 163.839, -0.710019, 35.8922, 152.731,
};

/// Joint angles (degrees) for teach-pendant measurement point 20, from
/// docs/preset_references/nachi-mz04d.md.
constexpr std::array<double, 6> kTeachPoint20Degrees = {
    -0.00219726, 0.00430813, 179.996, 0.00459559, 0.0046875, -0.000121055,
};

/// RGB color (0-1) applied to any actor whose link/geometry is currently reported as colliding.
constexpr std::array<double, 3> kCollisionHighlightColor = {
    0.92, 0.28, 0.18,
};

/// RGB color (0-1) for non-colliding primitive-backend sphere collision debug actors.
constexpr std::array<double, 3> kPrimitiveSphereColor = {
    0.35, 0.86, 0.72,
};

/// RGB color (0-1) for non-colliding primitive-backend capsule collision debug actors.
constexpr std::array<double, 3> kPrimitiveCapsuleColor = {
    0.98, 0.78, 0.34,
};

/// Locates the Nachi runtime asset directory (`presets/Nachi/MZ04`) by checking the application
/// directory, the current working directory, and up to 8 ancestor directories of the
/// application directory.
/// @return absolute path to the asset directory, or an empty string if none of the candidate
/// roots contain it.
QString findAssetsDirectory()
{
    const QString appDirPath = QCoreApplication::applicationDirPath();

    QStringList candidateRoots;
    candidateRoots << appDirPath << QDir::currentPath();

    QDir searchDir(appDirPath);
    for (int depth = 0; depth < 8; ++depth) {
        candidateRoots << searchDir.absolutePath();
        if (!searchDir.cdUp()) {
            break;
        }
    }

    for (const QString& rootPath : candidateRoots) {
        const QDir rootDir(rootPath);
        const QString repoAssets =
            rootDir.filePath(QStringLiteral("presets/Nachi/MZ04"));
        if (QDir(repoAssets).exists()) {
            return QDir(repoAssets).absolutePath();
        }
    }

    return QString();
}

/// Builds the de-duplicated list of candidate root directories (application directory, current
/// working directory, and up to 8 ancestor directories of the application directory) used by
/// findRepoRelativePath() to resolve preset-metadata-relative paths.
/// @return candidate root directories, most-specific first.
QStringList searchRootPaths()
{
    const QString appDirPath = QCoreApplication::applicationDirPath();

    QStringList candidateRoots;
    candidateRoots << appDirPath << QDir::currentPath();

    QDir searchDir(appDirPath);
    for (int depth = 0; depth < 8; ++depth) {
        candidateRoots << searchDir.absolutePath();
        if (!searchDir.cdUp()) {
            break;
        }
    }

    candidateRoots.removeDuplicates();
    return candidateRoots;
}

/// Resolves a path stored in preset metadata (e.g. a collision profile path) against the repo:
/// tries `relativePath` directly first, then joins it with each of searchRootPaths() until a
/// file exists.
/// @param relativePath path as recorded in preset metadata
/// @return absolute path to the first existing match, or an empty string if none exists.
QString findRepoRelativePath(const QString& relativePath)
{
    if (relativePath.isEmpty()) {
        return QString();
    }

    const QFileInfo directInfo(relativePath);
    if (directInfo.exists()) {
        return directInfo.absoluteFilePath();
    }

    for (const QString& rootPath : searchRootPaths()) {
        const QString candidate = QDir(rootPath).filePath(relativePath);
        if (QFileInfo(candidate).exists()) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }

    return QString();
}

/// Formats `value` as a fixed-point decimal string with the given number of decimal places.
QString formatNumber(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}

/// Copies a 4x4 Eigen transform into a newly allocated vtkMatrix4x4 with the same element values.
vtkSmartPointer<vtkMatrix4x4> toVtkMatrix(const Eigen::Matrix4d& values)
{
    vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            matrix->SetElement(row, column, values(row, column));
        }
    }
    return matrix;
}

/// Builds the 4x4 transform used to place a unit-sized VTK debug primitive (sphere/cylinder) at
/// a collision geometry's pose: combines `poseInBase`'s rotation with `sourceToGeometryRotation`
/// (to align the VTK source's default axis with the geometry's own axis) and the per-axis
/// `scaleMm`, and converts the pose's meter translation to millimeters (the scene's world unit).
/// @param poseInBase pose of the geometry origin in the robot base frame (meters)
/// @param sourceToGeometryRotation extra rotation applied before scaling
/// @param scaleMm per-axis scale, in millimeters, applied to the unit-sized VTK source
/// @return 4x4 transform suitable for vtkProp3D::SetUserMatrix
Eigen::Matrix4d scaledTransformMm(const Pose& poseInBase,
                                  const Eigen::Matrix3d& sourceToGeometryRotation,
                                  const Eigen::Vector3d& scaleMm)
{
    Eigen::Matrix4d matrix = Eigen::Matrix4d::Identity();
    matrix.block<3, 3>(0, 0) =
        poseInBase.isometry().linear() * sourceToGeometryRotation * scaleMm.asDiagonal();
    matrix.block<3, 1>(0, 3) = poseInBase.translation_m() * 1000.0;
    return matrix;
}

/// Builds a debug actor around an already-sized unit VTK polydata source (sphere/cylinder):
/// applies `color`/`opacity`, surface representation with dark edges, and leaves it hidden
/// (VisibilityOff) until applyDebugVisualState() shows it.
/// @param source unit-sized VTK source; caller has already set its radius/height/resolution
/// @return newly created actor, not yet added to the renderer
vtkSmartPointer<vtkActor> makePrimitiveActor(vtkPolyDataAlgorithm* source,
                                             const std::array<double, 3>& color,
                                             double opacity)
{
    vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(source->GetOutputPort());

    vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    actor->GetProperty()->SetOpacity(opacity);
    actor->GetProperty()->SetRepresentationToSurface();
    actor->GetProperty()->SetEdgeVisibility(1);
    actor->GetProperty()->SetEdgeColor(0.12, 0.16, 0.19);
    actor->GetProperty()->SetSpecular(0.12);
    actor->GetProperty()->SetSpecularPower(8.0);
    actor->VisibilityOff();
    return actor;
}

/// Computes, for each movable (revolute/prismatic) joint in `config` (in configured order), the
/// midpoint of its limit range, or its home value when the joint has no configured limits.
/// @return degrees for each movable joint, indexed 0..N-1 in configured joint order.
std::array<double, 6> midpointDegrees(const SerialRobotConfig& config)
{
    std::array<double, 6> midpoint{};
    int movableIndex = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type != JointType::Revolute && joint.type != JointType::Prismatic) {
            continue;
        }

        double value = 0.0;
        if (joint.limits.has_value()) {
            value = 0.5 * (joint.limits->lower + joint.limits->upper);
        } else {
            value = joint.home;
        }
        midpoint[static_cast<std::size_t>(movableIndex)] = units::toDeg(value);
        ++movableIndex;
    }
    return midpoint;
}

/// Computes, for each movable (revolute/prismatic) joint in `config`, its configured home value.
/// @return home-position degrees for each movable joint, indexed 0..N-1 in configured joint order.
std::array<double, 6> homeDegrees(const SerialRobotConfig& config)
{
    std::array<double, 6> home{};
    int movableIndex = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type != JointType::Revolute && joint.type != JointType::Prismatic) {
            continue;
        }

        home[static_cast<std::size_t>(movableIndex)] = units::toDeg(joint.home);
        ++movableIndex;
    }
    return home;
}

/// Looks up `linkId`'s pose within `chain`.
/// @return the link's pose in the base frame, or Pose::identity() if the chain has no entry for it.
Pose poseForLinkId(const FkChain& chain, const std::string& linkId)
{
    const auto it = chain.linkPosesInBase.find(linkId);
    return it == chain.linkPosesInBase.end() ? Pose::identity() : it->second;
}

/// Maps a visual-part key to its 0-based FK joint index.
/// @return joint index 0-5 for keys "j1".."j6", or -1 for parts with no associated joint
/// (e.g. "base", "tool").
int jointAxisIndexForPartKey(const QString& key)
{
    if (key == QStringLiteral("j1")) {
        return 0;
    } else if (key == QStringLiteral("j2")) {
        return 1;
    } else if (key == QStringLiteral("j3")) {
        return 2;
    } else if (key == QStringLiteral("j4")) {
        return 3;
    } else if (key == QStringLiteral("j5")) {
        return 4;
    } else if (key == QStringLiteral("j6")) {
        return 5;
    }
    return -1;
}

/// Returns the hardcoded visual-alignment correction pose for the STL part identified by `key`,
/// reconciling each mesh's CAD export frame with the canonical FK link frame; this is where
/// per-mesh placement tuning is edited (see body comment).
/// @param key part key ("base", "j1".."j6", "tool"); unrecognized keys return Pose::identity()
/// @return correction pose, applied after the FK home-relative delta in updateSceneFromChain()
Pose visualHomeCorrectionForPartKey(const QString& key)
{
    // Placement tuning lives here. When a mesh was exported in a CAD frame that does not
    // match the canonical link frame, edit the per-part correction in this function.
    // The returned pose is multiplied after the FK home-relative delta in updateSceneFromChain().
    if (key == QStringLiteral("base")) {
        return Pose::fromXYZRPY_mm_deg(0.0, 0.0, 0.0, 90.0, 0.0, 0.0);
    } else if (key == QStringLiteral("j1")) {
        return Pose::fromXYZRPY_mm_deg(0.0, 0.0, 340.0, 90.0, 0.0, 0.0);
    } else if (key == QStringLiteral("j2")) {
        return Pose::fromXYZRPY_mm_deg(0.0, 0.0, 340.0, 90.0, 0.0, 0.0);
    } else if (key == QStringLiteral("j3")) {
        return Pose::fromXYZRPY_mm_deg(260.0, 0.0, 340, 90.0, 0.0, 0.0);
    } else if (key == QStringLiteral("j4")) {
        return Pose::fromXYZRPY_mm_deg(285.0, 0.0, 259, 90.0, 0.0, 0.0);
    } else if (key == QStringLiteral("j5")) {
        return Pose::fromXYZRPY_mm_deg(285.0, 0.0, 60, 90.0, 0.0, 0.0);
    } else if (key == QStringLiteral("j6")) {
        return Pose::fromXYZRPY_mm_deg(285.0, 0.0, -12, 90.0, 0.0, 0.0);
    } else if (key == QStringLiteral("tool")) {
        return Pose::fromXYZRPY_mm_deg(285.0, 0.0, -12, 0.0, 0.0, 180.0);
    }

    return Pose::identity();
}

/// Builds the example robot configuration used by this demo: the Nachi MZ04D preset extended
/// with an example "centering_tool" TCP tool (45, 0, 112 mm offset from the flange), recorded
/// as the default example tool under metadata key "example_visual_tool".
/// @return the extended configuration used to construct MainWindow's kinematics helpers.
SerialRobotConfig buildExampleConfig()
{
    SerialRobotConfig config = Presets::nachiMZ04D();
    config.tools.push_back(Tool{
        "centering_tool",
        "Centering Tool (45, 0, 112 mm)",
        Pose::fromXYZRPY_mm_deg(45.0, 0.0, 112.0, 0.0, 0.0, 0.0),
    });
    config.metadata["example_visual_tool"] = "centering_tool";
    return config;
}
} // namespace

/// Constructs the main window: builds the example Nachi MZ04D configuration and kinematics
/// helpers (robot_, frameRegistry_, toolRegistry_, postureResolver_), wires up the VTK viewport
/// and UI controls, loads the robot/collision debug visuals, then poses the robot at its home
/// joint configuration and refreshes the scene/readouts.
/// @param parent optional parent widget
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , config_(buildExampleConfig())
    , robot_(config_)
    , frameRegistry_(FrameRegistry::fromConfig(config_))
    , toolRegistry_(ToolRegistry::fromConfig(config_))
    , postureResolver_(PostureResolverFactory::create(config_))
{
    ui->setupUi(this);

    setWindowTitle(QStringLiteral("RobotKinematics - Nachi MZ04D Pose Visualizer"));

    setupVtkViewport();
    setupModelState();
    setupUiState();
    connectSignals();
    loadRobotVisuals();
    loadCollisionDebugVisuals();
    loadMeshCollisionDebugVisuals();

    setJointDegrees(homeDegrees(config_));
    resetTargetToCurrentTcp();
    applyJointStateToSceneAndReadouts();
}

/// Destroys the generated UI (`ui`); the VTK smart pointers and loaded actors clean themselves up.
MainWindow::~MainWindow()
{
    delete ui;
}

/// Builds the VTK render pipeline: creates the QVTKOpenGLNativeWidget/render-window/renderer
/// hosted inside `ui->vtkContainerWidget`, sets a gradient background, adds a camera key light,
/// a wireframe ground plane, and an interactive orientation-marker axes widget.
void MainWindow::setupVtkViewport()
{
    auto* hostLayout = new QVBoxLayout(ui->vtkContainerWidget);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);

    vtkWidget_ = new QVTKOpenGLNativeWidget(ui->vtkContainerWidget);
    hostLayout->addWidget(vtkWidget_);

    renderWindow_ = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    renderer_ = vtkSmartPointer<vtkRenderer>::New();

    vtkWidget_->setRenderWindow(renderWindow_);
    renderWindow_->AddRenderer(renderer_);
    renderer_->SetBackground(0.08, 0.09, 0.11);
    renderer_->SetBackground2(0.18, 0.20, 0.24);
    renderer_->GradientBackgroundOn();

    vtkSmartPointer<vtkLight> keyLight = vtkSmartPointer<vtkLight>::New();
    keyLight->SetLightTypeToCameraLight();
    keyLight->SetPosition(0.3, 0.4, 1.0);
    keyLight->SetFocalPoint(0.0, 0.0, 0.0);
    keyLight->SetIntensity(1.1);
    renderer_->AddLight(keyLight);

    vtkSmartPointer<vtkPlaneSource> groundPlane = vtkSmartPointer<vtkPlaneSource>::New();
    groundPlane->SetOrigin(-500.0, -500.0, -5.0);
    groundPlane->SetPoint1(500.0, -500.0, -5.0);
    groundPlane->SetPoint2(-500.0, 500.0, -5.0);
    groundPlane->SetXResolution(10);
    groundPlane->SetYResolution(10);

    vtkSmartPointer<vtkPolyDataMapper> groundMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    groundMapper->SetInputConnection(groundPlane->GetOutputPort());

    vtkSmartPointer<vtkActor> groundActor = vtkSmartPointer<vtkActor>::New();
    groundActor->SetMapper(groundMapper);
    groundActor->GetProperty()->SetColor(0.32, 0.36, 0.40);
    groundActor->GetProperty()->SetRepresentationToWireframe();
    groundActor->GetProperty()->SetOpacity(0.55);
    renderer_->AddActor(groundActor);

    vtkSmartPointer<vtkAxesActor> axesActor = vtkSmartPointer<vtkAxesActor>::New();
    axesActor->SetTotalLength(120.0, 120.0, 120.0);
    orientationMarker_ = vtkSmartPointer<vtkOrientationMarkerWidget>::New();
    orientationMarker_->SetOrientationMarker(axesActor);
    orientationMarker_->SetViewport(0.0, 0.0, 0.2, 0.2);
    orientationMarker_->SetInteractor(renderWindow_->GetInteractor());
    orientationMarker_->SetEnabled(1);
    orientationMarker_->InteractiveOff();
}

/// Validates the posture resolver (warns the user via a message box if it could not be created,
/// disabling posture controls), then loads the primitive and mesh collision profiles and queries
/// mesh-backend availability.
void MainWindow::setupModelState()
{
    if (!postureResolver_) {
        QMessageBox::warning(this,
                             QStringLiteral("Posture metadata unavailable"),
                             QStringLiteral("The Nachi posture resolver could not be created. "
                                            "Posture controls will remain disabled."));
    }

    loadCollisionProfile();
    loadMeshCollisionProfiles();
    meshBackendInfo_ = CollisionBackends::meshInfo();
}

/// Configures static UI state after construction: splitter stretch factors, all populate*()
/// control-population passes, the IK-results/collision-pairs table header behavior, and the
/// initial status labels; finishes by refreshing action (button enabled) state.
void MainWindow::setupUiState()
{
    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);

    populateCombos();
    populateJointControls();
    populatePostureControls();
    populateSampleButtons();
    populateCollisionControls();
    populateBackendControls();
    populateDebugControls();

    ui->ikResultsTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->ikResultsTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->ikResultsTableWidget->verticalHeader()->setVisible(false);
    ui->collisionPairsTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->collisionPairsTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->collisionPairsTableWidget->verticalHeader()->setVisible(false);

    ui->sceneTitleLabel->setText(QStringLiteral("Simulation scene (visual world in millimeters unit)"));
    ui->jointStatusLabel->setText(QStringLiteral("Joint state not evaluated yet."));
    ui->ikStatusLabel->setText(QStringLiteral("IK status will appear here."));

    updateActionState();
}

/// Wires every interactive control (joint spin boxes, tool/reference-frame combos, sample-pose
/// buttons, IK buttons and results table, collision controls, backend combo, and the per-part
/// visibility/origin/axis checkboxes) to the scene/readout update logic.
void MainWindow::connectSignals()
{
    for (QDoubleSpinBox* spinBox : jointSpinBoxes()) {
        connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { applyJointStateToSceneAndReadouts(); });
    }

    connect(ui->toolComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { applyJointStateToSceneAndReadouts(); });
    connect(ui->referenceFrameComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) { applyJointStateToSceneAndReadouts(); });

    connect(ui->homeButton, &QPushButton::clicked, this,
            [this]() { setJointDegrees(homeDegrees(config_)); });
    connect(ui->midpointButton, &QPushButton::clicked, this,
            [this]() { setJointDegrees(kMidPointDegrees); });
    connect(ui->teachPoint1Button, &QPushButton::clicked, this,
            [this]() { setJointDegrees(kTeachPoint1Degrees); });
    connect(ui->teachPoint20Button, &QPushButton::clicked, this,
            [this]() { setJointDegrees(kTeachPoint20Degrees); });

    connect(ui->copyCurrentTcpToTargetButton, &QPushButton::clicked, this, &MainWindow::resetTargetToCurrentTcp);
    connect(ui->solveBestButton, &QPushButton::clicked, this,
            [this]() { solveInverseKinematics(false); });
    connect(ui->solveAllButton, &QPushButton::clicked, this,
            [this]() { solveInverseKinematics(true); });
    connect(ui->applySelectedSolutionButton, &QPushButton::clicked, this, &MainWindow::applySelectedIkSolution);
    connect(ui->ikResultsTableWidget, &QTableWidget::itemSelectionChanged, this, &MainWindow::updateActionState);
    connect(ui->collisionSafetyMarginSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double) { applyJointStateToSceneAndReadouts(); });
    connect(ui->showCollisionShapesCheckBox, &QCheckBox::toggled, this,
            [this](bool) { applyDebugVisualState(); });
    connect(ui->enableCollisionCheckBox, &QCheckBox::toggled, this,
            [this](bool) { applyJointStateToSceneAndReadouts(); });
    connect(ui->collisionBackendComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                const QVariant data = ui->collisionBackendComboBox->itemData(index);
                if (!data.isValid()) {
                    return;
                }
                backendSelection_ = static_cast<BackendSelection>(data.toInt());
                applyJointStateToSceneAndReadouts();
            });

    for (QCheckBox* checkBox : partVisibleCheckBoxes()) {
        connect(checkBox, &QCheckBox::toggled, this, [this](bool) { applyDebugVisualState(); });
    }
    for (QCheckBox* checkBox : partOriginCheckBoxes()) {
        connect(checkBox, &QCheckBox::toggled, this, [this](bool) { applyDebugVisualState(); });
    }
    for (QCheckBox* checkBox : partAxisCheckBoxes()) {
        connect(checkBox, &QCheckBox::toggled, this, [this](bool) { applyDebugVisualState(); });
    }
}

/// Fills the tool combo box from `config_.tools` (preferring the example "centering_tool", else
/// falling back to `config_.defaultToolId`) and the reference-frame combo box with "Base" plus
/// every configured user frame.
void MainWindow::populateCombos()
{
    ui->toolComboBox->clear();
    for (const Tool& tool : config_.tools) {
        ui->toolComboBox->addItem(QString::fromStdString(tool.name), QString::fromStdString(tool.id));
    }

    const int centeringToolIndex = ui->toolComboBox->findData(QStringLiteral("centering_tool"));
    if (centeringToolIndex >= 0) {
        ui->toolComboBox->setCurrentIndex(centeringToolIndex);
    } else {
        const int defaultIndex =
            ui->toolComboBox->findData(QString::fromStdString(config_.defaultToolId));
        ui->toolComboBox->setCurrentIndex(defaultIndex >= 0 ? defaultIndex : 0);
    }

    ui->referenceFrameComboBox->clear();
    ui->referenceFrameComboBox->addItem(QStringLiteral("Base"), QStringLiteral("base"));
    for (const UserFrame& frame : config_.frames.userFrames) {
        ui->referenceFrameComboBox->addItem(QString::fromStdString(frame.id),
                                            QString::fromStdString(frame.id));
    }
}

/// Sets the unit suffix (" deg") and valid range on each movable-joint spin box, taking the
/// range from the joint's configured limits or defaulting to ±360 deg when unlimited; also sets
/// the unit suffixes (mm/deg) on the IK target spin boxes.
void MainWindow::populateJointControls()
{
    int movableIndex = 0;
    for (const Joint& joint : config_.joints) {
        if (joint.type != JointType::Revolute && joint.type != JointType::Prismatic) {
            continue;
        }

        QDoubleSpinBox* spinBox = jointSpinBoxes().at(static_cast<std::size_t>(movableIndex));
        spinBox->setSuffix(QStringLiteral(" deg"));
        if (joint.limits.has_value()) {
            spinBox->setRange(units::toDeg(joint.limits->lower), units::toDeg(joint.limits->upper));
        } else {
            spinBox->setRange(-360.0, 360.0);
        }
        ++movableIndex;
    }

    ui->targetXSpinBox->setSuffix(QStringLiteral(" mm"));
    ui->targetYSpinBox->setSuffix(QStringLiteral(" mm"));
    ui->targetZSpinBox->setSuffix(QStringLiteral(" mm"));
    ui->targetRzSpinBox->setSuffix(QStringLiteral(" deg"));
    ui->targetRySpinBox->setSuffix(QStringLiteral(" deg"));
    ui->targetRxSpinBox->setSuffix(QStringLiteral(" deg"));
}

/// Fills the shoulder/elbow/wrist posture-request combo boxes with "Any" plus the configured
/// negative/positive branch labels for each axis (via the local addBranchItems() helper).
void MainWindow::populatePostureControls()
{
    /// Clears `comboBox` and repopulates it with "Any" plus the negative/positive branch labels
    /// configured for `axis` in `config_.posture.labels` (left empty if the axis is not configured).
    const auto addBranchItems = [&](QComboBox* comboBox, const char* axis) {
        comboBox->clear();
        comboBox->addItem(QStringLiteral("Any"), QString());

        const auto it = config_.posture.labels.find(axis);
        if (it == config_.posture.labels.end()) {
            return;
        }

        comboBox->addItem(QString::fromStdString(it->second.negative),
                          QString::fromStdString(it->second.negative));
        comboBox->addItem(QString::fromStdString(it->second.positive),
                          QString::fromStdString(it->second.positive));
    };

    addBranchItems(ui->shoulderRequestComboBox, "shoulder");
    addBranchItems(ui->elbowRequestComboBox, "elbow");
    addBranchItems(ui->wristRequestComboBox, "wrist");
}

/// Sets tooltips on the teach-point sample buttons pointing at their source measurement in
/// docs/preset_references/nachi-mz04d.md.
void MainWindow::populateSampleButtons()
{
    ui->teachPoint1Button->setToolTip(
        QStringLiteral("Teach-pendant measurement point 1 from docs/preset_references/nachi-mz04d.md"));
    ui->teachPoint20Button->setToolTip(
        QStringLiteral("Teach-pendant measurement point 20 from docs/preset_references/nachi-mz04d.md"));
}

/// Initializes the collision-control panel: safety-margin suffix, default checkbox states, and
/// status/note labels reflecting whether a valid primitive collision profile
/// (`collisionProfileAvailable_`) loaded.
void MainWindow::populateCollisionControls()
{
    ui->collisionSafetyMarginSpinBox->setSuffix(QStringLiteral(" mm"));
    ui->enableCollisionCheckBox->setChecked(true);
    ui->showCollisionShapesCheckBox->setChecked(false);
    ui->collisionProfileValueLabel->setText(
        collisionProfileAvailable_ ? collisionProfileSource_ : QStringLiteral("Unavailable"));
    ui->collisionProfileNoteLabel->setText(
        collisionProfileNote_.isEmpty()
            ? QStringLiteral("Collision truth comes from the selected RobotKinematics backend (primitive or mesh). VTK is only used for rendering and never as the collision oracle.")
            : collisionProfileNote_);
    ui->collisionStatusLabel->setText(
        collisionProfileAvailable_
            ? QStringLiteral("Collision status will update with the current joint state.")
            : QStringLiteral("Collision checking is unavailable until a valid profile loads."));
}

/// Rebuilds the wireframe debug actors for the mesh collision profiles: removes any previously
/// added actors from `meshOriginalDebugStates_`/`meshSimplifiedDebugStates_`, then for each
/// enabled mesh in a valid `meshOriginalProfile_`/`meshSimplifiedProfile_` loads its STL and adds
/// a hidden wireframe actor (shown later by applyDebugVisualState()).
void MainWindow::loadMeshCollisionDebugVisuals()
{
    /// Removes each state's actor from the renderer, then clears `states`.
    const auto removeActors = [this](std::vector<MeshDebugState>& states) {
        for (MeshDebugState& state : states) {
            if (state.actor) {
                renderer_->RemoveActor(state.actor);
            }
        }
        states.clear();
    };
    removeActors(meshOriginalDebugStates_);
    removeActors(meshSimplifiedDebugStates_);

    /// For each enabled mesh in `profileState` (skipped entirely if the profile is invalid),
    /// loads the STL from disk, adds a hidden wireframe actor in `color` to the renderer, and
    /// appends its MeshDebugState (link id, mesh-to-link offset, mm scale) to `output`.
    const auto loadStatesFor = [this](const MeshProfileState& profileState,
                                      std::vector<MeshDebugState>& output,
                                      const std::array<double, 3>& color) {
        if (!profileState.valid) {
            return;
        }
        for (const MeshCollisionGeometry& mesh : profileState.profile.meshes) {
            if (!mesh.enabled) {
                continue;
            }
            const QFileInfo info(QString::fromStdString(mesh.path));
            if (!info.exists()) {
                continue;
            }

            vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
            reader->SetFileName(info.absoluteFilePath().toLocal8Bit().constData());
            reader->Update();
            if (!reader->GetOutput() || reader->GetOutput()->GetNumberOfPoints() == 0) {
                continue;
            }

            vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            mapper->SetInputConnection(reader->GetOutputPort());

            vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            actor->GetProperty()->SetRepresentationToWireframe();
            actor->GetProperty()->SetColor(color[0], color[1], color[2]);
            actor->GetProperty()->SetLineWidth(1.0);
            actor->VisibilityOff();
            renderer_->AddActor(actor);

            MeshDebugState state;
            state.meshId = mesh.id;
            state.linkId = mesh.linkId;
            state.meshToLink = mesh.meshToLink;
            // vtkSTLReader returns coordinates as authored in the file. scaleToMeters*1000 maps
            // those authored values into the mm-based VTK world used by visualParts_.
            state.stlScaleToMm = mesh.scaleToMeters * 1000.0;
            state.baseColorRgb = color;
            state.actor = actor;
            output.push_back(state);
        }
    };

    constexpr std::array<double, 3> kMeshOriginalColor = {0.20, 0.85, 0.85};
    constexpr std::array<double, 3> kMeshSimplifiedColor = {0.95, 0.55, 0.15};
    loadStatesFor(meshOriginalProfile_, meshOriginalDebugStates_, kMeshOriginalColor);
    loadStatesFor(meshSimplifiedProfile_, meshSimplifiedDebugStates_, kMeshSimplifiedColor);
}

/// Rebuilds the primitive-backend collision debug actors (`primitiveDebugStates_`) from
/// `collisionProfile_`: for each enabled geometry, creates a sphere actor or a
/// cylinder+two-endcap-spheres actor set (unit-sized, scaled/positioned later per joint state by
/// updateCollisionDebugVisuals()), added hidden to the renderer.
void MainWindow::loadCollisionDebugVisuals()
{
    primitiveDebugStates_.clear();
    if (!collisionProfileAvailable_) {
        return;
    }

    for (const CollisionGeometry& geometry : collisionProfile_.geometries) {
        if (!geometry.enabled) {
            continue;
        }

        PrimitiveDebugState state;
        state.geometryId = geometry.id;
        state.linkId = geometry.linkId;
        state.shapeType = geometry.shape.type;
        state.geometryToLink = geometry.geometryToLink;

        if (geometry.shape.type == CollisionShapeType::Sphere) {
            vtkSmartPointer<vtkSphereSource> sphere = vtkSmartPointer<vtkSphereSource>::New();
            sphere->SetRadius(1.0);
            sphere->SetPhiResolution(24);
            sphere->SetThetaResolution(24);
            state.radius_m = geometry.shape.sphere.radius_m;
            state.baseColorRgb = kPrimitiveSphereColor;
            state.bodyActor = makePrimitiveActor(sphere, state.baseColorRgb, 0.28);
            renderer_->AddActor(state.bodyActor);
        } else {
            vtkSmartPointer<vtkCylinderSource> cylinder = vtkSmartPointer<vtkCylinderSource>::New();
            cylinder->SetRadius(1.0);
            cylinder->SetHeight(1.0);
            cylinder->SetResolution(28);
            cylinder->CappingOn();

            vtkSmartPointer<vtkSphereSource> capStart = vtkSmartPointer<vtkSphereSource>::New();
            capStart->SetRadius(1.0);
            capStart->SetPhiResolution(20);
            capStart->SetThetaResolution(20);

            vtkSmartPointer<vtkSphereSource> capEnd = vtkSmartPointer<vtkSphereSource>::New();
            capEnd->SetRadius(1.0);
            capEnd->SetPhiResolution(20);
            capEnd->SetThetaResolution(20);

            state.radius_m = geometry.shape.capsule.radius_m;
            state.length_m = geometry.shape.capsule.length_m;
            state.baseColorRgb = kPrimitiveCapsuleColor;
            state.bodyActor = makePrimitiveActor(cylinder, state.baseColorRgb, 0.24);
            state.capStartActor = makePrimitiveActor(capStart, state.baseColorRgb, 0.24);
            state.capEndActor = makePrimitiveActor(capEnd, state.baseColorRgb, 0.24);
            renderer_->AddActor(state.bodyActor);
            renderer_->AddActor(state.capStartActor);
            renderer_->AddActor(state.capEndActor);
        }

        primitiveDebugStates_.push_back(state);
    }
}

/// Sets the default debug-checkbox state (all parts visible, origins/axes hidden), disables the
/// base/tool axis checkboxes (those parts have no associated FK joint axis), and sets the debug
/// hint label text describing where placement parameters live in this file.
void MainWindow::populateDebugControls()
{
    const std::array<QCheckBox*, 8> visible = partVisibleCheckBoxes();
    const std::array<QCheckBox*, 8> origins = partOriginCheckBoxes();
    const std::array<QCheckBox*, 8> axes = partAxisCheckBoxes();

    for (QCheckBox* checkBox : visible) {
        checkBox->setChecked(true);
    }
    for (QCheckBox* checkBox : origins) {
        checkBox->setChecked(false);
    }
    for (QCheckBox* checkBox : axes) {
        checkBox->setChecked(false);
    }

    ui->baseAxisCheckBox->setEnabled(false);
    ui->toolAxisCheckBox->setEnabled(false);

    ui->debugHintLabel->setText(
        QStringLiteral("Use these toggles to inspect where each STL local origin ended up in the "
                       "scene. Placement parameters live in mainwindow.cpp: "
                       "`visualHomeCorrectionForPartKey()` for per-mesh corrections and "
                       "`buildExampleConfig()` for the tool TCP offset. The Axis column shows "
                       "the actual FK joint axis from `chain.joints`."));
}

/// Loads the 8 STL meshes listed in `kRobotParts` from the resolved asset directory, builds a
/// shaded actor plus an origin-axes actor and a joint-axis line actor for each, computes each
/// part's home-pose FK link transform and visual correction, and appends a VisualPartState to
/// `visualParts_`. Resets the camera and shows a status-bar/warning message summarizing any
/// missing or unreadable meshes.
void MainWindow::loadRobotVisuals()
{
    assetsDirectory_ = findAssetsDirectory();
    QStringList loadErrors;

    if (assetsDirectory_.isEmpty()) {
        loadErrors << QStringLiteral(
            "Could not find the Nachi runtime asset directory. Build from the repository "
            "root or keep the STL assets under `presets/Nachi/MZ04`.");
    } else {
        const JointVector homeJoints = JointVector::fromDegrees(
            {homeDegrees(config_)[0], homeDegrees(config_)[1], homeDegrees(config_)[2],
             homeDegrees(config_)[3], homeDegrees(config_)[4], homeDegrees(config_)[5]});
        const FkChain homeChain = ForwardKinematics::computeChain(config_, homeJoints);
        vtkSmartPointer<vtkNamedColors> colors = vtkSmartPointer<vtkNamedColors>::New();

        for (const RobotVisualPartSpec& spec : kRobotParts) {
            const QString meshPath = QDir(assetsDirectory_).filePath(QString::fromLatin1(spec.fileName));
            QFileInfo meshInfo(meshPath);
            if (!meshInfo.exists() || !meshInfo.isFile()) {
                loadErrors << QStringLiteral("%1 mesh is missing: %2")
                                  .arg(QString::fromLatin1(spec.displayName), meshPath);
                continue;
            }

            vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
            reader->SetFileName(meshInfo.absoluteFilePath().toLocal8Bit().constData());
            reader->Update();

            if (!reader->GetOutput() || reader->GetOutput()->GetNumberOfPoints() == 0) {
                loadErrors << QStringLiteral("%1 mesh is unreadable or empty: %2")
                                  .arg(QString::fromLatin1(spec.displayName), meshPath);
                continue;
            }

            vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            mapper->SetInputConnection(reader->GetOutputPort());

            vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            const auto baseColor = colors->GetColor3d(spec.colorName);
            actor->GetProperty()->SetColor(baseColor[0], baseColor[1], baseColor[2]);
            actor->GetProperty()->SetInterpolationToPhong();
            actor->GetProperty()->SetSpecular(0.18);
            actor->GetProperty()->SetSpecularPower(18.0);
            renderer_->AddActor(actor);

            vtkSmartPointer<vtkAxesActor> originActor = vtkSmartPointer<vtkAxesActor>::New();
            originActor->SetTotalLength(60.0, 60.0, 60.0);
            originActor->SetShaftTypeToLine();
            originActor->AxisLabelsOff();
            renderer_->AddActor(originActor);

            vtkSmartPointer<vtkLineSource> axisSource = vtkSmartPointer<vtkLineSource>::New();
            axisSource->SetPoint1(0.0, 0.0, 0.0);
            axisSource->SetPoint2(0.0, 0.0, 100.0);

            vtkSmartPointer<vtkPolyDataMapper> axisMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
            axisMapper->SetInputConnection(axisSource->GetOutputPort());

            vtkSmartPointer<vtkActor> axisActor = vtkSmartPointer<vtkActor>::New();
            axisActor->SetMapper(axisMapper);
            axisActor->GetProperty()->SetColor(1.0, 0.85, 0.1);
            axisActor->GetProperty()->SetLineWidth(3.0);
            renderer_->AddActor(axisActor);

            VisualPartState state;
            state.key = QString::fromLatin1(spec.key);
            state.displayName = QString::fromLatin1(spec.displayName);
            state.linkId = spec.linkId;
            state.homeLinkInBase = poseForLinkId(homeChain, spec.linkId);
            state.homeVisualCorrection = visualHomeCorrectionForPartKey(state.key);
            state.baseColorRgb = {baseColor[0], baseColor[1], baseColor[2]};
            state.jointAxisIndex = jointAxisIndexForPartKey(state.key);
            state.isLoaded = true;
            state.actor = actor;
            state.originActor = originActor;
            state.axisSource = axisSource;
            state.axisActor = axisActor;
            visualParts_.push_back(state);
        }
    }

    applyDebugVisualState();

    renderer_->ResetCamera();
    renderWindow_->Render();

    if (loadErrors.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("Loaded %1 robot meshes from %2")
                .arg(visualParts_.size())
                .arg(QDir::toNativeSeparators(assetsDirectory_)));
    } else {
        const QString errorText = loadErrors.join(QStringLiteral("\n"));
        statusBar()->showMessage(QStringLiteral("Asset loading completed with warnings"));
        QMessageBox::warning(
            this,
            QStringLiteral("Robot assets missing"),
            QStringLiteral("Some STL assets could not be loaded:\n\n%1").arg(errorText));
    }
}

/// Recomputes the forward-kinematics chain from the current joint spin-box values and refreshes
/// everything that depends on it: the 3D scene transforms, pose readouts, joint-limit status,
/// current posture classification, and collision state; re-renders the VTK viewport at the end.
void MainWindow::applyJointStateToSceneAndReadouts()
{
    const JointVector joints = currentJointVector();
    const FkChain chain = ForwardKinematics::computeChain(config_, joints);

    updateSceneFromChain(chain);
    updatePoseReadouts(chain);
    updateJointStatus(joints);
    updateCurrentPosture(joints);
    updateCollisionState(joints);
    updateActionState();

    renderWindow_->Render();
}

/// Applies `chain`'s link poses to every loaded visual part's VTK actor (mesh, origin axes, and
/// joint-axis line, the latter recomputed from the FK joint's actual origin/axis in `chain`),
/// then refreshes the collision/mesh debug overlays and visibility state.
/// @param chain forward-kinematics result for the current joint state
void MainWindow::updateSceneFromChain(const FkChain& chain)
{
    for (VisualPartState& part : visualParts_) {
        if (!part.actor) {
            continue;
        }

        // This is the final place where each mesh's VTK transform is set.
        // If a specific exported STL is offset or rotated incorrectly, adjust
        // its parameters in visualHomeCorrectionForPartKey() above.
        const Pose currentLinkPose = poseForLinkId(chain, part.linkId);
        const Eigen::Matrix4d matrixValues =
            Robot3DVisualizer::visualDeltaMatrixMm(
                currentLinkPose, part.homeLinkInBase, part.homeVisualCorrection);

        vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                matrix->SetElement(row, column, matrixValues(row, column));
            }
        }
        part.actor->SetUserMatrix(matrix);
        if (part.originActor) {
            part.originActor->SetUserMatrix(matrix);
        }
        if (part.axisActor && part.axisSource && part.jointAxisIndex >= 0 &&
            part.jointAxisIndex < static_cast<int>(chain.joints.size())) {
            const JointFrameData& jointData = chain.joints[static_cast<std::size_t>(part.jointAxisIndex)];
            const Eigen::Vector3d originMm = jointData.originInBase * 1000.0;
            const Eigen::Vector3d axisMm = jointData.axisInBase.normalized() * 120.0;
            const Eigen::Vector3d point1 = originMm - axisMm;
            const Eigen::Vector3d point2 = originMm + axisMm;
            part.axisSource->SetPoint1(point1.x(), point1.y(), point1.z());
            part.axisSource->SetPoint2(point2.x(), point2.y(), point2.z());
            part.axisSource->Update();
        }
    }

    updateCollisionDebugVisuals(chain);
    updateMeshDebugVisuals(chain);

    applyDebugVisualState();
}

/// Repositions each primitive-backend debug actor (`primitiveDebugStates_`) for the current
/// joint state: spheres get a uniform mm scale, capsules get a scaled/rotated cylinder body plus
/// two endcap spheres offset by ±half the capsule length along its axis.
/// @param chain forward-kinematics result providing each geometry's link pose
void MainWindow::updateCollisionDebugVisuals(const FkChain& chain)
{
    if (!collisionProfileAvailable_) {
        return;
    }

    const Eigen::Matrix3d cylinderSourceToGeometry =
        Eigen::AngleAxisd(0.5 * 3.14159265358979323846, Eigen::Vector3d::UnitX()).toRotationMatrix();

    for (PrimitiveDebugState& state : primitiveDebugStates_) {
        const auto linkIt = chain.linkPosesInBase.find(state.linkId);
        if (linkIt == chain.linkPosesInBase.end()) {
            continue;
        }

        const Pose geometryInBase = linkIt->second * state.geometryToLink;
        if (state.shapeType == CollisionShapeType::Sphere) {
            const Eigen::Vector3d sphereScaleMm = Eigen::Vector3d::Constant(state.radius_m * 1000.0);
            if (state.bodyActor) {
                state.bodyActor->SetUserMatrix(
                    toVtkMatrix(scaledTransformMm(geometryInBase, Eigen::Matrix3d::Identity(), sphereScaleMm)));
            }
            continue;
        }

        const Eigen::Vector3d capsuleScaleMm(
            state.radius_m * 1000.0,
            state.length_m * 1000.0,
            state.radius_m * 1000.0);
        if (state.bodyActor) {
            state.bodyActor->SetUserMatrix(
                toVtkMatrix(scaledTransformMm(geometryInBase, cylinderSourceToGeometry, capsuleScaleMm)));
        }

        const double halfLength = 0.5 * state.length_m;
        const Eigen::Vector3d sphereScaleMm = Eigen::Vector3d::Constant(state.radius_m * 1000.0);
        const Pose startPose = geometryInBase * Pose::fromXYZRPY_m_rad(0.0, 0.0, -halfLength, 0.0, 0.0, 0.0);
        const Pose endPose = geometryInBase * Pose::fromXYZRPY_m_rad(0.0, 0.0, halfLength, 0.0, 0.0, 0.0);
        if (state.capStartActor) {
            state.capStartActor->SetUserMatrix(
                toVtkMatrix(scaledTransformMm(startPose, Eigen::Matrix3d::Identity(), sphereScaleMm)));
        }
        if (state.capEndActor) {
            state.capEndActor->SetUserMatrix(
                toVtkMatrix(scaledTransformMm(endPose, Eigen::Matrix3d::Identity(), sphereScaleMm)));
        }
    }
}

/// Repositions every mesh-collision debug actor (in both `meshOriginalDebugStates_` and
/// `meshSimplifiedDebugStates_`) for the current joint state, scaling each by its own
/// `stlScaleToMm`.
/// @param chain forward-kinematics result providing each mesh's link pose
void MainWindow::updateMeshDebugVisuals(const FkChain& chain)
{
    /// Repositions each mesh actor in `states` whose link id is present in `chain`; entries
    /// without an actor or a matching link are left untouched.
    const auto updateGroup = [&chain](std::vector<MeshDebugState>& states) {
        for (MeshDebugState& state : states) {
            if (!state.actor) {
                continue;
            }
            const auto linkIt = chain.linkPosesInBase.find(state.linkId);
            if (linkIt == chain.linkPosesInBase.end()) {
                continue;
            }
            const Pose meshInBase = linkIt->second * state.meshToLink;
            const Eigen::Vector3d scaleMm = Eigen::Vector3d::Constant(state.stlScaleToMm);
            state.actor->SetUserMatrix(
                toVtkMatrix(scaledTransformMm(meshInBase, Eigen::Matrix3d::Identity(), scaleMm)));
        }
    };
    updateGroup(meshOriginalDebugStates_);
    updateGroup(meshSimplifiedDebugStates_);
}

/// Applies all per-part/per-geometry debug visibility and highlight-color state to the scene:
/// mesh/origin/axis actor visibility from the corresponding checkboxes (highlighting parts whose
/// link is in `collidingLinkIds_`), and primitive/mesh collision-shape actor visibility gated by
/// the "show collision shapes" checkbox and the currently selected `backendSelection_`
/// (highlighting geometries in `collidingGeometryIds_`); re-renders the viewport at the end.
void MainWindow::applyDebugVisualState()
{
    const std::array<QCheckBox*, 8> visible = partVisibleCheckBoxes();
    const std::array<QCheckBox*, 8> origins = partOriginCheckBoxes();
    const std::array<QCheckBox*, 8> axes = partAxisCheckBoxes();

    for (std::size_t index = 0; index < visualParts_.size() && index < visible.size(); ++index) {
        VisualPartState& part = visualParts_[index];
        if (part.actor) {
            const bool colliding = collidingLinkIds_.find(part.linkId) != collidingLinkIds_.end();
            const std::array<double, 3>& color = colliding ? kCollisionHighlightColor : part.baseColorRgb;
            part.actor->GetProperty()->SetColor(color[0], color[1], color[2]);
            part.actor->SetVisibility(part.isLoaded && visible[index]->isChecked());
        }
        if (part.originActor) {
            part.originActor->SetVisibility(part.isLoaded && origins[index]->isChecked());
        }
        if (part.axisActor) {
            const bool hasAxis = part.jointAxisIndex >= 0;
            part.axisActor->SetVisibility(part.isLoaded && hasAxis && axes[index]->isChecked());
        }
    }

    const bool showShapes = ui->showCollisionShapesCheckBox->isChecked();
    const bool showPrimitiveShapes =
        showShapes && backendSelection_ == BackendSelection::Primitive && collisionProfileAvailable_;
    const bool showMeshOriginalShapes =
        showShapes && backendSelection_ == BackendSelection::MeshOriginal &&
        meshOriginalProfile_.valid;
    const bool showMeshSimplifiedShapes =
        showShapes && backendSelection_ == BackendSelection::MeshSimplified &&
        meshSimplifiedProfile_.valid;

    for (PrimitiveDebugState& primitive : primitiveDebugStates_) {
        const bool colliding =
            collidingGeometryIds_.find(primitive.geometryId) != collidingGeometryIds_.end();
        const std::array<double, 3>& color = colliding ? kCollisionHighlightColor : primitive.baseColorRgb;

        if (primitive.bodyActor) {
            primitive.bodyActor->GetProperty()->SetColor(color[0], color[1], color[2]);
            primitive.bodyActor->SetVisibility(showPrimitiveShapes ? 1 : 0);
        }
        if (primitive.capStartActor) {
            primitive.capStartActor->GetProperty()->SetColor(color[0], color[1], color[2]);
            primitive.capStartActor->SetVisibility(showPrimitiveShapes ? 1 : 0);
        }
        if (primitive.capEndActor) {
            primitive.capEndActor->GetProperty()->SetColor(color[0], color[1], color[2]);
            primitive.capEndActor->SetVisibility(showPrimitiveShapes ? 1 : 0);
        }
    }

    /// Sets each mesh actor in `states` visible/hidden per `visible`, coloring colliding meshes
    /// (per `collidingGeometryIds_`) with the highlight color and others with their base color.
    const auto applyMeshGroup = [&](std::vector<MeshDebugState>& states, bool visible) {
        for (MeshDebugState& state : states) {
            if (!state.actor) {
                continue;
            }
            const bool colliding =
                collidingGeometryIds_.find(state.meshId) != collidingGeometryIds_.end();
            const std::array<double, 3>& color =
                colliding ? kCollisionHighlightColor : state.baseColorRgb;
            state.actor->GetProperty()->SetColor(color[0], color[1], color[2]);
            state.actor->SetVisibility(visible ? 1 : 0);
        }
    };
    applyMeshGroup(meshOriginalDebugStates_, showMeshOriginalShapes);
    applyMeshGroup(meshSimplifiedDebugStates_, showMeshSimplifiedShapes);

    if (renderWindow_) {
        renderWindow_->Render();
    }
}

/// Recomputes and displays the flange and TCP pose readouts (Nachi pendant XYZ/RzRyRx
/// convention) in the currently selected reference frame; on failure to resolve the selected
/// tool or reference frame, sets the affected fields to "n/a" and reports the error via
/// updateIkStatus() instead.
/// @param chain forward-kinematics result for the current joint state
void MainWindow::updatePoseReadouts(const FkChain& chain)
{
    const Result<Tool> tool = selectedTool();
    const Result<Pose> referenceInBase = selectedReferenceInBase(chain);

    if (!tool.ok()) {
        ui->flangeXLineEdit->setText(QStringLiteral("n/a"));
        ui->tcpXLineEdit->setText(QStringLiteral("n/a"));
        updateIkStatus(QStringLiteral("%1: %2").arg(Robot3DVisualizer::statusText(tool.status), QString::fromStdString(tool.message)));
        return;
    }
    if (!referenceInBase.ok()) {
        updateIkStatus(QStringLiteral("%1: %2")
                           .arg(Robot3DVisualizer::statusText(referenceInBase.status),
                                QString::fromStdString(referenceInBase.message)));
        return;
    }

    const JointVector joints = currentJointVector();
    const Pose flangeInBase = chain.flangeInBase;
    const Pose tcpInBase = ForwardKinematics::toolPose(config_, joints, tool.value.flangeToTcp);
    const Pose flangeInReference = referenceInBase.value.inverse() * flangeInBase;
    const Pose tcpInReference = referenceInBase.value.inverse() * tcpInBase;

    const Robot3DVisualizer::PendantPoseDisplay flangeDisplay =
        Robot3DVisualizer::toNachiPendantPose(flangeInReference);
    const Robot3DVisualizer::PendantPoseDisplay tcpDisplay =
        Robot3DVisualizer::toNachiPendantPose(tcpInReference);

    ui->flangeXLineEdit->setText(formatNumber(flangeDisplay.x_mm, 3));
    ui->flangeYLineEdit->setText(formatNumber(flangeDisplay.y_mm, 3));
    ui->flangeZLineEdit->setText(formatNumber(flangeDisplay.z_mm, 3));
    ui->flangeRzLineEdit->setText(formatNumber(flangeDisplay.rz_deg, 4));
    ui->flangeRyLineEdit->setText(formatNumber(flangeDisplay.ry_deg, 4));
    ui->flangeRxLineEdit->setText(formatNumber(flangeDisplay.rx_deg, 4));

    ui->tcpXLineEdit->setText(formatNumber(tcpDisplay.x_mm, 3));
    ui->tcpYLineEdit->setText(formatNumber(tcpDisplay.y_mm, 3));
    ui->tcpZLineEdit->setText(formatNumber(tcpDisplay.z_mm, 3));
    ui->tcpRzLineEdit->setText(formatNumber(tcpDisplay.rz_deg, 4));
    ui->tcpRyLineEdit->setText(formatNumber(tcpDisplay.ry_deg, 4));
    ui->tcpRxLineEdit->setText(formatNumber(tcpDisplay.rx_deg, 4));
}

/// Validates `joints` against the configured joint limits and updates `ui->jointStatusLabel`
/// with either a within-limits confirmation or the first reported limit violation (joint id,
/// value, and allowed range in degrees).
void MainWindow::updateJointStatus(const JointVector& joints)
{
    const JointLimitCheck check = JointLimitValidator::validate(config_, joints);
    if (check.ok()) {
        ui->jointStatusLabel->setText(QStringLiteral("Within configured joint limits."));
        return;
    }

    if (!check.violations.empty()) {
        const JointLimitViolation& violation = check.violations.front();
        ui->jointStatusLabel->setText(
            QStringLiteral("%1: %2 = %3 deg is outside [%4, %5] deg")
                .arg(Robot3DVisualizer::statusText(check.status))
                .arg(QString::fromStdString(violation.jointId))
                .arg(formatNumber(units::toDeg(violation.value), 4))
                .arg(formatNumber(units::toDeg(violation.lower), 4))
                .arg(formatNumber(units::toDeg(violation.upper), 4)));
        return;
    }

    ui->jointStatusLabel->setText(Robot3DVisualizer::statusText(check.status));
}

/// Classifies the current arm posture (shoulder/elbow/wrist) for `joints` via `postureResolver_`
/// and updates the corresponding value labels, showing "Unavailable" if no resolver was created
/// or classification fails.
void MainWindow::updateCurrentPosture(const JointVector& joints)
{
    if (!postureResolver_) {
        ui->currentShoulderValueLabel->setText(QStringLiteral("Unavailable"));
        ui->currentElbowValueLabel->setText(QStringLiteral("Unavailable"));
        ui->currentWristValueLabel->setText(QStringLiteral("Unavailable"));
        return;
    }

    const Result<ArmPosture> posture = postureResolver_->classify(config_, joints);
    if (!posture.ok()) {
        ui->currentShoulderValueLabel->setText(QStringLiteral("Unavailable"));
        ui->currentElbowValueLabel->setText(QStringLiteral("Unavailable"));
        ui->currentWristValueLabel->setText(QStringLiteral("Unavailable"));
        return;
    }

    ui->currentShoulderValueLabel->setText(
        Robot3DVisualizer::postureLabel(config_.posture, "shoulder", posture.value.shoulder));
    ui->currentElbowValueLabel->setText(
        Robot3DVisualizer::postureLabel(config_.posture, "elbow", posture.value.elbow));
    ui->currentWristValueLabel->setText(
        Robot3DVisualizer::postureLabel(config_.posture, "wrist", posture.value.wrist));
}

/// Runs a self-collision check for `joints` against the currently selected backend
/// (`backendSelection_`: primitive, mesh-original, or mesh-simplified), populates
/// `collidingGeometryIds_`/`collidingLinkIds_`/`lastCollisionPairs_` and the collision-pairs
/// table, and updates `ui->collisionStatusLabel` with the outcome (or with why checking is
/// unavailable/disabled). No-ops the check (but still clears prior state) when the "enable
/// collision check" checkbox is unchecked.
void MainWindow::updateCollisionState(const JointVector& joints)
{
    collidingGeometryIds_.clear();
    collidingLinkIds_.clear();
    lastCollisionPairs_.clear();

    if (!ui->enableCollisionCheckBox->isChecked()) {
        ui->collisionStatusLabel->setText(
            QStringLiteral("Collision check disabled by 'Enable collision check' toggle."));
        ui->collisionPairsTableWidget->setRowCount(0);
        applyDebugVisualState();
        return;
    }

    CollisionCheckResult result;
    QString backendSourceLabel;
    bool backendReady = false;

    switch (backendSelection_) {
    case BackendSelection::Primitive: {
        backendSourceLabel = collisionProfileSource_;
        if (collisionProfileAvailable_) {
            CollisionCheckRequest request;
            request.joints = joints;
            request.safetyMargin_m = units::mm(ui->collisionSafetyMarginSpinBox->value());
            request.returnAllPairs = true;
            result = CollisionBackends::checkPrimitive(config_, collisionProfile_, request);
            backendReady = true;
        } else {
            result.status = KinematicsStatus::InvalidRequest;
            result.message = collisionProfileNote_.isEmpty()
                                 ? std::string("no valid primitive profile loaded")
                                 : collisionProfileNote_.toStdString();
        }
        break;
    }
    case BackendSelection::MeshOriginal:
    case BackendSelection::MeshSimplified: {
        const MeshProfileState& profileState =
            (backendSelection_ == BackendSelection::MeshOriginal) ? meshOriginalProfile_
                                                                  : meshSimplifiedProfile_;
        backendSourceLabel = profileState.source;
        if (!meshBackendInfo_.available) {
            result.status = KinematicsStatus::UnsupportedSolver;
            result.message =
                "mesh collision backend is not compiled into this build: " +
                meshBackendInfo_.detail;
        } else if (!profileState.valid) {
            result.status = KinematicsStatus::InvalidRequest;
            result.message = profileState.note.isEmpty()
                                 ? std::string("mesh profile not loaded")
                                 : profileState.note.toStdString();
        } else {
            MeshCollisionCheckRequest request;
            request.joints = joints;
            request.safetyMargin_m = units::mm(ui->collisionSafetyMarginSpinBox->value());
            request.returnAllPairs = true;
            result = CollisionBackends::checkMesh(config_, profileState.profile, request);
            backendReady = true;
        }
        break;
    }
    }

    populateCollisionPairs(result);

    if (!backendReady || !result.ok()) {
        ui->collisionStatusLabel->setText(
            QStringLiteral("Collision checking unavailable (%1): %2")
                .arg(Robot3DVisualizer::statusText(result.status),
                     QString::fromStdString(result.message)));
        applyDebugVisualState();
        return;
    }

    lastCollisionPairs_ = result.pairs;
    int collidingPairCount = 0;
    for (const CollisionPairResult& pair : result.pairs) {
        if (!pair.colliding) {
            continue;
        }

        ++collidingPairCount;
        collidingGeometryIds_.insert(pair.geometryA);
        collidingGeometryIds_.insert(pair.geometryB);
        collidingLinkIds_.insert(pair.linkA);
        collidingLinkIds_.insert(pair.linkB);
    }

    /// Short label for the currently selected collision backend, used in the status message suffix.
    const QString backendLabel = [this]() -> QString {
        switch (backendSelection_) {
        case BackendSelection::Primitive:
            return QStringLiteral("primitive");
        case BackendSelection::MeshOriginal:
            return QStringLiteral("mesh-original");
        case BackendSelection::MeshSimplified:
            return QStringLiteral("mesh-simplified");
        }
        return QStringLiteral("unknown");
    }();
    const QString sourceSuffix = backendSourceLabel.isEmpty()
                                     ? QStringLiteral(" [backend=%1]").arg(backendLabel)
                                     : QStringLiteral(" [backend=%1; %2]")
                                           .arg(backendLabel, backendSourceLabel);
    if (result.hasCollision) {
        ui->collisionStatusLabel->setText(
            QStringLiteral("Self-collision detected in %1 pair(s) with safety margin %2 mm.%3")
                .arg(collidingPairCount)
                .arg(formatNumber(ui->collisionSafetyMarginSpinBox->value(), 3))
                .arg(sourceSuffix));
        applyDebugVisualState();
        return;
    }

    if (result.pairs.empty()) {
        ui->collisionStatusLabel->setText(
            QStringLiteral("No collision pairs were evaluated.%1").arg(sourceSuffix));
        applyDebugVisualState();
        return;
    }

    ui->collisionStatusLabel->setText(
        QStringLiteral("No self-collision detected for the current joint state with safety margin %1 mm.%2")
            .arg(formatNumber(ui->collisionSafetyMarginSpinBox->value(), 3))
            .arg(sourceSuffix));
    applyDebugVisualState();
}

/// Shows `message` in both the IK status label and the main window's status bar.
void MainWindow::updateIkStatus(const QString& message)
{
    ui->ikStatusLabel->setText(message);
    statusBar()->showMessage(message);
}

/// Enables `ui->applySelectedSolutionButton` only when a valid row is currently selected in the
/// IK results table.
void MainWindow::updateActionState()
{
    const int currentRow = ui->ikResultsTableWidget->currentRow();
    const bool hasSelection = currentRow >= 0 && currentRow < static_cast<int>(lastIkSolutions_.size());
    ui->applySelectedSolutionButton->setEnabled(hasSelection);
}

/// Computes the current TCP pose (selected tool, expressed in the selected reference frame) and
/// copies it into the IK target spin boxes (blocking their signals while doing so, so this does
/// not itself trigger a re-solve); reports an error via updateIkStatus() if the selected tool or
/// reference frame cannot be resolved.
void MainWindow::resetTargetToCurrentTcp()
{
    const JointVector joints = currentJointVector();
    const FkChain chain = ForwardKinematics::computeChain(config_, joints);
    const Result<Tool> tool = selectedTool();
    const Result<Pose> referenceInBase = selectedReferenceInBase(chain);

    if (!tool.ok()) {
        updateIkStatus(QStringLiteral("%1: %2").arg(Robot3DVisualizer::statusText(tool.status),
                                                    QString::fromStdString(tool.message)));
        return;
    }
    if (!referenceInBase.ok()) {
        updateIkStatus(QStringLiteral("%1: %2")
                           .arg(Robot3DVisualizer::statusText(referenceInBase.status),
                                QString::fromStdString(referenceInBase.message)));
        return;
    }

    const Pose tcpInBase = ForwardKinematics::toolPose(config_, joints, tool.value.flangeToTcp);
    const Pose tcpInReference = referenceInBase.value.inverse() * tcpInBase;
    const Robot3DVisualizer::PendantPoseDisplay display =
        Robot3DVisualizer::toNachiPendantPose(tcpInReference);

    const std::array<QDoubleSpinBox*, 6> spins = targetSpinBoxes();
    const std::array<double, 6> values = {
        display.x_mm, display.y_mm, display.z_mm, display.rz_deg, display.ry_deg, display.rx_deg,
    };
    for (std::size_t i = 0; i < spins.size(); ++i) {
        const QSignalBlocker blocker(spins[i]);
        spins[i]->setValue(values[i]);
    }

    updateIkStatus(QStringLiteral("Copied current TCP pose into the IK target fields."));
}

/// Builds an IKRequest from the current target spin boxes, requested posture, reference frame,
/// tool, and options, then solves it with `robot_.solve()` or `robot_.solveAll()`; populates the
/// results table and reports a summary (solution count and status) via updateIkStatus().
/// @param solveAll if true, calls solveAll() to enumerate every solution branch instead of just
/// the best one
void MainWindow::solveInverseKinematics(bool solveAll)
{
    const Result<std::optional<ArmPosture>> postureRequest = requestedPosture();
    if (!postureRequest.ok()) {
        updateIkStatus(QStringLiteral("%1: %2")
                           .arg(Robot3DVisualizer::statusText(postureRequest.status),
                                QString::fromStdString(postureRequest.message)));
        return;
    }

    if (ui->requirePostureCheckBox->isChecked() && !postureRequest.value.has_value()) {
        updateIkStatus(QStringLiteral("Select at least one posture branch or disable require posture."));
        return;
    }

    IKRequest request;
    request.targetPose = Robot3DVisualizer::fromNachiPendantPose(
        ui->targetXSpinBox->value(),
        ui->targetYSpinBox->value(),
        ui->targetZSpinBox->value(),
        ui->targetRzSpinBox->value(),
        ui->targetRySpinBox->value(),
        ui->targetRxSpinBox->value());
    request.seedJoint = currentJointVector();
    request.previousJoint = currentJointVector();
    if (postureRequest.value.has_value()) {
        request.posture = *postureRequest.value;
    }
    request.referenceFrame = FrameId{ui->referenceFrameComboBox->currentData().toString().toStdString()};
    request.tool = ToolId{ui->toolComboBox->currentData().toString().toStdString()};
    request.options.requirePosture = ui->requirePostureCheckBox->isChecked();
    request.options.maxSolutions = ui->maxSolutionsSpinBox->value();

    const IKResult result = solveAll ? robot_.solveAll(request) : robot_.solve(request);
    populateIkResults(result);

    QString summary = QStringLiteral("%1 returned %2 solution(s) with status %3")
                          .arg(solveAll ? QStringLiteral("solveAll") : QStringLiteral("solve"))
                          .arg(result.solutions.size())
                          .arg(Robot3DVisualizer::statusText(result.status));
    if (!result.message.empty()) {
        summary += QStringLiteral(": ") + QString::fromStdString(result.message);
    }
    updateIkStatus(summary);
}

/// Stores `result.solutions` in `lastIkSolutions_` and rebuilds the IK results table: one row
/// per solution with rank, per-joint degrees, position/orientation error, posture labels, and a
/// total-cost cell whose tooltip breaks down the individual cost components; selects the first
/// row (the best solution) when any exist.
void MainWindow::populateIkResults(const IKResult& result)
{
    lastIkSolutions_ = result.solutions;
    ui->ikResultsTableWidget->setRowCount(static_cast<int>(result.solutions.size()));

    for (int row = 0; row < static_cast<int>(result.solutions.size()); ++row) {
        const IKSolution& solution = result.solutions[static_cast<std::size_t>(row)];
        const std::vector<double> jointsDeg = solution.joints.toDegrees();

        auto* rankItem = new QTableWidgetItem(QString::number(row + 1));
        ui->ikResultsTableWidget->setItem(row, 0, rankItem);

        for (int jointIndex = 0; jointIndex < 6; ++jointIndex) {
            auto* jointItem = new QTableWidgetItem(formatNumber(jointsDeg[static_cast<std::size_t>(jointIndex)], 4));
            ui->ikResultsTableWidget->setItem(row, jointIndex + 1, jointItem);
        }

        ui->ikResultsTableWidget->setItem(
            row, 7, new QTableWidgetItem(formatNumber(units::toMm(solution.positionError_m), 6)));
        ui->ikResultsTableWidget->setItem(
            row, 8, new QTableWidgetItem(formatNumber(units::toDeg(solution.orientationError_rad), 6)));
        ui->ikResultsTableWidget->setItem(
            row, 9, new QTableWidgetItem(Robot3DVisualizer::postureLabel(config_.posture, "shoulder", solution.posture.shoulder)));
        ui->ikResultsTableWidget->setItem(
            row, 10, new QTableWidgetItem(Robot3DVisualizer::postureLabel(config_.posture, "elbow", solution.posture.elbow)));
        ui->ikResultsTableWidget->setItem(
            row, 11, new QTableWidgetItem(Robot3DVisualizer::postureLabel(config_.posture, "wrist", solution.posture.wrist)));

        auto* totalCostItem = new QTableWidgetItem(formatNumber(solution.score.totalCost, 6));
        totalCostItem->setToolTip(
            QStringLiteral("seed=%1\nmotion=%2\nlimit=%3\nposture=%4")
                .arg(formatNumber(solution.score.seedDistanceCost, 6))
                .arg(formatNumber(solution.score.motionContinuityCost, 6))
                .arg(formatNumber(solution.score.jointLimitMarginCost, 6))
                .arg(formatNumber(solution.score.postureMismatchCost, 6)));
        ui->ikResultsTableWidget->setItem(row, 12, totalCostItem);
    }

    if (!result.solutions.empty()) {
        ui->ikResultsTableWidget->selectRow(0);
    }

    updateActionState();
}

/// Rebuilds the collision-pairs table from `result.pairs`, sorted with colliding pairs first
/// (then by ascending clearance distance) and colliding rows highlighted; clears the table if
/// `result` did not complete successfully.
void MainWindow::populateCollisionPairs(const CollisionCheckResult& result)
{
    if (!result.ok()) {
        ui->collisionPairsTableWidget->setRowCount(0);
        return;
    }

    std::vector<CollisionPairResult> displayPairs = result.pairs;
    std::stable_sort(displayPairs.begin(), displayPairs.end(),
                     [](const CollisionPairResult& a, const CollisionPairResult& b) {
                         if (a.colliding != b.colliding) {
                             return a.colliding && !b.colliding;
                         }
                         return a.distance_m < b.distance_m;
                     });

    ui->collisionPairsTableWidget->setRowCount(static_cast<int>(displayPairs.size()));

    for (int row = 0; row < static_cast<int>(displayPairs.size()); ++row) {
        const CollisionPairResult& pair = displayPairs[static_cast<std::size_t>(row)];
        const QString stateText = pair.colliding ? QStringLiteral("Colliding") : QStringLiteral("Clear");
        auto* stateItem = new QTableWidgetItem(stateText);
        auto* geometryAItem = new QTableWidgetItem(QString::fromStdString(pair.geometryA));
        auto* geometryBItem = new QTableWidgetItem(QString::fromStdString(pair.geometryB));
        auto* linkAItem = new QTableWidgetItem(QString::fromStdString(pair.linkA));
        auto* linkBItem = new QTableWidgetItem(QString::fromStdString(pair.linkB));
        auto* clearanceItem = new QTableWidgetItem(formatNumber(units::toMm(pair.distance_m), 3));

        ui->collisionPairsTableWidget->setItem(row, 0, stateItem);
        ui->collisionPairsTableWidget->setItem(row, 1, geometryAItem);
        ui->collisionPairsTableWidget->setItem(row, 2, geometryBItem);
        ui->collisionPairsTableWidget->setItem(row, 3, linkAItem);
        ui->collisionPairsTableWidget->setItem(row, 4, linkBItem);
        ui->collisionPairsTableWidget->setItem(row, 5, clearanceItem);

        if (pair.colliding) {
            const QColor highlight(255, 234, 228);
            stateItem->setBackground(highlight);
            geometryAItem->setBackground(highlight);
            geometryBItem->setBackground(highlight);
            linkAItem->setBackground(highlight);
            linkBItem->setBackground(highlight);
            clearanceItem->setBackground(highlight);
        }
    }

    if (!displayPairs.empty()) {
        ui->collisionPairsTableWidget->selectRow(0);
    }
}

/// Copies the joint angles of the currently selected IK results-table row into the joint spin
/// boxes via setJointDegrees(); reports via updateIkStatus() if no row is selected.
void MainWindow::applySelectedIkSolution()
{
    const int row = ui->ikResultsTableWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(lastIkSolutions_.size())) {
        updateIkStatus(QStringLiteral("Select an IK solution before applying it."));
        return;
    }

    const std::vector<double> degrees = lastIkSolutions_[static_cast<std::size_t>(row)].joints.toDegrees();
    std::array<double, 6> values{};
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = degrees[i];
    }
    setJointDegrees(values);
    // ui->controlTabWidget->setCurrentWidget(ui->fkTab);
    updateIkStatus(QStringLiteral("Applied IK solution #%1 to the joint controls.").arg(row + 1));
}

/// Sets each joint spin box to the corresponding value in `degrees` (blocking signals so this
/// does not recursively trigger per-spin-box change handlers), then refreshes the scene and
/// readouts once via applyJointStateToSceneAndReadouts().
/// @param degrees target angle in degrees for each of the 6 movable joints, in spin-box order
void MainWindow::setJointDegrees(const std::array<double, 6>& degrees)
{
    const std::array<QDoubleSpinBox*, 6> spins = jointSpinBoxes();
    for (std::size_t i = 0; i < spins.size(); ++i) {
        const QSignalBlocker blocker(spins[i]);
        spins[i]->setValue(degrees[i]);
    }
    applyJointStateToSceneAndReadouts();
}

/// @return the current joint state read directly from the 6 joint spin boxes, in degrees.
JointVector MainWindow::currentJointVector() const
{
    const std::array<QDoubleSpinBox*, 6> spins = jointSpinBoxes();
    return JointVector::fromDegrees({
        spins[0]->value(),
        spins[1]->value(),
        spins[2]->value(),
        spins[3]->value(),
        spins[4]->value(),
        spins[5]->value(),
    });
}

/// @return the 6 movable-joint spin boxes (joint1..joint6), in joint order.
std::array<QDoubleSpinBox*, 6> MainWindow::jointSpinBoxes() const
{
    return {{
        ui->joint1SpinBox,
        ui->joint2SpinBox,
        ui->joint3SpinBox,
        ui->joint4SpinBox,
        ui->joint5SpinBox,
        ui->joint6SpinBox,
    }};
}

/// @return the 6 IK target spin boxes (X, Y, Z, Rz, Ry, Rx), in that order.
std::array<QDoubleSpinBox*, 6> MainWindow::targetSpinBoxes() const
{
    return {{
        ui->targetXSpinBox,
        ui->targetYSpinBox,
        ui->targetZSpinBox,
        ui->targetRzSpinBox,
        ui->targetRySpinBox,
        ui->targetRxSpinBox,
    }};
}

/// @return the 8 per-part "visible" checkboxes, in `kRobotParts` order (base, j1..j6, tool).
std::array<QCheckBox*, 8> MainWindow::partVisibleCheckBoxes() const
{
    return {{
        ui->baseVisibleCheckBox,
        ui->j1VisibleCheckBox,
        ui->j2VisibleCheckBox,
        ui->j3VisibleCheckBox,
        ui->j4VisibleCheckBox,
        ui->j5VisibleCheckBox,
        ui->j6VisibleCheckBox,
        ui->toolVisibleCheckBox,
    }};
}

/// @return the 8 per-part "show origin axes" checkboxes, in `kRobotParts` order.
std::array<QCheckBox*, 8> MainWindow::partOriginCheckBoxes() const
{
    return {{
        ui->baseOriginCheckBox,
        ui->j1OriginCheckBox,
        ui->j2OriginCheckBox,
        ui->j3OriginCheckBox,
        ui->j4OriginCheckBox,
        ui->j5OriginCheckBox,
        ui->j6OriginCheckBox,
        ui->toolOriginCheckBox,
    }};
}

/// @return the 8 per-part "show joint axis" checkboxes, in `kRobotParts` order.
std::array<QCheckBox*, 8> MainWindow::partAxisCheckBoxes() const
{
    return {{
        ui->baseAxisCheckBox,
        ui->j1AxisCheckBox,
        ui->j2AxisCheckBox,
        ui->j3AxisCheckBox,
        ui->j4AxisCheckBox,
        ui->j5AxisCheckBox,
        ui->j6AxisCheckBox,
        ui->toolAxisCheckBox,
    }};
}

/// @return the tool currently selected in `ui->toolComboBox` (looked up in `toolRegistry_`), or
/// the registry's default tool if the combo box has no valid selection.
Result<Tool> MainWindow::selectedTool() const
{
    const QString toolId = ui->toolComboBox->currentData().toString();
    return toolId.isEmpty() ? toolRegistry_.getDefault() : toolRegistry_.get(ToolId{toolId.toStdString()});
}

/// Resolves the reference frame currently selected in `ui->referenceFrameComboBox` to a pose in
/// the base frame: identity for "Base" (or an empty selection), otherwise looks up the user
/// frame in `frameRegistry_` and evaluates it against `chain`.
/// @param chain forward-kinematics result used to evaluate a user-defined frame, if selected
/// @return the resolved reference frame pose, or a failure Result if the selected frame id is
/// not registered
Result<Pose> MainWindow::selectedReferenceInBase(const FkChain& chain) const
{
    const QString frameId = ui->referenceFrameComboBox->currentData().toString();
    if (frameId.isEmpty() || frameId == QStringLiteral("base")) {
        return Result<Pose>::success(Pose::identity());
    }

    const Result<UserFrame> frame = frameRegistry_.get(FrameId{frameId.toStdString()});
    if (!frame.ok()) {
        return Result<Pose>::failure(frame.status, frame.message);
    }
    return ForwardKinematics::userFrameInBase(chain, frame.value);
}

/// Reads the shoulder/elbow/wrist posture-request combo boxes and resolves them (via
/// `postureResolver_->fromLabels()`) into an ArmPosture to constrain the next IK solve.
/// @return std::nullopt if no posture resolver exists or no branch was requested (any combo left
/// at "Any"); otherwise the resolved posture, or a failure Result if the selected label
/// combination could not be resolved.
Result<std::optional<ArmPosture>> MainWindow::requestedPosture() const
{
    if (!postureResolver_) {
        return Result<std::optional<ArmPosture>>::success(std::nullopt);
    }

    std::map<std::string, std::string> labels;
    /// Records the selected label for `axis` in `labels`, if `comboBox` has a non-empty selection.
    const auto capture = [&](QComboBox* comboBox, const std::string& axis) {
        const QString value = comboBox->currentData().toString();
        if (!value.isEmpty()) {
            labels[axis] = value.toStdString();
        }
    };

    capture(ui->shoulderRequestComboBox, "shoulder");
    capture(ui->elbowRequestComboBox, "elbow");
    capture(ui->wristRequestComboBox, "wrist");

    if (labels.empty()) {
        return Result<std::optional<ArmPosture>>::success(std::nullopt);
    }

    const Result<ArmPosture> posture = postureResolver_->fromLabels(config_.posture, labels);
    if (!posture.ok()) {
        return Result<std::optional<ArmPosture>>::failure(posture.status, posture.message);
    }
    return Result<std::optional<ArmPosture>>::success(posture.value);
}

/// Loads and validates the original and simplified mesh collision profiles referenced by
/// `config_.metadata["meshCollisionProfile"]`: resolves the original path via
/// findRepoRelativePath(), derives the simplified path via
/// Robot3DVisualizer::meshProfileSimplifiedPath(), and loads/validates each into
/// `meshOriginalProfile_`/`meshSimplifiedProfile_` (leaving both with an explanatory `note` if
/// metadata is missing or a file cannot be found).
void MainWindow::loadMeshCollisionProfiles()
{
    meshOriginalProfile_ = MeshProfileState{};
    meshSimplifiedProfile_ = MeshProfileState{};

    const auto metadataIt = config_.metadata.find("meshCollisionProfile");
    if (metadataIt == config_.metadata.end()) {
        meshOriginalProfile_.note = QStringLiteral(
            "Preset metadata has no `meshCollisionProfile` entry; mesh modes are unavailable.");
        meshSimplifiedProfile_.note = meshOriginalProfile_.note;
        return;
    }

    const QString relativeOriginal = QString::fromStdString(metadataIt->second);
    const QString resolvedOriginal = findRepoRelativePath(relativeOriginal);
    if (resolvedOriginal.isEmpty()) {
        meshOriginalProfile_.note =
            QStringLiteral("Preset references `%1`, but the file was not found.")
                .arg(relativeOriginal);
        meshSimplifiedProfile_.note = meshOriginalProfile_.note;
        return;
    }

    /// Loads the mesh collision profile JSON at `path` and validates it against `config_`,
    /// writing the outcome (loaded/valid flags, source path, and a human-readable note including
    /// mesh count and backend preference on success) into `state`.
    const auto loadAndValidate = [this](const QString& path, MeshProfileState& state) {
        state.loaded = false;
        state.valid = false;
        state.source.clear();
        state.note.clear();

        const Result<MeshCollisionProfile> loaded =
            MeshCollisionProfileJsonLoader::loadFile(path.toStdString());
        if (!loaded.ok()) {
            state.note = QStringLiteral("Failed to load %1 (%2).")
                             .arg(QDir::toNativeSeparators(path),
                                  QString::fromStdString(loaded.message));
            return;
        }

        const MeshCollisionProfileValidationResult validation =
            MeshCollisionProfileValidator::validate(config_, loaded.value);
        if (!validation.ok()) {
            state.loaded = true;
            state.source = QDir::toNativeSeparators(path);
            state.note =
                QStringLiteral("Validation failed: %1")
                    .arg(QString::fromStdString(validation.issues.front().message));
            return;
        }

        state.profile = loaded.value;
        state.loaded = true;
        state.valid = true;
        state.source = QDir::toNativeSeparators(path);
        state.note = QStringLiteral("%1 mesh(es) loaded; backend preference: ")
                         .arg(loaded.value.meshes.size());
        QStringList preferences;
        for (MeshCollisionBackendKind backend : loaded.value.backendPreference) {
            preferences << QString::fromStdString(toString(backend));
        }
        state.note += preferences.isEmpty() ? QStringLiteral("(none)") : preferences.join(QStringLiteral(", "));
    };

    loadAndValidate(resolvedOriginal, meshOriginalProfile_);

    const QString relativeSimplified = Robot3DVisualizer::meshProfileSimplifiedPath(relativeOriginal);
    const QString resolvedSimplified = findRepoRelativePath(relativeSimplified);
    if (resolvedSimplified.isEmpty()) {
        meshSimplifiedProfile_.note = QStringLiteral(
            "Simplified mesh profile not found at `%1`. Run "
            "scripts/run_mesh_simplification_nachi_msvc.bat to generate it.")
            .arg(relativeSimplified);
        return;
    }

    loadAndValidate(resolvedSimplified, meshSimplifiedProfile_);
}

/// Rebuilds `ui->collisionBackendComboBox` (primitive always available; mesh-original/simplified
/// added only when the mesh backend is compiled in and their respective profile is valid),
/// resets the selection to the primitive backend, and updates the mesh-backend availability and
/// per-profile status labels.
void MainWindow::populateBackendControls()
{
    QSignalBlocker blocker(ui->collisionBackendComboBox);
    ui->collisionBackendComboBox->clear();
    ui->collisionBackendComboBox->addItem(
        QStringLiteral("Primitive (sphere/capsule)"),
        static_cast<int>(BackendSelection::Primitive));

    const bool meshBackendAvailable = meshBackendInfo_.available;
    if (meshBackendAvailable && meshOriginalProfile_.valid) {
        ui->collisionBackendComboBox->addItem(
            QStringLiteral("Mesh - Original STL"),
            static_cast<int>(BackendSelection::MeshOriginal));
    }
    if (meshBackendAvailable && meshSimplifiedProfile_.valid) {
        ui->collisionBackendComboBox->addItem(
            QStringLiteral("Mesh - Simplified STL"),
            static_cast<int>(BackendSelection::MeshSimplified));
    }
    ui->collisionBackendComboBox->setCurrentIndex(0);
    backendSelection_ = BackendSelection::Primitive;

    if (meshBackendInfo_.available) {
        ui->meshBackendAvailabilityValueLabel->setText(
            QStringLiteral("Available: %1 - %2")
                .arg(QString::fromStdString(meshBackendInfo_.backendName),
                     QString::fromStdString(meshBackendInfo_.detail)));
    } else {
        ui->meshBackendAvailabilityValueLabel->setText(
            QStringLiteral("Unavailable: %1. Rebuild RobotKinematics with the optional mesh backend "
                           "(scripts\\build_msvc_mesh_coal.bat) and relink the example to the "
                           "build\\msvc_mesh_coal\\lib library to enable mesh modes.")
                .arg(QString::fromStdString(meshBackendInfo_.detail)));
    }

    /// Formats a one-line status string for a mesh profile: "OK - source (note)" when valid,
    /// "Invalid - note" when loaded but failed validation, the stored note if present, or
    /// "Not loaded" otherwise.
    const auto stateText = [](const MeshProfileState& state) {
        if (state.valid) {
            return QStringLiteral("OK - %1 (%2)").arg(state.source, state.note);
        }
        if (state.loaded) {
            return QStringLiteral("Invalid - %1").arg(state.note);
        }
        if (!state.note.isEmpty()) {
            return state.note;
        }
        return QStringLiteral("Not loaded");
    };

    ui->meshOriginalProfileValueLabel->setText(stateText(meshOriginalProfile_));
    ui->meshSimplifiedProfileValueLabel->setText(stateText(meshSimplifiedProfile_));
}

/// Loads the primitive collision profile referenced by
/// `config_.metadata["collisionProfile"]`, falling back to the built-in conservative
/// `CollisionProfiles::nachiMZ04D()` profile if metadata is missing, the file cannot be found,
/// or loading fails; validates whichever profile was obtained against `config_` and only marks
/// `collisionProfileAvailable_` true if validation succeeds. Records the source and any
/// fallback/validation notes in `collisionProfileSource_`/`collisionProfileNote_`.
void MainWindow::loadCollisionProfile()
{
    collisionProfileAvailable_ = false;
    collisionProfileSource_.clear();
    collisionProfileNote_.clear();

    Result<CollisionProfile> loadedProfile =
        Result<CollisionProfile>::failure(KinematicsStatus::InvalidRequest, "no collision profile attempted");

    const auto metadataIt = config_.metadata.find("collisionProfile");
    if (metadataIt != config_.metadata.end()) {
        const QString relativePath = QString::fromStdString(metadataIt->second);
        const QString resolvedPath = findRepoRelativePath(relativePath);
        if (!resolvedPath.isEmpty()) {
            loadedProfile = CollisionProfileJsonLoader::loadFile(resolvedPath.toStdString());
            if (loadedProfile.ok()) {
                collisionProfileSource_ = QStringLiteral("External JSON: %1")
                                              .arg(QDir::toNativeSeparators(resolvedPath));
            } else {
                collisionProfileNote_ =
                    QStringLiteral("Failed to load %1 (%2). Falling back to the built-in conservative profile.")
                        .arg(QDir::toNativeSeparators(resolvedPath),
                             QString::fromStdString(loadedProfile.message));
            }
        } else {
            collisionProfileNote_ =
                QStringLiteral("Preset metadata references `%1`, but the file was not found. Falling back to the built-in conservative profile.")
                    .arg(relativePath);
        }
    }

    if (!loadedProfile.ok()) {
        loadedProfile = Result<CollisionProfile>::success(CollisionProfiles::nachiMZ04D());
        if (collisionProfileSource_.isEmpty()) {
            collisionProfileSource_ = QStringLiteral("Built-in conservative Nachi profile");
        }
    }

    const CollisionProfileValidationResult validation =
        CollisionProfileValidator::validate(config_, loadedProfile.value);
    if (!validation.ok()) {
        collisionProfileAvailable_ = false;
        collisionProfileSource_ = QStringLiteral("Unavailable");
        collisionProfileNote_ =
            QStringLiteral("Collision profile validation failed: %1").arg(QString::fromStdString(validation.issues.front().message));
        return;
    }

    collisionProfile_ = loadedProfile.value;
    collisionProfileAvailable_ = true;
    if (collisionProfileNote_.isEmpty()) {
        collisionProfileNote_ =
            QStringLiteral("Conservative primitive profile only; useful for debugging and authoring, not safety-rated.");
    } else {
        collisionProfileNote_ += QStringLiteral(" Conservative primitive profile only; not safety-rated.");
    }
}
