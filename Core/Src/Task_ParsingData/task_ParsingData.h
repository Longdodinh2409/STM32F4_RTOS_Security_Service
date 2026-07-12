#ifndef TASK_PARSINGDATA_H
#define TASK_PARSINGDATA_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "../Task_FingerPrint/task_uart_FingerPrint.h"
#include "task.h"

#define PARSING_DATA_SRC_FINGERPRINT_BIT	(0x01)
#define PARSING_DATA_SRC_BBB_BIT			(0x02)

#define GMT_7								(7)

void ParsingRXData_Task(void* param);

// FingerPrint Parsing RX data
bool ParsingData_FP_ExtractPacketFromRingBuffer(const volatile uint8_t *buffer_ptr, uint16_t buffer_len, Fingerprint_Packet_t *packet);
bool ParsingData_FP_VerifyChecksum(const Fingerprint_Packet_t *packet);

// BBB Parsing RX data
int is_leap_year(int year);
void ProcessParsingTimeStamp(uint32_t timestamp, int timezone_offset_hours);

#endif // TASK_PARSINGDATA_H
