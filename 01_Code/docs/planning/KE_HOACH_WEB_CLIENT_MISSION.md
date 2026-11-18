# Kế hoạch hoàn thiện web client và mission cho dự án MAVLink Gateway

Ngày lập và cập nhật: 16/11/2026. Dự án làm việc: `Project_CK_LTM_v1`.

**Cập nhật 16/11/2026: đã triển khai web local, mission, map và tích hợp gateway; bộ CTest đầy đủ đạt 20/20. Chưa nghiệm thu UI trực tiếp hoặc chuyến bay PX4/Gazebo trên Ubuntu.**

Mã web được tách trong `web_client/` sau khi lập kế hoạch. Xem [cách chạy](../guides/web-client-running.md).
Bản ghi kiểm thử nằm tại `03_Phu_luc/Ket_qua_kiem_thu/web-client-results.md` tính từ gốc repository.

Các điểm triển khai khác cây dự kiến:

- `web_client/src/` chứa HTTP API và map compiler; `frontend/` chứa HTML/CSS/JS/assets; exporter nằm tại `web_client/tools/`.
- Navigation gộp vào `mission.hpp`/`mission_decoder.cpp`; builder mission mở rộng builder có sẵn để dùng cùng SEQ.
- Runtime headless dùng chung các thư viện client. Console giữ vòng lặp riêng, chưa thực hiện P07.2; hồi quy console vẫn được chạy.
- HTTP port và asset root dùng CLI; giới hạn subscriber/request và nhịp SSE là hằng số có tài liệu. Không tạo thêm web config JSON.
- Hạng mục có checkbox là đã thực hiện ở phạm vi code/local nêu trong báo cáo; chưa thay thế điều kiện qua mốc UI/SITL bằng test fake peer. Những bước yêu cầu đo thực, CI trên GitHub và publish tiếp tục để trống.


Các đường dẫn mã nguồn tương đối tính từ `01_Code`. Cây ở mục 5 là tham chiếu thiết kế ban đầu.
Đối chiếu cây triển khai thực tế trong `web_client/README.md`.

## 1. Kết quả cuối cùng cần đạt

Người dùng chạy gateway và web client trên Windows; PX4 1.15.4 cùng Gazebo chạy trên máy Ubuntu khác. Trình duyệt mở giao diện local, xem telemetry, chọn điểm trên bản đồ campus DUT, tải mission và yêu cầu PX4 thực hiện.

Luồng làm việc cuối cùng:

```text
Ubuntu: PX4 + Gazebo campus DUT
                ↕ UDP MAVLink 2
Windows: gateway.exe
                ↕ TCP MAVLink 2
Windows: web_client.exe — dùng lõi client C++
                ↕ HTTP API + Server-Sent Events
Trình duyệt: http://127.0.0.1:8080
```

Hai sản phẩm chạy độc lập: gateway thực hiện định tuyến; web client quản lý kết nối client, xử lý giao thức và phục vụ giao diện. Client console `c_client.exe` tiếp tục là một cách chạy để học, chẩn đoán và kiểm thử.

### 1.1. Phạm vi phải hoàn thiện trong bản đầu

- [ ] Giao diện tiếng Việt, chạy local và tải toàn bộ tài nguyên từ máy Windows.
- [ ] Vai trò Observer và Controller với trạng thái kết nối rõ ràng.
- [ ] Telemetry thật: pin, vị trí, độ cao, tư thế, armed/disarmed và chế độ bay.
- [ ] Các lệnh hiện có: ARM, DISARM, TAKEOFF, LAND; hiển thị ACK và kết quả cuối.
- [ ] Bản đồ campus 2D lấy từ cùng dữ liệu với mô hình Gazebo.
- [ ] Thêm, kéo, xóa, sắp xếp waypoint; đặt độ cao và thời gian dừng.
- [ ] Lưu/mở bản nháp bằng JSON có phiên bản và thông tin map.
- [ ] Mission gồm TAKEOFF → WAYPOINT… → LAND tại Home đã xác nhận.
- [ ] Tải mission, kiểm tra lại nội dung trên PX4, bắt đầu mission, theo dõi tiến độ.
- [ ] Xử lý từ chối, timeout, mất kết nối, tải dở và lộ trình đã bị thay đổi.
- [ ] Log có thể truy vết một lần chạy qua gateway, web client và PX4.
- [ ] Bộ kiểm thử, hướng dẫn demo hai máy và gói phát hành đầy đủ.

### 1.2. Giới hạn phạm vi

Không đưa joystick/bàn phím điều khiển liên tục vào đợt này; người dùng đã yêu cầu dừng chức năng đó. Các hạng mục Linux/Raspberry Pi cho gateway, bản đồ 3D tương tác, đăng nhập/cloud, quét khu vực tự động và tương thích đầy đủ file `.plan` của QGroundControl thuộc đợt mở rộng khác.

Bản đầu cho người dùng tự chọn đường bay. Đường nối trên bản đồ không phải kết quả của bộ lập đường tránh vật cản. Kiểm tra hình học các khối nhà được làm trong phạm vi dữ liệu hiện có; không tuyên bố bảo đảm tránh mọi vật thể trong mô phỏng hoặc ngoài thực tế.

## 2. Baseline lúc lập kế hoạch trước khi triển khai

| Thành phần | Hiện có | Cần bổ sung |
|---|---|---|
| A — `src/gateway/gateway.cpp` | UDP/TCP, phân vai, khóa nguồn PX4, chuyển khung; uplink nhận HEARTBEAT và COMMAND_LONG được phép | Định tuyến bản tin mission, kiểm tra loại/đích, loại bỏ gói thuộc phiên đã đóng |
| Cấu hình — `src/config/config.cpp` | Schema 1; chỉ nhận command ID 400/22/21 và đúng 5 msgid giải mã | Schema mở rộng có kiểm soát, bật/tắt mission và giới hạn tải |
| B — `src/processing/processing.cpp` | Parser/CRC, 5 thông điệp được giải mã, Inspector SEQ | Bộ giải mã mission/navigation riêng và trạng thái phục vụ UI |
| C — `src/client/command_manager.*` | ARM/DISARM, TAKEOFF, LAND; ACK, retry, timeout | MissionManager, thao tác bắt đầu mission, phối hợp với các lệnh hiện có |
| `apps/client_main.cpp` | Socket, luồng nhận, heartbeat, nhập lệnh console, log | Tách lõi dùng chung để console và web cùng sử dụng |
| Giao diện | Chỉ có bản minh họa ngoài project | Web thực, API local và dữ liệu từ runtime |
| Bản đồ | `dut_campus_v2` nằm ngoài project, có generator/model/texture | Export 2D, metadata, phép đổi tọa độ, tích hợp asset và đóng gói |
| Đóng gói | `tools/package_release.py` có danh sách thư mục cho phép cố định | Thêm `web/`, `simulation/`, executable web và kiểm tra tài nguyên |

### 2.1. Dữ liệu campus đã nhận

| Thuộc tính | Giá trị |
|---|---|
| Kích thước ảnh tọa độ gốc | 257 × 300 pixel |
| Tỉ lệ khai báo | 1,5 m/pixel ảnh gốc |
| Kích thước mặt bằng | 385,5 × 450 m |
| Cổng/gốc mô hình | Pixel `(205, 288)` → local `(0, 0)` |
| Texture đang có | 1542 × 1800 pixel; 6 lần ảnh gốc; 0,25 m/pixel texture |
| Hình học | 22 khối nhà, 4 tuyến đường; cây từ mesh |
| World | `dut_campus` |
| Mốc địa lý world | WGS84; latitude `16.074`, longitude `108.1499` |
| Elevation của gốc world | `10.0 m`; không tự đồng nhất với độ cao AMSL/Home PX4 |
| Hướng world | ENU; heading mặc định 0° |
| Campus trong world | Include `model://dut_campus`, không khai báo dịch/xoay riêng |
| Spawn được yêu cầu | `0,0,0.2,0,0,0` |

World nhận qua nội dung dán đã được lưu thành `simulation/gazebo/worlds/dut_campus.sdf` trong project. Ảnh người dùng gửi cho thấy drone tại cổng; bước đối chiếu tọa độ khi chạy thật vẫn là một hạng mục kiểm thử, chưa đánh dấu hoàn thành.

## 3. Các lựa chọn kỹ thuật cho kế hoạch

| Hạng mục | Phương án triển khai |
|---|---|
| Runtime | C++17, MinGW-w64 UCRT64 theo project hiện tại |
| Web backend | Target mới `web_client`; dùng lại lõi client, không điều khiển bằng cách đọc/ghi màn hình console |
| Frontend | HTML/CSS/JavaScript tách file; không cần build frontend hoặc Node.js khi người dùng chạy demo |
| Giao tiếp trình duyệt | HTTP JSON cho thao tác; SSE cho snapshot/sự kiện từ backend |
| Thư viện HTTP | Dự kiến `cpp-httplib`; phải qua thử build MinGW, khóa revision và lưu giấy phép trước khi dùng |
| Địa chỉ phục vụ | Chỉ bind `127.0.0.1`, mặc định cổng 8080 |
| Hiển thị telemetry | Snapshot UI khoảng 5 Hz; bộ xử lý giao thức vẫn nhận/xử lý từng khung |
| Bản đồ | SVG 2D và JSON metadata được export từ model/generator, không phụ thuộc tile online |
| Mission | MAVLink mission protocol qua gateway, chủ yếu dùng `MISSION_ITEM_INT` |
| Khung tọa độ mission | Đề xuất `MAV_FRAME_GLOBAL_RELATIVE_ALT_INT`; độ cao nhiệm vụ tính theo Home PX4 |
| Cấu hình chung | Giữ khả năng đọc schema 1; thêm schema 2 cho chức năng mission |
| Cấu hình web | File riêng cho HTTP, đường dẫn asset, map và giới hạn UI; tham chiếu cấu hình gateway/client chung |
| Bản nháp | JSON schema riêng của dự án, lưu tọa độ local/map và thông tin tham chiếu |

HTTP/SSE phù hợp với bản này vì thao tác điều khiển là yêu cầu riêng lẻ, còn telemetry là cập nhật từ server. SSE có kết nối lại ở lớp hiển thị; không dùng việc kết nối lại đó để phát lại lệnh điều khiển. Xem [MDN SSE](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events) và [tài liệu cpp-httplib](https://github.com/yhirose/cpp-httplib).

### 3.1. Ràng buộc kế thừa

- Giữ nguyên `include/mavlink_gateway/contracts.hpp` theo yêu cầu đã chốt. SHA256 hiện tại:

  ```text
  C58FED5058181115E15DFE3066C166959ECD836D74D951F69905B338FEA821F1
  ```

- Không sửa `DecodedMessage` để nhét các kiểu mission vào variant 5 thông điệp cũ. Thêm header/decoder/snapshot riêng, nhận đầu vào `FrameRecord` đã được parser kiểm tra.
- Giữ ý nghĩa B0/B1/B2 và cấu trúc log cũ; thông điệp cần thiết cho giao thức vẫn phải được xử lý khi chạy B0, giống ACK hiện tại.
- Không đổi thư viện MAVLink đã khóa phiên bản chỉ để dùng field mới trong tài liệu hiện hành. Phải đối chiếu header đang có với PX4 1.15.4.
- Gateway chuyển tiếp nguyên byte khung hợp lệ; việc đọc field để kiểm tra không được làm tái mã hóa khung khi forward.
- Một controller TCP tối đa. Các tab trình duyệt không được coi là các client TCP độc lập khi đo số client.
- Giữ source `dut_campus_v2` người dùng gửi; tích hợp bằng bản sao có nguồn gốc rõ vào project.
- Các thao tác bay trên PX4/Gazebo do người dùng chủ động chạy theo hướng dẫn. Kiểm thử tự động trước hết dùng đối tác giả lập tại localhost.

## 4. Phân công và trách nhiệm

| Người/phần | Chịu trách nhiệm chính | Đầu ra |
|---|---|---|
| A — Phạm Thị Hồng Nhung | Gateway, schema chung, whitelist, phiên và hàng đợi | Uplink/downlink mission đi đúng phiên, đúng đích; test mạng |
| B — Nguyễn Đức Hải | Parser tích hợp, decoder mở rộng, map/tọa độ, log/đối chiếu | Kiểu dữ liệu mission/navigation, bản đồ 2D, phép đổi tọa độ và bằng chứng kiểm thử |
| C — Đinh Tiến Trung | ClientRuntime, MissionManager, tích hợp CommandManager và SITL | Lõi client dùng chung, tải/đọc lại/chạy mission, kết quả trên Ubuntu |
| Phần web — tích hợp chung | HTTP API, state store, frontend và luồng thao tác | Web client hoàn chỉnh sử dụng các chức năng thật của A/B/C |

Phân công là trách nhiệm module, không tách thành ba bản dữ liệu khác nhau. B và C chốt kiểu dữ liệu trước; web dùng API của runtime, không tự viết lại bộ điều khiển MAVLink trong JavaScript.

## 5. Cấu trúc thư mục dự kiến sau khi hoàn thiện

```text
Project_CK_LTM_v1/
├── apps/
│   ├── gateway_main.cpp
│   ├── client_main.cpp
│   └── web_client_main.cpp                 # mới
├── include/mavlink_gateway/
│   ├── contracts.hpp                      # giữ nguyên
│   ├── mission.hpp                        # mới: kiểu/snapshot mission
│   └── navigation.hpp                     # mới: Home/mode/navigation bổ sung
├── src/
│   ├── config/                            # schema 1 + 2
│   ├── gateway/                           # kiểm tra/định tuyến mission
│   ├── processing/
│   │   ├── processing.cpp
│   │   ├── mission_decoder.*              # mới
│   │   └── navigation_decoder.*           # mới
│   ├── client/
│   │   ├── client_runtime.*               # mới: dùng chung console/web
│   │   ├── mission_manager.*              # mới
│   │   ├── mission_builder.*              # mới
│   │   └── command_manager.*              # mở rộng có kiểm soát
│   ├── maps/
│   │   ├── campus_map.*                   # mới
│   │   └── coordinate_transform.*         # mới
│   ├── web/
│   │   ├── http_server.*                  # mới
│   │   ├── api.*                          # mới
│   │   ├── web_config.*                   # mới
│   │   └── state_store.*                  # mới
│   └── logging/                           # log cũ + sự kiện mới
├── web/
│   ├── index.html
│   ├── styles/
│   ├── js/                                # API, map, planner, telemetry, commands
│   └── assets/maps/dut_campus/
│       ├── campus_2d.svg
│       └── campus_map.json
├── simulation/gazebo/
│   ├── gen_campus_v2.py
│   ├── worlds/dut_campus.sdf
│   └── models/dut_campus/                  # model.sdf, config, mesh, texture
├── config/
│   ├── gateway.example.json               # cấu hình cũ vẫn chạy
│   ├── gateway.mission.example.json       # mới, schema 2
│   └── web.example.json                   # mới
├── tests/
│   ├── unit/                              # bổ sung mission, maps, web
│   ├── integration/                       # giao thức + gateway + HTTP
│   └── fixtures/mission_v1/                # tách fixture cũ
├── tools/
│   ├── export_campus_map.py
│   └── fake_px4_mission.py
├── scripts/windows/CHAY_WEB_CLIENT.cmd
├── docs/                                  # kế hoạch, API, map, mission, demo
└── third_party/                           # thêm HTTP đã khóa revision/giấy phép
```

Tên file có thể điều chỉnh khi code nếu cần gộp module nhỏ; các ranh giới trách nhiệm và tiêu chí kiểm thử bên dưới vẫn phải được giữ.

## 6. Thứ tự thực hiện và mốc bàn giao

| Mốc | Nội dung | Phụ thuộc | Trạng thái |
|---|---|---|---|
| P00 | Chốt baseline và bằng chứng hiện trạng | Không | Đạt baseline 16/16 |
| P01 | Chuẩn hóa dữ liệu campus | P00 | Đã export, kiểm XML/asset |
| P02 | Phép đổi tọa độ | P01 | Unit đạt; chờ đối chiếu Gazebo |
| P03 | Giao ước dữ liệu và schema cấu hình | P00 | Đã triển khai, config test đạt |
| P04 | Decoder/builder mission, navigation | P03 | Đã triển khai, unit/protocol đạt |
| P05 | MissionManager + đối tác giả lập | P02, P04 | Đạt fake peer localhost |
| P06 | Gateway hỗ trợ mission | P03–P05 | Đã triển khai, test mạng đạt |
| P07 | Tách ClientRuntime dùng chung | P04–P06 | Runtime web đạt; console chưa chuyển vòng lặp |
| P08 | Backend HTTP/SSE local | P07 | API/SSE đã chạy; chờ kiểm UI |
| P09 | Web giám sát dữ liệu thật | P01, P02, P08 | Đã code; chưa kiểm trực tiếp browser |
| P10 | Web gửi các lệnh hiện có | P09 | API đạt; chưa nghiệm thu UI/PX4 |
| P11 | Planner và mission hoàn chỉnh trên web | P05, P06, P10 | Protocol đạt; chờ UI/SITL |
| P12 | Log và kiểm thử tích hợp toàn hệ thống | P11 | 20/20 CTest; UI chưa kiểm |
| P13 | Kiểm thử PX4/Gazebo hai máy | P12 | Chờ người dùng chạy Ubuntu |
| P14 | Sửa lỗi thực tế và nghiệm thu | P13 | Chờ kết quả P13 |
| P15 | Tài liệu, CI, đóng gói bàn giao | P14 | Tài liệu/đóng gói local; CI/publish chưa chạy |

Triển khai theo thứ tự để dễ xem diff và khoanh vùng lỗi. Có thể chuẩn bị tài liệu/API trong lúc chờ kết quả SITL, nhưng không đánh dấu P13/P14 hoàn tất bằng kết quả fake PX4.

## 7. P00 — Chốt baseline trước khi sửa chức năng

**Mục đích:** biết chính xác bản nào đang chạy và kiểm thử nào đã đạt.

- [x] P00.1 Ghi nhận cây source, cấu hình đang dùng, toolchain và phiên bản MAVLink vendored.
- [x] P00.2 Lưu fingerprint của `contracts.hpp` và các executable/source dùng cho baseline.
- [x] P00.3 Build bản hiện tại; chạy các kiểm thử có sẵn, lưu kết quả thật. Không lấy kết quả cũ làm bằng chứng cho lần sửa mới.
- [x] P00.4 Ghi nhận luồng console hiện có, gồm thao tác dừng và cách ghi log.
- [x] P00.5 Lưu bản tham chiếu giao diện campus đã duyệt, không coi HTML minh họa là frontend đã tích hợp.
- [x] P00.6 Nếu checkout có Git, ghi trạng thái thay đổi trước khi làm để không ghi đè công việc đang dở; việc tạo tài liệu kế hoạch không tự bao gồm thao tác push.

**Đầu ra:** ghi chú baseline và kết quả test trong `output/test_results/`; bản ghi tham chiếu tại `03_Phu_luc/Ket_qua_kiem_thu/` tính từ gốc repository.

**Điều kiện qua mốc:** build/test nền đạt, hoặc từng lỗi có sẵn được chỉ rõ trước khi bắt đầu refactor.

## 8. P01 — Chuẩn hóa dữ liệu campus và export bản 2D

**File chính:** `simulation/gazebo/`, `tools/export_campus_map.py`, `web/assets/maps/dut_campus/`.

- [x] P01.1 Đưa bản sao generator, model, mesh, texture vào `simulation/gazebo/`; bảo toàn đường dẫn tương đối.
- [x] P01.2 Tạo file world từ XML người dùng đã gửi; kiểm tra XML, loại bỏ dòng terminal trước/sau XML.
- [x] P01.3 Ghi metadata: pixel gốc, tỉ lệ, cổng, ENU, world origin, heading và thông tin version của map.
- [x] P01.4 Export SVG 2D từ dữ liệu bố trí/mesh: đường, footprint nhà, tên khu, hình chiếu cây, cổng.
- [x] P01.5 Dùng chiều cao đã được generator làm tròn theo tầng; ghi riêng phần mái/thiết bị khi cần kiểm tra vật cản.
- [x] P01.6 Phân biệt vật thể nhìn thấy với collision thực tế: model hiện có collision nhà và mặt đất, không đại diện đầy đủ mọi chi tiết nhìn thấy.
- [x] P01.7 Bảo đảm SVG và metadata dùng cùng extent, orientation và asset version; giữ đúng tỉ lệ khi resize.
- [x] P01.8 Cho exporter kiểm tra đầu vào và báo lỗi khi thiếu texture/mesh hoặc metadata không phù hợp.
- [x] P01.9 Lưu nguồn gốc map và ghi rõ đây là mô hình bố trí ước lượng theo generator, không phải bản khảo sát địa lý chính xác.

**Đầu ra:** `campus_2d.svg`, `campus_map.json`, bộ dữ liệu Gazebo có thể chép sang Ubuntu, hướng dẫn export.

**Điều kiện qua mốc:** cổng, thư viện, trục đường chính và ít nhất hai tòa nhà đối chiếu khớp source 3D; export lại không làm đổi hệ tọa độ.

## 9. P02 — Phép đổi tọa độ hai chiều

**File chính:** `src/maps/coordinate_transform.*`, `src/maps/campus_map.*`, `tests/unit/maps/`.

- [x] P02.1 Định nghĩa rõ tọa độ ảnh gốc, texture, canvas đang zoom/pan, local ENU và GPS; ghi đơn vị cho từng trường.
- [x] P02.2 Dùng công thức từ generator khi chưa dịch/xoay campus:

  ```text
  E = (pixel_x - 205) × 1.5
  N = (288 - pixel_y) × 1.5
  ```

- [x] P02.3 Xử lý kích thước hiển thị và zoom/pan bằng phép biến đổi ngược; không lấy pixel màn hình nhân thẳng với 1,5.
- [x] P02.4 Thực hiện ENU ↔ GPS theo WGS84, đối chiếu quy ước Gazebo; không tráo latitude/longitude hoặc East/North.
- [x] P02.5 Đặt phép đổi authoritative ở backend; frontend dùng cùng metadata và kết quả trả về để hiển thị.
- [x] P02.6 Tách `world origin elevation`, `Home PX4`, `altitude AMSL` và `relative altitude`; không dùng `spawn z=0.2` làm độ cao nhiệm vụ.
- [x] P02.7 Nhận Home/độ cao thực tế từ telemetry bổ sung; phát hiện Home chưa có, đã thay đổi hoặc PX4 đã tạo phiên mới.
- [x] P02.8 Không lấy GPS hiện tại của drone đang di chuyển làm gốc map mỗi lần cập nhật.
- [x] P02.9 Kiểm tra tọa độ hữu hạn, biên map, latitude/longitude hợp lệ và giá trị không tràn khi nhân `1e7`.
- [x] P02.10 Test điểm cổng `(205,288) → (0,0)`, sang phải 10 pixel → E +15 m, lên trên 10 pixel → N +15 m; test vòng đi-về và các góc map.
- [x] P02.11 Chuẩn bị bảng điểm tham chiếu để đối chiếu Gazebo tại P13; sai số cho phần toán phải được định lượng riêng với sai số GPS/EKF.

**Điều kiện qua mốc:** unit test và dữ liệu tham chiếu đạt; mỗi điểm UI có thể chuyển qua lại mà không lật trục hoặc mất tỉ lệ.

## 10. P03 — Giao ước module và cấu hình mở rộng

**File chính:** `include/mavlink_gateway/mission.hpp`, `navigation.hpp`, `config.hpp`, `src/config/config.cpp`, `src/client/client_config.*`.

- [x] P03.1 Thêm kiểu riêng cho mission item, upload/download operation, mission progress, Home/navigation và lỗi; giữ `contracts.hpp` nguyên vẹn.
- [x] P03.2 Quy định ID giao dịch, phiên TCP, đích PX4, dấu thời gian monotonic, revision/hash của bản nháp và map version.
- [x] P03.3 Thêm schema 2 cho cấu hình chung; schema 1 vẫn cho hành vi cũ và mission mặc định tắt.
- [x] P03.4 Giữ tập 5 `protocol.decode_msgids` cũ; các thông điệp mission/navigation đi qua decoder mở rộng riêng.
- [x] P03.5 Thêm phần mission có bật/tắt, số mục tối đa, timeout/retry, điều kiện upload/start; kiểm tra kiểu và khoảng giá trị.
- [x] P03.6 Cho command ID 300 chỉ khi mission được bật và cấu hình cho phép; giữ chặn command ngoài phạm vi.
- [x] P03.7 Tạo `gateway.mission.example.json`; không ghi đè cấu hình `.local.json` đang dùng một cách âm thầm.
- [ ] P03.8 Thêm cấu hình web: bind loopback, port, asset root, map, tốc độ snapshot và giới hạn request/subscriber.
- [x] P03.9 Ghi resolved config của schema mới để biết đúng giá trị khi phân tích log.
- [x] P03.10 Test cấu hình cũ, cấu hình mission hợp lệ, key lạ, port trùng, ID không hỗ trợ và cấu hình mâu thuẫn.

**Giới hạn bản đầu dự kiến:** tối đa 100 mission item tính cả TAKEOFF/LAND; độ cao UI 2–100 m so với Home, dừng tại điểm 0–60 giây. Đây là giới hạn ứng dụng có thể cấu hình, không phải giới hạn chung của PX4. Giới hạn thực dùng trong demo còn phụ thuộc mô hình và điều kiện nhiệm vụ.

**Điều kiện qua mốc:** A/B/C dùng cùng kiểu và schema; cấu hình cũ tiếp tục được nạp; kiểm tra hash xác nhận `contracts.hpp` không đổi.

## 11. P04 — Decoder và builder bổ sung

**File chính:** `src/processing/mission_decoder.*`, `navigation_decoder.*`, `src/client/mission_builder.*`, thư viện MAVLink đã có.

### 11.1. Bản tin cần xử lý

| Nhóm | Bản tin | Vai trò trong bản đầu |
|---|---|---|
| Tải mission | `MISSION_COUNT` (44), `MISSION_REQUEST_INT` (51), `MISSION_ITEM_INT` (73), `MISSION_ACK` (47) | Trao đổi các mục và kết quả |
| Đọc lại | `MISSION_REQUEST_LIST` (43) và các bản tin ở trên | Xác nhận nội dung đang lưu trên PX4 |
| Tương thích | `MISSION_REQUEST` (40), `MISSION_ITEM` (39) | Nhận diện/xử lý theo hành vi PX4 1.15.4 đã đối chiếu; ưu tiên dạng INT |
| Tiến độ | `MISSION_CURRENT` (42), `MISSION_ITEM_REACHED` (46) | Mục hiện tại và mục đã tới |
| Navigation | `HOME_POSITION` (242), `EXTENDED_SYS_STATE` (245), HEARTBEAT hiện có | Home, landed state, armed và chế độ bay |
| Bắt đầu | `COMMAND_LONG` (76), `COMMAND_ACK` (77), command ID 300 | Yêu cầu chạy mission và ACK |

Danh sách trên là đầu vào triển khai, không đồng nghĩa cho phép toàn bộ các msgid ở mọi chiều. A áp whitelist theo từng chiều và vai trò. Bộ định nghĩa field phải được lấy từ header MAVLink đang vendored và so với [MAVLink common](https://mavlink.io/en/messages/common.html).

- [x] P04.1 Xác nhận parser hiện có chấp nhận CRC các bản tin mới, không viết parser thứ hai.
- [x] P04.2 Thêm decoder mở rộng; không sửa giao diện `IDecoder`/variant cũ.
- [x] P04.3 Chuẩn hóa cách đọc trường extension bị lược bỏ trong khung MAVLink 2 và mission type mặc định.
- [x] P04.4 Dùng `mission_type=MISSION`; chưa hỗ trợ geofence/rally trong giao dịch này.
- [x] P04.5 Builder dùng cùng cơ chế cấp SEQ/đồng bộ hiện có, không tạo hai bộ SEQ cạnh tranh trong một client.
- [x] P04.6 Kiểm tra builder bằng fixture có đáp án độc lập: message ID, target, seq, command, frame, CRC và đơn vị.
- [x] P04.7 Với `MISSION_ITEM_INT`, kiểm tra đặc biệt `x=latitude×1e7`, `y=longitude×1e7`, altitude theo frame đã chọn.
- [ ] P04.8 Test CRC lỗi, payload ngắn/extension rút gọn, mission type khác, sai nguồn và trường không hợp lệ.
- [x] P04.9 Inspector tiếp tục thống kê SEQ trên toàn luồng theo nguồn; không chỉ đếm những msgid UI cần.

**Điều kiện qua mốc:** decode/build đúng các khung kiểm thử; dữ liệu mới có thể được dùng mà không đổi contract cũ.

## 12. P05 — MissionManager và kiểm thử giao thức độc lập

**File chính:** `src/client/mission_manager.*`, `mission_builder.*`, `tests/unit/mission/`, `tools/fake_px4_mission.py`.

### 12.1. Biên dịch bản nháp thành mission

- [x] P05.1 Chuyển bản nháp thành TAKEOFF + các WAYPOINT + LAND tại Home đã xác nhận; Home hiển thị trên bản đồ không tự trở thành một waypoint thừa.
- [x] P05.2 Tạo seq liên tục từ 0, khai báo item hiện tại đúng quy tắc, `autocontinue`, độ cao và thời gian dừng.
- [x] P05.3 Áp các giới hạn ở backend trước khi tạo gói; không chấp nhận dữ liệu không hữu hạn hoặc mission rỗng như một cách xóa nhầm mission.
- [x] P05.4 Kiểm tra đoạn đường với footprint/độ cao nhà và khoảng đệm cấu hình; báo rõ dữ liệu kiểm tra chỉ bao phủ phần hình học đã có, không chứng minh tránh mọi vật cản.
- [x] P05.5 Đóng băng snapshot mission cho mỗi lần upload; sửa bản nháp sau đó phải tạo revision khác.

### 12.2. State machine tải và đọc lại

- [x] P05.6 Tách trạng thái idle, upload đang thực hiện, chờ ACK, đọc lại, verified, failed và unknown; không dùng một cờ `success` cho mọi bước.
- [x] P05.7 Thực hiện COUNT → PX4 yêu cầu seq → gửi đúng item → ACK; có timeout/retry hữu hạn theo từng bước.
- [x] P05.8 Xử lý yêu cầu lặp đúng seq, seq ngoài biên, yêu cầu không đúng giao dịch, ACK lỗi và ACK đến muộn.
- [x] P05.9 Kiểm tra sysid/compid nguồn, target, mission type và phiên; bản tin tiến độ broadcast không được hoàn tất một giao dịch upload.
- [x] P05.10 Đối chiếu PX4 1.15.4 để xử lý request legacy; không suy ra tính tương thích chỉ từ tài liệu mới nhất.
- [x] P05.11 Sau upload, tải lại mission bằng REQUEST_LIST/COUNT/REQUEST_INT/ITEM_INT và kết thúc theo giao thức; so sánh ngữ nghĩa, seq và sai số số thực được định nghĩa.
- [x] P05.12 Chỉ đánh dấu verified cho đúng revision/map/Home/session đã kiểm tra; thay đổi đầu vào làm trạng thái start không còn hợp lệ.
- [x] P05.13 Tách timeout, PX4 từ chối, mất kết nối và unknown sau thời điểm có thể PX4 đã nhận đủ dữ liệu.
- [x] P05.14 Hủy thao tác local và dọn hàng đợi theo operation/session; không khẳng định đã hủy thao tác trên PX4 khi chưa có bằng chứng giao thức tương ứng.
- [x] P05.15 Mất kết nối không tự phát lại upload, ARM hoặc start sau reconnect. Phải đọc lại/đối chiếu trạng thái trước khi sử dụng tiếp.
- [x] P05.16 Một giao dịch mission tại một thời điểm; từ chối upload khi drone đang armed/bay trong chính sách bản đầu.
- [x] P05.17 Nếu QGroundControl hoặc GCS khác sửa mission, không giữ nhãn verified cho nội dung đã thay đổi; kiểm tra lại trước khi start, báo xung đột khi trao đổi ngoài giao dịch.

Luồng upload có yêu cầu từng mục và ACK xác nhận; timeout/retry thuộc giao thức, không phải việc web tự gửi waypoint liên tục. Xem [MAVLink Mission Protocol](https://mavlink.io/en/services/mission.html). Hành vi tương thích cụ thể phải đối chiếu [mission manager PX4 1.15.4](https://github.com/PX4/PX4-Autopilot/blob/v1.15.4/src/modules/mavlink/mavlink_mission.cpp).

### 12.3. Bắt đầu và theo dõi nhiệm vụ

- [x] P05.18 Thêm thao tác start bằng command ID 300, kiểm tra điều kiện ở backend: đúng controller, PX4 tươi, Home hợp lệ, armed và mission vừa xác minh còn khớp.
- [x] P05.19 Đặt tham số start theo PX4 1.15.4 và kiểm chứng seq đầu sau upload. Chính sách bản đầu: mỗi lần chạy lại phải upload/readback lại khi disarmed; đánh dấu lượt upload đã dùng ngay khi yêu cầu start được phát, không tái dùng nút Start cho lượt đó.
- [x] P05.20 Start mặc định không tự retry khi kết quả có thể đã được thực hiện; sau ACK mất/timeout hiển thị unknown và đối chiếu telemetry, không tự gửi lại.
- [x] P05.21 Dùng ACK để xác định kết quả lệnh; dùng heartbeat/mode, current/reached và telemetry để xác định tiến trình thực tế.
- [x] P05.22 Phân biệt đã tới waypoint cuối, đã yêu cầu LAND, đang hạ cánh, landed state từ EXTENDED_SYS_STATE và disarmed; thiếu tín hiệu xác nhận thì giữ trạng thái chưa xác nhận, không dùng riêng heartbeat disarmed để kết luận đã hạ cánh.
- [x] P05.23 LAND trong khi chạy mission là hành động riêng có xác nhận; trạng thái nhiệm vụ được đối chiếu lại từ PX4. Không tự DISARM khi đóng web hoặc mất socket.

**Lưu ý đã kiểm tra từ source:** `MAV_CMD_MISSION_START` trong PX4 1.15.4 có thể chuyển mode và thực hiện arm. Vì vậy điều kiện “người dùng ARM riêng trước khi Start” là chính sách của web/backend, không phải khẳng định PX4 bắt buộc tách hai lệnh. Xem [Commander PX4 1.15.4](https://github.com/PX4/PX4-Autopilot/blob/v1.15.4/src/modules/commander/Commander.cpp).

**Điều kiện qua mốc:** unit/fake-peer test đạt cả thành công và lỗi; có log thể hiện đầy đủ từng bước. Chưa cần browser để chứng minh MissionManager hoạt động.

## 13. P06 — Gateway hỗ trợ mission

**File chính:** `src/gateway/gateway.*`, schema chung, `tests/integration/gateway_test.py` và test mới.

- [x] P06.1 Thêm kiểm tra uplink theo loại bản tin: COUNT, ITEM_INT, REQUEST_LIST, REQUEST_INT, ACK dùng cho đọc lại; command 300 đi qua whitelist command.
- [x] P06.2 Chỉ cho phép mission khi schema/config bật chức năng; vẫn từ chối thông điệp ngoài phạm vi.
- [x] P06.3 Observer gửi bất cứ uplink nào vẫn bị từ chối theo chính sách cũ; UI ẩn nút không thay thế kiểm tra tại gateway.
- [x] P06.4 Kiểm tra GCS ID, target PX4, mission type, giá trị count/seq và độ dài field cần đọc; xử lý đúng payload có trường zero bị cắt ngắn.
- [x] P06.5 Giữ kiểm tra endpoint đã khóa và độ tươi heartbeat trước khi xếp hàng lẫn trước khi gửi UDP.
- [x] P06.6 Gắn các frame đang chờ với phiên controller; không gửi tiếp gói của controller đã đóng sau khi một controller mới kết nối.
- [x] P06.7 Hàng đợi có giới hạn; lỗi queue full phải tới được runtime/log, không được báo upload thành công.
- [x] P06.8 Downlink trả về các bản tin mission hợp lệ nguyên byte; không bị giới hạn bởi tập 5 thông điệp UI cũ.
- [x] P06.9 Ghi lý do chặn có cấu trúc: sai vai trò, sai GCS, sai target, stale, mission tắt, count/seq lỗi.
- [ ] P06.10 Test qua gateway thật bằng UDP/TCP localhost: đúng vai trò/nguồn/đích, mission tắt, hết hạn heartbeat, mất client, queue đầy và reconnect.

**Điều kiện qua mốc:** luồng upload/đọc lại đi qua A thật và đối tác giả lập đạt; dữ liệu nhận ở hai đầu được đối chiếu byte.

## 14. P07 — Tách ClientRuntime dùng chung console và web

**File chính:** `apps/client_main.cpp`, `src/client/client_runtime.*`, các manager, send queue và log.

- [ ] P07.1 Chuyển kết nối TCP, parser, heartbeat, ticker, receive loop và trạng thái sang lớp runtime có vòng đời rõ.
- [ ] P07.2 Console chỉ còn chuyển input thành yêu cầu runtime và hiển thị callback; web dùng cùng API đó.
- [x] P07.3 Có start/connect, disconnect, shutdown, snapshot, submit command và thao tác mission; không lộ socket thô cho HTTP handler.
- [x] P07.4 Xử lý manager tại owner thread hoặc khóa được quy định; HTTP thread gửi yêu cầu vào queue hữu hạn, không tự chạm đồng thời vào manager.
- [x] P07.5 Phối hợp CommandManager với MissionManager: khi upload/readback thì chặn ARM/start/lệnh làm xung đột; nhận diện thay đổi armed từ GCS khác.
- [x] P07.6 Giữ một writer cho TCP và cơ chế đo completion; kiểm tra lại các frame mission đã hết hạn khi thao tác/session bị hủy.
- [x] P07.7 Giữ GCS heartbeat không bị HTTP, renderer hoặc ghi log làm nghẽn.
- [x] P07.8 B0 vẫn xử lý các bản tin cần cho quản lý mission/ACK/nguồn; UI đầy đủ telemetry dùng B1/B2 và ghi rõ dữ liệu không có trong B0.
- [x] P07.9 Mỗi trường telemetry có thời điểm nhận và độ tươi; chỉ nhận trạng thái drone từ đúng nguồn PX4 dự kiến.
- [x] P07.10 Shutdown join đúng các worker, giải phóng socket/queue/log; không phụ thuộc console input đang chặn để dọn runtime.
- [x] P07.11 Test lại CLI, quiet controller console, các tham số TAKEOFF/LAND, ACK, retry và send queue hiện có.

**Điều kiện qua mốc:** console vẫn chạy theo cách cũ; runtime không cần stdin để hoạt động hoặc dừng sạch; MissionManager đã dùng chung được.

## 15. P08 — Backend HTTP/SSE local

**File chính:** `apps/web_client_main.cpp`, `src/web/`, `third_party/`, `CMakeLists.txt`.

- [x] P08.1 Thử build HTTP và SSE tối thiểu với toolchain hiện tại; chọn/khóa release hoặc commit hỗ trợ build thực tế, lưu nguồn và giấy phép.
- [x] P08.2 Tạo target `web_client`; bind mặc định `127.0.0.1:8080`, báo lỗi rõ khi port bận.
- [x] P08.3 Phục vụ frontend/map từ asset root của project/gói phát hành, không phụ thuộc working directory người dùng mở terminal.
- [x] P08.4 Tạo state store chứa snapshot telemetry, kết nối, operation command/mission và độ tươi.
- [x] P08.5 Tách trạng thái HTTP server, TCP gateway và heartbeat PX4; không gom thành một đèn xanh “connected”.
- [x] P08.6 API thao tác có `request_id`; yêu cầu gửi trùng do double-click/HTTP retry không tạo thêm lệnh thực.
- [x] P08.7 API trả tiếp nhận yêu cầu không được mang nghĩa PX4 đã chấp nhận. Kết quả thật cập nhật qua operation ID/snapshot/SSE.
- [x] P08.8 API thay đổi trạng thái chỉ nhận JSON hợp lệ và session/controller hợp lệ; kiểm tra Host/Origin, token thao tác local và giới hạn body; không mở CORS cho mọi nguồn.
- [x] P08.9 Static file chỉ được đọc trong asset root; chặn đường dẫn thoát thư mục, không cung cấp endpoint gửi shell command hoặc MAVLink tùy ý.
- [x] P08.10 SSE gửi snapshot khoảng 5 Hz và sự kiện cần thiết, có nhịp giữ kết nối; client chậm không được chặn luồng MAVLink hoặc tăng bộ nhớ vô hạn.
- [x] P08.11 Giới hạn subscriber/thread/queue; bảo đảm SSE không chiếm hết worker khiến API lệnh không được xử lý.
- [x] P08.12 Một instance web backend có một kết nối gateway đang hoạt động. Một browser session sở hữu quyền điều khiển; các tab khác chỉ đọc cho tới khi chuyển quyền rõ ràng.
- [x] P08.13 Refresh/SSE reconnect chỉ đồng bộ snapshot, không khôi phục lệnh cũ. Khi tab điều khiển đóng, bỏ quyền tab đó và chặn yêu cầu cũ; backend còn chạy phải hiển thị được trạng thái khi mở lại.
- [x] P08.14 Đóng tab không tự DISARM/LAND; tắt backend mới đóng kết nối TCP và ngừng heartbeat. Ghi rõ vòng đời này trong UI/hướng dẫn và kiểm thử.
- [ ] P08.15 Unit/integration test API validation, quyền, yêu cầu trùng, subscriber chậm, mất gateway, lỗi port và shutdown.

### 15.1. API dự kiến

| API | Mục đích |
|---|---|
| `GET /api/state` | Snapshot hiện tại và độ tươi |
| `GET /api/events` | SSE telemetry/trạng thái/operation |
| `GET /api/map` | Metadata/version bản đồ |
| `POST /api/connect` | Kết nối gateway với vai trò đã chọn |
| `POST /api/disconnect` | Ngắt kết nối theo yêu cầu rõ ràng |
| `POST /api/commands` | Các lệnh trong danh sách cho phép |
| `POST /api/missions/validate` | Kiểm tra/biên dịch bản nháp, trả bản xem trước |
| `POST /api/missions/upload` | Bắt đầu giao dịch tải mission |
| `POST /api/missions/readback` | Đọc mission PX4 để đối chiếu |
| `POST /api/missions/start` | Yêu cầu chạy đúng mission đã xác minh |
| `GET /api/operations/{id}` | Tiến độ/kết quả cho yêu cầu cụ thể |

Các endpoint cuối cùng phải được ghi trong `docs/design/web-api.md` với request/response, mã lỗi, quyền và ví dụ. GET không được khởi tạo thao tác bay hoặc tải mission.

**Điều kiện qua mốc:** API kết nối được gateway thật tại localhost và nhận snapshot; thao tác lỗi có kết quả rõ, quyền controller được kiểm tra ở backend.

## 16. P09 — Web giám sát dữ liệu thật

**File chính:** `web/index.html`, `web/styles/`, `web/js/`.

- [x] P09.1 Dựng giao diện theo bản campus đã duyệt, tách source thành các module nhỏ dễ đọc.
- [x] P09.2 Mọi font/icon/map cần cho runtime nằm trong gói; mở giao diện và bản đồ khi không có Internet.
- [x] P09.3 Chọn Observer/Controller; hiển thị đúng host/cổng từ cấu hình, lỗi 10048/10061 bằng lời dễ hiểu.
- [x] P09.4 Pin, tư thế, độ cao AMSL/relative, armed/mode và heartbeat lấy từ API; bỏ giá trị demo hard-code khỏi chế độ thật.
- [x] P09.5 Hiển thị drone trên map bằng GPS → local → ảnh; kiểm tra vị trí ngoài extent thay vì ép vào mép map.
- [x] P09.6 Trạng thái chờ dữ liệu, stale, TCP mất, PX4 im lặng và server ngừng hoạt động có nhãn riêng; dữ liệu cũ không được trông như vẫn đang cập nhật.
- [x] P09.7 Tạo luồng nhận lại snapshot khi SSE nối lại, tránh ghi đè dữ liệu mới bằng sự kiện cũ.
- [ ] P09.8 Kiểm tra bố cục laptop phổ biến, zoom trình duyệt, màn hình hẹp, label dài và thao tác bằng bàn phím.
- [x] P09.9 Hiển thị log gần nhất với giới hạn số dòng; phần chi tiết mở khi cần, không cho log làm nhảy ô đang nhập.

**Điều kiện qua mốc:** web nhận telemetry thật từ fake PX4 qua gateway, marker/timestamp thay đổi đúng; chưa cần bấm lệnh để nghiệm thu phần giám sát.

## 17. P10 — Đưa các lệnh hiện có lên web

- [x] P10.1 Nút ARM, DISARM, TAKEOFF, LAND gọi runtime/CommandManager qua API.
- [x] P10.2 Xác nhận hiển thị đúng tên lệnh và tác động; observer bị chặn cả UI lẫn backend.
- [x] P10.3 TAKEOFF mặc định giữ cách dùng vị trí hiện tại và MIS_TAKEOFF_ALT đã có; nếu mở tùy chọn độ cao AMSL thì ghi nhãn/đơn vị riêng.
- [ ] P10.4 Hiển thị request ID, queued, sent-to-socket, chờ ACK, in-progress, accepted/rejected/timeout/unknown.
- [x] P10.5 Các yêu cầu xung đột bị chặn theo runtime; không đè trạng thái pending bằng một lần click mới.
- [x] P10.6 Refresh, retry HTTP và tab thứ hai không phát thêm lệnh; kết quả được lấy lại bằng operation ID.
- [x] P10.7 ACK accepted không tự bật nhãn ARMED hay kết luận drone đã bay; nhãn lấy từ telemetry tương ứng.
- [ ] P10.8 Test browser/API với fake PX4: chấp nhận, từ chối, sai ACK, timeout và mất TCP giữa yêu cầu.

**Điều kiện qua mốc:** cùng một thao tác trên console và web tạo tham số MAVLink tương đương; UI giữ được khả năng nhập khi telemetry liên tục.

## 18. P11 — Planner và mission hoàn chỉnh trên web

- [x] P11.1 Cho thêm điểm bằng click, kéo đổi vị trí, xóa và đổi thứ tự; marker/map/list luôn cùng một dữ liệu.
- [ ] P11.2 Zoom/pan đúng phép đổi tọa độ; waypoint gần nhau vẫn chọn được qua danh sách.
- [x] P11.3 Editor có độ cao so với Home, thời gian dừng và tọa độ E/N; dữ liệu hiển thị không tráo trục/đơn vị.
- [x] P11.4 Hiển thị TAKEOFF/LAND riêng, seq kỹ thuật và thứ tự điểm UI được ánh xạ rõ.
- [x] P11.5 Lưu/mở JSON có schema, map ID/version, origin/frame và revision; tải file sai map/phien bản phải báo rõ trước khi dùng.
- [x] P11.6 Lưu bản nháp local không được gắn nhãn đã tải PX4; tải PX4 không tự bắt đầu mission.
- [x] P11.7 Gọi validate backend; nêu lỗi tại đúng waypoint/đoạn bay, không dùng validation JavaScript làm lớp kiểm tra duy nhất.
- [x] P11.8 Nút upload hiển thị tiến độ số mục, chờ ACK, đọc lại và kết quả đối chiếu; chỉ giao dịch đúng nguồn/phiên mới được cập nhật trạng thái.
- [x] P11.9 Sửa điểm, độ cao, thứ tự, Home hoặc map làm mất hiệu lực verified cho bản nháp đó; khi đang tải dùng snapshot đã khóa và ghi rõ revision.
- [x] P11.10 Trước start, kiểm tra lại mission trên PX4 để phát hiện thay đổi từ QGroundControl; backend kiểm tra cuối cùng trước phát lệnh. Khi demo, web là phía duy nhất ghi mission; đọc lại không loại trừ tuyệt đối việc GCS khác ghi mission ngay sau lúc kiểm tra.
- [x] P11.11 Start có xác nhận tóm tắt mission, Home, độ cao và số mục; điều kiện armed/fresh/verified được kiểm tra lại tại server.
- [x] P11.12 Khi chạy: marker drone và current/reached lấy từ PX4, không dùng timer nội bộ để giả chuyển điểm trong chế độ thật.
- [x] P11.13 Không lấy việc tới item cuối làm bằng chứng đã hạ cánh. Hiển thị rõ trạng thái kết thúc đã xác nhận hay đang chờ telemetry xác nhận.
- [x] P11.14 LAND khi thực hiện mission vẫn dùng lệnh rõ ràng và theo dõi kết quả; không tự nhấn lại start khi người dùng refresh.
- [ ] P11.15 Test đầy đủ luồng tạo → sửa → lưu/mở → upload → readback → ARM → start → theo dõi → LAND/kết thúc bằng đối tác giả lập.

**Điều kiện qua mốc:** có bản web thực dùng protocol client/gateway, không còn dùng ACK/tiến độ bay giả trong chế độ kết nối thật.

## 19. P12 — Log và kiểm thử toàn hệ thống

### 19.1. Log/bằng chứng

- [x] P12.1 Giữ log cũ; thêm log mission có schema rõ, operation ID, revision/hash, map version, session, target, seq, attempt và result.
- [x] P12.2 Lưu bản draft và mission đã biên dịch cho từng lần upload; có thể xác định chính xác bản nào đã được kiểm tra lại.
- [x] P12.3 Log API chỉ ghi dữ liệu cần phân tích; không ghi token điều khiển.
- [x] P12.4 Dùng monotonic trong mỗi process để đo timeout; không trừ timestamp monotonic giữa Windows và Ubuntu hoặc giữa hai process.
- [x] P12.5 Ghi nhận fingerprint của source, frontend, map và cấu hình để tránh demo nhầm bản.
- [x] P12.6 UI ghi đường dẫn log thực tế; lỗi tạo log/thư mục đã có phải được báo, không hiển thị đường dẫn mẫu như thư mục đã tồn tại.
- [ ] P12.7 Cập nhật metrics để phân biệt số TCP client và số browser subscriber; tách chi phí web khỏi phép so sánh B0/B1/B2 gốc.

### 19.2. Ma trận kiểm thử bắt buộc

| Nhóm | Tình huống chính | Bằng chứng mong đợi |
|---|---|---|
| Hồi quy | Parser/CRC/SEQ, queue, ACK, TAKEOFF/LAND và cấu hình cũ | Bộ test hiện có đạt sau thay đổi |
| Tọa độ | Cổng, hai trục, góc map, resize/zoom, ENU↔GPS↔ENU | Sai số được ghi và đạt ngưỡng đã định |
| Mission bình thường | Upload + readback + start + current/reached | Đủ bước, đúng seq/đích/nội dung |
| Thiếu/trùng gói | COUNT/request/item/ACK mất hoặc lặp | Retry hữu hạn, không nhảy bước, không tạo mission mới do lặp |
| Gói sai | Sai CRC/target/source/type/seq, payload rút gọn | Chặn/giải mã đúng, không cập nhật nhầm operation |
| Timeout | Chờ request, item, ACK hoặc readback | Kết quả failed/unknown rõ, không tự start |
| Vòng đời | TCP ngắt, PX4 reboot, heartbeat stale, session mới | Hủy hiệu lực trạng thái cũ, không gửi gói tồn từ phiên trước |
| Vai trò | Observer, controller thứ hai, hai tab cùng gửi | Chặn ở backend và gateway, không chỉ khóa nút |
| Web | HTTP trùng, JSON lỗi, static path, SSE nối lại/chậm | Giới hạn tài nguyên, không lặp lệnh hoặc nghẽn heartbeat |
| GCS khác | QGroundControl tải/sửa mission hoặc arm | Phát hiện trạng thái không còn khớp, yêu cầu xác minh lại |
| Độ cao | Relative/Home/AMSL, NaN/Inf, Home đổi | Không nhầm đơn vị, không upload/start khi tham chiếu chưa hợp lệ |
| Chạy lâu | Telemetry liên tục, cập nhật UI, subscriber chậm | Queue/thread/bộ nhớ có giới hạn, lệnh còn đáp ứng |
| Đóng gói | Giải nén nơi khác, không Internet | Asset/config/exe đầy đủ và đường dẫn đúng |

- [x] P12.8 Tích hợp test mới vào CTest/script hiện có; không chỉ chạy các test mới.
- [x] P12.9 Lưu báo cáo machine-readable, stdout và đường dẫn log; phân biệt skipped/not-run với passed.
- [ ] P12.10 Chạy UI thủ công trên Edge/Chrome; chụp bằng chứng trạng thái kết nối, upload lỗi, upload đạt, observer và mission progress.
- [x] P12.11 Chỉ mở rộng/repeat test khi có thay đổi, lỗi hoặc vấn đề chưa giải quyết; ghi rõ giới hạn kiểm thử.

**Điều kiện qua mốc:** các lỗi chặn đã được sửa; kiểm thử localhost đạt; tài liệu ghi rõ PX4 thật/SITL đang ở trạng thái chờ kiểm chứng.

## 20. P13 — Kiểm thử PX4 1.15.4/Gazebo trên hai máy

**Người thực hiện Ubuntu:** C/người dùng; các lệnh bay được thao tác chủ động. Người triển khai chuẩn bị gói và hướng dẫn, phân tích log sau mỗi lượt.

- [ ] P13.1 Chốt phiên bản build, map/world và lệnh khởi động đã dùng; kiểm tra IP hiện tại của hai máy, không mặc định IP cũ còn đúng.
- [ ] P13.2 Kiểm tra UDP tới gateway và hai cổng TCP; đóng chương trình giữ cổng đúng quy trình trước khi chạy bản mới.
- [ ] P13.3 Kiểm tra NavSat, Home, heartbeat và telemetry; cấu hình stream bổ sung dựa trên những stream thực có trong PX4 1.15.4.
- [ ] P13.4 Đối chiếu cổng và hai điểm tham chiếu theo hai trục. Có thể lấy pose tĩnh bằng công cụ mô phỏng; không phải bay ngay để đo tọa độ.
- [ ] P13.5 Chỉnh orientation/UV/transform nếu map 2D bị lật, lệch hoặc xoay; cố định metadata đúng trước khi upload nhiệm vụ bay.
- [ ] P13.6 Kiểm tra khả năng nhận mission bằng upload và đọc lại khi drone disarmed; dùng QGroundControl quan sát và đối chiếu, tránh cùng lúc tải hai mission.
- [ ] P13.7 Chọn mission ngắn phù hợp vùng trống/chiều cao mô hình; kiểm tra điều kiện PX4, không tắt kiểm tra preflight hoặc force-arm để vượt lỗi.
- [ ] P13.8 Người dùng ARM và Start; đối chiếu ACK, mode, đường bay Gazebo, current/reached và vị trí trên web.
- [ ] P13.9 Kiểm tra LAND/kết thúc, vị trí Home và trạng thái sau hạ cánh; lưu kết quả thực thay vì suy ra từ log upload.
- [ ] P13.10 Thử trường hợp bị PX4 từ chối, heartbeat ngừng, mở lại web và controller bận. Thử mất liên kết khi đang bay chỉ sau khi ghi rõ cấu hình failsafe và chuẩn bị cách xử lý trong SITL.
- [ ] P13.11 Gửi về log Windows, thông tin PX4/world, ảnh/video ngắn và các thao tác đã làm theo thứ tự.

**Điều kiện qua mốc:** có bằng chứng từ PX4/Gazebo thật của cả upload/readback và thực hiện mission. Nếu chưa có kết quả Ubuntu thì mục này vẫn là chưa hoàn thành.

## 21. P14 — Sửa lỗi thực tế và nghiệm thu chức năng

- [ ] P14.1 Phân loại lỗi từ P13: mạng, tọa độ, protocol, tham số PX4, UI hoặc vòng đời.
- [ ] P14.2 Tạo ca tái hiện tối thiểu, bổ sung regression test cho lỗi đáng kể trước/sau sửa.
- [ ] P14.3 Nếu thay đổi map/transform, vô hiệu hóa mission verified cũ và chạy lại các điểm tham chiếu.
- [ ] P14.4 Nếu thay đổi builder/manager/gateway, chạy lại ma trận protocol bị ảnh hưởng và bộ hồi quy.
- [ ] P14.5 Gửi đúng bản build mới, ghi fingerprint; thử lại trên Ubuntu đến khi lỗi chặn được xử lý.
- [ ] P14.6 Duyệt UX cuối: nhập không bị telemetry chen, lỗi chỉ rõ cách xử lý, nhãn độ cao rõ và thao tác thật khác dữ liệu demo.
- [ ] P14.7 Ghi kết quả nghiệm thu, phiên bản đã dùng và hạn chế còn lại có tác động thực tế.

**Điều kiện qua mốc:** đạt checklist nghiệm thu cuối ở mục 24; không còn lỗi làm gửi nhầm lệnh, sai tọa độ hoặc báo thành công sai.

## 22. P15 — Tài liệu, CI và đóng gói bàn giao

### 22.1. Tài liệu/hướng dẫn

- [x] P15.1 Cập nhật README và kiến trúc; ghi rõ source console, web, A/B/C và luồng dữ liệu.
- [x] P15.2 Viết `docs/design/web-api.md`, `docs/design/mission-protocol.md`, `docs/design/campus-map.md` và hướng dẫn chạy web.
- [x] P15.3 Cập nhật phân công A/B/C và giao ước tích hợp; giữ mô tả contract v1 chính xác.
- [x] P15.4 Viết hướng dẫn demo hai máy theo thứ tự cửa sổ/lệnh và tác dụng từng lệnh; có cách tìm IP, port bận, stale và vị trí log.
- [x] P15.5 Cập nhật hướng dẫn Word sau khi lệnh và giao diện thực ổn định; hình/lệnh trong tài liệu phải khớp bản bàn giao.
- [x] P15.6 Ghi rõ cách chạy tay bằng executable/tham số, sau đó mới giới thiệu script nhấp chạy.

### 22.2. Build và gói phát hành

- [x] P15.7 Cập nhật CMake, script build/test/run và thêm `CHAY_WEB_CLIENT.cmd`; tham số config/run-id được truyền rõ ràng.
- [x] P15.8 Cập nhật danh sách đóng gói để có `web/`, `simulation/`, cấu hình mẫu, map và `web_client.exe`; không bỏ sót chỉ vì allowlist cũ.
- [x] P15.9 Cập nhật manifest/hash cho source và asset; giữ bộ kiểm tra package hoạt động.
- [x] P15.10 Giữ `build/`, log/output, cache, file local/token và binary sinh ra ngoài source Git; giữ asset cần thiết và fixture kiểm thử.
- [x] P15.11 Build sạch từ source; với binary release, chạy web/map không cần Node/Python hoặc Internet. Python/Pillow chỉ phục vụ công cụ sinh map/demo theo tài liệu tương ứng.
- [x] P15.12 Giải nén gói ở thư mục khác có khoảng trắng và thử khởi động bằng lệnh/script; kiểm tra đường dẫn asset không dựa vào máy người viết.
- [x] P15.13 Cập nhật CI Windows để build target mới, chạy unit/integration phù hợp và kiểm tra gói; kết quả CI chỉ ghi là đạt khi workflow thực tế đạt.
- [x] P15.14 Chuẩn bị source ZIP, Windows ZIP, gói world/model cho Ubuntu nếu cần, checksum và ghi chú phát hành.

### 22.3. GitHub

- [ ] P15.15 Chuẩn bị các commit tiếng Việt theo module và thay đổi có thật, ví dụ map → config/decoder → mission → gateway → runtime → web → test/docs.
- [ ] P15.16 Khi có yêu cầu publish, xác định repository/branch hiện hành, xem diff và trạng thái remote trước khi push; không mặc định bản clone cũ còn tồn tại.
- [ ] P15.17 Gắn release note/tag với đúng bản đã kiểm thử; chỉ coi việc bàn giao trên GitHub hoàn thành khi nhánh/CI/gói phát hành thực tế khớp thông tin bàn giao.

**Điều kiện qua mốc:** một người khác nhận gói có thể dựng môi trường/chạy demo theo hướng dẫn, có thể xác định đúng phiên bản và kiểm tra kết quả bằng log.

## 23. Quy tắc cập nhật tiến độ trong khi thực hiện

1. Bắt đầu mỗi mốc bằng việc xác nhận đầu vào và điều kiện của mốc trước.
2. Đánh dấu checkbox chỉ sau khi công việc đã thực hiện; đường dẫn/kết quả test phải tra được.
3. Kết thúc mỗi mốc ghi: file thay đổi, chức năng mới, kiểm thử đã chạy, kết quả và phần chưa kiểm chứng.
4. Lỗi baseline, lỗi mới, test chưa chạy và phụ thuộc kết quả Ubuntu được ghi riêng; không gộp thành “đã xong”.
5. Việc cần dừng chương trình đang chạy để ghi đè executable được báo rõ; không tự đóng ứng dụng người dùng đang demo.
6. Các thay đổi cần cho mốc sau được thêm task ID mới; không xóa tiêu chí nghiệm thu để làm tiến độ trông hoàn tất.

Mẫu ghi sau mỗi mốc:

```text
Mốc:
Trạng thái: chưa làm / đang làm / đạt / chờ kết quả kiểm thử
File đã thay đổi:
Kiểm thử đã chạy và nơi lưu kết quả:
Kết quả trên PX4/Gazebo nếu có:
Lỗi/giới hạn còn lại:
Mốc tiếp theo:
```

## 24. Checklist nghiệm thu cuối cùng

Chỉ coi sản phẩm hoàn thiện khi tất cả các mục dưới đây đạt hoặc phạm vi thay đổi đã được người dùng chốt rõ.

- [ ] Source cấu trúc rõ, build sạch trên Windows theo hướng dẫn.
- [ ] `contracts.hpp` giữ nguyên hash đã chốt.
- [ ] Các chức năng console và bộ kiểm thử trước đó tiếp tục đạt.
- [ ] Cấu hình cũ chạy được; cấu hình mission/web mới có validation và tài liệu.
- [ ] Web/map mở được offline, mọi thông số hiển thị trong chế độ thật lấy từ runtime.
- [ ] Mất HTTP/TCP/PX4 được thể hiện đúng; dữ liệu stale không bị trình bày như dữ liệu mới.
- [ ] Observer không thể gửi lệnh/mission bằng cả UI, HTTP trực tiếp hoặc TCP trực tiếp.
- [ ] Controller và nhiều tab không tạo thao tác điều khiển trùng hoặc tranh quyền âm thầm.
- [ ] ARM/DISARM/TAKEOFF/LAND chạy qua CommandManager và có kết quả đúng trên UI/log.
- [ ] Chuyển đổi map↔GPS được kiểm tra bằng số và đối chiếu Gazebo.
- [ ] Draft lưu/mở giữ đúng map, điểm, độ cao và thời gian dừng.
- [ ] Upload + readback xác nhận đúng mission; lỗi/retry/mất phiên không báo thành công giả.
- [ ] Start chỉ dùng đúng bản đã xác minh; sửa draft/Home/mission ngoài hệ thống làm vô hiệu trạng thái cũ.
- [ ] Start/refresh/reconnect không tự phát lại lệnh bay.
- [ ] Tiến độ và trạng thái kết thúc lấy từ PX4; không dựa timer giả trong chế độ thật.
- [ ] Mission đã được thử trên PX4 1.15.4/Gazebo campus ở hai máy và có bằng chứng.
- [ ] Log, source/asset fingerprint và cấu hình đủ để tái hiện lượt demo.
- [ ] CI/checks và kiểm tra package đạt; thư mục giải nén khác vẫn chạy được.
- [ ] Tài liệu Markdown/Word, script và tên executable đúng bản phát hành.
- [ ] Gói source, binary và dữ liệu Ubuntu đã bàn giao cùng checksum/hạn chế được ghi rõ.

## 25. Nguồn tham chiếu khi triển khai

- Source hiện tại trong `apps/`, `src/`, `include/`, `config/`, `CMakeLists.txt` và `tools/package_release.py` đã được đọc khi lập kế hoạch.
- Generator/model campus người dùng gửi trong `dut_campus_v2`, cùng XML world và lệnh khởi động đã cung cấp.
- [MAVLink Mission Protocol](https://mavlink.io/en/services/mission.html).
- [MAVLink common messages](https://mavlink.io/en/messages/common.html).
- [Mission manager PX4 1.15.4](https://github.com/PX4/PX4-Autopilot/blob/v1.15.4/src/modules/mavlink/mavlink_mission.cpp).
- [Commander PX4 1.15.4](https://github.com/PX4/PX4-Autopilot/blob/v1.15.4/src/modules/commander/Commander.cpp).
- [PX4 1.15 — Mission Mode Multicopter](https://docs.px4.io/v1.15/en/flight_modes_mc/mission).
- [SDFormat 1.9 — spherical coordinates](https://github.com/gazebosim/sdformat/blob/sdf13/sdf/1.9/spherical_coordinates.sdf).
- [cpp-httplib](https://github.com/yhirose/cpp-httplib).
- [MDN — Server-Sent Events](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events).

Tài liệu online có thể mô tả field mới hơn firmware/thư viện của dự án. Khi có khác biệt, phải ghi phiên bản và kiểm chứng bằng header/source đúng PX4 1.15.4 cùng fixture, không coi trang mới nhất là bằng chứng tính năng đã hoạt động.
