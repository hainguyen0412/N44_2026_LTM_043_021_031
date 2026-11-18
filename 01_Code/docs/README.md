# Tài liệu dự án

- **guides/**: cách chạy và kiểm thử PX4.
- **design/**: kiến trúc A/B/C, giao ước, API, map, mission; sơ đồ ở design/diagrams.
- **planning/**: kế hoạch triển khai và các bước chờ xác minh trên Ubuntu.
- **references/**: đề cương gốc.

Các bản ghi kiểm chứng nằm tại `03_Phu_luc/Ket_qua_kiem_thu/` từ gốc repository.

Điểm bắt đầu:

- [Mở web và chạy demo](guides/web-client-running.md).
- [Cách chạy chương trình](guides/running.md).
- [Thiết lập PX4](guides/px4-testing.md).
- [Giao ước tích hợp](design/integration-contract.md).
- [API web](design/web-api.md), [mission](design/mission-protocol.md).
- [Tọa độ campus](design/campus-map.md).
- [Sơ đồ draw.io](design/diagrams/Luong_xu_ly_MAVLink_Gateway.drawio).

Executable chạy thường hiện ở bin/ và không ghi log file. Hướng dẫn Markdown bám cây thư mục hiện tại.
Git chỉ giữ sơ đồ gốc .drawio; xuất PNG/SVG từ draw.io khi cần chèn báo cáo.
