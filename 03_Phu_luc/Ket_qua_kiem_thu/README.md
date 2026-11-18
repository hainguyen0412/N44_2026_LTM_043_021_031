build bằng CMake/Ninja trên Windows MinGW UCRT64. `test_summary.json` ghi môi trường, phạm vi, kết quả và SHA256 binary.

| Kiểm tra | Kết quả |
|---|---|
| Configure/build CMake tại project chính | Đạt |
| Build từ tập file Git theo dõi, trong đường dẫn mới có dấu cách | Đạt |
| Unit CTest tại bản nguồn sạch | 6/6, gồm fixture độc lập, processing/logger, gateway, command manager, metrics và send queue |
| Parser/logger B | 19/19 nhóm |
| Command Manager | 102/102 kiểm tra |
| Metrics và send queue | 6/6 và 7/7 |
| Gateway integration | 7/7 nhóm |
| Retry / ACK / config / fake gateway / client metrics | 4/4, 48/48, 31/31, 22/22, 10/10 |
| Demo gateway/client thật với UDP giả lập | Đạt B0/B1/B2 |
| CMD trong scripts/windows mở từ cwd ngoài, source path có dấu cách | Đạt B0/B2 |
| Git loại build/output/executable/cache; giữ fixture và workflow | Đạt |
| Thư viện upstream đi qua Git giữ nguyên byte | Đạt, có quy tắc attributes riêng cho third_party |

15 test CTest đã được xác minh. Lượt đầy đủ đầu tiên đạt 14/15, phát hiện oracle fixture còn đọc metadata theo cwd. Sau khi sửa đường dẫn theo vị trí script, test đó đạt khi chạy lại; toàn bộ 6 unit tests cũng đạt trong bản nguồn sạch. Các bài mạng còn lại đã đạt ở lượt đầy đủ.

`contracts.hpp` giữ đúng bản chung của B, SHA256 `C58FED5058181115E15DFE3066C166959ECD836D74D951F69905B338FEA821F1`. Báo cáo này tổng hợp kiểm thử chức năng cục bộ; chưa chạy PX4 SITL hoặc benchmark 60 giây × 3 lượt mỗi mode. Workflow GitHub đã cấu hình; cần đưa repository lên GitHub để có kết quả runner thực tế.

Raw log/build và source checkout dùng kiểm thử được giữ ngoài repo trong output của workspace gốc. Không đưa các tệp sinh ra này vào Git.

## Kiểm tra sau khi dọn cây thư mục

Bản chạy chính chuyển sang bin/ gồm gateway.exe, web_client.exe và c_client.exe.
Build/test/tool vẫn nằm trong build khi được sinh lại. Các đường dẫn script,
package và hướng dẫn Markdown đã cập nhật; contracts.hpp giữ nguyên hash.

Bộ kiểm thử sau thay đổi có 20/21 bài đạt lần đầu; công cụ demo B2 đọc trúng CSV
header chưa ghi xong. Đã sửa đọc snapshot tới dòng hoàn chỉnh và chạy lại cả
B0/B1/B2, 3/3 đạt. Tất cả 21 bài đã được xác minh ở các lượt trên. Bản giải nén
ở đường dẫn có dấu cách đạt manifest, HTTP/asset/map và shutdown.

Theo yêu cầu dọn của người dùng, build cache, ZIP tạm và toàn bộ output/log được
xóa sau kiểm tra. Kết quả nêu trên là ghi nhận đã chạy, không phải cam kết giữ
raw log sau khi dọn. Kiểm thử bay PX4/Gazebo thật vẫn chưa được thực hiện lại.
