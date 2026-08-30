#!/usr/bin/env python3
"""Demo tối giản: đọc 1 số thực FLOAT32 từ 2 thanh ghi holding register.

Giả định client vừa đọc holding register địa chỉ 0 và 1 (FC03, quantity = 2),
dữ liệu sắp xếp big endian (ABCD) - tức là thanh ghi 0 chứa 2 byte cao.

    thanh ghi 0 = 0x4248      byte A B
    thanh ghi 1 = 0x0000      byte C D
    -> 0x42480000 -> 50.0

Chạy:  python decode_float32_simple.py
"""

import struct

# Hai thanh ghi đọc về từ địa chỉ 0 và 1.
registers = [0x4248, 0x0000]

# --- Cách 1: chỉ dùng thư viện chuẩn -------------------------------------- #
# ">HH" ghép 2 số 16 bit thành 4 byte theo thứ tự big endian,
# ">f"  đọc 4 byte đó thành FLOAT32.
raw = struct.pack(">HH", registers[0], registers[1])
value = struct.unpack(">f", raw)[0]

print("registers   :", " ".join(f"{r:04X}" for r in registers))
print("bytes (ABCD):", raw.hex().upper())
print("float32     :", value)

# --- Cách 2: dùng module đi kèm ------------------------------------------- #
# Cho kết quả y hệt, nhưng đổi kiểu / byte order chỉ bằng tham số.
from modbus_value_codec import decode  # noqa: E402

print("float32 (modbus_value_codec):", decode(registers, "float32", "ABCD"))
