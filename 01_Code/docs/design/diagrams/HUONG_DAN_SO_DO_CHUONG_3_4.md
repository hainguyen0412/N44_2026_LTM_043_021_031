# Sơ đồ Chương 3 và Chương 4

Tệp chính: `Chuong_3_4_MAVLink_Gateway.drawio`, gồm 12 trang cho 11 nhóm hình. Mỗi hộp, chữ và đường nối là đối tượng riêng có thể sửa trong draw.io. Tệp dùng XML không nén để thuận tiện lưu trữ và kiểm tra.

## Chèn vào báo cáo

- Mở tệp .drawio bằng draw.io hoặc diagrams.net, chọn trang theo số hình.
- Xuất trang cần dùng bằng File → Export as → PNG hoặc SVG. Các bản xuất không lưu trong Git để tránh trùng với sơ đồ gốc.
- Hình 4.2 tách thành `H4_2a` telemetry và `H4_2b` uplink. Có thể chèn hai phần dưới cùng caption hoặc tách thành hai hình và cập nhật số tham chiếu.
- Hình 4.7 trong báo cáo là ảnh chụp giao diện thật, cần nhóm tự chụp; không thay bằng một sơ đồ giả giao diện.
- Sơ đồ này được đánh số theo bản báo cáo chi tiết Chương 3 và Chương 4, không theo bản khung ban đầu.
- Các lưu đồ dài cần nới chiều cao vùng hình trong Word hoặc đặt riêng một trang để chữ đọc rõ; khoảng trống trong bản Word chỉ là vị trí dự trữ.

## Nội dung các trang

- `H3_1` — Sự kiện nhận và tách khung UDP và TCP
- `H3_2` — Trạng thái giao dịch mission
- `H3_3` — Pixel ENU WGS84 và mốc Home
- `H3_4` — Đoạn bay và khoảng đệm công trình
- `H4_1` — Worker và hàng đợi của gateway
- `H4_2a` — Telemetry và phân phối tới client
- `H4_2b` — Chấp nhận và chuyển uplink về PX4
- `H4_3` — Parser MAVLink 2 và tái đồng bộ
- `H4_4` — Command Manager và kết quả lệnh
- `H4_5` — Upload và readback mission
- `H4_6` — Dãy mission và các đoạn kiểm hình học
- `H4_8` — Một máy Ubuntu với lớp Wine

## Phạm vi và dữ liệu

Các sơ đồ bám mã nguồn hiện tại trong Project_CK_LTM_v2 và bản báo cáo chi tiết hai chương. Bản đồ H3.3 sử dụng các footprint và tuyến đường trong campus_map.json, với chú thích native có thể sửa. Cây và chi tiết trang trí được lược để tránh rối hình. Điểm P và độ cao ví dụ là minh họa toán học, không phải số đo bay. H4.8 mô tả executable Windows chạy qua Wine; không biểu diễn bản build Linux native.

Sơ đồ trạng thái mission tổng hợp MissionManager và ClientRuntime. Sơ đồ Command Manager tách trạng thái vận hành khỏi kết quả kết thúc yêu cầu. Các mốc timeout và giới hạn là cấu hình mẫu của dự án; điều chỉnh chú thích nếu nhóm thay cấu hình thực nghiệm.
