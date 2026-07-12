#include "task_uart_FingerPrint.h"
#include "SEGGER_RTT.h"
#include "../Task_Display/gui.h"
#include "stdbool.h"
#include "task.h"
#include "../Task_ParsingData/task_ParsingData.h"
#include "../Comm_BBB/Comm_BBB.h"

extern UART_HandleTypeDef huart2;
extern uint8_t UART2_rx_data;
extern TaskHandle_t task_PD_handler, task_FP_handler;
extern char msg[128];

static uint8_t s_u8CountFPOK = 0;
static bool s_u8CountFPOKEnableFlag = false;
static uint8_t s_u8CountFPBAD = 0;
static bool s_u8CountFPBADEnableFlag = false;
static bool s_u8BackToStandByFlag = false;

RingBuffer_t stRXRingBuffer = { .head = 0, .tail = 0 }; // for ISR

// Global TX
Fingerprint_Packet_t g_stFingerPrintTXData;
// Global RX
Fingerprint_Packet_t g_stFingerPrintRXData;
volatile uint8_t g_au8RXFingerPrintBufferSize = 0;
volatile uint8_t g_au8RXFingerPrintBuffer[RX_BUFFER_SIZE];
// Data Ready flag
volatile bool bDataReady = false;

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
	HAL_StatusTypeDef status = HAL_UART_Transmit_IT(&huart2, (uint8_t*) &g_stFingerPrintTXData, total_transmit_bytes);
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

			// SEGGER_RTT_WriteString(0, "Receive a packet FingerPrint RX data!\n");
			sprintf(msg, "[My Debug] Receive a packet FingerPrint RX data!\n");
			SEGGER_SYSVIEW_PrintfTarget(msg);

			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xTaskNotifyFromISR(task_PD_handler, PARSING_DATA_SRC_FINGERPRINT_BIT, eSetBits, &xHigherPriorityTaskWoken);
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
	uint8_t u8confirmstate;
	uint16_t u16matchedID = 0;
	uint16_t u16matchScore = 0;
	static uint32_t s_u32TaskNotifyValue = 0;

	// DEBUG: Initialize state machine on first call
	if (g_FingerState == FSM_NONE) {
		// printf("[FSM] Initializing Fingerprint State Machine...\r\n");
		// SEGGER_RTT_WriteString(0, "Init...");
		sprintf(msg, "[My Debug] Init...\n");
		SEGGER_SYSVIEW_PrintfTarget(msg);

		// Reset counter Display
		s_u8CountFPOK = 0;
		s_u8CountFPOKEnableFlag = false;
		s_u8CountFPBAD = 0;
		s_u8CountFPBADEnableFlag = false;
		s_u8BackToStandByFlag = false;

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
			if (s_u8BackToStandByFlag == true)
			{
				s_u8BackToStandByFlag = false;
				SetDisplayState(SCREEN_STATE_STANDBY);
			}

			// Gửi lệnh 01H (Không cần tham số data)
			sprintf(msg, "[My Debug] Send GEN_IMG - 01H\n");
			SEGGER_SYSVIEW_PrintfTarget(msg);

			Fingerprint_SendCommand(0x01, NULL, 0);
			g_FingerState = FSM_FINGER_WAIT_GENIMG; // Chuyển sang chờ
		}
		break;

		case FSM_FINGER_WAIT_GENIMG:
		{
			if (xTaskNotifyWait(0, FINGERPRINT_RX_NEW_PACKET_VALUE, &s_u32TaskNotifyValue, pdMS_TO_TICKS(1000)) == pdTRUE)	
			{
				if (s_u32TaskNotifyValue & FINGERPRINT_RX_NEW_PACKET_VALUE)
				{
					sprintf(msg, "[My Debug] WAIT_GENIMG receive data\n");
					SEGGER_SYSVIEW_PrintfTarget(msg);

					u8confirmstate = g_stFingerPrintRXData.payload[0];
	
					if (u8confirmstate == 0x00) // 0x00: Có ngón tay & chụp thành công
					{
						// Comm BBB: There's a Finger!!!
						CommBBB_SendStateInfo((uint8_t)g_FingerState, NONE_MATCHED_FP_ID, NONE_MATCHED_FP_SCORE);
						
						// Display
						SetDisplayState(SCREEN_STATE_PROCESSING);

						g_FingerState = FSM_FINGER_SEND_IMG2TZ; // Đi tiếp bước 2

						taskYIELD();
					}
					else
					{
						if (u8confirmstate == 0x02) // 0x02: Không có ngón tay trên kính
						{
							// Không có ngón tay thì nghỉ 1 lát (VD: 100ms) rồi quét lại
							g_FingerState = FSM_FINGER_DELAY;
						} 
						else 
						{
							// Lỗi khác (chụp lỗi, bẩn kính...), quét lại từ đầu
							g_FingerState = FSM_FINGER_SEND_GENIMG;
						}

						// Display
						SetDisplayState(SCREEN_STATE_STANDBY);
						taskYIELD();
					}
				}
			}
			else 	// timeout
            {
				sprintf(msg, "[My Debug] WAIT_GENIMG timeout, resend GEN_IMG - 01H\n");
				SEGGER_SYSVIEW_PrintfTarget(msg);

                g_FingerState = FSM_FINGER_SEND_GENIMG; // Try again!
            }
		}
		break;

		// ---------------------------------------------------------
		// BƯỚC 2: TẠO ĐẶC TRƯNG (IMG_2_TZ - 02H) VÀO BUFFER 1
		// ---------------------------------------------------------
		case FSM_FINGER_SEND_IMG2TZ:
		{
			sprintf(msg, "[My Debug] Send IMG_2_TZ - 02H, Chon CharBuffer1\n");
			SEGGER_SYSVIEW_PrintfTarget(msg);

			uint8_t bufferID[1] = { 0x01 }; // Tham số: Chọn CharBuffer1
			Fingerprint_SendCommand(0x02, bufferID, 1);
			g_FingerState = FSM_FINGER_WAIT_IMG2TZ;
		}
		break;

		case FSM_FINGER_WAIT_IMG2TZ:
		{
			if (xTaskNotifyWait(0, FINGERPRINT_RX_NEW_PACKET_VALUE, &s_u32TaskNotifyValue, pdMS_TO_TICKS(1000)) == pdTRUE)	
			{
				if (s_u32TaskNotifyValue & FINGERPRINT_RX_NEW_PACKET_VALUE)
				{
					u8confirmstate = g_stFingerPrintRXData.payload[0];
					
					if (u8confirmstate == 0x00) // Tạo đặc trưng thành công
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
		}
		break;

		// ---------------------------------------------------------
		// BƯỚC 3: TÌM KIẾM 1:N (SEARCH - 04H)
		// ---------------------------------------------------------
		case FSM_FINGER_SEND_SEARCH:
		{
			// Tham số cho lệnh Search: BufferID (1), StartPage (0x00, 0x00), PageNum (0x01, 0x2C - Tìm tối đa 300 vân tay)
			sprintf(msg, "[My Debug] Send SEARCH - 04H, BufferID (1), StartPage (0x00, 0x00), PageNum (0x01, 0x2C) search 300 FPs\n");
			SEGGER_SYSVIEW_PrintfTarget(msg);

			uint8_t au8searchParams[5] = { 0x01, 0x00, 0x00, 0x01, 0x2C };
			Fingerprint_SendCommand(0x04, au8searchParams, 5);
			g_FingerState = FSM_FINGER_WAIT_SEARCH;
		}
		break;

		case FSM_FINGER_WAIT_SEARCH:
		{
			if (xTaskNotifyWait(0, FINGERPRINT_RX_NEW_PACKET_VALUE, &s_u32TaskNotifyValue, pdMS_TO_TICKS(1000)) == pdTRUE)	
			{
				if (s_u32TaskNotifyValue & FINGERPRINT_RX_NEW_PACKET_VALUE)
				{
					u8confirmstate = g_stFingerPrintRXData.payload[0];

					if (u8confirmstate == 0x00) // 0x00: TÌM THẤY TRONG THƯ VIỆN!
					{
						u16matchedID = (g_stFingerPrintRXData.payload[1] << 8)
										| g_stFingerPrintRXData.payload[2];
						u16matchScore = (g_stFingerPrintRXData.payload[3] << 8)
										| g_stFingerPrintRXData.payload[4];

						// => BẠN CHECK TẠI ĐÂY: Nếu u16matchedID == 0 nghĩa là ngón trỏ của bạn!
						sprintf(msg, "[Conclusion] Xac thuc thanh cong! ID cua ban la: %d, Diem khop: %d\n", u16matchedID, u16matchScore);
						SEGGER_SYSVIEW_PrintfTarget(msg);

						// Comm BBB: result of Finger: Valid or not?!?
						CommBBB_SendStateInfo((uint8_t)g_FingerState, u16matchedID, u8confirmstate);

						// Display
						s_u8CountFPOKEnableFlag = true;
						s_u8CountFPOK = 0;
					}
					else if (u8confirmstate == 0x17)
					{
						// printf("Van tay da xac nhan truoc do. Hay bo tay ra va dat lai len Sensor! \n");
						sprintf(msg, "[Conclusion] Van tay da xac nhan truoc do. Hay bo tay ra va dat lai len Sensor! (neu muon) \n");
						SEGGER_SYSVIEW_PrintfTarget(msg);

						// Comm BBB: result of Finger: Valid or not?!?
						CommBBB_SendStateInfo((uint8_t)g_FingerState, u16matchedID, u8confirmstate);

						if (s_u8CountFPOKEnableFlag)
						{
							s_u8CountFPOK++;
							if (s_u8CountFPOK >= MAX_CONFIRMATION_CNT_FINGER_PRINT)
							{
								s_u8CountFPOK = 0;
								s_u8CountFPOKEnableFlag = false;

								SetDisplayState(SCREEN_STATE_PASS);
								vTaskDelay(pdMS_TO_TICKS(2000));

								s_u8BackToStandByFlag = true;
							}
						}

						if (s_u8CountFPBADEnableFlag)
						{
							s_u8CountFPBAD++;
							if (s_u8CountFPBAD >= MAX_CONFIRMATION_CNT_FINGER_PRINT)
							{
								s_u8CountFPBAD = 0;
								s_u8CountFPBADEnableFlag = false;
								
								SetDisplayState(SCREEN_STATE_FAIL);
								vTaskDelay(pdMS_TO_TICKS(2000));

								s_u8BackToStandByFlag = true;
							}
						}
					}
					else if (u8confirmstate == 0x09) // 0x09: KHÔNG TÌM THẤY (Ngón tay lạ)
					{
						// printf("Van tay sai! Khong tim thay trong thu vien.\n");
						sprintf(msg, "[Conclusion] Van tay sai! Khong tim thay trong thu vien.\n");
						SEGGER_SYSVIEW_PrintfTarget(msg);

						// Comm BBB: result of Finger: Valid or not?!?
						CommBBB_SendStateInfo((uint8_t)g_FingerState, u16matchedID, u8confirmstate);

						// Display
						s_u8CountFPBADEnableFlag = true;
						s_u8CountFPBAD = 0;
					}

					// Xử lý xong, bắt buộc phải đợi 1 lát (chờ người dùng rút ngón tay ra)
					g_FingerState = FSM_FINGER_DELAY;
				}
			}
		}
		break;

		// ---------------------------------------------------------
		// BƯỚC ĐỆM: NGHỈ NGƠI NON-BLOCKING (DELAY)
		// ---------------------------------------------------------
		case FSM_FINGER_DELAY:
		{
			// Nghỉ 100ms trước khi tiếp tục chu trình quét mới

			sprintf(msg, "[My Debug] FingerPrint TX-RX delay 100ms\n");
			SEGGER_SYSVIEW_PrintfTarget(msg);
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
		ProcessFingerPrintApplication();

		// --------------- End of function ---------------
		// vTaskDelay(pdMS_TO_TICKS(10));
	}
}
