Mục tiêu:
    - Build một abstract chung cho các device dạng robot.
    - Build device Huayan robot và viết unit test, chưa thêm vào task hay UI.

Reference code
    - Vị trí: src/device/robot/temp_reference_codebase, sẽ xóa sau khi hoàn thành mục tiêu.
    - Codebase của huayan robot đã đã được dùng trong một project khác, đã chạy được tuy nhiên chưa tối ưu về luồng, lãng phí tài nguyên.
    - Cách implement của reference codebase theo kiểu polling, chưa kiểm soát luồng tốt dễ bị race condition.

Yêu cầu:
    - Các module codebase của device robot dùng theo kiểu event driven, tối ưu sử dụng tài nguyên, không được phép gây crash hay race condition.