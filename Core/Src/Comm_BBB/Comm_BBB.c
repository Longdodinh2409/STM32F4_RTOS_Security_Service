#include <main.h>
#include <stdint.h>
#include <stdlib.h> // atoi()
#include <stdio.h>
#include <string.h>
#include "Comm_BBB.h"
#include "stm32f4xx_hal_def.h"
#include "SEGGER_RTT.h"

#define DEFAULT_CMD_VALUE 	(99)

extern UART_HandleTypeDef huart1;;
uint8_t rx_data;
char rx_buffer[50];
uint8_t rx_index = 0;
volatile uint8_t data_ready = 0;
volatile uint8_t processing = 0;
uint8_t cmd;

void InitUARTBBB()
{
	HAL_UART_Receive_IT(&huart1, &rx_data, 1);

	printf("STM32F411VE is ready! Enter your command: \r\n");
	fflush(stdout);
	memset(rx_buffer, 0, sizeof(rx_buffer));
	rx_index = 0;
	cmd = DEFAULT_CMD_VALUE;
}

uint8_t ProcessBBB()
{
	if (data_ready == 1)
	{
		processing = 1;  // set flag to avoid callback set data_ready again
		data_ready = 0;

	  // Processing
//	  cmd = DEFAULT_CMD_VALUE;
	  cmd = atoi(rx_buffer);
	  memset(rx_buffer, 0, sizeof(rx_buffer));
	  rx_index = 0;

	  if (cmd != DEFAULT_CMD_VALUE)
	  {
		  if (cmd > 0)
		  {
			  printf("\nReceived '%d' \n", cmd);
		  }
		  else
		  {
			  printf("\n[Err] U just sent rubbish: '%s'. Try again!\n", rx_buffer);
		  }

		  printf("Enter your command: \r\n");
		  //	fflush(stdout);
		  processing = 0;

		  return cmd;
	  }


	}
	processing = 0;  // set flag off, callback can set data_ready again
	return DEFAULT_CMD_VALUE;
}

int _write(int file, char *ptr, int len) {
    // HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
	// Send to BBB
	HAL_UART_Transmit_IT(&huart1, (uint8_t *)ptr, len);
	// Debug on RTT Viewwe (Terminal 0)
	SEGGER_RTT_WriteString(0, ptr);
    return len;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) 
	{
        // Check if this byte is end of data
        if (rx_data == '\n' || rx_data == '\r') {
            // Only set data ready when there's no data or in processing
            if (rx_index > 0 && !processing) {
                rx_buffer[rx_index] = '\0'; // confirm it's end
                data_ready = 1;             // handle in main() loop
            }
            // reset index to handle next data string
            rx_index = 0;
        } else if ((rx_data >= '0' && rx_data <= '9') || rx_data == '-' || rx_data == '+') {
                rx_buffer[rx_index++] = rx_data;
        }

        // enable interrupt for the next time
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}
