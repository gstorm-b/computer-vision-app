Phase 1:
Thiết kế module robot kinematics:
-	Mục đích chính: Hỗ trợ forward và inverse kinematic. Là một phần nhỏ được dùng trong các dự án có thể reuse ở nhiều module cho dự án này (có thể reuse cho các dự án khác) với nhiều vai trò: support tính toán trajector, tính toán inverse hỗ trợ vision output check sigularity hoặc position quá giới hạn.
-	API chính: config, get config, forwardKinematic, inverseKinematc
-	Có abstract class. Các dạng robot khác nhau là kế thừa.
-	Config có đầy đủ parameters: giới hạn joint angles, cấu hình vật lí của robot để hỗ trợ tính động học.
-	Với robot 6DOF hoặc các dạng serialize robot tương tự có hỗ trợ config lefty - righty, under- above, flip - nonflip (cho J5).
-	Result cho forward và inverse gồm: kết quả tính (có thành công hay không), kết quả góc hoặc position, có thể trả về nhiều bộ góc hoặc position nếu không chỉ định cấu hình lefty-righty, above-below,....
-   Config có dạng robot serial 4dof, 5dof, 6dof, delta (spider parallel 3 joint), scara. để hỗ trợ custom preset sau này.
-	Có sẵn preset cho một số loại robot: Nachi MZ04 (serial 6dof), Kawasaki RS007 (serial 6dof).
-	Hãy đề xuất phương pháp tính động học phù hợp: ngoài Denavit-Hartenbert parameter ra thì các source dùng mô phỏng robot trong môi trường ảo của ROS hoặc gazebo tính như thế nào.
-	Đặt module này vào một workspace riêng.

Phase 2:
- Tích hợp nó vào vision output device và widget vision device widget.
- Cho phép bật tắt config robot kinematic check.
- Cho phép chọn prest, thay đổi preset, (Hiện tại chưa cho phép tạo custom preset.)

Phase 3: Preset correction
- Đây là thông tin Urdf của Mz04 mà mình có được, không chắc chắn đúng:
        const UrdfJoint joints[6] = {
        //  ox    oy    oz     roll pitch yaw  ax ay az    min     max
        {  0.0,  0.0,   0.0,  0.0, 0.0, 0.0,  0, 0, 1,  -170.0,  170.0 }, // J1
        {  0.0,  0.0, 340.0,  0.0, 0.0, 0.0,  0, 1, 0,   -90.0,  145.0 }, // J2
        {  0.0,  0.0, 260.0,  0.0, 0.0, 0.0,  0, 1, 0,  -125.0,  280.0 }, // J3
        {  0.0,  0.0,  25.0,  0.0, 0.0, 0.0,  1, 0, 0,  -190.0,  190.0 }, // J4
        { 280.0, 0.0,   0.0,  0.0, 0.0, 0.0,  0, 1, 0,  -120.0,  120.0 }, // J5
        { 72.0,  0.0,   0.0,  0.0, 0.0, 0.0,  1, 0, 0,  -360.0,  360.0 }, // J6
    };
- Mình đã import các thông tin này tạo một preset và test kết quả nhưng dã sai (Với Forward kinematics)
- Đây là kết quả đúng khi monitor trên pendant của robot:
    - Joint: 28.16, -18.81, 163.84, -0.71, 35.89, 152.73
    - Pose (không có tool offset): 339.14, -182.12, -571.95, -0.31, 1.01, -179.94(x, y, z, rx, ry, rz ở hệ tọa độ world, robot đang được set ceiling, hệ tọa độ lật xuống dưới) 
- Hãy viết unit test, tìm ra preset chuẩn cho Urdf từ kết quả trên.


Thông tin pose:
- Joint (J1 -> J6):
    1  J(28.1579,-18.8069,163.839,-0.710019,35.8922,152.731)
    2  J(46.3996,-18.8029,163.841,-0.710019,35.8898,152.727)
    3  J(-2.49169,-18.8029,163.843,-0.710019,35.8898,152.727)
    4  J(28.1557,-13.0437,163.841,-0.710019,35.8898,152.727)
    5  J(28.1557,-30.531,163.843,-0.710019,35.8898,152.727)
    6  J(28.1557,-18.8049,180.206,-0.710019,35.8898,152.727)
    7  J(28.1557,-18.8029,147.018,-0.710019,35.8898,152.727)
    8  J(28.1557,-18.8049,163.841,34.0763,35.8898,152.727)
    9  J(28.1557,-18.8029,163.841,-35.5607,35.8898,152.727)
    10  J(28.1557,-18.8049,163.841,-0.714615,69.0539,152.727)
    11  J(28.1557,-18.7989,163.841,-0.714615,7.70156,152.721)
    12  J(28.1557,-18.8029,163.841,-0.710019,35.8898,200.78)
    13  J(28.1557,-18.8029,163.841,-0.712317,35.8898,120.206)
    14  J(28.2282,-18.9278,164.21,0.00919119,34.718,151.759)
    15  J(31.122,-16.8986,160.187,21.2041,48.5086,133.088)
    16  J(29.6938,-20.1833,169.043,25.1747,18.7945,127.285)
    17  J(28.2282,-18.9238,164.212,0.0137868,34.718,219.293)
    18  J(32.1613,-18.6659,165.74,40.5814,34.3617,128.805)
    19  J(23.2932,-15.8386,159.494,-35.6112,53.2523,185.217)
    20  J(-0.00219726,0.00430813,179.996,0.00459559,0.0046875,-0.000121055)
    21  J(0,90.002,-0.00217014,0.00459559,-0.00234375,6.05273e-05)

- Pose (X, Y, Z, roll, pitch, yaw) không có tool setting (tool offset 0,0,0,0,0,0):
    1  X(339.15,182.108,571.95,0.301758,-1.00829,0.060508)
    2  X(265.078,279.092,571.996,18.5415,-1.01493,0.0655774)
    3  X(384.555,-16.1959,572.015,-30.3416,-1.01655,0.0663919)
    4  X(316.881,170.172,609.464,0.129697,-6.10352,2.76823)
    5  X(373.609,200.531,488.92,-0.10146,9.3413,-5.45622)
    6  X(255.728,137.446,598.296,-0.835413,-15.4284,7.91414)
    7  X(414.47,222.397,518.315,-0.604066,13.8207,-7.93948)
    8  X(355.761,163.565,567.837,29.5244,3.54526,19.2647)
    9  X(333.466,206.294,567.493,-26.5584,-14.1285,-14.8798)
    10  X(304.422,163.866,559.622,-4.14049,-30.099,16.8315)
    11  X(369.405,197.824,564.041,-2.70127,23.8103,-13.6682)
    12  X(339.126,182.077,571.996,48.3522,-0.72722,-0.711033)
    13  X(339.117,182.074,572.001,-32.2176,-0.821677,0.600015)
    14  X(339.088,182.008,572.065,-0.00376925,-0.00358024,0.00786421)
    15  X(339.088,181.928,572.142,-0.0238987,-0.00493311,18.4547)
    16  X(339.057,182.004,572.09,-0.00233472,16.0384,0.0113963)
    17  X(339.062,182.009,572.097,67.5266,-0.0106346,-0.000248291)
    18  X(339.082,181.968,572.085,17.0802,11.0767,19.3541)
    19  X(339.119,182.571,571.895,6.5187,-18.922,-23.4265)
    20  X(234.998,-0.00901241,692.029,-179.995,0.00212387,-2.86984e-08)
    21  X(351.991,4.50987e-07,624.996,-61.782,-89.9947,-118.218)

Các cặp pose tương ứng vói nhau theo số thứ tự.
Cặp pose số 20 và số 21 là tư thế chuẩn như trong manual cho thấy có một vài thông tin trong preset là đúng:
- Với pose số 20: khoảng cách từ base đến vị trí của joint 1 là 340mm, joint 1 và joint 2 có thể xem là nằm cùng một ví trí (vì tổng từ base đến joint 2 theo trục z là 340) nhưng trục xoay khác nhau (joint 1 oz, joint 2 oy), từ joint 2 đến joint 3 là 235mm
- Góc xoay của các joint theo base ở tư thế số 21 lần lượt là oz, oy, oy, ox, oy, ox
- Hãy tạo thông tin urdf theo pose số 21.
- Trước tiên hãy kiểm tra dựa trên pose 21, 20 rồi đến các pose còn lại.
- Mình nghĩ thông tin udrf mình cung cấp ban đầu đã gần đúng.
