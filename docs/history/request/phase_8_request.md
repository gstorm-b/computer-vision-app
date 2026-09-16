Goal:
    - Thêm inherit cho camera device, là Jai_Gige.
        - Docs và sample code đặt tại: 
        reference_source/JaiCamTest và 
        reference_source/Jai_ebus_docs
    - Thiết kế widget cho device Jai_Gige.
    - Thêm frame và context cho McProtocolDevice frame
    dạng 1C, 3C:
        - Reference codebase cho 1C đặt tại:
        reference_source/1C
        - Reference codebase cho 3C đặt tại:
        reference_source/3C (codebase implement trên python)
        - Unit test đầy đủ command và benchmark riêng hỗ trợ 
        test với device thật cho 3E, 1C, 3C. Mình sẽ set 
        parameters và chạy các unit test này.
    - Thêm Modbus device client và modbus device server:
        - Chỉ hỗ trợ TCP/IP interface
        - Vừa có khả năng như một PLC device hỗ trợ IO Mapping 
        - Vừa có khả năng của một outut device hỗ trợ output
        kết quả vision (ghi kết quả vào holding register).
    - Thiết kế widget cho device modbus client và modbus server.

Notes:
    - Sau khi kết thúc phase owner sẽ tự xóa folder 
    reference_source.
