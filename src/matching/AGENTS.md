# Module: matching (level 1)

**Purpose.** Pattern-matching engine (OpenCV): `ImageMatcher`, match
patterns/groups/params, `PatternGroupManager`, matching result types,
vision utilities.

**Public surface.** `image_matcher.h`, `match_group.h`, `match_pattern.h`,
`pattern_group_manager.h`, `matching_types.h`, `manager_result.h`,
`robot_picking_checker.h` (interface consumed by model).

**May include.** Qt, OpenCV, `core/`, other `matching/` headers.
**Must NOT include.** UI, `model/`, `runtime/`, `device/`. Enforced by the
architecture contract test. The property-browser adapter for match configs
lives UI-side in `src/widgets/property_browser/match_config_property_adapter.*`
— keep it there.

**Invariants.**
- Matching runs on a worker thread (`matchingRunner`); callers must hand the
  worker an isolated deep copy (`TaskLocalization::snapshotPatternGroup()`),
  never the live `PatternGroupManager` group.
- New edge-match config fields follow the recipe in `edge_match_config.h`
  (one entry in `kEdgeSpecs[]` in the UI-side adapter).

**Verify.** `tests/architecture_contract_test`, root app build.

**Build registration.** `src/matching/matching.pri` only.
