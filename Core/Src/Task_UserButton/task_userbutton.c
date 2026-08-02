#include <stdio.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_userbutton.h"
#include "SEGGER_RTT.h"
#include "../Task_FingerPrint/task_uart_FingerPrint.h"

void Button_Task(void* param)
{
    uint32_t press_time_ms = 0;
    const uint32_t LONG_PRESS_THRESHOLD = 3000; // 3 giây
    const uint32_t POLL_RATE = 50; // Quét mỗi 50ms

    while(1)
    {
        // Nút B1 trên mạch Discovery mặc định là Active High (nhấn là lên mức 1)
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) 
        {
            press_time_ms += POLL_RATE;
            
            if (press_time_ms >= LONG_PRESS_THRESHOLD)
            {
                // Đã giữ đủ 3 giây -> Chạy process của bạn ở đây
                SEGGER_RTT_WriteString(0, "/n B1 User Button has been pressed for 3sec /n ");
                Fingerprint_StartEnrollment();
                
                // Đợi người dùng thả nút ra để tránh trigger liên tục
                while(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
                {
                    vTaskDelay(pdMS_TO_TICKS(POLL_RATE));
                }
                press_time_ms = 0; // Reset sau khi thả
            }
        }
        else
        {
            // Nút bị nhả ra trước khi đủ 3 giây
            press_time_ms = 0; 
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_RATE));
    }  
}

