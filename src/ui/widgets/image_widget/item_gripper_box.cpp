#include "item_gripper_box.h"
#include <QtMath>

ItemGripperBox::ItemGripperBox(QGraphicsItem *parent)
    : QGraphicsItem(parent),
    m_distance(40.0),
    m_size1(20.0, 20.0),
    m_size2(20.0, 20.0),
    m_placementAngle(0.0),
    m_cacheValid(false) {
  setFlags(ItemIsMovable | ItemIsSelectable);
  setAcceptHoverEvents(true);
}

qreal ItemGripperBox::distance() const {
  return m_distance;
}

void ItemGripperBox::setDistance(qreal d) {
  if (!qFuzzyCompare(d + 1.0, m_distance + 1.0)) {
    prepareGeometryChange();
    m_distance = d;
    m_cacheValid = false;
    update();
  }
}

QSizeF ItemGripperBox::size1() const {
  return m_size1;
}

void ItemGripperBox::setSize1(const QSizeF &s) {
  if (s != m_size1) {
    prepareGeometryChange();
    m_size1 = s;
    m_cacheValid = false;
    update();
  }
}

QSizeF ItemGripperBox::size2() const { return m_size2; }
void ItemGripperBox::setSize2(const QSizeF &s) {
  if (s != m_size2) {
    prepareGeometryChange();
    m_size2 = s;
    m_cacheValid = false;
    update();
  }
}

qreal ItemGripperBox::placementAngle() const {
  return m_placementAngle;
}

void ItemGripperBox::setPlacementAngle(qreal angleDeg) {
  if (!qFuzzyCompare(angleDeg + 1.0, m_placementAngle + 1.0)) {
    prepareGeometryChange();
    m_placementAngle = angleDeg;
    m_cacheValid = false;
    update();
  }
}

QPointF ItemGripperBox::pointAtAngleAndDistance(qreal angleDeg, qreal dist) const {
  qreal a = qDegreesToRadians(angleDeg);
  // Qt coordinate: +x sang phải, +y xuống dưới -> y = -sin(angle) để angle tính theo độ toán học (ccw)
  return QPointF(dist * qCos(a), -dist * qSin(a));
}

QRectF ItemGripperBox::boundingRect() const {
  if (m_cacheValid)
    return m_cachedBounding;

  // Tính vị trí trung tâm của 2 squares (local coordinates)
  QPointF p1 = pointAtAngleAndDistance(m_placementAngle, m_distance);
  QPointF p2 = pointAtAngleAndDistance(m_placementAngle + 180.0, m_distance);

  // Tạo rect cho mỗi hình vuông (tâm local -> top-left = center - size/2)
  QRectF r1(p1.x() - m_size1.width() / 2.0,
            p1.y() - m_size1.height() / 2.0,
            m_size1.width(),
            m_size1.height());

  QRectF r2(p2.x() - m_size2.width() / 2.0,
            p2.y() - m_size2.height() / 2.0,
            m_size2.width(),
            m_size2.height());

  QRectF unionRect = r1.united(r2);

  // thêm margin nhỏ để chứa pen width hoặc antialiasing
  const qreal margin = 2.0;
  unionRect.adjust(-margin, -margin, margin, margin);

  m_cachedBounding = unionRect;
  m_cacheValid = true;
  return m_cachedBounding;
}

void ItemGripperBox::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option)
  Q_UNUSED(widget)

  painter->setRenderHint(QPainter::Antialiasing, true);

  // Tut: vẽ tham chiếu tâm
  QPointF center(0, 0);
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(0, 0, 0, 0)); // nothing; optional

  // Tính tọa độ 2 center local của squares
  QPointF c1 = pointAtAngleAndDistance(m_placementAngle, m_distance);
  QPointF c2 = pointAtAngleAndDistance(m_placementAngle + 180.0, m_distance);

  // Hình vuông 1
  QRectF sq1(c1.x() - m_size1.width() / 2.0,
             c1.y() - m_size1.height() / 2.0,
             m_size1.width(),
             m_size1.height());

  // Hình vuông 2
  QRectF sq2(c2.x() - m_size2.width() / 2.0,
             c2.y() - m_size2.height() / 2.0,
             m_size2.width(),
             m_size2.height());

  // Vẽ fill và viền rõ ràng
  painter->setPen(QPen(Qt::darkBlue, 0.8));
  // painter->setBrush(QColor(200, 200, 255));
  painter->drawRect(sq1);

  painter->setPen(QPen(Qt::darkBlue, 0.8));
  // painter->setBrush(QColor(200, 255, 200));
  painter->drawRect(sq2);

  // Vẽ tâm (tuỳ chọn)
  painter->setBrush(Qt::red);
  painter->setPen(Qt::NoPen);
  painter->drawEllipse(center, 2.5, 2.5);
}
