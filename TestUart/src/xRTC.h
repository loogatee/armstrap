#ifndef RTC_H
#define RTC_H

//#include "i2c.h"

#define RTC_COMPLETION_BUSY       I2C_COMPLETION_BUSY
#define RTC_COMPLETION_OK         I2C_COMPLETION_OK
#define RTC_COMPLETION_TIMEOUT    I2C_COMPLETION_TIMEOUT


void  xRTC_Init(void);
void  xRTC_GetTime(void);
void  xRTC_SetTime(u8 *TA);
void  xRTC_SetTime_Canned(void);
u8    xRTC_ShowTime(void);
u8    xRTC_SetComplete(void);
void  xRTC_ShowTime_Loop(void);



void  xRTC_GetYearOnly(void);
u8    xRTC_ShowYearOnly(void);
void  xRTC_GetTemperatureOnly(void);
u8    xRTC_ShowTemperatureOnly( void );

#endif
