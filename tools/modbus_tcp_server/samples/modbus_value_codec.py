"""Chuyển đổi giá trị Modbus ở phía client (thuần Python, không cần thư viện Modbus).

Modbus chỉ truyền các thanh ghi 16 bit. Mọi kiểu dữ liệu lớn hơn (INT32,
FLOAT32, chuỗi ASCII, BCD...) đều là quy ước ghép nhiều thanh ghi lại. Các hãng
lại không thống nhất thứ tự byte, nên mỗi hàm ở đây đều nhận tham số ``order``.

Với một giá trị có các byte A B C D (A là byte cao nhất):

======  ======  ======  ==========================================
order   reg0    reg1    dùng khi
======  ======  ======  ==========================================
ABCD    AB      CD      đúng chuẩn Modbus
CDAB    CD      AB      phổ biến nhất trên PLC (kể cả thanh ghi D
                        của Mitsubishi)
BADC    BA      DC      một số gateway
DCBA    DC      BA      thiết bị đảo hoàn toàn
======  ======  ======  ==========================================

Cả 4 phép biến đổi đều là nghịch đảo của chính nó, nên cùng một hàm dùng được
cho cả đọc lẫn ghi.

Module này khớp 1-1 với ``datacodec.cpp`` của app Modbus TCP Server Demo:
cùng dữ liệu, cùng kiểu, cùng byte order thì ra cùng kết quả.
"""

from __future__ import annotations

import struct

__all__ = [
    "BYTE_ORDERS",
    "DATA_TYPES",
    "register_count",
    "registers_to_bytes",
    "bytes_to_registers",
    "decode",
    "encode",
    "format_value",
]

BYTE_ORDERS = ("ABCD", "CDAB", "BADC", "DCBA")

#: Số thanh ghi mà mỗi kiểu chiếm. ``ascii`` do người gọi quyết định.
DATA_TYPES = {
    "int16": 1,
    "uint16": 1,
    "hex16": 1,
    "bcd16": 1,
    "int32": 2,
    "uint32": 2,
    "hex32": 2,
    "bcd32": 2,
    "int64": 4,
    "uint64": 4,
    "float32": 2,
    "float64": 4,
    "ascii": None,  # tuỳ độ dài chuỗi, 2 ký tự mỗi thanh ghi
}


# --------------------------------------------------------------------------- #
# Thứ tự byte
# --------------------------------------------------------------------------- #

def _apply_order(raw: bytes, order: str) -> bytes:
    """Đưa chuỗi byte về (hoặc ra khỏi) dạng big endian ABCD."""
    if order == "ABCD":
        return raw
    if order == "DCBA":
        return raw[::-1]
    if order == "BADC":
        out = bytearray(raw)
        for i in range(0, len(out) - 1, 2):
            out[i], out[i + 1] = out[i + 1], out[i]
        return bytes(out)
    if order == "CDAB":
        return b"".join(raw[i:i + 2] for i in range(len(raw) - 2, -1, -2))
    raise ValueError(f"byte order không hợp lệ: {order!r} (dùng {BYTE_ORDERS})")


def _effective_order(data_type: str, order: str) -> str:
    """Với chuỗi ASCII chỉ việc đảo byte trong từng thanh ghi là có ý nghĩa."""
    if data_type != "ascii":
        return order
    return "BADC" if order in ("BADC", "DCBA") else "ABCD"


def registers_to_bytes(registers, order: str = "ABCD", data_type: str = "") -> bytes:
    """Ghép danh sách thanh ghi 16 bit thành chuỗi byte đã chuẩn hoá về ABCD."""
    raw = b"".join(struct.pack(">H", int(r) & 0xFFFF) for r in registers)
    return _apply_order(raw, _effective_order(data_type, order))


def bytes_to_registers(raw: bytes, order: str = "ABCD", data_type: str = ""):
    """Chiều ngược lại: chuỗi byte ABCD -> danh sách thanh ghi theo ``order``."""
    swapped = _apply_order(raw, _effective_order(data_type, order))
    return [struct.unpack(">H", swapped[i:i + 2])[0] for i in range(0, len(swapped), 2)]


# --------------------------------------------------------------------------- #
# BCD
# --------------------------------------------------------------------------- #

def _bcd_to_int(raw: bytes) -> int:
    value = 0
    for byte in raw:
        high, low = byte >> 4, byte & 0x0F
        if high > 9 or low > 9:
            raise ValueError(f"nibble không phải BCD trong {raw.hex().upper()}")
        value = value * 100 + high * 10 + low
    return value


def _int_to_bcd(value: int, byte_count: int) -> bytes:
    if value < 0:
        raise ValueError("BCD không biểu diễn được số âm")
    out = bytearray(byte_count)
    for i in range(byte_count - 1, -1, -1):
        low = value % 10
        value //= 10
        high = value % 10
        value //= 10
        out[i] = (high << 4) | low
    if value:
        raise ValueError(f"giá trị vượt quá {byte_count * 2} chữ số BCD")
    return bytes(out)


# --------------------------------------------------------------------------- #
# API chính
# --------------------------------------------------------------------------- #

def register_count(data_type: str, ascii_registers: int = 1) -> int:
    """Số thanh ghi một giá trị chiếm."""
    if data_type not in DATA_TYPES:
        raise ValueError(f"kiểu không hỗ trợ: {data_type!r}")
    count = DATA_TYPES[data_type]
    return max(1, int(ascii_registers)) if count is None else count


def decode(registers, data_type: str, order: str = "ABCD"):
    """Đọc giá trị từ danh sách thanh ghi.

    Trả về ``int`` cho các kiểu nguyên/HEX/BCD, ``float`` cho FLOAT32/FLOAT64
    và ``str`` cho ASCII. Dùng :func:`format_value` nếu muốn chuỗi hiển thị
    giống trong app.
    """
    registers = list(registers)
    needed = register_count(data_type, len(registers))
    if len(registers) < needed:
        raise ValueError(f"{data_type} cần {needed} thanh ghi, chỉ nhận {len(registers)}")

    raw = registers_to_bytes(registers[:needed], order, data_type)

    if data_type == "int16":
        return struct.unpack(">h", raw)[0]
    if data_type in ("uint16", "hex16"):
        return struct.unpack(">H", raw)[0]
    if data_type == "int32":
        return struct.unpack(">i", raw)[0]
    if data_type in ("uint32", "hex32"):
        return struct.unpack(">I", raw)[0]
    if data_type == "int64":
        return struct.unpack(">q", raw)[0]
    if data_type == "uint64":
        return struct.unpack(">Q", raw)[0]
    if data_type in ("bcd16", "bcd32"):
        return _bcd_to_int(raw)
    if data_type == "float32":
        return struct.unpack(">f", raw)[0]
    if data_type == "float64":
        return struct.unpack(">d", raw)[0]
    if data_type == "ascii":
        return raw.split(b"\x00", 1)[0].decode("latin-1")
    raise ValueError(f"kiểu không hỗ trợ: {data_type!r}")


def encode(value, data_type: str, order: str = "ABCD", ascii_registers: int = 1):
    """Chuyển một giá trị thành danh sách thanh ghi để ghi bằng FC06 / FC16."""
    needed = register_count(data_type, ascii_registers)

    if data_type == "int16":
        raw = struct.pack(">h", _check_range(int(value), -32768, 32767, data_type))
    elif data_type in ("uint16", "hex16"):
        raw = struct.pack(">H", _check_range(int(value), 0, 0xFFFF, data_type))
    elif data_type == "int32":
        raw = struct.pack(">i", _check_range(int(value), -2**31, 2**31 - 1, data_type))
    elif data_type in ("uint32", "hex32"):
        raw = struct.pack(">I", _check_range(int(value), 0, 0xFFFFFFFF, data_type))
    elif data_type == "int64":
        raw = struct.pack(">q", _check_range(int(value), -2**63, 2**63 - 1, data_type))
    elif data_type == "uint64":
        raw = struct.pack(">Q", _check_range(int(value), 0, 2**64 - 1, data_type))
    elif data_type in ("bcd16", "bcd32"):
        raw = _int_to_bcd(int(value), needed * 2)
    elif data_type == "float32":
        raw = struct.pack(">f", float(value))
    elif data_type == "float64":
        raw = struct.pack(">d", float(value))
    elif data_type == "ascii":
        text = str(value).encode("latin-1")
        capacity = needed * 2
        if len(text) > capacity:
            raise ValueError(f"chuỗi dài {len(text)} ký tự, chỉ chứa được {capacity}")
        raw = text.ljust(capacity, b"\x00")
    else:
        raise ValueError(f"kiểu không hỗ trợ: {data_type!r}")

    return bytes_to_registers(raw, order, data_type)


def format_value(value, data_type: str) -> str:
    """Chuỗi hiển thị giống hệt cột Value của app."""
    if data_type == "hex16":
        return f"0x{value:04X}"
    if data_type == "hex32":
        return f"0x{value:08X}"
    if data_type == "float32":
        return f"{value:.7g}"
    if data_type == "float64":
        return f"{value:.16g}"
    return str(value)


def _check_range(value: int, low: int, high: int, data_type: str) -> int:
    if not low <= value <= high:
        raise ValueError(f"{data_type} phải nằm trong [{low}, {high}], nhận {value}")
    return value


# --------------------------------------------------------------------------- #
# Tự kiểm tra:  python modbus_value_codec.py
# --------------------------------------------------------------------------- #

def _self_test() -> int:
    failures = 0

    def show(value):
        if isinstance(value, list):
            return "[" + " ".join(f"{v:04X}" for v in value) + "]"
        return repr(value)

    def expect(name, got, want):
        nonlocal failures
        ok = got == want
        if not ok:
            failures += 1
        line = f"[{'PASS' if ok else 'FAIL'}] {name}: {show(got)}"
        if not ok:
            line += f"  (mong doi {show(want)})"
        print(line)

    # 1.0f = 0x3F800000, nhìn qua 4 thứ tự byte
    expect("FLOAT32 1.0 ABCD", encode(1.0, "float32", "ABCD"), [0x3F80, 0x0000])
    expect("FLOAT32 1.0 CDAB", encode(1.0, "float32", "CDAB"), [0x0000, 0x3F80])
    expect("FLOAT32 1.0 BADC", encode(1.0, "float32", "BADC"), [0x803F, 0x0000])
    expect("FLOAT32 1.0 DCBA", encode(1.0, "float32", "DCBA"), [0x0000, 0x803F])

    expect("INT32 -2 ABCD", encode(-2, "int32", "ABCD"), [0xFFFF, 0xFFFE])
    expect("INT32 -2 CDAB", encode(-2, "int32", "CDAB"), [0xFFFE, 0xFFFF])
    expect("BCD16 1234", encode(1234, "bcd16"), [0x1234])
    expect("BCD16 doc lai", decode([0x1234], "bcd16"), 1234)
    expect("ASCII AB ABCD", encode("AB", "ascii", "ABCD", 1), [0x4142])
    expect("ASCII AB BADC", encode("AB", "ascii", "BADC", 1), [0x4241])
    expect("ASCII doc lai", decode([0x4D6F, 0x6462, 0x7573], "ascii"), "Modbus")

    # vòng tròn khép kín cho mọi kiểu và mọi thứ tự byte
    samples = [
        (-12345, "int16", 1), (54321, "uint16", 1), (0xBEEF, "hex16", 1),
        (9999, "bcd16", 1), (-1234567, "int32", 2), (4000000000, "uint32", 2),
        (0xDEADBEEF, "hex32", 2), (98765432, "bcd32", 2),
        (-1234567890123, "int64", 4), (18000000000000000000, "uint64", 4),
        (-273.15, "float32", 2), (3.14159265358979, "float64", 4),
        ("Modbus", "ascii", 3),
    ]
    mismatches = 0
    for value, data_type, registers in samples:
        for order in BYTE_ORDERS:
            back = decode(encode(value, data_type, order, registers), data_type, order)
            if data_type == "float32":
                same = abs(back - value) < 1e-2
            elif data_type == "float64":
                same = abs(back - value) < 1e-12
            else:
                same = back == value
            if not same:
                mismatches += 1
                print(f"       vong tron sai: {data_type} {order} {value!r} -> {back!r}")
    expect(f"vong tron {len(samples)} kieu x {len(BYTE_ORDERS)} byte order", mismatches, 0)

    print("---")
    print("TAT CA PASS" if failures == 0 else f"{failures} FAILURES")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(_self_test())
