
#include "stm32f4xx.h"
#include "SerialOutp.h"


#define ASCII_BACKSPACE         8
#define ASCII_LINEFEED          10
#define ASCII_CARRIAGERETURN    13
#define ASCII_SPACE             32
#define ASCII_TILDE             126
#define ASCII_DELETE            127




#define SERO_STATE_GETJOB      0
#define SERO_STATE_DOCHARS     1
#define SERO_SQENTRYS          100




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
static char   serd_databuf[11];                //   data buffer for value conversion


static char hexv[] =  { '0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9' ,'A', 'B', 'C', 'D', 'E', 'F' };





void U2_Init( void )
{
    serd_num_Qitems     = 0;
    serd_inn_Qindex     = 0;
    serd_out_Qindex     = 0;
    serd_ostate_machine = SERO_STATE_GETJOB;
}


//---------------------------------------------------------------------------------------
//  Queues up a Job on the 'Serial Jobs Output List' (Implemented as a Circular Q)
//  This is the 'Front End' or Producer of Serial Data.   The idea is to stash the
//  parameters quickly in a Queue, then let the 'Back End' (the Consumer) process the
//  data off the Queue, and perform the printing.
//  The data is destined for USART2
//
//  Parameters:
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
void U2_Send( u32 otype, char *sptr, u32 *completionptr, u32 aval )
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
}


static void BtoH( u8 val, char *S )
{
    s8 k;

    for( k=1; k >= 0; --k )
    {
        S[k]   = hexv[val & 0xF];
        val  >>= 4;
    }

    S[2] = 0;
}

static void ItoH( u32 val, char *S )
{
    s8  k;

    for( k=7; k >= 0; --k )
    {
        S[k]   = hexv[val & 0xF];
        val  >>= 4;
    }

    S[8] = 0;
}



void U2_Process( void )
{
    char  end_of_string;

    switch( serd_ostate_machine )
    {
    case SERO_STATE_GETJOB:                                                              // Looking for items on the Queue

        if( serd_num_Qitems == 0 ) { return; }                                           // return IMMEDIATELY if Queue is empty

        serd_active_Qitem   = &serd_Q_items[serd_out_Qindex];                            // Item to operate on is at 'serd_out_qindex'
        serd_ostate_machine = SERO_STATE_DOCHARS;                                        // switch state to 'Processing Characters'

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
        
        return;                                                                          // could FALL_THRU, but don't be a CPU hog!


    case SERO_STATE_DOCHARS:                                                        // Actively printing out characters

    	if( !(USART2->SR & USART_FLAG_TC) ) { return; }                             // TC=1 when Transmission is Complete

        USART2->DR     = *serd_active_Qitem->sr_sptr++;                             // TX reg filled with a byte of data
        end_of_string  = *serd_active_Qitem->sr_sptr;                               // Examine character just past the one printed

        if( end_of_string == 0 )                                                    // Found the string terminator ?
        {
            if( serd_active_Qitem->sr_otype >= SERO_TYPE_32 )                       // Any of these types: need to print out the val in databuf
            {                                                                       //    [ ordering in SERD_OTYPE is important!! ]
                serd_active_Qitem->sr_otype = SERO_TYPE_STR;                        // change type to STR
                serd_active_Qitem->sr_sptr  = (char *)serd_databuf;                 // data is in serd_databuf
            }
            else                                                                    // ELSE this print job is done
            {
                serd_ostate_machine = SERO_STATE_GETJOB;                            // switch state:  look for another job
                --serd_num_Qitems;                                                  // Can now decrement Queue size by 1
                if( ++serd_out_Qindex == SERO_SQENTRYS ) { serd_out_Qindex = 0; }   // index to next element in the Circular Q.  Wrap if necessary

                if( serd_active_Qitem->sr_compPtr != 0 )                            // Is there a valid Completion Pointer ?
                    *serd_active_Qitem->sr_compPtr = 1;                             //    signal a 1 to that address to indicate completion
            }
        }
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

