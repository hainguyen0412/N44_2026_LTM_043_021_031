# Tắt ghi log file cho bản chạy thường

Gateway, web_client và c_client trong bin không ghi file log và không có
tùy chọn runtime bật lại. Các CMD chạy gateway/web/observer/controller dùng đúng
ba executable này. Lỗi, ACK, telemetry và sự kiện mission trên màn hình vẫn hoạt động.
Thay đổi không xóa log cũ.

Kiểm chứng: 21/21 CTest đạt, không skipped. Bằng chứng:
output/test_results/no_file_logging.xml và no_file_logging_binaries.json.
logging_integration chạy chính ba executable phát hành với PX4 giả lập localhost:
nhận telemetry, upload/readback mission, ARM, disconnect/shutdown; xác nhận không
tạo thư mục logging ngay cả khi truyền run-id. Cả --log on và --log off đều bị
từ chối vì tùy chọn đã bỏ.

CsvLogger của phần B vẫn có unit test. Những ca hồi quy CSV/metrics lịch sử dùng
gateway/client được instrument riêng dưới build/test_support/bin, chỉ tạo khi
BUILD_TESTING bật. Chúng không nằm trong gói Windows. Chạy CTest/công cụ đo kiểm
có thể tạo bằng chứng thử nghiệm, khác với chạy chương trình thường.

contracts.hpp giữ nguyên SHA256
C58FED5058181115E15DFE3066C166959ECD836D74D951F69905B338FEA821F1.

Chưa kiểm thử lại chuyến bay PX4/Gazebo thật; kiểm thử mạng dùng localhost.
Tài liệu kết quả web trước thay đổi này mô tả phiên bản còn ghi log.
