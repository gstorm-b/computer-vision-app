# Nachi CFDs Client cho Vision Output Device

Bộ 2 user task SLIM2 chạy trên Nachi CFDs (FD series) để kết nối với
`VisionOutputDevice` (TCP server, 2 port).

## Files
| File | Mô tả |
| --- | --- |
| `common_defs.prg`        | Hằng số và global variable dùng chung 2 task. |
| `task1_main_channel.prg` | Task 1 — kênh chính (port 5000): trigger và nhận kết quả matching, parse vào global. |
| `task2_heartbeat.prg`    | Task 2 — kênh heartbeat (port 5001): nhận `connection_check.`, trả `ack,{count}.`, supervisor cho Task 1. Nhận `disconnect.` → ngắt sạch (không báo lỗi). |

## Mapping IO và global

### IO output (báo trạng thái ra ngoài)
| Tên | Bit | Ý nghĩa |
| --- | --- | --- |
| `O_VISION_LINK_OK`  | `O[1]` | =1 khi heartbeat khoẻ và Task 1 đã kết nối. |
| `O_VISION_LINK_ERR` | `O[2]` | =1 khi mất kết nối (timeout / sai format / lỗi socket). |
| `O_TASK1_BUSY`      | `O[3]` | =1 khi Task 1 đang chờ / xử lý 1 package. |

### Global (chia sẻ giữa 2 task)
| Tên | Slot | Loại | Owner | Mô tả |
| --- | --- | --- | --- | --- |
| `GB_SERVER_ALIVE`      | `GB[10]` | bool | Task 2 | =1 khi heartbeat đang chạy bình thường. |
| `GB_TASK1_RESET_REQ`   | `GB[11]` | bool | Task 2 | =1 → Task 1 phải đóng socket, về trạng thái idle. |
| `GB_TASK1_CONNECTED`   | `GB[12]` | bool | Task 1 | =1 khi Task 1 đã connect port chính. |
| `GI_HB_MSG_COUNT`      | `GI[10]` | int  | Task 2 | Counter `msg_count` (0…65535, wrap về 0). |
| `GI_DETECTED_COUNT`    | `GI[20]` | int  | Task 1 | Số position đã detect ở kết quả mới nhất. |
| `GR_POS_X[i]`          | `GR[30+i*4]` | real | Task 1 | Toạ độ X (i = 0…7). |
| `GR_POS_Y[i]`          | `GR[31+i*4]` | real | Task 1 | Toạ độ Y. |
| `GR_POS_Z[i]`          | `GR[32+i*4]` | real | Task 1 | Toạ độ Z. |
| `GR_POS_R[i]`          | `GR[33+i*4]` | real | Task 1 | Góc R. |

Tối đa 8 position (slot `GR[30..61]`). Nếu cần nhiều hơn chỉnh
`MAX_POS` trong `common_defs.prg`.

## Auto-start

Trên teach pendant của Nachi CFDs:

`SETTING → CONTROLLER → AUTO START PROGRAM`

- Slot Task 1: `TASK1_MAIN_CHANNEL`
- Slot Task 2: `TASK2_HEARTBEAT`

Cả 2 task sẽ tự chạy mỗi khi power-on controller.

## Server endpoint mặc định
- IP: `192.168.1.100`
- Main port: `5000`
- Heartbeat port: `5001`

Đổi trong `common_defs.prg` nếu khác.

## Heartbeat protocol (port 5001)
Server (vision software) là heartbeat master, mọi message kết thúc bằng `.`:

| Hướng | Message | Ý nghĩa |
| --- | --- | --- |
| server → client | `connection_check.` | Probe định kỳ, client phải trả `ack,{count}.`. |
| client → server | `ack,{count}.` | Reply; `count` 0…65535, wrap về 0. |
| server → client | `disconnect.` | Server báo ngắt **chủ động** trước khi đóng link. Client stand down sạch, **không** bật error IO. |

`disconnect.` chỉ gửi khi ngắt có kế hoạch (`deviceDisconnect()`), không bao
giờ gửi trên đường mất kết nối (timeout / sai format).

## Result format (port 5000)
Server trả kết quả matching kết thúc bằng `;`:

```
{detected},{x,y,z,r},{x,y,z,r},...;
```

Mỗi trục là fixed-width `%08.2f` (zero-pad 8 ký tự, 2 số thập phân), ví dụ
`1.0` → `00001.00`. `PARSE_RESULT` dùng `VAL(field$)` nên số zero-pad được
convert đúng (`VAL("00001.00") = 1.0`), không cần đổi logic parse.

## Lưu ý cú pháp SLIM
Các hàm `TCP_OPEN`, `TCP_CONNECT`, `TCP_SEND`, `TCP_RECV`, `TCP_CLOSE`
là API Ethernet option của Nachi CFDs. Trên một số model tên hàm là
`SOCK_OPEN` / `SOCK_CONNECT` v.v. — chỉ cần đổi tên hàm, signature
giống nhau (handle id, ip string, port, buffer, timeout ms).
