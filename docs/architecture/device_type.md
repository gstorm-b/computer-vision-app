> **Đã cập nhật 2026-08-23 (Phase 7 / D).** Ba sub-type ảo đã được đăng ký thật. Chi tiết
> đầy đủ — token, calibration tổng hợp, marker R8 — ở
> [../domains/virtual_devices/virtual_devices.md](../domains/virtual_devices/virtual_devices.md).
> File này giữ nguyên vai trò ghi chú cấu trúc.

> **Cập nhật 2026-08-27 (Phase 8 / B + C).** Modbus TCP client/server và JAI GigE đã đăng ký
> thật. Wire contract của Modbus ở
> [../domains/task_localization/modbus_result_contract.md](../domains/task_localization/modbus_result_contract.md);
> khác biệt giữa hai SDK camera ở [../../src/device/AGENTS.md](../../src/device/AGENTS.md).

Cấu trúc device type hiện tại:
    Device type:
        - UserType
        - Camera
            - Realsense
            - BaslerGige (Pylon SDK)
            - JaiGige (done — Pleora eBUS SDK, token "Jai_GigE")
            - BaslerUsb
            - Virtual (done — VirtualCameraDevice, không cần hardware)
        - McDevice
        - PLC
            - MitsubishiMc (3E binary + 1C/3C computer link)
            - ModbusTcpClient (done — mình là master)
            - ModbusTcpServer (done — mình là slave)
            - Virtual (done — VirtualPlcDevice)
        - VisionOutput
            - Virtual (done — VirtualVisionOutputDevice)
        - Robot (không có sub-type ảo — chưa có consumer nào)

Cấu trúc device type muốn thay đổi:
    Device type:
        - UserType
        - Camera (not change)
            - Realsense (future update)
            - BaslerGige
            - BaslerUsb (future update)
        - PLC
            - McDevice (current McDevice at top level)
        - VisionOutput:
            - VisionTCPIP (TCP/IP server — software listens, done)
            - VisionTcpipClient (TCP/IP client — software dials out + reconnect, done)
            - VisionSerial (future update)
        - Robot:
            - Kawasaki (future update)
            - Huayan (future update)
            - Nachi (future update)


temp_notes:
- handle signal when PLC ID changed, Output Vision changed -> re-setup task (wire signal, start runnner,... base on task phase)