#ifndef TASK_UART_FINGERPRINT_H
#define TASK_UART_FINGERPRINT_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#define TX_PAYLOAD_MAX_SIZE 		(16)
#define RX_BUFFER_SIZE 				(256)

#define FINGERPRINT_TIMEOUT_MS      (1000)
#define ENROLL_START_TIMEOUT_MS     (5000)

#define FINGERPRINT_RX_NEW_PACKET_VALUE 	(uint32_t)(0x01)
#define FINGERPRINT_DONE_BLOCK_BY_DISPLAY		(uint32_t)(0x02)
#define FINGERPRINT_BBB_ASSIGN_ID_READY_VALUE 	(uint32_t)(0x04)
#define FINGERPRINT_END_BLOCK_INFINITY_VALUE			(uint32_t)(0x08)

// TX - RX frame
#pragma pack(push, 1)
typedef struct {
    uint8_t  header[2];      // 0xEF01
    uint8_t  address[4];     // 0xFFFFFFFF
    uint8_t  packet_ID;    // 0x01 (Command), 0x02 (Data), 0x07 (ACK)
    uint8_t  length[2];      // length of sum(Payload + Checksum) Big-Endian
    uint8_t  payload[TX_PAYLOAD_MAX_SIZE]; // Instruction/Parameters + Checksum
} Fingerprint_Packet_t;
#pragma pack(pop)

typedef struct {
    volatile uint16_t head;  // Write pointer (Write to)
    volatile uint16_t tail;  // Read pointer (Read from)
    uint8_t buffer[RX_BUFFER_SIZE];
} RingBuffer_t;

// testing
typedef enum {
	FSM_NONE,

    FSM_FINGER_SEND_GENIMG = 1, // Yêu cầu chụp ảnh mặt kính
    FSM_FINGER_WAIT_GENIMG,     // Đợi kết quả chụp ảnh

    FSM_FINGER_SEND_IMG2TZ,     // Yêu cầu chuyển ảnh thành đặc trưng (lưu Buffer 1)
    FSM_FINGER_WAIT_IMG2TZ,     // Đợi kết quả chuyển đặc trưng

    FSM_FINGER_SEND_SEARCH,     // Yêu cầu tìm kiếm 1:N trong thư viện Flash
    FSM_FINGER_WAIT_SEARCH,     // Đợi kết quả tìm kiếm

    FSM_FINGER_DELAY,            // Nghỉ 1 chút xíu nếu không thấy ngón tay rồi mới quét lại

	FSM_FINGER_BLOCK_5M,		// Bị block 5 phút khi sai vân tay 5 lần
	FSM_FINGER_BLOCK_10M,		// Bị block 10 phút khi sai vân tay 10 lần
	FSM_FINGER_BLOCK_INF,		// Bị block mãi mãi khi sai vân tay 15 lần, cho đến khi BBB unlock
	FSM_FINGER_UNBLOCK,			// Đã hết giờ Block

	FSM_SYSTEM_STM32F4_WAKEUP,

	FSM_ENROLL_REQUEST_ID,      // STM32F4 yêu cầu BBB cấp ID mới cho enrollment
	FSM_ENROLL_ID_ERROR,        // BBB không cấp được ID mới cho enrollment
	FSM_NEW_FINGERPRINT_ADDED,
	FSM_REMOVE_SPECIFIC_FINGERPRINT
} Fingerprint_State_t;

typedef enum {
    ENROLL_IDLE = 0,            // Trạng thái rảnh
    ENROLL_START,               // Bắt đầu quá trình thêm vân tay (chờ ID cần lưu từ phía BBB)
    ENROLL_GET_IMG_1,           // Chờ ngón tay chạm lần 1
    ENROLL_IMG2TZ_1,            // Đang xử lý ảnh lần 1
    ENROLL_WAIT_REMOVE,         // Yêu cầu nhấc ngón tay ra
    ENROLL_GET_IMG_2,           // Chờ ngón tay chạm lần 2
    ENROLL_IMG2TZ_2,            // Đang xử lý ảnh lần 2
    ENROLL_REG_MODEL,           // Đang tổng hợp Model
    ENROLL_STORE_MODEL,         // Đang lưu vào Flash
    ENROLL_SUCCESS,             // Hoàn thành
    ENROLL_ERROR                // Báo lỗi (Time out hoặc ngón tay không khớp)
} EnrollState_t;

void Init_UART2_FingerPrint(void);

void Fingerprint_SendCommand(uint8_t instructionCode, uint8_t *params, uint8_t param_len);

void ProcessFingerPrintApplication(void);
void ProcessFingerPrintEnrollmentApplication(void);
void Fingerprint_StartEnrollment(void);
void Fingerprint_SetEnrollID(uint16_t enrollID);
void FingerPrint_UART_RxCallback(uint8_t rx_byte);

bool Fingerprint_GetConfirmFPOKEnableFlag(void);
void Fingerprint_SetConfirmFPOKEnableFlag(bool enable);
bool Fingerprint_GetConfirmFPBADEnableFlag(void);
void Fingerprint_SetConfirmFPBADEnableFlag(bool enable);

void Fingerprint_StateMachine_Task(void* param);

#endif // TASK_UART_FINGERPRINT_H
