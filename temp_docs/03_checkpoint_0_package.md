# Gói Checkpoint 0 — Phase 10 (đóng Phase 9, dựng khung, bảng as-is)

**Ngày:** 2026-09-16
**Trạng thái:** Stage 0 đã hoàn tất 5/5 WP; bảng as-is đang được reviewer độc lập đối chiếu với code (mục 4 điền khi có kết quả).
**Người đọc:** owner. Thời gian đọc dự kiến: 15 phút cho mục 1–3.

---

## 1. Bạn cần làm gì ở checkpoint này

| # | Việc | Ghi chú |
|---|---|---|
| 1 | Xác nhận **đóng Phase 9** với bảng carried 14 dòng (13 dòng của WP-00 + dòng A1 M-only tôi thêm) | Bảng nằm ở Checkpoint Z của plan Phase 9 và ở đầu `technical_debt_and_next_steps.md` |
| 2 | Đọc mục 2 (tóm tắt bảng as-is) và mục 3 (ô mở); **chấp nhận bảng as-is làm baseline** của thiết kế lại | Bảng đầy đủ: [runtime_state_contract.md](../docs/domains/task_localization/runtime_state_contract.md) (761 dòng, chỉ cần đọc §3b grid, §5 scenario, §8 drift) |
| 3 | Cho phép **publish** charter + PQ list (bản tiếng Anh) và chuyển `wp/`, `reports/` vào `docs/plan/phase_10/` | Theo quyết định §6.5 của charter |
| 4 | **Sync và tag `phase9-closeout`** từ máy có git | Máy này không chạy git; danh sách file đã đổi ở mục 5 |
| 5 | Trả lời 3 câu hỏi ở mục 7 | Câu 3 là quyết định phạm vi thật sự |

Checkpoint này **không** yêu cầu quyết policy nào; toàn bộ PQ thuộc Stage 2.

---

## 2. Tóm tắt bảng as-is (một trang)

**Kích thước.** 6 state × 23 event = 138 ô: 81 transition có action, 24 IGNORED (state được kiểm tra, event bị bỏ có log), 18 NO-OP (không đọc state, không tác dụng), 15 UNREACHABLE (event không thể tới ở state đó), **0 UNSPECIFIED**. 15 điểm gán `m_cycleState` trong 10 hàm, đánh index AS-01..AS-15. 17 scenario (S-01..S-17), 19 dòng DRIFT giữa docs/UML và code.

**Điều bảng làm lộ ra** — những gì code *thật sự* làm mà docs/UML nói khác, hoặc chưa ai đặt tên. Đây là lí do phải trích bảng trước khi thiết kế lại:

| # | Code làm gì hôm nay | Docs/UML nói gì | Nguồn |
|---|---|---|---|
| 1 | **Cycle fault không bao giờ vào `Faulted`.** Mọi cycle fault đi `WaitingTriggerReset` (trigger còn cao) rồi `Recovering` (trigger hạ); `Faulted` chỉ dành cho setup invalid và index bị từ chối | UML vẽ cạnh `ReadyForTrigger → CycleFaulted` cho pattern/calibration invalid; `task_localization.md` nói "move to recovery or Faulted depending on policy" | DRIFT-2/6/7 |
| 2 | **`Recovering` mang hai nghĩa** không phân biệt bằng member nào: role outage (không có timer) và cycle fault đang chờ auto-clear (có timer) | — | đã biết, PQ-6 |
| 3 | **Escalation 301 chạy `abortCycle()` từ mọi state**, kể cả khi không có cycle. Một write `READY` bị từ chối 3 lần lúc idle → publish `CYCLE_FAULT(301)` với `bMatchingFinished=1`, arm timer 2 s, rồi viết `READY` lại → lặp mỗi ~2 s chừng nào tag còn bị từ chối | Docs mô tả 301 là "abort the cycle" | DRIFT-18/19; nghi vấn defect #3 |
| 4 | **Index non-numeric giữa cycle** → `Faulted` ngay giữa chừng, connection grab/match/send còn sống, cycle không bao giờ publish OK/FAULT, `bMatchingBusy` kẹt = 1 | — | nghi vấn defect #2 |
| 5 | **Role báo status không recoverable** (`Disconnected`/`Connecting`/`NoConnection`) lúc `ReadyForTrigger` → không rút `bTaskReady`; `startCycle()` không kiểm tra role health → trigger vẫn chạy cycle với PLC/output đã ngắt | — | nghi vấn defect #4 |
| 6 | **Timer auto-recover không bị huỷ** khi re-arm qua reconnect hoặc index hợp lệ → có thể nổ trong `ReadyForTrigger` (publish `FAULT_CLEAR` thừa) hoặc `Faulted` (xoá `bTaskFault` dưới latch) | `plc_signal_contract.md` liệt kê 3 điều kiện huỷ, thiếu trường hợp này | DRIFT-17; nghi vấn #5 |
| 7 | **`bErrorReset` trong `WaitingTriggerReset`** xoá `bTaskFault`/`nFaultCode` khi PLC còn giữ trigger và `bMatchingFinished=1` | `task_localization.md`: "keep bTaskFault until the trigger returns to false" | DRIFT-8; nghi vấn #6 |
| 8 | `setup()` invalid gán `Faulted` nhưng **không emit `runtimeFault()`** | `runtime_controller_api.md` và doc comment nói ngược | DRIFT-13; nghi vấn #7 |
| 9 | **Không có publish nào khi teardown** (`STOP` không tồn tại) | `task_localization.md` "Runtime Stop" liệt kê 5 tag cần clear | DRIFT-5 |
| 10 | `onVisionOutputResultFinished()` gán thẳng `ReadyForTrigger`, bỏ qua gate; nếu gate từ chối sau đó, state vẫn `ReadyForTrigger` với `bTaskReady=0` và trigger kế tiếp vẫn chạy cycle | UML: "every arrow into ReadyForTrigger runs through markRuntimeReady()" | DRIFT-1; PQ-11; nghi vấn #1 |

Sáu dòng 3–8 là **nghi vấn defect** do agent trích xuất phát hiện khi đọc code (tổng cộng 12, đủ trong report WP-02). Chúng chưa được coi là xác nhận cho tới khi reviewer độc lập đối chiếu xong (mục 4).

**Test coverage nhìn qua bảng.** 157 hàm test; mọi row "happy path", index refusal, role loss, 301, auto-clear đều có test. **Chưa có test** cho: mọi row `execute()`; nhánh `onVisionOutputResultFinished` gán thẳng; type mismatch từ state khác `ReadyForTrigger`; setter từ `WaitingTriggerReset`/`Recovering`; 301 từ `Running`/`WTR`/`Recovering`/`Faulted`; auto-recover nổ ở `ReadyForTrigger`/`WTR`/`Faulted`; status không recoverable ở mọi state; teardown. Đây chính là vùng các nghi vấn defect nằm — không phải ngẫu nhiên.

---

## 3. Ô mở

- **Trong grid: 0 ô UNSPECIFIED.** Controller quyết định được mọi cặp (state, event).
- **Ở mức adapter/device, 2 scenario chưa xác định** vì phụ thuộc PLC family (first-poll suppression của MC/Modbus): **S-04** (trigger đang cao khi start) và **S-07** (PLC connect muộn, register có được deliver không). Đây đúng là PQ-1 và PQ-2 của cụm A; Stage 2 sẽ đo trên virtual PLC + đọc code device trước khi ra PQ card.
- **Một row reachability chưa chứng minh**: T-RFT-RoleUnhealthy-3 (nghi vấn #11).

---

## 4. Kết quả review độc lập (reviewer Opus xhigh, context mới) — 2026-09-16

**Verdict: ACCEPT WITH CHANGES.** Bản đầy đủ: [reports/WP-02_review.md](reports/WP-02_review.md).

**Bảng mô tả đúng code:** 15 điểm gán khớp; grid cộng đúng từng event; 14 row đối chiếu code khớp guard/action/next state/snapshot; mọi hằng số §6 và fault code đúng; 9/19 DRIFT kiểm tra đều trích đúng nguyên văn; không có đề xuất, không cite số dòng.

**Nghi vấn defect được XÁC NHẬN trên code (xếp theo hậu quả trên cell):**

| # | Lớp | Defect | Đường code |
|---|---|---|---|
| R1 | **Silent wrong result** | Escalation 301 lúc idle: chỉ tag bị từ chối fail, 4 tag handshake còn lại ghi bình thường → master latch trên `bMatchingFinished` thấy "cycle xong, 0 detected" mà không có trigger; `recoverFromFault()` ghi lại `bTaskFault`/`nFaultCode`, `markRuntimeReady()` ghi lại `bTaskReady` → exhaustion tiếp → lặp ~2 s. Test `test_a_link_that_refuses_everything_escalates_once_and_stops` chỉ chờ 1000 ms (< 2000 ms) nên không bắt được vòng lặp | `escalatePlcWriteFailure()` → `abortCycle()`; `recoverFromFault()`; xác nhận bằng cách nâng `qWait` lên 6000 ms |
| R2 | **Hang** | Index non-numeric giữa cycle: `reportSignalTypeMismatch()` không đọc state, không disconnect gì; 4 slot completion đều `return` khi state ≠ `Running`; `TYPE_MISMATCH` không publish snapshot nào xoá `bMatchingBusy` → kẹt = 1, chỉ thoát bằng index hợp lệ | `reportSignalTypeMismatch()`, các slot `on*Finished()` |
| R3 | **Silent wrong result** | Role báo `Disconnected`/`Connecting`/`NoConnection` lúc Ready: `decideRecoveryAction()` trả `Ignore`, `handleRoleStatusChanged()` return trước nhánh Ready → `bTaskReady` vẫn 1; `startCycle()` chỉ kiểm tra group/calibration/camera runner; với PLC `Disconnected`, `trackHandshakeWrite()` bỏ mọi write handshake im lặng — không retry, không 301, không fault | `decideRecoveryAction()`, `handleRoleStatusChanged()`, `startCycle()`, `trackHandshakeWrite()` |
| R4 | Fault sai mã | `m_faultRecoverTimer` nổ trong `Faulted`: `markRuntimeReady()` và các setter không huỷ timer → `FAULT_CLEAR` ghi `bTaskFault=0`, `nFaultCode=0` dưới latch còn giữ, `bTaskReady=0` | `markRuntimeReady()`, `armFaultAutoRecovery()` |
| R5 | Mất chẩn đoán | `bErrorReset` trong `WaitingTriggerReset`: `acknowledgeFault()` không kiểm tra state, `recoverFromFault()` publish vô điều kiện → fault code bị xoá trước khi master đọc; falling edge kế tiếp đi nhánh sạch | `acknowledgeFault()`, `recoverFromFault()` |

Bảy nghi vấn còn lại của WP-02 (PQ-11 gán thẳng, entry point chết, `qDebug`, test nominal, reachability RoleUnhealthy-3, `setup()` invalid không emit `runtimeFault`, UML thiếu cạnh) reviewer không đối chiếu; giữ trạng thái PLAUSIBLE.

**Lỗi trong chính bảng (phải sửa trước khi làm baseline — WP-02b, ở máy có git):**

- §7 gán sai 3 test: fixture chỉ connect camera *active*, camera 2 không bao giờ connect và `requestConnect()` là queued, nên `test_a_camera_rebind_does_not_drop_the_plc_input_stream` và `test_the_runtime_never_writes_the_command_registers` thực ra chạy **T-RFT-SelectCamera-4** (không phải -3) và re-arm qua **T-RFT-RoleHealthy-1** — hai row đang bị liệt kê "chưa có test". Mapping của `test_localization_a_refused_pattern_group_is_not_forgiven_by_a_valid_camera` sang T-FLT-SelectCamera-3 chưa chứng minh. → **Re-derive §7 với quy tắc connect-timing áp dụng thống nhất** (quyết định PM: làm lại, không chỉ thêm caveat, vì §7 là nguồn điền ring 1 của DR và sinh test sau này).
- E22 mô tả thiếu: `destroyRuntimeController()` chỉ ngắt link task↔controller rồi `deleteLater()`; `ITask::endRuntime()` dừng runner *sau đó* → runner→controller connection còn sống tới khi delete chạy; E09/E10/E15 vẫn có thể tới controller đã bị bỏ, `handleRoleStatusChanged()` vẫn có thể `abortCycle()` và ghi PLC; `endRuntime()`/`stopAll()` tạo controller mới ngay. → Ghi lại row E22 và cửa sổ này thành scenario S-18.
- E23 đánh dấu NO-OP nhưng thật ra ghi state của edge detector (`m_lastErrorReset` falling edge; mức trigger quyết định `WaitingTriggerReset` vs `Recovering` ở `abortCycle()` kế tiếp). → **Giữ E23 là event, đổi 6 ô thành A** với row T-Any-LevelSample-1 "records level; feeds the abortCycle ternary" (grid thành A 87 · N 12, vẫn 138). Quyết định PM: giữ vì mức trigger là state thật, thiết kế to-be sẽ mô hình hoá nó.
- Cosmetic: E09/`Running`, E09/`WTR` nên là I (state được kiểm tra, event bị bỏ); thêm row cho guard/log riêng của `startCycle()` ("Cycle start ignored…", hiện dead); §3.1 vs §7.1 về test 301 (§7.1 đúng); thêm scenario "completion không bao giờ tới" (chỉ có watchdog grab ở runner, E13/E14 không có timeout → `Running` không có lối ra riêng); report ghi 761 dòng, file thật 887.

**Ý nghĩa cho câu hỏi 3 ở mục 7:** ba defect R1–R3 là ứng viên hotfix; R4–R5 và phần còn lại để thiết kế lại xử lí.

---

## 5. Những gì đã đổi trên đĩa trong Stage 0 (để sync/tag)

| File | Thay đổi | WP |
|---|---|---|
| `docs/history/plan/phase_9_implementation_plan.md` | header closed-with-carry; location note; status line + bảng carried 14 dòng ở Checkpoint Z; header "Owner decisions" trỏ `docs/decisions/` | WP-00, PM |
| `docs/backlog/technical_debt_and_next_steps.md` | mục "Carried Out Of Phase 9" (14 dòng) | WP-00, PM |
| `docs/backlog/later_todo_list.md` | bảng triage 65 dòng ở đầu; closing note item 60, 23; item 71, 72 mới; ghi chú post-triage | WP-03, PM |
| `AGENT.md` | Read First 3b; hierarchy bullet `docs/decisions/`; bullet Phase 10 ở Highest Priority | WP-01 |
| `docs/README.md` | 2 dòng plan/decisions; dòng `history/plan/` | WP-01, PM |
| `docs/plan/README.md`, `docs/plan/phase_10/README.md` | mới | WP-01 |
| `docs/decisions/README.md`, `DR-0001`…`DR-0008` | mới (+ rule 5; cluster DR-0003/0008 sửa) | WP-01, PM |
| `docs/domains/task_localization/runtime_state_contract.md` | mới, 761 dòng, AS-IS | WP-02 |
| `.claude/agents/wp-implementer.md`, `wp-reviewer.md` | mới (Opus, effort xhigh) | PM |
| `temp_docs/**` | charter, PQ list, templates, handoff WP-00..04, WP-10, reports, bản EN | PM, WP-04 |

Không file source/test/uml nào bị đụng. Không build nào được chạy (Stage 0 là docs-only).

**Sau khi bạn cho phép publish** (mục 1 việc 3), tôi chuyển: `temp_docs/en/charter.md` → `docs/plan/phase_10/charter.md`; `temp_docs/en/open_policy_questions.md` → `docs/plan/phase_10/open_policy_questions.md`; `temp_docs/wp/*` → `docs/plan/phase_10/wp/`; `temp_docs/reports/*` → `docs/plan/phase_10/reports/`; cập nhật `docs/plan/phase_10/README.md` và link trong Checkpoint Z. Bản tiếng Việt vẫn ở `temp_docs/` làm working copy.

---

## 6. Sau Checkpoint 0

- **Stage 1 / WP-10** (funnel `transitionTo()`, guard quan sát, trace line) đã có handoff, có thể phát hành ngay sau publish. Không đổi hành vi, contract suite giữ 159/0 → 161/0.
- **Stage 2 / WP-20** (PQ card cụm A) là việc của tôi; bắt đầu bằng đo S-04/S-07 trên virtual PLC + đọc `McProtocolDevice` / `ModbusRegisterMap` first-poll path.
- Reviewer kết quả (mục 4) quyết định có mục "hotfix" hay không — xem câu hỏi 3.

---

## 7. Ba câu hỏi cho owner

1. **Publish** theo mục 5 — đồng ý?
2. **Stage 1 (WP-10) chạy song song với Stage 2 (PQ card)?** Touch-list rời nhau (code vs docs), và trace line từ WP-10 giúp Stage 2 có field evidence sớm. Tôi khuyến nghị: có.
3. **Với các nghi vấn defect được reviewer CONFIRMED thuộc lớp "silent wrong result / hang" (ứng viên: #4 trigger chạy cycle với role đã ngắt; #2 cycle kẹt `bMatchingBusy=1`; #3 vòng lặp 301 mỗi 2 s), bạn muốn:**
   - (a) **hotfix ngay trên code hiện tại**, mỗi cái một WP nhỏ có test (row tương ứng trong bảng), trước Stage 3 — vì cell đang chạy; hay
   - (b) **để thiết kế lại xử lí** ở Stage 3–4, chấp nhận rủi ro trên cell trong lúc đó.
   Tôi khuyến nghị (a) cho tối đa 3 defect nghiêm trọng nhất sau khi reviewer xác nhận, phần còn lại (b). Mỗi hotfix phải là behaviour-change có DR nhỏ (vì đổi contract quan sát được trên PLC).
