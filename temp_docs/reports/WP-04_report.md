# WP-04 report — English charter and policy-question list

**WP:** `temp_docs/wp/WP-04_english_charter_and_pq_list.md` · **Date:** 2026-09-16 · **Kind:** docs
only, no build step.

## 1. Changed / created files

Created (both new, nothing else on disk was opened for writing):

- `temp_docs/en/charter.md`
- `temp_docs/en/open_policy_questions.md`
- `temp_docs/reports/WP-04_report.md` (this report)

Not touched: the Vietnamese originals, anything under `docs/` (including
`docs/domains/task_localization/runtime_state_contract.md`, which WP-02 owns), any source or test
file. No git command was run.

## 2. Evidence

Read in the handoff's order: `AGENT.md`; the handoff; `temp_docs/02_phase10_charter_and_work_breakdown.md`;
`temp_docs/01_open_policy_questions.md`; `docs/plan/README.md`; `docs/decisions/README.md`;
`temp_docs/00_assessment_and_runtime_design_process.md`. Skills invoked before work started:
`using-agent-skills`, then `documentation-and-adrs` per its routing.

**Structure counts, original vs English (mechanical, whole-file):**

| Metric | Charter VI → EN | PQ list VI → EN |
|---|---|---|
| Markdown table rows (lines starting `\|`) | 63 → 63 | 26 → 26 |
| Headings (lines starting `#`) | 17 → 17 | 7 → 7 |

Section order is unchanged in both files. Charter: header block, §1 with 1.1–1.4, §2, §3 with Stages
0–5 plus the Stage 0 progress note, the four housekeeping bullets and Checkpoints 0–4, §4, §5, §6.
PQ list: header, Clusters A–E, "Suggested order", the closing note on non-core items.

Per-table row counts carried over one for one: §1.1 carried table 6; §1.2 do/do-not 5; §1.3 options
3 (plus the six numbered defect classes); Stage 0 4; Stage 1 3; Stage 2 4; Stage 3 5; Stage 4 4;
Stage 5 2; §4 risks 7; §5 handoff 9 items and report 6 items; §6 five decisions. PQ clusters: A 4,
B 3, C 4, D 2, E 3 — PQ-1..PQ-16 complete, no renumbering.

**Identifiers.** A set-difference of every `WP-nn` / `PQ-n` / `DR-nnnn` / `S-nn` / `Dn` token between
each original and its English version returns empty in the Vietnamese→English direction for both
files. The only English-only tokens are the two intended DR citations named in the handoff's Facts
(`DR-0008` in §1.2 of the charter, `DR-0005` in PQ-14) — see Deviations.

**Symbols, test names, paths.** A set-difference of every inline code span outside fenced blocks
returns **empty in both directions for both files**: every `transitionTo()`, `canTransitionCycleState()`,
`onVisionOutputResultFinished()`, `markRuntimeReady()`, `buildVisionOutputPositions()`,
`setRecoveryPolicies()`, `beginRuntime()`, `deviceConnect()`, `abortCycle()`,
`handleRoleStatusChanged()`, `handlePlcValues()`, `publishXxxOutputs()`,
`TaskLocalizeConfig::robotCheckConfig()`, `queueSetActiveCameraNumber()`,
`queueSetActivePatternGroupNumber()`, `onCommDeviceValueChanged()`, `execute()`/`executeLocalization()`,
`m_lastErrorReset`, `bTaskFault`, `bTaskReady`, `bMatchingLowArea`, `bErrorReset`, `RECOVERING`,
`CYCLE_FAULT`, `LostConnected`, `ReadyForTrigger`, `CycleState`, `RuntimeCore`, `UNSPECIFIED`,
`RT state=…`, `tests/architecture_contract_test/main.cpp`, `tests/runtime_core_test`,
`task_localization.*`, `docs/plan/**`, `docs/decisions/**`, `docs/README.md`, `AGENT.md`,
`phase9-closeout`, `phase10-core`, and the fault codes `201` / `301` / `400` appear byte-identical.
The two fenced blocks (the `RuntimeCore` sketch in §2, the folder layout in §5) are reproduced
character for character except for their two Vietnamese trailing comments, which are translated.

**§6 reads as decisions taken.** Heading: "Decisions taken by the owner (2026-09-16)". Each of the
five points keeps its original question, then the owner's answer as a verbatim block quote
(`Đồng ý` — with no full stop, as in the original —, `Tổ chức lại theo 1.2 đã đề xuất.`,
`Chấp nhận.`, `Theo đề xuất của bạn.`, `Đồng ý.`), then a one-line English gloss. A scan for
Vietnamese diacritics in `temp_docs/en/charter.md` returns exactly those five quoted lines and
nothing else.

**No line-number citations.** A scan for `line|lines|dòng` + digits and for `:<digits>` returns
nothing in either English file. The originals carried none either; "440 lines of prose" in §1.1 is a
document length, not a citation, and is kept.

**Relative links, resolved against the future home `docs/plan/phase_10/`:** every target exists
today.

| Link | Resolves to | Exists |
|---|---|---|
| `wp/WP-00_phase9_closeout.md`, `wp/WP-10_transition_funnel.md` | `docs/plan/phase_10/wp/…` | moves with the folder at Checkpoint 0 |
| `../../domains/task_localization/plc_signal_contract.md` | `docs/domains/task_localization/plc_signal_contract.md` | yes |
| `../../domains/task_localization/runtime_state_contract.md` | same folder | yes (WP-02 has created it) |
| `../../backlog/technical_debt_and_next_steps.md` | `docs/backlog/…` | yes |
| `../../backlog/later_todo_list.md` | `docs/backlog/…` | yes |
| `../../history/request/phase_9_request.md`, `phase_10_request.md`, `cratch_notes.md` | `docs/history/request/…` | yes |

## 3. Negative checks

Not applicable. This work package creates two documents and runs no code; there is no behaviour to
invert, no test to drive red, and no source file was touched, so there is nothing to inject or
restore.

## 4. Deviations

1. **Two factual corrections applied, both instructed by the handoff's Facts.**
   "65 item (đánh số tới 70)" becomes "the backlog's 65 items, numbered up to 72" in both places it
   occurs (§1.2 do/do-not table and the WP-03 row). Verified in
   `docs/backlog/later_todo_list.md`: items 71 ("Phase 9 / A1's M-only MC trigger delivery is
   unverified on hardware") and 72 ("A refused index selection updates no camera visual on the
   dashboard (residual of item 61)") exist, and the WP-03 triage totals in the charter — 21 + 35 + 8
   + 1 — sum to 65.
2. **Stage 0 progress line advanced for WP-01 only.** The Vietnamese line says "WP-01, WP-02 đang
   chạy" (both running), but the handoff's Facts say WP-00, WP-01 and WP-03 are accepted and WP-02
   is running, and the charter's own ticked housekeeping bullet ("review WP-01: rule 5 …") already
   records the WP-01 review. The English reads "WP-00 ✅ accepted …; WP-01 ✅ accepted; WP-03 ✅
   accepted (…); WP-02 running". `temp_docs/reports/WP-01_report.md` exists, consistent with that.
   Nothing else in the progress or housekeeping state was advanced: the "Publish the English
   charter…" and "Move `wp/`, `reports/` and the PQ list…" bullets stay unticked, since they are the
   PM's Checkpoint 0 actions.
3. **"D1–D8" cited as DR numbers where it is a reference, kept where it is a mapping.** Per the
   handoff's Facts, §1.2 reads "import the Phase 9 D1–D8 rulings as DR-0001..DR-0008" and PQ-14's
   Source column reads "item 67; DR-0005 (defer)" instead of "D5 (defer)" (DR-0005 is
   `DR-0005-delete-connect-timeout-ms.md`, "defer persisting policy values" — the same ruling). The
   WP-01 row keeps its original wording "import D1–D8 as DR-0001..0008" because there the mapping
   itself is the work item.
4. **Document links added where the original had a bare filename.** The originals contain only two
   markdown links (the two sample handoffs). To satisfy the handoff's step 3 — relative links
   rewritten for `docs/plan/phase_10/` — the first mention of each externally-owned document is now
   a link whose visible text is the unchanged original string (so a grep still finds it); later
   mentions of the same document stay plain code spans. Repository-rooted paths (`docs/plan/**`,
   `tests/…`, `src`-side symbols) were left as code spans, since they are already unambiguous.
5. **Two Vietnamese comments inside the §2 code block were translated**: `→ bộ giá trị PLC tag` →
   `→ the set of PLC tag values`, and `(một hàm, một bảng, mọi transition qua đây)` →
   `(one function, one table, every transition goes through it)`. They are prose, not identifiers;
   every identifier in that block is untouched.
6. **Title replaced as the handoff dictates.** The original title also carried the scope phrase
   "đóng Phase 9, tổ chức lại, thiết kế lại runtime core"; that phrase survives as the §1 heading
   ("the 'close 9 → reorganise → redesign' plan"), so nothing is lost.

### Translation choices worth recording

- **`D1` in the §1.1 carried table is a Phase 9 *task* id, not decision D1, and was left as `D1`.**
  `docs/history/plan/phase_9_implementation_plan.md` has both "Task D1: A wedged connect must be
  visible" (the vision-output reconnect work carried to item 70) and "Task C3: … (D1 / item 58)"
  (where D1 is the decision that became DR-0001). Converting the carried row to "DR-0001" would have
  pointed at the wrong artefact.
- **Owner/source quotes stay Vietnamese.** The four `cratch_notes.md` quotes in the PQ list (PQ-2,
  PQ-4, PQ-5, PQ-7) are the owner's own words and are reproduced verbatim without a gloss, following
  `docs/plan/README.md` ("The owner's own words, when quoted, are copied verbatim in whatever
  language they were written"). Only charter §6 carries glosses, because the handoff asks for them
  there.
- **WP-11's clause was kept literal.** "Xoá entry point chết đã được owner duyệt ở PQ-13 … — hoặc bỏ
  nếu owner giữ" is rendered "Delete the dead entry points the owner approved at PQ-13 … — or drop
  this WP if the owner keeps them", preserving the original's own hedge rather than resolving it
  (see Findings 2).
- **Vocabulary fixed across both files** so the reviewer can grep: *cụm* → cluster, *ô* → cell,
  *hàng/row* → row, *nguồn lệnh* → command source, *tổ chức lại* → reorganise, *thiết kế lại* →
  redesign, *lõi* → core, *vòng* → ring, *chốt/latch* → latch, *cell* (the factory cell) left as
  "cell". British spelling ("reorganise", "parallelise") matches the surrounding `docs/` prose.
- **`Hôm nay` / `Vì sao phải quyết`** became the column headers **Today** / **Why a ruling is
  needed**; the header paragraph that names those two columns uses the same two strings.

## 5. Findings out of scope (not fixed)

1. **WP-04 is not in the charter's Stage 0 table.** The table defines WP-00..WP-03 only, and
   Checkpoint 0 is written as "once all 4 WPs are done". This package's deliverable corresponds to
   the PM's housekeeping bullets "Publish the English charter…" and "Move `wp/`, `reports/` and the
   PQ list…". It is a gap in the charter's own bookkeeping, not a contradiction, so the charter was
   rendered as written; the PM may want a WP-04 row before publication.
2. **The charter says the owner approved deleting the dead entry points "at PQ-13"; the PQ list
   still carries PQ-13 as open**, and the charter's own WP-11 row ends "or drop this WP if the owner
   keeps them". §6 point 2 ("Tổ chức lại theo 1.2 đã đề xuất.") approves the *scope* that contains
   WP-11, which is probably what the sentence means, but no DR records a PQ-13 ruling
   (`docs/decisions/README.md` indexes DR-0001..DR-0008 only). Both documents were rendered as
   written.
3. **`templates/policy_decision_record_template.md`, cited in the PQ list header, will not resolve
   from `docs/plan/phase_10/`.** It lives at `temp_docs/templates/`, and the same template is now
   reproduced in `docs/decisions/README.md` § Template. It was left as a plain code span (not a
   link) so nothing breaks; the PM may want to repoint it at publication.
4. **`cratch_notes.md` is spelled that way in the repository** (`docs/history/request/cratch_notes.md`),
   apparently a typo for "scratch". The originals cite it with that spelling and so does the English
   version; renaming a history file is out of scope.
5. **Test-count drift between the working documents:** `temp_docs/00_assessment_and_runtime_design_process.md`
   says 157 tests, the charter says 159 (the number Checkpoints 1 and 4 are written against). The
   charter's 159 was reproduced unchanged.
6. **The two English files are still in `temp_docs/en/`,** not in `docs/plan/phase_10/`. Publishing
   them is the PM's Checkpoint 0 action, per the handoff's Do-not list. `docs/plan/phase_10/README.md`
   still points at the Vietnamese draft and will need updating at the same time.

## 6. Open questions

1. Should a WP-04 row be added to the Stage 0 table (and Checkpoint 0's "all 4 WPs" updated) before
   the charter is published, or does this package stay part of the PM's housekeeping?
2. Charter §6 is now titled "Decisions taken by the owner". Should the five points keep their
   interrogative form, as here, or be rewritten as declarative statements with the question kept
   only as context?
3. When the PQ list moves to `docs/plan/phase_10/`, should its `templates/…` citation be repointed
   at `docs/decisions/README.md` § Template (Finding 3)?
4. Should the Vietnamese `cratch_notes.md` quotes in the PQ list carry English glosses, as charter
   §6 does, or stay verbatim-only as they are now?

---

## PM review — 2026-09-16 — ACCEPTED with four PM edits applied to both language versions

Checked: the Stage 0/1 tables and §6 of `temp_docs/en/charter.md` against the Vietnamese original;
the identifier evidence in §2 of this report; touch-list respected.

Rulings on the findings and questions:

1. **WP-04 row added** to the Stage 0 table in both versions; the housekeeping heading now says
   "all Stage 0 WPs"; the progress line records WP-04 accepted.
2. **WP-11 reworded** in both versions as "conditional on the PQ-13 ruling" — the original sentence
   was a hedge, not an approval; §6 point 2 approves the scope list, not the deletion itself.
3. **PQ-list template citation** repointed in both versions at the committed copy in
   `docs/decisions/README.md` § Template, keeping the `temp_docs` original as the working file.
4. **157 vs 159** is not drift: 157 `test_` functions, 159 passed including
   `initTestCase`/`cleanupTestCase`. A parenthetical at Checkpoint 1 now says so in both versions.
5. §6 keeps the interrogative form with the quoted ruling beneath — the question is the context
   the ruling answers. The `cratch_notes.md` quotes stay verbatim without glosses; the owner reads
   Vietnamese and the committed-text rule keeps quotes untouched.
6. `docs/plan/phase_10/README.md` is updated at publication (Checkpoint 0), with the two files.
