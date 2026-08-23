# Project Restructure Plan

**Status:** Closed initial restructure roadmap; follow-up implementation backlog is active  
**Last updated:** 2026-06-24  
**Audience:** project owner, project manager, tech lead, and coding agents

## Purpose

This plan organizes the refactor into staged, verifiable phases. The project is
not treated as a blank rewrite: the current code already contains the main
production architecture for the first Localization release, including device
families, task/device bindings, runtime runners, localization runtime control,
robot kinematic checking, UI pages, persistence, and architecture contract
tests.

The goal was to reduce delivery risk by first fixing source-of-truth drift,
then proving the current baseline, then hardening the runtime path, and only
then doing scoped structural cleanup. The initial roadmap is now closed; future
work should start from [restructure_closeout.md](../history/restructure/restructure_closeout.md) and
[technical_debt_and_next_steps.md](../backlog/technical_debt_and_next_steps.md).

## First-Release Scope

- Application domain: industrial Qt/C++ vision picking configuration and
  runtime operation.
- First task: `TaskLocalization`.
- First supported physical integrations: Basler GigE camera, Mitsubishi MC PLC,
  VisionOutput TCP/IP server/client, and advisory RobotKinematics checking for
  outgoing pick poses.
- Deferred or stubbed integrations: robot communication runner, Huayan robot,
  VisionSerial, BaslerUSB, Realsense, custom calibration-board authoring, and
  customer installer implementation.

## Source Of Truth

Use this hierarchy when documents disagree:

1. Source code and tests for implemented behavior.
2. Root `AGENT.md` for session entry, reading order, and agent operating rules.
3. `docs/rules/design_rules.md` for process, language, and general architecture
   conventions.
4. `docs/rules/ui_design_rules.md` for UI/QSS implementation workflow, and
   `docs/rules/ui_theme_tokens.md` for token names, values, palette rationale, and
   colour-scheme migration workflow.
5. `docs/domains/task_localization/` and `docs/architecture/robot_kinematics_module.md` for current
   module-level behavior.
6. `uml/*.puml` for structural contracts that should mirror the code.
7. `docs/backlog/later_todo_list.md` for deferred work only; resolved or superseded
   items must not remain indistinguishable from open work.
8. `docs/generated/architecture_docs/` for API reference only. If an entry is stale, fix
   it in Phase 0 or mark it for removal/regeneration.

## Backlog Status Rules

Every backlog entry should carry one of these statuses:

- `Proposed`: useful idea, not yet approved for implementation.
- `In Progress`: actively being changed.
- `Implemented-Unverified`: code exists, but required build/test/manual
  verification is still missing.
- `Verified`: implementation and required verification both passed.
- `Superseded`: replaced by a newer design or document.
- `Removed`: intentionally deleted because it was generated, obsolete, or
  harmful as current guidance.

Do not mark an item `Verified` based only on code inspection.

## Phase 0 - Documentation And Governance Cleanup

**Status:** Completed.

**Goal.** Make the project navigable again before any larger refactor. This
phase removes misleading guidance, records the roadmap, and separates real open
work from stale historical notes.

**Tasks.**

- P0.1 Create this restructure plan in `docs/`.
- P0.2 Repair high-confidence documentation drift:
  - `uml/README.md` must describe `components/RobotKinematics`, not the removed
    standalone `rkin` module.
  - `docs/generated/architecture_docs/ITask.md` must use the current `TaskState` enum and
    commission/runtime lifecycle.
  - `uml/06_ui_widgets.puml` must reflect the implemented working ROI and
    condition ROI behavior.
  - Inline comments that contradict current behavior should be fixed when they
    are source-of-truth comments, not merely old session notes.
- P0.3 Add a Phase 0 documentation audit that lists completed fixes, deletion
  candidates, and open product decisions.
- P0.4 Normalize backlog statuses for obviously stale items found during the
  audit. Do not silently close uncertain items.
- P0.5 Identify generated or obsolete docs that can be deleted later. Delete
  only files with clear ownership and no remaining traceability value.

**Acceptance criteria.**

- New contributors can identify the current architecture without reading stale
  `rkin`, old `TaskState`, or old ROI guidance.
- The roadmap has phase gates and verification expectations.
- Deletion candidates are recorded even when not deleted in the same pass.
- No functional code behavior changes are mixed into Phase 0.

**Verification.**

- Search for stale terms after edits: old `rkin` architecture text, old
  `TaskState` list, stale ROI TODO references, and comments saying
  condition ROI is data/UI only.

**Result.** Completed. See
[phase0_documentation_audit.md](../history/restructure/phase0_documentation_audit.md).

## Phase 1 - Baseline Verification

**Status:** Completed.

**Goal.** Prove the current baseline before refactoring behavior.

**Tasks.**

- Build and run `tests/architecture_contract_test`.
- Run a full Debug app build with the expected Qt/MSVC/OpenCV environment.
- Capture the exact kit/toolchain and any environment assumptions.
- Update backlog items from `Implemented-Unverified` to `Verified` only when
  their verification is actually executed.
- Decide whether architecture reference docs in `docs/generated/architecture_docs/` are
  hand-maintained, regenerated, or removed.

**Acceptance criteria.**

- The project has a known build/test baseline.
- Architecture contract tests either pass or produce a prioritized failure list.
- The backlog no longer claims completed work without verification evidence.

**Result (2026-06-24).** Phase 1 baseline verification passed for the current
MSVC/Qt Debug baseline. See
[phase1_baseline_verification.md](../history/restructure/phase1_baseline_verification.md).

## Phase 2 - Localization Runtime Production Hardening

**Status:** Partially completed and verified; remaining items moved to
[technical_debt_and_next_steps.md](../backlog/technical_debt_and_next_steps.md).

**Goal.** Make the first commercial Localization runtime path reliable enough
for production validation.

**Tasks.**

- Add focused runtime/controller tests for trigger edge behavior, held-trigger
  suppression, trigger reset, grab timeout fault `102`, VisionOutput send
  failure fault `201`, invalid pattern fault `400`, invalid calibration fault
  `401`, and lost device handling during `RunningCycle`.
- Add PLC write behavior tests for valid `M` bit tags, valid `D` word tags, and
  invalid tag rejection once a fake PLC request sink exists.
- Verify active-camera switching remains runner-based and does not reconnect
  devices directly from the task object.
- Verify runtime matching latency and UI responsiveness; move matching off the
  controller call stack only if measurement shows it is needed.
- Run an operator UI pass against a real or simulated cycle: lamps, fault panel,
  KPIs, result table, task-local log, and read-only dashboard behavior.

**Acceptance criteria.**

- Localization runtime has automated coverage for critical success and fault
  paths.
- Manual or simulated operator verification is recorded.
- Remaining production risks are explicit and narrow.

**Progress (2026-06-24).** Added and verified focused runtime contract coverage
for invalid pattern/calibration setup faults, valid PLC output writes, invalid
PLC tag rejection, and camera loss during `RunningCycle`. See
[phase2_phase3_runtime_hardening.md](../history/restructure/phase2_phase3_runtime_hardening.md).
Operator UI verification, latency measurement, and the broader active-camera
switching refactor remain open.

## Phase 3 - Scoped Architecture Cleanup

**Status:** Low-risk cleanup pass completed and verified; broader cleanup moved
to [technical_debt_and_next_steps.md](../backlog/technical_debt_and_next_steps.md).

**Goal.** Improve maintainability where the current design already exposes real
friction. Avoid broad rewrites.

**Candidate work.**

- UI/QSS token mechanism and mechanical QSS migration.
- `IDeviceWidget` hardening: virtual destructor, `Q_DISABLE_COPY_MOVE`, and any
  remaining base-widget contract fixes.
- Replace unsafe widget `static_cast` downcasts with `qobject_cast` plus guarded
  failure handling.
- Extract shared gadget meta-property helpers if duplicate dispatch logic keeps
  growing.
- Review `TaskLocalization` naming and constants only after runtime tests are
  in place.
- Decide whether RobotKinematics should stay source-included through `.pri` or
  become a separately packaged library.

**Acceptance criteria.**

- Each cleanup has a narrow reason, owner, and verification path.
- UI changes pass `docs/rules/ui_design_rules.md` checklist.
- UML and docs are updated in the same change as structural edits.

**Progress (2026-06-24).** Verified a low-risk cleanup pass covering
`IDeviceWidget` destructor/copy-move hardening and guarded widget downcasts for
Basler and Vision TCP/IP device widgets. See
[phase2_phase3_runtime_hardening.md](../history/restructure/phase2_phase3_runtime_hardening.md).

## Phase 4 - Product Split And Commercial Packaging

**Status (2026-07-28):** ON HOLD. The plan and decision below stand, but Phase 4
is deferred — the product needs further development before a first release is
meaningful. The tasks and acceptance criteria in this section are not active
work. See the hold notice in
[phase4_product_release_plan.md](../product/phase4_product_release_plan.md).

**Status (2026-06-24):** Product/release plan defined; installer implementation
pending.

**Goal.** Turn the architecture into a deliverable product layout.

**Decision.**

The first release should ship as one Qt application with explicit
Commission/Runtime modes. Do not split into separate configuration/runtime
executables until the current operator runtime flow is validated. See
[phase4_product_release_plan.md](../product/phase4_product_release_plan.md).

**Tasks.**

- Define project-file ownership, runtime startup flow, and operator permissions.
- Define installer layout for app binaries, Qt/OpenCV/Basler dependencies,
  `RobotKinematics` DLLs, and `robot_assets/`; keep the clean-machine checklist
  in [customer_installer_packaging.md](../product/customer_installer_packaging.md).
- Verify RobotKinematics transitive DLLs on a clean target machine.
- Produce a release checklist: build, smoke test, device simulation, asset
  presence, license/dependency inventory, and rollback plan.

**Acceptance criteria.**

- A clean machine can run the packaged app with required assets.
- The runtime operator flow is documented and validated.
- The team has a release checklist that does not depend on tribal knowledge.

**Result.** Product shape, packaging payload, clean-machine gate, and deferred
two-executable criteria are documented in
[phase4_product_release_plan.md](../product/phase4_product_release_plan.md) and
[customer_installer_packaging.md](../product/customer_installer_packaging.md). Actual
installer implementation and clean-machine validation remain future work.

## Deletion Policy

Phase 0 may delete docs only when the file is clearly harmful or redundant as
current guidance. Prefer audit-first for historical material.

Good deletion candidates:

- generated HTML mirrors when the Markdown source is canonical;
- build outputs accidentally stored under `docs/`;
- scratch prompts that have been replaced by durable specs;
- stale API docs that cannot be repaired cheaply and are not referenced.

Keep or archive instead of deleting:

- old session summaries that are explicitly marked historical;
- request prompts that explain why a feature exists;
- docs linked from onboarding, design rules, or active module references.

## Refactor Gate

Phase 0 and Phase 1 gates have passed. Future broad work must still be sliced
through tests and release-risk gates: start with
[technical_debt_and_next_steps.md](../backlog/technical_debt_and_next_steps.md), keep
architecture contract tests green, and do not claim production readiness without
operator and clean-machine verification.
