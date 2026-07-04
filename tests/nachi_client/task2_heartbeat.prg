'======================================================================
' TASK2_HEARTBEAT.PRG
'   Kênh heartbeat (port 5001) — đồng thời là supervisor cho Task 1.
'
'   Vai trò:
'     - Connect tới VisionOutputDevice port 2.
'     - Nhận probe "connection_check." từ server, reply
'       "ack,{msg_count}." với msg_count tăng dần và wrap về 0 khi
'       vượt 2^16.
'     - Khi link healthy: set GB_SERVER_ALIVE=1 → Task 1 mới được phép
'       connect.
'     - Khi mất kết nối (recv timeout / lỗi format / lỗi socket):
'         * Set GB_TASK1_RESET_REQ=1 để Task 1 đóng socket và idle.
'         * Bật IO O_VISION_LINK_ERR, tắt O_VISION_LINK_OK.
'         * Đóng socket, delay rồi connect lại.
'     - Khi server báo ngắt chủ động (nhận "disconnect."):
'         * Stand down Task 1 sạch sẽ, KHÔNG bật O_VISION_LINK_ERR
'           (đây là ngắt có kế hoạch, không phải lỗi).
'         * Đóng socket, delay rồi connect lại.
'======================================================================

PROGRAM TASK2_HEARTBEAT

'--- Constants (mirror common_defs.prg) ---
CONST SERVER_IP$       = "192.168.1.100"
CONST PORT_HB          = 5001
CONST SK_HB            = 2
CONST HB_RECV_TIMEOUT  = 3000
CONST RECONNECT_DELAY  = 1000
CONST PROTOCOL_LIMIT   = 65536

CONST GB_SERVER_ALIVE    = 10
CONST GB_TASK1_RESET_REQ = 11
CONST GI_HB_MSG_COUNT    = 10

CONST O_VISION_LINK_OK   = 1
CONST O_VISION_LINK_ERR  = 2

CONST HB_PROBE$        = "connection_check."
CONST HB_ACK_PREFIX$   = "ack,"
CONST HB_DISCONNECT$   = "disconnect."

'--- Local vars ---
DEFINT ret
DEFINT count
DEFSTR rx$

'======================================================================
' MAIN
'======================================================================
MAIN_LOOP:
    CALL INIT_STATE
    CALL CONNECT_HB
    IF ret <> 0 THEN
        CALL DECLARE_LOST("connect_failed")
        CALL WAIT_RECONNECT
        GOTO MAIN_LOOP
    ENDIF

    'Link đã OK → cho Task 1 được phép connect.
    CALL GRANT_TASK1

HB_LOOP:
    CALL RECV_PROBE
    IF ret <> 0 THEN
        CALL DECLARE_LOST("recv_timeout")
        CALL CLOSE_HB
        CALL WAIT_RECONNECT
        GOTO MAIN_LOOP
    ENDIF

    'Planned shutdown announced by the server: stand down cleanly, no alarm.
    IF rx$ = HB_DISCONNECT$ THEN
        CALL DECLARE_SERVER_DISCONNECT
        CALL CLOSE_HB
        CALL WAIT_RECONNECT
        GOTO MAIN_LOOP
    ENDIF

    CALL VALIDATE_PROBE
    IF ret <> 0 THEN
        CALL DECLARE_LOST("invalid_probe")
        CALL CLOSE_HB
        CALL WAIT_RECONNECT
        GOTO MAIN_LOOP
    ENDIF

    CALL SEND_ACK
    IF ret <> 0 THEN
        CALL DECLARE_LOST("send_failed")
        CALL CLOSE_HB
        CALL WAIT_RECONNECT
        GOTO MAIN_LOOP
    ENDIF

    CALL INC_MSG_COUNT
    GOTO HB_LOOP

END

'======================================================================
' SUB: INIT_STATE
'   Reset cờ và counter về trạng thái sạch trước khi kết nối.
'======================================================================
SUB INIT_STATE
    GB[GB_SERVER_ALIVE]    = 0
    GB[GB_TASK1_RESET_REQ] = 1     ' đảm bảo Task 1 idle khi chưa có link
    GI[GI_HB_MSG_COUNT]    = 0
    SETO O_VISION_LINK_OK,  0
    SETO O_VISION_LINK_ERR, 0
ENDSUB

'======================================================================
' SUB: CONNECT_HB
'   Mở TCP client socket tới port heartbeat. ret=0 nếu OK.
'======================================================================
SUB CONNECT_HB
    ret = TCP_OPEN(SK_HB, "TCP")
    IF ret <> 0 THEN EXITSUB
    ret = TCP_CONNECT(SK_HB, SERVER_IP$, PORT_HB)
ENDSUB

'======================================================================
' SUB: CLOSE_HB
'======================================================================
SUB CLOSE_HB
    TCP_CLOSE(SK_HB)
ENDSUB

'======================================================================
' SUB: RECV_PROBE
'   Đợi nhận probe từ server, timeout HB_RECV_TIMEOUT ms.
'   Kết quả ghi vào rx$, ret=0 nếu nhận được, <>0 nếu timeout/error.
'======================================================================
SUB RECV_PROBE
    rx$ = ""
    ret = TCP_RECV(SK_HB, rx$, LEN(HB_PROBE$), HB_RECV_TIMEOUT)
    IF ret < 0 THEN
        ret = 1
    ELSE
        ret = 0
    ENDIF
ENDSUB

'======================================================================
' SUB: VALIDATE_PROBE
'   Kiểm tra rx$ có đúng là "connection_check." không. ret=0 nếu OK.
'======================================================================
SUB VALIDATE_PROBE
    IF rx$ = HB_PROBE$ THEN
        ret = 0
    ELSE
        ret = 1
    ENDIF
ENDSUB

'======================================================================
' SUB: SEND_ACK
'   Gửi "ack,{count}." với count = GI_HB_MSG_COUNT hiện tại.
'======================================================================
SUB SEND_ACK
    DEFSTR payload$
    payload$ = HB_ACK_PREFIX$ + STR$(GI[GI_HB_MSG_COUNT]) + "."
    ret = TCP_SEND(SK_HB, payload$)
    IF ret < 0 THEN
        ret = 1
    ELSE
        ret = 0
    ENDIF
ENDSUB

'======================================================================
' SUB: INC_MSG_COUNT
'   Tăng counter, wrap về 0 khi >= 2^16.
'======================================================================
SUB INC_MSG_COUNT
    count = GI[GI_HB_MSG_COUNT] + 1
    IF count >= PROTOCOL_LIMIT THEN count = 0
    GI[GI_HB_MSG_COUNT] = count
ENDSUB

'======================================================================
' SUB: GRANT_TASK1
'   Cho phép Task 1 connect: set server_alive, clear reset_req.
'   Bật IO báo link OK.
'======================================================================
SUB GRANT_TASK1
    GB[GB_SERVER_ALIVE]    = 1
    GB[GB_TASK1_RESET_REQ] = 0
    SETO O_VISION_LINK_OK,  1
    SETO O_VISION_LINK_ERR, 0
ENDSUB

'======================================================================
' SUB: DECLARE_LOST
'   Báo lost connect: set reset_req cho Task 1, bật IO error.
'   Tham số reason$ chỉ để ghi log/teach pendant.
'======================================================================
SUB DECLARE_LOST(reason$)
    GB[GB_SERVER_ALIVE]    = 0
    GB[GB_TASK1_RESET_REQ] = 1
    SETO O_VISION_LINK_OK,  0
    SETO O_VISION_LINK_ERR, 1
    PRINT "TASK2 lost connect: ", reason$
ENDSUB

'======================================================================
' SUB: DECLARE_SERVER_DISCONNECT
'   Server announced a planned disconnect ("disconnect."). Stand Task 1
'   down cleanly and wait for the server to come back. This is expected,
'   so the error IO (O_VISION_LINK_ERR) stays OFF — only DECLARE_LOST
'   raises the fault output.
'======================================================================
SUB DECLARE_SERVER_DISCONNECT
    GB[GB_SERVER_ALIVE]    = 0
    GB[GB_TASK1_RESET_REQ] = 1
    SETO O_VISION_LINK_OK,  0
    SETO O_VISION_LINK_ERR, 0
    PRINT "TASK2 server disconnect (planned)"
ENDSUB

'======================================================================
' SUB: WAIT_RECONNECT
'======================================================================
SUB WAIT_RECONNECT
    DELAY RECONNECT_DELAY
ENDSUB
