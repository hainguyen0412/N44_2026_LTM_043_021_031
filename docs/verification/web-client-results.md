# Kết quả triển khai web client và mission

**Cập nhật mới hơn:** [bản chạy thường đã tắt hẳn file log](no-file-logging.md). Các kết quả và mô tả log bên dưới là bản triển khai ban đầu.

Ngày 16/11/2026. Phạm vi: source trong Project_CK_LTM_v1, Windows MinGW UCRT64,
PX4 giả lập giao thức tại localhost. **Chưa nghiệm thu chuyến bay trên PX4 thật
hoặc bố cục/thao tác trực tiếp bằng trình duyệt.**

## Chức năng đã triển khai

- Module web tách ở `web_client/`, có executable riêng và frontend offline.
- Runtime nhận telemetry từ gateway, dùng parser/Inspector/logger hiện có;
  quyền Observer/Controller và một tab sở hữu controller.
- ARM, DISARM, TAKEOFF, LAND qua CommandManager; hiển thị ACK và trạng thái riêng.
- Export campus 2D từ dữ liệu model/generator; WGS84/ENU, Home và version map.
- Soạn, kéo, sắp xếp, lưu/mở waypoint; zoom/pan; upload, đọc lại, đối chiếu,
  ARM/Start và theo dõi current/reached.
- Gateway schema 2 kiểm whitelist mission, nguồn/đích/phiên; schema 1 vẫn chạy.
- CSRF/Origin, idempotency, lease, queue/subscriber có giới hạn.
- Script chạy, tài liệu Markdown/Word, công cụ fake peer và đóng gói.

Console giữ vòng lặp nhập/nhận cũ; web dùng ClientRuntime mới cùng các thư viện
client. Chưa chuyển toàn bộ console sang runtime như P07.2 trong kế hoạch.
HTTP dùng CLI và hằng số có tài liệu thay cho web config JSON riêng.

## Kiểm thử thực tế

| Kiểm tra | Kết quả và bằng chứng |
|---|---|
| Baseline trước thay đổi | 16/16 CTest; build/windows-release/output/test_results/web_baseline_allowed.xml |
| Bộ đầy đủ sau thay đổi C++ | **20/20**, không skipped; output/test_results/web_release_final.xml |
| Sau sửa hiển thị mất backend và build lại fingerprint | Web mission + client config **2/2**; output/test_results/web_asset_final.xml |
| Web/gateway + fake peer độc lập | **11/11 kịch bản** trong results.json của web_mission_integration |
| Unit mission mới | 7 nhóm, gồm CRC, sai nguồn/đích, request lặp, mismatch, timeout và Start không retry |
| Map | Cổng, chiều trục, góc map, round-trip, draft và đoạn bay qua nhà |
| Schema | V1/V2 hợp lệ; key lạ, command trùng/tắt, giới hạn sai đều bị từ chối |
| Console và gateway trước đó | Hồi quy lệnh/ACK/retry/params/metrics/queue và demo B0/B1/B2 đạt |
| Source ở đường dẫn mới có dấu cách | Configure/build sạch 7 runtime executable đạt; fingerprint khớp source cuối |
| Frontend tĩnh | node --check, ID HTML/selector JS, XML world, re-export map không đổi hash đều đạt |
| Gói Windows giải nén ở đường dẫn khác | Manifest, CLI help, HTTP frontend/map/SVG, không tự nối TCP, shutdown đạt |
| Word | 4 trang đã render bằng Microsoft Word và kiểm tra từng trang |

11 kịch bản web: happy path, mất request, mất ACK upload, sai target,
readback mismatch, PX4 từ chối, timeout, quyền gateway, mission bị tắt,
mất ACK Start và PX4 ARMED giữa upload. Lệnh Start chỉ được phát một lần trong
ca mất ACK; không dùng việc fake peer kết thúc làm bằng chứng chuyến bay thật.

Lượt kiểm thử đầu tiên phát hiện phiên controller đang đóng vẫn bị tính vào
giới hạn controller. Đã sửa chỉ đếm phiên đang hoạt động; ca reconnect và bộ
kiểm thử đầy đủ sau sửa đạt. Các lượt bị sandbox chặn loopback không được dùng
làm kết quả chức năng; lượt xác minh nêu trên đã được phép dùng localhost.

Hash binary/source và đường dẫn bằng chứng cụ thể nằm ở
[`web-client-results.json`](web-client-results.json).

## Các mục chưa kiểm chứng

- Computer Use đã dừng vì không xác định chắc URL Edge để kiểm tra quyền thao tác.
  Không dùng công cụ khác để vượt chặn. Chưa kiểm visual, zoom/pan/drag/save/open,
  bố cục màn hình hẹp hoặc workflow đầy đủ bằng browser thực.
- Chưa chạy bản web với PX4 1.15.4/Gazebo trên máy Ubuntu; chưa đo vị trí cổng,
  hướng trục, Home/EKF, readback chuẩn hóa firmware và chuyến bay mission.
- Chưa kiểm mọi tổ hợp subscriber chậm, queue đầy, GCS khác ghi đồng thời và
  mất mạng giữa chuyến bay. Không diễn giải ma trận fake peer là bao phủ toàn bộ.
- Workflow CI đã bổ sung target/test/package; chưa có kết quả GitHub Actions.
  Checkout hiện không có .git; lần triển khai này không commit/push/release tag.
- Chưa đo benchmark web so với B0/B1/B2. Browser subscriber không phải TCP client
  riêng để đưa vào phép so sánh số client.

Kiểm nhà có buffer cố định và giới hạn hình học đã biết, không phải tránh mọi
vật cản. Một GCS khác vẫn có thể sửa mission sau lần đọc lại trước Start.
Đóng tab không tự LAND/DISARM.

## Chạy và gửi kết quả

Xem [hướng dẫn chạy](../guides/web-client-running.md). Có thể thử fake peer trên
Windows trước; sau đó nhóm C chạy SITL theo cùng world và gửi log, phiên bản,
IP, ảnh/video và thứ tự thao tác. P13/P14 trong kế hoạch tiếp tục chờ kết quả đó.
