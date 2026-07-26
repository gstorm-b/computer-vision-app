#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <RobotKinematics/Collision/CollisionBackend.h>
#include <RobotKinematics/Collision/CollisionChecker.h>
#include <RobotKinematics/Collision/CollisionProfile.h>
#include <RobotKinematics/Collision/MeshCollisionProfile.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Model/FrameRegistry.h>
#include <RobotKinematics/Model/ToolRegistry.h>
#include <RobotKinematics/Posture/PostureResolver.h>

#include <vtkSmartPointer.h>

#include <array>
#include <memory>
#include <set>
#include <vector>

class QVTKOpenGLNativeWidget;
class QCheckBox;
class QDoubleSpinBox;
class QTableWidget;
class vtkGenericOpenGLRenderWindow;
class vtkActor;
class vtkAxesActor;
class vtkLineSource;
class vtkOrientationMarkerWidget;
class vtkRenderer;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/// Main window for the RobotKinematics 3D pose visualizer example: hosts the VTK viewport and the
/// FK/IK/collision control panels, and owns the SerialRobotKinematics model instance plus every
/// scene/debug actor used to render the Nachi MZ04D preset robot.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// Builds the window: wires up the UI, VTK viewport, and model state, loads the robot's visual
    /// and collision-debug meshes, then poses the robot at its home joint configuration.
    /// @param parent optional parent widget
    explicit MainWindow(QWidget *parent = nullptr);
    /// Destroys the generated UI form (`ui`); VTK actors/renderer are released via smart pointers.
    ~MainWindow() override;

private:
    /// Runtime state for one rendered robot mesh part (e.g. a joint link or the tool): its STL
    /// actor, the home-pose baseline used to compute per-frame deltas, and the optional
    /// origin/joint-axis debug actors toggled from the UI.
    struct VisualPartState {
        QString key;                    ///< Part identifier matching RobotVisualPartSpec::key (e.g. "j1", "tool").
        QString displayName;            ///< Human-readable label shown in the debug controls.
        std::string linkId;             ///< Kinematic link this part's pose is driven from.
        RobotKinematics::Pose homeLinkInBase = RobotKinematics::Pose::identity();        ///< Link pose in base frame at the home joint configuration; the FK delta baseline.
        RobotKinematics::Pose homeVisualCorrection = RobotKinematics::Pose::identity();  ///< Per-part CAD-to-link correction from visualHomeCorrectionForPartKey().
        std::array<double, 3> baseColorRgb = {0.75, 0.75, 0.75};  ///< Non-colliding display color for the actor.
        int jointAxisIndex = -1;         ///< Index into FkChain::joints for the axis-debug line, or -1 if this part has no associated joint.
        bool isLoaded = false;           ///< True once the STL mesh loaded successfully.
        vtkSmartPointer<vtkActor> actor;             ///< The mesh actor itself.
        vtkSmartPointer<vtkAxesActor> originActor;   ///< Debug actor showing the part's local origin/axes.
        vtkSmartPointer<vtkLineSource> axisSource;   ///< Line geometry backing the joint-axis debug actor.
        vtkSmartPointer<vtkActor> axisActor;         ///< Debug actor rendering the joint axis line.
    };

    /// Runtime state for one primitive collision-debug shape (sphere or capsule) mirrored from the
    /// active CollisionProfile, driving the translucent debug actors shown when "show collision
    /// shapes" is enabled.
    struct PrimitiveDebugState {
        std::string geometryId;          ///< Matches CollisionGeometry::id in the active CollisionProfile.
        std::string linkId;              ///< Kinematic link the shape is attached to.
        RobotKinematics::CollisionShapeType shapeType = RobotKinematics::CollisionShapeType::Sphere;  ///< Sphere or Capsule; selects which actors below are used.
        RobotKinematics::Pose geometryToLink = RobotKinematics::Pose::identity();  ///< Shape pose relative to the link frame.
        double radius_m = 0.0;           ///< Sphere/capsule radius in metres.
        double length_m = 0.0;           ///< Capsule segment length in metres (unused for spheres).
        std::array<double, 3> baseColorRgb = {0.45, 0.85, 0.65};  ///< Non-colliding display color.
        vtkSmartPointer<vtkActor> bodyActor;      ///< Sphere actor, or capsule cylinder body actor.
        vtkSmartPointer<vtkActor> capStartActor;  ///< Capsule start-cap sphere actor (capsules only).
        vtkSmartPointer<vtkActor> capEndActor;    ///< Capsule end-cap sphere actor (capsules only).
    };

    /// Runtime state for one mesh collision-debug wireframe (original or simplified STL), loaded
    /// from a MeshCollisionProfile and re-posed each frame from the FK chain.
    struct MeshDebugState {
        std::string meshId;              ///< Matches MeshCollisionGeometry::id in the source profile.
        std::string linkId;              ///< Kinematic link the mesh is attached to.
        RobotKinematics::Pose meshToLink = RobotKinematics::Pose::identity();  ///< Mesh pose relative to the link frame.
        double stlScaleToMm = 1.0;       ///< Scale from the STL's authored units to the mm-based VTK scene (mesh.scaleToMeters * 1000).
        std::array<double, 3> baseColorRgb = {0.2, 0.85, 0.85};  ///< Non-colliding wireframe color.
        vtkSmartPointer<vtkActor> actor; ///< The wireframe actor.
    };

    /// Which collision backend/profile the UI is currently checking self-collision against.
    enum class BackendSelection {
        Primitive,      ///< Built-in sphere/capsule primitive backend.
        MeshOriginal,   ///< Mesh backend using the original (unsimplified) STL collision profile.
        MeshSimplified, ///< Mesh backend using the simplified STL collision profile.
    };

    /// Load/validation state for one mesh collision profile (original or simplified STL set),
    /// populated by loadMeshCollisionProfiles().
    struct MeshProfileState {
        RobotKinematics::MeshCollisionProfile profile;  ///< The parsed profile; valid only when `valid` is true.
        bool loaded = false;  ///< True once a JSON file was found and parsed (independent of validation).
        bool valid = false;   ///< True when `profile` parsed and passed MeshCollisionProfileValidator.
        QString source;       ///< Resolved, native-separator path the profile was loaded from.
        QString note;         ///< Human-readable status/error message shown in the backend controls.
    };

    /// Creates the VTK render window/renderer inside the UI's viewport container, and adds the
    /// gradient background, ground-plane wireframe, key light, and orientation-marker axes.
    void setupVtkViewport();
    /// Warns if the posture resolver could not be built for the active preset, then loads the
    /// primitive and mesh collision profiles and queries the compiled mesh-backend info.
    void setupModelState();
    /// Configures the splitter, populates every combo/joint/posture/sample/collision/backend/debug
    /// control, sets up the results tables, and initializes the status labels.
    void setupUiState();
    /// Wires every joint/target/tool/frame/posture/collision UI control to
    /// applyJointStateToSceneAndReadouts() (or a narrower slot), and the sample/IK/debug buttons to
    /// their handlers.
    void connectSignals();
    /// Locates the Nachi STL asset directory, loads each robot part's mesh into a VTK actor (with
    /// origin and joint-axis debug actors), and records per-part home pose/correction state in
    /// visualParts_; reports any missing/unreadable meshes via a message box and the status bar.
    void loadRobotVisuals();
    /// Loads the primitive CollisionProfile referenced by the preset's metadata (falling back to
    /// the built-in conservative Nachi profile) and validates it, updating
    /// collisionProfileAvailable_, collisionProfileSource_, and collisionProfileNote_.
    void loadCollisionProfile();
    /// Loads and validates the original and simplified mesh collision profiles referenced by the
    /// preset's `meshCollisionProfile` metadata entry, populating meshOriginalProfile_ and
    /// meshSimplifiedProfile_ with the parsed profile or a diagnostic note on failure.
    void loadMeshCollisionProfiles();
    /// Builds translucent sphere/capsule debug actors for every enabled geometry in the active
    /// primitive CollisionProfile and stores their runtime state in primitiveDebugStates_.
    void loadCollisionDebugVisuals();
    /// Rebuilds the mesh collision-debug wireframe actors for both the original and simplified mesh
    /// profiles: removes any previously added actors, then loads an STL reader per enabled mesh
    /// geometry that exists on disk.
    void loadMeshCollisionDebugVisuals();
    /// Recomputes the FK chain from the current joint spin boxes and refreshes the scene transforms,
    /// pose readouts, joint/posture/collision status, and action-button enablement; re-renders the
    /// VTK view.
    void applyJointStateToSceneAndReadouts();
    /// Repositions every loaded visual part's actor (and its origin/axis debug actors) from `chain`,
    /// then refreshes the collision- and mesh-debug visuals and debug visibility state.
    /// @param chain FK chain to pose the scene from
    void updateSceneFromChain(const RobotKinematics::FkChain& chain);
    /// Repositions the sphere/capsule primitive collision-debug actors from `chain`; a no-op if no
    /// primitive collision profile is available.
    /// @param chain FK chain used to place each primitive geometry in the base frame
    void updateCollisionDebugVisuals(const RobotKinematics::FkChain& chain);
    /// Repositions the original and simplified mesh collision-debug wireframe actors from `chain`.
    /// @param chain FK chain used to place each mesh geometry in the base frame
    void updateMeshDebugVisuals(const RobotKinematics::FkChain& chain);
    /// Computes the flange and TCP poses in the selected reference frame and writes them (in Nachi
    /// pendant XYZ/RPY convention) into the flange/TCP line edits; shows an IK status message
    /// instead if the selected tool or reference frame is invalid.
    /// @param chain FK chain to read the flange pose from
    void updatePoseReadouts(const RobotKinematics::FkChain& chain);
    /// Validates `joints` against the configured joint limits and reports the first violation (or
    /// "within limits") in the joint status label.
    void updateJointStatus(const RobotKinematics::JointVector& joints);
    /// Classifies the current shoulder/elbow/wrist posture via the posture resolver and updates the
    /// corresponding value labels, or shows "Unavailable" if no resolver exists or classification
    /// fails.
    void updateCurrentPosture(const RobotKinematics::JointVector& joints);
    /// Runs a self-collision check with the currently selected backend (primitive, mesh-original,
    /// or mesh-simplified) and safety margin, records the colliding geometry/link ids, refreshes
    /// the collision pairs table and status label, and updates the debug visual highlight colors.
    void updateCollisionState(const RobotKinematics::JointVector& joints);
    /// Shows `message` in the IK status label and the window's status bar.
    void updateIkStatus(const QString& message);
    /// Enables the "apply selected solution" button only when a valid row is selected in the IK
    /// results table.
    void updateActionState();
    /// Fills the tool combo box from the preset's tools (preferring "centering_tool" if present)
    /// and the reference-frame combo box with "Base" plus every configured user frame.
    void populateCombos();
    /// Sets each joint spin box's suffix/range from the preset's joint limits (or +/-360 degrees if
    /// unlimited) and sets the target pose spin boxes' mm/degree suffixes.
    void populateJointControls();
    /// Fills the shoulder/elbow/wrist posture-request combo boxes with "Any" plus the preset's
    /// negative/positive branch labels for each axis.
    void populatePostureControls();
    /// Sets tooltips on the teach-point sample buttons referencing their source documentation.
    void populateSampleButtons();
    /// Initializes the collision safety-margin suffix and default checkbox states, and shows the
    /// active collision profile's source/note in the collision panel labels.
    void populateCollisionControls();
    /// Rebuilds the collision-backend combo box (primitive, plus mesh-original/mesh-simplified only
    /// when the mesh backend is compiled in and the corresponding profile is valid), resets the
    /// selection to Primitive, and updates the mesh-backend/profile status labels.
    void populateBackendControls();
    /// Resets the per-part visible/origin/axis debug checkboxes to their default states, disables
    /// the base/tool axis checkboxes (they have no associated joint), and sets the debug hint label.
    void populateDebugControls();
    /// Applies per-part visibility (and collision-highlight color) to every visual part's actor,
    /// origin actor, and axis actor from their checkboxes and collidingLinkIds_, and shows/hides the
    /// primitive or mesh collision-debug shapes for the currently selected backend; re-renders the
    /// VTK view.
    void applyDebugVisualState();
    /// Copies the current TCP pose (in the selected reference frame) into the IK target spin boxes,
    /// blocking their signals so this does not itself trigger a scene update.
    void resetTargetToCurrentTcp();
    /// Builds an IKRequest from the target pose, current joints (as seed), the selected
    /// posture/tool/reference-frame controls, and solver options, then solves and reports the
    /// result in the IK status label.
    /// @param solveAll true to call SerialRobotKinematics::solveAll() and report every solution
    /// branch; false to call solve() and report only the best one
    void solveInverseKinematics(bool solveAll);
    /// Fills the IK results table with one row per solution (joint angles, position/orientation
    /// error, posture labels, and a cost-breakdown tooltip), selects the first row, and updates
    /// action state.
    /// @param result IK solve result whose solutions populate the results table
    void populateIkResults(const RobotKinematics::IKResult& result);
    /// Sorts the collision pairs (colliding first, then by increasing clearance) into the collision
    /// pairs table, highlighting colliding rows.
    /// @param result collision check result whose pairs populate the table
    void populateCollisionPairs(const RobotKinematics::CollisionCheckResult& result);
    /// Applies the joint angles of the currently selected IK results-table row to the joint
    /// controls, or reports an error via the IK status label if no row is selected.
    void applySelectedIkSolution();
    /// Sets the joint spin boxes to `degrees` (blocking their signals) and refreshes the scene and
    /// readouts once.
    /// @param degrees the six joint values, in degrees, to write into the joint spin boxes
    void setJointDegrees(const std::array<double, 6>& degrees);
    /// @return the current joint spin box values as a JointVector, in degrees
    RobotKinematics::JointVector currentJointVector() const;
    /// @return the six per-joint spin box widgets, joint 1 through joint 6, in order
    std::array<QDoubleSpinBox*, 6> jointSpinBoxes() const;
    /// @return the six IK-target spin box widgets (X, Y, Z, Rz, Ry, Rx), in order
    std::array<QDoubleSpinBox*, 6> targetSpinBoxes() const;
    /// @return the eight "part visible" checkboxes, base/j1..j6/tool, in order
    std::array<QCheckBox*, 8> partVisibleCheckBoxes() const;
    /// @return the eight "show part origin" checkboxes, base/j1..j6/tool, in order
    std::array<QCheckBox*, 8> partOriginCheckBoxes() const;
    /// @return the eight "show joint axis" checkboxes, base/j1..j6/tool, in order
    std::array<QCheckBox*, 8> partAxisCheckBoxes() const;
    /// @return the tool currently selected in the tool combo box, or the preset's default tool if
    /// no tool id is set; failure if the id does not resolve
    RobotKinematics::Result<RobotKinematics::Tool> selectedTool() const;
    /// @param chain FK chain used to resolve a user frame's pose, if one is selected
    /// @return identity pose when "Base" is selected, otherwise the selected user frame's pose in
    /// base; failure if the frame id does not resolve
    RobotKinematics::Result<RobotKinematics::Pose> selectedReferenceInBase(
        const RobotKinematics::FkChain& chain) const;
    /// @return nullopt if no posture resolver exists or no branch combo box has a selection;
    /// otherwise the ArmPosture resolved from the selected shoulder/elbow/wrist branch labels, or
    /// failure if the labels do not resolve
    RobotKinematics::Result<std::optional<RobotKinematics::ArmPosture>> requestedPosture() const;

    Ui::MainWindow *ui;                                                  ///< Generated UI form; owned by this window, deleted in the destructor.
    QVTKOpenGLNativeWidget* vtkWidget_ = nullptr;                        ///< VTK render widget embedded in the viewport container.
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow_;         ///< Render window backing vtkWidget_.
    vtkSmartPointer<vtkOrientationMarkerWidget> orientationMarker_;      ///< Small axes overlay showing scene orientation.
    vtkSmartPointer<vtkRenderer> renderer_;                              ///< Renderer holding every scene/debug actor.
    RobotKinematics::SerialRobotConfig config_;                          ///< Example robot configuration: the Nachi MZ04D preset plus the demo centering tool.
    RobotKinematics::SerialRobotKinematics robot_;                       ///< FK/IK solver constructed from config_.
    RobotKinematics::FrameRegistry frameRegistry_;                       ///< User-frame lookup built from config_.
    RobotKinematics::ToolRegistry toolRegistry_;                         ///< Tool lookup built from config_.
    std::unique_ptr<RobotKinematics::PostureResolver> postureResolver_;  ///< Posture classifier for config_; null if it could not be created for this preset.
    RobotKinematics::CollisionProfile collisionProfile_;                 ///< Active primitive collision profile; valid only when collisionProfileAvailable_ is true.
    bool collisionProfileAvailable_ = false;                             ///< True once collisionProfile_ has loaded and passed validation.
    MeshProfileState meshOriginalProfile_;                               ///< Load/validation state for the original-STL mesh collision profile.
    MeshProfileState meshSimplifiedProfile_;                             ///< Load/validation state for the simplified-STL mesh collision profile.
    RobotKinematics::CollisionBackendInfo meshBackendInfo_;              ///< Capability info for the compiled mesh collision backend.
    BackendSelection backendSelection_ = BackendSelection::Primitive;    ///< Collision backend currently selected in the UI.
    std::vector<VisualPartState> visualParts_;                           ///< Loaded robot mesh parts and their scene actors.
    std::vector<PrimitiveDebugState> primitiveDebugStates_;              ///< Primitive collision-debug shapes for the active profile.
    std::vector<MeshDebugState> meshOriginalDebugStates_;                ///< Mesh collision-debug wireframes for the original-STL profile.
    std::vector<MeshDebugState> meshSimplifiedDebugStates_;              ///< Mesh collision-debug wireframes for the simplified-STL profile.
    std::vector<RobotKinematics::IKSolution> lastIkSolutions_;           ///< Solutions from the most recent solveInverseKinematics() call, backing the results table.
    std::vector<RobotKinematics::CollisionPairResult> lastCollisionPairs_;  ///< Pairs from the most recent collision check, backing the collision pairs table.
    std::set<std::string> collidingGeometryIds_;                        ///< Geometry ids involved in the current colliding pairs; used to highlight debug shapes.
    std::set<std::string> collidingLinkIds_;                            ///< Link ids involved in the current colliding pairs; used to highlight mesh actors.
    QString assetsDirectory_;                                           ///< Resolved directory the robot's STL assets were loaded from.
    QString collisionProfileSource_;                                    ///< Human-readable source of the active primitive collision profile.
    QString collisionProfileNote_;                                      ///< Human-readable status/warning note for the active primitive collision profile.
};
#endif // MAINWINDOW_H
