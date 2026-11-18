# Kiến trúc và phân công

## Luồng dữ liệu

```mermaid
flowchart LR
    PX4[PX4 SITL trên Ubuntu] -->|Telemetry UDP| Network[A: Network I/O]
    Network --> Parser[B: Parser và CRC]
    Parser --> Inspector[B: Decoder và Inspector]
    Parser --> Clients[A: Client Manager]
    Clients -->|MAVLink TCP nguyên byte| Observer[C: Observer]
    Clients -->|MAVLink TCP nguyên byte| Controller[C: Controller]
    Controller -->|Heartbeat và COMMAND_LONG| Uplink[A: Kiểm tra vai trò và đích]
    Uplink -->|UDP tới endpoint đã khóa| PX4
    Inspector --> Logger[B: CSV Logger]
    Controller --> Commands[C: Command Manager và ACK]
```

## Trách nhiệm và vị trí

| Phần | Vị trí | Đầu ra |
|---|---|---|
| A | `src/gateway` | UDP/TCP, phân quyền, chuyển raw frame, hàng đợi và vòng đời client |
| B | `src/processing`, `src/logging`, API trong `include/mavlink_gateway` | Khung hợp lệ, dữ liệu giải mã, Inspector SEQ, CSV và summary |
| C | `src/client`, `tools`, kiểm thử trong `tests` | Console, lệnh/ACK/retry, dữ liệu giả lập và thống kê client |
| Dùng chung | `src/config`, `include/mavlink_gateway/config.hpp`, `config` | JSON đã kiểm tra và cấu hình hiệu lực |

`apps/gateway_main.cpp` và `apps/client_main.cpp` ghép các module thành chương trình. Header nội bộ gateway/client nằm cạnh phần triển khai; header dùng chung nằm trong `include`. Chỉ có một bản contracts.hpp của nhóm.

## Build và phụ thuộc

CMake tạo thư viện nội bộ `mgw_processing`, `mgw_logging`, `mgw_config`, `mgw_gateway`, `mgw_transport`, `mgw_client`; các executable link thư viện cần dùng. Fixture generator dùng MAVLink C trực tiếp, không gọi parser B để giữ phép đối chiếu độc lập.

Thư viện ngoài nằm trong `third_party`, được lưu cùng repository để build offline sau khi có toolchain. Không tự nâng phiên bản MAVLink/JSON khi configure.

## Ranh giới nền tảng

Gateway hiện dùng Winsock và Windows process metrics. Preset hỗ trợ Windows MinGW UCRT64; CMake báo rõ khi chọn nền tảng khác. Chưa triển khai gateway Linux. PX4 có thể chạy trên máy Ubuntu riêng qua UDP; laptop dual boot không chạy đồng thời hai hệ điều hành.

## Đầu vào và dữ liệu sinh ra

Mã nguồn, fixture, cấu hình mẫu, sơ đồ và tài liệu là đầu vào trong Git. `build` chứa cache, thư viện và executable; `output` chứa log và bằng chứng chạy. Cấu hình riêng của máy dùng `config/*.local.json`. Các dữ liệu sinh ra và cấu hình riêng được Git bỏ qua.

## Web client và mission

```mermaid
flowchart LR
    Browser[Web HTML/CSS/JS local] <-->|HTTP JSON + SSE| Web[web_client: HTTP và quyền tab]
    Web --> Runtime[ClientRuntime + CommandManager + MissionManager]
    Runtime <-->|TCP MAVLink 2| Gateway[gateway: whitelist / queue / phiên]
    Gateway <-->|UDP MAVLink 2| PX4[PX4 + Gazebo trên Ubuntu]
    Campus[Map JSON + SVG từ dữ liệu campus] --> Browser
    Campus --> Compiler[CampusMap: ENU/WGS84 và kiểm mission]
    Web --> Compiler
    Compiler --> Runtime
```

`web_client/` tách riêng frontend, HTTP backend, map compiler và exporter. Runtime
C++ nằm ở `src/client`, dùng chung parser, builder, command manager, send queue và
logger. Console hiện giữ vòng lặp nhập/nhận riêng; chưa chuyển toàn bộ
`apps/client_main.cpp` sang ClientRuntime để hạn chế phạm vi thay đổi CLI.

Uplink bổ sung mission chỉ khi schema 2 bật chức năng. Không sửa `contracts.hpp`;
Home, landed state và mission dùng kiểu/decoder mở rộng riêng. Server web chỉ
bind loopback, một tab giữ quyền điều khiển, các tab khác đọc snapshot. Browser
không phát raw MAVLink hoặc ghi stdin của console.

Xem [API](web-api.md), [mission](mission-protocol.md), [map](campus-map.md) và
[hướng dẫn chạy](../guides/web-client-running.md). Kết quả local cùng phần chưa kiểm chứng
được ghi trong `03_Phu_luc/Ket_qua_kiem_thu/web-client-results.md` tính từ gốc repository.

## Chính sách file log của bản chạy thường

Gateway, web và console đã tắt ghi log file; không có tham số runtime bật lại.
UI vẫn giữ sự kiện gần đây trong bộ nhớ và hiển thị ACK/lỗi/tiến độ. Parser,
Inspector, định tuyến và mission không phụ thuộc vào việc có file log.

CsvLogger của phần B được giữ làm thư viện và có unit test. Với BUILD_TESTING,
CMake tạo gateway/client đo kiểm riêng trong build/test_support/bin. Các bài
kiểm thử metrics/CSV cũ dùng hai executable này; gói Windows chỉ lấy executable
trong bin. Test logging_integration dùng chính gateway/web/console phát
hành để xác nhận không có thư mục log ngay cả sau telemetry, upload, ARM và
shutdown. Việc chạy bộ test có thể tạo bằng chứng test; mở web chạy thường
không tạo các file đó.
