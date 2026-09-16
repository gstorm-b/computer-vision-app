Pattern matching:
Overlap filter problems
Preprocess, area filter promblems
=> actually the problem because of crop offset, considering to add more parameters into group pattern config and pattern config
=> considering to change matching mechanism, search in large imgae if there is no match crop image

TCP/IP msg control
Msg interface: send command and recv respond.
<msg name>,<param 1>,<param 2>,<param N>,;
Example:
Trigger,<task number (just to confirm)>,;
ChangePart,<index pattern>,;
ChangeCamera,<camera number>,;

Camera number và pattern number không tìm thấy phải báo failled, hiện tại đổi part và camera number sang một index chư được đăng kí task vẫn ready, camera valid và pattern valid.
Về modbus client, trong runtime không thể ghi coil value hãy check lại log.
Hãy đồng bộ tên gọi trên UI và tên gọi của các địa chỉ trong các vùng memory của modbus.

Đã run cycle với modbus device ở cả 2 role, xác nhận object ngoài condition ROI skipped.
Ở modbus server camera đổi number out range, camera lost connect nhưng chưa update vào UI.
Ở modbus client, đã có connect với modbus server nhưng chưa udpate PLC và output device vào UI. 
Ở modbus client khi thay đổi camera number hoặc pattern number thì controller đêu ghi ngược lại giá trị vào server (fail vì client không thể ghi vào input register) hãy xem lại log trong build/bin.


Chặn luôn connect camera và output device trước khi connect thành công với plc.
Mặc dù camera index = 0 nhưng vẫn có connect với camera dẫn tới báo không đúng mã lỗi, lẽ ra phải là camera invalid thay vì camera lost.

Nên set fault trước khi về recovering
Recovering không disconnect hoàn toàn output device

Comms device,


Laser marker add place holder registration table
Make an flexible bridge control with plc

Tomorrow:
Ncrn continue task F, and a new task, change policy of vision output device server, only at connected status when have a client connected, add new method for modbus server and vision output device to accept only one client, because with tasklocalization one task only need one client to connect
Make laser marker software more flexible.

**Bug found: currently in setup state, check for camera number và index number but, if plc cannot connect and connected at the attemp, the camera number và index number will not update.

Còn lại — owner-run, chưa cái nào chạy
F1: commission pick check từ task settings trên cell Modbus dual-role, xác nhận pose không với tới bị loại. Item 57 vẫn mở.
F2: đổi binding PLC/vision-output khi dashboard đang mở, không đóng cửa sổ — label và lamp phải theo. Đây là phép thử duy nhất phân biệt được hai nguyên nhân.
F3: bắt 20 cycle và ghi phân bố vào plan. Con số đó mới là deliverable, không phải test.
Bug vision-output recovery vẫn dừng nguyên chỗ cũ, chưa đụng gì.

Tôi đã ghi F2 và F1, nhưng không lấy được số liệu F3 từ app log, và đó là lỗi của tôi, không phải lỗi lúc bạn chạy.

F3 — dòng breakdown chưa bao giờ được ghi xuống file
Dòng cycle=… (grab …, match …, send …) đi qua appendTaskLog(). Nơi duy nhất nhận nó là panel task log trên dashboard, nên nó không vào app_log. Trong log ngày 09-10 và 09-11 có 0 dòng cycle=. Tôi đã tự phát hiện điều này lúc điều tra vision-output mà vẫn không áp dụng khi thiết kế F3. Test không bắt được vì test đọc signal, không đọc file.

Tôi dựng lại số liệu từ timestamp của các dòng camera và dòng chuyển trạng thái trong log. Có 67 cycle, ghi dạng min / median / max (ms):

Phiên (09-11)	n	Grab	Grab xong → trigger reset	Tổng (cận trên)
16:52:27–16:52:37	3	199 / 199 / 232	306 / 308 / 372	505 / 507 / 604
16:52:48–16:57:21	64	191 / 204 / 218	805 / 1013 / 1099	1002 / 1217 / 1304
Bảng này có bốn giới hạn:

Cột tổng không phải cycle time. Cả 67 cycle đều kết thúc bằng "Trigger reset", nên nó gồm cả thời gian PLC phản ứng và hạ bExecuteTrigger, cộng thêm tối đa một chu kỳ poll MC 50 ms.
Cột giữa không tách được. Match, send, publish và thời gian chờ PLC nằm chung trong đó, mà tách chúng ra chính là mục đích của F3. Chỉ riêng grab là đo được sạch: 191–218 ms.
Hai phiên là hai cấu hình khác nhau. Mức nhảy từ ~308 lên ~1013 ms trùng với lần restart runtime lúc 16:52:42. Có một cài đặt nào đó đã đổi giữa hai lần, log không cho biết là cài đặt gì. Bạn có đổi gì lúc đó không, ví dụ bật pick check để test F1? Trong phiên 2, cột giữa còn chia thành hai cụm: 17 cycle ở 800–899 ms và 39 cycle ở 1000–1099 ms.
Timestamp lấy theo giờ hệ thống, chỉ đủ dùng để dựng lại, không đạt chuẩn đo của F3.
Vì vậy tiêu chí xem xét lại threading vẫn chưa đánh giá được. Cách sửa chỉ khoảng một dòng: ghi thêm chính dòng tóm tắt đó xuống app log bằng LOG_USER_INFO, rồi bạn chạy lại 20 cycle. Tôi chưa sửa, chờ bạn đồng ý.

F1 — đã xác nhận, nhưng không phải trên binding mà item 57 yêu cầu
Mọi lần chạy runtime sau bản build F1 đều dùng MC (PLC_Mitsu_01) + VisionOut_01. Thiết bị Modbus chỉ xuất hiện lúc 09-10 08:14 (trước bản build F1) và 09-11 16:59 (sau cycle cuối), và không có runtime nào chạy trên chúng.

Đã chứng minh: check đi từ task settings tới checker trên phần cứng thật và loại đúng các pose không với tới.
Chưa quan sát: đúng trường hợp của item 57, tức một PLC mang role vision_output.
Rủi ro còn lại nhỏ: buildRuntimeContext() không còn hỏi device nào nữa, nên loại device được bind không thể thay đổi giá trị checker nhận được. Trường hợp PLC mang output đã có test tự động riêng.
Tôi để item 57 mở. Bạn quyết định: chạy thêm một lần trên Modbus dual-role, hay đóng item dựa trên rủi ro còn lại này.

F2 — đóng một phần
Đổi binding thì dashboard cập nhật, đây đúng là phép thử phân biệt hai nguyên nhân, nên backlog 61 đã đóng. Phần "rút cáp thì đúng lamp chuyển đỏ rồi xanh lại" bạn chưa báo kết quả, nên item 25 còn mở đúng một dòng đó.

Đã ghi vào Checkpoint F trong phase_9_implementation_plan.md và các item 25, 57, 61 trong later_todo_list.md.

