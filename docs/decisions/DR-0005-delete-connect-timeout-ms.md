# DR-0005: Delete `LocalizationRecoveryPolicy::connectTimeoutMs`; defer persisting policy values

**Status:** Implemented — landed in Phase 9 Task Z1
**Date accepted:** 2026-09-07
**From:** no PQ card — imported from [`docs/history/plan/phase_9_implementation_plan.md`](../history/plan/phase_9_implementation_plan.md) → "Decisions taken with the project owner (locked 2026-09-07)", row D5
**Cluster:** recovery
**Supersedes / replaces text in:** `docs/domains/task_localization/runtime_controller_api.md` → "Fault And Recovery" (gained the statement that no production caller of `setRecoveryPolicies()` exists, naming the injection point); the `LocalizationRecoveryPolicy` blocks in `uml/04_localization_task.puml` and `uml/03_runtime_threading.puml` (the field's line removed) — per the plan's Z1 landing record.

## Question
Does the dead `connectTimeoutMs` field stay, and are recovery-policy values persisted?

## Decision
Delete `LocalizationRecoveryPolicy::connectTimeoutMs` (dead). Defer persisting policy values.

## Owner's ruling (verbatim)
> Recorded in English in the Phase 9 plan; the decision text above is that record.

## Rationale
Its only reader is an assertion at `tests/architecture_contract_test/main.cpp:2195`; `setRecoveryPolicies()`'s only caller is `:2257`. The product ships on defaults.

## Rejected options and why
Not recorded in the source.

## Contract changes
Not recorded in the source.

## Verification
- Ring 1 (table tests): `test_localization_recovery_policy_defaults_match_runtime_spec` — still asserts every surviving default; the plan's Z1 landing record confirms the case count did not drop (contract test 159 / 0, shape 157 == 157).
- Ring 2 (owner-run script): none planned by the source. The plan's Task Z1: "No negative check applies — nothing is added. The build and the search are the evidence."
- Ring 3 (log replay): not applicable to a deletion; the source records instead the search for `connectTimeoutMs` over `src\`, `components\app\`, `runtime_app\`, `tests\`, `uml\` and `docs\` (excluding `docs\generated\` and `docs\history\`), which finds two deliberate hits: the removal notes in `localization_recovery_policy.h` and `runtime_controller_api.md`.

## Implementation
- Symbols touched (per the plan's Z1 landing record): `LocalizationRecoveryPolicy::connectTimeoutMs` removed from `localization_recovery_policy.h`; its single assertion removed from `test_localization_recovery_policy_defaults_match_runtime_spec`; both UML diagrams (which also lost stale `maxRetries`, `canRetry(currentRetryCount)` and `EscalateFault` entries, removed when reconnect became unbounded in Phase 6 / B1 — recorded as a deviation); a doc comment on `setRecoveryPolicies()` and `runtime_controller_api.md` → "Fault And Recovery" stating that the product never sets recovery policies, the defaults are what ships, and naming the injection point `TaskLocalization::setupRuntimeController()` immediately before `controller->setup(context)`. The `.cpp` doc on `setRecoveryPolicies()` was corrected: a policy set after `bindRoleContext()` has copied it changes nothing until the role is bound again.
- Backlog items closed: none named by the source for Z1. Related, filed by Task Z3: backlog item 67 (recovery policies not settable), which carries the deferred persistence.
- Git tag: Not recorded in the source.

## History (what this replaces)
Imported from the Phase 9 implementation plan on 2026-09-16; that plan remains the primary record of the discussion. The Decision and Rationale above are copied unchanged, including the plan's own source line references, which are as of 2026-09-08 and are not maintained here. The Z1 landing record notes that the assertion was at `main.cpp:2922` and the caller at `main.cpp:2983` by the time the task ran — the rationale's `:2195` and `:2257` were already stale then.

What this replaces: `LocalizationRecoveryPolicy` carried a `connectTimeoutMs` field that nothing read except one test assertion, and both UML diagrams listed it. Every role runs on `defaultCameraRecoveryPolicy()` / `defaultPlcRecoveryPolicy()` / `defaultVisionOutputRecoveryPolicy()`; persisting policy values stays deferred.
