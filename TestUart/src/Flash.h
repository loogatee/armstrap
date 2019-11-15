#ifndef FLASH_H
#define FLASH_H

#include "i2c.h"

#define FLASH_COMPLETION_BUSY       I2C_COMPLETION_BUSY
#define FLASH_COMPLETION_OK         I2C_COMPLETION_OK
#define FLASH_COMPLETION_TIMEOUT    I2C_COMPLETION_TIMEOUT


void  xFlash_Init(void);
void  xFlash_GetMem16(UW2B addr);
u8    xFlash_ShowMem16(void);
void  xFlash_Write8( UW2B addr, u8 *TA );
u8    xFlash_WriteComplete( void );
void  xFlash_Erase8( UW2B addr );
u8    xFlash_EraseComplete( void );


#endif
