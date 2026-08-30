# Modbus TCP Server Demo (Qt Widgets + qmake)

App demo một **Modbus TCP server** viết bằng Qt Widgets, mở và build trực tiếp trong Qt Creator.

* Đủ **4 area** của Modbus, mỗi area **64 địa chỉ**, chỉnh trực tiếp trên HMI.
* **Chọn kiểu dữ liệu hiển thị ngay trong bảng** của 2 tab register (giống *Batch Monitor* của GX Works2): INT16/32/64, UINT, HEX, **BCD**, **FLOAT32 (REAL)**, FLOAT64, **ASCII** — với đủ 4 thứ tự byte (ABCD / CDAB / BADC / DCBA). Chọn kiểu 32/64 bit thì bảng tự gộp 2/4 register thành một dòng.
* **Data Monitor**: danh sách theo dõi tự chọn từng địa chỉ, đọc/ghi/giám sát theo kiểu dữ liệu (kể cả chuỗi ASCII dài nhiều register).
* Instance Modbus nằm trong **worker thread riêng** (`ModbusRunner`) → xử lý request không bao giờ block UI.
* **Toàn bộ widget khai báo trong `mainwindow.ui`** để bạn tự chỉnh trong Qt Designer.
* **Log sự kiện** đầy đủ: client kết nối / ngắt kết nối, từng request (giải mã đúng theo từng function code), client ghi giá trị nào vào đâu, exception, lỗi server.

---

## 1. Yêu cầu

| Thành phần | Ghi chú |
|---|---|
| **Qt 6** (đã test trên **Qt 6.11.1 MSVC2022 64bit**) | cần module **SerialBus** (`QT += serialbus`) |
| Qt Creator | build bằng **qmake** |
| Compiler | MSVC 2019/2022 hoặc MinGW / GCC / Clang |

> Module SerialBus phải được cài trong Qt Maintenance Tool (mục *Qt Serial Bus*). Nếu thiếu, `.pro` sẽ dừng với thông báo rõ ràng.
>
> Project yêu cầu Qt 6 (Qt 5 không build được: `QModbusDataUnit` của Qt 5 dùng `QVector`, và `QComboBox::currentIndexChanged` của Qt 5 có 2 overload).

---

## 2. Build

**Trong Qt Creator**

1. `File > Open File or Project…` → chọn **`ModbusTcpServer.pro`**
2. Chọn kit desktop (ví dụ *Desktop Qt 6.11.1 MSVC2022 64bit*) → **Configure Project**
3. Nhấn **Ctrl+R** (Run)

**Bằng dòng lệnh**

```bat
mkdir build && cd build
qmake ..\ModbusTcpServer.pro
nmake            :: hoặc jom / mingw32-make
```

> Thư mục `build/` và `.qtcreator/` còn sót lại từ project CMake ban đầu — nên xoá, tránh Qt Creator dùng nhầm thư mục shadow build.

---

## 3. Kiến trúc

```
        GUI thread                                worker thread (QThread)
 ┌──────────────────────────┐               ┌──────────────────────────────────┐
 │ MainWindow               │               │ ModbusRunner                     │
 │  - mainwindow.ui         │  queued       │  - QModbusTcpServer (instance)   │
 │  - 4 QTableWidget        │  signals      │  - shadow copy 4 x 64 giá trị    │
 │  - Data Monitor          │ ◄──────────►  │  - thống kê / đếm request        │
 │  - QPlainTextEdit  (log) │               │  - QModbusTcpConnectionObserver  │
 │  KHÔNG chạm Modbus       │               │                                  │
 └──────────────────────────┘               └──────────────────────────────────┘
```

**Chiều GUI → worker:** `startServer` · `stopServer` · `writeValue` · **`writeBlock`** · `fillArea` · `randomizeArea` · `setAcceptingConnections` · `setLogLevel`

**Chiều worker → GUI:** `logMessage` · `serverStateChanged` · `blockChanged` · `statsChanged`

Hai object nằm ở hai thread khác nhau nên Qt tự dùng `Qt::QueuedConnection` — không cần mutex, và UI không bao giờ bị chặn bởi client Modbus.

### Các file

| File | Vai trò |
|---|---|
| `main.cpp` | đăng ký metatype (`ModbusBlock`, `ModbusStats`) rồi mở cửa sổ |
| `modbusdefs.h` | hằng số dùng chung: **`kAddressCount = 64`**, enum `Area`, `LogLevel`, struct truyền giữa 2 thread |
| `datacodec.h/.cpp` | chuyển đổi register ↔ INT/UINT/HEX/BCD/FLOAT/ASCII theo 4 thứ tự byte |
| `modbusrunner.h/.cpp` | worker: giữ `QModbusTcpServer`, shadow copy, thống kê, log |
| `loggingmodbusserver.h/.cpp` | subclass `QModbusTcpServer`, override `processRequest()` để log mọi request |
| `rowhoverdelegate.h/.cpp` | delegate làm nổi **cả dòng** khi rê chuột, thay vì chỉ một ô |
| `mainwindow.h/.cpp` | HMI: bảng, Data Monitor, log, vòng đời worker thread |
| `mainwindow.ui` | **tất cả widget** — sửa trong Qt Designer |
| `samples/*.py` | ví dụ chuyển đổi giá trị **phía client** bằng Python (xem mục 7) |

---

## 4. Sử dụng

### 4.1 Khởi động server

Hàng trên cùng có 3 khối cạnh nhau: **Modbus TCP server**, **Status** và **Event log**.

1. **Listen address** — `0.0.0.0` (mọi card mạng) hoặc `127.0.0.1`. Log lúc khởi động in sẵn các IPv4 của máy.
2. **TCP port** — mặc định `502`. Nếu bị chiếm, đổi sang `5020`/`5502`.
3. **Unit ID** — mặc định `1`. ⚠️ Xem mục *Lưu ý* bên dưới.
4. **Start server** → trạng thái chuyển `Listening`.
5. Bỏ tick **Accept new clients** để từ chối client mới (client đang kết nối vẫn giữ nguyên).

### 4.2 Bảng dữ liệu của 4 area

| Tab | Register type | Client đọc | Client ghi |
|---|---|---|---|
| Coils (0x) | `Coils` | FC01 | FC05, FC15 |
| Discrete Inputs (1x) | `DiscreteInputs` | FC02 | — (chỉ sửa trên HMI) |
| Input Registers (3x) | `InputRegisters` | FC04 | — (chỉ sửa trên HMI) |
| Holding Registers (4x) | `HoldingRegisters` | FC03 | FC06, FC16 |

**Coils / Discrete Inputs** — cột *State* là checkbox ON/OFF, cột *Value* hiện `1`/`0`. Hai tab này không có bộ chọn định dạng vì bit chỉ có một cách hiển thị.

**Input / Holding Registers** — có combo **Display** ngay trên bảng, chọn kiểu dữ liệu sẽ dùng để **hiển thị và nhập** trong cột *Value*:

| Kiểu | Register / dòng | Số dòng (với 64 địa chỉ) |
|---|---|---|
| INT16 / UINT16 / HEX16 / BCD16 / ASCII | 1 | 64 |
| INT32 / UINT32 / HEX32 / BCD32 / FLOAT32 | 2 | 32 |
| INT64 / UINT64 / FLOAT64 | 4 | 16 |

* Chọn kiểu 32/64 bit thì bảng **gộp register thành một dòng**, cột *Address* và *PLC ref* hiện luôn dải (ví dụ `2 - 3`) — đúng cách Batch Monitor của GX Works2 gộp `D0, D2, D4…`.
* Combo **byte order** bên cạnh chỉ bật khi kiểu chiếm nhiều hơn 1 register.
* Cột **Raw** luôn hiện các register thô dạng hex (`0000 3FC0`), nên bạn vẫn thấy dữ liệu gốc dù đang xem ở kiểu nào.
* Gõ vào cột *Value* là ghi xuống: giá trị được mã hoá theo kiểu + byte order đang chọn rồi ghi cả nhóm register. Nhập sai định dạng thì ô tự trả về giá trị cũ và log ghi rõ lý do.
* ASCII trong bảng là **2 ký tự / register** (1 register mỗi dòng), giống batch monitor. Muốn chuỗi dài nhiều register thì dùng tab **Data Monitor**.

**Chung cho cả 4 tab**

* **Set all / Clear all / Randomize** thao tác cả 64 địa chỉ một lần (ô *Raw value* là giá trị register thô).
* **Cột PLC ref**: mỗi tab có ô tick **PLC ref from** để bật/tắt cột, kèm spin box chọn **địa chỉ bắt đầu**. Mặc định theo quy ước cũ (1 / 10001 / 30001 / 40001) nhưng bạn đổi tuỳ ý — ví dụ đặt `0` nếu PLC của bạn đánh số từ 0, hoặc đặt theo dải `D` của Mitsubishi.
* Bảng chọn theo **cả dòng**, và rê chuột cũng làm nổi **cả dòng** (không chỉ một ô).

### 4.3 Tab Data Monitor — kiểu dữ liệu

Bảng 4 area hiển thị *toàn bộ* area theo một kiểu duy nhất. Tab này ngược lại: bạn tự chọn từng địa chỉ muốn theo dõi, mỗi dòng một kiểu riêng — và đây là nơi duy nhất nhập được **chuỗi ASCII dài nhiều register**.

1. Điền **Name** (tuỳ chọn), chọn **Area** (Input hoặc Holding Registers), **Start address**, **Data type**, **Byte order**. Với **ASCII text** thì chọn thêm số **Registers** (2 ký tự / register).
2. Bấm **Add value** → dòng mới xuất hiện, cột **Current value** cập nhật **realtime** mỗi khi giá trị thay đổi (do bạn sửa hay do client ghi).
3. Muốn ghi: gõ vào cột **Write value**, chọn dòng rồi bấm **Write selected**. Sai định dạng/vượt dải sẽ có thông báo cụ thể.

**Kiểu hỗ trợ**

| Kiểu | Số register | Tương đương |
|---|---|---|
| INT16 / UINT16 / HEX16 | 1 | IEC `INT`, Mitsubishi `K` / `H` |
| BCD16 | 1 | 4 chữ số BCD (Mitsubishi BCD) |
| INT32 / UINT32 / HEX32 | 2 | IEC `DINT` |
| BCD32 | 2 | 8 chữ số BCD |
| FLOAT32 | 2 | IEC `REAL`, Mitsubishi `E`, IEEE 754 single |
| INT64 / UINT64 | 4 | IEC `LINT` |
| FLOAT64 | 4 | IEC `LREAL`, IEEE 754 double |
| ASCII text | tuỳ chọn | 2 ký tự / register |

**Thứ tự byte** (giá trị 32 bit có các byte A B C D, A là byte cao nhất):

| Lựa chọn | reg0 | reg1 | Dùng khi |
|---|---|---|---|
| `ABCD` big endian | AB | CD | đúng chuẩn Modbus |
| `CDAB` word swapped | CD | AB | **phổ biến nhất trên PLC**, kể cả thanh ghi D của Mitsubishi |
| `BADC` byte swapped | BA | DC | một số gateway |
| `DCBA` little endian | DC | BA | thiết bị đảo hoàn toàn |

Với ASCII chỉ có việc đảo byte trong từng register là có ý nghĩa: `ABCD`/`CDAB` = byte cao trước, `BADC`/`DCBA` = byte thấp trước.

Ô nhập chấp nhận cả `0x1234` và `$1234` cho các kiểu số nguyên.

### 4.4 Log

* **Minimum level** — bộ lọc được áp **ngay trong worker thread**, nên chọn `Debug` (hiện cả lệnh đọc) cũng không làm ngập hàng đợi sự kiện của GUI. Mặc định `Info`.
* **Auto scroll**, **Clear**, **Save…** (hoặc `Ctrl+S`).
* Mỗi request được giải mã **đúng theo function code**: FC22 hiện AND mask / OR mask, FC23 hiện cả dải đọc lẫn dải ghi, FC07/11/17 ghi rõ *no operands*.

---

## 5. Lưu ý quan trọng (đã kiểm chứng trên Qt 6.11.1)

* **Unit ID phải khớp tuyệt đối.** `QModbusTcpServer` **âm thầm bỏ qua** mọi frame có unit id khác `serverAddress()` — không trả lời, không exception. Phía client chỉ thấy *timeout*. Nhiều tool mặc định unit id `255` hoặc `0`, nhớ đổi cho khớp ô **Unit ID**. App in một dòng cảnh báo trong log mỗi lần start.
* **Listen address chỉ nhận IPv4 dạng số.** Gõ `localhost` hay địa chỉ IPv6 sẽ bind thất bại với thông báo vô nghĩa của Qt, nên app tự chặn trước và báo lỗi rõ ràng.
* **`QModbusServer::setData()` cũng phát `dataWritten()`.** Vì vậy `ModbusRunner` dùng cờ `m_localWrite` (kèm RAII guard) để phân biệt "HMI ghi" với "client ghi" — nếu bỏ cờ này sẽ có vòng lặp cập nhật.
* **FC05 lưu nguyên hằng `0xFF00`** vào ô coil. `ModbusRunner::onDataWritten()` chuẩn hoá lại về `0/1` trước khi đưa lên HMI.
* **Giá trị không mất khi stop/start**: `ModbusRunner` giữ shadow copy và nạp lại vào `setMap()` mỗi lần start.
* Port **502** trên Windows không cần quyền admin; trên Linux/macOS thì cần (port < 1024) — dùng `5020` cho tiện.

---

## 6. Tuỳ chỉnh

**Đổi số địa chỉ mỗi area** — sửa đúng một dòng trong `modbusdefs.h`:

```cpp
constexpr int kAddressCount = 64;   // đổi thành 128, 256, …
```

Bảng, `setMap()`, Data Monitor và mọi kiểm tra biên đều lấy theo hằng số này.

**Thêm kiểu dữ liệu mới** — thêm một giá trị vào `enum DataCodec::Type`, một tên vào `DataCodec::typeNames()`, số register vào `registerCount()`, rồi nhánh xử lý trong `decode()` / `encode()`. Cả combo **Display** của 2 tab register lẫn combo của Data Monitor đều tự lấy danh sách từ `typeNames()`, không phải sửa gì thêm.

**Đổi giao diện** — mở `mainwindow.ui` bằng Qt Designer. Giữ nguyên `objectName` của các widget đang được code dùng (`coilsTable`, `holdingRegistersPlcBaseSpin`, `monitorTable`, `logView`, `startButton`, …) là code vẫn chạy.

---

## 7. Ví dụ phía client bằng Python (`samples/`)

App lo phía server. Phía client thì việc ghép/tách thanh ghi là của bạn, nên trong
`samples/` có sẵn ba file **thuần Python, không cần thư viện Modbus**:

| File | Nội dung |
|---|---|
| `samples/modbus_value_codec.py` | module chuyển đổi: `decode()`, `encode()`, `register_count()`, `format_value()` cho đủ 13 kiểu và 4 thứ tự byte |
| `samples/decode_float32_simple.py` | **demo tối giản**: 2 thanh ghi (địa chỉ 0 và 1) → 1 số FLOAT32 big endian |
| `samples/holding_register_demo.py` | demo đầy đủ với **dữ liệu holding register giả định** — danh sách theo dõi, batch monitor, ma trận byte order, đóng gói giá trị để ghi, các cạm bẫy hay gặp |

```bat
cd samples
python decode_float32_simple.py   :: demo tối giản
python holding_register_demo.py   :: demo đầy đủ
python modbus_value_codec.py      :: tự kiểm tra module
```

Module này **khớp 1-1 với `datacodec.cpp`** của app (đã đối chiếu 520 tổ hợp chiều đọc
và 140 tổ hợp chiều ghi), nên giá trị bạn thấy trên HMI và giá trị client giải mã
luôn giống nhau.

Dùng nhanh:

```python
from modbus_value_codec import decode, encode

regs = [0x0000, 0x3FC0]                 # 2 thanh ghi vừa đọc bằng FC03
decode(regs, "float32", "CDAB")         # -> 1.5
decode(regs, "float32", "ABCD")         # -> 2.286919e-41  (sai byte order!)

encode(25.5, "float32", "CDAB")         # -> [0x0000, 0x41CC]  gửi bằng FC16
encode("OK", "ascii", "ABCD", 1)        # -> [0x4F4B]          gửi bằng FC06
encode(9876, "bcd16")                   # -> [0x9876]
```

Chỗ nối vào thư viện Modbus của bạn chỉ có 2 dòng:

```python
regs = client.read_holding_registers(0, count=64, slave=1).registers   # pymodbus
client.write_registers(20, encode(25.5, "float32", "CDAB"), slave=1)
```

---

## 8. Kiểm thử nhanh

Dùng bất kỳ Modbus master nào (Modbus Poll, QModMaster, `pymodbus`, …), trỏ tới `127.0.0.1:502`, **unit id 1**:

```python
from pymodbus.client import ModbusTcpClient
c = ModbusTcpClient("127.0.0.1", port=502)
c.connect()
print(c.read_holding_registers(0, count=10, slave=1).registers)
c.write_register(5, 0x1234, slave=1)
c.write_coil(3, True, slave=1)
c.close()
```

Cửa sổ log sẽ hiện lần lượt: client connected → từng request → `Client wrote HoldingRegisters[5] = 0x1234` → client disconnected.

Kiểm tra kiểu dữ liệu: thêm một dòng Data Monitor `FLOAT32`, `CDAB`, địa chỉ 30 trên Holding Registers, gõ `1.0` vào *Write value* rồi **Write selected** — client đọc register 30/31 sẽ thấy `0x0000 0x3F80`.
