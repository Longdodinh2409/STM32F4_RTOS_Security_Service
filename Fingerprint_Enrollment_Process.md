# Tổng hợp quy trình thêm member mới với enroll new fingerprint và giao tiếp BBB

## 1. Mục tiêu của luồng

Luồng này thực hiện quy trình “new fingerprint enrollment” theo mô hình state machine của STM32F4:

- STM32F4 bắt đầu quá trình enroll
- STM32F4 yêu cầu BBB cấp `member ID` mới cho người dùng
- BBB trả về `member ID` qua frame `#State=...,#ID=...`
- STM32F4 lấy `ID` đó để đi tiếp vào quá trình capture / register / store model của sensor
- Khi hoàn tất, STM32F4 gửi kết quả về BBB theo trạng thái mới như `FSM_NEW_FINGERPRINT_ADDED`

---

## 2. Bước đầu tiên: bắt đầu enrollment

Khi có lệnh bắt đầu thêm member mới, firmware sẽ vào nhánh `ENROLL_START` của fingerprint task.

### Vai trò trong bước này

- đặt màn hình hiển thị ở trạng thái enroll
- gửi yêu cầu BBB cấp `member ID` mới bằng `CommBBB_RequestEnrollID()`
- chờ notify từ parsing task để biết BBB đã trả `ID` hợp lệ

### Luồng thực tế

1. `Fingerprint_StartEnrollment()` đổi trạng thái sang `ENROLL_START`
2. `ProcessFingerPrintEnrollmentApplication()` chạy nhánh `ENROLL_START`
3. nếu chưa gửi request lần nào thì gọi `CommBBB_RequestEnrollID()`
4. task fingerprint chờ `xTaskNotifyWait()` với timeout `ENROLL_START_TIMEOUT_MS`
5. nếu đến timeout mà không nhận được notify thì:
   - báo lỗi và gửi `FSM_ENROLL_ID_ERROR`
   - chuyển `g_EnrollState` sang `ENROLL_ERROR`

> Đây là điểm khác biệt lớn so với cách cũ: không còn chờ bằng `HAL_GetTick()` polling, mà task fingerprint đang block ngủ và chỉ được đánh thức khi có notify.

---

## 3. Giao tiếp với BBB theo kiểu state-based frame

### 3.1 Request ID mới

Khi cần cấp `member ID` mới cho enrollment, STM32F4 không gửi lệnh dạng `#Cmd=ReqID;` nữa mà gửi theo dạng state frame:

```c
CommBBB_SendStateInfo((uint8_t)FSM_ENROLL_REQUEST_ID, 0, 0);
```

Tức là frame truyền đi có dạng:

```text
#State=<FSM_ENROLL_REQUEST_ID>
```

### 3.2 Trường hợp lỗi cấp ID

Nếu quá thời gian chờ notify mà BBB không trả `ID`, hoặc `ID` không hợp lệ, STM32F4 gửi:

```c
CommBBB_SendStateInfo((uint8_t)FSM_ENROLL_ID_ERROR, 0, 0);
```

Tức là frame gửi đi là:

```text
#State=<FSM_ENROLL_ID_ERROR>
```

---

## 4. BBB RX callback phân nhánh dựa trên giá trị `#State`

Trong `BBB_UART_RxCpltCallback()`:

- nếu frame bắt đầu bằng `#TS` thì notify parsing timestamp
- nếu frame bắt đầu bằng `#State` thì parse giá trị `State`
- sau đó chọn bit notify phù hợp cho parsing task

### Mức routing hiện tại

- nếu `State == FSM_FINGER_WAIT_SEARCH`:
  - notify `PARSING_MEMBER_NAME_SRC_BBB_BIT`
  - dùng cho flow tìm tên member dựa trên kết quả search vân tay

- nếu `State == FSM_ENROLL_REQUEST_ID`:
  - notify `PARSING_MEMBER_ID_AVAILABLE_TO_ADD_SRC_BBB_BIT`
  - dùng cho flow “BBB cấp ID mới để thêm member”

Về mặt ý nghĩa, `State` trở thành discriminator chính để routing packet từ BBB sang task xử lý thích hợp.

---

## 5. Parsing task nhận và xử lý ID mới được BBB gán

### 5.1 Parser entry point

`ParsingRXData_Task()` đang lắng nghe các notification bit từ BBB. Khi nhận bit:

- `PARSING_MEMBER_NAME_SRC_BBB_BIT` → xử lý `ProcessParsingMemberName()`
- `PARSING_MEMBER_ID_AVAILABLE_TO_ADD_SRC_BBB_BIT` → xử lý `ProcessParsingMemberIDAvailableToAdd()`

### 5.2 Format frame cần parse

Parser `ProcessParsingMemberIDAvailableToAdd()` đang parse frame theo format:

```text
#State=<FSM_ENROLL_REQUEST_ID>,#ID=<memberID>
```

và dùng `sscanf()` để đọc:

```c
sscanf(g_acRXBufferBBB, "#State=%hhu,#ID=%hu", &parsed_state, &g_u16MemberIDAvailableToAdd);
```

Nếu `parsed_state == FSM_ENROLL_REQUEST_ID`, hệ thống sẽ:

1. gọi `Fingerprint_SetEnrollID(g_u16MemberIDAvailableToAdd)`
2. notify lại task fingerprint bằng `FINGERPRINT_BBB_ASSIGN_ID_READY_VALUE`

### 5.3 Ý nghĩa của bước này

Bước này là “cầu nối” giữa BBB và sensor enrollment:

- BBB trả `member ID` sẵn cho `new fingerprint`
- STM32F4 không tự random ID nữa
- STM32F4 chỉ lấy `ID` đã được phân bổ từ BBB để dùng trong `STORE_MODEL`

---

## 6. Sau khi có ID, fingerprint task tiếp tục enroll

Khi nhận được notify từ parser, task fingerprint kiểm tra `s_u16EnrollID`:

- nếu ID hợp lệ:
  - log `[Enroll] Assigned new ID from BBB = ...`
  - chuyển trạng thái sang `ENROLL_GET_IMG_1`
- nếu không hợp lệ:
  - gửi `FSM_ENROLL_ID_ERROR`
  - chuyển sang `ENROLL_ERROR`

### 6.1 Dòng state machine tiếp theo

Sau khi đã có ID, STM32F4 thực hiện các bước sensor enrollment chuẩn:

1. `ENROLL_GET_IMG_1`
2. `ENROLL_IMG2TZ_1`
3. `ENROLL_WAIT_REMOVE`
4. `ENROLL_GET_IMG_2`
5. `ENROLL_IMG2TZ_2`
6. `ENROLL_REG_MODEL`
7. `ENROLL_STORE_MODEL`
8. `ENROLL_SUCCESS` hoặc `ENROLL_ERROR`

### 6.2 State `STORE_MODEL`

Trong bước lưu model, `ID` được đưa vào lệnh bảo lưu bằng payload `STORE_MODEL`:

```c
uint8_t params[3] = {
    0x01,
    (uint8_t)((s_u16EnrollID >> 8) & 0xFF),
    (uint8_t)(s_u16EnrollID & 0xFF)
};
Fingerprint_SendCommand(0x06, params, 3);
```

Nghĩa là trên sensor, `member ID` mới được gán trong payload lưu model bằng 16-bit `ID_HI` / `ID_LOW`.

---

## 7. Khi enrollment thành công: gửi trạng thái mới cho BBB

Nếu `STORE_MODEL` hoàn tất thành công, hệ thống sẽ gửi thông tin xác nhận cho BBB bằng:

```c
CommBBB_SendStateInfo((uint8_t)FSM_NEW_FINGERPRINT_ADDED, s_u16EnrollID, 0x00);
```

Frame kết quả có dạng:

```text
#State=<FSM_NEW_FINGERPRINT_ADDED>,#ID=<memberID>
```

### Ý nghĩa của frame này

- `FSM_NEW_FINGERPRINT_ADDED` → đã thêm vân tay mới thành công
- `memberID` → id vừa được gán cho member mới

---

## 8. Khi enrollment thất bại

Nếu có lỗi ở bất kỳ bước nào như:

- timeout chờ BBB cấp ID
- `ID` nhận được không hợp lệ
- sensor trả lỗi trong quá trình capture / convert / register / store

thì hệ thống sẽ:

- chuyển `g_EnrollState` sang `ENROLL_ERROR`
- gửi frame error về BBB theo kiểu:

```c
CommBBB_SendStateInfo((uint8_t)FSM_ENROLL_ID_ERROR, 0, 0);
```

và trong một số trường hợp lỗi khác nếu cần nhắc user, hệ thống cũng có thể báo `g_EnrollState` tương ứng không phải `FSM_ENROLL_ID_ERROR`.

---

## 9. Tóm tắt quy trình end-to-end

### Mô hình hoạt động mới

```text
User trigger enroll
   ↓
STM32F4 -> ENROLL_START
   ↓
STM32F4 gửi request ID bằng #State=FSM_ENROLL_REQUEST_ID
   ↓
BBB cấp ID mới và trả #State=FSM_ENROLL_REQUEST_ID,#ID=<memberID>
   ↓
BBB UART callback route theo State -> notify PARSING_MEMBER_ID_AVAILABLE_TO_ADD_SRC_BBB_BIT
   ↓
Parsing task parse ID và notify fingerprint task
   ↓
Fingerprint task nhận ID, chuyển sang ENROLL_GET_IMG_1
   ↓
Capture / IMG2TZ / REG_MODEL / STORE_MODEL
   ↓
Nếu thành công -> BBB nhận #State=FSM_NEW_FINGERPRINT_ADDED,#ID=<memberID>
Nếu lỗi -> BBB nhận #State=FSM_ENROLL_ID_ERROR
```

### Điểm cần lưu ý

- `State` là khóa phân nhánh giao tiếp BBB trong hệ thống mới.
- Flow `ID assignment` phải chạy ở parsing layer, không tích hợp trực tiếp trong task fingerprint.
- `ENROLL_START` hiện đang là một bước “đợi notify hoặc timeout” thay vì polling tick như trước.

---

## 10. Kết luận

Quy trình thêm member mới hiện tại đã được đổi thành một mô hình đồng bộ hơn giữa STM32F4 và BBB:

- BBB chịu trách nhiệm cấp `member ID` mới cho enrollment
- STM32F4 chỉ lấy `ID` đó và dùng nó để lưu model vân tay
- giao tiếp giữa BBB và STM32F4 chuyển từ lệnh dạng `Cmd` sang dạng `State` để thống nhất với FSM
- `ENROLL_START` bây giờ là một trạng thái chờ notify theo event-driven pattern, phù hợp hơn với FreeRTOS và đúng với yêu cầu thiết kế mới.
