#ifndef DEF_COMM_BBB_H
#define DEF_COMM_BBB_H

#define NONE_MATCHED_FP_ID		(99)
#define NONE_MATCHED_FP_SCORE	(0)

void InitUARTBBB(void);
uint8_t ProcessBBB(void);
// int _write(int file, char *ptr, int len);
// void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

void CommBBB_SendStateInfo(uint8_t state, uint16_t matchedID, uint8_t confirmState);

#endif // DEF_COMM_BBB_H
