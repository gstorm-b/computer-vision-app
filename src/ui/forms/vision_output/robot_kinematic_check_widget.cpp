#include "robot_kinematic_check_widget.h"
#include "ui_robot_kinematic_check_widget.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <Eigen/Geometry>

#include <RobotKinematics/Collision/CollisionBackend.h>
#include <RobotKinematics/Collision/MeshCollisionProfile.h>
#include <RobotKinematics/Collision/MeshCollisionProfileJsonLoader.h>
#include <RobotKinematics/Core/JointVector.h>
#include <RobotKinematics/Core/Pose.h>
#include <RobotKinematics/Core/Result.h>
#include <RobotKinematics/Core/Units.h>
#include <RobotKinematics/Kinematics/ForwardKinematics.h>
#include <RobotKinematics/Kinematics/SerialRobotKinematics.h>
#include <RobotKinematics/Model/RobotModelConfig.h>
#include <RobotKinematics/Presets/NachiMZ04D.h>

using namespace RobotKinematics;

/// Internal helpers for this widget: robot-config construction, pose <-> RPY conversion, status
/// text, and the self-collision check used by the FK/IK tester. Not part of the public API.
namespace {

// The new RobotKinematics component ships a single built-in C++ preset that is
// relevant here. Custom-preset authoring is out of scope for this widget.
constexpr char kNachiMz04dPreset[] = "Nachi MZ04D";
constexpr char kPickingToolId[]    = "picking_tcp";

/// Posture branch labels for a preset's axes (axis -> {negative, positive} labels).
/// @param presetName robot preset name; only kNachiMz04dPreset is recognised
/// @return the preset's posture labels, or an empty map when `presetName` is unknown (drives the
/// pick-path posture combo boxes)
std::map<std::string, std::pair<QString, QString>> postureLabels(const QString &presetName) {
    std::map<std::string, std::pair<QString, QString>> out;
    if (presetName != QLatin1String(kNachiMz04dPreset))
        return out;
    const SerialRobotConfig cfg = Presets::nachiMZ04D();
    for (const auto &kv : cfg.posture.labels) {
        out[kv.first] = { QString::fromStdString(kv.second.negative),
                          QString::fromStdString(kv.second.positive) };
    }
    return out;
}

/// Builds the flange -> TCP transform from the widget's mm/deg offset fields.
Pose flangeToTcpPose(const vc::device::RobotKinematicCheckConfig &cfg) {
    return Pose::fromXYZRPY_mm_deg(cfg.tcpX, cfg.tcpY, cfg.tcpZ,
                                   cfg.tcpRoll, cfg.tcpPitch, cfg.tcpYaw);
}

/// Builds the selected robot config with the picking TCP appended as the default tool.
/// @param cfg widget config supplying the preset name and TCP offset
/// @param out output robot config; populated only on success
/// @return false when `cfg.presetName` is not a recognised preset (leaves `out` untouched)
bool buildRobotConfig(const vc::device::RobotKinematicCheckConfig &cfg,
                      SerialRobotConfig &out) {
    if (cfg.presetName != QLatin1String(kNachiMz04dPreset))
        return false;

    out = Presets::nachiMZ04D();
    Tool tool;
    tool.id          = kPickingToolId;
    tool.name        = cfg.tcpName.toStdString();
    tool.flangeToTcp = flangeToTcpPose(cfg);
    out.tools.push_back(tool);
    out.defaultToolId = kPickingToolId;
    return true;
}

/// Extracts roll/pitch/yaw, in degrees, from a pose. Inverse of Pose::fromXYZRPY, which composes
/// R = Rz(yaw) * Ry(pitch) * Rx(roll).
/// @return (roll, pitch, yaw) in degrees
Eigen::Vector3d rpyDegFromPose(const Pose &pose) {
    const Eigen::Matrix3d r = pose.isometry().rotation();
    const double pitch = std::atan2(-r(2, 0), std::sqrt(r(0, 0) * r(0, 0) + r(1, 0) * r(1, 0)));
    const double yaw   = std::atan2(r(1, 0), r(0, 0));
    const double roll  = std::atan2(r(2, 1), r(2, 2));
    return Eigen::Vector3d(units::toDeg(roll), units::toDeg(pitch), units::toDeg(yaw));
}

/// Maps a KinematicsStatus to a short lower-case human-readable string for the tester status
/// label (e.g. "target unreachable", "singularity").
QString statusToString(KinematicsStatus s) {
    switch (s) {
    case KinematicsStatus::Ok:                          return QStringLiteral("ok");
    case KinematicsStatus::InvalidRobotConfig:          return QStringLiteral("invalid robot config");
    case KinematicsStatus::InvalidRequest:              return QStringLiteral("invalid request");
    case KinematicsStatus::FrameNotFound:               return QStringLiteral("frame not found");
    case KinematicsStatus::ToolNotFound:                return QStringLiteral("tool not found");
    case KinematicsStatus::JointDimensionMismatch:      return QStringLiteral("joint dimension mismatch");
    case KinematicsStatus::JointLimitViolation:         return QStringLiteral("joint limit violation");
    case KinematicsStatus::TargetUnreachable:           return QStringLiteral("target unreachable");
    case KinematicsStatus::Singularity:                 return QStringLiteral("singularity");
    case KinematicsStatus::MaxIterationsReached:        return QStringLiteral("max iterations reached");
    case KinematicsStatus::NoConvergedSolution:         return QStringLiteral("no converged solution");
    case KinematicsStatus::PostureConstraintUnsatisfied:return QStringLiteral("posture constraint unsatisfied");
    case KinematicsStatus::UnsupportedSolver:           return QStringLiteral("unsupported solver");
    case KinematicsStatus::NumericalError:              return QStringLiteral("numerical error");
    }
    return QStringLiteral("unknown");
}

// ── Self-collision check (simplified mesh, Coal backend) ────────────────────
// The tester always uses the simplified (voxel) Nachi MZ04D mesh profile — fast
// enough for an interactive click and accurate enough for a reachability gate.
constexpr char kSimplifiedProfileDeployedRel[] =
    "robot_assets/Nachi/MZ04/nachi_mz04d_mesh_collision_simplified.json";
constexpr char kSimplifiedProfileSourceRel[] =
    "components/RobotKinematics/presets/Nachi/MZ04/nachi_mz04d_mesh_collision_simplified.json";

/// Source-tree fallback resolver for a deployed asset (dev runs from source): walks up from the
/// app's current/executable directory looking for `relative`. Deployed runs find the asset next
/// to the binary before this is called.
/// @param relative asset path, relative to a deployed install root or a source-tree root
/// @return the resolved absolute path, or an empty string if `relative` isn't found under any
/// candidate root
QString resolveAssetPath(const QString &relative) {
    const QFileInfo direct(relative);
    if (direct.exists()) return direct.absoluteFilePath();

    QStringList roots;
    roots << QDir::currentPath() << QCoreApplication::applicationDirPath();
    QDir walker(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 8 && walker.cdUp(); ++i)
        roots << walker.absolutePath();
    roots.removeDuplicates();

    for (const QString &root : roots) {
        const QFileInfo candidate(QDir(root).filePath(relative));
        if (candidate.exists()) return candidate.absoluteFilePath();
    }
    return {};
}

/// Lazily loaded, shared simplified mesh-collision profile: tries the deployed asset path first,
/// then falls back to resolveAssetPath() for source-tree dev runs; loaded once (function-local
/// static) and reused for every call.
/// @return pointer to the shared profile, or nullptr if the JSON asset could not be found/parsed
const MeshCollisionProfile *simplifiedMeshProfile() {
    static const std::optional<MeshCollisionProfile> profile =
        []() -> std::optional<MeshCollisionProfile> {
            QString path;
            const QFileInfo deployed(QDir(QCoreApplication::applicationDirPath())
                                         .filePath(QString::fromLatin1(kSimplifiedProfileDeployedRel)));
            if (deployed.exists())
                path = deployed.absoluteFilePath();
            else
                path = resolveAssetPath(QString::fromLatin1(kSimplifiedProfileSourceRel));
            if (path.isEmpty())
                return std::nullopt;
            const Result<MeshCollisionProfile> res =
                MeshCollisionProfileJsonLoader::loadFile(path.toStdString());
            if (!res.ok())
                return std::nullopt;
            return res.value;
        }();
    return profile ? &(*profile) : nullptr;
}

/// Outcome of a self-collision check: disabled by config, the mesh profile couldn't be loaded, the
/// backend check itself failed, or the check ran and found the pose collision-free/colliding.
enum class CollisionState { Disabled, Unavailable, Error, Free, Colliding };

/// Runs the simplified-mesh self-collision check for the given joints, using the Coal backend.
/// @param cfg widget config; `collisionCheckEnabled` gates whether the check runs at all
/// @param robot robot config to check against
/// @param joints joint pose to test for self-collision
/// @return Disabled when the operator has not enabled the collision check, Unavailable when the
/// mesh profile could not be loaded, Error when the backend check failed, otherwise
/// Free/Colliding per the result
CollisionState checkSelfCollision(const vc::device::RobotKinematicCheckConfig &cfg,
                                  const SerialRobotConfig &robot,
                                  const JointVector &joints) {
    if (!cfg.collisionCheckEnabled) return CollisionState::Disabled;
    const MeshCollisionProfile *profile = simplifiedMeshProfile();
    if (!profile) return CollisionState::Unavailable;

    MeshCollisionCheckRequest req;
    req.joints = joints;
    const CollisionCheckResult res = CollisionBackends::checkMesh(robot, *profile, req);
    if (!res.ok()) return CollisionState::Error;
    return res.hasCollision ? CollisionState::Colliding : CollisionState::Free;
}

/// Builds the status-label suffix describing a collision result (e.g. " | SELF-COLLISION"); empty
/// when the check is disabled.
QString collisionSuffix(CollisionState s) {
    switch (s) {
    case CollisionState::Disabled:    return {};
    case CollisionState::Unavailable: return QObject::tr(" | collision: profile unavailable");
    case CollisionState::Error:       return QObject::tr(" | collision: check error");
    case CollisionState::Free:        return QObject::tr(" | collision-free");
    case CollisionState::Colliding:   return QObject::tr(" | SELF-COLLISION");
    }
    return {};
}

} // namespace

/// Constructs the widget: builds the UI from the .ui form, seeds the robot-preset combo with the
/// single built-in preset (Nachi MZ04D), sets up the pick-path table headers, and wires all
/// editing controls to notifyConfigChanged() plus the FK/IK tester buttons.
RobotKinematicCheckWidget::RobotKinematicCheckWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::RobotKinematicCheckWidget) {

    ui->setupUi(this);   // structure from robot_kinematic_check_widget.ui

    // Data-driven combo content: built-in robot presets (Rule 1.2). The new
    // RobotKinematics component ships Nachi MZ04D as its only relevant preset.
    ui->cbb_robot_preset->addItem(QString::fromLatin1(kNachiMz04dPreset));
    m_currentPreset = ui->cbb_robot_preset->currentText();

    // Pick-path table columns.
    const QStringList pathHeaders = {
        tr("#"), tr("dX (mm)"), tr("dY (mm)"), tr("dZ (mm)"),
        tr("dRoll (deg)"), tr("dPitch (deg)"), tr("dYaw (deg)"),
        tr("Shoulder"), tr("Elbow"), tr("Wrist") };
    ui->tbl_pick_path->setColumnCount(pathHeaders.size());
    ui->tbl_pick_path->setHorizontalHeaderLabels(pathHeaders);
    ui->tbl_pick_path->verticalHeader()->setVisible(false);

    // ── Behaviour: any edit emits configChanged (unless we are loading) ─────
    auto emitChanged = [this]() { notifyConfigChanged(); };
    connect(ui->chk_kinematic_enable, &QCheckBox::toggled, this, emitChanged);
    connect(ui->chk_collision_enable, &QCheckBox::toggled, this, emitChanged);
    connect(ui->cbb_robot_preset, &QComboBox::currentTextChanged,
            this, &RobotKinematicCheckWidget::onPresetChanged);
    connect(ui->ledit_tcp_name, &QLineEdit::editingFinished, this, emitChanged);
    for (QDoubleSpinBox *s : { ui->spb_tcp_x, ui->spb_tcp_y, ui->spb_tcp_z,
                               ui->spb_tcp_roll, ui->spb_tcp_pitch, ui->spb_tcp_yaw })
        connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, emitChanged);

    connect(ui->btn_path_add, &QPushButton::clicked,
            this, &RobotKinematicCheckWidget::onAddPathRow);
    connect(ui->btn_path_remove, &QPushButton::clicked,
            this, &RobotKinematicCheckWidget::onRemovePathRow);

    // ── FK / IK tester (scratchpad, not part of the saved config) ───────────
    // The tester group is hidden until requested. Its spin boxes do NOT emit
    // configChanged — they only drive the manual forward/inverse computations.
    // connect(ui->chk_show_tester, &QCheckBox::toggled,
    //         ui->grp_kinematic_tester, &QWidget::setVisible);
    connect(ui->chk_show_tester, &QCheckBox::toggled,
            this, [this]() {
        ui->grp_kinematic_tester->setVisible(ui->chk_show_tester->isChecked());
        emit testerWidgetVisibleChanged();
    });
    connect(ui->btn_compute_fk, &QPushButton::clicked,
            this, &RobotKinematicCheckWidget::onComputeForward);
    connect(ui->btn_compute_ik, &QPushButton::clicked,
            this, &RobotKinematicCheckWidget::onComputeInverse);

    ui->grp_kinematic_tester->setVisible(ui->chk_show_tester->isChecked());
}

/// Destroys the widget, deleting the generated `ui` object.
RobotKinematicCheckWidget::~RobotKinematicCheckWidget() {
    delete ui;
}

/// Loads `cfg` into the UI controls: enable flags, preset selection, TCP name/offset, and the
/// pick-path table (rebuilt from scratch).
/// @note Sets `m_loading` for the duration of the load so control-changed signals don't re-emit
/// configChanged() while the form is being populated.
void RobotKinematicCheckWidget::setConfig(const vc::device::RobotKinematicCheckConfig &cfg) {
    m_loading = true;

    ui->chk_kinematic_enable->setChecked(cfg.enabled);
    ui->chk_collision_enable->setChecked(cfg.collisionCheckEnabled);
    if (!cfg.presetName.isEmpty()) {
        const int idx = ui->cbb_robot_preset->findText(cfg.presetName);
        if (idx >= 0) ui->cbb_robot_preset->setCurrentIndex(idx);
    }
    ui->ledit_tcp_name->setText(cfg.tcpName);
    ui->spb_tcp_x->setValue(cfg.tcpX);
    ui->spb_tcp_y->setValue(cfg.tcpY);
    ui->spb_tcp_z->setValue(cfg.tcpZ);
    ui->spb_tcp_roll->setValue(cfg.tcpRoll);
    ui->spb_tcp_pitch->setValue(cfg.tcpPitch);
    ui->spb_tcp_yaw->setValue(cfg.tcpYaw);

    ui->tbl_pick_path->setRowCount(0);
    for (const vc::device::PickPathPoint &p : cfg.pickPath)
        addPathRow(p);

    m_currentPreset = ui->cbb_robot_preset->currentText();
    m_loading = false;
}

/// Reads the current UI state back into a RobotKinematicCheckConfig: enable flags, preset, TCP
/// offset, and every pick-path row's deltas and posture selections.
vc::device::RobotKinematicCheckConfig RobotKinematicCheckWidget::config() const {
    vc::device::RobotKinematicCheckConfig cfg;
    cfg.enabled               = ui->chk_kinematic_enable->isChecked();
    cfg.collisionCheckEnabled = ui->chk_collision_enable->isChecked();
    cfg.presetName            = ui->cbb_robot_preset->currentText();
    cfg.tcpName               = ui->ledit_tcp_name->text().trimmed();
    cfg.tcpX = ui->spb_tcp_x->value();
    cfg.tcpY = ui->spb_tcp_y->value();
    cfg.tcpZ = ui->spb_tcp_z->value();
    cfg.tcpRoll  = ui->spb_tcp_roll->value();
    cfg.tcpPitch = ui->spb_tcp_pitch->value();
    cfg.tcpYaw   = ui->spb_tcp_yaw->value();

    const auto spinValue = [this](int row, int col) {
        auto *s = qobject_cast<QDoubleSpinBox *>(ui->tbl_pick_path->cellWidget(row, col));
        return s ? s->value() : 0.0;
    };
    const auto comboLabel = [this](int row, int col) {
        auto *c = qobject_cast<QComboBox *>(ui->tbl_pick_path->cellWidget(row, col));
        return c ? c->currentData().toString() : QString();
    };
    for (int row = 0; row < ui->tbl_pick_path->rowCount(); ++row) {
        vc::device::PickPathPoint p;
        p.dx = spinValue(row, 1); p.dy = spinValue(row, 2); p.dz = spinValue(row, 3);
        p.dRoll = spinValue(row, 4); p.dPitch = spinValue(row, 5); p.dYaw = spinValue(row, 6);
        p.shoulder = comboLabel(row, 7);
        p.elbow    = comboLabel(row, 8);
        p.wrist    = comboLabel(row, 9);
        cfg.pickPath.append(p);
    }
    return cfg;
}

/// Emits configChanged(), unless a setConfig() load is currently in progress (`m_loading`).
void RobotKinematicCheckWidget::notifyConfigChanged() {
    if (m_loading) return;
    emit configChanged();
}

/// (Re)populates a posture combo box with "(any)" plus the negative/positive branch labels for
/// `axis` under the currently selected preset, restoring `current` as the selection if it is
/// still one of the offered labels (falls back to "(any)" otherwise).
void RobotKinematicCheckWidget::populatePostureCombo(QComboBox *combo, const QString &axis,
                                                     const QString &current) {
    QSignalBlocker block(combo);
    combo->clear();
    combo->addItem(tr("(any)"), QString());   // userData "" => any branch
    const auto labels = postureLabels(ui->cbb_robot_preset->currentText());
    const auto it = labels.find(axis.toStdString());
    if (it != labels.end()) {
        combo->addItem(it->second.first, it->second.first);     // negative branch
        combo->addItem(it->second.second, it->second.second);   // positive branch
    }
    const int idx = combo->findData(current);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
}

/// Appends one pick-path row to the table: a read-only index cell, six delta spin boxes (dX..dYaw,
/// each wired to notifyConfigChanged()), and three posture combo boxes (shoulder/elbow/wrist,
/// populated via populatePostureCombo() and wired to notifyConfigChanged()).
void RobotKinematicCheckWidget::addPathRow(const vc::device::PickPathPoint &point) {
    const int row = ui->tbl_pick_path->rowCount();
    ui->tbl_pick_path->insertRow(row);

    auto *idxItem = new QTableWidgetItem(QString::number(row + 1));
    idxItem->setFlags(idxItem->flags() & ~Qt::ItemIsEditable);
    idxItem->setTextAlignment(Qt::AlignCenter);
    ui->tbl_pick_path->setItem(row, 0, idxItem);

    const double values[6] = { point.dx, point.dy, point.dz,
                               point.dRoll, point.dPitch, point.dYaw };
    const double minimums[6] = { -100000.0, -100000.0, -100000.0, -360.0, -360.0, -360.0 };
    const double maximums[6] = {  100000.0,  100000.0,  100000.0,  360.0,  360.0,  360.0 };
    for (int c = 0; c < 6; ++c) {
        auto *spin = new QDoubleSpinBox;
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setDecimals(2);
        spin->setRange(minimums[c], maximums[c]);
        spin->setValue(values[c]);
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, &RobotKinematicCheckWidget::notifyConfigChanged);
        ui->tbl_pick_path->setCellWidget(row, 1 + c, spin);
    }

    const QString labels[3] = { point.shoulder, point.elbow, point.wrist };
    const char *axes[3]     = { "shoulder", "elbow", "wrist" };
    for (int a = 0; a < 3; ++a) {
        auto *combo = new QComboBox;
        populatePostureCombo(combo, QString::fromLatin1(axes[a]), labels[a]);
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, &RobotKinematicCheckWidget::notifyConfigChanged);
        ui->tbl_pick_path->setCellWidget(row, 7 + a, combo);
    }
}

/// Renumbers the pick-path table's index column (1-based) to match each row's current position.
void RobotKinematicCheckWidget::renumberPathRows() {
    for (int row = 0; row < ui->tbl_pick_path->rowCount(); ++row) {
        if (auto *item = ui->tbl_pick_path->item(row, 0))
            item->setText(QString::number(row + 1));
    }
}

/// Adds a blank pick-path row and notifies listeners of the change.
void RobotKinematicCheckWidget::onAddPathRow() {
    addPathRow(vc::device::PickPathPoint{});
    notifyConfigChanged();
}

/// Removes the currently selected pick-path row(s) (highest row index first, so removing one row
/// doesn't invalidate the indices of the others still pending removal), renumbers the remaining
/// rows, and notifies listeners of the change.
void RobotKinematicCheckWidget::onRemovePathRow() {
    const QModelIndexList selected =
        ui->tbl_pick_path->selectionModel()->selectedRows();
    QVector<int> rows;
    for (const QModelIndex &idx : selected)
        rows.append(idx.row());
    std::sort(rows.begin(), rows.end(), [](int a, int b) { return a > b; });
    for (int row : rows)
        ui->tbl_pick_path->removeRow(row);
    renumberPathRows();
    notifyConfigChanged();
}

/// Handles a robot-preset combo change: if the pick-path table has rows, asks for confirmation
/// before clearing it (posture labels are preset-specific and would no longer apply), reverting
/// the combo selection if the user declines; otherwise accepts the new preset and notifies
/// listeners of the change.
void RobotKinematicCheckWidget::onPresetChanged(const QString &preset) {
    if (m_loading || preset == m_currentPreset) {
        notifyConfigChanged();
        return;
    }

    // Posture labels are preset-specific; switching preset invalidates any
    // configured pick-path. Confirm before discarding it.
    if (ui->tbl_pick_path->rowCount() > 0) {
        const auto answer = QMessageBox::question(
            this, tr("Change robot preset"),
            tr("Changing the robot preset will clear the configured pick-path "
               "points (posture labels differ per preset). Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            QSignalBlocker block(ui->cbb_robot_preset);
            ui->cbb_robot_preset->setCurrentText(m_currentPreset);
            return;
        }
        ui->tbl_pick_path->setRowCount(0);
    }

    m_currentPreset = preset;
    notifyConfigChanged();
}

/// Runs forward kinematics for the tester: builds a robot config from the current preset and TCP
/// offset, computes the TCP pose for the joint angles in the tester's spin boxes, writes the
/// resulting pose (mm/deg) back to the pose spin boxes, and reports the result -- plus a
/// self-collision check -- in the tester status label. Requires a recognised, 6-axis preset;
/// otherwise shows an error in the status label instead of computing.
void RobotKinematicCheckWidget::onComputeForward() {
    const vc::device::RobotKinematicCheckConfig cfg = config();
    SerialRobotConfig robot;
    if (!buildRobotConfig(cfg, robot)) {
        ui->lbl_tester_status->setText(tr("FK: select a robot preset first"));
        return;
    }
    if (ForwardKinematics::movableJointCount(robot) != 6) {
        ui->lbl_tester_status->setText(tr("FK: preset is not a 6-axis arm"));
        return;
    }

    const JointVector q = JointVector::fromDegrees({
        ui->spb_j1->value(), ui->spb_j2->value(), ui->spb_j3->value(),
        ui->spb_j4->value(), ui->spb_j5->value(), ui->spb_j6->value() });

    const Pose tcpPose = ForwardKinematics::toolPose(robot, q, flangeToTcpPose(cfg));
    const Eigen::Vector3d t   = tcpPose.translation_m();
    const Eigen::Vector3d rpy = rpyDegFromPose(tcpPose);

    ui->spb_pose_x->setValue(units::toMm(t.x()));
    ui->spb_pose_y->setValue(units::toMm(t.y()));
    ui->spb_pose_z->setValue(units::toMm(t.z()));
    ui->spb_pose_roll->setValue(rpy.x());
    ui->spb_pose_pitch->setValue(rpy.y());
    ui->spb_pose_yaw->setValue(rpy.z());

    const CollisionState collision = checkSelfCollision(cfg, robot, q);
    ui->lbl_tester_status->setText(tr("FK: ok") + collisionSuffix(collision));
}

/// Runs inverse kinematics for the tester: builds a robot config from the current preset and TCP
/// offset, solves for the target pose in the tester's pose spin boxes, writes the best solution's
/// joints (degrees) back to the joint spin boxes, and reports the result -- plus a self-collision
/// check -- in the tester status label. Requires a recognised preset; otherwise shows an error in
/// the status label instead of solving.
void RobotKinematicCheckWidget::onComputeInverse() {
    const vc::device::RobotKinematicCheckConfig cfg = config();
    SerialRobotConfig robot;
    if (!buildRobotConfig(cfg, robot)) {
        ui->lbl_tester_status->setText(tr("IK: select a robot preset first"));
        return;
    }

    IKRequest req;
    req.targetPose = Pose::fromXYZRPY_mm_deg(
        ui->spb_pose_x->value(), ui->spb_pose_y->value(), ui->spb_pose_z->value(),
        ui->spb_pose_roll->value(), ui->spb_pose_pitch->value(), ui->spb_pose_yaw->value());
    req.tool = ToolId{kPickingToolId};

    const IKResult ik = SerialRobotKinematics(robot).solve(req);
    if (!ik.ok()) {
        ui->lbl_tester_status->setText(tr("IK: %1").arg(statusToString(ik.status)));
        return;
    }

    // Show the best solution's joints (degrees).
    const std::vector<double> sol = ik.best().joints.toDegrees();
    QDoubleSpinBox *joints[6] = { ui->spb_j1, ui->spb_j2, ui->spb_j3,
                                  ui->spb_j4, ui->spb_j5, ui->spb_j6 };
    for (int i = 0; i < 6 && i < static_cast<int>(sol.size()); ++i)
        joints[i]->setValue(sol[i]);

    const CollisionState collision = checkSelfCollision(cfg, robot, ik.best().joints);
    ui->lbl_tester_status->setText(
        tr("IK: %1 solution(s) — %2")
            .arg(static_cast<int>(ik.solutions.size()))
            .arg(statusToString(ik.status))
        + collisionSuffix(collision));
}
