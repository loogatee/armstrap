
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

static u8   serd_lbuf[SIZE_LBUF];
static u8   serd_printchar;
static u8   serd_Iprint;
static u8  *serd_dbufPtr;
static u8   serd_Alen;






void U2_Init( void )
{
    serd_num_Qitems     = 0;
    serd_inn_Qindex     = 0;
    serd_out_Qindex     = 0;
    serd_ostate_machine = SERO_STATE_GETJOB;
    serd_pfindex        = 0;
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

        if( completionptr ) { *completionptr = 0; }                             // if pointer is valid, store 0, indicating Not Done
    }
                                                                                // Else the Q is full.  Effectively tosses the data
}                                                                               //   note intention not to return -1 if Q is full






void U2_Process( void )
{
    u8    end_of_string;

    switch( serd_ostate_machine )
    {
    case SERO_STATE_GETJOB:                                                              // Looking for items on the Queue

        if( serd_num_Qitems == 0 ) { return; }                                           // return IMMEDIATELY if Queue is empty

        serd_active_Qitem   = &serd_Q_items[serd_out_Qindex];                            // Item to operate on is at 'serd_out_qindex'
        serd_ostate_machine = SERO_STATE_WRITE_TO_LOCALBUF;                              // switch state to 'Processing Characters'

        serd_dbufPtr = serd_lbuf;
        serd_Alen    = 0;

        switch( serd_active_Qitem->sr_otype )                                            // sr_otype dictates the action
        {
          case SERO_TYPE_ONECHAR: serd_databuf[0] = (u8)serd_active_Qitem->sr_dval;      // Character to print is placed in databuf
                                  serd_databuf[1] = 0;                                   // string terminator
                                  serd_active_Qitem->sr_sptr = serd_databuf;             // sptr point to the data in databuf
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
        
        break;                                                                      // could FALL_THRU, but don't be a CPU hog!


    case SERO_STATE_WRITE_TO_LOCALBUF:                                                   // Actively printing out characters

    	  // WAIT_ON_SHARED_BUF_AVAILABLE!
          //if( !(USART1->SR & USART_FLAG_TC) ) { return; }

        *serd_dbufPtr++ = *serd_active_Qitem->sr_sptr++;                                  // instead of a uart, write to a buffer (like the shared buffer)
        serd_Alen++;                                                                       // Increment Accumulated Length

        end_of_string = *serd_active_Qitem->sr_sptr;                                       // take a peek: Examine character just past the one printed

        if ((end_of_string == 0) || (serd_Alen == (SIZE_LBUF-1)))                          // Found the string terminator?, Or at the MAX LENGTH allowed??
        {                                                                                  //    The -1 FORCES space for 0x00 in last byte at buf[SIZE_LBUF-1]
        	*serd_dbufPtr = 0;                                                             // buf will ALWAYS, ALWAYS be terminated with a 0

        	if (serd_active_Qitem->sr_otype >= SERO_TYPE_32 && serd_Alen < (SIZE_LBUF-1))  // Value in databuf to print? DO ONLY if within buf size limit
            {                                                                              //    note that ordering in SERD_OTYPE is important!! (hacky)
                serd_active_Qitem->sr_otype = SERO_TYPE_STR;                               // change type to STR.  so this code will not re-enter
                serd_active_Qitem->sr_sptr  = (char *)serd_databuf;                        // data is in serd_databuf.   no change in state.
            }
        	else
        	{
                --serd_num_Qitems;                                                         // Done with Message.  Decrement Queue size by 1
                if( ++serd_out_Qindex == SERO_SQENTRYS ) { serd_out_Qindex = 0; }          // index to next element in the Circular Q.  Wrap if necessary

                if( serd_active_Qitem->sr_compPtr != 0 )                                   // Is there a valid Completion Pointer ?
                    *serd_active_Qitem->sr_compPtr = 1;                                    //    signal a 1 to that address to indicate completion

                serd_ostate_machine = SERO_STATE_PRINTSTRING;                              // All the data is in the buffer.   Now print it.
                serd_Iprint         = 0;                                                   // Index into serd_lbuf
                serd_printchar      = serd_lbuf[serd_Iprint++];                            // 0th char in serd_lbuf
        	}
        }

        break;


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






#ifdef ACCUMULATINGSTRINGS


    case SERO_STATE_WRITE_TO_LOCALBUF:                                                       // Copy what will fit to the local shared buffer.

    	if (*ValidBytePtr == 1) { return; }                                                  // The other side will eventually drain, and flip to 0

        dlen = (u8)strlen(serd_active_Qitem->sr_sptr);                                       // Length of string to be printed

        if ((dlen+1) > SIZE_LBUF)                                                            // Enforce size limit
        {                                                                                    // NOTE:  both cases result in 0x00-terminated string
            strncpy((void *)serd_lbuf, serd_active_Qitem->sr_sptr, (SIZE_LBUF-1));           // fill as much as necessary.
            serd_lbuf[SIZE_LBUF-1] = 0x00;                                                   // force terminate
        }
        else
        {
            strcpy((void *)serd_lbuf, serd_active_Qitem->sr_sptr);                           // Room!! copy into the shared buffer.  Guaranteed 0x00 terminator
        }

        if ((serd_active_Qitem->sr_otype) >= SERO_TYPE_32 && (dlen < (SIZE_LBUF-2)))         // TYPE_8N is 2 bytes.  That's the only one that could squeeze in
        {
            dblen = strlen(serd_databuf);                                                    // There's an additional value in serd_databuf to print out
                                                                                             // The buffer is only so big.  If it won't fit, do nothing.
            if ((dlen + dblen + 1) < SIZE_LBUF)                                              // (Size current buf) + (size databuf) + (0x00 terminator):  will it fit?
            {
                strcpy((void *)&serd_lbuf[dlen], serd_databuf);                              // If it fits, copy to the shared buffer.
            }                                                                                // note string destination. starts where the 1st string ends.
        }

        --serd_num_Qitems;                                                  // Can now decrement Queue size by 1
        if( ++serd_out_Qindex == SERO_SQENTRYS ) { serd_out_Qindex = 0; }   // index to next element in the Circular Q.  Wrap if necessary

        if( serd_active_Qitem->sr_compPtr != 0 )                            // Is there a valid Completion Pointer ?
            *serd_active_Qitem->sr_compPtr = 1;                             //   If yes, hit with a signal so it knows.

        serd_ostate_machine = SERO_STATE_PRINTSTRING;
        serd_Iprint         = 0;
        serd_printchar      = serd_lbuf[serd_Iprint++];

            // Allow to fall thru

#endif




