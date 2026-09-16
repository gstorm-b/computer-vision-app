# Danh sách policy question đang mở — seed cho bước D0

**Ngày:** 2026-09-16. Trích từ backlog (`later_todo_list.md`), plan Phase 9, `phase_9_request.md`,
`phase_10_request.md`, `cratch_notes.md`, và khảo sát source. Mỗi mục sẽ trở thành một PQ card
(template ở `templates/policy_decision_record_template.md`; bản sao đã commit ở `docs/decisions/README.md` § Template) khi được đưa vào vòng lặp.

Cột **Hôm nay** là hành vi đã xác minh trong code/docs, không phải suy đoán. Cột **Vì sao phải quyết**
nói ai bị ảnh hưởng nếu không quyết. Thứ tự đề xuất ở cuối.

## Cụm A — Startup / arming (đề xuất làm pilot)

| PQ | Câu hỏi | Nguồn | Hôm nay | Vì sao phải quyết |
|---|---|---|---|---|
| PQ-1 | Trigger đang giữ mức cao khi runtime start: chạy cycle, bỏ qua im lặng, hay giữ not-ready cho tới khi thấy mức thấp? | item 54; Phase 9 G1/G2 (owner-gated); `phase_9_request.md` | MC/Modbus suppress first poll → không có edge → task **Ready mà không nói gì**. Snapshot từ C6 đã cho phép đọc mức thật nhưng chưa dùng. `m_lastErrorReset` không reset trong `setup()` (bất đối xứng với trigger). | `plc_signal_contract.md` đang ghi "Not yet specified"; PLC program không thể viết chắc. Phase G đã có thiết kế (option "not-ready until seen low"), chỉ chờ ruling. |
| PQ-2 | PLC connect **muộn** (setup đã fallback về project default vì không có snapshot trong 2 s), khi PLC lên thì có đọc lại selection không? | `cratch_notes.md` "Bug found: … camera number và index number will not update" | `setup()` đọc snapshot một lần; sau đó chỉ **thay đổi** mới tới setter. Register giữ giá trị từ trước khi kết nối không bao giờ được deliver. | Cell khởi động mà PLC lên sau vision (thứ tự boot thực tế) sẽ chạy sai camera/group một cách im lặng — đúng lớp defect item 58. |
| PQ-3 | Register **được map nhưng chưa bao giờ được ghi** (0 từ power-up) có phải là commissioning fault riêng, hay gộp với "unmapped"? | item 58 part B | Unmapped và không-đọc-được đều fallback về project default với USER warn; không tách khỏi "master thật sự ghi 0" (case này đã fault đúng). | Ảnh hưởng thông điệp cho người commissioning; ảnh hưởng PQ-2. |
| PQ-4 | Thứ tự connect: chặn camera **và output** cho tới khi PLC connected? | `cratch_notes.md` "Chặn luôn connect camera và output device trước khi connect thành công với plc" | C6: `beginRuntime()` connect PLC + output trước, camera sau. Output vẫn song song với PLC. | Nếu selection phụ thuộc PLC (PQ-2) thì output cũng có thể phụ thuộc (dual-role Modbus). Cần nói rõ "required-before" graph giữa các role. |

## Cụm B — Fault vs Recovering

| PQ | Câu hỏi | Nguồn | Hôm nay | Vì sao phải quyết |
|---|---|---|---|---|
| PQ-5 | Mất role **ngoài cycle** có publish `bTaskFault` (mã mới) trước khi vào Recovering không? | `cratch_notes.md` "Nên set fault trước khi về recovering" | Phase B quyết định có chủ đích: dead link **không bao giờ** raise `bTaskFault`; PLC phải watch `bTaskReady`. Docs cảnh báo ⚠️ rõ. | Ghi chú của owner mâu thuẫn với contract đang có → phải hoặc đảo DR, hoặc xác nhận giữ. Đây là điểm PLC program phụ thuộc trực tiếp. |
| PQ-6 | `Recovering` đang mang **hai nghĩa** (role outage; cycle fault chờ auto-clear). Tách state hay giữ? | khảo sát source (`abortCycle()`, `handleRoleStatusChanged()`) | Một enum value, hai đường vào, hai bộ output khác nhau (`RECOVERING` vs `CYCLE_FAULT`). Dashboard hiển thị cùng một lamp. | Ảnh hưởng bảng transition (số row), dashboard, và user manual Phase 10. Quyết trước khi trích bảng ở D0 sẽ rẻ hơn. |
| PQ-7 | Khi role `LostConnected`, có tear-down transport hoàn toàn trước khi redial không? | `cratch_notes.md` "Recovering không disconnect hoàn toàn output device"; item 70 | Reconnect là `deviceConnect()` idempotent → với vision-output client trả "already active" → **no-op**; task kẹt Recovering. | Một phần là device bug (item 70), một phần là policy: "reconnect" nghĩa là gì ở mức core. Cần row rõ cho `RoleStatus(LostConnected)` → action. |

## Cụm C — Output của cycle

| PQ | Câu hỏi | Nguồn | Hôm nay | Vì sao phải quyết |
|---|---|---|---|---|
| PQ-8 | Cap **2 vị trí / cycle** là cố ý? Nếu có: hằng số hay setting; lí do trong row "Skipped". | item 68 | Literal `2` trong `buildVisionOutputPositions()`; vị trí dư bị `Skipped` **không lí do**; không doc nào nêu. | Operator thấy part thứ ba bị bỏ mà không biết vì sao; robot program có thể đang giả định 2. |
| PQ-9 | "No match" là **cycle thành công với 0** (không fault) — xác nhận và ghi DR; `bMatchingLowArea` có ý nghĩa gì với PLC. | khảo sát source | Đúng như vậy hôm nay; không có fault code cho no-match; không có DR. | Cần ghi thành quyết định để user manual và PLC program cùng hiểu. Rẻ. |
| PQ-10 | Result publish **va chạm với poll** trên dual-role Modbus: retry publish, ưu tiên publish trước poll, hay coi là retryable ở controller? | item 64 (field, ~2.4 % cycle) | Abort cycle `201`, cần `bErrorReset`. Ba option đã liệt kê trong backlog, "needs a decision, not a quick patch". | Đang xảy ra trên cell thật. Ràng buộc: send phải resolve đúng một lần; không được báo "sent" khi chưa tới register. |
| PQ-11 | `onVisionOutputResultFinished()` gán thẳng `ReadyForTrigger`, bỏ qua gate `markRuntimeReady()` — cố ý (fast path) hay lỗ? | khảo sát source | Là đường duy nhất vào `ReadyForTrigger` không qua gate; `markRuntimeReady()` được gọi ngay sau nên hôm nay vô hại. | Khi đưa `transitionTo()` + guard table vào, row này phải có câu trả lời. Rẻ. |

## Cụm D — Nguồn lệnh (trước khi làm TCP command)

| PQ | Câu hỏi | Nguồn | Hôm nay | Vì sao phải quyết |
|---|---|---|---|---|
| PQ-12 | TCP command (`Trigger,`/`ChangePart,`/`ChangeCamera,`): cùng lúc với PLC hay loại trừ per task? Ưu tiên khi xung đột? Ack qua kênh nào? Held-trigger nghĩa là gì trên TCP? | `cratch_notes.md` "TCP/IP msg control" | Không có. Event trong controller gắn với PLC tag. | Đây là thay đổi hình dạng state machine lớn nhất sắp tới; quyết trước thì core chỉ thêm row Event, quyết sau thì phải sửa `handlePlcValues()`. |
| PQ-13 | Entry point chết (`execute()`/`executeLocalization()`, `queueSetActiveCameraNumber()`, `queueSetActivePatternGroupNumber()`, `onCommDeviceValueChanged()`): xoá, hay giữ làm đường cho UI/TCP? | khảo sát source; Phase 9 C5 đã xoá 4 cái khác | Không caller/connect production. | Bề mặt API rộng hơn hành vi; agent sau dễ nhầm. Nếu PQ-12 cần "manual trigger" thì giữ và wire; nếu không thì xoá. |

## Cụm E — Cấu hình / mã lỗi

| PQ | Câu hỏi | Nguồn | Hôm nay | Vì sao phải quyết |
|---|---|---|---|---|
| PQ-14 | Recovery policy: giữ default cứng, hay per-task setting có persist? | item 67; D5 (defer) | `setRecoveryPolicies()` chỉ test gọi; ship trên default 5000 ms / unlimited. | Không gấp; ghi DR "defaults only until a cell asks" là đủ. |
| PQ-15 | Mã `400` gánh 4 lỗi content-invalid khác nhau: tách mã? | item 63 | Một mã, bốn nguyên nhân, PLC không phân biệt được. | Mã lỗi là contract ổn định; thêm mã mới rẻ, đổi mã cũ đắt. Quyết trước manual Phase 10. |
| PQ-16 | Robot pick check có **hai editor** (task settings và từng vision-output device): giữ cả hai, hay bỏ editor phía device (deprecation + migration)? | item 66; Phase 9 open question O-2 | Task đọc `TaskLocalizeConfig::robotCheckConfig()`; editor phía device chỉ còn điều khiển advisory check của transport. Hai nơi có thể lệch nhau im lặng. | Là câu hỏi ownership cấu hình, không phải state của core, nhưng manual Phase 10 phải mô tả đúng một nơi. Thêm 2026-09-16 theo triage WP-03. |

## Thứ tự đề xuất

1. **Cụm A** (PQ-1..4) — pilot, một vòng lặp đầy đủ; Phase G đã có nền; PQ-2 là bug thật đang mở.
2. **PQ-9, PQ-11, PQ-8** — rẻ, chủ yếu là ghi DR + hằng số; làm ngay trong D0 để bảng contract không còn ô "no DR".
3. **PQ-6** — quyết trước khi trích bảng (ảnh hưởng số state).
4. **Cụm D** — trước khi bắt đầu TCP command.
5. **PQ-5, PQ-7, PQ-10** — cùng lúc với điều tra item 70 và 64 (có phần device bug).
6. **Cụm E** — trước manual Phase 10.

Item không phải policy của core, xử lí bằng vòng debug thường nhưng scenario vẫn vào catalogue:
item 64 (transport), item 70 (client redial), item 62 (build), item 65 (qmake).
