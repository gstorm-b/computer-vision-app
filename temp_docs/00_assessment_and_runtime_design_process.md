# ncr_picking — Đánh giá sơ bộ và đề xuất quy trình thiết kế policy / logic / state machine cho runtime controller

**Ngày:** 2026-09-16
**Trạng thái:** bản tư vấn, chưa áp dụng vào codebase. Nằm ngoài `docs/` có chủ đích.

> **Đính chính 2026-09-16 (sau phản hồi của owner):** project **có** git ở nơi khác; **máy này không được
> phép chạy git**. Vì vậy R1 bên dưới không còn là "không có VCS" mà là "không có git cục bộ". Mọi chỗ trong
> tài liệu nói `git init` / branch / stash / tag đọc là: *owner sync và tag từ máy được phép tại mỗi checkpoint;
> trên máy này agent làm việc bằng touch-list, test id và khôi phục thủ công trong cùng phiên*. Bản kế hoạch
> chi tiết đã điều chỉnh theo ràng buộc này nằm ở [02_phase10_charter_and_work_breakdown.md](02_phase10_charter_and_work_breakdown.md).
**Người đọc:** project owner. Văn bản này viết tiếng Việt vì là tài liệu làm việc; mọi template ở phụ lục viết tiếng Anh vì chúng sẽ được đưa vào `docs/` sau khi owner duyệt.

---

## 0. Tóm tắt điều hành

1. **Dự án có nền docs, rule và test kỷ luật cao hơn mức thường thấy** — `AGENT.md` làm authority map, scope card từng module, architecture contract test, virtual device là shipped code, negative check bắt buộc, quyết định của owner được ghi nguyên văn. Đây là tài sản lớn, cần giữ.
2. **Máy làm việc này không chạy được git** (project có git ở nơi khác). Hệ quả cục bộ vẫn phải được thiết kế quanh: negative check là "sửa source cho hỏng rồi khôi phục bằng tay trong cùng phiên"; checkpoint ghi bằng test id + count và được owner tag từ máy được phép; review của agent dựa trên touch-list trong report, không dựa trên diff.
3. **Contract của runtime controller đang tồn tại nhưng phân tán và viết theo lối tường thuật**: ít nhất 5 tài liệu + UML + code + backlog cùng mô tả hành vi, xen lẫn lịch sử ("since Phase 9 / C3", "earlier versions said the opposite"). Không có một bảng chuẩn tắc nào dạng *state × event → guard → action → next state → outputs*. Hệ quả nhìn thấy được: docs "drifted into fiction" (Phase 9 / Z2), UML dùng tên khác enum, contract có lỗ "Not yet specified", và cùng một lớp câu hỏi policy quay lại nhiều lần (item 54, 58B, 64, 68, 70).
4. **Controller là một class ~3000 dòng** (`localization_runtime_controller.cpp` 2229 + `.h` 738) gộp state machine, IO plumbing, retry, latch, logging. Suite test là một file 5839 dòng với 157 test. Cả hai vẫn đúng, nhưng không còn *đọc được* — và gần như mọi defect Phase F/9 đều do owner phát hiện trên cell chứ không phải test.
5. **Đề xuất cốt lõi: chuyển sang "contract-first runtime loop"** — mỗi câu hỏi policy đi qua 1 Policy Question card → 1 Decision Record → cập nhật *bảng* state/event/transition (nguồn chuẩn tắc duy nhất) → test sinh từ bảng → implement trong một *pure state core* tách khỏi IO → verify 3 vòng (table test, owner-run có script, replay log) → đóng bằng git tag. Pilot đầu tiên: cụm "startup arming" (item 54 + 58B + thứ tự connect + "set fault before recovering"), vì đây là chỗ sắp phải quyết và đã có sẵn Phase G làm nền.

---

## Phần A — Đánh giá sơ bộ

### A.1 Điểm mạnh cần giữ nguyên

| Điểm mạnh | Bằng chứng | Vì sao quan trọng |
|---|---|---|
| Authority map rõ ràng cho agent | `AGENT.md` → `docs/README.md` → scope card `AGENTS.md` mỗi module | Agent mới vào biết đọc gì, không đọc gì; source-of-truth hierarchy có thứ tự khi docs mâu thuẫn |
| Layering được enforce bằng test, không bằng lời | `architecture_contract_test::test_module_include_layering_contract` | Cấu trúc module không trôi theo thời gian |
| Runner là con đường duy nhất qua thread | `src/runtime/AGENTS.md`, capability "ask the device, not the family" | Đây là quyết định đúng cho hệ có PLC + camera + TCP; đã trả giá và ghi lại bài học |
| Virtual device là shipped code, không phải test double | `docs/domains/virtual_devices/virtual_devices.md` | Một implementation, hai consumer, không drift; đã bắt được 4 product bug |
| Quyết định owner ghi nguyên văn, có rationale | D1–D8 trong `phase_9_implementation_plan.md` | Agent phase sau không re-litigate |
| Negative check hai chiều (red + stay-green) | Rule chung của Phase 9 | Đã bắt được một negative check "pass vì lí do sai" ở Phase 8 |
| OWNER-RUN tách bạch khỏi verified | Toàn bộ plan Phase 9 | Không có "verified" giả trên UI khi chưa có widget test |
| Bản thân runtime đã có vài invariant tốt | Một đường re-arm duy nhất `markRuntimeReady()`; `cycleId` chống stale result; latch per-signal; một validator cho mọi entry point | Đây là những "mảnh" state machine chặt chẽ đã tồn tại — quy trình đề xuất chỉ cần *gom* chúng lại thành bảng, không cần phát minh lại |

### A.2 Rủi ro / điểm yếu, xếp theo mức độ ảnh hưởng

#### R1 — Không có git cục bộ trên máy làm việc (đính chính: project có git ở nơi khác)

Thư mục làm việc không có `.git`; máy này không được phép chạy git. Đây là ràng buộc môi trường, không phải lỗi dự án, nhưng quy trình phải được thiết kế quanh nó vì hệ quả đang thấy trong docs:

- Negative check = sửa source cho defect quay lại, chạy test, rồi *restore bằng tay*. Một lần quên restore là defect ship thật. Cần quy tắc: inject và restore trong cùng phiên, report ghi rõ file đã đụng.
- Checkpoint được định nghĩa bằng *số lượng test* ("101 → 103"), và chính plan thừa nhận chuỗi số này "false precision that drifts". Cần ghi checkpoint bằng test id.
- Line-number citation trong docs/plan rot trong vài ngày (plan Phase 9 tự ghi "roughly a dozen stale cites"). Cần cite theo symbol.
- Owner không có diff cục bộ để review từng task; cần agent liệt kê touch-list đầy đủ trong report, và owner sync + tag từ máy được phép tại mỗi checkpoint (ví dụ trước/sau bump schema).

#### R2 — Contract runtime phân tán, tường thuật, xen lịch sử

Cùng một hành vi hiện được mô tả ở:

| Tài liệu | Vai trò tự nhận | Vấn đề |
|---|---|---|
| `task_localization.md` | "behavior contract" | Vẫn chứa đoạn đã lỗi thời ("first implementation pass runs matching synchronously", "controller remains owned by the task object") cạnh đoạn mới |
| `plc_signal_contract.md` | PLC contract | Đúng và chi tiết, nhưng là prose 440 dòng; có mục "Not yet specified" (held trigger) |
| `runtime_controller_api.md` | API + cycle state | Liệt kê 6 state và một happy path 13 bước; không có bảng transition |
| `maintenance_and_extension.md` | how-to | Có "Runtime Readiness Invariants" — thực chất là guard của state machine, nằm trong tài liệu how-to |
| `uml/08_runtime_state_machines.puml` | diagram | Vẽ tay; tên state "CycleFaulted"/"RecoveringCycle" khác enum; note dài hơn diagram |
| Code (`localization_runtime_controller.cpp`) | implemented truth | Là nơi duy nhất trả lời được "event X đến ở state Y thì sao" |
| Backlog item 54/58/64/68/70 | policy chưa quyết | Mỗi item là một lỗ trong contract, ghi ở nơi khác contract |

Không có nơi nào trả lời **đầy đủ** câu: *với mỗi cặp (state, event), guard là gì, action là gì, next state là gì, output PLC là gì*. Vì thế:

- Agent phải đọc code để trả lời; docs được hand-rewrite định kì rồi lại drift (Z2 ghi rõ "signals, fault codes and recovery model had drifted into fiction").
- Cùng một "lớp" câu hỏi (edge vs level, first-poll suppression, cái gì là fault vs cái gì là not-ready) quay lại ở 54, 58B, G2, 70.
- Owner không thể review policy trên một trang; phải đọc plan 3500 dòng.

#### R3 — Controller là God-class; state transition không tách khỏi IO

`localization_runtime_controller.cpp` 2229 dòng + header 738 dòng gánh: signal mapping, validate index, role binding, recovery timer, handshake write retry (E4), cycle orchestration, build output positions (kể cả cap 2 vị trí chưa ai giải thích — item 68), logging rate-limit, dashboard events. Khi mọi thứ ở một chỗ:

- "Đọc để hiểu policy" đồng nghĩa "đọc 3000 dòng".
- Test phải dựng runner/thread thật cho cả những câu hỏi thuần logic ("held trigger lúc start có fire không").
- Thêm nguồn lệnh thứ hai (TCP `Trigger,` / `ChangePart,` / `ChangeCamera,` trong scratch notes) sẽ phải chạm vào cùng file này, cùng lúc với `handlePlcValues`.

*(Nhận xét ở mức code — transition tập trung hay rải rác, thread nào chạm state — xem A.4, bổ sung sau khi khảo sát source.)*

#### R4 — Test suite lớn nhưng không có "coverage theo transition"

- Một file `main.cpp` 5839 dòng, 157 test, nhiều fixture. Không có bảng nào nói "transition này được test bởi case nào".
- Kiểm chứng bằng *đếm* test → dễ chạy nhầm binary (plan ghi rõ "a task that reports 96 has not passed; it has run the wrong binary"), dễ có test "biến mất" mà suite vẫn xanh (backlog 62).
- Test chủ yếu là scenario tường thuật; không có table-driven test cho (state, event) → next.

#### R5 — Workflow owner–agent tốt nhưng nặng và thiếu vài mảnh

- **Request** của owner là ghi chú tiếng Việt, tự nhận "mình chỉ dự tính"; agent từng hiểu nhầm là spec (plan Phase 9 phải sửa). Cần một bước "định khung câu hỏi" trước plan.
- **Plan** 3500 dòng cho một phase; **backlog** 3400 dòng, 70 item. Owner review khó; agent load context tốn.
- **Decision** ghi trong plan, đánh số D1..D8 *theo phase* → phase sau lại D1. Không có decision log toàn cục.
- `docs/history/plan/` là "traceability only" theo `AGENT.md` nhưng chứa plan *hiện hành*; khuyến nghị chuyển sang `docs/plan/` đã "năm phase tuổi".
- Line-number citation là quy ước chính trong plan/backlog → rot nhanh; nên cite theo symbol.
- OWNER-RUN item chưa có script + expected log lines; agent phải dựng lại số liệu từ timestamp (scratch notes F3).

#### R6 — Scope sắp tới sẽ ép state machine đổi hình

Từ `phase_10_request.md` và `cratch_notes.md`:

- Mask cho pattern (không chạm state machine).
- **Signals chart UML cho mọi trường hợp** + **user manual có giải thích signal** → cần chính bảng contract ở R2 làm nguồn, nếu không manual sẽ là bản tường thuật thứ sáu.
- **TCP message control**: `Trigger,<task>`, `ChangePart,<idx>`, `ChangeCamera,<n>` → nguồn lệnh thứ hai song song PLC. Nếu state machine vẫn nhận event dưới dạng "PLC tag value" thay vì "command", đây sẽ là một nhánh `if` mới trong `handlePlcValues` và một lớp bug mới (ưu tiên nguồn nào, ack qua kênh nào, held-trigger nghĩa là gì trên TCP).
- Các ý policy rời rạc: "chặn connect camera/output trước khi PLC connected", "set fault trước khi về Recovering", "Recovering không disconnect hoàn toàn output device", "bug: PLC connect muộn thì camera/index number không update". Tất cả thuộc cùng cụm **startup / arming / recovery ordering** — là pilot lí tưởng cho quy trình mới.

### A.3 Kết luận đánh giá

Dự án không thiếu kỷ luật, thiếu **một nguồn chuẩn tắc dạng bảng cho hành vi runtime** và **một vòng lặp quyết định policy ngắn** để owner ra quyết định trên một trang thay vì trên 3500 dòng plan. Cộng với việc chưa có git, ba việc này cần làm trước khi thêm nguồn lệnh TCP.

### A.4 Nhận xét ở mức code (khảo sát 2026-09-16, cite theo symbol)

**Cái đã tốt, cần nhân rộng chứ không thay:**

- Task-level FSM (`TaskState`) đã có **guard table** `canTransitionTaskState()` trong `task_state_machine.h` và **một điểm chuyển duy nhất** `ITask::transitionTaskState()` có log. Đây chính là mẫu cần áp cho `CycleState`.
- Ba regime lỗi tách bạch có chủ đích và được ghi rõ trong code: mất kết nối → retry vô hạn, không fault; cycle fault → latch + auto-clear 2 s qua `recoverFromFault()`; handshake write fail → retry 3 lần rồi `301` qua `escalatePlcWriteFailure()` có chống re-entry.
- `markRuntimeReady()` là funnel re-arm; `++m_activeCycleId` chống stale result; latch per-signal; một validator `validateCameraNumber()` / `validatePatternGroupNumber()` cho ba call-site; `commandedIndexFromPlc()` tách "unmapped / not-read / value".

**Cái làm policy khó chặt:**

1. **`CycleState` (6 giá trị, private) được gán ở 15 chỗ trong 10 hàm** — `setActiveCameraNumber`, `setActivePatternGroupNumber`, `setup`, `handlePlcValues`, `markRuntimeReady`, `reportSignalTypeMismatch`, `startCycle`, `abortCycle`, `handleRoleStatusChanged`, `onVisionOutputResultFinished`. Không có `transitionTo()`, không guard table, không dòng log transition thống nhất. Riêng `onVisionOutputResultFinished()` gán thẳng `ReadyForTrigger` rồi mới gọi `markRuntimeReady()` — con đường duy nhất đi vòng qua gate.
2. **"Policy" không phải một khái niệm trong code** mà là 6–7 cơ chế rời: `LocalizationRecoveryPolicy` (kiểu policy duy nhất có tên; `setRecoveryPolicies()` không có caller production), `isHandshakeSignal()`, `requiredSignalNames()`, latch per-signal, disposition trong `buildVisionOutputPositions()` (cap 2 vị trí hard-code, vị trí dư bị `Skipped` không lí do — item 68), retry grab nằm ở `CameraRunner::kMaxGrabAttempts`. Không có nơi nào liệt kê chúng cạnh nhau với tham số và test tương ứng.
3. **Event chưa nguồn-độc-lập.** `handlePlcValues()` nhận map tag→value, tự detect edge cho `bExecuteTrigger`/`bErrorReset`, tự gọi setter cho index. Nguồn lệnh TCP sắp tới sẽ phải chen vào đây hoặc nhân đôi logic edge.
4. **Vài ngữ nghĩa chưa được đặt tên thành quyết định**: "no match" không phải fault (cycle thành công với `nDetectedNumber = 0`) — có thể đúng nhưng chưa có DR; `m_lastErrorReset` không reset trong `setup()` trong khi `m_lastExecuteTrigger` có (item 54 ghi nhận); một `qDebug()` tạm còn trong hot path (item 68).
5. **Entry point chết làm bề mặt API rộng hơn hành vi thật**: `execute()` / `TaskLocalization::executeLocalization()`, `queueSetActiveCameraNumber()` / `queueSetActivePatternGroupNumber()`, `onCommDeviceValueChanged()` không có caller hoặc `connect` production. Agent sau dễ hiểu nhầm đây là đường đang dùng.
6. **Threading rõ nhưng có vài mép chưa đóng**: controller ở runtime thread, runner QObject ở GUI thread, kết nối auto→queued, request ra là direct call nhưng chỉ emit signal queued — mô hình đúng. Không có mutex. Hai chỗ đọc `device()->connectStatus()` xuyên thread không đồng bộ (`allRequiredRolesHealthy()`, `trackHandshakeWrite()`) — rủi ro thấp hôm nay, nhưng là đúng loại dữ liệu phải đi vào "context snapshot" khi tách pure core. `awaitPrimaryPlcSnapshot()` chạy nested `QEventLoop` trên GUI thread; `setupRuntimeController()` dùng `BlockingQueuedConnection`; runner không có state enum (chỉ `m_busy`), `disconnectAndWait()` / `detachFromOutsideThread()` là nested loop blocking.
7. **Test không nhìn thấy state**: `CycleState` private nên test chỉ quan sát gián tiếp qua PLC output; toàn suite không có `_data()` / `QFETCH`; transition-rule test chỉ có cho `TaskState` với ~10 assertion tay (không phủ ma trận 9×9). Không có test project riêng cho controller hay runner.

**Hệ quả cho quy trình:** bước đầu tiên của việc tách pure core nên là ba việc **cơ học, không đổi hành vi**: (a) một hàm `transitionTo(CycleState, reason)` làm đường ghi duy nhất (15 chỗ sửa), (b) `canTransitionCycleState()` guard table giống `TaskState`, (c) một dòng trace cố định tại đó. Chỉ riêng ba việc này đã cho phép ring 3 (replay log) hoạt động và cho test assert được transition trước khi làm bất kì refactor lớn nào.

---

## Phần B — Quy trình đề xuất: "Contract-first runtime loop"

Mục tiêu: owner và agent thiết kế được policy / logic / state của runtime controller **chặt chẽ, review được trên một trang, test sinh từ bảng, và có bằng chứng field khi đóng**.

### B.0 Chuẩn bị một lần

| Việc | Kết quả |
|---|---|
| `git init`, `.gitignore`, commit baseline, tag `baseline-2026-09-16` | Mọi bước sau có diff, có rollback |
| Tạo **một** tài liệu chuẩn tắc: `docs/domains/task_localization/runtime_state_contract.md` (template: Phụ lục 1) | Nơi duy nhất trả lời "(state, event) → ?" |
| Tạo `docs/decisions/` với đánh số toàn cục `DR-0001…` (template: Phụ lục 2); import D1–D8 Phase 9 và các quyết định cũ còn hiệu lực | Decision log không reset theo phase |
| Chuyển plan/request hiện hành ra `docs/plan/`; `docs/history/` chỉ giữ cái đã đóng | Hết mâu thuẫn với `AGENT.md` |
| Quy ước cite theo **symbol** (`Class::method`, tên test), không theo số dòng | Docs không rot theo edit |

### B.1 Vòng lặp cho MỖI câu hỏi policy

```
[1] Frame        owner nêu ý (VN, tự do)  →  agent viết Policy Question card (1 trang)
[2] Decide       owner chọn option        →  agent ghi DR-xxxx (Accepted)
[3] Model        agent cập nhật BẢNG: state / event / transition / output / scenario
                 adversarial review trên bảng (không phải trên code)
[4] Derive tests mỗi row transition & mỗi scenario → test id; table-driven
[5] Implement    pure state core trước, adapter sau; log transition theo format cố định
[6] Verify       ring 1 table tests (virtual devices)
                 ring 2 owner-run theo script + expected log lines
                 ring 3 replay app_log → agent đối chiếu trace
[7] Close        DR: Accepted+Implemented; bảng cập nhật; UML regen; backlog item đóng
                 bằng test id + log evidence; git tag
```

#### [1] Frame — Policy Question card

Agent dùng skill `interview-me` (hỏi một câu một lần) rồi viết card, tối đa một trang, gồm:

- **Câu hỏi** đúng một câu ("Trigger đang giữ mức cao khi runtime start thì cycle có chạy không?").
- **Ngữ cảnh kĩ thuật thật**: điều gì xảy ra hôm nay, có bằng chứng (test id / log line / symbol), không suy đoán. Ví dụ item 54: "trên MC và Modbus, first poll bị suppress nên edge không bao giờ tới; hệ quả thực là task Ready mà không nói gì".
- **Các option (2–4)** với hệ quả trên **PLC program / robot program / operator**, không phải trên code.
- **Ràng buộc từ DR đã có** (ví dụ D7: index invalid vẫn là fault).
- **Khuyến nghị của agent** + lí do trong 3 dòng.
- **Cái gì sẽ đổi trong bảng** nếu chọn mỗi option (số row thêm/sửa).

Owner trả lời bằng tiếng Việt trong chat; agent ghi DR bằng tiếng Anh, quote nguyên văn câu của owner như Phase 9 đang làm.

#### [2] Decide — Decision Record

Một file/một mục mỗi quyết định, đánh số toàn cục, trạng thái `Proposed → Accepted → Implemented → Superseded`. Nội dung tối thiểu: câu hỏi, quyết định, rationale, option bị loại và vì sao, ràng buộc lên PLC/robot, row nào trong bảng contract bị ảnh hưởng, test id, bằng chứng field khi Implemented.

Quy tắc: **docs chuẩn tắc chỉ mô tả hiện tại**; mọi câu "earlier versions said the opposite" chuyển sang DR. Đây là cách xoá lịch sử khỏi contract mà không mất traceability.

#### [3] Model — Bảng thay cho prose

`runtime_state_contract.md` gồm 5 bảng (template Phụ lục 1):

1. **State table**: tên state (đúng tên enum), invariant, snapshot output PLC bắt buộc khi ở state đó, ai được phép rời state.
2. **Event table**: tên event, nguồn (PLC tag / TCP command / runner status / timer / UI), edge hay level, thread phát, có được queue khi busy không.
3. **Transition table**: `state × event → guard → actions → next state`. **Mọi cặp phải có row**, kể cả row "Ignore + log". Ô trống là lỗi. Ô chưa quyết ghi `UNSPECIFIED (DR-xxxx pending)`.
4. **Output table**: với mỗi transition, bộ giá trị tag PLC được publish (thay cho các khối `bTaskReady = false …` đang lặp trong 3 docs).
5. **Scenario catalogue**: Given/When/Then dạng dãy event, bao gồm mọi scenario field đã quan sát (item 58, 64, 70…) và scenario của user manual sau này.

Adversarial review (skill `doubt-driven-development`) chạy **trên bảng**: agent thứ hai đi từng (state, event) hỏi "row này có hợp với invariant của state không", "hai row có cùng guard không", "event từ nguồn TCP có row chưa". Rẻ hơn nhiều so với 13-agent pass trên code.

Vì sao bảng, không phải diagram: diagram (PlantUML) nên **sinh từ bảng** bằng script nhỏ, để UML không thể sai khác enum như hiện nay.

#### [4] Derive tests — coverage theo row

- Mỗi row transition có id `T-<state>-<event>-<n>`; mỗi scenario có id `S-<n>`. Test đặt tên theo id.
- Table-driven test cho phần pure core: dữ liệu vào là (state, event, context flags), kì vọng là (next state, actions). QtTest `_data()` phù hợp.
- Coverage report = "row nào chưa có test", không phải "bao nhiêu test". Checkpoint nói "T-… và S-… xanh", không nói "153 passed".
- Negative check làm trên branch/stash, không sửa tay trên working tree.
- Tách `tests/architecture_contract_test/main.cpp` thành nhiều file theo khu vực (layering / runners / runtime core / task-level); giữ một binary hoặc nhiều, tuỳ, nhưng không còn một file 6000 dòng.

#### [5] Implement — pure state core, adapter bên ngoài

Không refactor lớn một lần. Dùng strangler: mỗi câu hỏi policy mới được implement trong một **pure core** (`RuntimeCycleStateMachine` hoặc tên tương đương):

- Input: `(state, event, context snapshot)`; output: `(next state, list<Action>)`. Không Qt signal, không runner, không timer, không log side-effect — chỉ trả về action "Publish(tag, value)", "RequestGrab", "StartTimer(kFaultAutoRecoverMs)", "Log(level, msg)".
- Controller hiện tại trở thành adapter: nhận signal từ runner → dựng event → gọi core → thực thi action qua runner. Phần đã có (`markRuntimeReady()`, latch, validator) chuyển vào core theo từng slice.
- Quy tắc cứng: **không gán state ở bất kì đâu ngoài một hàm `transitionTo()`**, hàm này log một dòng theo format cố định `RT state=<from>-><to> event=<event> reason=<...> cycle=<id>` — dòng này là thứ ring 3 replay.
- Event phải **nguồn-độc-lập**: `TriggerRise`, `SelectCamera(n)`, `ErrorReset` — không phải "tag M10 đổi". Việc map PLC tag hay TCP message thành event nằm ở adapter. Đây là điều kiện để thêm TCP command mà không sửa core.

#### [6] Verify — ba vòng

| Ring | Ai | Cái gì | Bằng chứng |
|---|---|---|---|
| 1 | agent | table tests + scenario tests với virtual devices; negative check trên branch | test id xanh/đỏ có chủ đích |
| 2 | owner | OWNER-RUN theo **script** viết sẵn (bước bấm, giá trị tag) và **expected log lines** | app_log + ảnh dashboard |
| 3 | agent | replay: parse `RT state=…` lines trong app_log của owner, đối chiếu với scenario catalogue | bảng "expected vs observed" |

Ring 3 là thứ hiện chưa có: scratch notes F3 cho thấy agent phải dựng số liệu từ timestamp vì dòng tóm tắt cycle chỉ vào task-log panel, không vào app_log. Một format transition-line cố định giải quyết việc này một lần cho tất cả.

#### [7] Close

DR → `Implemented` kèm test id và log evidence; bảng contract là bản mới nhất; PlantUML regen; backlog item đóng bằng link tới DR; `git tag`. Không có "verified" nếu thiếu một trong ba.

### B.2 Phân vai

| | Owner | Agent |
|---|---|---|
| Nêu vấn đề | Tiếng Việt, tự do, có thể tentative | Không coi note là spec; luôn qua bước Frame |
| Quyết định | Chọn option, có thể override khuyến nghị | Ghi DR nguyên văn; không re-litigate |
| Mô hình hoá | Review bảng (một trang) | Viết bảng; adversarial review bảng |
| Test | — | Sinh từ bảng; đặt tên theo id |
| Implement | — | Pure core trước; adapter sau; slice nhỏ |
| Verify | Ring 2 theo script | Ring 1, ring 3 |
| Đóng | Xác nhận field evidence | DR, docs, UML, tag |

### B.3 Quy tắc nhỏ nhưng có tác dụng lớn

1. **Plan cap**: task card ≤ 1 trang; plan ≤ ~600 dòng; chi tiết dài link sang doc riêng. Plan 3500 dòng không ai review nổi.
2. **Không cite số dòng** trong docs/plan/backlog; cite symbol hoặc test id.
3. **Checkpoint = git tag + danh sách test id**, không phải tổng số test.
4. **Mọi OWNER-RUN item** kèm script + expected log lines trước khi giao owner.
5. **Contract docs không chứa lịch sử**; lịch sử vào DR.
6. **Mỗi phase mở bằng một "policy question list"** (những câu owner phải quyết) trước khi agent viết plan — chính là điều Phase 9 đã làm ngầm với D1–D8 nhưng sau khi draft plan.

### B.4 Ánh xạ sang skill sẵn có trong `.claude/skills`

| Bước | Skill |
|---|---|
| [1] Frame | `interview-me` → `idea-refine` (khi >2 option) |
| [2] Decide | `documentation-and-adrs` |
| [3] Model | `spec-driven-development` (spec = bảng), `doubt-driven-development` (review bảng) |
| [4] Derive tests | `test-driven-development` |
| [5] Implement | `incremental-implementation`, `api-and-interface-design` (event/action API của core) |
| [6] Verify | `observability-and-instrumentation` (format transition line), `debugging-and-error-recovery` |
| [7] Close | `git-workflow-and-versioning`, `code-review-and-quality` |

Meta-skill `using-agent-skills` vẫn là entry; thêm một dòng route: "Runtime policy / state question? → contract-first loop (this doc)".

---

## Phần C — Đề xuất về quản lí source và cấu trúc

### C.1 Làm việc không có git cục bộ (đính chính)

- Owner sync và tag từ máy được phép tại mỗi checkpoint (đề xuất tag `phase9-closeout`, `phase10-cp<n>`); diff để review nằm ở đó.
- Trên máy này: mỗi WP/task có **touch-list** (file được phép sửa) trong handoff và **changed-file list** trong report; agent không đụng file ngoài touch-list.
- Negative check: inject và restore trong cùng phiên; report ghi rõ đã restore; reviewer xác nhận bằng cách đọc lại vị trí đã inject.
- Không hai agent cùng sửa một file trong cùng thời điểm; song song chỉ giữa các touch-list rời nhau.

### C.2 Docs

- `docs/plan/` (active) tách khỏi `docs/history/plan/` (closed).
- `docs/decisions/DR-xxxx-<slug>.md` toàn cục.
- `docs/domains/task_localization/runtime_state_contract.md` là chuẩn tắc; `task_localization.md`, `runtime_controller_api.md`, `plc_signal_contract.md` giữ lại nhưng thu về vai trò *giải thích* và link tới bảng, xoá đoạn lỗi thời.
- `uml/08_runtime_state_machines.puml` sinh từ bảng (script Python nhỏ trong `scripts/`).

### C.3 Code (từng slice, không big-bang)

- `src/model/runtime_cycle_state_machine.{h,cpp}` (pure core) — bắt đầu bằng slice trigger/arm (Phase G), rồi fault/recover, rồi index selection.
- `LocalizationRuntimeController` giữ vai trò adapter; giảm dần.
- `tests/architecture_contract_test/` tách thành nhiều `.cpp`; thêm `runtime_core_test` table-driven cho pure core (không cần thread, chạy nhanh).

### C.4 Cái KHÔNG nên làm

- Không viết lại controller một lần; không đổi enum/tên state đang được PLC contract và test tham chiếu.
- Không tạo abstraction "command source" trước khi TCP command thật sự vào (đúng rule của `AGENT.md`) — chỉ cần event trong core nguồn-độc-lập là đủ chuẩn bị.
- Không di chuyển docs history hàng loạt; chỉ chuyển cái đang active.

---

## Phần D — Lộ trình đề xuất (3 bước, mỗi bước có thể dừng)

| Bước | Nội dung | Kết quả kiểm chứng được |
|---|---|---|
| **D0 (1 buổi)** | git init + tag; tạo `runtime_state_contract.md` bằng cách **trích** bảng từ code hiện tại (agent làm, không đổi hành vi); import DR từ D1–D8 | Bảng transition đầy đủ cho 6 state hiện có; mọi ô UNSPECIFIED được liệt kê thành policy question list |
| **D1 — Pilot "startup arming"** | Chạy vòng lặp B.1 cho cụm: item 54 (held trigger), 58B (mapped-but-never-written), thứ tự connect PLC→camera/output, "set fault before recovering", bug "PLC connect muộn thì index không update" | 3–5 DR; slice đầu của pure core; test theo row; owner-run có script; Phase G đóng bằng evidence |
| **D2 — Nguồn lệnh TCP** | Frame câu hỏi ưu tiên/ack/held-trigger cho TCP; thêm event nguồn-độc-lập vào core; adapter TCP | Core không sửa khi thêm adapter; bảng chỉ thêm row Event |
| **D3 — Manual & signal charts (Phase 10)** | Sinh signal charts và phần "signals" của user manual **từ scenario catalogue** | Manual không trở thành bản tường thuật thứ sáu |

Item 64 (Modbus publish va poll) và 70 (vision-output client kẹt Recovering) là bug device/transport, không phải policy của core; xử lí bằng vòng thường (debug → test → fix) nhưng **scenario của chúng vẫn được ghi vào catalogue** để core có row rõ ràng cho "send failed while Running".

---

## Phụ lục

- Phụ lục 1 — template `runtime_state_contract.md`: [templates/runtime_state_contract_template.md](templates/runtime_state_contract_template.md)
- Phụ lục 2 — template Decision Record + Policy Question card: [templates/policy_decision_record_template.md](templates/policy_decision_record_template.md)
- Phụ lục 3 — danh sách policy question hiện đang mở (để bắt đầu D0): [01_open_policy_questions.md](01_open_policy_questions.md)
