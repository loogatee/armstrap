
#include "stm32f4xx.h"
#include "proj_common.h"
#include "Uart.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>






#define SERO_STATE_GETJOB                  0
#define SERO_STATE_WRITE_TO_LOCALBUF       1
#define SERO_STATE_PRINTSTRING             2
#define SERO_SQENTRYS                      100

#define LEN_PRINTF_BUF         1024

#define SIZE_LBUF  63

typedef struct
{                                  // Items on the Serial Job Queue
    u32    sr_otype;               // Output Type.  See SERD_OTYPE_XX
    //u16    sr_SendNow;             // Flag to override string accumulation
    char  *sr_sptr;                // pointer to string to be printed
    u32   *sr_compPtr;             // if != 0, sets to 1 on completion
    u32    sr_dval;                // data value as 8-bit or 32-bit
} SERI;




                                               // OUTPUT related global variables:
static u32    serd_num_Qitems;                 //   Number of Items on the Queue
static u32    serd_inn_Qindex;                 //   Item will be deposited here
static u32    serd_out_Qindex;                 //   Item will be removed from here
static SERI  *serd_active_Qitem;               //   pointer to currently active item
static SERI   serd_Q_items[SERO_SQENTRYS];     //   Serial Job Data Items
static u32    serd_ostate_machine;             //   holds state of Serial Output machine
static char   serd_databuf[16];                //   data buffer for value conversion
static u8     serd_pfbuf[LEN_PRINTF_BUF];
static u32    serd_pfindex;
static u32    *serd_SavedcompPtr;

static u8   serd_lbuf_0[SIZE_LBUF];
static u8   serd_lbuf_1[SIZE_LBUF]
static bool serd_buf1_active;
static u8   serd_printchar;
static u8   serd_Iprint;






void U2_Init( void )
{
    serd_num_Qitems     = 0;
    serd_inn_Qindex     = 0;
    serd_out_Qindex     = 0;
    serd_ostate_machine = SERO_STATE_GETJOB;
    serd_pfindex        = 0;
    serd_Ilbuf          = 0;
    serd_SavedcompPtr   = 0;   
}


//---------------------------------------------------------------------------------------
//  Queues up a Job on the 'Serial Jobs Output List' (Implemented as a Circular Q)
//  This is the 'Front End' or Producer of Serial Data.   The idea is to stash the
//  parameters quickly in a Queue, then let the 'Back End' (the Consumer) process the
//  data off the Queue, and perform the printing.
//  The data is destined for USART2.
//     **Note the use of void for return. Alternative is return -1 when Q is full.
//       Consider using completionptr to let the Q drain, then keep printing.
//
//
//        otype - The type of output requested.  see SERO_TYPE_ defines
//
//        sptr  - Pointer to string to be printed.
//
//        completionptr - IF 0, then there is no completion signal.   IF
//                        non-zero, this address will be signaled with a 1 
//                        to indicate request has been completed.
//
//        aval - Value that can optionally be printed along with sptr.
//               Value can be 8-bit, or 32-bit.   otype will control this.
//
void U2_Send( u32 otype, char *sptr, u32 *completionptr, u32 aval)
{
    SERI  *lqitem;                                                              // Pointer to Array Element where the data will go

    if( serd_num_Qitems != SERO_SQENTRYS )                                      // Proceed if the Queue is not full
    {
        lqitem = &serd_Q_items[serd_inn_Qindex];                                // item will go in at index serd_inn_qindex
        ++serd_num_Qitems;                                                      // Queue size is increased by 1
        if( ++serd_inn_Qindex == SERO_SQENTRYS ) { serd_inn_Qindex = 0; }       // where the next item will go:  wrap if necessary

        lqitem->sr_dval    = aval;                                              // aval goes into element on the Q
        lqitem->sr_otype   = otype;                                             // otype goes into element on the Q
        lqitem->sr_sptr    = sptr;                                              // sptr goes into element on the Q
        lqitem->sr_compPtr = completionptr;                                     // completionptr goes into element on the Q
        
        //if (otype == SERO_TYPE_STRNOW)
        //	lqitem->sr_SendNow = TRUE;
        //else
        //	lqitem->sr_SendNow = FALSE;

        if( completionptr )
        {
            *completionptr = 0;                                                 // if pointer is valid, store 0, indicating Not Done
        }
    }
                                                                                // Else the Q is full.  Effectively tosses the data
}                                                                               //   note intention not to return -1 if Q is full






void U2_Process( void )
{
    u8    dlen;
    bool  OverflowFlag;

    switch( serd_ostate_machine )
    {
    case SERO_STATE_GETJOB:                                                              // Looking for items on the Queue

        if( serd_num_Qitems == 0 ) { return; }                                           // return IMMEDIATELY if Queue is empty

        serd_active_Qitem   = &serd_Q_items[serd_out_Qindex];                            // Item to operate on is at 'serd_out_qindex'
        serd_ostate_machine = SERO_STATE_WRITE_TO_LOCALBUF;                              // switch state to 'Processing Characters'

        switch( serd_active_Qitem->sr_otype )                                            // sr_otype dictates the action
        {
          case SERO_TYPE_ONECHAR: serd_databuf[0] = (u8)serd_active_Qitem->sr_dval;      // Character to print is placed in databuf
                                  serd_databuf[1] = 0;                                   // string terminator
                                  serd_active_Qitem->sr_SendNow = TRUE;
                                  serd_active_Qitem->sr_sptr    = serd_databuf;             // sptr point to the data in databuf
                                  break;
                                    
          case SERO_TYPE_32:      ItoH(serd_active_Qitem->sr_dval, serd_databuf);        // 32-bit to Hex Data Conversion.  Place in databuf
                                  serd_databuf[8]  = ASCII_CARRIAGERETURN;               // tack on <CR> at the end
                                  serd_databuf[9]  = ASCII_LINEFEED;                     // tack on <LF> at the end
                                  serd_databuf[10] = 0;                                  // this is the string terminator
                                  break;
                                  
          case SERO_TYPE_32N:     ItoH(serd_active_Qitem->sr_dval, serd_databuf);        // 8-bit to Hex Data Conversion.  Place in databuf
                                  break;
                                    
          case SERO_TYPE_8:       BtoH((u8)serd_active_Qitem->sr_dval, serd_databuf);    // 8-bit to Hex Data Conversion.  Place in databuf
                                  serd_databuf[2] = ASCII_CARRIAGERETURN;                // tack on <CR> at the end
                                  serd_databuf[3] = ASCII_LINEFEED;                      // tack on <LF> at the end
                                  serd_databuf[4] = 0;                                   // this is the string terminator
                                  break;
                                  
          case SERO_TYPE_8N:      BtoH((u8)serd_active_Qitem->sr_dval, serd_databuf);    // 8-bit to Hex Data Conversion.  Place in databuf
                                  break;                          
        }
        
        return;                                                                         // could FALL_THRU, but don't be a CPU hog!


    case SERO_STATE_WRITE_TO_LOCALBUF:                                                        // Actively printing out characters

    	  // WAIT_ON_SHARED_BUF_AVAILABLE!
          //if( !(USART1->SR & USART_FLAG_TC) ) { return; }

        *dbufPtr++  = *serd_active_Qitem->sr_sptr++;
        serd_ALen++;                                   // Accumulated Length

        end_of_string = *serd_active_Qitem->sr_sptr;                               // Examine character just past the one printed

        if ((end_of_string == 0) || (serd_Alen == (SIZE_LBUF-2)))                  // Found the string terminator, or overflowing?
        {
        	*dbufPtr = 0;                                                          // dbuf[62] when SIZE_LBUF=63.

        	if (serd_active_Qitem->sr_otype >= SERO_TYPE_32 && serd_Alen < (SIZE_LBUF-2))         // Any of these types: need to print out the val in databuf
            {                                                                                     //    [ ordering in SERD_OTYPE is important!! ]
                serd_active_Qitem->sr_otype = SERO_TYPE_STR;                                      // change type to STR
                serd_active_Qitem->sr_sptr  = (char *)serd_databuf;                               // data is in serd_databuf
                break;
            }

            serd_ostate_machine = SERO_STATE_GETJOB;                            // switch state:  look for another job
            --serd_num_Qitems;                                                  // Can now decrement Queue size by 1
            if( ++serd_out_Qindex == SERO_SQENTRYS ) { serd_out_Qindex = 0; }   // index to next element in the Circular Q.  Wrap if necessary

            if( serd_active_Qitem->sr_compPtr != 0 )                            // Is there a valid Completion Pointer ?
                *serd_active_Qitem->sr_compPtr = 1;                             //    signal a 1 to that address to indicate completion
        }


            // Allow to fall thru

    case SERO_STATE_PRINTSTRING:

        if( !(USART1->SR & USART_FLAG_TC) ) { return; }
        USART1->DR  = serd_printchar;

        serd_printchar = serd_lbuf[serd_Iprint++];

        if ((serd_printchar == 0) || (serd_Iprint == SIZE_LBUF))
            { serd_ostate_machine = SERO_STATE_GETJOB; }

        break;

    }
}









void U2_PrintCH( char ch )
{
    U2_Send( SERO_TYPE_ONECHAR, 0, 0, (u32)ch );
}

void U2_PrintSTR( const char *pstr )
{
    U2_Send( SERO_TYPE_STR, (char *)pstr, 0, 0 );
}

void U2_PrintSTRNow( const char *pstr )
{
    U2_Send( SERO_TYPE_STRNOW, (char *)pstr, 0, 0 );
}

void U2_Print32( const char *pstr, u32 val )
{
    U2_Send( SERO_TYPE_32, (char *)pstr, 0, val );
}

void U2_Print32N( const char *pstr, u32 val )
{
    U2_Send( SERO_TYPE_32N, (char *)pstr, 0, val );
}

void U2_Print8( const char *pstr, u8 val )
{
    U2_Send( SERO_TYPE_8, (char *)pstr, 0, (u32)val );
}

void U2_Print8N( const char *pstr, u8 val )
{
    U2_Send( SERO_TYPE_8N, (char *)pstr, 0, (u32)val );
}


int _write( void *fp, char *buf, u32 len )
{
    u32  remaining = (LEN_PRINTF_BUF - serd_pfindex);

    if( remaining < (len+2) ) { serd_pfindex=0; }                    // if it won't fit, wrap

    strncpy((void *)&serd_pfbuf[serd_pfindex], buf, len);
    U2_PrintSTR( (const char *)&serd_pfbuf[serd_pfindex] );

    serd_pfindex               += len;
    serd_pfbuf[serd_pfindex++]  = 0;

    if( serd_pfindex >= (LEN_PRINTF_BUF - 6) ) { serd_pfindex=0; }   // wrap if its close to the end

    return len;
}






#ifdef ACCUMULATING STRINGS

    case SERO_STATE_WRITE_TO_LOCALBUF:                                                        // Actively printing out characters

    	  // WAIT_ON_SHARED_BUF_AVAILABLE!


        //if( !(USART1->SR & USART_FLAG_TC) ) { return; }
        //USART1->DR     = *serd_active_Qitem->sr_sptr++;


        dlen      = (u8)strlen(serd_active_Qitem->sr_sptr);
        avail_len = SIZE_LBUF - serd_Ilbuf;

        if ((dlen+1) > avail_len)
        {
            strncpy((void *)&serd_lbuf[serd_Ilbuf], serd_active_Qitem->sr_sptr, avail_len);        // fill as much as you can
            strcpy((void *)&serd_lbuf[SIZE_LBUF - 3],"\n\r");                                     // last 3 chars of lbuf:  0x13 0x10 0x00
            serd_Ilbuf  = SIZE_LBUF - 1;                                                // will be a 00 at Ilbuf
        }
        else
        {
            strcpy((void *)&serd_lbuf[serd_Ilbuf], serd_active_Qitem->sr_sptr);              // Copies a 0 into the buffer, (may not have \n\r)
            serd_Ilbuf += dlen;                                                       // will be 00 at LbufIndex
        }

        if ((serd_active_Qitem->sr_otype >= SERO_TYPE_32) && (serd_Ilbuf < (SIZE_LBUF-4)))
        {                                                                       //    [ ordering in SERD_OTYPE is important!! ]
            serd_active_Qitem->sr_otype = SERO_TYPE_STR;                        // change type to STR
            serd_active_Qitem->sr_sptr  = (char *)serd_databuf;                 // data is in serd_databuf
            break;
        }
        else                                                                    // ELSE this print job is done
        {
            --serd_num_Qitems;                                                  // Can now decrement Queue size by 1
            if( ++serd_out_Qindex == SERO_SQENTRYS ) { serd_out_Qindex = 0; }   // index to next element in the Circular Q.  Wrap if necessary

            if( serd_active_Qitem->sr_compPtr != 0 )                            // Is there a valid Completion Pointer ?
                *serd_active_Qitem->sr_compPtr = 1;

            ch1 = serd_lbuf[serd_Ilbuf-1];
            ch2 = serd_lbuf[serd_Ilbuf-2];

            if ( (ch1 != '\n' || ch2 != '\r') && (ch1 != '\r' || ch2 != '\n') && (serd_active_Qitem->sr_SendNow == FALSE))
            {
            	serd_ostate_machine = SERO_STATE_GETJOB;
                break;                                                                   // need to keep filling string
            }
            else
            {
                serd_ostate_machine = SERO_STATE_PRINTSTRING;
                serd_Iprint         = 0;
                serd_Ilbuf          = 0;     // finished up filling string.  Init to 0.
                serd_printchar      = serd_lbuf[serd_Iprint++];
            }
            // Allow to fall thru
        }

        ------------------------------

    case SERO_STATE_WRITE_TO_LOCALBUF:                                                        // Actively printing out characters

    	  // WAIT_ON_SHARED_BUF_AVAILABLE!

#if SERIAL_CONSOLE == dev_USART1
        //if( !(USART1->SR & USART_FLAG_TC) ) { return; }
        //USART1->DR     = *serd_active_Qitem->sr_sptr++;
#elif SERIAL_CONSOLE == dev_USART2
        if( !(USART2->SR & USART_FLAG_TC) ) { return; }                             // TC=1 when Transmission is Complete
        USART2->DR     = *serd_active_Qitem->sr_sptr++;                             // TX reg filled with a byte of data
#elif SERIAL_CONSOLE == dev_USART2
        if( !(USART6->SR & USART_FLAG_TC) ) { return; }                             // TC=1 when Transmission is Complete
        USART6->DR     = *serd_active_Qitem->sr_sptr++;                             // TX reg filled with a byte of data
#endif

        dlen = (u8)strlen(serd_active_Qitem->sr_sptr);

        if ((dlen+1) > SIZE_LBUF)
        {
            strncpy((void *)serd_lbuf, serd_active_Qitem->sr_sptr, SIZE_LBUF);       // fill as much as you can
            strcpy((void *)&serd_lbuf[SIZE_LBUF - 5],"XX\n\r");                      // last 5 chars of lbuf:  0x58 0x58 0x13 0x10 0x00
        }
        else
        {
            strcpy((void *)serd_lbuf, serd_active_Qitem->sr_sptr);              // Copies a 0 into the buffer, (may not have \n\r)
        }

        if (serd_active_Qitem->sr_otype >= SERO_TYPE_32)
        {                                                                       //    [ ordering in SERD_OTYPE is important!! ]
            dblen = strlen(serd_databuf);

            if ((dlen + dblen + 1) > SIZE_LBUF)
            	strcpy((void *)&serd_lbuf[SIZE_LBUF - 6],"XXX\n\r");        // XXX indicates overflow
            else
            	strcpy((void *)&serd_lbuf[dlen], serd_databuf);
        }

        --serd_num_Qitems;                                                  // Can now decrement Queue size by 1
        if( ++serd_out_Qindex == SERO_SQENTRYS ) { serd_out_Qindex = 0; }   // index to next element in the Circular Q.  Wrap if necessary

        if( serd_active_Qitem->sr_compPtr != 0 )                            // Is there a valid Completion Pointer ?
            *serd_active_Qitem->sr_compPtr = 1;

        serd_ostate_machine = SERO_STATE_PRINTSTRING;
        serd_Iprint         = 0;
        serd_printchar      = serd_lbuf[serd_Iprint++];

            // Allow to fall thru

#endif




