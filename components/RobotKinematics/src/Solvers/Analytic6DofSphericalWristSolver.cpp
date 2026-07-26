#include <RobotKinematics/Solvers/Analytic6DofSphericalWristSolver.h>

#include <RobotKinematics/Kinematics/ForwardKinematics.h>

#include <Eigen/Dense>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

/// Robot kinematics library: forward/inverse kinematics types, models, and solvers for
/// serial robot manipulators.
namespace RobotKinematics {

/// Internal helpers, tolerances, and geometry-extraction routines used only within this
/// translation unit to implement the analytic spherical-wrist IK solver.
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;  ///< Value of Pi (double precision), used for angle-wrapping.
constexpr double kGeomTol = 1e-6;      ///< Tolerance in meters/radians for morphology checks (axis intersection, perpendicularity, parallelism).
constexpr double kAcceptPos = 1e-6;    ///< Position-error tolerance (meters) below which an FK-verified candidate solution is accepted.
constexpr double kAcceptOri = 1e-6;    ///< Orientation-error tolerance (radians) below which an FK-verified candidate solution is accepted.
constexpr double kDupTol = 1e-6;       ///< Joint-vector norm tolerance below which two candidate solutions are treated as duplicates.

/// Collects pointers to the configuration's movable joints (Revolute or Prismatic) in
/// declaration order, skipping fixed joints.
/// @param config the serial robot configuration to scan
/// @return pointers into `config.joints` for each movable joint, in order
std::vector<const Joint*> movableJoints(const SerialRobotConfig& config)
{
    std::vector<const Joint*> joints;
    for (const Joint& joint : config.joints) {
        if (joint.type == JointType::Revolute || joint.type == JointType::Prismatic) {
            joints.push_back(&joint);
        }
    }
    return joints;
}

/// Builds the joint vector at the robot's home configuration (each movable joint's
/// `home` value), in the same order as movableJoints().
/// @param config the serial robot configuration
/// @return the home joint vector, sized to the movable joint count
JointVector homeJoint(const SerialRobotConfig& config)
{
    Eigen::VectorXd q(ForwardKinematics::movableJointCount(config));
    int index = 0;
    for (const Joint& joint : config.joints) {
        if (joint.type == JointType::Revolute || joint.type == JointType::Prismatic) {
            q[index++] = joint.home;
        }
    }
    return JointVector(q);
}

/// Computes the least-squares point closest to a set of axis lines (each defined by a
/// point and a direction).
/// @param points a point on each axis line
/// @param dirs the direction of each axis line (need not be pre-normalized)
/// @param out output; set to the least-squares closest point on success
/// @return false if the normal-equations matrix is singular (e.g. parallel/degenerate axes)
bool closestPointToAxes(const std::vector<Eigen::Vector3d>& points,
                        const std::vector<Eigen::Vector3d>& dirs,
                        Eigen::Vector3d& out)
{
    Eigen::Matrix3d M = Eigen::Matrix3d::Zero();
    Eigen::Vector3d b = Eigen::Vector3d::Zero();
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Eigen::Vector3d u = dirs[i].normalized();
        const Eigen::Matrix3d A = Eigen::Matrix3d::Identity() - u * u.transpose();
        M += A;
        b += A * points[i];
    }
    const Eigen::FullPivLU<Eigen::Matrix3d> lu(M);
    if (!lu.isInvertible()) {
        return false;
    }
    out = lu.solve(b);
    return true;
}

/// Computes the perpendicular distance from point `x` to the line through `p` with
/// direction `u`.
/// @param x the point to measure from
/// @param p a point on the line
/// @param u the line direction (need not be pre-normalized)
/// @return the perpendicular distance
double distanceToAxis(const Eigen::Vector3d& x, const Eigen::Vector3d& p, const Eigen::Vector3d& u)
{
    const Eigen::Vector3d n = u.normalized();
    return ((Eigen::Matrix3d::Identity() - n * n.transpose()) * (x - p)).norm();
}

/// All geometry needed to solve a standard articulated spherical-wrist 6R, extracted from
/// the canonical model at the home configuration. valid == false => unsupported morphology.
struct ArmWristGeometry {
    bool valid = false;  ///< True once the model has been confirmed to match the supported articulated spherical-wrist 6R morphology.
    std::string link3Id;  ///< Link id of joint 3's child link (elbow link), used to query its pose via linkPose().
    Eigen::Vector3d shoulder;   ///< Intersection point of axis 1 and axis 2 (shoulder point), in base frame at home.
    Eigen::Vector3d u1;         ///< Axis 1 direction (unit vector), in base frame at home.
    Eigen::Vector3d radial0;    ///< Home radial direction (horizontal, in arm plane), perpendicular to u1.
    Eigen::Vector3d wristInFlange; ///< Wrist center expressed in the flange frame; constant across configurations.
    double upperArm = 0.0;      ///< Shoulder -> elbow length (m).
    double foreArm = 0.0;       ///< Elbow -> wrist-center length (m).
    double beta1Home = 0.0;     ///< Home upper-arm plane angle (angle of the shoulder->elbow vector in the arm plane).
    double elbowHome = 0.0;     ///< Home forearm-relative plane angle (elbow angle relative to the upper arm at home).
    double s2 = 1.0;            ///< Sign mapping the plane-angle delta to joint 2 (q2 = s2 * (beta1 - beta1Home)).
    double s3 = 1.0;            ///< Sign mapping the plane-angle delta to joint 3 (q3 = s3 * (E - elbowHome)).
    double mu = 0.0;            ///< Wrist-reduction offset applied to q4 in the N = Rz(q4+mu) Ry(q5) Rz(q6+delta-mu) decomposition.
    double delta = 0.0;         ///< Wrist-reduction offset applied to q6 in the N = Rz(q4+mu) Ry(q5) Rz(q6+delta-mu) decomposition.
    Eigen::Matrix3d A4 = Eigen::Matrix3d::Identity(); ///< Rotation part of joint-4's origin transform (link3 frame -> joint-4 frame).
};

/// Computes the forward-kinematics chain for joint vector `q` and returns the pose of
/// link `linkId` in the base frame.
/// @param config the serial robot configuration
/// @param q the full 6-joint vector to evaluate the chain at
/// @param linkId the link whose pose is requested
/// @return the link's pose in base frame, or Pose::identity() if `linkId` is not found in the chain
Pose linkPose(const SerialRobotConfig& config, const Eigen::Matrix<double, 6, 1>& q,
              const std::string& linkId)
{
    const FkChain chain = ForwardKinematics::computeChain(config, JointVector(Eigen::VectorXd(q)));
    const auto it = chain.linkPosesInBase.find(linkId);
    return it == chain.linkPosesInBase.end() ? Pose::identity() : it->second;
}

/// Extracts and validates the articulated spherical-wrist 6R geometry (shoulder point,
/// upper-arm/forearm lengths, home plane angles, q2/q3 sign conventions, and wrist-reduction
/// mu/delta offsets) from the robot's home configuration by sampling its forward kinematics.
/// Rejects the model (returns `.valid == false`) unless it has exactly 6 movable joints,
/// wrist axes 4/5/6 intersect at a common point, shoulder axes 1/2 intersect with axis 1
/// perpendicular to axis 2 and axis 2 parallel to axis 3, the elbow is off the J1 axis, and
/// the wrist reduction decomposes as a supported Rz-Ry-Rz rotation.
/// @param config the serial robot configuration to analyze
/// @return the extracted geometry; check `.valid` before using any other field
ArmWristGeometry extractGeometry(const SerialRobotConfig& config)
{
    ArmWristGeometry g;
    const std::vector<const Joint*> mv = movableJoints(config);
    if (mv.size() != 6) {
        return g;
    }
    g.link3Id = mv[2]->childLinkId;

    const FkChain home = ForwardKinematics::computeChain(config, homeJoint(config));
    if (home.joints.size() != 6) {
        return g;
    }
    std::array<Eigen::Vector3d, 6> axis;
    std::array<Eigen::Vector3d, 6> point;
    for (int i = 0; i < 6; ++i) {
        axis[i] = home.joints[i].axisInBase.normalized();
        point[i] = home.joints[i].originInBase;
    }

    // Spherical wrist: axes 4,5,6 intersect.
    Eigen::Vector3d wristCenter;
    if (!closestPointToAxes({point[3], point[4], point[5]}, {axis[3], axis[4], axis[5]}, wristCenter)) {
        return g;
    }
    for (int i = 3; i < 6; ++i) {
        if (distanceToAxis(wristCenter, point[i], axis[i]) > kGeomTol) {
            return g;
        }
    }

    // Articulated arm: axes 1,2 intersect (no shoulder offset); axis 1 _|_ axis 2; axis 2 || axis 3.
    if (!closestPointToAxes({point[0], point[1]}, {axis[0], axis[1]}, g.shoulder)) {
        return g;
    }
    if (distanceToAxis(g.shoulder, point[1], axis[1]) > kGeomTol
        || std::abs(axis[0].dot(axis[1])) > kGeomTol
        || axis[1].cross(axis[2]).norm() > kGeomTol) {
        return g;
    }
    g.u1 = axis[0];

    const Pose link3Home = linkPose(config, Eigen::Matrix<double, 6, 1>::Zero(), g.link3Id);
    const Eigen::Vector3d elbowHome = link3Home.translation_m();
    const Eigen::Matrix3d R03Home = link3Home.isometry().rotation();

    Eigen::Vector3d radial = (elbowHome - g.shoulder);
    radial -= radial.dot(g.u1) * g.u1;
    if (radial.norm() < kGeomTol) {
        return g; // degenerate (elbow on the J1 axis)
    }
    g.radial0 = radial.normalized();

    g.upperArm = (elbowHome - g.shoulder).norm();
    g.foreArm = (wristCenter - elbowHome).norm();

    const auto plane = [&](const Eigen::Vector3d& p) {
        return Eigen::Vector2d((p - g.shoulder).dot(g.radial0), (p - g.shoulder).dot(g.u1));
    };
    const Eigen::Vector2d e0 = plane(elbowHome);
    const Eigen::Vector2d w0 = plane(wristCenter);
    g.beta1Home = std::atan2(e0.y(), e0.x());
    g.elbowHome = std::atan2(w0.y() - e0.y(), w0.x() - e0.x()) - g.beta1Home;

    // Sign calibration for q2 -> upper-arm angle and q3 -> elbow angle.
    const Eigen::Vector3d wristInLink3 = R03Home.transpose() * (wristCenter - elbowHome);
    Eigen::Matrix<double, 6, 1> qd2 = Eigen::Matrix<double, 6, 1>::Zero();
    qd2[1] = 0.1;
    const Eigen::Vector2d e2 = plane(linkPose(config, qd2, g.link3Id).translation_m());
    g.s2 = ((std::atan2(e2.y(), e2.x()) - g.beta1Home) >= 0.0) ? 1.0 : -1.0;

    Eigen::Matrix<double, 6, 1> qd3 = Eigen::Matrix<double, 6, 1>::Zero();
    qd3[2] = 0.1;
    const Pose link3d3 = linkPose(config, qd3, g.link3Id);
    const Eigen::Vector3d elbow3 = link3d3.translation_m();
    const Eigen::Vector3d wrist3 = link3d3.isometry().rotation() * wristInLink3 + elbow3;
    const Eigen::Vector2d e3 = plane(elbow3);
    const Eigen::Vector2d w3 = plane(wrist3);
    const double elbow3Angle = std::atan2(w3.y() - e3.y(), w3.x() - e3.x()) - std::atan2(e3.y(), e3.x());
    g.s3 = ((elbow3Angle - g.elbowHome) >= 0.0) ? 1.0 : -1.0;

    // Wrist reduction: N = A4^T R36 = Rz(q4+mu) Ry(q5) Rz(q6+delta-mu).
    g.A4 = mv[3]->origin.isometry().rotation();
    const Eigen::Matrix3d A5 = mv[4]->origin.isometry().rotation();
    const Eigen::Matrix3d A6 = mv[5]->origin.isometry().rotation();
    const Eigen::Vector3d m = A5 * Eigen::Vector3d::UnitZ();
    if (std::abs(m.z()) > kGeomTol) {
        return g; // J5 axis not perpendicular to J4 -> unsupported reduction
    }
    g.mu = std::atan2(-m.x(), m.y());
    const Eigen::Matrix3d D = A5 * A6;
    if (std::abs(D(0, 2)) > kGeomTol || std::abs(D(1, 2)) > kGeomTol || std::abs(D(2, 2) - 1.0) > kGeomTol) {
        return g; // A5*A6 is not a rotation about z -> unsupported reduction
    }
    g.delta = std::atan2(D(1, 0), D(0, 0));

    // Wrist center in the flange frame (constant): T_home^-1 * wristCenter.
    g.wristInFlange = home.flangeInBase.isometry().inverse() * wristCenter;

    g.valid = true;
    return g;
}

/// Computes the two ZYZ Euler-angle branches (q5 >= 0 and q5 <= 0) that decompose
/// N = Rz(A) Ry(q5) Rz(B), then maps each branch to joint angles (q4,q5,q6) via the
/// mu/delta wrist-reduction offsets.
/// @param N the wrist relative-rotation matrix to decompose (e.g. A4^T * R03^T * R_target)
/// @param mu wrist-reduction offset subtracted from A to form q4
/// @param delta wrist-reduction offset combined with mu and subtracted from B to form q6
/// @param out output; out[0] is the q5>=0 branch, out[1] is the q5<=0 branch
void wristAngles(const Eigen::Matrix3d& N, double mu, double delta,
                 std::array<Eigen::Vector3d, 2>& out)
{
    for (int b = 0; b < 2; ++b) {
        const double sign = (b == 0) ? 1.0 : -1.0;
        const double q5 = std::atan2(sign * std::hypot(N(0, 2), N(1, 2)), N(2, 2));
        double A;
        double B;
        if (std::abs(std::sin(q5)) < 1e-8) {
            A = 0.0;
            B = std::atan2(N(1, 0), N(0, 0));
        } else {
            A = std::atan2(sign * N(1, 2), sign * N(0, 2));
            B = std::atan2(sign * N(2, 1), -sign * N(2, 0));
        }
        out[b] = Eigen::Vector3d(A - mu, q5, B - delta + mu);
    }
}

/// Wraps angle `q` by a multiple of 2*pi (preferring the unshifted value) to bring it
/// within `limits`, if given.
/// @param q the candidate joint angle (radians)
/// @param limits optional joint limits; if absent, `q` is returned unchanged and `ok` is true
/// @param ok output; true if a representation of `q` within limits was found
/// @return `q` (or `q` shifted by +-2*pi) within limits, or the original `q` if none fits
double wrapToLimits(double q, const std::optional<JointLimits>& limits, bool& ok)
{
    ok = true;
    if (!limits.has_value()) {
        return q;
    }
    // Prefer the natural value (k = 0); only shift by +-2*pi when it is out of range.
    for (const int k : {0, -1, 1}) {
        const double candidate = q + 2.0 * kPi * k;
        if (candidate >= limits->lower - 1e-9 && candidate <= limits->upper + 1e-9) {
            return candidate;
        }
    }
    ok = false;
    return q;
}

/// Returns -1 if `v` is negative, otherwise 1; used to report elbow/shoulder/wrist posture signs.
int branchSign(double v) { return v < 0.0 ? -1 : 1; }
}

/// Returns the solver's identifier, "analytic_6dof_spherical_wrist".
const char* Analytic6DofSphericalWristSolver::name() const
{
    return "analytic_6dof_spherical_wrist";
}

/// Checks whether `config` is a serial, all-revolute 6-DOF robot whose geometry matches
/// the supported articulated spherical-wrist morphology (see extractGeometry()).
/// @param config the serial robot configuration to check
/// @return true if this solver can be used for `config`
bool Analytic6DofSphericalWristSolver::supportsModel(const SerialRobotConfig& config) const
{
    if (config.topology != RobotTopologyType::Serial || ForwardKinematics::movableJointCount(config) != 6) {
        return false;
    }
    for (const Joint& joint : config.joints) {
        if (joint.type == JointType::Prismatic) {
            return false;
        }
    }
    return extractGeometry(config).valid;
}

/// Computes all analytic IK branches via solveAll() and returns the single solution
/// closest to `context.request.seedJoint` (or `previousJoint` if no seed is given); if
/// neither reference is available, returns the first branch found.
/// @param config the serial robot configuration
/// @param context the IK target pose and solve options (seed/previous joints)
/// @return an IKResult containing at most one solution, or the failure status from solveAll()
IKResult Analytic6DofSphericalWristSolver::solve(const SerialRobotConfig& config,
                                                 const IKSolveContext& context) const
{
    IKResult all = solveAll(config, context);
    if (!all.ok()) {
        return all;
    }

    // Single best: closest to seed (then previous) joints if provided, else first branch.
    const IKRequest& request = context.request;
    const std::optional<JointVector> reference = request.seedJoint.has_value() ? request.seedJoint
                                                                              : request.previousJoint;
    if (reference.has_value() && reference->size() == 6) {
        std::size_t best = 0;
        double bestCost = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < all.solutions.size(); ++i) {
            const double cost = (all.solutions[i].joints.values() - reference->values()).norm();
            if (cost < bestCost) {
                bestCost = cost;
                best = i;
            }
        }
        const IKSolution chosen = all.solutions[best];
        all.solutions = {chosen};
    } else {
        all.solutions.resize(1);
    }
    return all;
}

/// Analytically solves inverse kinematics for the articulated spherical-wrist 6R model,
/// enumerating all geometric branches (2 shoulder x 2 elbow x 2 wrist = up to 8 candidates),
/// verifying each by forward kinematics against `context.targetFlangeInBase`, and discarding
/// candidates that fall outside joint limits, fail the position/orientation tolerance check,
/// or duplicate an already-found solution.
/// @param config the serial robot configuration; must satisfy supportsModel()
/// @param context the IK target pose and solve options
/// @return IKStatus::UnsupportedSolver if the model geometry doesn't match; Singularity if
///   the wrist center lies on the J1 axis; TargetUnreachable if no in-limit branch passes
///   the FK check; otherwise Ok with all found solutions (posture-tagged by joint sign)
IKResult Analytic6DofSphericalWristSolver::solveAll(const SerialRobotConfig& config,
                                                    const IKSolveContext& context) const
{
    const ArmWristGeometry g = extractGeometry(config);
    if (!g.valid) {
        return IKResult{IKStatus::UnsupportedSolver, {},
                        "analytic spherical-wrist solver does not support this model"};
    }

    const Eigen::Isometry3d target = context.targetFlangeInBase.isometry();
    const Eigen::Matrix3d R = target.rotation();
    const Eigen::Vector3d p = target.translation();
    const Eigen::Vector3d wristCenter = R * g.wristInFlange + p;

    const Eigen::Vector3d v = wristCenter - g.shoulder;
    const double v1 = v.dot(g.u1);
    const Eigen::Vector3d vh = v - v1 * g.u1;
    const double rho = vh.norm();

    const std::vector<const Joint*> mv = movableJoints(config);

    IKResult result;
    result.status = IKStatus::Ok;

    if (rho < 1e-9) {
        // Wrist center on the J1 axis: shoulder singularity, q1 undetermined.
        return IKResult{IKStatus::Singularity, {}, "wrist center lies on the J1 axis (shoulder singularity)"};
    }
    const Eigen::Vector3d vhat = vh / rho;
    const double q1a = std::atan2(g.radial0.cross(vhat).dot(g.u1), g.radial0.dot(vhat));

    const std::array<std::pair<double, double>, 2> shoulderBranches = {{{q1a, 1.0}, {q1a + kPi, -1.0}}};
    for (const auto& sb : shoulderBranches) {
        const double q1 = sb.first;
        const double X = sb.second * rho;
        const double Y = v1;
        const double D2 = X * X + Y * Y;
        double cosE = (D2 - g.upperArm * g.upperArm - g.foreArm * g.foreArm)
                      / (2.0 * g.upperArm * g.foreArm);
        if (cosE > 1.0 + 1e-9 || cosE < -1.0 - 1e-9) {
            continue; // target unreachable for this branch
        }
        cosE = std::max(-1.0, std::min(1.0, cosE));
        const double elbowMag = std::acos(cosE);

        for (const double Eb : {elbowMag, -elbowMag}) {
            const double beta1 = std::atan2(Y, X)
                                 - std::atan2(g.foreArm * std::sin(Eb), g.upperArm + g.foreArm * std::cos(Eb));
            const double q2 = g.s2 * (beta1 - g.beta1Home);
            const double q3 = g.s3 * (Eb - g.elbowHome);

            Eigen::Matrix<double, 6, 1> arm = Eigen::Matrix<double, 6, 1>::Zero();
            arm[0] = q1;
            arm[1] = q2;
            arm[2] = q3;
            const Eigen::Matrix3d R03 = linkPose(config, arm, g.link3Id).isometry().rotation();
            const Eigen::Matrix3d N = g.A4.transpose() * (R03.transpose() * R);

            std::array<Eigen::Vector3d, 2> wrist;
            wristAngles(N, g.mu, g.delta, wrist);
            for (const Eigen::Vector3d& w : wrist) {
                std::array<double, 6> q = {q1, q2, q3, w.x(), w.y(), w.z()};

                bool withinLimits = true;
                for (int i = 0; i < 6 && withinLimits; ++i) {
                    bool ok = true;
                    q[i] = wrapToLimits(q[i], mv[i]->limits, ok);
                    withinLimits = ok;
                }
                if (!withinLimits) {
                    continue;
                }

                Eigen::VectorXd qv(6);
                for (int i = 0; i < 6; ++i) {
                    qv[i] = q[i];
                }
                const JointVector joints(qv);
                const Pose fk = ForwardKinematics::flangePose(config, joints);
                const double posErr = (fk.translation_m() - p).norm();
                const double oriErr =
                    fk.rotationQuaternion().angularDistance(context.targetFlangeInBase.rotationQuaternion());
                if (posErr > kAcceptPos || oriErr > kAcceptOri) {
                    continue;
                }

                bool duplicate = false;
                for (const IKSolution& existing : result.solutions) {
                    if ((existing.joints.values() - qv).norm() <= kDupTol) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    continue;
                }

                IKSolution solution;
                solution.joints = joints;
                solution.posture.shoulder = branchSign(q[0]);
                solution.posture.elbow = branchSign(q[2]);
                solution.posture.wrist = branchSign(q[4]);
                solution.positionError_m = posErr;
                solution.orientationError_rad = oriErr;
                result.solutions.push_back(solution);
            }
        }
    }

    if (result.solutions.empty()) {
        return IKResult{IKStatus::TargetUnreachable, {},
                        "analytic spherical-wrist solver found no in-limit solution"};
    }
    return result;
}

} // namespace RobotKinematics
