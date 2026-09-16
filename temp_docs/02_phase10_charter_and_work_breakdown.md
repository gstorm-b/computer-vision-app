# Phase 10 — Charter và work breakdown: đóng Phase 9, tổ chức lại, thiết kế lại runtime core

**Ngày:** 2026-09-16
**Vai trò:** tôi là PM + system design engineer; implement giao cho agent khác qua handoff md.
**Trạng thái:** owner đã duyệt cả 5 điểm ở mục 6 (2026-09-16). Stage 0 phát hành 2026-09-16: WP-00..03 giao cho agent, handoff trong `wp/`, report về `reports/`.
**Ràng buộc môi trường:** máy này không chạy git. Mọi WP thiết kế để làm việc bằng touch-list + report; owner sync/tag từ máy được phép tại checkpoint.

---

## 1. Đánh giá phương án "đóng 9 → tổ chức lại → thiết kế lại"

**Kết luận: nên làm, với ba điều kiện.** Không có điều kiện nào trong ba điều này thì phương án trở thành một "cleanup phase" tốn công mà hành vi không tốt lên, hoặc một rewrite làm mất kiến thức đã trả giá trên cell.

### 1.1 Đóng Phase 9 — đóng *trung thực*, không đóng *sạch*

Checkpoint Z hiện ghi **NOT CLOSED** vì owner-run và item carried. Đóng Phase 9 là một hành động sổ sách, không phải một cột mốc kĩ thuật: ghi closeout với bảng carried, mỗi item chỉ đích đến (WP của Phase 10 hoặc backlog #), rồi dừng.

| Carried khỏi Phase 9 | Đích đến đề xuất |
|---|---|
| Phase G / item 54 (held trigger), chưa bắt đầu | **Pilot của Stage 5** — thiết kế G2 được dùng lại, implement trên core mới |
| E4 trên hardware (PLC thật từ chối write) | backlog 69, không chặn |
| D1 vision-output reconnect thất bại trên cell → item 70 | Stage 3 (role health machine) phải có row cho scenario S-06; sửa device bug riêng |
| Owner review `plc_signal_contract.md` (Z2) | Thay bằng review **bảng contract** ở Checkpoint 2 — hiệu quả hơn review 440 dòng prose |
| Doc build (tools thiếu — backlog 44) | không chặn |
| Backlog 51 (device half), 56, 58B, 63, 64, 66, 67, 68 | Triage ở WP-03: cái nào được thiết kế lại hấp thụ, cái nào giữ |

Không làm Phase G bên trong Phase 9. Không chờ E4 hardware hay item 70. Cả hai đều có chỗ tốt hơn trong Phase 10.

### 1.2 Tổ chức lại — *bị giới hạn* và *không đổi hành vi*

Chỉ tổ chức lại những gì thiết kế lại cần. Danh sách đóng, không mở rộng giữa chừng:

| Làm | Không làm |
|---|---|
| `docs/plan/` cho plan hiện hành; `docs/decisions/` với DR đánh số toàn cục; import D1–D8 | Không di chuyển module, không đổi tên file source |
| `runtime_state_contract.md` trích *as-is* từ code (bảng trạng thái hiện tại, kể cả chỗ xấu) | Không đổi tên state, fault code, signal name (PLC program phụ thuộc) |
| Một funnel `transitionTo()` + guard table ở chế độ *quan sát* + dòng trace cố định | Không sửa test hiện có (159 case là lưới an toàn) |
| Triage backlog 65 item (đánh số tới 70): đóng/gộp/giữ, mỗi cái một dòng | Không viết lại docs prose — chỉ đánh dấu "explanatory, see table" |
| (Tuỳ chọn) tách `main.cpp` của contract test theo khu vực | Không tách nếu làm chậm Stage 3; có thể dời sang cuối |

Tiêu chí mỗi WP của phần này: contract suite **cùng số test, 0 fail** trước và sau.

### 1.3 Thiết kế lại — *thiết kế trọn vẹn trước*, *thay lõi phía sau adapter*, không rewrite từ đầu

Tôi đồng ý với nhận định "logic chưa chặt chẽ, tiềm ẩn nhiều bug", nhưng muốn nói chính xác nó *ở đâu*, vì cách sửa phụ thuộc vào đó. Cấu trúc hiện tại cho phép **sáu lớp defect** sau tồn tại mà không test nào nhìn thấy:

1. State được gán ở 15 chỗ; một chỗ (`onVisionOutputResultFinished()`) đã đi vòng qua gate. Không có gì ngăn chỗ thứ hai.
2. Event tới ở state không mong đợi được xử lí bằng chuỗi `if` rải rác, không có bảng đầy đủ → không thể trả lời "cặp (state, event) nào chưa được xử lí".
3. `Recovering` mang hai nghĩa (role outage; cycle fault chờ auto-clear) với hai bộ output khác nhau.
4. `CycleState` gộp **ba mối quan tâm trực giao**: tiến trình của cycle, sức khoẻ của role, tính hợp lệ của selection/config. Mỗi mối quan tâm đã có cơ chế riêng trong code (cycleId, role context, latch per-signal) nhưng bị ép vào một enum 6 giá trị → các state "NotReady / Recovering / Faulted" là những tổ hợp đã bị làm phẳng, và mỗi lần thêm trường hợp mới (PLC connect muộn, held trigger, TCP) lại phải làm phẳng thêm.
5. Đọc `device()->connectStatus()` xuyên thread không đồng bộ; nested `QEventLoop` trong lifecycle.
6. Policy là hằng số và `if` rải rác, không có registry.

Từ đó, ba lựa chọn:

| Lựa chọn | Ưu | Nhược | Đánh giá |
|---|---|---|---|
| **A. Vá từng lỗi trên code hiện tại** | Rẻ từng bước | Không xử lí lớp 2 và 4; TCP command sẽ chồng thêm | Chỉ hợp nếu roadmap 6 tháng tới không đụng runtime |
| **B. Rewrite controller từ spec mới** | Sạch | 159 test + PLC contract + bài học field (latch, first-poll suppression, cycleId, WaitingTriggerReset-is-busy) dễ mất im lặng; mọi defect Phase F đều do owner tìm trên cell — rewrite nhân số lần đó lên | Không khuyến nghị |
| **C. Thiết kế lõi mới trọn vẹn (bảng + API), xây lõi *bên cạnh*, chạy cả bảng test mới lẫn 159 test cũ, rồi chuyển adapter sang lõi mới theo từng slice** | Giữ toàn bộ kiến thức đã có làm lưới; thiết kế vẫn là thiết kế lại thật sự; mỗi bước có thể dừng | Dài hơn B trên giấy; cần kỉ luật touch-list | **Khuyến nghị** |

Phương án C là "thiết kế lại", không phải "refactor nhẹ": lõi mới có cấu trúc khác hẳn (mục 2), chỉ *vỏ* adapter và contract PLC được giữ.

### 1.4 Có đáng không, so với làm feature?

Phase 4 đang hold vì "product cần thêm feature". Phase 10 tiêu khoảng 20 WP cỡ S/M. Đáng, **nếu** hai điều sau đúng — và theo ghi chú của owner thì đúng:
- nguồn lệnh TCP (`Trigger,` / `ChangePart,` / `ChangeCamera,`) sắp vào → thay đổi hình dạng state machine;
- owner vẫn đang tìm thấy defect runtime trên cell mỗi phase.

Nếu roadmap chỉ là mask + manual trong vài tháng tới, làm **Stage 0 + 1 + 2** (khoảng 8 WP, không đổi hành vi, cho ra bảng contract và DR) rồi dừng cũng là kết quả tốt: manual Phase 10 sẽ có nguồn chuẩn tắc, và Stage 3–5 mở lại khi cần.

---

## 2. Hướng thiết kế đề xuất (giả thuyết để Stage 2 kiểm chứng, chưa phải quyết định)

Tách `CycleState` thành **ba máy trạng thái trực giao + một fault register + hai hàm suy dẫn**, tất cả trong một lớp C++ thuần (không QObject, không thread, không timer thật):

```
RuntimeCore
├─ CyclePhase        Idle | Grabbing | Matching | Sending | Publishing | AwaitingTriggerReset
├─ RoleHealth[role]  Connected | Connecting | Lost           (camera, primaryPlc, visionOutput)
├─ Selection[index]  Adopted(n) | Refused(n, latched)         (camera, patternGroup)
├─ ConfigValidity    setupValid, calibrationValid, groupUsable
├─ FaultRegister     none | latched(code, armedAutoClear)
├─ derived: isReady()        = setupValid && all Connected && no Refused && phase==Idle && fault==none
├─ derived: outputSnapshot() = f(phase, fault, isReady, selection)   → bộ giá trị PLC tag
└─ step(event, ctx) -> Actions   (một hàm, một bảng, mọi transition qua đây)
```

- **Event** nguồn-độc-lập: `TriggerRise`, `TriggerFall`, `ErrorResetRise`, `SelectCamera(n)`, `RoleStatus(role, s)`, `GrabDone(ok)`, `MatchDone(id, r)`, `SendDone(ok)`, `WriteDone(id, ok)`, `Timer(kind)`, `Snapshot(map)`, `Setup(ctx)`, `Teardown`.
- **Action** là dữ liệu: `Publish(snapshot)`, `RequestGrab`, `RequestMatch`, `RequestSend(positions)`, `RequestReconnect(role)`, `StartTimer(kind, ms)`, `CancelTimer(kind)`, `Log(level, msg)`, `Trace(from, to, event, reason)`.
- **Policy** là struct tham số truyền vào core: recovery interval, handshake retry budget/delay, auto-clear ms, positions cap, required signals.
- **Adapter** (controller hiện tại, thu nhỏ dần): map runner signal / PLC tag / TCP message / timer → event; thực thi Action qua runner; giữ cycleId và stale-drop; giữ mọi thứ Qt.

Vì sao đây không vi phạm "no broad abstractions before a second implementation": ba máy này **đã tồn tại ngầm** trong code (cycleId + WaitingTriggerReset; role recovery context; latch per-signal) — đây là factoring cái đang có, và implementation thứ hai của "nguồn lệnh" (TCP) đã nằm trong ghi chú của owner.

Hai quyết định thiết kế cần owner xác nhận ở Stage 2 (sẽ có PQ card):
- `Recovering` và `Faulted` không còn là state; chúng là *tổ hợp* (Lost role / Refused / fault latched) và dashboard hiển thị theo tổ hợp. Tên trên PLC không đổi.
- Output PLC được publish theo **diff snapshot** (chỉ tag đổi giá trị) thay vì theo chuỗi `publishXxxOutputs()`; handshake retry vẫn áp cho 5 tag H.

---

## 3. Work breakdown

Cỡ: XS/S/M theo skill planning (≤ 5 file/WP). Mỗi WP là một handoff md riêng (template mục 5). Song song chỉ khi touch-list rời nhau.

### Stage 0 — Đóng và dựng khung (docs only, có thể song song hoàn toàn)

| WP | Nội dung | Cỡ | Touch |
|---|---|---|---|
| **WP-00** | Phase 9 closeout: status line Checkpoint Z, bảng carried có đích đến, header + location note của plan 9 | S | plan 9, `technical_debt_and_next_steps.md` |
| **WP-01** | `docs/plan/` + `docs/decisions/`; import D1–D8 thành DR-0001..0008; `docs/README.md` và `AGENT.md` trỏ tới hai thư mục và charter Phase 10 | S | `docs/plan/**`, `docs/decisions/**`, `docs/README.md`, `AGENT.md` |
| **WP-02** | Trích **as-is** `runtime_state_contract.md` từ code: 6 state × 16 event, mọi ô có giá trị hoặc `UNSPECIFIED`; map 159 test hiện có → row; liệt kê ô không test | M | 1 doc mới |
| **WP-03** | Triage backlog 65 item (đánh số tới 72): đóng / gộp / giữ / hấp thụ-bởi-Phase-10, một dòng mỗi item | S | `later_todo_list.md` (chèn một bảng ở đầu) |
| **WP-04** | Bản tiếng Anh của charter và PQ list để publish vào `docs/plan/phase_10/` tại Checkpoint 0 (bản Việt vẫn là working copy của owner) | S | `temp_docs/en/` |

**Tiến độ Stage 0 (2026-09-16):** WP-00 ✅ accepted (+1 dòng carried cho A1 M-only do PM thêm); WP-01 ✅ accepted; WP-03 ✅ accepted (21 closed-already · 35 keep · 8 absorb · 1 merge · 0 needs-owner); WP-04 ✅ accepted; WP-02 ✅ landed (a)+(b): 138 ô, 0 UNSPECIFIED, 19 DRIFT — reviewer độc lập: **ACCEPT WITH CHANGES** (5 defect xác nhận R1–R5; 3 lỗi trong bảng → **WP-02b** sửa trước khi làm baseline); gói Checkpoint 0 cho owner: [03_checkpoint_0_package.md](03_checkpoint_0_package.md). **Phiên này dừng tại đây theo yêu cầu owner; tiếp tục trên máy có git theo [04_session_handoff.md](04_session_handoff.md).**

**Housekeeping PM làm tại Checkpoint 0, sau khi cả 5 WP Stage 0 xong (một lần chạm mỗi file):**
- ✅ 2026-09-16 `later_todo_list.md`: closing note cho item 60 và 23; item 71 (A1 M-only) và 72 (residual của 61) đã lập; hai bảng carried trỏ tới backlog 71.
- ✅ 2026-09-16 review WP-01: rule 5 cho DR nhập từ lịch sử; cluster DR-0003/0008 sửa; `history/plan/` vào doc map; header plan 9 trỏ `docs/decisions/`.
- Publish charter tiếng Anh vào `docs/plan/phase_10/charter.md`; thêm link vào status line Checkpoint Z và mục "Carried Out Of Phase 9".
- Chuyển `wp/`, `reports/`, PQ list sang `docs/plan/phase_10/` (PQ list dịch sang tiếng Anh khi chuyển).

**Checkpoint 0:** owner đọc bảng as-is (một trang) và danh sách `UNSPECIFIED`; xác nhận đóng Phase 9. Sync/tag `phase9-closeout`.

### Stage 1 — Làm máy hiện tại quan sát được (code, không đổi hành vi, tuần tự)

| WP | Nội dung | Cỡ | Touch |
|---|---|---|---|
| **WP-10** | `transitionTo(next, reason)` là đường ghi duy nhất (15 chỗ); `canTransitionCycleState()` trong header mới, **chế độ quan sát** (log WARN, không chặn); dòng trace `RT state=<from>-><to> event=<e> reason=<r> cycle=<id>`; source-grep test "đúng một điểm gán" | M | controller `.h/.cpp`, header mới, contract test |
| **WP-11** | Xoá entry point chết (`execute()`, queue setters, `onCommDeviceValueChanged()`) — **điều kiện: ruling của PQ-13**; nếu owner giữ chúng làm đường cho UI/TCP thì bỏ WP này | S | `task_localization.*`, controller |
| **WP-12** *(tuỳ chọn)* | Tách `tests/architecture_contract_test/main.cpp` thành 4 file theo khu vực, cùng binary | M | tests |

**Checkpoint 1:** contract suite 159/0 (159 = 157 hàm `test_` + `initTestCase`/`cleanupTestCase`; hoặc cùng số sau WP-11); một field run của owner có dòng `RT state=` trong app_log và **0 guard warning**. Nếu có warning → đó là một transition ngoài dự kiến, ghi vào bảng as-is trước khi đi tiếp.

### Stage 2 — Thiết kế (tôi + reviewer agent + owner)

| WP | Nội dung | Cỡ | Ai |
|---|---|---|---|
| **WP-20** | PQ card cho cụm A (PQ-1..4), PQ-6, PQ-8/9/11 (rẻ), PQ-12/13 (nguồn lệnh) | M | tôi |
| **WP-21** | Bảng **to-be** theo mục 2: state × event, output snapshot, scenario S-01..S-07+, policy registry; mỗi ô có DR hoặc `UNSPECIFIED` | M | tôi |
| **WP-22** | Adversarial review bảng to-be bằng agent mới (fresh context): hoàn chỉnh cặp, guard trùng, invariant, mọi test cũ vẫn map được sang row mới | S | reviewer agent |
| **WP-23** | Spec API `RuntimeCore` (event/action/policy struct, `step()`), theo `api-and-interface-design` | S | tôi |

**Checkpoint 2 (cổng quan trọng nhất):** owner ruling cho các PQ → DR Accepted; bảng to-be không còn ô trống; mọi test cũ có row đích; API core được duyệt. Không viết code core trước cổng này.

### Stage 3 — Xây lõi bên cạnh (code mới, test project mới; song song được theo máy con)

| WP | Nội dung | Cỡ | Touch |
|---|---|---|---|
| **WP-30** | Skeleton `RuntimeCore` + test project `tests/runtime_core_test` table-driven (`_data()`), không event loop; hàm completeness check sinh từ bảng | M | file mới |
| **WP-31** | Cycle machine rows (Idle→…→AwaitingTriggerReset, stale drop, abort) | M | core + test |
| **WP-32** | Role health rows + recovery policy struct | S | core + test |
| **WP-33** | Selection + config validity + fault register rows (latch, auto-clear, ErrorReset) | M | core + test |
| **WP-34** | `outputSnapshot()` + diff publish + handshake retry policy | M | core + test |

**Checkpoint 3:** mọi row có test và xanh; completeness check = 0 thiếu; chưa wire vào app. Contract suite cũ không đổi.

### Stage 4 — Chuyển adapter sang lõi mới (tuần tự, chạm controller)

| WP | Nội dung | Cỡ |
|---|---|---|
| **WP-40** | Adapter: runner signal / PLC tag / timer → event; Action → runner request; controller delegate cycle + fault + readiness cho core; xoá code path cũ theo slice | M (chia 2 nếu vượt 5 file) |
| **WP-41** | Chạy **159 test cũ không sửa** trên lõi mới; mọi đỏ được phân loại: bug lõi mới / test cũ pin hành vi cũ đã đổi theo DR (chỉ được sửa test khi có DR) | S |
| **WP-42** | Owner-run S-01..S-07 theo script + expected `RT` lines; agent replay app_log → bảng expected vs observed | S |
| **WP-43** | Docs prose thu về giải thích + link bảng; UML sinh từ bảng; DR → Implemented | S |

**Checkpoint 4:** contract suite ≥ 159/0; replay khớp; DR Implemented; sync/tag `phase10-core`.

### Stage 5 — Năng lực đầu tiên trên lõi mới (chứng minh thiết kế)

| WP | Nội dung |
|---|---|
| **WP-50** | Phase G (held trigger) theo DR của PQ-1: thêm row, thêm test, owner-run |
| **WP-51** | Nguồn lệnh TCP: adapter mới map message → event; **core không sửa** — đây là tiêu chí thành công của toàn phase |

---

## 4. Rủi ro và cách giảm

| Rủi ro | Giảm |
|---|---|
| Tổ chức lại phình thành cleanup vô hạn | Danh sách đóng ở 1.2; mỗi WP phải giữ 159/0; WP-12 tuỳ chọn |
| Thiết kế đẹp trên giấy, sai trên cell | Stage 1 chạy trước để thu trace thật; bảng to-be phải map được mọi test cũ; Checkpoint 4 có replay log |
| Test cũ bị "sửa cho xanh" | Quy tắc WP-41: chỉ sửa test khi có DR nói hành vi đổi; report phải liệt kê từng test đã sửa và DR tương ứng |
| Không có git cục bộ → agent làm ngoài phạm vi | Touch-list trong handoff; changed-file list trong report; reviewer đối chiếu; owner sync/tag từ máy được phép ở mỗi checkpoint |
| Hai agent va nhau trên controller | Stage 1 và 4 tuần tự; Stage 0 và 3 song song theo file rời |
| Owner-run dồn cục | Chỉ ba điểm: Checkpoint 1 (một field run lấy trace), 4 (S-01..S-07), 5 |
| Bump schema / contract PLC | Không đổi tên tag/fault code trong Phase 10; nếu DR nào đòi đổi, ghi migration riêng như C3 đã làm |

---

## 5. Giao thức làm việc PM ↔ agent (qua md, không git cục bộ)

```
temp_docs/            (trong lúc chờ duyệt)  →  docs/plan/phase_10/   (sau duyệt)
├─ charter.md                       ← file này
├─ wp/WP-xx_<slug>.md               ← handoff, tôi viết, agent đọc
├─ reports/WP-xx_report.md          ← agent viết khi xong
└─ (docs/decisions/DR-xxxx.md)      ← tôi viết sau ruling của owner
```

**Handoff (tôi → agent), tối đa ~1 trang:**
1. Goal — một câu.
2. Read first — ≤ 5 link (AGENT.md luôn là số 1).
3. Facts — điều đã xác minh, cite symbol/test id.
4. Steps — có thứ tự.
5. Acceptance — ≤ 5 gạch đầu dòng, kiểm được.
6. Verification — lệnh build/test nguyên văn từ `build_and_verification.md`; tên test.
7. Touch-list — file được phép; **Do-not** — cái không được chạm.
8. Stop rules — khi nào dừng và hỏi (ví dụ: một test cũ đỏ; cần sửa file ngoài touch-list; phát hiện hành vi không có trong bảng).
9. Report format — mục dưới.

**Report (agent → tôi):**
1. Changed files — đầy đủ, từng file.
2. Evidence — test id xanh/đỏ, count, dòng log.
3. Negative check — inject gì, đỏ đúng test nào, đã restore (xác nhận bằng cách nêu lại vị trí).
4. Deviations — khác handoff ở đâu, vì sao.
5. Findings out of scope — một dòng mỗi cái, không sửa.
6. Open questions.

**Review (tôi):** đọc report, mở từng file trong changed-file list, chạy lại lệnh verification, đối chiếu touch-list. Một WP không có report đầy đủ thì không được coi là xong. Owner chỉ nhìn checkpoint.

**Ngôn ngữ:** handoff, report, DR, contract — tiếng Anh (sẽ commit). Charter và trao đổi với owner — tiếng Việt.

---

## 6. Owner cần quyết ngay để bắt đầu

1. Đóng Phase 9 theo bảng carried ở 1.1 — đồng ý?

    Đồng ý

2. Phạm vi tổ chức lại theo 1.2 — bỏ/thêm mục nào? (đặc biệt WP-12 tách test, và WP-11 xoá entry point chết)

    Tổ chức lại theo 1.2 đã đề xuất.

3. Hướng thiết kế ở mục 2 — chấp nhận làm giả thuyết cho Stage 2, hay muốn thấy phương án thay thế trước?

    Chấp nhận.

4. Làm trọn Stage 0–5, hay dừng ở Stage 2 rồi quyết tiếp (mục 1.4)?

    Theo đề xuất của bạn.

5. Nơi đặt plan: giữ `temp_docs/` tới Checkpoint 0, rồi chuyển sang `docs/plan/phase_10/` — đồng ý?

    Đồng ý.

Handoff mẫu đã viết sẵn để owner thấy hình dạng: [wp/WP-00_phase9_closeout.md](wp/WP-00_phase9_closeout.md) và [wp/WP-10_transition_funnel.md](wp/WP-10_transition_funnel.md).
