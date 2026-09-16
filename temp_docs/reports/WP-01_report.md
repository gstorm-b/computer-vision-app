# WP-01 report — Active-plan folder, global decision log, import Phase 9 decisions D1–D8

**Executed:** 2026-09-16 · **Handoff:** `temp_docs/wp/WP-01_docs_structure_and_decision_log.md` · **Stop rules:** none fired (the plan's D-list has exactly eight decisions; every landing task was identifiable; the D3/D6 "item 8" numbering correction is recorded in those DRs' History sections, not resolved).

## 1. Files created

| Path | What |
|---|---|
| `docs/plan/README.md` | What lives in the active-plan folder, lifecycle (folder moves to `docs/history/plan/` on close), layout `phase_<n>/{charter.md, request.md, wp/, reports/}`, language, link to `../decisions/README.md` |
| `docs/plan/phase_10/README.md` | Stub (6 lines): charter published at Checkpoint 0; working draft `temp_docs/02_phase10_charter_and_work_breakdown.md` |
| `docs/decisions/README.md` | Purpose, global numbering, statuses, file naming, the "contract docs stay current-only" rule, the index (below), and the DR template + four rules copied verbatim from `temp_docs/templates/policy_decision_record_template.md` §B |
| `docs/decisions/DR-0001-active-selection-status-signals.md` | D1 |
| `docs/decisions/DR-0002-robot-pick-check-is-a-task-setting.md` | D2 |
| `docs/decisions/DR-0003-handshake-write-failure-aborts-the-cycle.md` | D3 |
| `docs/decisions/DR-0004-signal-map-gate-at-setup.md` | D4 |
| `docs/decisions/DR-0005-delete-connect-timeout-ms.md` | D5 |
| `docs/decisions/DR-0006-close-umbrella-item-26.md` | D6 |
| `docs/decisions/DR-0007-invalid-index-remains-a-task-fault.md` | D7 |
| `docs/decisions/DR-0008-connect-plc-first-read-live-selection.md` | D8 |
| `temp_docs/reports/WP-01_report.md` | this report |

## 2. Files changed (insertions only)

| Path | Insertion |
|---|---|
| `docs/README.md` | Two rows at the top of "Active Planning And Backlog": `Active phase plan → plan/README.md` and ``Owner decision log (`DR-xxxx`) → decisions/README.md``. Nothing else touched. |
| `AGENT.md` | (a) "Read First": new item **3b** after 3a, wording as the handoff gives it. (b) "Source-Of-Truth Hierarchy" item 3: new bullet ``owner design rulings: `docs/decisions/`.`` after the `uml/` bullet. (c) "Highest Priority Next Work": new first bullet "Phase 10 (runtime core redesign): …", wording as the handoff gives it. Nothing else touched. |

Not touched: `docs/history/**`, `docs/backlog/**`, `docs/domains/**`, any source or test. No git commands were run; no build.

## 3. DR index as landed

| DR | Title | Status | Phase | Supersedes |
|---|---|---|---|---|
| DR-0001 | Active selection is announced on separate optional status signals, never echoed onto the command registers | Implemented (Phase 9 Task C3) | 9 | — |
| DR-0002 | The robot pick check is a task setting, read family-independently | Implemented (Phase 9 Task F1) | 9 | — |
| DR-0003 | A failed handshake write is retried, then aborts the cycle with fault 301 | Implemented (Phase 9 Tasks E1–E4; E4 hardware observation pending) | 9 | — |
| DR-0004 | Signal-map gate at setup: unmapped required signals and orphan tags are hard errors | Implemented (Phase 9 Task C4) | 9 | — |
| DR-0005 | Delete `LocalizationRecoveryPolicy::connectTimeoutMs`; defer persisting policy values | Implemented (Phase 9 Task Z1) | 9 | — |
| DR-0006 | Close umbrella backlog item 26, folding its bullets into their real homes | Implemented (Phase 9 Task Z3) | 9 | — |
| DR-0007 | An invalid camera/pattern index remains a task fault | Accepted (standing behaviour, no code change) | 9 | — |
| DR-0008 | `beginRuntime()` connects the PLC and output roles first; setup reads the live selection from the PLC | Implemented (Phase 9 Task C6) | 9 | — |

Dates accepted: DR-0001..0006 = 2026-09-07 (the plan's "locked" date); DR-0007, DR-0008 = 2026-09-08.

## 4. Verification performed

Script `verify_wp01.ps1` (scratchpad, not committed) checked, against the plan as it stood at run time:

- D1–D6: the DR "Decision" and "Rationale" sections equal the plan's *Decision (verbatim)* and *Rationale* cells exactly after whitespace normalisation — **6/6 pass on both**.
- D7/D8: every paragraph in "Decision" and "Rationale" is a verbatim substring of the plan — **all pass**; both Vietnamese owner quotes found verbatim in DR and plan (checked separately without non-ASCII script literals; the first run's two "failures" were the script file's own encoding, not the DRs).
- Template block in `docs/decisions/README.md` equals template §B line-for-line (fenced block, blank line, "### Rules", the four rules) — **pass**.
- Every relative link in the four new READMEs, the eight DRs and `docs/README.md` resolves — **pass**.
- Every DR has all nine template headings and five metadata lines, none blank — **pass**.
- `docs/history/plan/` listing unchanged by me. Note: `phase_9_implementation_plan.md` has a 2026-09-16 15:08 mtime — WP-00 editing it in parallel, as expected; the comparisons above were made against its content at check time and passed.

## 5. Wording I had to choose (not present in the source)

Everything below is mine; Decision, Rationale, owner quotes and the template are copied.

1. **DR titles and slugs** (index above). The plan gives the decisions no titles.
2. **`**From:**` line.** No PQ card ever existed, so each reads "no PQ card — imported from `docs/history/plan/phase_9_implementation_plan.md` → <section>, <row>".
3. **`**Cluster:**`** — assigned from the template's list: DR-0001 `selection`, DR-0002 `output`, DR-0003 `output`, DR-0004 `startup-arming`, DR-0005 `recovery`, DR-0007 `selection`, DR-0008 `command-source`; DR-0006 "none of the template's runtime clusters — backlog hygiene". Reassign freely; nothing else depends on them.
4. **`## Question`** — one sentence per DR, written by me from the decision.
5. **`## Contract changes`** — the template's table replaced by the sentence "Not recorded in the source." (the runtime state contract tables did not exist when the plan was written). DR-0003 additionally names `plc_signal_contract.md` → "Handshake Writes Are Acknowledged" because backlog item 69 names it. DR-0006 reads "Not applicable — the ruling changes backlog records, not runtime behaviour."
6. **D7/D8 section mapping.** The plan records D7/D8 as a bold headline, a quote and paragraphs, not as table columns. DR-0007: Decision = headline + the full second paragraph (including the two sentences about deleting Task C1, kept so nothing is sliced mid-paragraph); Rationale = the third paragraph (`reportSignalTypeMismatch()` consistency). DR-0008: Decision = headline; Rationale = both following paragraphs. Rejected options quote the plan's own sentences.
7. **DR-0001..0006 "Owner's ruling"** = the handoff's sentence "Recorded in English in the Phase 9 plan; the decision text above is that record." (unquoted, inside the blockquote).
8. **Verification rings.** Ring 1 lists only test names the plan or `later_todo_list.md` names for that landing task, each with its source named. Where the source describes a case but does not name it (DR-0002: three F1 cases the plan cites by suffix only; DR-0003: three E4 cases; DR-0004: the duplicate-tag and `m_lastErrorReset` cases) the DR says "to be filled from the WP-02 test map". Ring 2 records owner-run facts from the plan/backlog in my words, with "**Pending:**" where the source says the run has not happened (DR-0003 E4 on hardware; DR-0004 dialog in both themes and Japanese). Ring 3 cites the app-log file by date and the quoted message, never by line number; "Not recorded in the source." where nothing is. DR-0005 and DR-0006 say "not applicable" for rings that cannot apply to a deletion / a docs-only change.
9. **"Implementation → Symbols touched"** — symbol lists compiled from each task's landing record in the plan (and backlog items 1, 57, 58, 69), source named in each. **Git tag** = "Not recorded in the source." everywhere.
10. **History** — the handoff's import sentence, plus one sentence noting that the copied text carries the plan's own source line references "as of 2026-09-08 and not maintained here" (rule 4 forbids *my* line citations; rule 5 forbids altering the copied text, which contains the plan's). DR-0005 also notes the Z1 record's own remark that `:2195`/`:2257` were already stale when Z1 ran.
11. **Numbering correction (D3, D6)** — quoted from the plan verbatim in History, marked "recorded here as the plan states it and not resolved in this record"; the Decision text keeps "item 8" as written; both note that Z3 filed the write-failure entry as item 69.
12. **DR-0007 Status line** — says explicitly that no code change was made or required, hence no landing task and no advance to Implemented (handoff step 4).
13. **`docs/plan/README.md`** — the "Phases 5 to 9 predate this folder; their plans are single files already under `docs/history/plan/` and stay there" sentence, and the rule that a plan cites a `DR-nnnn` rather than restating it.
14. **`AGENT.md` punctuation** — the existing `uml/` bullet ends with a full stop; the new bullet after it has its own full stop and the existing line was left untouched, per "change nothing else".

## 6. Open questions for the PM

1. **Template rule 2 vs. the handoff's Status.** Rule 2 says a DR is Implemented "only with all three verification rings recorded". DR-0001..0006 and DR-0008 are marked Implemented per handoff step 4, but several rings read "Not recorded in the source" or "to be filled from the WP-02 test map". Either accept that imported DRs are exempt (say so in the decisions README) or downgrade those to Accepted until WP-02's test map fills ring 1.
2. **Cluster names** (§5 item 3) were chosen by me; confirm or change before Phase 10 PQ cards start citing them.
3. **`docs/README.md` "Traceability Only" table does not list `history/plan/`** at all, so the five archived phase plans are not in the doc map. Not in my step list ("change nothing else"), so left as is — worth one row when someone next edits that table.
4. **Backlog item 69 names no test names** and item 57 names only one; the plan's F1 landing record names three further cases by suffix only. WP-02's test map is the place to close DR-0002/0003/0004's ring 1.

## 7. Noticed in files I may not edit (for WP-00 / WP-02 / WP-03)

- `docs/history/plan/phase_9_implementation_plan.md` still opens with "**Status:** DRAFT — not started." and the Location note saying `docs/plan/` is "five phases old" — now false, since `docs/plan/` exists. WP-00's closeout should update both.
- The Phase 9 plan's `**Owner decisions:** D1–D6, locked 2026-09-07` header line does not mention D7/D8; the DR index now does, so a pointer from that line to `docs/decisions/` would help.
- `docs/backlog/later_todo_list.md` item 58 part B and item 69's E4 observation are the two open items DR-0008 and DR-0003 point at; nothing to change, just confirming the DRs cite them as open.

---

## PM review — 2026-09-16 — ACCEPTED with four PM edits

Checked: the three insertions in `AGENT.md` and the two rows in `docs/README.md`; the DR index;
DR-0001, DR-0007 and DR-0008 read in full against the plan (Decision/Rationale unchanged, quotes
verbatim, rings honest about what the source does not record). Touch-list respected.

Rulings on the open questions:

1. **Imported DRs keep Implemented.** A new rule 5 in `docs/decisions/README.md` (and in the
   template) says an imported DR keeps the status its source plan's evidence supports; unrecorded
   rings are filled from later artefacts, never invented. Rule 2 applies in full from Phase 10 on.
2. **Clusters** — two changed by the PM: DR-0008 `command-source` → `startup-arming` (it is about
   where the startup selection comes from, not about a second command source); DR-0003 `output` →
   `cycle-fault` (a handshake write failure is a cycle fault, and the pick-check DR-0002 already
   holds `output`). The rest stand.
3. **`history/plan/` row** added by the PM to the "Traceability Only" table in `docs/README.md`.
4. **Ring 1 for DR-0002/0003/0004** is filled at Checkpoint 0 housekeeping from the WP-02 test map.

Also done by the PM: the Phase 9 plan's "Owner decisions" header line now mentions D7–D8 and points
at `docs/decisions/`. The other two "noticed" items were already handled by WP-00 (the plan header
and the location note carry the closed status and the `docs/plan/` correction).
