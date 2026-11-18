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

