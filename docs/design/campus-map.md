# Bản đồ campus DUT

Source gốc của người dùng được giữ ở workspace. Bản sao đóng gói nằm tại
`simulation/gazebo/`: generator, world và models/dut_campus gồm model SDF,
mesh Collada, ground/facade texture.

Đây là mô hình bố trí theo generator với kích thước ước lượng; không phải bản
khảo sát địa lý hoặc bản đồ tránh vật cản đầy đủ.

## Tọa độ và tham chiếu

| Đại lượng | Giá trị |
|---|---|
| Ảnh gốc / texture | 257 × 300 / 1542 × 1800 |
| Tỉ lệ ảnh gốc / texture | 1,5 / 0,25 m mỗi pixel |
| Mặt bằng | 385,5 × 450 m |
| Cổng | Pixel (205,288) → ENU (0,0) |
| World origin WGS84 | Latitude 16.074°, longitude 108.1499°, elevation 10 m |
| Hướng world | ENU, heading 0° |
| Campus pose | Không dịch/xoay so với world |
| Hình học 2D | 22 khối nhà, 4 tuyến đường, 154 hình chiếu tán cây |

```text
East  = (x - 205) × 1.5 m
North = (288 - y) × 1.5 m
```

Backend dùng WGS84 ECEF ↔ ENU hai chiều, không dùng GPS drone làm gốc trôi.
UI đổi pointer qua bounding rectangle thực của map, kể cả scale/pan; ảnh giữ
tỉ lệ 257/300. Độ cao mission theo Home PX4, khác elevation gốc world và spawn z.
Home phải trong 5 m quanh cổng của map để compiler chấp nhận bản này.

Các điểm tham chiếu cho SITL:

| Pixel | East / North |
|---|---|
| 205,288 | 0 / 0 m |
| 215,288 | +15 / 0 m |
| 205,278 | 0 / +15 m |

Unit test kiểm chiều trục, cổng, các góc và round-trip dưới 1 mm cho phần phép
toán trong extent campus. Cần đo riêng độ lệch GPS/EKF và pose trong Gazebo;
chưa có số đo thực cho bản web.

## Sinh lại asset

```powershell
python -B web_client/tools/export_campus.py
```

Exporter dùng thư viện chuẩn Python và đọc literal trong AST của generator,
không chạy script người dùng. World XML phải ENU/heading 0, campus pose 0;
FLIP_X/Y phải false. Kiểu khác cần hiệu chỉnh trước khi export.

Đầu ra: `web_client/frontend/assets/campus_map.json` và `campus_2d.svg`.
Map JSON chứa hash generator/mesh/world và version tính từ nội dung chuẩn hóa.
Draft gắn với map_id/version; mở sai map phải báo lỗi.

Độ cao mái dùng số tầng đã làm tròn của generator (FH=3,6 m), có xét gờ mái và
thiết bị khi kiểm đoạn bay. Compiler kiểm footprint nhà cộng khoảng đệm ngang
2 m, cao 2 m, dùng độ cao nhỏ nhất của đoạn để xét bảo thủ. Độ cao relative Home được quy về gốc world và trừ thêm 2 m dự phòng thay đổi Home được phép. Các cây/cột/chi tiết
khác không được coi đã kiểm tránh va chạm. SDF hiện có collision nhà và ground;
không phải mọi vật nhìn thấy đều có collision.

Đưa lên Ubuntu: chép `simulation/gazebo/models/dut_campus` vào
`~/PX4-Autopilot/Tools/simulation/gz/models/` và world vào thư mục worlds
tương ứng đang được PX4/Gazebo tìm thấy. Giữ đúng tên `dut_campus`.
Lệnh chạy cùng các bước đối chiếu nằm trong [hướng dẫn demo](../guides/web-client-running.md).

Đường về gồm waypoint trên Home tại độ cao waypoint cuối, rồi LAND. Cả đoạn tiếp cận và cột hạ/cất cánh đều dùng tọa độ Home thực tế, không ép về pixel cổng. Vùng nhà tại Home vẫn bị chặn dù waypoint đang ở 100 m.
