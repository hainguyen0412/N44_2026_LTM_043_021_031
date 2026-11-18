# Chạy web client và mission

## 1. Chọn đúng kiểu demo

| Trường hợp | Cấu hình | Cổng |
|---|---|---|
| PX4 trên Ubuntu gửi tới Windows | `config/gateway.mission.example.json` | UDP 14550, observer 5760, controller 5761; web 8080 |
| Chỉ thử giao thức trên một máy Windows | `config/gateway.web-demo.example.json` | Loopback UDP 15450, observer 15760, controller 15761; web 8081 |

Hai kiểu dùng cổng riêng. Fake peer chỉ gửi tới localhost; không cần PX4, Gazebo hoặc QGroundControl khi chạy kiểu thứ hai.

Web và gateway phải dùng cùng một file cấu hình hoặc các bản sao có cùng thông số. Cấu hình cũ `gateway.example.json` là schema 1, không bật mission.

## 2. Thử ngay tại Windows bằng dữ liệu giả lập

Mở ba cửa sổ PowerShell riêng. Cả ba đứng tại thư mục project:

```powershell
cd "D:\BK_subjects\Nam_4_ki_1\LT_mang\du_an_cuoi_ki\Project_CK_LTM_v2"
```

Cửa sổ 1 — gateway:

```powershell
.\bin\gateway.exe --config config/gateway.web-demo.example.json
```

Giữ cửa sổ này chạy; chờ dòng READY.

Cửa sổ 2 — đối tác PX4 giả lập giao thức:

```powershell
python -B tools/fake_px4_mission.py --config config/gateway.web-demo.example.json
```

Cửa sổ 3 — server/client web:

```powershell
.\bin\web_client.exe --config config/gateway.web-demo.example.json --web-port 8081
```

Mở trình duyệt tại **http://127.0.0.1:8081**. Chọn Controller, nhấn Kết nối. Pin và vị trí xuất hiện khi đã có heartbeat. Bản đồ là dữ liệu campus thật của project; telemetry/hành vi ở lượt demo này là giả lập giao thức, không phải mô phỏng động lực học bay.

Thử theo thứ tự:

1. Chọn waypoint, sửa độ cao/thời gian dừng; Thêm điểm, kéo, đổi thứ tự hoặc xóa. Dùng +/− để zoom; khi đã zoom và tắt Thêm điểm, kéo nền để xem vùng khác. Nhấn phần trăm để đặt lại góc nhìn.
2. Lưu JSON rồi Mở JSON để kiểm tra bản nháp.
3. Nhấn Tải lên PX4; chờ đủ số mục, ACK và bước đọc lại/đối chiếu. Nhãn phải chuyển thành “PX4 đã xác minh”.
4. Sang tab Điều khiển, ARM và xác nhận; chờ telemetry hiển thị ARMED.
5. Về tab Lập lộ trình, Bắt đầu mission và xác nhận.
6. Quan sát mode/current/reached và trạng thái kết thúc do fake peer phát. Fake peer không mô phỏng chính xác quỹ đạo/GPS theo mọi waypoint.
7. Muốn chạy lần nữa, đợi DISARMED rồi tải/xác minh một lượt mission mới. Nút Start không tái sử dụng lượt đã chạy.

## 3. Chạy với PX4/Gazebo trên Ubuntu

Đây là quy trình kiểm thử SITL cần người dùng thực hiện; chưa có kết quả bay thực tế cho bản web này trong lần triển khai.

### Windows

Đóng controller console đang giữ cổng controller. Chạy hai lệnh ở hai cửa sổ PowerShell:

```powershell
.\bin\gateway.exe --config config/gateway.mission.example.json
```

```powershell
.\bin\web_client.exe --config config/gateway.mission.example.json
```

Hoặc nhấp lần lượt:

- `scripts/windows/CHAY_GATEWAY_MISSION.cmd`
- `scripts/windows/CHAY_WEB_CLIENT.cmd`

Mở **http://127.0.0.1:8080**. Hai file CMD gọi các lệnh trên thông qua `scripts/run.ps1`; chúng không thay thế gateway bằng một ứng dụng khác.

### Ubuntu

Dùng lệnh đã chạy thành công với world của bạn:

```bash
cd ~/PX4-Autopilot
GZ_SIM_RESOURCE_PATH=$GZ_SIM_RESOURCE_PATH:$HOME/PX4-Autopilot/Tools/simulation/gz/models PX4_GZ_WORLD=dut_campus PX4_GZ_MODEL_POSE="0,0,0.2,0,0,0" make px4_sitl gz_x500
```

Giữ cách cấu hình MAVLink UDP đã kiểm tra trước đó: port local 14280, remote Windows:14550. Địa chỉ Windows phải là IP hiện tại của máy bạn; không mặc định IP hotspot cũ còn đúng.

Trong `pxh>`, kiểm tra `mavlink status`, rồi bật thêm Home và landed state trên đúng instance. Tên stream phải tồn tại trong PX4 đang dùng; nếu báo không có stream, gửi kết quả để đối chiếu phiên bản.

```text
mavlink stream -u 14280 -s HEARTBEAT -r 1
mavlink stream -u 14280 -s SYS_STATUS -r 1
mavlink stream -u 14280 -s ATTITUDE -r 10
mavlink stream -u 14280 -s GLOBAL_POSITION_INT -r 5
mavlink stream -u 14280 -s HOME_POSITION -r 1
mavlink stream -u 14280 -s EXTENDED_SYS_STATE -r 1
```

`MISSION_CURRENT` và `MISSION_ITEM_REACHED` được xử lý theo bản tin PX4 phát trong giao thức mission. Web không tự tạo tiến độ bay.

### Đối chiếu trước lượt bay đầu tiên

1. Chọn Observer trước để kiểm tra telemetry và marker tại cổng.
2. Đối chiếu cổng cùng hai điểm theo hai trục với Gazebo; origin/ENU/ảnh cần đúng hướng. Chưa bay nếu marker bị lật hoặc lệch.
3. Ngắt Observer rồi kết nối Controller; QGroundControl có thể quan sát nhưng không đồng thời ghi/thay mission.
4. Chọn mission ngắn trong vùng đã kiểm tra. Bộ kiểm tra hiện xét các footprint nhà đã biết với khoảng đệm; không phải tính năng tránh mọi vật cản.
5. Chờ Home và dữ liệu PX4 hợp lệ; upload/readback khi DISARMED, rồi ARM và Start chủ động.
6. Nếu PX4 từ chối ARM/start, xem và chụp ACK/preflight. Không dùng force-arm hay tắt preflight để vượt lỗi.
7. Theo dõi cả Gazebo, trạng thái web và thông báo lệnh. Chỉ coi đã hạ cánh khi có landed state cùng trạng thái tương ứng.

## 4. Tham chiếu độ cao và tọa độ

- Waypoint trên web: độ cao theo Home PX4, không phải độ cao tăng thêm so với vị trí drone hiện tại.
- TAKEOFF riêng: dùng cách đóng gói hiện có của client, vị trí hiện tại và MIS_TAKEOFF_ALT.
- Mission TAKEOFF: dùng độ cao cất cánh đã nhập trên planner.
- Elevation 10 m của world và spawn z=0,2 m không phải độ cao cất cánh mission.
- Map có tỉ lệ 1,5 m/pixel ảnh gốc, cổng (205,288), hệ ENU. UI resize không đổi tỉ lệ tọa độ.
- Home phải gần gốc cổng trong giới hạn 5 m của bản này; sai tham chiếu phải chỉnh/calibrate chứ không bỏ kiểm tra.

## 5. Quyền controller và nhiều tab

Một backend có một kết nối TCP gateway. Một tab sở hữu token điều khiển; tab khác có thể xem dữ liệu nhưng không gửi lệnh thay tab đó. Token lưu trong sessionStorage của tab, không ghi log.

Tab điều khiển duy trì lease mỗi 4 giây, hết hạn sau 15 giây không duy trì. Khi hết hạn, tab khác có thể nhấn Nhận quyền điều khiển. Trình duyệt đặt tab ngủ có thể làm lease hết hạn; API sẽ từ chối token cũ và yêu cầu nhận quyền lại.

Refresh/SSE reconnect chỉ đồng bộ dữ liệu, không tự gửi ARM/Start. Đóng tab không tự LAND/DISARM và không tắt backend. Backend còn chạy có thể vẫn giữ kết nối GCS/heartbeat; dừng chương trình bằng Ctrl+C khi muốn đóng kết nối đó. PX4 xử lý mất liên kết theo cấu hình failsafe của nó.

## 6. Không ghi file log và cách dừng

Gateway, web và client console đã **tắt hẳn ghi log file**, không có tùy chọn
bật. Không sinh CSV, run.json, resolved_config.json hoặc measurement.json khi
chạy các executable thường trong bin. Run-id và B2 không bật ghi file.

Lỗi, ACK và tiến độ mission vẫn hiện trên màn hình. Web giữ sự kiện gần đây
trong bộ nhớ; đóng chương trình sẽ không có lịch sử file của lần chạy đó.
Log cũ từ các lần chạy trước vẫn còn, không bị xóa bởi thay đổi này. Phần mô tả
file log trong tài liệu Word cũ chỉ là tham chiếu cho bản đo kiểm trước đây.

Bộ kiểm thử CSV/metrics dùng executable riêng ở build/test_support/bin; chúng
không nằm trong gói Windows phát hành. Chạy CTest hoặc CHAY_DEMO (công cụ đo
kiểm) có thể sinh bằng chứng thử nghiệm. CHAY_DEMO cần bản source đã build test;
hướng dẫn fake peer ở mục 2 dùng executable thường và không ghi file log.

Ngắt kết nối trên web đóng TCP theo quyền. Ctrl+C tại web dừng HTTP/runtime;
Ctrl+C tại gateway dừng gateway. Khi thử fake peer, Ctrl+C tại peer dừng dữ liệu.
Đóng tab không tự LAND/DISARM và không tự tắt backend.

## 7. Các lỗi thường gặp

| Hiện tượng | Kiểm tra |
|---|---|
| HTTP 8080 bận | Dừng web cũ hoặc dùng `--web-port 8081` |
| Gateway bind 10048 | Có chương trình giữ UDP/TCP; kiểm tra đúng PID trước khi dừng |
| Connect 10061 | Gateway chưa nghe đúng cổng/cấu hình |
| TCP nối rồi đóng ngay | Có thể controller khác đang giữ vai trò; xem thông báo tại cửa sổ gateway |
| Mission timeout | Cả hai phía có dùng schema 2/bật mission không; UDP về đúng PX4 không |
| Chưa có HOME_POSITION | Kiểm tra stream Home và trạng thái PX4 |
| Start không bật | Cần dữ liệu tươi, ARMED, landed state trên mặt đất và đúng lượt upload đã xác minh |
| Readback mismatch | PX4 giữ nội dung khác, GCS khác đã sửa hoặc khác biệt firmware; ghi lại thông báo lỗi để phân tích |
| Token/lease hết hạn | Nhận quyền lại; nếu backend đã restart thì tải lại trang |
| Map version không khớp JSON | Mở đúng map hoặc tạo lại bản nháp; không sửa version để ép nhận |

## 8. Kiểm thử phát triển

```powershell
cmake --build --preset windows-release
ctest --preset windows-release
```

Test mạng cần được phép dùng loopback. Kết quả nằm trong `output/test_results/`. Kiểm tra gói phát hành bằng `python -B tools/verify_package.py` bên trong thư mục đã giải nén.

## Điểm về Home trong mission

Web tự thêm mục Về trên Home, cùng độ cao với waypoint cuối, dừng 1 giây rồi
LAND. Tổng số mục là số waypoint bạn chọn cộng 3; cấu hình 100 mục cho tối đa
97 waypoint. Muốn đổi độ cao bay về, sửa độ cao waypoint cuối trước khi upload.
Home được phép lệch tối đa 1 m ngang/2 m cao so với lúc biên dịch; vượt ngưỡng
phải disarm, kiểm tra Home rồi tải lại mission. Giới hạn công trình vẫn được
áp cho cả đường tiếp cận và vùng hạ cánh. Cần đối chiếu hành vi trên PX4/Gazebo.
