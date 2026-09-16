# Active Plan

This folder holds the plan for the phase that is currently being worked. It is
the one place an agent looks for "what is the project doing right now"; it is
not history, and it is not the backlog.

## What lives here

For the active phase:

- the phase **charter** — scope, checkpoints, stop rules, and what the phase
  explicitly leaves out;
- its **work packages** (the handoff a single agent executes) and the
  **reports** each work package returns;
- the owner's **request notes** for that phase — the raw ask the charter was
  written from.

Owner rulings that constrain design do **not** live here. They are recorded
once, with global numbering, in the decision log:
[../decisions/README.md](../decisions/README.md). A phase plan cites a
`DR-nnnn`; it never restates or re-litigates one.

## Layout

```text
docs/plan/
  README.md                 this file
  phase_<n>/
    charter.md              the phase charter, published at the phase's Checkpoint 0
    request.md              the owner's request notes for the phase
    wp/                     work-package handoffs, one file per package
    reports/                the report each work package returns
```

Until a charter is published, `phase_<n>/README.md` says where the working
draft is.

## Lifecycle

- A phase folder is created here when the phase opens.
- When the phase closes, its folder moves to `docs/history/plan/` unchanged.
  From then on it is traceability only, as `AGENT.md` says of everything under
  `docs/history/`.
- Phases 5 to 9 predate this folder; their plans are single files already under
  `docs/history/plan/` and stay there.

## Language

English, like every committed document. The owner's own words, when quoted, are
copied verbatim in whatever language they were written.
