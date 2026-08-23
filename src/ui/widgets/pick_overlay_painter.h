#ifndef PICK_OVERLAY_PAINTER_H
#define PICK_OVERLAY_PAINTER_H

#include <QPointF>

class QPainter;

/**
 * @file pick_overlay_painter.h
 * @brief pick_overlay — the single implementation of the pick-point, picking-orientation
 *        and jaw-pair overlays, shared by every widget that draws them.
 */

/**
 * @namespace pick_overlay
 * @brief Painting primitives for a pattern's pick geometry: the pick marker, the X/Y
 *        orientation gizmo, and the two gripper jaw boxes.
 *
 * Extracted so the authoring canvas (AddPatternImageCanvas) and the read-only pattern
 * thumbnail render identically. "Drawn the same way" is a requirement that decays
 * silently with two implementations, so there is deliberately only one: a second copy
 * would already have missed the two revisions this gizmo went through.
 *
 * Every function takes and returns WIDGET (viewport) pixels, never image pixels. Callers
 * do their own image-to-widget mapping and pass the result in; the painter therefore
 * knows nothing about zoom, pan, scene transforms or crop rectangles. Sizes that must
 * stay constant on screen (the gizmo) are baked in here as widget-pixel constants, while
 * sizes that must track the image (the jaw boxes) are passed in already scaled.
 *
 * @note These draw with the painter's current transform. A QGraphicsView caller must
 *       reset the world transform first, otherwise the overlay is drawn in scene space
 *       and scales with the zoom.
 * @see AddPatternImageCanvas, PatternThumbnailView
 */
namespace pick_overlay {

// ── Gizmo geometry, in widget pixels ─────────────────────────────────────────
constexpr double kAxisLen        = 52.0;  ///< Half-length of each axis line, from the pick point.
constexpr double kArrowBarb      = 9.0;   ///< Arrowhead barb length.
constexpr double kHandleRadius   = 6.0;   ///< Rotation-knob visual radius.
/// Bearing of the rotation knob relative to the X axis. 45 degrees puts it on the
/// bisector between the X and Y arrows, so neither the knob, its connector nor its angle
/// readout lands on top of an axis line or an axis letter.
constexpr double kHandleBearing  = 45.0;
/// Knob distance from the pick point. Just inside the axis half-length, so the knob stays
/// within the gizmo's visual envelope instead of enlarging it.
constexpr double kHandleDistance = kAxisLen - 8.0;

/**
 * @brief Returns the rotation knob's offset from the pick point, in widget pixels.
 * @param[in] angleDeg picking orientation, in degrees
 * @return offset to add to the pick point's widget position to get the knob centre
 */
QPointF knobOffset(double angleDeg);

/**
 * @brief Draws the X/Y orientation gizmo centred on @p pickWidget.
 *
 * Each axis is a full line spanning kAxisLen either side of the pick point, with the
 * arrowhead on the positive end only — the frame reads as a pair of axes while still
 * showing which way each one points. The negative halves are thinner and translucent so
 * the positive direction stays unambiguous.
 *
 * Y is drawn at +90 degrees, not -90: callers work in image coordinates where Y grows
 * downward, so +90 renders as the familiar X-right / Y-down frame and matches the
 * convention the matcher reports angles in.
 *
 * @param[in,out] p painter to draw with, in widget coordinates
 * @param[in] pickWidget pick point, already mapped to widget coordinates
 * @param[in] angleDeg picking orientation, in degrees
 * @param[in] showKnob true to draw the drag knob and angle readout (interactive callers)
 * @param[in] knobHeld true while the knob is being dragged, which inverts its fill so the
 *        grab reads as "held" even when the cursor runs ahead of it
 * @note Call this BEFORE drawPickRing()/drawPickCrosshair(): both axes run through the
 *       pick point, and the centre marker is what covers the crossing.
 */
void drawOrientationGizmo(QPainter &p, const QPointF &pickWidget, double angleDeg,
                          bool showKnob, bool knobHeld);

/**
 * @brief Draws the filled ring used to mark the pick point on the authoring pick step.
 * @param[in,out] p painter to draw with, in widget coordinates
 * @param[in] pickWidget pick point, already mapped to widget coordinates
 */
void drawPickRing(QPainter &p, const QPointF &pickWidget);

/**
 * @brief Draws the compact white crosshair + dot used to mark the pick point wherever the
 *        jaw boxes are also shown.
 * @param[in,out] p painter to draw with, in widget coordinates
 * @param[in] pickWidget pick point, already mapped to widget coordinates
 */
void drawPickCrosshair(QPainter &p, const QPointF &pickWidget);

/**
 * @brief Draws the symmetric jaw pair (boxes A and B) and their connectors to the pick point.
 *
 * Box B mirrors box A through the pick point, which is what makes the pair symmetric by
 * construction rather than by the caller keeping two sets of values in step.
 *
 * @param[in,out] p painter to draw with, in widget coordinates
 * @param[in] pickWidget pick point, already mapped to widget coordinates
 * @param[in] boxWidget jaw width, ALREADY scaled to widget pixels
 * @param[in] boxHeightWidget jaw height, ALREADY scaled to widget pixels
 * @param[in] distWidget centre-to-centre half-distance, ALREADY scaled to widget pixels
 * @param[in] angleDeg jaw-pair orientation, in degrees
 */
void drawJawPair(QPainter &p, const QPointF &pickWidget,
                 double boxWidget, double boxHeightWidget,
                 double distWidget, double angleDeg);

} // namespace pick_overlay
#endif // PICK_OVERLAY_PAINTER_H
