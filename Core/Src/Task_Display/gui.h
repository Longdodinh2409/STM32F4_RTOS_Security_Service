#ifndef DEF_GUI_H
#define DEF_GUI_H

#include "fonts.h"
#include "ssd1306.h"

typedef enum {
	SCREEN_STATE_INIT,
	SCREEN_STATE_STANDBY,
	SCREEN_STATE_PROCESSING,
	SCREEN_STATE_PASS,
	SCREEN_STATE_FAIL,
	SCREEN_STATE_TEMP_LOCK,
	SCREEN_STATE_INFINITY_LOCK,
	SCREEN_STATE_MAX_CNT
} E_SCREEN_STATE;

// extern unsigned char garfield_128x64[];
// extern E_SCREEN_STATE g_u8ScreenState;

#define INVALID_CMD				(99)

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

void InitOLEDScreen(void);
void ProcessDisplay(void);
void ProcessDisplayInit(void);
void ProcessDisplayStandby(void);
void ProcessDisplayScanning(void);
void ProcessDisplayPass(void);
void ProcessDisplayFail(void);
void ProcessDisplayTempLock(void);
void ProcessDisplayInfLock(void);

void Display_Task(void* param);

#endif // DEF_GUI_H
