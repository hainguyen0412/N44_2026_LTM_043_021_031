# Sửa Home và tiếp cận hạ cánh

Ngày 17/11/2026, Project_CK_LTM_v2.

- Hai phép so sánh Home dùng chung home_reference.hpp: khoảng cách ngang 1 m,
  sai lệch cao 2 m; so với Home dùng biên dịch mission. Sai lệch cộng dồn vẫn bị
  phát hiện. Thông báo lỗi có độ lệch theo mét.
- Compiler thêm waypoint trên Home ở độ cao waypoint cuối, dừng 1 giây,
  acceptance radius 1 m; sau đó LAND. Giới hạn 100 item cho tối đa 97 waypoint
  người dùng. Kiểm footprint gồm cất cánh, tiếp cận và cột hạ tại Home thực tế.
- Độ cao relative Home được quy về gốc world; thêm dự phòng 2 m thay đổi Home
  ngoài khoảng đệm mái hiện có. Không tự tăng độ cao hoặc bỏ kiểm công trình.
- UI hiển thị điểm tiếp cận và dùng map_home gắn với mission đã tải.

Kiểm chứng:

- Unit mission: 8 nhóm đạt, gồm ngưỡng Home, khoảng cách hai trục và dữ liệu lỗi.
- Unit map: tọa độ, biên dịch, 97/98 điểm, tiếp cận 50 m qua nhà, từ chối đường
  thấp, dự phòng thay đổi Home và vật cản tại vị trí Home lệch khỏi cổng đều đạt.
- 17/17 kịch bản web/gateway/fake PX4 localhost đạt, gồm Home nhỏ khi ARM/Start,
  Home lớn theo phương ngang/cao, dịch dần và mission có điểm tiếp cận.
- Sau khi cập nhật bin: happy path, Home nhỏ/lớn khi Start, điểm tiếp cận và
  kiểm tra không ghi log file đều đạt lại.
- SHA256 executable trong bin trùng bản kiểm thử. Source fingerprint:
  66c5230b23f5a311580ad020d2c87f1e060e4dfbf8db41ab414094c028fefd52.
- contracts.hpp giữ nguyên; JavaScript qua node --check.

Đây là kiểm chứng phần mềm/giao thức localhost, chưa phải kết quả bay PX4/Gazebo.
Raw dữ liệu build/kiểm thử tạm được xóa sau khi cập nhật để giữ cây dự án gọn.
Chạy lại chương trình và tải lại mission trước khi thử; mission cũ chưa có điểm
tiếp cận mới. Các ngưỡng Home cần đối chiếu trên SITL thực dùng của nhóm.
