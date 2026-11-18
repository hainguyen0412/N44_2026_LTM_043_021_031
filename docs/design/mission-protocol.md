# Mission trong phiên bản web

## Vị trí mã nguồn

| Thành phần | File |
|---|---|
| Kiểu bổ sung, không đổi contract v1 | `include/mavlink_gateway/mission.hpp` |
| Đọc mission/Home/landed từ FrameRecord đã kiểm CRC | `src/processing/mission_decoder.cpp` |
| Đóng gói, dùng cùng SEQ client | `src/client/mavlink_builder.cpp` |
| Upload/readback, timeout/retry | `src/client/mission_manager.*` |
| Quyền, heartbeat, điều kiện Start, vòng đời | `src/client/client_runtime.*` |
| Whitelist và forward nguyên byte | `src/gateway/gateway.cpp` |

`contracts.hpp` và tập 5 thông điệp decode ban đầu giữ nguyên. Cấu hình schema 1
tiếp tục dùng cho console; mission chỉ bật bằng schema 2 và `mission.enabled`.
Command 300 cũng cần nằm trong `commands.allowed_command_ids`.

## Giao dịch

```text
Web draft → compile TAKEOFF / WAYPOINT... / WAYPOINT tren Home / LAND
Client → PX4: MISSION_COUNT
PX4 → Client: MISSION_REQUEST_INT(seq)
Client → PX4: MISSION_ITEM_INT(seq)
... → MISSION_ACK accepted
Client → PX4: MISSION_REQUEST_LIST
PX4 → Client: MISSION_COUNT
Client → PX4: MISSION_REQUEST_INT(seq)
PX4 → Client: MISSION_ITEM_INT(seq)
... → Client gửi MISSION_ACK → so khớp → verified
```

Manager kiểm tra sysid/compid, target, mission type và thứ tự mục. Mission type
chỉ là flight mission (0). Yêu cầu lặp được trả lại đúng mục; không dùng ACK sớm,
gói sai target hoặc bản tin current/reached để hoàn tất upload. Timeout/retry
theo cấu hình, tối đa 5 lần mỗi bước và 120 giây cho giao dịch.

Khung tọa độ gửi: GLOBAL_RELATIVE_ALT_INT (6), lat/lon nhân 1e7, altitude mét
theo Home. Readback cho phép PX4 chuẩn hóa frame 3/6, sai số lat/lon 1 đơn vị E7,
altitude 0,02 m, parameter 0,001; NaN yaw đối chiếu với NaN. Cờ current có thể
thay đổi trong readback nên không dùng làm nội dung so khớp.

Request legacy 40 được nhận theo hành vi đã đối chiếu PX4 1.15.4, đầu ra vẫn
ưu tiên ITEM_INT. Đây chưa phải cam kết tương thích mọi autopilot/firmware.
Tham chiếu: [MAVLink mission protocol](https://mavlink.io/en/services/mission.html),
[PX4 1.15.4 mission manager](https://github.com/PX4/PX4-Autopilot/blob/v1.15.4/src/modules/mavlink/mavlink_mission.cpp).

## Bắt đầu và kết thúc

Upload khi DISARMED. Start yêu cầu controller, heartbeat tươi, ARMED,
EXTENDED_SYS_STATE mới xác nhận đang trên mặt đất và đúng upload_request_id.
Runtime đọc lại thêm một lần ngay trước Start; nếu nội dung khác thì không gửi
command 300. Mỗi lượt upload chỉ dùng Start một lần. Muốn chạy lại phải disarm,
upload và xác minh lượt mới.

Command 300 không tự retry: mất ACK tạo kết quả unknown, vì PX4 có thể đã thực
hiện. Accepted ACK chỉ là kết quả lệnh. Tiến độ lấy từ MISSION_CURRENT /
MISSION_ITEM_REACHED; kết thúc chỉ xác nhận sau khi đã thấy mode MISSION,
landed state mới và DISARMED. UI không giả lập quỹ đạo bằng timer.

Home vượt ngưỡng so với mốc biên dịch (1 m ngang hoặc 2 m cao), boot time reset, heartbeat stale hoặc nội dung mission thay đổi
làm mất verified. Giao dịch mission failed/unknown đóng TCP và dọn queue để không
trộn phản hồi cũ vào giao dịch mới. Reconnect không tự upload, ARM hay Start.
Đọc lại sau reconnect không đủ xác nhận bản nháp chưa upload của phiên mới.

## Phạm vi

Web không tự xóa mission PX4 khi disconnect và không tự LAND/DISARM khi đóng
tab. QGroundControl có thể quan sát; tránh hai GCS đồng thời ghi mission.
Readback trước Start phát hiện khác biệt đã tồn tại, không khóa được PX4 trước
một GCS khác ghi ngay sau readback. Bộ fake peer chỉ kiểm giao thức localhost;
kết quả chuyến bay thật phải lấy từ PX4/Gazebo.

Log: `events.csv` chứa mission draft, từng item đã biên dịch và sự kiện kèm
request ID; `commands.csv` chứa ACK. Cấu hình và fingerprint nằm cùng thư mục log.

## Điểm tiếp cận và mốc Home

Compiler thêm một WAYPOINT ngay trên Home trước LAND. Độ cao điểm này bằng
waypoint cuối do người dùng chọn, dừng 1 giây, acceptance radius 1 m. Đoạn về
Home kiểm ở độ cao tiếp cận; đoạn hạ xuống kiểm cột tại Home thực tế. Chỉ có
97 waypoint người dùng nếu max_items=100; ba mục còn lại là TAKEOFF, approach
và LAND. Không tự tăng độ cao để bỏ qua lỗi công trình.

Hai chỗ so sánh Home trong runtime dùng cùng hàm khoảng cách theo mét. Mốc so
sánh là Home đã dùng biên dịch mission, không trượt theo từng gói nhận. Sai lệch
nhỏ không hủy Start; sai lệch vượt giới hạn làm mất verified và hiển thị độ lệch
ngang/cao. Giới hạn trên là chính sách ứng dụng cho SITL, cần nghiệm thu trên
world/firmware thực dùng. Mất ACK Start vẫn không tự retry.
