#include <main.h>
#include <stdint.h>
#include <stdlib.h> // atoi()
#include <stdio.h>
#include <string.h>
#include "Comm_BBB.h"
#include "stm32f4xx_hal_def.h"
#include "SEGGER_RTT.h"
#include "../Task_FingerPrint/task_uart_FingerPrint.h"
#include "../Task_ParsingData/task_ParsingData.h"

#define DEFAULT_CMD_VALUE 	(99)

extern UART_HandleTypeDef huart1;
extern uint8_t UART1_rx_data;
extern TaskHandle_t task_PD_handler;
extern char g_acRXBufferBBB[BBB_RX_MAX_LEN];

uint8_t rx_data;
char rx_buffer[BBB_RX_MAX_LEN];
char g_acRXBufferBBB[BBB_RX_MAX_LEN];
uint8_t rx_index = 0;
volatile uint8_t data_ready = 0;
volatile uint8_t processing = 0;
uint8_t cmd;

char acTxBBBBuffer[64];

static uint16_t CommBBB_GetFrameLength(const char *pFrame)
{
	return (uint16_t)strlen(pFrame);
}

void InitUARTBBB(void)
{
	HAL_UART_Receive_IT(&huart1, &rx_data, 1);

	printf("STM32F411VE is ready! Enter your command: \r\n");
	fflush(stdout);
	memset(rx_buffer, 0, sizeof(rx_buffer));
	rx_index = 0;
	cmd = DEFAULT_CMD_VALUE;
}

uint8_t ProcessBBB(void)
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

void CommBBB_SendStateInfo(uint8_t u8State, uint16_t u16MatchedID, uint8_t u8ConfirmState)
{
	char acTxSEGGER[64];
	uint16_t u16ReportID = NONE_MATCHED_FP_ID;

	memset(acTxSEGGER, 0, 64);

	if ((u8State == (uint8_t)FSM_FINGER_WAIT_SEARCH) || (u8State == (uint8_t)FSM_NEW_FINGERPRINT_ADDED))
	{
		if (u8ConfirmState == 0x00)		// Found finger!!!
		{
			u16ReportID = u16MatchedID;
		}
		else if (u8ConfirmState == 0x17)	// same finger (after get valid/invalid finger)
		{
			u16ReportID = RECONFIRM_FINGER_ID;
		}
		else // if (u8ConfirmState == 0x09)		// Unknown finger
		{
			u16ReportID = UNKNOWN_FINGER_ID;
		} 

		sprintf(acTxBBBBuffer, "#State=%d,#ID=%d\n", u8State, u16ReportID);
	} 
	else 
	{
		sprintf(acTxBBBBuffer, "#State=%d\n", u8State);
	}

	uint16_t u16FrameLen = CommBBB_GetFrameLength(acTxBBBBuffer);
	// HAL_StatusTypeDef status = HAL_UART_Transmit_IT(&huart1, (uint8_t *)acTxBBBBuffer, u16FrameLen);
	
	// sprintf(acTxSEGGER, "TX status = %d\n", (uint8_t)status);
	// SEGGER_RTT_WriteString(0, acTxSEGGER);

	if (huart1.gState == HAL_UART_STATE_READY) 
	{
		HAL_StatusTypeDef status = HAL_UART_Transmit_IT(&huart1, (uint8_t *)acTxBBBBuffer, u16FrameLen);
		sprintf(acTxSEGGER, "TX BBB status = %d\n", (uint8_t)status);
		SEGGER_RTT_WriteString(0, acTxSEGGER);
	} 
	else 
	{
		sprintf(acTxSEGGER, "TX BBB is BUSY. Packet dropped!\n");
		SEGGER_RTT_WriteString(0, acTxSEGGER);
	}

	// Debug on RTT Viewer (Terminal 0)
	SEGGER_RTT_WriteString(0, acTxBBBBuffer);
}

void CommBBB_RequestEnrollID(void)
{
	char acTxSEGGER[64];

	CommBBB_SendStateInfo((uint8_t)FSM_ENROLL_REQUEST_ID, 0, 0);

	if (huart1.gState == HAL_UART_STATE_READY)
	{
		SEGGER_RTT_WriteString(0, "Request ID sent via FSM state to BBB.\n");
	} 
	else 
	{
		SEGGER_RTT_WriteString(0, "Request ID TX BBB is BUSY. Packet dropped!\n");
	}

	sprintf(acTxSEGGER, "Request enroll ID state = %d\n", (uint8_t)FSM_ENROLL_REQUEST_ID);
	SEGGER_RTT_WriteString(0, acTxSEGGER);
}

void CommBBB_SendEnrollIDError(uint8_t u8ErrorCode)
{
	char acTxSEGGER[64];

	(void)u8ErrorCode;
	CommBBB_SendStateInfo((uint8_t)FSM_ENROLL_ID_ERROR, 0, 0);

	if (huart1.gState == HAL_UART_STATE_READY)
	{
		SEGGER_RTT_WriteString(0, "Enroll ID error sent via FSM state to BBB.\n");
	}
	else
	{
		SEGGER_RTT_WriteString(0, "Enroll ID error TX BBB is BUSY. Packet dropped!\n");
	}

	sprintf(acTxSEGGER, "Enroll ID error state = %d\n", (uint8_t)FSM_ENROLL_ID_ERROR);
	SEGGER_RTT_WriteString(0, acTxSEGGER);
}

void Init_UART1_FingerPrint(void)
{
	HAL_UART_Receive_IT(&huart1, &UART1_rx_data, 1);
}

void BBB_UART_RxCpltCallback(uint8_t rx_data)
{
	/* Append printable digits to buffer, ignore CR, on LF parse value */
	if (rx_data == ';')
	{
		/* End of line received - parse and store */
		if (rx_index > 0)
		{
			// memset(g_acRXBufferBBB, 0, BBB_RX_MAX_LEN);
			// strncpy(g_acRXBufferBBB, rx_buffer, BBB_RX_MAX_LEN);
			strcpy(g_acRXBufferBBB, rx_buffer);
			
			if (!strncmp(g_acRXBufferBBB, "#TS", 3))
			{
				BaseType_t xHigherPriorityTaskWoken = pdFALSE;
				xTaskNotifyFromISR(task_PD_handler, PARSING_TIMESTAMP_SRC_BBB_BIT, eSetBits, &xHigherPriorityTaskWoken);
				portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
			}
			else if (!strncmp(g_acRXBufferBBB, "#State", 6))
			{
				uint8_t u8BBBState = 0;
				int parsed_state_count = sscanf(g_acRXBufferBBB, "#State=%hhu", &u8BBBState);

				if (parsed_state_count == 1)
				{
					if (u8BBBState == (uint8_t)FSM_FINGER_WAIT_SEARCH)
					{
						BaseType_t xHigherPriorityTaskWoken = pdFALSE;
						xTaskNotifyFromISR(task_PD_handler, PARSING_MEMBER_NAME_SRC_BBB_BIT, eSetBits, &xHigherPriorityTaskWoken);
						portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
					}
					else if (u8BBBState == (uint8_t)FSM_ENROLL_REQUEST_ID)
					{
						BaseType_t xHigherPriorityTaskWokenID = pdFALSE;
						xTaskNotifyFromISR(task_PD_handler, PARSING_MEMBER_ID_AVAILABLE_TO_ADD_SRC_BBB_BIT, eSetBits, &xHigherPriorityTaskWokenID);
						portYIELD_FROM_ISR(xHigherPriorityTaskWokenID);
					}
				}
			}

		}
		/* reset buffer for next frame */
		memset(rx_buffer, 0, sizeof(rx_buffer));
		rx_index = 0;
	}
	else
	{
		if (rx_index < (sizeof(rx_buffer) - 1))
		{
			rx_buffer[rx_index++] = (char)rx_data;
			rx_buffer[rx_index] = '\0';
		}
	}
	/* ignore carriage return and other bytes */
}
