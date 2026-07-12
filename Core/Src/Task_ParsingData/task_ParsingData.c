#include "task_ParsingData.h"
#include "../Comm_BBB/Comm_BBB.h"
#include "SEGGER_RTT.h"
#include "../Task_Display/gui.h"

// Global RX
extern Fingerprint_Packet_t g_stFingerPrintRXData;
extern volatile uint8_t g_au8RXFingerPrintBufferSize;
extern volatile uint8_t g_au8RXFingerPrintBuffer[RX_BUFFER_SIZE];
// Data Ready flag
extern volatile bool bDataReady;

extern TaskHandle_t task_FP_handler;
extern bool bIsClearStandbyLayoutByNewTS;

void ParsingRXData_Task(void* param) {
	bool bIsItGood = false;

	while(1)
	{
		uint32_t ulNotificationValue = 0;
		if (xTaskNotifyWait(0, 0xFFFFFFFF, &ulNotificationValue, portMAX_DELAY) == pdTRUE)
		{
			/* Only handle notifications originating from FingerPrint RX */
			if (ulNotificationValue & PARSING_DATA_SRC_FINGERPRINT_BIT) 
			{
				bDataReady = false;
	
				__disable_irq();
				bIsItGood = ParsingData_FP_ExtractPacketFromRingBuffer(g_au8RXFingerPrintBuffer, 
																	g_au8RXFingerPrintBufferSize,
																	&g_stFingerPrintRXData);
				__enable_irq();
		
				if (bIsItGood == false) {
					// printf("/n failed extract./n ");
					SEGGER_RTT_WriteString(0, "/n Parsing FP data failed extract./n ");
					// return;
				}
		
				// 2. Checksum
				if (ParsingData_FP_VerifyChecksum(&g_stFingerPrintRXData) == false) {
					// printf("/n wrong checksum./n ");
					SEGGER_RTT_WriteString(0, "/n Parsing FP data wrong checksum /n ");
					// return;
				}
		
				/* ------------- Important flag -------------*/
				/* Notify fingerprint state machine that a new packet is ready */
				xTaskNotify(task_FP_handler, FINGERPRINT_RX_NEW_PACKET_VALUE, eSetBits);
			}

			if (ulNotificationValue & PARSING_DATA_SRC_BBB_BIT)
			{
				ProcessParsingTimeStamp(g_u32BBBReceivedValue, GMT_7);
			}
		}
	}
}

// Extract packet from array buffer to packet structure
// Return: true if extraction successful, false if insufficient data
// Input: buffer_ptr - pointer to receive buffer array
//        buffer_len - number of bytes available in buffer
//        packet - pointer to output Fingerprint_Packet_t structure
bool ParsingData_FP_ExtractPacketFromRingBuffer(const volatile uint8_t *buffer_ptr, uint16_t buffer_len, Fingerprint_Packet_t *packet) {
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

bool ParsingData_FP_VerifyChecksum(const Fingerprint_Packet_t *packet) {
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

int is_leap_year(int year) 
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

void ProcessParsingTimeStamp(uint32_t timestamp, int timezone_offset_hours) 
{
    char acTimeStamp[64];

    // Bước 1: Cộng bù múi giờ (Ví dụ VN là +7, mỗi giờ có 3600 giây)
    timestamp += (timezone_offset_hours * 3600);

    // Bước 2: Tách lấy Tổng số ngày và Số giây lẻ trong ngày
    uint32_t total_days = timestamp / 86400;
    uint32_t seconds_in_day = timestamp % 86400;

    // Bước 3: Tính Giờ, Phút, Giây
    int hour = seconds_in_day / 3600;
    int minute = (seconds_in_day % 3600) / 60;
    int second = seconds_in_day % 60;

    // Bước 4: Tính Năm
    int year = 1970;
    while (1) {
        int days_in_this_year = is_leap_year(year) ? 366 : 365;
        if (total_days >= days_in_this_year) {
            total_days -= days_in_this_year; // Trừ bớt số ngày của năm nay
            year++;                          // Tiến lên năm tiếp theo
        } else {
            break; // Nếu số ngày còn lại không đủ một năm, thoát vòng lặp
        }
    }

    // Bước 5: Tính Tháng
    int month = 1;
    // Mảng lưu số ngày của 12 tháng (index 0 bỏ trống để dùng index 1-12 cho trực quan)
    int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // Nếu là năm nhuận, tháng 2 có 29 ngày
    if (is_leap_year(year)) {
        days_in_month[2] = 29;
    }

    while (total_days >= days_in_month[month]) {
        total_days -= days_in_month[month]; // Trừ bớt số ngày của tháng này
        month++;                            // Tiến lên tháng tiếp theo
    }

    // Bước 6: Tính Ngày (Phải cộng thêm 1 vì total_days là số ngày "đã trôi qua" tính từ ngày mùng 1)
    int day = total_days + 1;

    SetStandbyDay((uint8_t)day);
    SetStandbyMonth((uint8_t)month);
    SetStandbyYear((uint16_t)year);
    SetStandbyHour((uint8_t)hour);
    SetStandbyMinute((uint8_t)minute);

	if (GetDisplayState() == SCREEN_STATE_STANDBY)
	{
		bIsClearStandbyLayoutByNewTS = true;
	}

    // In kết quả
    // printf("Ket qua thu cong: %02d/%02d/%d %02d:%02d:%02d\n", day, month, year, hour, minute, second);
	sprintf(acTimeStamp, "%02d/%02d/%d %02d:%02d:%02d\n", day, month, year, hour, minute, second);
	SEGGER_RTT_WriteString(0, acTimeStamp);
}
