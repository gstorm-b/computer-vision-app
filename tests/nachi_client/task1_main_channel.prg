'======================================================================
' TASK1_MAIN_CHANNEL.PRG
'   Kênh chính (port 5000) — yêu cầu matching và nhận kết quả.
'
'   Vai trò:
'     - Đợi Task 2 cho phép (GB_SERVER_ALIVE=1 và GB_TASK1_RESET_REQ=0).
'     - Connect tới VisionOutputDevice port 1.
'     - Khi chương trình motion khác cần kết quả mới: gọi sub
'       SEND_TRIGGER (gửi yêu cầu), rồi RECEIVE_RESULT (đợi 1 packet
'       kết thúc bằng ';').
'     - Parse packet "{N},x,y,z,r,x,y,z,r,...;" vào global:
'         GI_DETECTED_COUNT = N
'         GR_POS_X[i], GR_POS_Y[i], GR_POS_Z[i], GR_POS_R[i] với
'         i = 0..N-1.
'     - Nếu Task 2 báo reset (GB_TASK1_RESET_REQ=1) hoặc xảy ra lỗi
'       socket → đóng socket, về idle, quay lại đợi Task 2.
'
'   Trigger: trong design này client tự chủ động gửi "trigger,1;" mỗi
'   khi cần shot mới. Có thể đổi sang chế độ passive (chỉ chờ server
'   push) bằng cách bỏ SEND_TRIGGER trong vòng main loop.
'======================================================================

PROGRAM TASK1_MAIN_CHANNEL

'--- Constants (mirror common_defs.prg) ---
CONST SERVER_IP$       = "192.168.1.100"
CONST PORT_MAIN        = 5000
CONST SK_MAIN          = 1
CONST MAIN_RECV_LIMIT  = 30000
CONST MAIN_RECV_POLL   = 200
CONST RECONNECT_DELAY  = 1000

CONST GB_SERVER_ALIVE    = 10
CONST GB_TASK1_RESET_REQ = 11
CONST GB_TASK1_CONNECTED = 12
CONST GI_DETECTED_COUNT  = 20
CONST GR_POS_BASE        = 30
CONST MAX_POS            = 8

CONST O_TASK1_BUSY       = 3

CONST RESULT_TERMINATOR$ = ";"
CONST FIELD_SEP$         = ","
CONST TRIGGER_REQ$       = "trigger,1;"

'--- Local vars ---
DEFINT ret
DEFSTR packet$

'======================================================================
' MAIN
'======================================================================
MAIN_LOOP:
    CALL CLEAR_RESULT
    CALL WAIT_FOR_PERMISSION

    CALL CONNECT_MAIN
    IF ret <> 0 THEN
        CALL HANDLE_DISCONNECT("connect_failed")
        GOTO MAIN_LOOP
    ENDIF

    GB[GB_TASK1_CONNECTED] = 1

WORK_LOOP:
    'Kiểm tra Task 2 có yêu cầu reset không.
    IF GB[GB_TASK1_RESET_REQ] = 1 THEN
        CALL HANDLE_DISCONNECT("reset_by_task2")
        GOTO MAIN_LOOP
    ENDIF

    SETO O_TASK1_BUSY, 1

    CALL SEND_TRIGGER
    IF ret <> 0 THEN
        SETO O_TASK1_BUSY, 0
        CALL HANDLE_DISCONNECT("send_failed")
        GOTO MAIN_LOOP
    ENDIF

    CALL RECEIVE_RESULT
    IF ret <> 0 THEN
        SETO O_TASK1_BUSY, 0
        CALL HANDLE_DISCONNECT("recv_failed")
        GOTO MAIN_LOOP
    ENDIF

    CALL PARSE_RESULT
    SETO O_TASK1_BUSY, 0

    'Sau khi parse xong, các task khác có thể đọc GI_DETECTED_COUNT
    'và GR_POS_*. Ở đây ta giả lập 1 chu kỳ rồi quay lại lấy shot mới.
    DELAY MAIN_RECV_POLL
    GOTO WORK_LOOP

END

'======================================================================
' SUB: WAIT_FOR_PERMISSION
'   Block cho đến khi Task 2 báo link OK và không có yêu cầu reset.
'======================================================================
SUB WAIT_FOR_PERMISSION
PERM_WAIT:
    IF GB[GB_SERVER_ALIVE] = 1 AND GB[GB_TASK1_RESET_REQ] = 0 THEN
        EXITSUB
    ENDIF
    DELAY MAIN_RECV_POLL
    GOTO PERM_WAIT
ENDSUB

'======================================================================
' SUB: CONNECT_MAIN
'======================================================================
SUB CONNECT_MAIN
    ret = TCP_OPEN(SK_MAIN, "TCP")
    IF ret <> 0 THEN EXITSUB
    ret = TCP_CONNECT(SK_MAIN, SERVER_IP$, PORT_MAIN)
ENDSUB

'======================================================================
' SUB: CLOSE_MAIN
'======================================================================
SUB CLOSE_MAIN
    TCP_CLOSE(SK_MAIN)
    GB[GB_TASK1_CONNECTED] = 0
ENDSUB

'======================================================================
' SUB: SEND_TRIGGER
'   Gửi yêu cầu matching xuống server. ret=0 nếu OK.
'======================================================================
SUB SEND_TRIGGER
    ret = TCP_SEND(SK_MAIN, TRIGGER_REQ$)
    IF ret < 0 THEN
        ret = 1
    ELSE
        ret = 0
    ENDIF
ENDSUB

'======================================================================
' SUB: RECEIVE_RESULT
'   Đọc từ socket cho đến khi gặp ký tự ';' hoặc timeout.
'   Kết quả nguyên packet (kể cả ';') ghi vào packet$.
'======================================================================
SUB RECEIVE_RESULT
    DEFSTR chunk$
    DEFINT elapsed
    DEFINT n

    packet$ = ""
    elapsed = 0

RECV_LOOP:
    chunk$ = ""
    n = TCP_RECV(SK_MAIN, chunk$, 256, MAIN_RECV_POLL)
    IF n > 0 THEN
        packet$ = packet$ + chunk$
        IF INSTR(packet$, RESULT_TERMINATOR$) > 0 THEN
            ret = 0
            EXITSUB
        ENDIF
    ELSEIF n < 0 THEN
        'Lỗi socket
        ret = 1
        EXITSUB
    ENDIF

    elapsed = elapsed + MAIN_RECV_POLL
    IF elapsed >= MAIN_RECV_LIMIT THEN
        ret = 2     ' timeout
        EXITSUB
    ENDIF

    'Trong khi chờ vẫn phải tôn trọng lệnh reset từ Task 2.
    IF GB[GB_TASK1_RESET_REQ] = 1 THEN
        ret = 3
        EXITSUB
    ENDIF

    GOTO RECV_LOOP
ENDSUB

'======================================================================
' SUB: PARSE_RESULT
'   Bóc packet$ "{N},x,y,z,r,x,y,z,r,...;" vào các global.
'   Mỗi trục là fixed-width "%08.2f" (vd "00001.00"); VAL() tự bỏ
'   zero-pad nên không cần xử lý thêm.
'   Mọi sai format sẽ set GI_DETECTED_COUNT=0 và không đụng GR_POS_*.
'======================================================================
SUB PARSE_RESULT
    DEFSTR body$
    DEFINT term_pos
    DEFINT i
    DEFINT field_idx
    DEFINT pos_count

    'Bỏ ký tự ';' cuối.
    term_pos = INSTR(packet$, RESULT_TERMINATOR$)
    IF term_pos <= 1 THEN
        GI[GI_DETECTED_COUNT] = 0
        EXITSUB
    ENDIF
    body$ = LEFT$(packet$, term_pos - 1)

    'Field đầu tiên là detected count.
    CALL POP_FIELD(body$)
    pos_count = VAL(field$)
    IF pos_count < 0 THEN pos_count = 0
    IF pos_count > MAX_POS THEN pos_count = MAX_POS
    GI[GI_DETECTED_COUNT] = pos_count

    'Mỗi position 4 field: x, y, z, r.
    FOR i = 0 TO pos_count - 1
        CALL POP_FIELD(body$)
        GR[GR_POS_BASE + i * 4 + 0] = VAL(field$)
        CALL POP_FIELD(body$)
        GR[GR_POS_BASE + i * 4 + 1] = VAL(field$)
        CALL POP_FIELD(body$)
        GR[GR_POS_BASE + i * 4 + 2] = VAL(field$)
        CALL POP_FIELD(body$)
        GR[GR_POS_BASE + i * 4 + 3] = VAL(field$)
    NEXT i
ENDSUB

'======================================================================
' SUB: POP_FIELD
'   Tách field đầu tiên của body$ (ngăn cách bằng ',') ra biến field$
'   và rút gọn body$ phần còn lại.
'======================================================================
DEFSTR field$
SUB POP_FIELD(buf$)
    DEFINT sep
    sep = INSTR(buf$, FIELD_SEP$)
    IF sep = 0 THEN
        field$ = buf$
        buf$ = ""
    ELSE
        field$ = LEFT$(buf$, sep - 1)
        buf$ = MID$(buf$, sep + 1)
    ENDIF
ENDSUB

'======================================================================
' SUB: CLEAR_RESULT
'   Xoá kết quả cũ trước khi vào chu kỳ mới.
'======================================================================
SUB CLEAR_RESULT
    DEFINT i
    GI[GI_DETECTED_COUNT] = 0
    FOR i = 0 TO MAX_POS * 4 - 1
        GR[GR_POS_BASE + i] = 0.0
    NEXT i
    SETO O_TASK1_BUSY, 0
ENDSUB

'======================================================================
' SUB: HANDLE_DISCONNECT
'   Đóng socket, xoá kết quả, đợi reconnect.
'======================================================================
SUB HANDLE_DISCONNECT(reason$)
    PRINT "TASK1 disconnect: ", reason$
    CALL CLOSE_MAIN
    CALL CLEAR_RESULT
    DELAY RECONNECT_DELAY
ENDSUB
