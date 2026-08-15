#ifndef DEF_GUI_H
#define DEF_GUI_H

#include <stdint.h>
#include "fonts.h"
#include "ssd1306.h"

typedef enum {
	SCREEN_STATE_INIT,
	SCREEN_STATE_STANDBY,
	SCREEN_STATE_PROCESSING,
	SCREEN_STATE_ENROLL,
	SCREEN_STATE_PASS,
	SCREEN_STATE_FAIL,
	SCREEN_STATE_TEMP_LOCK,
	SCREEN_STATE_INFINITY_LOCK,
	SCREEN_STATE_MAX_CNT
} E_SCREEN_STATE;

#define INVALID_CMD				(99)

#define MAX_LENGTH_NAME_MEMBER_DISPLAY	(16)

#define MAX_CONFIRMATION_CNT_FINGER_PRINT	(3)
#define MAX_LIMIT_NO_FINGER_CNT					(2)

#define NOTI_ENTER_STANDBY_DISPLAY_STATE		(uint32_t)(0x01)

#define DEFAULT_DAY				(1)
#define DEFAULT_MONTH			(1)
#define DEFAULT_YEAR			(2026)
#define DEFAULT_HOUR			(0)
#define DEFAULT_MINUTE			(0)

// Scanning animation
#define FIXED_X_SCANNING_BAR	(2)
#define START_Y_SCANNING_BAR	(2)
#define END_Y_SCANNING_BAR		(62)
#define SCANNING_BAR_WIDTH		(124)
#define SCANNING_BAR_HEIGHT		(2)

#define TOTAL_STEP_OF_SCANNING_ANIMATION	(11)

#define WIDTH_EACH_STEP_SCANNING_ANIMATION ((END_Y_SCANNING_BAR - START_Y_SCANNING_BAR) / (TOTAL_STEP_OF_SCANNING_ANIMATION - 1))

void SetDisplayState(E_SCREEN_STATE eState);
E_SCREEN_STATE GetDisplayState(void);
void ClearAllDisplayState(void);

void SetStandbyDay(uint8_t day);
uint8_t GetStandbyDay(void);
void SetStandbyMonth(uint8_t month);
uint8_t GetStandbyMonth(void);
void SetStandbyYear(uint16_t year);
uint16_t GetStandbyYear(void);
void SetStandbyHour(uint8_t hour);
uint8_t GetStandbyHour(void);
void SetStandbyMinute(uint8_t minute);
uint8_t GetStandbyMinute(void);

void InitOLEDScreen(void);
void ProcessDisplay(void);
void ProcessDisplayInit(void);
void ProcessDisplayStandby(void);
void ProcessDisplayScanning(void);
void ProcessDisplayPass(const char *pcMemberNameBuffer);
void ProcessDisplayFail(void);
void ProcessDisplayTempLock(uint8_t u8BlockMin, uint8_t u8BlockSec, uint32_t u32RemainingTimeSec, uint32_t u32TotalDurationSec);
void ProcessDisplayInfLock(void);

void Display_Task(void* param);

#endif // DEF_GUI_H
