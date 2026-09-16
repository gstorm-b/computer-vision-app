# Decision Log

The record of owner rulings that constrain design. When the project owner
decides how the runtime, the PLC handshake, the configuration or the backlog
must behave, the ruling is written here once, in English, quoting the owner's
own words verbatim, and every later document cites it by number instead of
restating it.

Read the index before proposing a behaviour change: a proposal that
contradicts an Accepted or Implemented DR needs a new DR that supersedes it,
not an edit to the old one.

## Numbering

`DR-0001`, `DR-0002`, … — one global sequence. Numbers are never reset per
phase and never reused.

## Statuses

`Proposed → Accepted → Implemented → Superseded by DR-nnnn`

## File naming

`DR-<nnnn>-<slug>.md`, one file per decision, in this folder.

## Contract docs stay current-only

A contract document (for example the runtime state contract or the PLC signal
contract) describes **current behaviour only**. When a DR changes behaviour,
the old behaviour moves out of the contract doc and into the "History" section
of the DR that changed it. A contract doc never carries "previously…" text.

## Index

| DR | Title | Status | Phase | Supersedes |
|---|---|---|---|---|
| [DR-0001](DR-0001-active-selection-status-signals.md) | Active selection is announced on separate optional status signals, never echoed onto the command registers | Implemented (Phase 9 Task C3) | 9 | — |
| [DR-0002](DR-0002-robot-pick-check-is-a-task-setting.md) | The robot pick check is a task setting, read family-independently | Implemented (Phase 9 Task F1) | 9 | — |
| [DR-0003](DR-0003-handshake-write-failure-aborts-the-cycle.md) | A failed handshake write is retried, then aborts the cycle with fault 301 | Implemented (Phase 9 Tasks E1–E4; E4 hardware observation pending) | 9 | — |
| [DR-0004](DR-0004-signal-map-gate-at-setup.md) | Signal-map gate at setup: unmapped required signals and orphan tags are hard errors | Implemented (Phase 9 Task C4) | 9 | — |
| [DR-0005](DR-0005-delete-connect-timeout-ms.md) | Delete `LocalizationRecoveryPolicy::connectTimeoutMs`; defer persisting policy values | Implemented (Phase 9 Task Z1) | 9 | — |
| [DR-0006](DR-0006-close-umbrella-item-26.md) | Close umbrella backlog item 26, folding its bullets into their real homes | Implemented (Phase 9 Task Z3) | 9 | — |
| [DR-0007](DR-0007-invalid-index-remains-a-task-fault.md) | An invalid camera/pattern index remains a task fault | Accepted (standing behaviour, no code change) | 9 | — |
| [DR-0008](DR-0008-connect-plc-first-read-live-selection.md) | `beginRuntime()` connects the PLC and output roles first; setup reads the live selection from the PLC | Implemented (Phase 9 Task C6) | 9 | — |

DR-0001 to DR-0008 were imported on 2026-09-16 from the Phase 9 implementation
plan (`docs/history/plan/phase_9_implementation_plan.md`, sections "Decisions
taken with the project owner (locked 2026-09-07)" and "Two decisions taken
after the draft, 2026-09-08"). That plan remains the primary record of the
discussion behind each of them.

## Template

Copied verbatim from `temp_docs/templates/policy_decision_record_template.md`
§B "Decision Record".

```markdown
# DR-<nnnn>: <short title>

**Status:** Proposed | Accepted | Implemented | Superseded by DR-<nnnn>
**Date accepted:** YYYY-MM-DD
**From:** PQ-<n> (<link>)
**Cluster:** <...>
**Supersedes / replaces text in:** <doc + section, e.g. plc_signal_contract.md → Trigger>

## Question
<one sentence>

## Decision
<one paragraph, imperative, testable>

## Owner's ruling (verbatim)
> "<original words, Vietnamese allowed inside the quote>"

## Rationale
<why this option; what it protects on the PLC / robot side>

## Rejected options and why
- Option 1 — <one line>
- Option 3 — <one line>

## Contract changes
| Table | Row / snapshot | Change |
|---|---|---|
| §3 transitions | T-… | added / guard changed |
| §4 outputs | … | … |
| §5 scenarios | S-… | added |
| §6 policy registry | … | parameter … |

## Verification
- Ring 1 (table tests): <test ids>
- Ring 2 (owner-run script): <script id>, expected log lines: `RT state=… event=… reason=…`
- Ring 3 (log replay): <app_log file + matched lines>

## Implementation
- Symbols touched: <...>
- Backlog items closed: <...>
- Git tag: <...>

## History (what this replaces)
<the old behaviour, moved here from the contract doc so the contract stays current-only>
```

### Rules

1. A DR is **Accepted** the moment the owner rules, even before code exists. The contract table
   gets its row (or its `UNSPECIFIED` cell filled) in the same commit as the DR.
2. A DR is **Implemented** only with all three verification rings recorded. An owner-run that
   has not happened is written as "pending", never omitted.
3. Superseding a DR never edits the old one beyond its Status line.
4. Import the existing Phase 9 D1–D8 as DR-0001…DR-0008 in step D0, with "Owner's ruling"
   copied from `phase_9_implementation_plan.md`, so the numbering starts from what is already law.
5. An **imported** DR (one whose ruling predates this log, such as DR-0001…DR-0008) keeps the
   status its source plan's evidence supports. Rings the source does not record read "Not recorded
   in the source" and are filled from later artefacts (for example the WP-02 test map), never
   invented. Rule 2 applies in full to every DR raised through a PQ card from Phase 10 on.
