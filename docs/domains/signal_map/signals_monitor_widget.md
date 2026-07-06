Mục tiêu: thiết kế Signals monitoring widget.
    - Dạng list view.
    - Dữ liệu cung cấp giống với SignalsMapWidget.
    - Mỗi row show 1 widget.
    - Widget row gồm: Qlabel Name (Signal display name), Qlabel Signal type, Qlabel Current value, QPushButton modify value.
    - Size của tất cả các object trong row widget sẽ được căn dọc (căng theo cột, cùng size với các object khác của row khác trong list view).
    - Signal type gồm Bool, number.
    - QLabel current value dạng bool sẽ show ON/OFF tag(hoặc chip), dạng number sẽ show số (dùng method setText của QLabel, dạng số float hay int sẽ phát triển thêm sau, hiện tại chỉ có số int 16bit).
    - QPushButton modify value: khi click sẽ popup ra một dialog hỗ trợ modify value.
    - Khi Device cung cấp signal chưa được connect QPushButton sẽ bị disable, chỉ enable khi Device được connect.
    - Khi một signal là bool thay đổi quá nhanh (trong vòng 200ms từ OFF thành ON và thành OFF) thì kế bên tag ON/OFF status sẽ thêm một tag Rising edge để dễ monitor. Tag rising edge sẽ ẩn đi sau 2s hoặc khi có 1 rising edge mới khi tag vẫn chưa ẩn thì nó sẽ nháy 1 lần (ẩn 500ms xong hiện lại).
    - Method refesh sẽ dùng tên của tag (không phải display name) và new value.
    - Có method cơ bản: add row, remove row, insert row

Sau khi thiết kế xong:
    - Promote: listWidget_tag_status trong LocalizatinDashboardWidget (src/ui/forms/task) bằng SignalsMonitoringWidget.

Yêu cầu fix LocalizationDashboardWidget:
    - Định nghĩa Display name trực tiếp kSignalRows.
    - Wire refesh signal của listWidget_tag_status với signal signalChanged của LocalizationTask, không wire trực tiếp với Communication device.
    - refeshBool, refreshNumber không dùng tag name mà dùng signal name để xác định signal value cần refresh.
    - Các implement liên quan đến signal trong TaskLocalization sẽ được implement trong request khác.