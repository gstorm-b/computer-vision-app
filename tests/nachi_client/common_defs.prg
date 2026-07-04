'======================================================================
' COMMON_DEFS.PRG
'   Hằng số và global variable dùng chung giữa Task 1 và Task 2.
'   File này KHÔNG cần load như task, chỉ dùng làm tài liệu tham chiếu.
'   Khi import vào controller, copy các CONST tương ứng vào đầu mỗi
'   task .prg (SLIM không có #include).
'======================================================================

'--- Server endpoint ---
CONST SERVER_IP$       = "192.168.1.100"
CONST PORT_MAIN        = 5000      ' Port 1 - matching channel
CONST PORT_HB          = 5001      ' Port 2 - heartbeat channel

'--- Timing (ms) ---
CONST HB_RECV_TIMEOUT  = 3000      ' >> server interval (1s) để chịu jitter
CONST MAIN_RECV_POLL   = 200       ' chu kỳ poll khi chờ result
CONST RECONNECT_DELAY  = 1000      ' delay giữa các lần thử reconnect
CONST MAIN_RECV_LIMIT  = 30000     ' timeout đợi result cho 1 trigger
CONST PROTOCOL_LIMIT   = 65536     ' 2^16, wrap msg_count

'--- Socket handle (đánh số duy nhất trong toàn controller) ---
CONST SK_MAIN          = 1
CONST SK_HB            = 2

'--- IO output bits ---
CONST O_VISION_LINK_OK  = 1
CONST O_VISION_LINK_ERR = 2
CONST O_TASK1_BUSY      = 3

'--- Global flags / registers ---
'   GB[] = global bool, GI[] = global int, GR[] = global real
CONST GB_SERVER_ALIVE    = 10
CONST GB_TASK1_RESET_REQ = 11
CONST GB_TASK1_CONNECTED = 12

CONST GI_HB_MSG_COUNT    = 10
CONST GI_DETECTED_COUNT  = 20

CONST GR_POS_BASE        = 30     ' GR[30+i*4 .. 33+i*4] = x,y,z,r
CONST MAX_POS            = 8      ' tối đa 8 position / 1 result

'--- Protocol payload ---
CONST HB_PROBE$          = "connection_check."
CONST HB_ACK_PREFIX$     = "ack,"
CONST HB_DISCONNECT$     = "disconnect."   ' server announces a planned close
CONST HB_TERMINATOR$     = "."
CONST RESULT_TERMINATOR$ = ";"
CONST FIELD_SEP$         = ","
