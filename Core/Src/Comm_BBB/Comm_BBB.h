#include <stdint.h>
#ifndef DEF_COMM_BBB_H
#define DEF_COMM_BBB_H

#define NONE_MATCHED_FP_ID		(99)
#define NONE_MATCHED_FP_SCORE	(0)
#define UNKNOWN_FINGER_ID		(254)
#define RECONFIRM_FINGER_ID		(255)
#define MAX_LENGTH_NAME_MEMBER_BUFFER	(16)
#define BBB_RX_MAX_LEN 	(64)

void InitUARTBBB(void);
uint8_t ProcessBBB(void);
// int _write(int file, char *ptr, int len);
// void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

void CommBBB_SendStateInfo(uint8_t state, uint16_t matchedID, uint8_t confirmState);
void CommBBB_RequestEnrollID(void);
void CommBBB_SendEnrollIDError(uint8_t u8ErrorCode);

void Init_UART1_FingerPrint(void);
void BBB_UART_RxCpltCallback(uint8_t rx_data);

extern char g_acRXBufferBBB[BBB_RX_MAX_LEN];

#endif // DEF_COMM_BBB_H
