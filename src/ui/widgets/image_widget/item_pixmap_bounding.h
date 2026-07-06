#ifndef ITEM_PIXMAP_BOUNDING_H
#define ITEM_PIXMAP_BOUNDING_H

#include <QGraphicsPixmapItem>
#include <QPainter>

class PixmapBoundingLine : public QGraphicsPixmapItem {
public:
  PixmapBoundingLine(const QPixmap &pixmap)
      : QGraphicsPixmapItem(pixmap),
      m_bouding_color(Qt::green),
      m_border_width(2) {

  }

  void paint(QPainter *painter,
             const QStyleOptionGraphicsItem *option,
             QWidget *widget = nullptr) override {
    // draw base pixmap
    QGraphicsPixmapItem::paint(painter, option, widget);

    // draw bouding box
    int border_adjusted = m_border_width + 1;
    QRectF rect = boundingRect().adjusted(-border_adjusted, -border_adjusted,
                                          border_adjusted, border_adjusted);
    QPen pen(m_bouding_color);
    pen.setWidth(m_border_width);
    painter->setPen(pen);
    painter->drawRect(rect);
  }

  void setBorderColor(QColor color) {
    if (color.isValid()) {
      m_bouding_color = color;
    }
  }

  void setBorderLineWidth(int width) {
    m_border_width = width;
  }

private:
  int m_border_width;
  QColor m_bouding_color;
};


#endif // ITEM_PIXMAP_BOUNDING_H
