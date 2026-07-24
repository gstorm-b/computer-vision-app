#include "robot_point.h"

namespace rb {

/// CoorPoint
void CoorPoint::setPercision(int percision) {
    if ((percision > 0) && (percision <= 10)){
        this->m_percision = percision;
    }
}

bool CoorPoint::verifyFromJson(const QJsonObject &json, QString key) {
    if (json.contains(key) ) {
        if (json[key].isDouble()) {
            return true;
        }
    }
    return false;
}
/// END CoorPoint

/// CartesianPoint
// CartesianPoint::CartesianPoint() :
//     m_x(0), m_y(0), m_z(0), m_rx(0), m_ry(0), m_rz(0) {

// }

CartesianPoint::CartesianPoint(double x, double y, double z, double rx, double ry, double rz) :
    m_x(x), m_y(y), m_z(z), m_rx(rx), m_ry(ry), m_rz(rz) {

    m_type = CoorPoint::Type::Cartesian;
}

std::shared_ptr<CoorPoint> CartesianPoint::clone() {
    // return std::shared_ptr<CoorPoint>(new CartesianPoint(m_x, m_y, m_z, m_rx, m_ry, m_rz));
    return std::make_shared<CartesianPoint>(*this);
}

QString CartesianPoint::toQString() {
    return QString("%1,%2,%3,%4,%5,%6")
        .arg(m_x, 0, 'f', m_percision)
        .arg(m_y, 0, 'f', m_percision)
        .arg(m_z, 0, 'f', m_percision)
        .arg(m_rx, 0, 'f', m_percision)
        .arg(m_ry, 0, 'f', m_percision)
        .arg(m_rz, 0, 'f', m_percision);
}

QJsonObject CartesianPoint::toJsonObject() {
    QJsonObject obj;
    obj["X"] = this->m_x;
    obj["Y"] = this->m_y;
    obj["Z"] = this->m_z;
    obj["RX"] = this->m_rx;
    obj["RY"] = this->m_ry;
    obj["RZ"] = this->m_rz;
    return obj;
}

void CartesianPoint::fromJsonObject(const QJsonObject &json, bool *ok) {

    bool state = CoorPoint::verifyFromJson(json, "X");
    if (state) {
        this->m_x = json["X"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "Y");
    if (state) {
        this->m_y = json["Y"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "Z");
    if (state) {
        this->m_z = json["Z"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "RX");
    if (state) {
        this->m_rx = json["RX"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "RY");
    if (state) {
        this->m_ry = json["RY"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "RZ");
    if (state) {
        this->m_rz = json["RZ"].toDouble(.0f);
    }

    if (ok != nullptr) {
        *ok = state;
    }
}

JointPoint CartesianPoint::toJoint() {
    return JointPoint(m_x, m_y, m_z, m_rx, m_ry, m_rz);
}
/// END CartesianPoint

/// JointPoint
// JointPoint::JointPoint() :
//     m_j1(0), m_j2(0), m_j3(0), m_j4(0), m_j5(0), m_j6(0) {

// }

JointPoint::JointPoint(double j1, double j2, double j3, double j4, double j5, double j6) :
    m_j1(j1), m_j2(j2), m_j3(j3), m_j4(j4), m_j5(j5), m_j6(j6) {

    m_type = CoorPoint::Type::Joint;
}

std::shared_ptr<CoorPoint> JointPoint::clone() {
    // return std::shared_ptr<CoorPoint>(new JointPoint(m_j1, m_j2, m_j3, m_j4));
    return std::make_shared<JointPoint>(*this);
}

QString JointPoint::toQString() {
    return QString("%1,%2,%3,%4,%5,%6")
        .arg(m_j1, 0, 'f', m_percision)
        .arg(m_j2, 0, 'f', m_percision)
        .arg(m_j3, 0, 'f', m_percision)
        .arg(m_j4, 0, 'f', m_percision)
        .arg(m_j5, 0, 'f', m_percision)
        .arg(m_j6, 0, 'f', m_percision);
}

QJsonObject JointPoint::toJsonObject() {
    QJsonObject obj;
    obj["J1"] = this->m_j1;
    obj["J2"] = this->m_j2;
    obj["J3"] = this->m_j3;
    obj["J4"] = this->m_j4;
    obj["J5"] = this->m_j5;
    obj["J6"] = this->m_j6;
    return obj;
}

void JointPoint::fromJsonObject(const QJsonObject &json, bool *ok) {

    bool state = CoorPoint::verifyFromJson(json, "J1");
    if (state) {
        this->m_j1 = json["J1"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "J2");
    if (state) {
        this->m_j2 = json["J2"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "J3");
    if (state) {
        this->m_j3 = json["J3"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "J4");
    if (state) {
        this->m_j4 = json["J4"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "J5");
    if (state) {
        this->m_j5 = json["J5"].toDouble(.0f);
    }

    state &= CoorPoint::verifyFromJson(json, "J6");
    if (state) {
        this->m_j6 = json["J6"].toDouble(.0f);
    }

    if (ok != nullptr) {
        *ok = state;
    }
}

CartesianPoint JointPoint::toCartesian() {
    return CartesianPoint(m_j1, m_j2, m_j3, m_j4);
}
/// END JointPoint

/// CoordinateCreator
std::shared_ptr<CartesianPoint> CoordinateCreator::createCartesianPoint(CoorPoint *point) {
    if (point->type() != CoorPoint::Type::Cartesian) {
        return nullptr;
    }

    return std::shared_ptr<CartesianPoint>(new CartesianPoint(((CartesianPoint*)point)->x(),
                                                              ((CartesianPoint*)point)->y(),
                                                              ((CartesianPoint*)point)->z(),
                                                              ((CartesianPoint*)point)->rx(),
                                                              ((CartesianPoint*)point)->ry(),
                                                              ((CartesianPoint*)point)->rz()));
}

std::shared_ptr<JointPoint> CoordinateCreator::createJointPoint(CoorPoint *point) {
    if (point->type() != CoorPoint::Type::Joint) {
        return nullptr;
    }

    return std::shared_ptr<JointPoint>(new JointPoint(((JointPoint*)point)->j1(),
                                                      ((JointPoint*)point)->j2(),
                                                      ((JointPoint*)point)->j3(),
                                                      ((JointPoint*)point)->j4(),
                                                      ((JointPoint*)point)->j5(),
                                                      ((JointPoint*)point)->j6()));
}
/// END CoordinateCreator

}
