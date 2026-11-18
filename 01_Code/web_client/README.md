# Web client local — MAVLink campus DUT

Thư mục này tách mã web khỏi mã gateway. Backend là C++17 và dùng thư viện client chung của dự án; browser không kết nối TCP/MAVLink trực tiếp và không chạy lệnh trong console.

## Các phần

| Đường dẫn | Trách nhiệm |
|---|---|
| `frontend/index.html` | Bố cục tiếng Việt |
| `frontend/styles.css` | Giao diện desktop/mobile, theme |
| `frontend/app.js` | Map, waypoint, REST, SSE, hiển thị kết quả |
| `frontend/assets/` | Map JSON có version và bản SVG export |
| `src/main.cpp` | HTTP API, quyền controller theo tab, idempotency và SSE |
| `src/campus_map.*` | WGS84/ENU và biên dịch/kiểm tra mission |
| `tools/export_campus.py` | Tái tạo map từ dữ liệu Gazebo, không thực thi generator |
| `CMakeLists.txt` | Target `web_client`, được build từ CMake gốc |

Phần dùng chung nằm ở `src/client/client_runtime.*`, `mission_manager.*`, `mavlink_builder.*` và `src/processing/mission_decoder.cpp`. Gateway chỉ thay đổi phần cấu hình/kiểm tra/định tuyến cần cho mission. `contracts.hpp` giữ nguyên.

## Build

Tại `01_Code` trong repository hoặc thư mục chứa `CMakeLists.txt` của gói
đã giải nén, sau khi có toolchain theo README:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --target gateway web_client
```

Executable tạo ở `bin/web_client.exe`. Không chạy CMake riêng trong thư mục `web_client` vì module này liên kết với các thư viện C++ của project.

## Chạy

Xem [hướng dẫn từng bước](../docs/guides/web-client-running.md).

```powershell
.\bin\web_client.exe --config config/gateway.mission.example.json
```

Mở `http://127.0.0.1:8080`. Chương trình chưa nối gateway hoặc gửi lệnh cho đến khi người dùng thao tác trên web. Đổi cổng bằng `--web-port 8081`.

Không cần Python, Node.js hoặc Internet để chạy executable và giao diện. Python được dùng cho công cụ sinh map, fake peer và các kiểm thử phát triển.

## Tái tạo map

```powershell
python -B web_client/tools/export_campus.py
```

Đầu vào mặc định là `simulation/gazebo`; đầu ra là `frontend/assets`. Thay đổi dữ liệu sẽ tạo map version mới, các draft cũ phải được kiểm tra/chuyển đổi thay vì âm thầm dùng lại.

## Tình trạng thực tế

Chức năng được kiểm tra ở localhost với web/gateway executable và đối tác MAVLink giả lập độc lập. Việc chạy chuyến bay trên PX4 1.15.4/Gazebo campus cùng kiểm tra UI trực tiếp được ghi riêng trong báo cáo bàn giao; không coi code đã build là bằng chứng bay thành công.
