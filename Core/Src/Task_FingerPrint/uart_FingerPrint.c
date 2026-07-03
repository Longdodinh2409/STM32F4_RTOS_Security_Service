#include <Task_FingerPrint/task_uart_FingerPrint.h>

extern UART_HandleTypeDef huart2;
extern uint8_t UART2_rx_data;
extern TaskHandle_t task_PD_handler;

RingBuffer_t stRXRingBuffer = { .head = 0, .tail = 0 }; // for ISR
uint8_t g_au8RXFingerPrintBufferSize = 0;
uint8_t g_au8RXFingerPrintBuffer[RX_BUFFER_SIZE];
static uint32_t wait_start_time = 0;

// Global TX
Fingerprint_Packet_t g_stFingerPrintTXData;
// Global RX
Fingerprint_Packet_t g_stFingerPrintRXData;
// Data Ready flag
volatile bool bDataReady = false;
bool bNewPacketRX = false;

//testing
// Biến toàn cục hoặc tĩnh quản lý FSM
Fingerprint_State_t g_FingerState = FSM_NONE;

void Init_UART2_FingerPrint(void)
{
	HAL_UART_Receive_IT(&huart2, &UART2_rx_data, 1);
}

void Fingerprint_SendCommand(uint8_t instructionCode, uint8_t *params, uint8_t param_len) 
{
	uint32_t sum = 0;

	// 1. Header, Big-Endian
	g_stFingerPrintTXData.header[0] = 0xEF;
	g_stFingerPrintTXData.header[1] = 0x01;
	g_stFingerPrintTXData.address[0] = 0xFF;
	g_stFingerPrintTXData.address[1] = 0xFF;
	g_stFingerPrintTXData.address[2] = 0xFF;
	g_stFingerPrintTXData.address[3] = 0xFF;
	g_stFingerPrintTXData.packet_ID = 0x01; // Command Packet

	// 2. Length (Instruction 1 byte + Params + Checksum 2 bytes)
	uint16_t package_len = 1 + param_len + 2;
	g_stFingerPrintTXData.length[0] = (package_len >> 8) & 0xFF;
	g_stFingerPrintTXData.length[1] = package_len & 0xFF;

	// 3. Instruction & Parameters
	g_stFingerPrintTXData.payload[0] = instructionCode;
	if (param_len > 0 && params != NULL) {
		memcpy(&g_stFingerPrintTXData.payload[1], params, param_len);
	}

	// 4. Checksum = sum(packet_ID + Length + Payload_Content)
	sum = g_stFingerPrintTXData.packet_ID + g_stFingerPrintTXData.length[0]
			+ g_stFingerPrintTXData.length[1];
	for (int i = 0; i < (package_len - 2); i++) {
		sum += g_stFingerPrintTXData.payload[i];
	}

	uint16_t checksum_offset = package_len - 2;
	g_stFingerPrintTXData.payload[checksum_offset] = (sum >> 8) & 0xFF;
	g_stFingerPrintTXData.payload[checksum_offset + 1] = sum & 0xFF;

	// 6. Send UART
	uint16_t total_transmit_bytes = 9 + package_len;
	// HAL_StatusTypeDef status = HAL_UART_Transmit_IT(&huart2, (uint8_t*) &g_stFingerPrintTXData, total_transmit_bytes);
	HAL_StatusTypeDef status = HAL_UART_Transmit(&huart2, (uint8_t*) &g_stFingerPrintTXData, total_transmit_bytes, portMAX_DELAY);

	// Debug: Check if transmit succeeded
	if (status == HAL_OK) {
		printf("[TX] Sent instruction 0x%02X, %d bytes\r\n", instructionCode,
				total_transmit_bytes);
	} else {
		printf("[TX_ERROR] Instruction 0x%02X failed! Status=%d\r\n",
				instructionCode, status);
	}
}

// Extract packet from array buffer to packet structure
// Return: true if extraction successful, false if insufficient data
// Input: buffer_ptr - pointer to receive buffer array
//        buffer_len - number of bytes available in buffer
//        packet - pointer to output Fingerprint_Packet_t structure
bool Fingerprint_ExtractPacketFromRingBuffer(uint8_t *buffer_ptr,
		uint16_t buffer_len, Fingerprint_Packet_t *packet) {
	uint16_t packet_length;
	uint16_t total_packet_size;
	uint16_t i;

	// 1. Need at least 9 bytes (Header + Address + Packet_ID + Length)
	if (buffer_len < 9) {
		return false;
	}

	// 2. Check Header (0xEF01) at the start position
	if (buffer_ptr[0] != 0xEF || buffer_ptr[1] != 0x01) {
		return false;
	}

	// 3. Get length (2 bytes, Big-Endian) at offset 7-8
	uint8_t length_high = buffer_ptr[7];
	uint8_t length_low = buffer_ptr[8];
	packet_length = ((uint16_t) length_high << 8) | length_low;

	// 4. Check packet_length is valid (minimum 2 bytes checksum, maximum payload_size)
	if (packet_length < 2 || packet_length > TX_PAYLOAD_MAX_SIZE) {
		return false;
	}

	// 5. Calculate total packet size: 9 (header+addr+pktID+len) + payload
	total_packet_size = 9 + packet_length;

	// 6. Check if there is enough data in the buffer
	if (buffer_len < total_packet_size) {
		return false;
	}

	// 7. Extract packet from array buffer to packet structure
	for (i = 0; i < 2; i++) {
		packet->header[i] = buffer_ptr[i];
	}

	for (i = 0; i < 4; i++) {
		packet->address[i] = buffer_ptr[2 + i];
	}

	packet->packet_ID = buffer_ptr[6];
	packet->length[0] = length_high;
	packet->length[1] = length_low;

	// 8. Copy payload
	for (i = 0; i < packet_length; i++) {
		packet->payload[i] = buffer_ptr[9 + i];
	}

	return true;
}

void ProcessFingerPrintRXData(void* param) {
	bool bIsItGood = false;

	while(1)
	{
		// if (bDataReady == true) 
		if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdTRUE)
		{
			bDataReady = false;
	
			__disable_irq();
			// 1. Parse data from array buffer
			// Note: stRXRingBuffer.head indicates the number of bytes available in g_au8RXFingerPrintBuffer
			bIsItGood = Fingerprint_ExtractPacketFromRingBuffer(g_au8RXFingerPrintBuffer, 
																g_au8RXFingerPrintBufferSize,
																&g_stFingerPrintRXData);
			__enable_irq();
	
			if (bIsItGood == false) {
				printf("/n failed extract./n ");
				return;
			}
	
			// 2. Checksum
			if (Fingerprint_VerifyChecksum(&g_stFingerPrintRXData) == false) {
				printf("/n wrong checksum./n ");
				return;
			}
	
			/* ------------- Important flag -------------*/
			bNewPacketRX = true;
			/* ------------- -------------- -------------*/
		}
	}
}

bool Fingerprint_VerifyChecksum(const Fingerprint_Packet_t *packet) {
	uint32_t calculated_sum = 0; // Use uint32_t to avoid overflow when accumulating

	// 1. Calculate the length of the sequence (Payload + Checksum) - Big Endian
	uint16_t packet_len = (packet->length[0] << 8) | packet->length[1];

	// 2. Actual data length (excluding 2 bytes checksum)
	uint16_t data_len = packet_len - 2;

	// 3. Accumulate Packet Type and 2 bytes Length
	calculated_sum += packet->packet_ID;
	calculated_sum += packet->length[0];
	calculated_sum += packet->length[1];

	// 4. Accumulate bytes in Payload (excluding 2 bytes checksum at the end)
	for (uint16_t i = 0; i < data_len; i++) {
		calculated_sum += packet->payload[i];
	}

	// 5. Get actual Checksum from sensor (Located at end of payload array)
	uint16_t received_checksum = (packet->payload[data_len] << 8)
			| packet->payload[data_len + 1];

	// 6. Compare lower 16-bit of calculated_sum with received_checksum
	return ((calculated_sum & 0xFFFF) == received_checksum);
}

void FingerPrint_UART_RxCallback(uint8_t rx_byte) {
	// Static variables to track current packet state
	static uint16_t byte_count = 0;
	static uint16_t expected_total_length = 0;
	static uint8_t length_high = 0; // Temporarily store byte 8 (Length H)

	// 1. Always push received byte into Ring Buffer first
	stRXRingBuffer.buffer[stRXRingBuffer.head] = rx_byte;
	stRXRingBuffer.head = (stRXRingBuffer.head + 1) % RX_BUFFER_SIZE;

	// Increment byte counter for current packet
	byte_count++;

	// 2. Get length parameter at byte 8 and 9 (1-indexed)
	if (byte_count == 8) {
		// Save Length High byte
		length_high = rx_byte;
	} else if (byte_count == 9) {
		// Combine Length High and Length Low (byte 9)
		uint16_t payload_length = (length_high << 8) | rx_byte;

		// Total packet length = 9 header bytes + payload length
		expected_total_length = 9 + payload_length;

		// (Optional) Protection: If calculated length exceeds buffer capacity, reset
		if (expected_total_length > 256) {
			byte_count = 0;
			expected_total_length = 0;
			stRXRingBuffer.tail = stRXRingBuffer.head;
		}
	}

	// 3. If received enough bytes for the packet
	if ((byte_count >= 9) && (byte_count == expected_total_length)) {

		// Only copy if Main thread finished processing previous packet (prevent overwrite)
		if (bDataReady == false) {
			// Get all data from Ring Buffer (from tail to head) and copy to flat array
			for (uint16_t i = 0; i < expected_total_length; i++) {
				g_au8RXFingerPrintBuffer[i] =
						stRXRingBuffer.buffer[stRXRingBuffer.tail];
				stRXRingBuffer.tail =
						(stRXRingBuffer.tail + 1) % RX_BUFFER_SIZE;
			}

			// Save size and set flag for ProcessMain
			g_au8RXFingerPrintBufferSize = expected_total_length;

			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xTaskNotifyFromISR(task_PD_handler, 0, eNoAction, &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			
			bDataReady = true;
		} else {
			// Case where bDataReady is still true (Main hasn't processed yet)
			// You can choose to advance tail to discard old packet in Ring Buffer
			// or report Overrun error depending on system logic.
			stRXRingBuffer.tail = (stRXRingBuffer.tail + expected_total_length)
					% RX_BUFFER_SIZE;
		}

		// 4. Reset counter to prepare for next packet
		byte_count = 0;
		expected_total_length = 0;
	}
}

void ProcessFingerPrintApplication(void)
{
	uint8_t confirm_code = g_stFingerPrintRXData.payload[0];
	uint16_t matchedID = 0;
	uint16_t matchScore = 0;

	// DEBUG: Initialize state machine on first call
	if (g_FingerState == FSM_NONE) {
		// printf("[FSM] Initializing Fingerprint State Machine...\r\n");

		// initialize FingerPrint sensor
		g_FingerState = FSM_FINGER_SEND_GENIMG;
	}

	switch (g_FingerState) 
	{
		// ---------------------------------------------------------
		// BƯỚC 1: CHỤP ẢNH VÂN TAY (GEN_IMG - 01H)
		// ---------------------------------------------------------
		case FSM_FINGER_SEND_GENIMG:
		{
			// Gửi lệnh 01H (Không cần tham số data)
			Fingerprint_SendCommand(0x01, NULL, 0);
			g_FingerState = FSM_FINGER_WAIT_GENIMG; // Chuyển sang chờ
			
			wait_start_time = HAL_GetTick();
		}
		break;

		case FSM_FINGER_WAIT_GENIMG:
		{
			if (bNewPacketRX) // Cờ có dữ liệu từ luồng Interrupt RX
			{
				bNewPacketRX = false; // Xóa cờ

				if (confirm_code == 0x00) // 0x00: Có ngón tay & chụp thành công
				{
					g_FingerState = FSM_FINGER_SEND_IMG2TZ; // Đi tiếp bước 2
				} 
				else if (confirm_code == 0x02) // 0x02: Không có ngón tay trên kính
				{
					// Không có ngón tay thì nghỉ 1 lát (VD: 50ms) rồi quét lại
					g_FingerState = FSM_FINGER_DELAY;
				} 
				else 
				{
					// Lỗi khác (chụp lỗi, bẩn kính...), quét lại từ đầu
					g_FingerState = FSM_FINGER_SEND_GENIMG;
				}
			}
			else 
            {
                // Logic Timeout: Kiểm tra xem đã quá thời gian chờ chưa
                if ((HAL_GetTick() - wait_start_time) > FINGERPRINT_TIMEOUT_MS)
                {
                    printf("[WARN] Sensor Timeout! Resetting FSM...\r\n");
                    // Hủy gói tin cũ, reset buffer nếu cần thiết
                    bDataReady = false;
                    g_FingerState = FSM_FINGER_SEND_GENIMG; // Thử lại từ đầu
                }
            }
		}
		break;

		// ---------------------------------------------------------
		// BƯỚC 2: TẠO ĐẶC TRƯNG (IMG_2_TZ - 02H) VÀO BUFFER 1
		// ---------------------------------------------------------
		case FSM_FINGER_SEND_IMG2TZ:
		{
			uint8_t bufferID[1] = { 0x01 }; // Tham số: Chọn CharBuffer1
			Fingerprint_SendCommand(0x02, bufferID, 1);
			g_FingerState = FSM_FINGER_WAIT_IMG2TZ;
		}
		break;

		case FSM_FINGER_WAIT_IMG2TZ:
		{
			if (bNewPacketRX) 
			{
				bNewPacketRX = false;
				
				if (confirm_code == 0x00) // Tạo đặc trưng thành công
				{
					g_FingerState = FSM_FINGER_SEND_SEARCH; // Đi tới tìm kiếm
				}
				else
				{
					// Tạo lỗi (ngón tay ướt/nhoè), quay lại chờ ngón tay mới
					g_FingerState = FSM_FINGER_SEND_GENIMG;
				}
			}
		}
		break;

		// ---------------------------------------------------------
		// BƯỚC 3: TÌM KIẾM 1:N (SEARCH - 04H)
		// ---------------------------------------------------------
		case FSM_FINGER_SEND_SEARCH:
		{
			// Tham số cho lệnh Search: BufferID (1), StartPage (0x00, 0x00), PageNum (0x01, 0x2C - Tìm tối đa 300 vân tay)
			uint8_t searchParams[5] = { 0x01, 0x00, 0x00, 0x01, 0x2C };
			Fingerprint_SendCommand(0x04, searchParams, 5);
			g_FingerState = FSM_FINGER_WAIT_SEARCH;
		}
		break;

		case FSM_FINGER_WAIT_SEARCH:
		{
			if (bNewPacketRX) 
			{
				bNewPacketRX = false;

				if (confirm_code == 0x00) // 0x00: TÌM THẤY TRONG THƯ VIỆN!
				{
					matchedID = (g_stFingerPrintRXData.payload[1] << 8)
									| g_stFingerPrintRXData.payload[2];
					matchScore = (g_stFingerPrintRXData.payload[3] << 8)
									| g_stFingerPrintRXData.payload[4];

					// => BẠN CHECK TẠI ĐÂY: Nếu matchedID == 0 nghĩa là ngón trỏ của bạn!
					printf("Xac thuc thanh cong! ID cua ban la: %d, Diem khop: %d\n", matchedID, matchScore);

					// Mở cửa, bật còi báo, v.v...
					// UnlockDoor();
				}
				else if (confirm_code == 0x17)
				{
					printf("Van tay da xac nhan truoc do. Hay bo tay ra va dat lai len Sensor! \n");
				}
				else if (confirm_code == 0x09) // 0x09: KHÔNG TÌM THẤY (Ngón tay lạ)
				{
					printf("Van tay sai! Khong tim thay trong thu vien.\n");
				}

				// Xử lý xong, bắt buộc phải đợi 1 lát (chờ người dùng rút ngón tay ra)
				g_FingerState = FSM_FINGER_DELAY;
			}
		}
		break;

		// ---------------------------------------------------------
		// BƯỚC ĐỆM: NGHỈ NGƠI NON-BLOCKING (DELAY)
		// ---------------------------------------------------------
		case FSM_FINGER_DELAY:
		{
			// // Nghỉ 100ms trước khi tiếp tục chu trình quét mới
			// // Việc này giúp module rảnh rang không bị quá tải lệnh liên tục
			g_FingerState = FSM_FINGER_SEND_GENIMG; // Quay lại từ đầu
			vTaskDelay(pdMS_TO_TICKS(100));
		}
		break;
	}
}

void Fingerprint_StateMachine_Task(void* param)
{
	while (1)
	{
		// Step 1: Parsing data & set bNewPacketRX flag
		// ProcessFingerPrintRXData();

		// Step 2: Using parsed data for Finger Print processing
		ProcessFingerPrintApplication();

		// --------------- End of function ---------------
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
