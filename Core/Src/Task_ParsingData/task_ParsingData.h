#ifndef TASK_PARSINGDATA_H
#define TASK_PARSINGDATA_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "../Task_FingerPrint/task_uart_FingerPrint.h"
#include "task.h"

void ParsingRXData_Task(void* param);

// FingerPrint Parsing RX data
bool ParsingData_FP_ExtractPacketFromRingBuffer(const volatile uint8_t *buffer_ptr, uint16_t buffer_len, Fingerprint_Packet_t *packet);
bool ParsingData_FP_VerifyChecksum(const Fingerprint_Packet_t *packet);

#endif // TASK_PARSINGDATA_H
