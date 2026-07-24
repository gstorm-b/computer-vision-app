#ifndef KAWASAKI_COMMAND_H
#define KAWASAKI_COMMAND_H

#include <QString>
#include "robot/kawasaki/kawasaki_context.h"
#include "robot/robot_command.h"

namespace rb {

// clamp helper
inline static double clamp01(double v) {
    if (v > 1.0) return 1.0;
    if (v < -1.0) return -1.0;
    return v;
}

inline CartesianPoint eulerZYZ_2_XYZ(CartesianPoint point, double eps = 1e-9) {
    CartesianPoint new_point = point;
    double o = point.rx() * M_PI / 180.0;
    double a = point.ry() * M_PI / 180.0;
    double t = point.rz() * M_PI / 180.0;

    double cO = std::cos(o);
    double cA = std::cos(a);
    double cT = std::cos(t);
    double sO = std::sin(o);
    double sA = std::sin(a);
    double sT = std::sin(t);

    double r00 = cO * (cA * cT) - sO * (sT);             // R[0,0]
    double r10 = sO * (cA * cT) + cO * (sT);             // R[1,0]

    double r20 = - sA * cT;                              // R[2,0]
    double r21 = sA * sT;                                // R[2,1]
    double r22 = cA;                                     // R[2,2]

    double r01 = cO * (-cA * sT) - sO * (cT);            // R[0,1]
    double r11 = sO * (-cA * sT) + cO * (cT);            // R[1,1]

    double sin_beta = - r20;
    sin_beta = clamp01(sin_beta);
    double beta = std::asin(sin_beta);
    double cb = std::cos(beta);

    double alpha = 0.0, gamma = 0.0;
    if (std::abs(cb) > eps) {
        alpha = std::atan2(r21, r22);
        gamma = std::atan2(r10, r00);
    } else {
        // gimbal-lock: cos(beta) ~ 0 -> set alpha = 0 convention
        alpha = 0.0;
        if (sin_beta >= 1.0 - eps) { // beta ~ +pi/2
            beta = + M_PI / 2.0;
            gamma = std::atan2(-r01, r11);
        } else { // beta ~ -pi/2
            beta = - M_PI / 2.0;
            gamma = std::atan2(r01, -r11);
        }
    }

    new_point.setRx(alpha * 180.0 / M_PI);
    new_point.setRy(beta * 180.0 / M_PI);
    new_point.setRz(gamma * 180.0 / M_PI);
    return new_point;
}

inline CartesianPoint eulerXYZ_2_ZYZ(CartesianPoint &point, double eps = 1e-9) {
    CartesianPoint new_point = point;
    // convert to radians
    double alpha = point.rx() * M_PI / 180.0;
    double beta = point.ry() * M_PI / 180.0;
    double gamma = point.rz() * M_PI / 180.0;

    double cA = std::cos(alpha);
    double cB = std::cos(beta);
    double cG = std::cos(gamma);
    double sA = std::sin(alpha);
    double sB = std::sin(beta);
    double sG = std::sin(gamma);

    double r00 = cG * cB;
    double r10 = sG * cB;

    double r20 = -sB;
    double r21 = cB * sA;
    double r22 = cB * cA;
    double r02 = cG * sB * cA + sG * sA;
    double r12 = sG * sB * cA + cG * sA;

    // Compute sinA (>=0)
    double sinA = std::sqrt(r20*r20 + r21*r21);
    // Clamp r22 into [-1,1] for numerical stability
    double r22c = clamp01(r22);

    double A = std::atan2(sinA, r22c); // A in [0, pi]

    double O = 0.0, T = 0.0;
    if (sinA > eps) {
        // regular case
        O = std::atan2(r12, r02);           // O = atan2(R[1,2], R[0,2])
        T = std::atan2(r21, -r20);          // T = atan2(R[2,1], -R[2,0])
    } else {
        // singular: sin(A) ~ 0 -> A ~ 0 or A ~ pi
        // Only O+T (sum) is determined. We'll set T = 0 and choose O = atan2(R[1,0], R[0,0])
        T = 0.0;
        if (r22c > 0.0) {
            // A ~ 0
            A = 0.0;
            O = std::atan2(r10, r00);
        } else {
            // A ~ pi
            A = M_PI;
            // one valid choice: O = atan2(-R[1,0], -R[0,0])
            O = std::atan2(-r10, -r00);
        }
    }

    // convert to degree
    new_point.setRx(O * 180.0 / M_PI);
    new_point.setRy(A * 180.0 / M_PI);
    new_point.setRz(T * 180.0 / M_PI);
    return new_point;
}

class KawasakiCmdInterface : public RbCmdInterface {
public:
    KawasakiCmdInterface(KwMsgSender& _msg, KwFlagSender& _flag)
        : msg(_msg), flag(_flag) {

    }

    KwMsgSender& msg;
    KwFlagSender& flag;

protected:
    int robot_id;
};

class KawasakiCommand : public RbCommand{
public:
    KawasakiCommand();

    void setContext(std::shared_ptr<RbCmdInterface> _interface) override;
    virtual void execute() override {}

    virtual QString commandId() {
        return "";
    }

    const bool validateCmd() override {
        return true;
    }

    virtual void resetCommand() override {
        // do nothing
    }

    virtual std::shared_ptr<RbCommand> clone() const override {
        return std::make_shared<KawasakiCommand>(*this);
    }

    virtual const bool isMotionCommand() override {
        return false;
    }

    const bool isComplete() override;
    const bool isExecuting() override;
    const ExecuteState getExecuteState() override;
    const RbType getRobotType() override;
    void setLineNumber(int line_number) override;
    const int getLineNumber() override;

protected:
    ExecuteState m_execute_state;
    std::shared_ptr<KawasakiCmdInterface> m_interface;
    int m_line_number;

private:
    RbType m_robot_type = RbType::Kawasaki_EF;
};

}

#endif // KAWASAKI_COMMAND_H
