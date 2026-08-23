Goal:
    - Thêm vào TaskLocalizeConfig:
        - nErrorReset (bool) : khi bTaskFault ON, cạnh lên từ nErrorReset sẽ off 
        bTaskFault, đây là hành vi confirm lỗi.
    - Thay đổi recovering policy của runtime:
            - Từ try to reconnect với số lần giới hạn chuyển thành try to reconnect 
            cho tới khi endRuntime xảy ra (không vào state Faulted từ Recovering).
                - Mục đích: Giảm thao tác của người dùng, tự động reconnect và quay 
                trở lại ready khi mất connect.
            - Khi mất kết nối vào Recovering không thông báo vào event log một sự 
            kiện connect failed giống nhau liên tục tránh làm loãng event log, gây khó
            quan sát cho operator.
    - Đã có sự thay đổi về hành vi grab failed của camera:
        - Khi runner của camera nhận được kết quả failed thì emit lại shot trigger để
        camera grab lại. Mục đính tránh gián đoạn quá trình tự động.
        - Mình đã sử hành vi tuy nhiên chưa update vào docs, hãy giúp mình update.
    - Thiết kế runtime app:
        - Main widget chứa một stacked widget gồm các page:
            - Select project file
            - Runtime view
        - Runtime khởi động lên thì load từ app setting file đã chạy trong lần chạy 
        trước đó và nhảy tới runtime view, nêu chưa có file path đã chạy hoặc không
        tìm thấy file path thì nhảy tới Select project file.
        - Các task trong runtime view:
            - Chỉ hiển thị dashboard, tự động vào runtime không cần phải nhấn nút.
            - Vì có nhiều task cùng chạy nên task có thể dock hoặc float.
            - khi dock có thể chọn layout 1, 2, 4, 6, 8 tùy số lượng task, max 8.