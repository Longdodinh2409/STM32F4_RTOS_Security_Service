# Security Service STM32

## Tổng quan

Dự án này là phần mềm điều khiển trên vi điều khiển STM32F411VET6 dành cho hệ thống kiểm soát truy cập an ninh dựa trên vân tay. Mạch điều khiển nhận diện người dùng bằng module vân tay, hiển thị trạng thái trên màn hình OLED 128x64, tương tác với nền tảng backend/BBB qua UART, và điều khiển bằng nút người dùng.

Mục tiêu chính của hệ thống là:

- Quét và đối chiếu vân tay người dùng
- Cho phép đăng ký vân tay mới
- Hiển thị trạng thái hoạt động trực quan trên OLED
- Chặn tạm thời hoặc vô hạn nếu phát hiện nhiều lần nhận diện sai
- Đồng bộ thời gian và thông tin thành viên từ BBB
- Gửi trạng thái hệ thống lại cho backend để xử lý nghiệp vụ phía ngoài

---

## Thông tin cơ bản dự án

### MCU và nền tảng phần cứng

- Vi điều khiển: STM32F411VET6
- Họ chip: STM32F4
- Hệ thống clock: sử dụng PLL từ HSI, chạy ở nhịp CPU hiệu quả cho ứng dụng điều khiển thời gian thực
- Compiler / IDE: hỗ trợ xây dựng bằng STM32CubeIDE và MDK-ARM

### Peripherals chính

- I2C1: giao tiếp với màn hình OLED SSD1306
- USART1: giao tiếp với BBB hoặc module quản lý trung tâm qua UART
- USART2: giao tiếp với module cảm biến vân tay
- GPIO: nút người dùng, LED, cờ trạng thái
- FreeRTOS: chạy các task đồng thời trong hệ thống

### Môi trường phát triển

- STM32CubeMX / STM32CubeIDE
- ARM MDK-ARM
- FreeRTOS v10
- SEGGER RTT / SEGGER SystemView để debug và trace
- HAL driver của STMicroelectronics

---

## Kiến trúc phần mềm

Project được tổ chức theo mô-đun rõ ràng, theo phong cách ứng dụng nền embedded:

- Core/Src/main.c
  - Khởi tạo hệ thống
  - Cấu hình clock
  - Khởi tạo UART, I2C, GPIO
  - Tạo và khởi động các task FreeRTOS

- Core/Src/Task_FingerPrint/
  - Quản lý FSM của cảm biến vân tay
  - Gửi lệnh tới module vân tay
  - Nhận và phân tích packet phản hồi
  - Xử lý enrollment, search, lock, unblock

- Core/Src/Task_ParsingData/
  - Parse dữ liệu nhận từ module vân tay và BBB
  - Verify checksum
  - Chuyển đổi timestamp, tên thành viên, trạng thái bảo mật

- Core/Src/Comm_BBB/
  - Giao tiếp UART với BBB
  - Gửi trạng thái hệ thống như xác thực thành công, thất bại, đăng ký mới, lock
  - Nhận dữ liệu từ BBB: timestamp, tên thành viên, yêu cầu cấp ID, trạng thái unblock

- Core/Src/Task_Display/
  - Quản lý OLED
  - Chạy các màn hình standby, quét, pass, fail, lock, enroll

- Core/Src/Task_UserButton/
  - Theo dõi nút người dùng
  - Kích hoạt enrollment khi nhấn giữ lâu hơn 3 giây

---

## Các task chính trong FreeRTOS

### 1. Button_Task

- Đọc trạng thái nút bấm PA0
- Nếu nhấn giữ ≥ 3 giây thì kích hoạt tiến trình đăng ký vân tay mới
- Được chạy định kỳ theo polling 50 ms

### 2. ParsingRXData_Task

- Chờ nhận notification từ luồng UART
- Phân tách dữ liệu từ module vân tay
- Validate checksum
- Chuyển dữ liệu đến task xử lý vân tay hoặc parsing BBB data

### 3. Fingerprint_StateMachine_Task

- Nền tảng xử lý chính của hệ thống
- Chạy mọi trạng thái FSM:
  - chụp ảnh
  - chuyển ảnh thành feature
  - tìm kiếm trong thư viện
  - xác nhận vân tay
  - đăng ký mới
  - khóa tạm thời / khóa vĩnh viễn

### 4. Display_Task

- Cập nhật trạng thái màn hình OLED theo FSM
- Hiển thị:
  - standby
  - scanning
  - pass
  - fail
  - temp lock
  - infinity lock
  - enroll process

---

## Tính năng chính của Project

### 1. Nhận diện vân tay

- Gửi lệnh GEN_IMG (chụp ảnh)
- Chuyển ảnh sang template với IMG_2_TZ
- Tìm kiếm 1:N trong library vân tay bằng SEARCH
- Trích xuất ID và độ khớp
- Xử lý các trạng thái trả về từ module vân tay như thành công, không có ngón tay, sai vân tay, vân tay đã xác minh trước đó

### 2. Đăng ký vân tay mới

- Người dùng bấm giữ nút để bắt đầu enrollment
- STM32 yêu cầu BBB cấp ID dành cho người dùng mới
- Quá trình bao gồm:
  - chụp vân tay lần 1
  - chuyển ảnh thành template lần 1
  - yêu cầu nhấc tay ra khỏi cảm biến
  - chụp lần 2
  - chuyển ảnh thành template lần 2
  - tạo model
  - lưu model vào flash của cảm biến
- Nếu thành công, hệ thống báo thêm fingerprint mới và hiển thị màn hình pass

### 3. Quản lý trạng thái khóa hệ thống

Hệ thống có cơ chế khóa dựa trên số lần nhận diện sai:

- 5 lần sai: khóa 5 phút
- 10 lần sai: khóa 10 phút
- 15 lần sai: khóa vô hạn

Khi ở trạng thái khóa:

- OLED hiển thị countdown thời gian hoặc trạng thái khóa vô hạn
- Gửi thông tin lên BBB
- Chờ đến khi được mở khóa từ phía BBB

### 4. Giao tiếp với BBB

- UART1 dùng để giao tiếp với BBB
- Dữ liệu truyền đi bao gồm:
  - trạng thái hệ thống
  - ID khớp
  - trạng thái confirm
  - yêu cầu cấp ID enrollment
  - báo lỗi enrollment

- Dữ liệu nhận về từ BBB bao gồm:
  - timestamp dạng #TS=...
  - thông tin tên thành viên khi ở trạng thái tìm kiếm người dùng
  - ID có thể thêm mới khi yêu cầu enroll
  - lệnh unlock khỏi block infinity

### 5. Hiển thị trên OLED SSD1306

Màn hình OLED hiển thị nhiều trạng thái, bao gồm:

- Standby: thời gian, ngày tháng, trạng thái sẵn sàng quét
- Scanning: animation quét vân tay
- Pass: chào mừng người dùng và tên thành viên
- Fail: thông báo thử lại
- Temp lock: hiển thị thời gian còn lại
- Infinity lock: thông báo phải liên hệ bảo vệ / PIC
- Enroll: hiển thị quá trình thêm vân tay từng bước

### 6. Đồng bộ thời gian và dữ liệu người dùng

- Khi nhận chuỗi timestamp từ BBB, hệ thống chuyển đổi Unix timestamp sang thời gian thực và cập nhật màn hình standby
- Khi nhận tên thành viên, hệ thống lưu tên hiển thị tương ứng với người dùng đã xác thực

### 7. Debug và trace

- Dùng SEGGER RTT để log dữ liệu debug qua terminal
- Dùng SEGGER SystemView để trace task và hoạt động thời gian thực
- Có logging cho các trạng thái như gửi lệnh, nhận packet, timeout, block, enroll, pass/fail

---

## Luồng hoạt động chính

1. MCU khởi động và cấu hình clock, UART, I2C, GPIO
2. Tạo các task FreeRTOS
3. Bộ đếm thời gian và giao tiếp được bắt đầu
4. Cảm biến vân tay ở chế độ quét liên tục
5. Khi có ngón tay đặt lên cảm biến:
   - chụp ảnh
   - tạo template
   - search trong thư viện
6. Nếu tìm thấy ID khớp:
   - hiển thị pass
   - gửi trạng thái tới BBB
7. Nếu không khớp:
   - hiển thị fail
   - tăng số lần sai
   - có thể khóa tạm thời hoặc khóa vĩnh viễn
8. Nếu user nhấn giữ nút dài:
   - bắt đầu enrollment mới
9. Khi enrollment xong:
   - gửi báo cáo về BBB
   - chuyển về trạng thái standby

---

## Cấu trúc thư mục chính

```text
Security_service_STM32/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       ├── main.c
│       ├── Comm_BBB/
│       ├── Task_Display/
│       ├── Task_FingerPrint/
│       ├── Task_ParsingData/
│       └── Task_UserButton/
├── Drivers/
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
├── 3rdParty/
│   └── FreeRTOS/
├── MDK-ARM/
├── STM32CubeIDE/
├── Security_service_STM32.ioc
├── STM32F411VETX_FLASH.ld
├── STM32F411VETX_RAM.ld
└── README.md
```

---

## Hướng dẫn build / chạy

### Với STM32CubeIDE

1. Mở dự án trong STM32CubeIDE
2. Chọn đúng toolchain và định nghĩa chip STM32F411VETx
3. Build project
4. Flash firmware lên board qua ST-Link hoặc J-Link

### Với Keil MDK

1. Mở file .uvprojx trong thư mục MDK-ARM
2. Build project
3. Flash lên board

> Lưu ý: project hiện đang có cấu hình J-Link / ST-Link đã được định nghĩa sẵn; cần đảm bảo debugger và boot mode phù hợp với board.

---

## Ghi chú kỹ thuật

- Mã nguồn dựa theo lập trình HAL của STM32
- Hệ thống chạy theo mô hình event-driven + task-based với Real-Time OS
- Phần giao tiếp vân tay sử dụng packet protocol dạng header + address + packet ID + length + payload + checksum
- Mỗi task có nhiệm vụ độc lập: nhận dữ liệu, parsing, FSM, display
- Dự án này tập trung vào hệ thống kiểm soát truy cập dựa trên vân tay cho thiết bị truy cập cổng / kho / khu vực an ninh

---

## Kết luận

Project này là một ứng dụng embedded hoàn chỉnh để triển khai hệ thống nhận diện vân tay, giám sát trạng thái bằng OLED, kết nối với BBB và kiểm soát truy cập dựa trên logic khóa/tạm khóa. Nó thể hiện rõ sự kết hợp giữa điều khiển vi điều khiển STM32, FreeRTOS, giao tiếp UART/I2C và xử lý FSM phức tạp cho cảm biến vân tay.
