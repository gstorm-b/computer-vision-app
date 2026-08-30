#!/usr/bin/env python3
"""Ví dụ chuyển đổi giá trị holding register ở phía client.

Script này KHÔNG nói chuyện Modbus. Nó giả định bạn đã đọc xong 64 thanh ghi
holding register (FC03) và có sẵn danh sách số nguyên 16 bit - đúng thứ mà mọi
thư viện Modbus trả về, ví dụ::

    regs = client.read_holding_registers(0, count=64, slave=1).registers   # pymodbus

Phần còn lại - ghép thanh ghi thành INT32 / FLOAT32 / BCD / ASCII, và tách giá
trị ra thành thanh ghi để ghi bằng FC06 / FC16 - là việc của client, và đó là
những gì được minh hoạ ở đây.

Chạy:  python holding_register_demo.py

Trên cmd.exe cũ, nếu tiếng Việt hiện sai thì gõ ``chcp 65001`` trước.
"""

from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Console Windows mặc định là cp1252, không in được tiếng Việt.
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

from modbus_value_codec import (  # noqa: E402
    BYTE_ORDERS,
    decode,
    encode,
    format_value,
    register_count,
)

# --------------------------------------------------------------------------- #
# Dữ liệu giả định: ảnh chụp holding register 0..63 vừa đọc về
# --------------------------------------------------------------------------- #

HOLDING = [0] * 64

HOLDING[0:2] = [0x0000, 0x3FC0]                    # FLOAT32, word swapped (CDAB)
HOLDING[2:4] = [0x4248, 0x0000]                    # FLOAT32, big endian (ABCD)
HOLDING[4] = 0xFB2E                                # INT16 âm
HOLDING[5] = 0x1234                                # BCD 4 chữ số
HOLDING[6:8] = [0xFFFE, 0xFFFF]                    # INT32, word swapped
HOLDING[8:11] = [0x4D6F, 0x6462, 0x7573]           # ASCII "Modbus"
HOLDING[12:16] = [0x4009, 0x21FB, 0x5444, 0x2D18]  # FLOAT64 big endian
HOLDING[16] = 0xEA60                               # UINT16
HOLDING[17:19] = [0x2800, 0xEE6B]                  # UINT32, word swapped

#: Danh sách theo dõi, giống tab Data Monitor của app.
#: (tên, địa chỉ, kiểu, byte order, số thanh ghi cho ASCII)
WATCH = [
    ("Setpoint",     0,  "float32", "CDAB", 1),
    ("Flow rate",    2,  "float32", "ABCD", 1),
    ("Offset",       4,  "int16",   "ABCD", 1),
    ("Batch number", 5,  "bcd16",   "ABCD", 1),
    ("Total count",  6,  "int32",   "CDAB", 1),
    ("Device name",  8,  "ascii",   "ABCD", 3),
    ("Pi",          12,  "float64", "ABCD", 1),
    ("Raw counter", 16,  "uint16",  "ABCD", 1),
    ("Big counter", 17,  "uint32",  "CDAB", 1),
]

PLC_BASE = 40001  # địa chỉ 0 tương ứng 40001, đổi cho khớp thiết bị của bạn


def rule(title: str) -> None:
    print()
    print(title)
    print("-" * len(title))


def words(registers) -> str:
    return " ".join(f"{r:04X}" for r in registers)


# --------------------------------------------------------------------------- #
# 1. Danh sách theo dõi
# --------------------------------------------------------------------------- #

def show_watch_list() -> None:
    rule("1. Danh sách theo dõi (giống tab Data Monitor)")
    print(f"{'Tên':<14}{'Địa chỉ':<12}{'PLC ref':<16}{'Kiểu':<10}"
          f"{'Order':<7}{'Thanh ghi thô':<20}{'Giá trị'}")

    for name, address, data_type, order, ascii_regs in WATCH:
        count = register_count(data_type, ascii_regs)
        registers = HOLDING[address:address + count]
        value = decode(registers, data_type, order)

        span = str(address) if count == 1 else f"{address} - {address + count - 1}"
        plc = (str(PLC_BASE + address) if count == 1
               else f"{PLC_BASE + address} - {PLC_BASE + address + count - 1}")

        print(f"{name:<14}{span:<12}{plc:<16}{data_type:<10}"
              f"{order:<7}{words(registers):<20}{format_value(value, data_type)}")


# --------------------------------------------------------------------------- #
# 2. Batch monitor: cùng một vùng nhớ, nhiều cách hiển thị
# --------------------------------------------------------------------------- #

def show_batch_monitor(start: int, addresses: int, data_type: str, order: str) -> None:
    per_row = register_count(data_type, 1)
    rows = addresses // per_row

    rule(f"2. Batch monitor {start}..{start + addresses - 1} "
         f"dạng {data_type.upper()} / {order}")
    print(f"{'Địa chỉ':<12}{'PLC ref':<16}{'Thanh ghi thô':<20}{'Giá trị'}")

    for row in range(rows):
        address = start + row * per_row
        registers = HOLDING[address:address + per_row]
        span = str(address) if per_row == 1 else f"{address} - {address + per_row - 1}"
        plc = (str(PLC_BASE + address) if per_row == 1
               else f"{PLC_BASE + address} - {PLC_BASE + address + per_row - 1}")

        try:
            text = format_value(decode(registers, data_type, order), data_type)
        except ValueError as error:            # ví dụ nibble không hợp lệ khi đọc BCD
            text = f"<{error}>"

        print(f"{span:<12}{plc:<16}{words(registers):<20}{text}")


# --------------------------------------------------------------------------- #
# 3. Cùng một cặp thanh ghi, đọc bằng 4 thứ tự byte khác nhau
# --------------------------------------------------------------------------- #

def show_byte_order_matrix() -> None:
    rule("3. Vì sao byte order quan trọng: cùng 2 thanh ghi, 4 cách đọc")
    registers = HOLDING[0:2]
    print(f"Thanh ghi 0..1 = {words(registers)}")
    print(f"{'Order':<8}{'FLOAT32':<18}{'INT32':<14}{'UINT32'}")
    for order in BYTE_ORDERS:
        f32 = format_value(decode(registers, "float32", order), "float32")
        i32 = decode(registers, "int32", order)
        u32 = decode(registers, "uint32", order)
        print(f"{order:<8}{f32:<18}{i32:<14}{u32}")
    print("=> Chỉ CDAB cho ra 1.5, đúng như thiết bị đã ghi.")


# --------------------------------------------------------------------------- #
# 4. Tạo thanh ghi để ghi xuống (FC06 / FC16)
# --------------------------------------------------------------------------- #

def show_write_examples() -> None:
    rule("4. Đóng gói giá trị để ghi (FC06 cho 1 thanh ghi, FC16 cho nhiều)")
    examples = [
        (25.5,         "float32", "CDAB", 1, 20),
        (-12345,       "int32",   "ABCD", 1, 24),
        (9876,         "bcd16",   "ABCD", 1, 26),
        ("OK",         "ascii",   "ABCD", 1, 27),
        ("Line-A 01",  "ascii",   "ABCD", 5, 28),
        (0xBEEF,       "hex16",   "ABCD", 1, 34),
    ]

    print(f"{'Giá trị':<14}{'Kiểu':<10}{'Order':<7}{'Địa chỉ':<12}"
          f"{'Hàm':<8}{'Dữ liệu gửi đi'}")
    for value, data_type, order, ascii_regs, address in examples:
        registers = encode(value, data_type, order, ascii_regs)
        function = "FC06" if len(registers) == 1 else "FC16"
        span = (str(address) if len(registers) == 1
                else f"{address} - {address + len(registers) - 1}")
        # ghi vào ảnh nhớ giả lập rồi đọc lại để chứng minh vòng tròn khép kín
        HOLDING[address:address + len(registers)] = registers
        back = format_value(decode(HOLDING[address:address + len(registers)],
                                   data_type, order), data_type)

        print(f"{back:<14}{data_type:<10}{order:<7}{span:<12}"
              f"{function:<8}{words(registers)}")


# --------------------------------------------------------------------------- #
# 5. Cạm bẫy thường gặp
# --------------------------------------------------------------------------- #

def show_pitfalls() -> None:
    rule("5. Vài cạm bẫy")

    # BCD không phải hex
    print(f"Thanh ghi 5 = {HOLDING[5]:04X}: "
          f"đọc BCD16 -> {decode([HOLDING[5]], 'bcd16')}, "
          f"đọc UINT16 -> {decode([HOLDING[5]], 'uint16')}")

    # nibble > 9 thì không phải BCD hợp lệ
    try:
        decode([0x1A34], "bcd16")
    except ValueError as error:
        print(f"0x1A34 đọc BCD16 -> lỗi: {error}")

    # vượt dải
    try:
        encode(70000, "int16")
    except ValueError as error:
        print(f"Ghi 70000 vào INT16 -> lỗi: {error}")

    # chuỗi dài hơn chỗ đã dành
    try:
        encode("QUA-DAI", "ascii", "ABCD", 2)
    except ValueError as error:
        print(f'Ghi "QUA-DAI" vào 2 thanh ghi -> lỗi: {error}')

    # ASCII lẻ ký tự được đệm bằng 0x00
    print(f'Ghi "OK!" vào 2 thanh ghi -> {words(encode("OK!", "ascii", "ABCD", 2))}')


def main() -> int:
    print("=" * 78)
    print("Chuyển đổi giá trị holding register ở phía client - dữ liệu giả định")
    print("=" * 78)

    show_watch_list()
    show_batch_monitor(0, 8, "int16", "ABCD")
    show_batch_monitor(0, 8, "hex16", "ABCD")
    show_batch_monitor(0, 8, "float32", "CDAB")
    show_byte_order_matrix()
    show_write_examples()
    show_pitfalls()

    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
