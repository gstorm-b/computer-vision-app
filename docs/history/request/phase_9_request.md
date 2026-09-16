Goal: (Quyết định hành vi của runtime, xây dựng contract cho các
    tình huống có thể xảy ra trong runtime)
    - Trường hợp camera active number invalid hoặc pattern group 
    active number invalid không được tính là task fault, đã có hai
    signal output là camera invalid và pattern invalid để xác nhận
    -> Cần điều chỉnh lại hành vi.
    <!-- - Kiểm tra lại quá trình beginRuntime: 
        - Luôn luôn connect PLC và Output device trước khi bắt đầu
        setup camera và pattern, vì hai input signal là nActiveCamera
        và nActivePatternGroup phụ thuộc vào giá trị đến từ hai device
        role PLC.
        - Khi bắt đầu setup cho camera và pattern trong quá trình begin
        runtime, phải request giá trị trực tiếp từ PLC role device. 
        Đây là todo item số 55 -->
    - Giải quyết todo later số 53
    - Giải quyết todo later số 54:
        - bExecuteTrigger đang giữ mức cao khi runtime khởi động thì xử
        lí như trường hợp task không ready -> Không start cycle, update
        trạng thái vào UI và output signals.