# MAVLink Telemetry Gateway và Packet Inspector

Đồ án cuối kỳ môn Lập trình mạng, nhóm 16. Gateway C++17 nhận MAVLink 2 qua UDP,
kiểm tra khung và phân phối telemetry cho các client TCP. Console hỗ trợ vai
trò observer và controller. Web local hiển thị telemetry, điều khiển qua Gateway
và hỗ trợ lập, nạp, đọc lại và xác minh mission trên bản đồ khuôn viên DUT.

## Cấu trúc repository

| Thư mục | Nội dung |
|---|---|
| [01_Code](01_Code/README.md) | Mã nguồn C++ và web, cấu hình mẫu, CMake, script, fixture và tài nguyên Gazebo |
| [02_Bao_cao](02_Bao_cao/BAO_CAO_CUOI_KI_NHOM16.docx) | Báo cáo cuối kỳ Word |
| [03_Phu_luc](03_Phu_luc/Ket_qua_kiem_thu/README.md) | Các bản ghi kiểm chứng theo phiên bản |
| [.github/workflows](.github/workflows/windows-ci.yml) | Build, CTest và kiểm tra gói trên Windows |

## Build và kiểm thử trên Windows

Cần Windows 64 bit, g++ MinGW-w64 UCRT64, CMake 3.25 trở lên, Ninja và Python
3.9 trở lên. Đưa các công cụ vào `PATH` rồi mở PowerShell tại gốc repository.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\01_Code\scripts\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\01_Code\scripts\test.ps1
```

Ba chương trình dùng thường nằm trong `01_Code/bin/`. Executable có đo kiểm
nằm trong `01_Code/build/test_support/bin/`. Bộ CTest chạy với fixture và peer
giả lập trên localhost. Kết quả của các bài tích hợp được ghi vào
`01_Code/output/test_results/`.

Có thể xem danh sách bài và chạy CTest trực tiếp từ gốc mã nguồn.

```powershell
Set-Location .\01_Code
ctest --preset windows-release -N
ctest --preset windows-release
```

Xem [hướng dẫn chạy chi tiết](01_Code/docs/guides/running.md) và
[hướng dẫn đóng gói](01_Code/README.md#github-và-đóng-gói).

## Chạy Gateway và web client

Từ gốc repository, mở mỗi lệnh trong một cửa sổ PowerShell riêng.

```powershell
& .\01_Code\scripts\windows\CHAY_GATEWAY_MISSION.cmd
```

```powershell
& .\01_Code\scripts\windows\CHAY_WEB_CLIENT.cmd
```

Mở `http://127.0.0.1:8080`. Bản đồ có thể mở trước khi nhận dữ liệu PX4.
Cấu hình mẫu dùng UDP 14550 cho PX4, TCP 5760 cho observer và TCP 5761 cho
controller. Chỉ một controller hoạt động tại một thời điểm. Gateway, console
và web client dùng thường hiển thị sự kiện trên màn hình và không ghi log tệp.

Để thử trên một máy Windows với cổng loopback riêng, xem
[hướng dẫn web với peer giả lập](01_Code/docs/guides/web-client-running.md#2-thử-ngay-tại-windows-bằng-dữ-liệu-giả-lập).

## Kết nối PX4 SITL và Gazebo

Trên Ubuntu, dùng PX4 1.15.4 với Gazebo, model x500 và world `dut_campus`.
World và model nằm tại `01_Code/simulation/gazebo/`. Cấu hình luồng MAVLink
UDP từ SITL đến địa chỉ IPv4 của máy Windows đang chạy Gateway, cổng 14550
theo cấu hình mẫu. Vị trí cài world, model và lệnh chạy có trong
[hướng dẫn Ubuntu](01_Code/docs/guides/web-client-running.md#ubuntu).

## Nhóm thực hiện

| Thành viên | MSSV |
|---|---|
| Phạm Thị Hồng Nhung | 106230021 |
| Nguyễn Đức Hải | 106230043 |
| Đinh Tiến Trung | 106230031 |

Giảng viên hướng dẫn là TS. Nguyễn Văn Hiếu.
