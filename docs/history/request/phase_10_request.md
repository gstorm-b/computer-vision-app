Goal:
    - Thêm mask cho pattern config, và một biến bool như là optional cho việc 
    có sử dụng mask hay không:
        - Mask dạng polygon, chỉ lấy các edge bên trong mask để matching.
    - Thay đổi schema của pattern config để lưu mask.
    - Điều chỉnh method learnPattern của MatchPattern để dùng mask.
    - Chỉnh sửa AddPatternWizard và EditPatternWizard để hỗ trợ edit mask.

    
    - Vẽ signals chart uml của task localization cho mọi trường hợp 
    có từ contract và hành vi của task localization đã thiết kế.
    - Thiết kế user manual cho project.
        - Có hai ngôn ngữ english và japanese, làm english trước
        translate japanese sau khi đã thiết kế xong manual.
        - Có cấu trúc rõ ràng, người đọc dễ hiểu.
        - Có giải thích cho signals của task.
        - Có đầy đủ diagram giải thích cơ chế hoạt động của task.