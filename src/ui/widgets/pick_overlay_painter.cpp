#include "ui/widgets/pick_overlay_painter.h"
#include "ui/forms/pattern/pattern_theme.h"

#include <QPainter>
#include <QFont>
#include <QtMath>

#include <cmath>

namespace pick_overlay {

QPointF knobOffset(double angleDeg) {
    const double rad = qDegreesToRadians(angleDeg + kHandleBearing);
    return QPointF(std::cos(rad), std::sin(rad)) * kHandleDistance;
}

void drawOrientationGizmo(QPainter &p, const QPointF &pickWidget, double angleDeg,
                          bool showKnob, bool knobHeld) {
    const double radX = qDegreesToRadians(angleDeg);
    const double radY = qDegreesToRadians(angleDeg + 90.0);

    // Draws one labelled axis: a full line through the pick point, arrowhead on the
    // positive tip, letter just beyond it.
    auto axis = [&](double rad, const QColor &color, const QString &label) {
        const QPointF dir(std::cos(rad), std::sin(rad));
        const QPointF tip  = pickWidget + dir * kAxisLen;
        const QPointF tail = pickWidget - dir * kAxisLen;

        p.setBrush(Qt::NoBrush);

        // Negative half — thinner and translucent, so it reads as the axis continuing
        // rather than as a second arrow.
        QColor dim(color); dim.setAlpha(110);
        p.setPen(QPen(dim, 1.2));
        p.drawLine(tail, pickWidget);

        // Positive half.
        p.setPen(QPen(color, 2.0));
        p.drawLine(pickWidget, tip);

        // Arrowhead: two barbs swept back from the tip by +/-150 degrees.
        for (const double sweep : {150.0, -150.0}) {
            const double b = rad + qDegreesToRadians(sweep);
            p.drawLine(tip, tip + QPointF(std::cos(b), std::sin(b)) * kArrowBarb);
        }

        // Axis letter, nudged past the tip along the arrow so it never overlaps the barbs.
        p.setPen(color);
        p.setFont(QFont("JetBrains Mono", 8, QFont::Bold));
        p.drawText(tip + dir * 10.0 + QPointF(-3, 4), label);
    };

    axis(radX, QColor(ptn::ERR), QStringLiteral("X"));   // red   — conventional X
    axis(radY, QColor(ptn::OK),  QStringLiteral("Y"));   // green — conventional Y

    if (!showKnob) return;

    // Drag knob on the X/Y bisector, with its connector along the same bearing — nothing
    // here crosses an axis line or an axis letter.
    const QPointF off  = knobOffset(angleDeg);
    const QPointF knob = pickWidget + off;
    const double  lenK = std::hypot(off.x(), off.y());
    const QPointF dirK = lenK > 0.0 ? off / lenK : QPointF(1, 0);

    QColor knobColor(ptn::WARN);
    p.setPen(QPen(knobColor, 1.2, Qt::DotLine));
    p.drawLine(pickWidget + dirK * 10.0, knob - dirK * kHandleRadius);
    p.setBrush(knobHeld ? QColor("white") : knobColor);
    p.setPen(QPen(knobHeld ? knobColor : QColor("white"), 1.2));
    p.drawEllipse(knob, kHandleRadius, kHandleRadius);

    // Angle readout, continuing outward along the same bearing so it stays clear of both
    // axes. A drag therefore reports its value without a trip to the spin box.
    p.setPen(knobColor);
    p.setFont(QFont("JetBrains Mono", 8, QFont::Bold));
    p.drawText(knob + dirK * (kHandleRadius + 6.0) + QPointF(0, 4),
               QString("%1°").arg(angleDeg, 0, 'f', 1));
}

void drawPickRing(QPainter &p, const QPointF &pickWidget) {
    QColor ringFill(ptn::WARN); ringFill.setAlpha(34);
    p.setBrush(ringFill);
    p.setPen(QPen(QColor(ptn::WARN), 1.5));
    p.drawEllipse(pickWidget, 9, 9);
}

void drawPickCrosshair(QPainter &p, const QPointF &pickWidget) {
    p.setPen(QPen(QColor("white"), 1));
    p.drawLine(QPointF(pickWidget.x() - 12, pickWidget.y()),
               QPointF(pickWidget.x() + 12, pickWidget.y()));
    p.drawLine(QPointF(pickWidget.x(), pickWidget.y() - 12),
               QPointF(pickWidget.x(), pickWidget.y() + 12));
    p.setBrush(QColor(ptn::WARN));
    p.setPen(Qt::NoPen);
    p.drawEllipse(pickWidget, 3, 3);
}

void drawJawPair(QPainter &p, const QPointF &pickWidget,
                 double boxWidget, double boxHeightWidget,
                 double distWidget, double angleDeg) {
    const QColor jawColor(ptn::OK);
    QColor jawFill(ptn::OK); jawFill.setAlpha(34);

    // Draws one jaw at `angle` from the pick point; B is simply A rotated 180 degrees,
    // which is what keeps the pair symmetric without a second set of values.
    auto drawBox = [&](double angle, const QString &label) {
        const double  r = qDegreesToRadians(angle);
        const QPointF c = pickWidget + QPointF(std::cos(r), std::sin(r)) * distWidget;

        p.setPen(QPen(jawColor, 1, Qt::DashLine));
        p.drawLine(pickWidget, c);

        p.save();
        p.translate(c);
        p.rotate(angle);
        p.setBrush(jawFill);
        p.setPen(QPen(jawColor, 1.5));
        p.drawRect(QRectF(-boxWidget / 2, -boxHeightWidget / 2, boxWidget, boxHeightWidget));
        p.setBrush(jawColor);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(0, 0), 2.5, 2.5);
        p.restore();

        p.setPen(jawColor);
        p.setFont(QFont("JetBrains Mono", 8, QFont::Bold));
        p.drawText(QPointF(c.x() - 4, c.y() - boxHeightWidget / 2 - 4), label);
    };

    drawBox(angleDeg,         QStringLiteral("A"));
    drawBox(angleDeg + 180.0, QStringLiteral("B"));
}

} // namespace pick_overlay
