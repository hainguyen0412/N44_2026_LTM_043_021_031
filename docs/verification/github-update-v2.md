# Kiểm tra bản cập nhật v2 trước khi đưa lên GitHub

Ngày 18/11/2026, Windows 64 bit, MinGW UCRT64.

- Build sạch từ bản sao mã nguồn đã đối chiếu với Project_CK_LTM_v2.
- CTest đạt 21/21: 9 bài unit và 12 bài tích hợp localhost, gồm gateway,
  web/mission, Home và hạ cánh, ACK/retry, console, không ghi log và B0/B1/B2.
- Gói nguồn và Windows được tạo thành công. Manifest gói Windows đối chiếu
  đủ 424/424 file tại thời điểm kiểm tra; web phục vụ HTML/JS/CSS/map đúng
  khi giải nén sang đường dẫn có dấu cách, không tự nối TCP khi vừa mở.
- Git giữ source, cấu hình mẫu, thư viện/giấy phép, fixture, asset map/Gazebo
  và tài liệu Markdown/sơ đồ gốc. Không đưa executable, build/output, log,
  cấu hình riêng, ZIP, bản backup hoặc bản xuất báo cáo trùng nội dung vào Git.
- Các liên kết tương đối trong tài liệu đã đối chiếu với cây file đưa lên Git.

Kiểm thử trên dùng peer giả lập ở localhost. Kết quả bay PX4/Gazebo thực tế
và kết quả GitHub Actions được xác nhận riêng, không suy ra từ các test này.
