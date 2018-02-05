#include "stm32f4xx.h"
#include "proj_common.h"
#include "Uart.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "Cmds.h"

#define  SAFE_MEM_ADDR           0x20006400


#define  CMDSM_WAITFORLINE       0
#define  CMDSM_MEMDUMP           1
#define  CMDSM_RTC_RDONE         2
#define  CMDSM_RTC_WDONE         3

#define  DO_INIT                 0
#define  DO_PROCESS              1


static bool   cmds_input_ready;
static char  *cmds_InpPtr;
static u32    cmds_state_machine;
static u32    cmds_completion;
static u32    cmds_word1;
static u32    cmds_count1;

//static u8     cmds_TA[6];

static volatile u32     cmds_xtest;


static bool cmds_R ( void );
static bool cmds_T ( void );
static bool cmds_MD( u32 state );
static bool cmds_SC( void );

//static bool cmds_A ( u32 state );
//static bool cmds_Z ( u32 state );
//static bool cmds_ST( void );



void CMDS_Init(void)
{
    cmds_input_ready   = FALSE;
    cmds_state_machine = CMDSM_WAITFORLINE;
    cmds_xtest         = 0x11223398;

    *((uint32_t *)SAFE_MEM_ADDR) = 0xBC00BC99;
}


void CMDS_Process(void)
{
    bool  signal_done = TRUE;
    
    switch( cmds_state_machine )
    {
    case CMDSM_WAITFORLINE:
        
        if( cmds_input_ready == FALSE ) { return; }
        cmds_input_ready = FALSE;
        
        if( cmds_InpPtr[0] == 'a' )
        {
            ;//signal_done = cmds_A( DO_INIT );
        }
        else if( cmds_InpPtr[0] == 'd' )
        {
            ;
        }
        else if( cmds_InpPtr[0] == 'm' && cmds_InpPtr[1] == 'd')
        {
            signal_done = cmds_MD( DO_INIT );
        }
        else if( cmds_InpPtr[0] == 'r' )
        {
            signal_done = cmds_R();
        }
        else if( cmds_InpPtr[0] == 's' && cmds_InpPtr[1] == 'c')
        {
            cmds_SC();
        }
        else if( cmds_InpPtr[0] == 't' )
        {
            signal_done = cmds_T();
        }
        else if( cmds_InpPtr[0] == 'v' )
        {
            signal_done = CMDS_DisplayVersion();
        }
        else if( cmds_InpPtr[0] == 'z' )
        {
            ;//signal_done = cmds_Z( DO_INIT );
        }
        break;
        
    case CMDSM_MEMDUMP:      signal_done = cmds_MD( DO_PROCESS );    break;
    //case CMDSM_RTC_RDONE:    signal_done = cmds_A ( DO_PROCESS );    break;
    //case CMDSM_RTC_WDONE:    signal_done = cmds_Z ( DO_PROCESS );    break;
        
    }

    if( signal_done == TRUE ) { U2Inp_SignalCmdDone(); }
}








bool CMDS_DisplayVersion(void)
{
    U2_PrintSTR("\r\n");
    U2_Print8N (VERSION_STR, VERSION_MINOR);
    U2_PrintSTR(", ");
    U2_PrintSTR(VERSION_DATE);
    
    return TRUE;
}


void CMDS_SetInputStr(char *StrInp)
{
    cmds_InpPtr      = StrInp;
    cmds_input_ready = TRUE;
}




static bool cmds_R( void )
{
    u32  tmpw;
    
    if( cmds_InpPtr[1] == 'p' )
    {
        U2_Print32( "PWR->CR  ", (u32)PWR->CR );
        U2_Print32( "PWR->CSR ", (u32)PWR->CSR );
    }
    else if( cmds_InpPtr[1] == 'm' )
    {
        tmpw = HtoI(&cmds_InpPtr[3]) & 0xFFFFFFFC;                              // bits 0 and 1 forced to 0
        U2_Print32N( "0x", tmpw );
        U2_Print32( ": ", (u32)*((u32 *)tmpw) );
    }
    
    return TRUE;
}


static bool cmds_T( void )
{
    int  tmpI;
    u32  tmp32;
    
    if( cmds_InpPtr[1] == '1' )
    {
        tmpI = AtoI("452");
        U2_Print32( "452: 0x", (u32)tmpI );
        
        tmpI = AtoI("-4392");
        if( tmpI == -4392 )
            U2_PrintSTR( "-4392 Good\r\n" );
        else
            U2_PrintSTR( "Conversion did not yield -4392\r\n" );
    }
    else if( cmds_InpPtr[1] == '2' )
    {
        tmp32 = (u32)*((u32 *)SAFE_MEM_ADDR);
        U2_Print32( "*0x20006400 = ", tmp32 );
    }
    else if( cmds_InpPtr[1] == '3' )
    {
        tmp32 = 1234567;
        printf("testing, tmp32 = %d\n\r",(int)tmp32);
        printf("This is Only Just a Test %s   %d    0x%x\n\r","hello world",(int)tmp32,0x1234567);
    }
    
    return TRUE;
}



static bool cmds_MD( u32 state )
{
    u32  i;
    bool retv = FALSE;
    
    switch( state )
    {
    case DO_INIT:

        if( strlen(cmds_InpPtr) > 2 ) { cmds_word1 = HtoI(&cmds_InpPtr[3]) & 0xFFFFFFFC; }
        cmds_count1        = 0;
        cmds_state_machine = CMDSM_MEMDUMP;
        cmds_completion    = 1;
        FALL_THRU;
        
    case DO_PROCESS:
    
        if( cmds_completion == 1 )
        {
            U2_Print32N( "0x", cmds_word1 );
            U2_Print8N( ": ", (u8)*((u8 *)cmds_word1++) );

            for( i=0; i < 15; i++ )
            {
                U2_Print8N( " ", (u8)*((u8 *)cmds_word1++) );
            }

            U2_Send( SERO_TYPE_STR, (char *)"\r\n", &cmds_completion, 0 );

            if( ++cmds_count1 == 4 )
            {
                cmds_state_machine = CMDSM_WAITFORLINE;
                ItoH( cmds_word1, &cmds_InpPtr[2] );
                retv = TRUE;
            }
        }
        break;
    }
    
    return retv;
}

/*

static bool cmds_A( u8 state )
{
    bool retv = FALSE;
    
    switch( state )
    {
    case DO_INIT:
        
        RTC_GetTime();
        cmds_state_machine = CMDSM_RTC_RDONE;
        break;
        
    case DO_PROCESS:

        if( RTC_ShowTime() != RTC_COMPLETION_BUSY )
        {
            cmds_state_machine = CMDSM_WAITFORLINE;
            retv = TRUE;
        }
        break;
    }
    
    return retv;
}


static bool cmds_Z( u8 state )
{
    bool retv = FALSE;
    
    switch( state )
    {
    case DO_INIT:
        
        RTC_SetTime_Canned();
        cmds_state_machine = CMDSM_RTC_WDONE;
        break;
        
    case DO_PROCESS:

        if( RTC_SetComplete() != RTC_COMPLETION_BUSY )
        {
            cmds_state_machine = CMDSM_WAITFORLINE;
            retv = TRUE;
        }
        break;
    }
    
    return retv;
}



//
//  012345678901234567890
//  st 38 12 03 31 01 17
//
static bool cmds_ST( void )
{
    u8  i,k;
    
    if( strlen(cmds_InpPtr) == 2 )
    {
        SER_PrintSTR( "st mins hrs wkday day mon yr\r\n" );
    }
    else
    {
        for( i=3,k=0; i < 20; i += 3 )
        {
            cmds_InpPtr[i+2] = 0;
            cmds_TA[k++]     = (u8)HtoU16( &cmds_InpPtr[i] );
        }

        RTC_SetTime( cmds_TA );
    }
    
    return TRUE;
}
*/




static bool cmds_SC( void )
{
    uint32_t tmp;
    RCC_ClocksTypeDef  rclocks;

    tmp = RCC->CFGR & RCC_CFGR_SWS;
    if( tmp == 0 )
        U2_PrintSTR("HSI\n\r");
    else if( tmp == 4 )
        U2_PrintSTR("HSE\n\r");
    else
        U2_PrintSTR("PLL\n\r");

    SystemCoreClockUpdate();
    RCC_GetClocksFreq(&rclocks);

    U2_Print32( "SYSCLK: ", rclocks.SYSCLK_Frequency );
    U2_Print32( "HCLK:   ", rclocks.HCLK_Frequency   );
    U2_Print32( "PCLK1:  ", rclocks.PCLK1_Frequency  );
    U2_Print32( "PCLK2:  ", rclocks.PCLK2_Frequency  );

    U2_Print32( "RCC->APB1ENR:  ", RCC->APB1ENR  );

    return TRUE;
}

































