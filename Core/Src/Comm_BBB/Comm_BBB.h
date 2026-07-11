#ifndef DEF_COMM_BBB_H
#define DEF_COMM_BBB_H

void InitUARTBBB();
uint8_t ProcessBBB();
int _write(int file, char *ptr, int len);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif // DEF_COMM_BBB_H
