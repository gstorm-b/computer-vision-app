# Templates: Policy Question card and Decision Record

Two artefacts, one per step of the contract-first loop:

- **Policy Question (PQ) card** — written by the agent *before* the owner decides. One page.
  Lives in the phase plan (or `docs/plan/<phase>/pq/`) while open.
- **Decision Record (DR)** — written by the agent *after* the owner decides. Global numbering,
  never reset per phase. Lives in `docs/decisions/DR-xxxx-<slug>.md`.

Discussion may be in Vietnamese. The committed DR is in English and quotes the owner's ruling
verbatim, as the Phase 9 plan already does for D1–D8.

---

## A. Policy Question card

```markdown
# PQ-<n>: <one-sentence question>

**Raised by:** owner note / field log / audit — <link>
**Blocks:** <task ids, or "nothing yet">
**Cluster:** <startup-arming | selection | cycle-fault | recovery | output | command-source>

## What happens today (facts only, with evidence)
- <symbol or test id or log line> — <observed behaviour>
- <...>

## Constraints already decided
- DR-xxxx: <one line>
- PLC/robot program assumption that must not break: <one line>

## Options
| # | Option | Effect on PLC program | Effect on robot / operator | Rows changed in runtime_state_contract.md | Cost |
|---|---|---|---|---|---|
| 1 | ... | ... | ... | T-…, S-… | S/M/L |
| 2 | ... | ... | ... | ... | ... |

## Agent recommendation
<Option n>, because <≤3 lines>. What would make this wrong: <one line>.

## Owner ruling
<left blank; filled by the agent from the owner's words, then promoted to DR-xxxx>
```

### Worked example (seed for the pilot)

```markdown
# PQ-1: Does a bExecuteTrigger that is already high when the runtime starts run a cycle?

**Raised by:** backlog item 54; Phase 9 Task G1/G2 (owner-gated); phase_9_request.md note
**Blocks:** Phase G; the Trigger section of plc_signal_contract.md ("Not yet specified")
**Cluster:** startup-arming

## What happens today
- `setup()` resets `m_lastExecuteTrigger = false`; `handlePlcValues()` treats the first
  `true` as a rising edge.
- On MC and on Modbus the first poll is suppressed (shadow adopted at connect), so a level
  that was already high is never delivered as a change: no edge, no cycle, task goes Ready
  and says nothing. (Plan Phase 9, "The held-trigger item".)
- Since C6, `setup()` reads a full snapshot via `PlcRunner::pollingUpdate`, so the runtime
  *can* learn the power-up level of the trigger, but does not use it.
- `m_lastErrorReset` is not reset by `setup()`; the two edge inputs are initialised by
  different rules.

## Constraints already decided
- D7: an invalid index remains a task fault. (Does not apply: a held trigger is a
  handshake state, not a selection.)
- Contract: a cycle starts only on a 0→1 edge while ReadyForTrigger.
- Rejected mechanism (item 54): seeding `m_lastExecuteTrigger` from the first observed value
  — it swallows the session's first genuine edge.

## Options
| # | Option | PLC program | Robot / operator | Rows | Cost |
|---|---|---|---|---|---|
| 1 | Keep today: go Ready, ignore the level | Must drop and re-raise the trigger after a restart; nothing tells it so | Silent; a held trigger looks like "vision not responding" | none | XS (doc only) |
| 2 | Not-ready until the trigger has been seen low once; no fault; reason on dashboard + log (Phase 9 G2) | Same requirement, but `bTaskReady` stays 0 so the PLC can see it | Dashboard shows "waiting for trigger release" | +2 rows (arm flag), S-04 | M |
| 3 | Treat the held level as a request: run one cycle at start | No change needed on restart | Robot may receive a result it did not ask for after a restart | +1 row | S, risky |

## Agent recommendation
Option 2. It makes the condition visible on the signal the PLC already watches (`bTaskReady`)
without inventing a fault, and it is the only option that cannot fire an unrequested cycle.
What would make this wrong: a cell whose master legitimately holds the trigger high across a
vision restart and expects service — then option 3 is what they want, and it must be a
per-task setting, not the default.

## Owner ruling
(pending)
```

---

## B. Decision Record

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
