#ifndef ROBOT_POINT_H
#define ROBOT_POINT_H

#include <memory>
#include <QString>
#include <QJsonObject>

namespace rb {

class CoorPoint {
public:
    enum Type {
        Cartesian,
        Joint
    };

    virtual ~CoorPoint() = default;

    void setPercision(int percision);
    inline Type type() {
        return this->m_type;
    }

    virtual std::shared_ptr<CoorPoint> clone() = 0;
    virtual QString toQString() = 0;
    virtual QJsonObject toJsonObject()  = 0;
    virtual void fromJsonObject(const QJsonObject &json, bool *ok) = 0;

    static bool verifyFromJson(const QJsonObject &json, QString key);

protected:
    int m_percision = 3;
    Type m_type;
};

class JointPoint;

class CartesianPoint : public CoorPoint {
public:
    // CartesianPoint();
    CartesianPoint(double x = .0f, double y = .0f, double z = .0f,
                   double rx = .0f, double ry = .0f, double rz = .0f);

    std::shared_ptr<CoorPoint> clone() override;
    QString toQString() override;
    QJsonObject toJsonObject() override;
    void fromJsonObject(const QJsonObject &json, bool *ok) override;
    JointPoint toJoint();

    inline double x() {
        return this->m_x;
    }

    inline double y() {
        return this->m_y;
    }

    inline double z() {
        return this->m_z;
    }

    inline double rx() {
        return this->m_rx;
    }

    inline double ry() {
        return this->m_ry;
    }

    inline double rz() {
        return this->m_rz;
    }

    inline double& x_ref() {
        return this->m_x;
    }

    inline double& y_ref() {
        return this->m_y;
    }

    inline double& z_ref() {
        return this->m_z;
    }

    inline double& rx_ref() {
        return this->m_rx;
    }

    inline double& ry_ref() {
        return this->m_ry;
    }

    inline double& rz_ref() {
        return this->m_rz;
    }

    inline void setX(double x) {
        this->m_x = x;
    }

    inline void setY(double y) {
        this->m_y = y;
    }

    inline void setZ(double z) {
        this->m_z = z;
    }

    inline void setRx(double rx) {
        this->m_rx = rx;
    }

    inline void setRy(double ry) {
        this->m_ry = ry;
    }

    inline void setRz(double rz) {
        this->m_rz = rz;
    }

private:
    double m_x;
    double m_y;
    double m_z;
    double m_rx;
    double m_ry;
    double m_rz;
};

class JointPoint : public CoorPoint {
public:
    // JointPoint();
    JointPoint(double j1 = .0f, double j2 = .0f, double j3 = .0f,
               double j4 = .0f, double j5 = .0f, double j6 = .0f);

    std::shared_ptr<CoorPoint> clone() override;
    QString toQString() override;
    QJsonObject toJsonObject() override;
    void fromJsonObject(const QJsonObject &json, bool *ok) override;
    CartesianPoint toCartesian();

    inline double j1() {
        return this->m_j1;
    }

    inline double j2() {
        return this->m_j2;
    }

    inline double j3() {
        return this->m_j3;
    }

    inline double j4() {
        return this->m_j4;
    }

    inline double j5() {
        return this->m_j5;
    }

    inline double j6() {
        return this->m_j6;
    }

    inline double& j1_ref() {
        return this->m_j1;
    }

    inline double& j2_ref() {
        return this->m_j2;
    }

    inline double& j3_ref() {
        return this->m_j3;
    }

    inline double& j4_ref() {
        return this->m_j4;
    }

    inline double& j5_ref() {
        return this->m_j5;
    }

    inline double& j6_ref() {
        return this->m_j6;
    }

    inline void setJ1(double j1) {
        this->m_j1 = j1;
    }

    inline void setJ2(double j2) {
        this->m_j2 = j2;
    }

    inline void setJ3(double j3) {
        this->m_j3 = j3;
    }

    inline void setJ4(double j4) {
        this->m_j4 = j4;
    }

    inline void setJ5(double j5) {
        this->m_j5 = j5;
    }

    inline void setJ6(double j6) {
        this->m_j6 = j6;
    }

private:
    double m_j1;
    double m_j2;
    double m_j3;
    double m_j4;
    double m_j5;
    double m_j6;
};

class CoordinateCreator {
public:
    static std::shared_ptr<CartesianPoint> createCartesianPoint(CoorPoint *point);
    static std::shared_ptr<JointPoint> createJointPoint(CoorPoint *point);
};
}

#endif // ROBOT_POINT_H
