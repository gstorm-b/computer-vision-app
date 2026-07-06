# Vision Widgets Implementation Plan

**Date:** 2026-06-27  
**Status:** Phase 1 accepted after owner build/manual test and PM spot-check  
**Owner role:** frontend/widget implementation agent, reviewed by project manager  
**Decision source:** [vision_widgets_planning_questions.md](vision_widgets_planning_questions.md)

## Review State

The implementation compiles according to the project owner and the latest manual UI test result is acceptable. PM spot-check found the major rework items implemented in code.

This plan is now closed for the current phase. Keep the rework request sections below as traceability for what was accepted.

## Progress Review - 2026-06-28

Accepted for this phase:

- Result labels use image-safe chip rendering instead of plain theme foreground text.
- Pattern raw page is read-only preview and no longer exposes the ROI editor palette as the default workspace flow.
- Workspace ROI editing remains in `WorkspaceSettingDialog`; the dialog now labels Workspace ROI and Condition ROI.
- Result toolbar uses compact controls and an overlay menu.
- Empty/null image handling guards invalid pixmap painting.
- Overlay pens/labels/center markers use viewport-stable drawing affordances.
- Result overlay includes object direction arrows and top-left locator arrows.
- Result table selection synchronizes with image overlay selection in Pattern and Dashboard views.
- Image-view matched object hover/click selection is implemented.
- Selected-object isolation hides/disables competing per-object overlays.
- Overlay labels/status chips are anchored near the bounding-box top-left.
- Optional picking box overlay toggle/rendering is implemented.

Verification:

- Project owner confirmed local compile has no errors.
- Project owner completed manual UI testing and reported the result as acceptable.
- PM spot-check reviewed the relevant implementation paths in `src/ui/widgets/vision/`, `LocalizationPatternsWidget`, `LocalizationDashboardWidget`, and `WorkspaceSettingDialog`.

Residual follow-up:

- Keep an eye on runtime row-to-overlay mapping if future matching output starts reordering accepted/rejected objects differently from the result table.
- Run the full architecture contract suite again before committing or handing off as release-ready.

## Completed PM Rework Request - 2026-06-28

The items below are retained as traceability for the accepted rework.

### 1. Overlay Label Contrast Is Not Image-Safe

Current issue:

- Result labels for score, angle, and pattern id use theme text colors such as white in dark theme and black in light theme.
- These labels are drawn on top of camera images, so theme text colors are not reliable. The image content, not the app theme, determines readability.

Required fix:

- Use image-safe overlay label styling.
- Add a high-contrast label treatment such as:
  - bright accent text with dark semi-transparent backing;
  - or white/yellow text with black outline/shadow;
  - or a compact label chip with semi-transparent background.
- Keep label styling readable on both bright and dark image regions.
- Do not rely only on `text.primary`, `text.secondary`, or theme foreground tokens for image overlays.

Suggested touchpoint:

- `src/ui/widgets/vision/vision_canvas.cpp`, `VisionCanvas::rebuildOverlayItems()`.

Acceptance:

- Score, angle, and pattern labels remain readable over mixed bright/dark images in both app themes.

### 2. Raw Page Must Not Expose Meaningless Editable ROI State

Current issue:

- The pattern widget raw page currently shows editable ROI selection behavior.
- For the current localization workflow, workspace ROI is already configured through the workspace setup dialog, so exposing editable ROI on the raw monitor page is confusing and has no clear meaning.

Required fix:

- Raw page in `LocalizationPatternsWidget` should behave as an image/result preview surface, not as an editable ROI authoring surface.
- Keep workspace/condition ROI as read-only auxiliary overlays when "Show ROI" is enabled.
- Do not show selected editable ROI state on the raw page unless a dedicated pattern-ROI editing workflow is explicitly entered.
- Do not keep a separate "Edit ROI" page inside the Pattern widget for workspace ROI editing. Workspace ROI editing belongs to the existing workspace edit dialog.

Suggested touchpoints:

- `src/ui/forms/task/localization_patterns_widget.cpp`, `installVisionWidgets()`, `displayRawImage()`, `updateWorkspaceRoiOverlay()`.
- `src/ui/widgets/vision/vision_roi_editor_widget.*`.
- `src/ui/widgets/vision/vision_canvas.*`.

Acceptance:

- Raw page displays raw image plus optional read-only workspace/condition ROI overlays.
- User cannot accidentally create/select pattern ROI while using the normal raw monitor page.
- Workspace ROI editing remains routed through the existing workspace dialog.

### 3. Tool Palette And Result Toolbar Are Too Large For The UI

Current issue:

- The button bar above the image consumes too much vertical space and feels visually heavy.
- The current result viewer exposes many full text toggle buttons directly on the surface.

Required fix:

- Make image controls compact and visually subordinate to the image.
- For the raw page, remove the tool palette entirely unless a dedicated edit mode is active.
- For result viewer controls, prefer a compact fit button plus an overlay menu or compact segmented/toggle group.
- Use icons where available and short tooltips instead of many large text buttons.

Suggested touchpoints:

- `src/ui/widgets/vision/vision_tool_palette.*`.
- `src/ui/widgets/vision/vision_result_viewer_widget.*`.
- `src/ui/forms/task/localization_patterns_widget.cpp`.

Acceptance:

- Image area remains the dominant UI element.
- Toolbar height is compact.
- Overlay toggles are still available but do not dominate the page.

### 4. Overlay Items And ROI Handles Must Stay Visible Across Zoom Levels

Current issue:

- When a large image is fit to view, scene-drawn items become too small to see.
- Bounding boxes, center points, text labels, and ROI handles should remain readable/usable as the view zooms in or out.

Required fix:

- Use viewport-stable drawing for overlay affordances.
- Apply cosmetic pens or dynamically scale pen widths.
- Use `ItemIgnoresTransformations` where appropriate for labels and point markers.
- Ensure ROI handles remain usable when the image is fit-to-view or zoomed out.

Suggested touchpoints:

- `src/ui/widgets/vision/vision_canvas.cpp`.
- Existing ROI item classes if they are reused: `src/ui/widgets/image_widget/item_roi.*`, `src/ui/widgets/image_widget/item_roi_rotated.*`.

Acceptance:

- On a high-resolution image fitted to the available view, boxes, center points, labels, and handles are still visible.
- Zooming does not make overlay text unreadably tiny or comically large.

### 5. Tool Palette Should Not Be Present On Normal Pattern Raw Page

Current issue:

- The raw page currently exposes a tool palette even though normal raw-image operations can be handled through mouse and keyboard interactions.

Required fix:

- Remove the palette from the default raw page.
- Keep basic interactions:
  - pan;
  - zoom;
  - fit-to-view;
  - optional keyboard/mouse shortcuts.
- Only show ROI authoring tools inside a dedicated ROI edit mode/view.

Acceptance:

- Normal pattern raw page has no draw/edit/delete tool palette.
- Dedicated ROI editing still has clear controls if that workflow remains in scope.

### 6. Fix Repeated QPainter Warnings During Matching Result Display

Current issue:

- Matching result display emits repeated `QPainter` painter-not-active warnings.

Likely cause to audit:

- `VisionCanvas::setImage(const cv::Mat&)` converts unsupported or empty `cv::Mat` to a null `QPixmap`.
- `VisionCanvas::setImage(const QPixmap&)` then still creates or updates a pixmap item with that null pixmap.
- Runtime result paths may pass empty `rawImage` or unsupported image format while still trying to paint.

Required fix:

- Guard null/empty pixmaps before adding them to the scene.
- Treat empty/unsupported images as `clearImage()` or a controlled no-image state.
- Ensure result display falls back only when the fallback image is non-empty and supported.
- Do not leave a null pixmap item in the scene.

Suggested touchpoints:

- `src/ui/widgets/vision/vision_canvas.cpp`, `setImage(const cv::Mat&)`, `setImage(const QPixmap&)`, `clearImage()`.
- `src/ui/widgets/vision/vision_geometry.cpp`, `pixmapFromMat()`.
- `src/ui/forms/task/localization_dashboard_widget.cpp`, `updateCycleResult()`.
- `src/ui/forms/task/localization_patterns_widget.cpp`, `displayResultOverlay()`.

Acceptance:

- Matching result display produces no repeated `QPainter` warnings.
- Empty/unsupported images do not create invalid scene paint devices.

### 7. Documentation Placement Must Be Correct

Current issue:

- Implementation progress was written at the top of this plan and the status was changed to implemented before PM acceptance.

Required fix:

- Keep the top of this file as the planning and review authority.
- Put implementation notes only under `Implementation Record`.
- Do not change status to accepted/implemented until the PM review passes.

### 8. Merge ROI Editing Into Existing Workspace Edit Flow

Current issue:

- A separate "Edit ROI" page was introduced inside the Pattern widget.
- The project already has a workspace edit dialog for workspace ROI and condition ROI.
- Having both creates duplicate workflow and unclear ownership.

Required fix:

- Remove or stop exposing the separate Pattern-widget "Edit ROI" page for workspace ROI editing.
- Use the existing workspace edit dialog as the only workflow for editing workspace ROI and condition ROI.
- The workspace edit dialog does not need major interaction changes for this rework.
- Add clear text labels inside the workspace edit view so the user can distinguish:
  - workspace ROI;
  - condition ROI.

Suggested touchpoints:

- `src/ui/forms/task/workspace_setting_dialog.*`.
- `src/ui/forms/task/localization_patterns_widget.*`.
- `src/ui/widgets/vision/vision_canvas.*` if the same label rendering utility can be reused for read-only ROI labels.

Acceptance:

- There is one clear workspace ROI editing flow.
- Workspace ROI and condition ROI are visually labeled in the workspace edit dialog.
- Pattern widget no longer exposes a duplicate ROI-edit page for the same workspace concept.

### 9. Add Direction And Status Locator Arrows To Result Overlay

Current issue:

- The output/result overlay is generally acceptable, but it is missing orientation cues.
- In dense detection scenes, users can have difficulty finding the status label/chip that belongs to a bounding box.

Required fix:

- At each detected object's center, draw X/Y direction arrows that show the object's orientation.
- From the bounding box top-left corner, draw two small locator arrows pointing outward toward the status/label area.
- These arrows are mandatory visual aids and do not need user-facing on/off toggles.
- The arrows must remain visible when zoomed out and must not obscure the object center excessively.

Suggested touchpoints:

- `src/ui/widgets/vision/vision_canvas.cpp`, result overlay rendering.
- `src/ui/widgets/vision/vision_overlay_types.h` only if extra style/status metadata is needed.

Acceptance:

- Each detected object has visible direction arrows at the center.
- The top-left locator arrows make the corresponding status/label easier to find in dense scenes.
- No new toggle is added for these mandatory aids.

### 10. Link Result Table Row Selection To Overlay Highlight

Current issue:

- The result overlay and match-result table are not interactively linked.
- Users need to inspect one row and immediately find its object in the image.

Required fix:

- When a user clicks a match-result row, highlight the corresponding overlay object.
- Reduce the visual prominence of all non-selected overlay objects while a row/object is selected.
- Pressing `Esc` while focus is on the image/result widget should clear the selection and restore normal overlay display.
- The behavior should work for Pattern matching result table first; apply the same pattern to Dashboard result table if the same viewer/table contract is available.

Suggested touchpoints:

- `src/ui/widgets/vision/vision_result_viewer_widget.*`.
- `src/ui/widgets/vision/vision_canvas.*`.
- `src/ui/widgets/vision/vision_overlay_types.h`.
- `src/ui/forms/task/localization_patterns_widget.cpp`, result table wiring.
- `src/ui/forms/task/localization_dashboard_widget.cpp`, dashboard result table wiring if feasible in the same pass.

Acceptance:

- Clicking a result table row highlights the matching overlay object.
- Other overlay objects become visually muted while one object is selected.
- Pressing `Esc` on the image/result widget clears the highlight and restores normal overlay rendering.

### 11. Add Matched Object Hover/Click Interaction On Image View

Current issue:

- Matched objects can be inspected through the table, but the image view itself does not provide enough direct interaction.
- Overlay labels/status are currently positioned at the bottom-left, which is harder to scan with the new top-left locator-arrow convention.

Required fix:

- Add hover feedback for matched objects on the image view.
- When the user clicks a matched object on the image view:
  - select that object;
  - highlight it in the image view;
  - select and visually emphasize the corresponding row in the result table.
- When an object is selected, hide or disable all per-object overlays for other objects. Non-selected object geometry may stay visible only if it is clearly muted and does not compete with the selected object.
- Pressing `Esc` on the image/result widget clears image selection and restores all object overlays.
- Move each object's overlay label/status chip to the top-left anchor of its bounding box. It should align with the top-left locator arrows from item 9.

Suggested touchpoints:

- `src/ui/widgets/vision/vision_canvas.*`.
- `src/ui/widgets/vision/vision_result_viewer_widget.*`.
- `src/ui/widgets/vision/vision_overlay_types.h`.
- `src/ui/forms/task/localization_patterns_widget.cpp`, result table selection wiring.
- `src/ui/forms/task/localization_dashboard_widget.cpp`, dashboard table selection wiring if feasible in the same pass.

Acceptance:

- Hovering a matched object gives clear visual feedback.
- Clicking a matched object selects the matching table row.
- Clicking a table row and clicking the object produce the same selected state.
- While selected, other objects' labels/status overlays are hidden or disabled so the selected object is visually dominant.
- `Esc` clears selected object state and restores normal overlays.
- Overlay label/status chips are anchored at the object's bounding-box top-left, not bottom-left.

### 12. Add Optional Picking Box Overlay

Current issue:

- The result overlay shows detected object geometry, but it does not expose the picking box geometry used to evaluate gripper/pickability context.
- Operators need a way to inspect picking boxes without permanently cluttering the result view.

Required fix:

- Add optional picking box overlay rendering for matched objects.
- The overlay must be controlled by a user-facing option/toggle in the result viewer.
- When enabled, draw the picking box geometry associated with each matched object.
- When an object is selected, picking box visibility should follow the same selection policy as other per-object overlays:
  - selected object stays emphasized;
  - non-selected object picking boxes are hidden or clearly muted.
- If the current DTO/adapter does not expose enough picking box geometry, extend the UI-facing result DTO/adapter without changing backend ownership.

Suggested touchpoints:

- `src/ui/widgets/vision/vision_overlay_types.h`.
- `src/ui/widgets/vision/vision_result_adapter.*`.
- `src/ui/widgets/vision/vision_canvas.*`.
- `src/ui/widgets/vision/vision_result_viewer_widget.*`.
- `src/matching/match_object.h` only for reading existing public geometry; do not redesign matcher ownership in this task.

Acceptance:

- Result viewer has an option to show/hide picking box overlays.
- Picking boxes render in image coordinates and remain readable across zoom levels.
- Picking box overlays participate correctly in selected-object isolation.

## Goal

Build reusable frontend vision widgets for localization workspace visualization/editing and runtime result visualization.

The implementation must keep workspace ROI editing in the existing workspace edit flow, then view runtime match results as overlays on the raw image. The runtime path should stop relying on pre-rendered result images from `ImageMatcher` for visualization; instead, the frontend widget renders overlays from UI-facing DTOs/adapters.

## Phase 1 Scope

Implement in this phase:

- Reusable widgets under `src/widgets`, preferably `src/ui/widgets/vision/`.
- Shared image canvas with stable image-pixel coordinate mapping.
- Compact tool palette only where a real edit workflow needs it.
- Numeric inspector only where selected ROI coordinate editing is meaningful.
- Existing axis-aligned workspace ROI and condition ROI editing remains in `WorkspaceSettingDialog`.
- No new rotated-ROI or pattern-ROI editor page should be exposed in this rework.
- Read-only runtime result overlay rendering.
- Integration into `LocalizationPatternsWidget` without turning the normal raw page into an always-on ROI editor.
- Workspace ROI/condition ROI editing remains merged into the existing workspace edit dialog.
- Integration into `LocalizationDashboardWidget` for runtime result display.
- UI-facing DTOs/adapters for ROI and match-result overlays.
- Existing-config persistence where an existing field/config already supports the required data.

Defer:

- Polygon mask editor.
- Circle/ellipse ROI.
- Free polygon ROI.
- Multiple ROI workflows.
- Hierarchical ROI for inspection tasks.
- Backend schema changes, unless explicitly approved later.
- Streaming/live preview performance work. Phase 1 targets still images and last runtime result only.

## Existing Code Touchpoints

- `src/ui/widgets/image_widget/image_widget.*`  
  Existing editable `QGraphicsView` with image loading, pan/zoom, normal ROI, rotated ROI, right-click menu.
- `src/ui/widgets/image_widget/image_view_only.*`  
  Existing read-only-ish image view with simple rectangle overlays.
- `src/ui/widgets/image_widget/item_roi.*`  
  Existing axis-aligned ROI item with handles.
- `src/ui/widgets/image_widget/item_roi_rotated.*`  
  Existing rotated ROI item with rotate handle.
- `src/ui/forms/task/localization_patterns_widget.*`  
  Pattern management, test image input, raw/result/binary monitor tabs, match result table.
- `src/ui/forms/task/localization_dashboard_widget.*`  
  Runtime dashboard, match view, result rows, condition ROI overlay.
- `src/matching/image_matcher.h`  
  `mtc::MatchResult` contains `Objects`, `cropOffsetPoint`, `ExecutionTime`, `Image`, image size, match/fault flags.
- `src/matching/match_object.h`  
  `mtc::MatchedObject` contains corners, center, point angle, match angle, score, pattern id/name, pickability/collision/condition ROI flags.
- `src/model/camera_workspace.h`  
  Existing persisted axis-aligned camera workspace ROI and condition ROI.
- `src/matching/match_pattern_config.h`  
  Existing pattern identity/search/pick config and raw training image. No confirmed dedicated pattern ROI field at this planning point.

Do not delete `ImageWidget` or `ImageViewOnly` in this pass. Migrate localization callers onto the new vision widgets first, then mark old usages as compatibility paths if still needed by dialogs.

## Target Architecture

Create a small vision widget module:

```text
src/ui/widgets/vision/
  vision_geometry.h/.cpp
  vision_overlay_types.h
  vision_canvas.h/.cpp
  vision_tool_palette.h/.cpp
  vision_numeric_inspector.h/.cpp
  vision_roi_editor_widget.h/.cpp
  vision_result_viewer_widget.h/.cpp
  vision_result_adapter.h/.cpp
  vision_roi_config_adapter.h/.cpp
```

The file names can change if the implementation finds a better local naming style, but the boundaries should remain.

## Data Contracts

`VisionRoi`:

- `QString id`
- `QString label`
- `VisionRoiShape shape`
- `QPointF center`
- `QSizeF size`
- `double angleDeg`
- `bool editable`
- `bool visible`
- Optional display/state fields: color role, selected, warning.

Supported phase 1 shapes:

- `AxisAlignedRect`
- `RotatedRect`

Coordinate rules:

- Store geometry in image pixel coordinates.
- Store sizes in image pixels.
- Store angle in degrees.
- The adapter must document and test the sign convention when mapping between Qt item rotation and matcher result angle. Do not silently flip angle direction in the canvas.

`VisionResultObject`:

- `int index`
- `QString patternName`
- `int patternIndex`
- `double score`
- `QPointF center`
- `QVector<QPointF> corners`
- `double matchedAngleDeg`
- `double pointAngleDeg`
- `bool rejected`
- `bool faulted`
- `bool sentToOutput`
- `bool possibleToPick`
- `bool hasCollision`
- `bool outsideConditionRoi`
- Stable id/index suitable for synchronizing image selection with result-table rows.
- Optional picking box geometry for overlay rendering, when available.

`VisionResultOverlay`:

- Source image size.
- Accepted objects.
- Rejected candidates, if available.
- Optional picking point markers.
- Optional picking box geometry.
- Optional condition/workspace ROI overlays.
- Optional runtime signal values.
- Visibility flags for score, angle, pattern id, rejected candidates, fault markers, sent-to-output markers, picking boxes, runtime signal values.

## Widget Responsibilities

`VisionCanvas`

- Owns the image scene/view.
- Loads `QPixmap` and `cv::Mat`.
- Handles pan, zoom, fit to view.
- Maintains image-to-scene and scene-to-image transforms.
- Renders image, ROI items, result overlays, and optional debug layers.
- Emits selected ROI and ROI changed signals.
- Supports read-only mode.
- Must guard against empty/unsupported images.

Implementation note: it is acceptable to reuse/refactor `ImageWidget`, `ImageViewOnly`, `ItemRoi`, and `ItemRoiRotated` internally. New callers should depend on the new vision-facing API, not on legacy right-click/menu behavior.

`VisionToolPalette`

- Provides compact controls only for a meaningful edit mode:
  - select/move
  - draw axis-aligned rectangle
  - draw rotated rectangle
  - delete selected ROI
  - fit to view
  - pan/zoom mode
  - undo
  - redo
- Polygon-related actions should not be shown or should be disabled in phase 1.
- The palette must be stateful: selected tool reflects the active canvas mode.
- The palette should not appear on the normal raw monitor page.
- The palette should not introduce a duplicate workspace ROI edit page in the Pattern widget.

`VisionNumericInspector`

- Shows and edits selected ROI values:
  - center x/y
  - width/height
  - angle
- Uses image pixel units.
- Edits must update the canvas item.
- Canvas drag/resize/rotate must update the inspector.
- Must clamp invalid values to the image bounds or reject them with a visible validation state.
- Do not add this inspector to the workspace dialog in the current rework unless a later requirement explicitly asks for numeric workspace editing.

`VisionRoiEditorWidget`

- Composition widget for ROI setup/editing when a dedicated edit workflow is actually needed.
- Contains `VisionToolPalette`, `VisionCanvas`, and `VisionNumericInspector`.
- Public API:
  - setImage / clearImage
  - setRois / rois
  - setReadOnly
  - selectedRoi
- signals for ROI added/changed/deleted/selection changed.
- Do not expose it as a separate Pattern-widget workspace ROI page in the current rework.

`VisionResultViewerWidget`

- Composition widget for runtime/result viewing.
- Read-only by default.
- Contains canvas plus compact overlay controls.
- Public API:
  - setImage
  - setOverlay
  - setOverlayVisibility
  - selectObject / clearObjectSelection, or equivalent selection API
  - clearResult
- Emits an object-selected signal when the user clicks a matched object in the image view.

## Adapter Responsibilities

`VisionResultAdapter`

- Converts `mtc::MatchResult` to `VisionResultOverlay`.
- Converts `LocalizationRuntimeController::CycleResult` to `VisionResultOverlay` where needed.
- Handles crop offset explicitly.
- Does not mutate matcher output.
- Does not draw into `cv::Mat`.

Required mapping:

- `MatchedObject::point_LT/RT/LB/RB` to overlay corners.
- `MatchedObject::point_Center` to picking/result center.
- `MatchedObject::matched_Score` to score label.
- `MatchedObject::matched_Angle` and `point_angle` to angle labels.
- `pattern_index` / `pattern_name` to id/name labels.
- `hasCollision`, `isOutsideConditionRoi`, `isPossibleToPick` to status/fault/rejected visual states.
- Existing picking box geometry to optional picking box overlay DTO fields, when available.

`VisionRoiConfigAdapter`

- First audit for existing config support:
  - Camera workspace axis-aligned ROI already exists in `CameraWorkspace`.
  - Pattern-specific ROI/rotated ROI is not confirmed in `MatchPatternConfig`.
- If an existing field exists, use it.
- If no field exists, keep ROI persistence behind this adapter and document the missing backend field as a follow-up. Do not add backend schema fields in phase 1 without PM approval.

## Implementation Steps

### Step 0 - Preflight Audit

- Confirm exact legacy widgets used by pattern setup and dashboard `.ui`.
- Confirm whether pattern-specific ROI persistence exists anywhere outside `MatchPatternConfig`.
- Record the persistence decision in this plan or a short implementation note.

Exit gate:

- Agent can state exactly which existing config is reused and which missing persistence remains deferred.

### Step 1 - Add Vision DTOs And Geometry Helpers

- Add `vision_overlay_types.h`.
- Add `vision_geometry.h/.cpp`.
- Add qmake entries in `ncr_picking.pro`.
- Verify DTOs do not include UI widget headers.

### Step 2 - Build VisionCanvas Foundation

- Load and clear images safely.
- Fit to view.
- Pan/zoom.
- Convert viewport/scene/image coordinates.
- Render read-only result overlays.
- Expose overlay visibility flags.

Exit gate:

- Can display a still image.
- Can draw overlay corners/center/labels on the correct image positions.
- Empty images do not create invalid pixmap items or painter warnings.

### Step 3 - Add Dedicated Edit-Mode Tool Palette And ROI Editing

- Wire tools to canvas modes.
- Add axis-aligned rectangle draw flow.
- Add rotated rectangle draw flow.
- Support select, move, resize handles, rotate handle, delete selected ROI.
- Add undo/redo for create, delete, move, resize, rotate.

Exit gate:

- Existing code may keep these reusable capabilities for future dedicated ROI workflows.
- The current Pattern-widget workspace ROI rework must not expose a duplicate edit page.
- Workspace ROI editing remains in `WorkspaceSettingDialog`.

### Step 4 - Add Numeric Inspector

- Bind selected ROI to spin boxes.
- Update inspector on canvas changes.
- Update canvas on inspector edits.
- Clamp center/size/angle to valid image-space values.
- Avoid mouse wheel accidental edits by using existing no-wheel spinbox classes where useful.

### Step 5 - Compose Reusable Widgets

- Add `VisionRoiEditorWidget`.
- Add `VisionResultViewerWidget`.
- Ensure both widgets are usable from `.ui` promotion or programmatic insertion.
- Keep form widgets from manipulating internal `QGraphicsItem`s.

### Step 6 - Integrate Pattern Setup

Primary target:

- `src/ui/forms/task/localization_patterns_widget.*`
- `src/ui/forms/task/localization_patterns_widget.ui`

Required behavior:

- Normal raw page is read-only preview plus optional workspace/condition ROI overlays.
- Do not expose a separate Pattern-widget "Edit ROI" page for workspace ROI editing.
- Workspace ROI and condition ROI editing must use the existing workspace edit dialog.
- Workspace edit dialog must label workspace ROI and condition ROI clearly in the image view.
- Keep manual image loading.
- Keep camera image input through `setCameraImage(const cv::Mat &image)`.
- Show selected pattern raw image in the correct view.
- Save/reload ROI through `VisionRoiConfigAdapter` where supported.
- Keep existing result table behavior.

Result test view:

- When running a matching test, render raw image plus `VisionResultOverlay`.
- Stop using `mtc::MatchResult::Image` for user-facing visualization in this path, except as temporary fallback during migration if documented.

### Step 7 - Integrate Runtime Dashboard

Primary target:

- `src/ui/forms/task/localization_dashboard_widget.*`
- `src/ui/forms/task/localization_dashboard_widget.ui`

Required behavior:

- Replace `gv_match_view` display with `VisionResultViewerWidget` or promote the `.ui` placeholder.
- Convert `CycleResult` or underlying `MatchResult` data to `VisionResultOverlay`.
- Prefer raw image display plus overlay.
- Preserve existing result table, KPI labels, lamps, fault panel, and task log.
- Keep dashboard read-only.
- Wire result table row selection to result viewer object selection where feasible.
- Wire result viewer object clicks back to table selection where feasible.

### Step 8 - Styling And Theme Compliance

- Follow `docs/rules/ui_design_rules.md`.
- Use `@{token}` theme tokens from `docs/rules/ui_theme_tokens.md` for app-surface styling.
- Use image-safe colors/backing for image overlays.
- Use dynamic properties for visual states.
- Do not introduce dark-only custom painting.
- Do not put widget-specific objectName selectors into global QSS.

### Step 9 - Verification

Build:

- Use `.codex/LOCAL_BUILD_GUIDE.md` for this local machine.
- Use `docs/rules/build_and_verification.md` for build-folder rules.
- Root app builds under root `build\`.
- Tests build beside their `.pro`.

Minimum verification:

- qmake configure for root app.
- Focused compile of changed files or full app build if feasible.
- Architecture contract test when DTO/adapters touch architecture boundaries.
- Manual smoke through the relevant forms if the app launches locally.

Functional checklist:

- Load pattern image manually.
- Load pattern/raw image from camera path where available.
- Normal raw page has no draw/edit/delete palette.
- Workspace edit dialog remains the single workspace/condition ROI edit flow.
- Workspace edit dialog labels workspace ROI and condition ROI.
- Fit/pan/zoom image.
- Run matching test and see raw image plus overlays.
- Result overlay shows object-center X/Y direction arrows.
- Result overlay shows top-left locator arrows for status/label discovery.
- Result viewer can toggle picking box overlays on/off.
- Clicking a match-result table row highlights the matching overlay object.
- Hovering a matched object gives visual feedback.
- Clicking a matched object selects and emphasizes the matching result-table row.
- When one object is selected, other objects' per-object overlays are hidden or disabled.
- `Esc` on the result image clears overlay highlight.
- Object overlay label/status chips are anchored at bounding-box top-left.
- Dashboard shows last result with overlays.
- Toggle score/angle/pattern id/rejected/fault/sent markers.
- Switch dark/light theme and verify readability.
- Matching result display emits no repeated `QPainter` warnings.

## Acceptance Criteria

The phase is accepted when:

- Reusable vision widgets exist under `src/widgets`.
- Pattern setup uses the new vision widgets without making the normal raw page an always-on ROI editor.
- Workspace ROI editing is not duplicated in a Pattern-widget edit page and remains in the existing workspace edit dialog.
- Runtime dashboard uses the new read-only result viewer path.
- Axis-aligned workspace ROI and condition ROI remain editable through the existing workspace edit flow.
- Tool palette is compact and appears only where it is meaningful.
- Numeric inspector is not exposed in the normal Pattern raw/result workflow.
- Runtime/pattern matching result overlays are rendered by the frontend widget on raw images.
- Overlay labels and handles remain readable/usable across image content and zoom levels.
- Result overlays include mandatory object direction arrows and top-left status locator arrows.
- Result viewer supports optional picking box overlays.
- Result table row selection highlights the corresponding overlay object and `Esc` clears the highlight.
- Matched object hover/click interaction works in the image view.
- Clicking a matched object selects the corresponding result-table row.
- Object overlay label/status chips are positioned at bounding-box top-left.
- Existing backend behavior is preserved.
- Any missing persistence support is documented precisely instead of hidden.
- qmake build verification and manual UI smoke notes are recorded by the implementing agent.

## Review Checklist For PM

Review these before accepting the agent's implementation:

- No backend schema changes were made without explicit approval.
- `LocalizationPatternsWidget` and `LocalizationDashboardWidget` do not reach into raw `QGraphicsItem` internals.
- `ImageMatcher::DrawMatchResult` or `MatchResult::Image` is not the primary visualization path for the migrated UI.
- Angle/crop-offset mapping is tested or at least demonstrated with a synthetic result.
- Old `ImageWidget`/`ImageViewOnly` are either still used only by unrelated legacy dialogs or have a clear migration note.
- QSS follows token/dynamic-property rules.
- Image overlay labels are image-safe, not theme-foreground-only.
- Workspace/condition ROI editing is not duplicated outside `WorkspaceSettingDialog`.
- Result-table selection and overlay selection stay synchronized.
- Image-view object click and result-table row click produce the same selected state.
- Selected object state hides or disables other objects' per-object overlays.
- Picking box overlay toggle works and follows selected-object isolation.
- Matched object hover feedback is visible but not distracting.
- Build outputs follow the documented build-folder layout.

## Known Risks

- **Pattern ROI persistence may not exist yet.** `CameraWorkspace` persists axis-aligned camera ROI, but `MatchPatternConfig` currently has no confirmed pattern ROI field. Do not hide this. Use an adapter boundary and document follow-up if needed.
- **Angle convention can drift.** Qt graphics rotation, matcher `matched_Angle`, and `point_angle` may use different sign/origin conventions. Add an explicit adapter test.
- **Crop offset can misplace overlays.** Runtime matching can use a workspace ROI. The adapter must account for `MatchResult::cropOffsetPoint` and whether the displayed image is full-frame or cropped.
- **Existing result rows must stay stable.** Overlay migration should not regress KPI/table/log behavior.
- **Undo/redo scope can expand.** Keep phase 1 undo/redo limited to ROI create/delete/geometry edits.

## Implementation Record

- 2026-06-27: First implementation added `src/ui/widgets/vision/` and integrated it into `LocalizationPatternsWidget` and `LocalizationDashboardWidget`.
- 2026-06-27: Implementing agent reported architecture contract test passed after adapter coverage for crop-offset/result mapping.
- 2026-06-27: Implementing agent reported focused root-app compile of changed widgets/forms passed.
- 2026-06-27: Project owner confirmed full app compile completed with no errors.
- 2026-06-28: PM review requested rework for overlay readability, raw-page UX, toolbar density, zoom-stable item rendering, and repeated `QPainter` warnings.
- 2026-06-28: Project owner confirmed compile has no errors after local build/test, then requested additional UX rework: merge ROI editing into workspace edit flow, add result direction/status locator arrows, and wire result-table row selection to overlay highlighting.
- 2026-06-28: Project owner confirmed compile has no errors after another local build/test, then requested image-view interaction rework: matched-object hover, image-click selection synced to the result table, selected-object overlay isolation, and moving object overlays to bounding-box top-left.
- 2026-06-28: Project owner requested optional picking box overlay rendering in the result viewer.
- 2026-06-28: Project owner confirmed compile/manual test result is acceptable. PM spot-check confirmed the phase rework is implemented and the plan can be closed for this phase.

## Completed Implementation Breakdown

Implemented in this phase:

1. Fix empty image/null pixmap handling and `QPainter` warnings.
2. Fix image-safe overlay label styling and zoom-stable overlay drawing.
3. Remove duplicate Pattern-widget ROI edit page and keep workspace ROI editing in the existing workspace dialog with labels.
4. Add mandatory result direction arrows and top-left status locator arrows.
5. Wire match-result table selection to overlay highlight and `Esc` reset.
6. Add image-view matched-object hover/click selection and table synchronization.
7. Move object overlay labels/status chips to bounding-box top-left.
8. Add optional picking box overlay toggle and rendering.
9. Compact result overlay controls where they still feel heavy.
10. Re-run architecture contract test, full compile, and manual UI smoke before commit/release handoff.
