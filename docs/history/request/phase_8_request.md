Goal:
    - Thêm mask cho pattern config, và một biến bool như là optional cho việc 
    có sử dụng mask hay không:
        - Mask dạng polygon, chỉ lấy các edge bên trong mask để matching.
    - Thay đổi schema của pattern config để lưu mask.
    - Điều chỉnh method learnPattern của MatchPattern để dùng mask.
    - Chỉnh sửa AddPatternWizard và EditPatternWizard để hỗ trợ edit mask.
  