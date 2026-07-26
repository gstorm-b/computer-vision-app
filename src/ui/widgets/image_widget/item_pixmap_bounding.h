#ifndef ITEM_PIXMAP_BOUNDING_H
#define ITEM_PIXMAP_BOUNDING_H

#include <QGraphicsPixmapItem>
#include <QPainter>

/// QGraphicsPixmapItem that draws itself with a colored border rectangle
/// around the pixmap's bounding rect, in addition to the normal pixmap paint.
class PixmapBoundingLine : public QGraphicsPixmapItem {
public:
  /// Constructs the item for `pixmap` with a default green, 2px-wide border.
  PixmapBoundingLine(const QPixmap &pixmap)
      : QGraphicsPixmapItem(pixmap),
      m_bouding_color(Qt::green),
      m_border_width(2) {

  }

  /// Draws the base pixmap via the base class, then an outline rectangle
  /// around the bounding rect (expanded by border width + 1) in the
  /// configured color and width.
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

  /// Sets the border color, ignoring the call if `color` is invalid.
  void setBorderColor(QColor color) {
    if (color.isValid()) {
      m_bouding_color = color;
    }
  }

  /// Sets the border line width in pixels.
  void setBorderLineWidth(int width) {
    m_border_width = width;
  }

private:
  int m_border_width;      ///< Width, in pixels, of the drawn border rectangle.
  QColor m_bouding_color;   ///< Color of the drawn border rectangle.
};


#endif // ITEM_PIXMAP_BOUNDING_H
