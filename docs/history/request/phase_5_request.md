Goal:
    - Thay đổi tên của GripperBox trong mtc::MatchedObject thành CollisionBoxes, 
    - Khai báo một class mới là GripperBoxes, sau đó chuyển các member từ MatchPatternConfig vào
    các member gồm:
        - m_pickingBoxDistance
        - m_pickingBoxSize
        - m_pickingboxAngle
    - Thiết kế Gripper Type Register Widget table:
        - Cho phép đăng kí các preset của GripperBoxes
        - Đưa ra gợi ý cho người dùng chọn Picking box khi add new pattern
        - Đưa ra gợi ý cho người dùng khi setting trong property widget
    - Thay đổi schema của MatchPatternConfig:
        - Thêm vào GripperBoxes thay thế cho các member m_pickingBoxDistance, m_pickingBoxSize, m_pickingboxAngle
        - Thêm vào m_pickingRotationOffset (cv::Point3f) chứa thông tin của RX, RY, RZ
        - Thêm vào member m_usePickingBox (bool) : là optional cho phép bật tắt check collision
    - Thay đổi cách tính worldPoint trong LocalizationRuntimeController::buildVisionOutputPositions offset được cộng
    vào worldPoint lúc sẽ có thêm RX, RY, RZ (rotation này là rotation tính trong hệ tọa độ của tool)
    - Thay đổi schema của PickPathPoint, thêm vào 6 member dạng bool tương ứng cho x, y, z, rx, ry, rz:
        - Nếu các các member này true thì giá trị của trục này sẽ được xem như vị trí tuyệt đối thay vì offset.
        - Thay đổi cách tính trong RobotKinematicPickingChecker::isPickable
    - Thay đổi method ImageMatcher::robotPossiblePickingCheck, thêm tham số truyền vào là offset x,y,z,rx,ry,rz của 
    pattern config để kết quả possilble picking sát với thực tế hơn.
    - Thay đổi Pattern library widget:
        - Thêm vào một button "Gripper" ở row cuối cùng hàng với Add Group và Auto Sort.
        - Khi click button này thì emit event wire ở Localization pattern widget, slot này sẽ pop up một widget chứa 
        Gripper Type Register Widget cho phép người dùng đăng kí các preset
    - Thay đổi New Pattern Wizard:
        - Ở step chọn picking box, thì cho người dùng lựa chọn preset đã có sẵn hoặc custom một preset mới.
    

Điều chỉnh yêu cầu cho phase 5:
    - m_pickingBoxAngle chuyển thành per-pattern vì mỗi pattern sẽ có góc nhau, đây là nhầm lẫn khi viết yêu cầu ban đầu.
    - Chỉnh sửa AddPatternWizard và EditPatternWizard
        - Ở step set picking position: vừa set picking position vừa set picking angle
        - chèn thêm một step phía sau step set picking box: set offset X, Y, Z, RX, RY, RZ
        - Chặn sự kiện đóng dialog khi vô nhấn enter hoặc esc
    
Yêu cầu bổ sung cho phase 5:
    - Thay đổi pattern thumbnail:
        - Thay QGraphicsScene bằng một custom QGraphicsView chỉ có nhiệm vụ render thumbnail cho pattern.
        - Cách vẽ picking point, picking orientation và picking box giống như canvas của add pattern widget.
        - Có khả năng pan, zoom, reset transform nhưng không cho phép tương tác với các graphics item như picking point,
        picking orientation arrow, picking box.
