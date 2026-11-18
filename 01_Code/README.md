# MAVLink Telemetry Gateway và Packet Inspector

Dự án C++17 nhận MAVLink 2 qua UDP, kiểm tra khung và chuyển tiếp tới client TCP.
Web local hiển thị telemetry, điều khiển và lập mission trên map campus DUT.
Windows 64 bit chạy gateway/client; máy Ubuntu riêng chạy PX4 1.15.4 và Gazebo.

Các lệnh trong tài liệu này chạy từ `01_Code` trong repository hoặc từ thư mục
chứa `CMakeLists.txt` của gói đã giải nén.

## Mở web

Trong `scripts/windows/`, mở hai file và giữ hai cửa sổ chạy:

1. `CHAY_GATEWAY_MISSION.cmd`
2. `CHAY_WEB_CLIENT.cmd`

Mở **http://127.0.0.1:8080**. Có thể xem map khi chưa có PX4; nhận telemetry và
điều khiển cần PX4 gửi UDP tới Windows.

Chạy trực tiếp bằng hai cửa sổ PowerShell tại gốc project:

```powershell
.\bin\gateway.exe --config config/gateway.mission.example.json
```

```powershell
.\bin\web_client.exe --config config/gateway.mission.example.json
```

Gateway/web/console **không ghi log file và không có tùy chọn bật**. Lỗi, ACK và
tiến độ vẫn hiện trên màn hình. Dừng từng chương trình bằng Ctrl+C.

Hướng dẫn: [web và demo hai máy](docs/guides/web-client-running.md).

## Cây thư mục

```text
01_Code/                      # Gốc mã nguồn và các lệnh bên dưới
├── bin/                       # gateway.exe, web_client.exe, c_client.exe
├── config/                    # Cấu hình chung, mission và cấu hình máy
├── apps/                      # Entry point gateway và console
├── include/mavlink_gateway/    # Giao diện và kiểu dữ liệu chung
├── src/
│   ├── gateway/               # A: UDP/TCP, client, định tuyến, queue
│   ├── processing/            # B: parser, CRC, decoder, Inspector
│   ├── logging/               # B: thư viện CSV cho kiểm thử/đo kiểm
│   ├── client/                # C: runtime, lệnh/ACK và mission
│   └── config/                # Kiểm tra cấu hình JSON
├── web_client/
│   ├── frontend/              # HTML/CSS/JS và map 2D
│   ├── src/                   # HTTP/SSE và biên dịch lộ trình
│   └── tools/                 # Export map từ dữ liệu Gazebo
├── simulation/gazebo/         # World, model, mesh, texture campus
├── scripts/windows/           # CMD chạy, build và kiểm thử
├── tests/                     # Unit/integration và fixture
├── tools/                     # Fake peer, replay, sinh mẫu, đóng gói
├── examples/                  # Source demo parser B
├── third_party/               # Thư viện đã khóa phiên bản và giấy phép
├── docs/
│   ├── guides/                # Hướng dẫn chạy và PX4
│   ├── design/                # Kiến trúc, giao ước, API, mission, sơ đồ
│   ├── planning/              # Kế hoạch triển khai
│   └── references/            # Đề cương gốc
├── CMakeLists.txt
└── CMakePresets.json
```

`bin/` là bản chạy. `build/` chứa cache, object, công cụ và executable kiểm thử;
`output/` chứa dữ liệu kiểm thử/đóng gói tạm. Hai thư mục này chỉ sinh lại khi
build hoặc dùng công cụ phát triển, không cần để mở gateway/web/console.

Giữ nguyên `contracts.hpp`. Code web nằm riêng trong `web_client/`;
frontend/config/map cần nằm cùng project khi chạy executable.

## Console và cấu hình

```powershell
.\bin\c_client.exe --config config/gateway.mission.example.json --role observer
.\bin\c_client.exe --config config/gateway.mission.example.json --role controller
```

Mỗi lệnh dùng cửa sổ riêng. Một controller tối đa: ngắt controller web trước khi
nối console điều khiển. Lệnh console: `arm`, `disarm`, `takeoff`, `land`, `help`,
`quit`. `takeoff N` dùng N mét AMSL, không phải tăng thêm N mét.

Cổng mặc định: UDP **14550**, observer **5760**, controller **5761**, HTTP **8080**.
Cấu hình riêng hiện có được giữ ở `config/px4.local.json`; truyền cùng file config
cho các chương trình nếu dùng cấu hình riêng.

## Build và kiểm thử

Chạy executable trong `bin/` không cần Python/CMake. Để sửa code và build, cần
g++ MinGW UCRT64, CMake 3.25+, Ninja; Python 3.9+ cho kiểm thử/công cụ. Thư viện
C++ đã nằm trong `third_party/`.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test.ps1
```

Hoặc nhấp `scripts/windows/BUILD.cmd`, `TEST.cmd`. Test tự build nếu cache chưa có.
Dừng executable trước khi build đè. Unit/integration dùng localhost giả lập;
bản thử CSV/metrics riêng ở `build/test_support/bin`, không bật file log trên
ba chương trình trong `bin/`.

`CHAY_DEMO.cmd` là công cụ đo kiểm, sẽ build lại công cụ nếu cần và tạo bằng chứng.
Chạy thông thường dùng các CMD gateway/web/observer/controller. Demo fake peer
không cần Ubuntu được mô tả trong hướng dẫn web.

## GitHub và đóng gói

Trong repository, CI Windows nằm ở `.github/workflows/` ngay tại gốc.
Git theo dõi mã nguồn, cấu hình mẫu, fixture, tài nguyên và phụ thuộc.
`bin/`, `build/`, `output/`, cấu hình `*.local.json` và cache là dữ liệu cục bộ.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package.ps1 -WithBinaries
```

Gói Windows lấy đúng 3 executable từ `bin/` cùng mã nguồn, cấu hình, tài nguyên
và tài liệu của gốc CMake. Báo cáo, phụ lục và workflow nằm trong repository.
ZIP và checksum sinh vào `build/releases/`. Trong bản giải nén, dùng
`python -B tools/verify_package.py` để kiểm manifest.

## Tài liệu

Xem [mục lục](docs/README.md), [kiến trúc A/B/C](docs/design/architecture.md) và
[hướng dẫn web](docs/guides/web-client-running.md).
