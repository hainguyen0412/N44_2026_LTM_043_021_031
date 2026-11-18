# API web local

Backend: `web_client/src/main.cpp`. Chỉ bind IPv4 loopback; mặc định
`http://127.0.0.1:8080`. Một instance có tối đa một kết nối TCP gateway.
Không có endpoint chạy shell hay phát gói MAVLink tùy ý.

## Khởi tạo và quyền

1. GET `/api/bootstrap` lấy `csrf`, map version và đường dẫn cấu hình.
2. Mọi POST dùng JSON, `Content-Type: application/json` và
   `X-CSRF-Token: <csrf>`. Host/Origin phải khớp URL local đang phục vụ.
3. POST `/api/connect` với `{"role":"controller"}` trả
   `controller_token`; gửi token trong `X-Controller-Token` cho các thao tác.
   Observer dùng `{"role":"observer"}`.
4. Tab controller POST `/api/lease` mỗi 4 giây; server hết quyền sau 15 giây.
   Token không ghi vào log. Đây là khóa thao tác local giữa các tab, không phải
   dịch vụ đăng nhập hoặc API công khai trên LAN.

## Các endpoint

| Method/path | Nội dung |
|---|---|
| GET `/api/bootstrap` | CSRF và cấu hình tham chiếu |
| GET `/api/map` | Toàn bộ metadata/hình học map có version |
| GET `/api/state` | Snapshot TCP, heartbeat, telemetry, command, mission, sự kiện |
| GET `/api/events` | SSE `data: <snapshot JSON>`, khoảng 5 lần/giây |
| POST `/api/connect` | Nối hoặc chuyển vai trò; không chiếm quyền tab khác |
| POST `/api/disconnect` | Đóng TCP; controller cần đúng token sở hữu |
| POST `/api/lease` | Gia hạn quyền của tab |
| POST `/api/commands` | `command`: ARM, DISARM, TAKEOFF hoặc LAND; có `request_id` |
| POST `/api/missions/validate` | Kiểm tra draft, map, Home, giới hạn và khối nhà; không upload |
| POST `/api/missions/upload` | Draft cùng `request_id`; upload rồi tự đọc lại |
| POST `/api/missions/readback` | `request_id`; đọc lại khi disarmed |
| POST `/api/missions/start` | `request_id` và `upload_request_id` của đúng lượt đã verified |
| GET `/api/operations/<id>` | Tra cứu yêu cầu của owner hiện tại; 404 nếu không có |

HTTP 202 chỉ có nghĩa runtime đã tiếp nhận yêu cầu. ACK và trạng thái thực phải
đọc từ snapshot/SSE. Yêu cầu trùng cùng owner/ID/nội dung trả lại lần tiếp nhận
cũ; dùng lại ID cho nội dung khác trả 409. Cache tối đa 256 yêu cầu trong process.
Trình duyệt không tự retry POST điều khiển khi fetch lỗi.

Tra cứu operation trả thông tin command/mission còn khớp ID. Nếu dữ liệu hiện tại
đã chuyển sang yêu cầu khác, `superseded=true`; lịch sử đầy đủ xem log CSV.
Không dùng endpoint này như một cơ sở dữ liệu kết quả lâu dài.

## Draft

```json
{
  "schema_version": 1,
  "map_id": "dut_campus_v2",
  "map_version": "<version từ GET /api/map>",
  "revision": 1,
  "takeoff_altitude": 10,
  "points": [{"x": 205, "y": 240, "alt": 10, "hold": 0}]
}
```

Tọa độ dùng pixel ảnh gốc, độ cao tính theo Home PX4. Giới hạn: 1–97 waypoint,
tổng tối đa 100 item tùy cấu hình; độ cao 2–100 m, hold 0–60 s. Backend bổ sung
TAKEOFF, điểm tiếp cận trên Home và LAND. Trường `_home` nội bộ được backend ghi lại từ telemetry,
không tin giá trị người gọi gửi.

Snapshot có độ tươi riêng của telemetry và heartbeat. `connected=true` chỉ xác
nhận TCP; cần `fresh=true` để điều khiển. `request_error` là lỗi phát hiện tại
worker sau lúc API đã tiếp nhận.

Giới hạn HTTP: body 64 KiB, 8 worker, hàng đợi 32, tối đa 3 SSE subscriber.
Các giới hạn hiện là hằng số trong server; port/asset root dùng CLI, không có
`web.example.json` riêng trong phiên bản này.

Bootstrap có max_waypoints = max_items - 3. Snapshot có map_home; khi mission đã gắn mốc, map_home dùng Home lúc biên dịch, giúp UI không vẽ lại đích đã upload theo sai lệch nhỏ của telemetry.
