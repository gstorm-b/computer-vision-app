Goal:
    - Giải quyết vi phạm rule của RuntimeShellWindow. Cấu trúc lại folder
    runtime_app, có folder src, ui tách bạch.
    - Hỗ trợ chuyển đổi qua lại giữa runtime app và editor app.
    - Tại một thời điểm chỉ có một app được chạy, hoặc là runtime hoặc là
    editor và chỉ 1 instance duy nhất được chạy tránh mở nhiều editor hoặc
    nhiều runtime.
    - Runtime app và editor app dùng chung app setting và app logger, log
    vào app logger cả sự kiện mở runtime app và editor app.
    - Trên runtime app có menubar hỗ trợ các tính năng cơ bản như:
        - Project:
            - Load
            - Close
            - Open Editor
        - View
            - System log
            - Theme
            - Language
    - Thiết kế các virtual device kế thừa từ abstract devices, dùng cho trường
    hợp chạy unit test, đặt trong folder tools/vitual_device (dưới root).
    