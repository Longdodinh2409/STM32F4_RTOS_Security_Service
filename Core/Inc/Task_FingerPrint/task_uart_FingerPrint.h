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

    FSM_FINGER_DELAY            // Nghỉ 1 chút xíu nếu không thấy ngón tay rồi mới quét lại
} Fingerprint_State_t;

void Init_UART2_FingerPrint(void);

void Fingerprint_SendCommand(uint8_t instructionCode, uint8_t *params, uint8_t param_len);
void Fingerprint_Test_TX(void);  // Test function for debugging TX

void ProcessFingerPrintRXData(void* param);
void ProcessFingerPrintApplication(void);
bool Fingerprint_VerifyChecksum(const Fingerprint_Packet_t *packet);
void FingerPrint_UART_RxCallback(uint8_t rx_byte);
bool Fingerprint_ExtractPacketFromRingBuffer(uint8_t *buffer_ptr, uint16_t buffer_len, Fingerprint_Packet_t *packet);

void Fingerprint_StateMachine_Task(void* param);
