#include "stm32f4xx.h"
#include "proj_common.h"
#include "Uart.h"
#include "Cmds.h"
#include "string.h"
//#include "gpio_CDC3.h"



typedef struct
{
    uint32_t      SysTicks;
} GLOBALS_t;



static GLOBALS_t   TimrGlobals;


void init_timer( void )
{
	TimrGlobals.SysTicks = 0;
}


uint32_t GetSysTick( void )
{
    return TimrGlobals.SysTicks;
}


uint32_t GetSysDelta( uint32_t OriginalTime )
{
    uint32_t v = TimrGlobals.SysTicks;

    if( v >= OriginalTime )
        return( v - OriginalTime );
    else
        return( ~OriginalTime + 1 + v );
}


void Increment_SysTicks( void )
{
	++TimrGlobals.SysTicks;
}



