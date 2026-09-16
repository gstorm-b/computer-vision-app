# Session handoff — continue Phase 10 on the machine with git

**Written:** 2026-09-16, end of the Stage 0 session on the no-git machine.
**For:** the next Claude Code session (same PM + system-design role) and the owner.
**Self-contained on purpose:** the PM's per-machine memory does not travel. Everything needed to
resume is in this file and in the repository.

> **Owner (tiếng Việt):** file này là điểm vào cho phiên kế tiếp trên máy có git. Việc đầu tiên bên
> đó là commit + tag `phase9-closeout` (mục 3), rồi trả lời ba câu hỏi ở mục 5, rồi PM phát hành
> WP-02b (sửa bảng theo review) và publish `docs/plan/phase_10/`. Không có WP nào đang chạy dở.

---

## 1. Read in this order on the new machine

1. `AGENT.md` (root) — operating rules; note the new "Read First 3b" (decisions) and the Phase 10
   bullet under "Highest Priority Next Work".
2. `docs/plan/README.md`, `docs/decisions/README.md` (index DR-0001..0008, rule 5 for imported DRs).
3. **This file.**
4. `temp_docs/02_phase10_charter_and_work_breakdown.md` (Vietnamese working charter; English:
   `temp_docs/en/charter.md`) — the plan, §3 Stage 0 progress, §6 owner decisions.
5. `temp_docs/03_checkpoint_0_package.md` — what the owner must confirm; §4 has the review outcome
   and the confirmed-defect table R1..R5.
6. `docs/domains/task_localization/runtime_state_contract.md` (AS-IS, 887 lines) with
   `temp_docs/reports/WP-02_report.md` and `temp_docs/reports/WP-02_review.md`.
7. `temp_docs/01_open_policy_questions.md` (PQ-1..16; English: `temp_docs/en/open_policy_questions.md`).

Invoke the `using-agent-skills` skill first in any new session (owner expectation), then
`planning-and-task-breakdown` when issuing WPs.

## 2. Environment facts that used to live in the PM's memory (reproduce, do not rediscover)

- **Git:** the previous machine could not run git; this one can. Everything below was done without
  branches or diffs, so the first commit is large and deliberate (mục 3).
- **Agents:** `.claude/agents/wp-implementer.md` and `.claude/agents/wp-reviewer.md` are in the
  repository (frontmatter `model: opus`, `effort: xhigh`). The owner wants **every delegated agent
  on Opus with extra-high effort**. A session started before the files existed does not list them;
  fall back to `general-purpose` + `model: "opus"` and paste the rules from the definition into the
  prompt. Effort cannot be passed per call, only via the definition.
- **Rate limits:** two agents were cut by a session limit (resets 17:00 Asia/Tokyo). Resuming an
  agent by `SendMessage` to its id keeps its context and worked; prefer that over relaunching.
- **Build/test (from `docs/rules/build_and_verification.md`, verified on the old machine):** Qt
  `C:\Qt\6.8.3\msvc2022_64`, VS 2022 Community `vcvars64.bat`, OpenCV `C:\opencv\build`
  (`opencv_world4110`), Pylon **v10** (`PylonBase_v10`), `qmake/local_paths.pri` untracked. Always
  `qmake` before `nmake`; an interrupted `nmake` can leave a 0-byte `.obj` (delete it); a stale root
  `main.moc` in a test build dir silently drops tests (delete it, re-run qmake, delete `main.obj`);
  run Qt tests with `QT_QPA_PLATFORM=minimal` and `-o results.txt,txt`; confirm a suite's shape with
  `<exe> -functions` before trusting a total; **close both shells before the contract suite** (the
  single-instance guard and the GigE camera are exclusive). Paths on the new machine may differ —
  check `qmake/local_paths.pri` there.
- **Baseline counts (Checkpoint Z, 2026-09-14):** `architecture_contract_test` 159 passed
  (157 `test_` functions + init/cleanup), `mc_frame_test` 54, `modbus_device_test` 25,
  `vision_output_device_test` 11, `vision_tcpip_client_device_test` 10, all 0 failed.
- **Owner working style:** writes Vietnamese; tentative notes are not specs (frame first, PQ card,
  then ruling); wants assumptions surfaced and forks asked before code; approves scope explicitly
  (charter §6 answered inline in the Vietnamese charter).
- **Committed text rules:** English; symbols and test names, never line numbers; owner quotes
  verbatim; no backwards-compatibility shims; no new hard-coded machine paths.

## 3. First action on the new machine: commit and tag `phase9-closeout`

Stage 0 was docs-only; no source, test, `.pro`, `.pri` or `uml/` file changed. Commit these:

| Path | Change |
|---|---|
| `AGENT.md` | Read First 3b; hierarchy bullet `owner design rulings: docs/decisions/`; Phase 10 bullet |
| `docs/README.md` | rows for `plan/README.md`, `decisions/README.md`, `history/plan/` |
| `docs/plan/README.md`, `docs/plan/phase_10/README.md` | new |
| `docs/decisions/README.md`, `docs/decisions/DR-0001-…` … `DR-0008-…` | new (rule 5 added; DR-0003 cluster `cycle-fault`, DR-0008 cluster `startup-arming`) |
| `docs/history/plan/phase_9_implementation_plan.md` | header closed-with-carry; location note sentence; "Owner decisions" header points at `docs/decisions/`; Checkpoint Z status line + 14-row carry table |
| `docs/backlog/technical_debt_and_next_steps.md` | "Carried Out Of Phase 9" section (same 14 rows) |
| `docs/backlog/later_todo_list.md` | triage table (65 rows) at top + post-triage note; closing notes on items 60 and 23; new items 71 and 72 |
| `docs/domains/task_localization/runtime_state_contract.md` | new, AS-IS (review changes pending — WP-02b) |
| `.claude/agents/wp-implementer.md`, `.claude/agents/wp-reviewer.md` | new |
| `temp_docs/**` | charter (VI + EN), PQ list (VI + EN), templates, handoffs WP-00..04 + WP-10, reports WP-00..04 + WP-02 review, checkpoint package, this file |

Suggested: one commit "Phase 9 closeout + Phase 10 Stage 0 (docs only)", tag `phase9-closeout`.
Whether `temp_docs/` is tracked is the owner's call; it is the working record of the phase and the
plan is to move its committable parts into `docs/plan/phase_10/` (mục 6, step 3).

## 4. State of every work package

| WP | Status | Where |
|---|---|---|
| WP-00 Phase 9 closeout | ACCEPTED (+1 carried row by PM) | `temp_docs/reports/WP-00_report.md` |
| WP-01 docs/plan + decisions + DR import | ACCEPTED (+4 PM edits) | `…/WP-01_report.md` |
| WP-02 AS-IS contract | ACCEPTED WITH CHANGES → **WP-02b pending** | `…/WP-02_report.md`, `…/WP-02_review.md` |
| WP-03 backlog triage | ACCEPTED; items 60/23 closed, 71/72 filed by PM | `…/WP-03_report.md` |
| WP-04 English charter + PQ list | ACCEPTED (+4 PM edits, mirrored VI/EN) | `…/WP-04_report.md`, `temp_docs/en/` |
| WP-10 transition funnel | **written, not issued** | `temp_docs/wp/WP-10_transition_funnel.md` |
| WP-02b | **to be written** (content fixed — mục 6) | — |

No agent is running. Nothing is half-edited.

## 5. Waiting on the owner (Checkpoint 0, package §7)

1. Publish to `docs/plan/phase_10/` (charter EN, PQ list EN, `wp/`, `reports/`) — yes/no.
2. Run Stage 1 (WP-10) in parallel with Stage 2 (PQ cards) — PM recommends yes.
3. Hotfix the confirmed R1–R3 defects on the current code before Stage 3, or leave them to the
   redesign — PM recommends hotfix R1–R3 (each a small WP with a table row test and a short DR,
   because each changes PLC-observable behaviour), R4–R5 and the rest to the redesign.

Also implicit: confirmation that Phase 9 is closed on the 14-row carry table, and acceptance of the
AS-IS table as baseline once WP-02b lands.

## 6. Work queue on the new machine, in order

1. **Commit + tag** (mục 3).
2. **Collect the three answers** (mục 5).
3. **WP-02b — apply the review to the AS-IS table** (docs only, S/M, `wp-implementer`). Content is
   fixed by the PM rulings in `WP-02_report.md` → "PM review": re-derive §7 with the connect-timing
   rule stated once (fixture connects only the active camera; `requestConnect()` is queued, so a
   setter right after `setup()` runs before the new role is healthy); move T-RFT-SelectCamera-4 and
   T-RFT-RoleHealthy-1 out of §7.2; re-mark E23 cells A with row T-Any-LevelSample-1 (grid A 87 ·
   I 24 · N 12 · U 15 = 138); rewrite the E22 row and add S-18 (teardown window: runner→controller
   connections stay live until the queued `deleteLater()`; `endRuntime()`/`stopAll()` create a new
   controller immediately); re-mark E09/`Running` and E09/`WaitingTriggerReset` as I; add a row for
   `startCycle()`'s own guard/log; align §3.1 with §7.1 on the 301 test; add a scenario for "an
   expected completion never arrives" (only the grab has a watchdog); prepend the R1..R5 ranking to
   the report's findings; correct the line count. Then a short `wp-reviewer` pass on §7 only.
4. **Publish** (if answer 1 is yes): move `temp_docs/en/charter.md` → `docs/plan/phase_10/charter.md`,
   `temp_docs/en/open_policy_questions.md` → `docs/plan/phase_10/open_policy_questions.md`,
   `temp_docs/wp/*` → `docs/plan/phase_10/wp/`, `temp_docs/reports/*` → `docs/plan/phase_10/reports/`;
   rewrite `docs/plan/phase_10/README.md`; add the charter link to the Checkpoint Z status line and
   to "Carried Out Of Phase 9". Keep the Vietnamese originals in `temp_docs/`. Tag `phase10-cp0`.
5. **Hotfix WPs** (if answer 3 is (a)): one WP each for R1, R2, R3, in that order, each with: the
   table row(s) it changes, a short DR (`DR-0009`…), a test derived from the row, negative check,
   contract suite count before/after. R3 is the most consequential on a running cell (trigger runs a
   cycle against a disconnected PLC); R1 is the most visible (a 2-second loop); R2 is a hang.
6. **WP-10** (Stage 1) — issue as written; behaviour-neutral; 159 → 161.
7. **Stage 2** — PM writes PQ cards for cluster A (PQ-1..4) starting from S-04/S-07: measure on the
   virtual PLC and read `McProtocolDevice`'s first-poll suppression and `ModbusRegisterMap`'s
   baseline adoption before the card; then PQ-6 (split `Recovering`), the cheap PQ-8/9/11, then
   PQ-12/13 (command source) before any TCP work. Then WP-21 to-be table, WP-22 reviewer, WP-23 core
   API. Do not write core code before Checkpoint 2.

## 7. Risks carried into the next session

- The AS-IS table is not yet baseline-grade until WP-02b lands (three test attributions flatter
  coverage; E22/E23 understated).
- R3 is live on the cell today. If the owner runs the cell before a hotfix, a PLC or output that
  reports `Disconnected` (not `LostConnected`) at Ready leaves `bTaskReady=1`.
- `docs/plan/phase_10/README.md` still points at the Vietnamese draft until publication.
- Backlog item 61's residual (item 72) and item 51's device half are absorbed by the redesign only
  on paper until Stage 4.
- Rate limits: Stage 3/4 WPs are large; issue one heavy agent at a time and use `SendMessage` to
  resume rather than relaunch.

## 8. Protocol reminders (charter §5)

Handoff ≤ 1 page with touch-list and stop rules; report with changed-file list, evidence, negative
checks, deviations, findings out of scope, open questions; PM reviews every listed file; owner sees
checkpoints only. With git available now: one commit per WP, tag per checkpoint, negative checks on a
branch or stash instead of edit-and-restore.
