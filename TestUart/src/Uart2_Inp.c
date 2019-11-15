#include "stm32f4xx.h"
#include "proj_common.h"
#include "Uart.h"
#include "Cmds.h"





#define SERI_STATE_GETCHARS    0
#define SERI_STATE_WAITDONE    1

#define SERI_MAX_CHARS         26



                                               // INPUT related global variables:
static u32    seri_state_machine;              //   state of Serial machine, Input
static bool   seri_CmdDone;                    //   signal for CMD completion
static u32    seri_cnt;                        //   input: count of chars in buffer
static u8     seri_ch;                         //   input: character just received
static char   seri_dat[SERI_MAX_CHARS];        //   input: data buffer


static const char   *seri_crlf   = "\n\r";     // carriage-return, line-feed
static const char   *seri_prompt = ">> ";      // prompt string
static const char   *seri_bksp   = "\b \b";    // backspace string



void U2Inp_Init( void )
{
    seri_cnt           = 0;
    seri_state_machine = SERI_STATE_GETCHARS;
}


void U2Inp_Process( void )
{
    switch( seri_state_machine )
    {
    case SERI_STATE_GETCHARS:

#ifdef USE_FTDI_PORT
        if( !(USART2->SR & USART_FLAG_RXNE) ) { return; }                             // RXNE=1 when a byte is available
        seri_ch = (USART2->DR & 0xff);
#else
        if( !(USART1->SR & USART_FLAG_RXNE) ) { return; }                             // RXNE=1 when a byte is available
        seri_ch = (USART1->DR & 0xff);
#endif

        if( seri_ch == ASCII_CARRIAGERETURN || seri_ch == ASCII_LINEFEED )
        {
            U2_PrintSTR(seri_crlf);
            seri_dat[seri_cnt] = 0;

            if( seri_cnt > 0 )
            {
                seri_cnt           = 0;
                seri_CmdDone       = FALSE;
                seri_state_machine = SERI_STATE_WAITDONE;
                CMDS_SetInputStr(seri_dat);
            }
            else
            {
                U2_PrintSTR(seri_prompt);
            }
        }
        else if( seri_ch == ASCII_BACKSPACE || seri_ch == ASCII_DELETE )
        {
            if( seri_cnt > 0 )
            {
                U2_PrintSTR(seri_bksp);
                --seri_cnt;
            }
        }
        else if(( seri_ch >= ASCII_SPACE ) && ( seri_ch <= ASCII_TILDE ))
        {
            U2_PrintCH(seri_ch);
            seri_dat[seri_cnt] = seri_ch;
            if( ++seri_cnt >= SERI_MAX_CHARS ) { --seri_cnt; }
        }

        break;

    case SERI_STATE_WAITDONE:

        if( seri_CmdDone == TRUE )
        {
            U2_PrintSTR(seri_crlf);
            U2_PrintSTR(seri_prompt);
            seri_state_machine = SERI_STATE_GETCHARS;
        }
        break;
    }
}


void U2Inp_SignalCmdDone( void )
{
    seri_CmdDone = TRUE;
}












