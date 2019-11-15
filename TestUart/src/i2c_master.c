#include "stm32f4xx.h"
#include "proj_common.h"
#include "Uart.h"
#include "Cmds.h"
#include "string.h"
#include "i2c.h"
#include "timer.h"
#include "Utils.h"


#define I2C_1_QENTRYS   5
#define TIMEOUT_VAL     35


#define RTN_CONTINUE    0
#define RTN_RESTART     1
#define RTN_SUCCESS     2


#define I2C_STATE_GETJOB                 0
#define I2C_STATE_GO_AGAIN               1
#define I2C_STATE_SENDSTART              2
#define I2C_STATE_START_COMPLETE         3
#define I2C_STATE_WRITE_SLAVEADDR        4
#define I2C_STATE_WRITE_CMDREG           5
#define I2C_STATE_WRITE_CMDREG2          6
#define I2C_STATE_WRITE_WAITBTF          7
#define I2C_STATE_WAIT_STOPF             8
#define I2C_STATE_START_COMPLETE2        9
#define I2C_STATE_WRITE_SLAVEADDR_RW     10
#define I2C_STATE_READ_THE_DATA          11
#define I2C_STATE_WAIT_MBR               12
#define I2C_STATE_WAIT_STOP              13
#define I2C_STATE_DO_WRITEONLY           14
#define I2C_STATE_FINISH_CLEANUP         15
#define I2C_STATE_WRITE_WAITBTF1         16

#define I2C_WRITE_SUBSTATE_START         0
#define I2C_WRITE_SUBSTATE_WAITCOMPLETE  1



#define I2C_ADD_TO_QUEUE    &i2c_1_data[i2c_1_inn_qindex]; \
	                        ++i2c_1_num_qitems; \
                            if( ++i2c_1_inn_qindex == I2C_1_QENTRYS ) { i2c_1_inn_qindex = 0; }

#define I2C_DEL_FROM_QUEUE  --i2c_1_num_qitems; \
        	                if( ++i2c_1_out_qindex == I2C_1_QENTRYS ) { i2c_1_out_qindex = 0; }




typedef struct
{
    u8       *ic_dptr;            // destination for Read Data
    u8       *ic_wdata;           // Source ptr to Write Data
    u32      *ic_complptr;        // completion Ptr
    I2CCMDS  *ic_cmdptr;          // Ptr to the Job List
} II2C;




static II2C     i2c_1_data[I2C_1_QENTRYS];
static u8       i2c_1_num_qitems;
static u8       i2c_1_inn_qindex;
static u8       i2c_1_out_qindex;
static II2C    *i2c_1_activeitem;
static u8       i2c_1_state;
static u32      i2c_1_starttime;
static u8      *i2c_1_lcdptr;
static u8      *i2c_1_lcwdata;
static u8       i2c_1_lcnumbytes;
static u8       i2c_1_lcslave;
static u8       i2c_1_lccmd;
static u8       i2c_1_lccmd2;
static u8       i2c_1_lccmdtype;
static I2CCMDS *i2c_1_lclistptr;
static bool     i2c_1_useCmdReg2;
static u8       i2c_1_wtype;

static u8       i2c_1_write_substate;
static u8       i2c_1_write_data;
static u8       i2c_1_cleanup_count;


static u8  i2c_1_do_write_substate( unsigned int   wait_type );


extern void init_gpioI2C( void );


void I2C_master_Init( void )
{
    memset( (void *)i2c_1_data, 0, sizeof(i2c_1_data) );

    i2c_1_num_qitems    = 0;
    i2c_1_inn_qindex    = 0;
    i2c_1_out_qindex    = 0;
    i2c_1_cleanup_count = 0;
      
    i2c_1_state = I2C_STATE_GETJOB;
}



bool I2C_master_SendCmd( u8 *dptr, u8 *wptr, u32 *complptr, I2CCMDS *listptr )
{
    II2C  *lqitem;
    bool   retv = FALSE;

    if( i2c_1_num_qitems != I2C_1_QENTRYS )
    {
        retv                = TRUE;
        lqitem              = I2C_ADD_TO_QUEUE;
        lqitem->ic_dptr     = dptr;
        lqitem->ic_wdata    = wptr;
        lqitem->ic_complptr = complptr;
        lqitem->ic_cmdptr   = listptr;
        
        if( complptr ) { *complptr = I2C_COMPLETION_BUSY; }
    }

    return retv;
}



static void cleanup( void )
{
	GPIO_InitTypeDef Xgpio;

	if( ++i2c_1_cleanup_count == 1 )
	{
		U2_Print8( "cleanup, i2c_1_state: ", i2c_1_state );
	    U2_Print32("  SR1: ", STM_REGISTER I2C1->SR1);
	    U2_Print32("  SR2: ", STM_REGISTER I2C1->SR2);
	}

   	STM_REGISTER I2C1->CR1 |= I2C_CR1_SWRST;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1,   DISABLE);
    STM_REGISTER GPIOB->AFR[0] = 0;

    GPIO_StructInit(&Xgpio);
     Xgpio.GPIO_Mode  = GPIO_Mode_OUT;
     Xgpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
     Xgpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
     Xgpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &Xgpio);

    GPIO_SetBits(GPIOB, GPIO_Pin_6|GPIO_Pin_7);

	i2c_1_state = I2C_STATE_FINISH_CLEANUP;                               // finish the cleanup in master_Process()
}





//
void I2C_master_Process( void )
{
    u8  tmpb;
    u16 S1,S2;


    if( i2c_1_state != I2C_STATE_GETJOB && i2c_1_state != I2C_STATE_FINISH_CLEANUP )     // If the state machine is active
    {
        if( GetSysDelta( i2c_1_starttime ) >= TIMEOUT_VAL )                              // Safety valve: too long to complete this job ?
        {
        	U2_Print8( "TO! i2c_1_state: ", i2c_1_state );                               //     Message out to debug port (optional)

        	I2C_DEL_FROM_QUEUE;                                                          //     Remove the active job from the Queue

        	if( i2c_1_activeitem->ic_complptr != 0 )                                     //     Valid Completion Pointer ?
        	    *i2c_1_activeitem->ic_complptr = I2C_COMPLETION_TIMEOUT;                 //         signal to indicate completion
        	cleanup();                                                                   //     Attempt to recover the bus
            // Log Occurence here!
            return;                                                                      //     return immediately
        }
    }

    switch( i2c_1_state )
    {
    case I2C_STATE_FINISH_CLEANUP:

    	init_gpioI2C();

    	if( (i2c_1_cleanup_count < 50) && (STM_REGISTER I2C1->SR2 & I2C_SR2_BUSY) )    // cleanup_count prevents an endless loop
    	{                                                                              // We're really looking for the BUSY bit to be clear.
    	    cleanup();                                                                 // attempts another reset of the I2C line
    	    return;
    	}
    	U2_Print8("cleanup_count: ", i2c_1_cleanup_count);
    	i2c_1_cleanup_count = 0;
    	i2c_1_state         = I2C_STATE_GETJOB;
    	break;

    case I2C_STATE_GETJOB:

        if( i2c_1_num_qitems == 0 ) { return; }                                   // If nothing on the Q, jump out

        i2c_1_activeitem     = &i2c_1_data[i2c_1_out_qindex];                     // Got something. ActiveItem is at out_qindex
        i2c_1_lclistptr      = i2c_1_activeitem->ic_cmdptr;                       // makes local copy of the Command List
        i2c_1_starttime      = GetSysTick();                                      // Initialize the "safety valve" ticker
        i2c_1_write_substate = I2C_WRITE_SUBSTATE_START;                          // Make sure write_substate is initialized for each job
        FALL_THRU;

    case I2C_STATE_GO_AGAIN:

        i2c_1_lcslave    = i2c_1_lclistptr->ct_slaveaddr;                         // Local Copy, Slave Address
        i2c_1_lccmd      = i2c_1_lclistptr->ct_cmdreg;                            // Local Copy, Command Register
        i2c_1_lccmd2     = i2c_1_lclistptr->ct_cmdreg2;                           // Local Copy, Command Register #2
        i2c_1_lcnumbytes = i2c_1_lclistptr->ct_numbytes;                          // Local Copy, Number of Bytes
        i2c_1_lccmdtype  = i2c_1_lclistptr->ct_cmdtype;                           // Local Copy, Command Type
        i2c_1_useCmdReg2 = FALSE;

        if( i2c_1_lccmdtype & I2C_CMDTYPE_CMDREG2 )
        {
            i2c_1_lccmdtype &= ~I2C_CMDTYPE_CMDREG2;
            i2c_1_useCmdReg2 = TRUE;
        }
        
        i2c_1_lcdptr  = i2c_1_activeitem->ic_dptr;                                // Local Copy, Data Pointer
        i2c_1_lcwdata = i2c_1_activeitem->ic_wdata;                               // Local Copy, Pointer to Write Data
        i2c_1_state   = I2C_STATE_SENDSTART;                                      // transition to SENDSTART
        FALL_THRU;                                                                // no need to break

    case I2C_STATE_SENDSTART:

    	if( STM_REGISTER I2C1->SR2 & I2C_SR2_BUSY ) {return;}                     // Must wait for IDLE

        i2c_1_state = I2C_STATE_START_COMPLETE;                                   // Have IDLE, new state will be START_COMPLETE
        STM_REGISTER I2C1->CR1 |= I2C_CR1_START;                                  // START!
        FALL_THRU;                                                                // no need to break

    case I2C_STATE_START_COMPLETE:

    	S1 = STM_REGISTER I2C1->SR1;
    	S2 = STM_REGISTER I2C1->SR2;
    	if( !(S1 & I2C_SR1_SB) ||  !(S2 & I2C_SR2_MSL) || !(S2 & I2C_SR2_BUSY) ) { return; }

        i2c_1_state      = I2C_STATE_WRITE_SLAVEADDR;                             // Got it!  Transition to WRITE_SLAVEADDR
        i2c_1_write_data = i2c_1_lcslave;                                         // This is the byte which will be written
        FALL_THRU;                                                                // no need to break

    case I2C_STATE_WRITE_SLAVEADDR:
            
        if( i2c_1_do_write_substate(1) == RTN_CONTINUE ) { return; }     // Keep calling write_substate while return value says to CONTINUE
                                                                                  // ELSE return indicates SUCCESS!
        i2c_1_state      = I2C_STATE_WRITE_CMDREG;                                //    new state is WRITE_CMDREG
        i2c_1_write_data = i2c_1_lccmd;                                           //    this is the byte which will be written
        FALL_THRU;                                                                //    no need to break

    case I2C_STATE_WRITE_CMDREG:

        if( i2c_1_do_write_substate(2) == RTN_CONTINUE ) { return; }     // Keep calling write_substate while return value says to CONTINUE

        if( i2c_1_useCmdReg2 == FALSE )
        {
            if( i2c_1_lccmdtype == I2C_CMDTYPE_WRITEONLY )                        // Check to see if this job is WRITE-ONLY
            {                                                                     //    Means nothing to read, need only to write some registers
                i2c_1_state      = I2C_STATE_DO_WRITEONLY;                        //    new state is DO_WRITEONLY
                i2c_1_write_data = *i2c_1_lcwdata ++;                             //    Get the byte to write, and increment pointer thru this list
                return;                                                           //    return immediately
            }
            i2c_1_state = I2C_STATE_WRITE_WAITBTF;                                // Wait for BTF bit to be set
        }
        else
        {
            i2c_1_state      = I2C_STATE_WRITE_CMDREG2;
            i2c_1_write_data = i2c_1_lccmd2;
            return;
        }
        FALL_THRU;                                                                // no need to break

    case I2C_STATE_WRITE_CMDREG2:

        if( i2c_1_state == I2C_STATE_WRITE_CMDREG2 )
        {
            if( i2c_1_do_write_substate(2) == RTN_CONTINUE ) { return; }  // Keep calling write_substate while return value says to CONTINUE
                                                                                  // ELSE return indicates SUCCESS!
            if( i2c_1_lccmdtype == I2C_CMDTYPE_WRITEONLY )                        // Check to see if this job is WRITE-ONLY
            {                                                                     //    Means nothing to read, need only to write some registers
                i2c_1_state      = I2C_STATE_DO_WRITEONLY;                        //    new state is DO_WRITEONLY
                i2c_1_write_data = *i2c_1_lcwdata ++;                             //    Get the byte to write, and increment pointer thru this list
                return;                                                           //    return immediately
            }

            i2c_1_state = I2C_STATE_WRITE_WAITBTF;                                // ELSE wait on BTF bit
        }
        FALL_THRU;

    case I2C_STATE_WRITE_WAITBTF:

    	if( !(STM_REGISTER I2C1->SR1 & I2C_SR1_BTF) ) { return; }

    	STM_REGISTER I2C1->CR1 |= I2C_CR1_STOP;
        i2c_1_state = I2C_STATE_WAIT_STOPF;
        FALL_THRU;

    case I2C_STATE_WAIT_STOPF:

        if( (STM_REGISTER I2C1->SR1 & I2C_SR1_STOPF) || (STM_REGISTER I2C1->SR2 &  I2C_SR2_BUSY) ) { return; }

        STM_REGISTER I2C1->CR1 |= I2C_CR1_ACK;                              // I2C_AcknowledgeConfig(I2Cx, ENABLE);
        STM_REGISTER I2C1->CR1 &= I2C_NACKPosition_Current;                 // I2C_NACKPositionConfig(I2Cx, I2C_NACKPosition_Current);
        STM_REGISTER I2C1->CR1 |= I2C_CR1_START;                            // START!

        i2c_1_state = I2C_STATE_START_COMPLETE2;
        FALL_THRU;

    case I2C_STATE_START_COMPLETE2:

        S1 = STM_REGISTER I2C1->SR1;
        S2 = STM_REGISTER I2C1->SR2;
        if( !(S1 & I2C_SR1_SB) ||  !(S2 & I2C_SR2_MSL) || !(S2 & I2C_SR2_BUSY) ) { return; }

        i2c_1_state      = I2C_STATE_WRITE_SLAVEADDR_RW;                          // Transition the state
        i2c_1_write_data = i2c_1_lcslave | 1;                                     // IMPORTANT!!  The 'OR 1' turns the bus around, so Read is Enabled
        FALL_THRU;                                                                // no need to break

    case I2C_STATE_WRITE_SLAVEADDR_RW:

        if( i2c_1_do_write_substate(3) == RTN_CONTINUE ) { return; }              // Keep calling write_substate while return value says to CONTINUE

        i2c_1_state = I2C_STATE_READ_THE_DATA;                                    // new state = READ_THE_DATA
        tmpb        = i2c_1_lclistptr->ct_numbytes;

        if( tmpb > 2 )
        {
            hammer( I2C1->SR2 );                                                  // Read SR2, toss
        }
        else if( tmpb == 2 )
        {
        	STM_REGISTER I2C1->CR1 |= I2C_NACKPosition_Next;
            //__disable_irq();
        	hammer( I2C1->SR2 );                                                  // Clear ADDR flag
        	I2C1->CR1 &= (uint16_t)~((uint16_t)I2C_CR1_ACK);
            //__enable_irq();
        }

        FALL_THRU;


    case I2C_STATE_READ_THE_DATA:

    	//I2C_AcknowledgeConfig(I2C1, DISABLE);
    	//I2C_AcknowledgeConfig(I2C1, DISABLE);
    	/* Disable the acknowledgement */
    	//I2Cx->CR1 &= (uint16_t)~((uint16_t)I2C_CR1_ACK);
    	tmpb = i2c_1_lclistptr->ct_numbytes;

    	if( (tmpb > 1) && !(STM_REGISTER I2C1->SR1 & I2C_SR1_BTF) ) { return; }

    	if( (tmpb > 2) && (i2c_1_lcnumbytes != 3) )
    	{
    		*i2c_1_lcdptr++ = STM_REGISTER I2C1->DR;
    	    i2c_1_lcnumbytes--;
    	    return;
    	}



      	if( tmpb == 1 )
      	{
      		STM_REGISTER I2C1->CR1 &= (uint16_t)~((uint16_t)I2C_CR1_ACK);        // Disable the acknowledgement
      		//__disable_irq();
      		hammer( I2C1->SR2 );
      		STM_REGISTER I2C1->CR1 |= I2C_CR1_STOP;                            // Generate a STOP condition
      	    //__enable_irq();
      		i2c_1_wtype = 1;
      	}
      	else if( tmpb == 2 )
      	{
      		//__disable_irq();
      		STM_REGISTER I2C1->CR1 |= I2C_CR1_STOP;                            // Generate a STOP condition
      		*i2c_1_lcdptr++ = STM_REGISTER I2C1->DR;
      		//__enable_irq();
      		*i2c_1_lcdptr++ = STM_REGISTER I2C1->DR;
      		i2c_1_state = I2C_STATE_WAIT_STOP;
      		return;
      	}
      	else
      	{
      		STM_REGISTER I2C1->CR1 &= (uint16_t)~((uint16_t)I2C_CR1_ACK);        // Disable the acknowledgement
            //__disable_irq();
      	    *i2c_1_lcdptr++ = STM_REGISTER I2C1->DR;                           // U2_Print8("ch3: ", STM_REGISTER I2C1->DR);
            STM_REGISTER I2C1->CR1 |= I2C_CR1_STOP;                            // Generate a STOP condition
            //__enable_irq();
            i2c_1_wtype = 2;
            *i2c_1_lcdptr++ = STM_REGISTER I2C1->DR;                           // U2_Print8("ch2: ", STM_REGISTER I2C1->DR);
      	}

        i2c_1_state = I2C_STATE_WAIT_MBR;                                        // new state = READ_THE_DATA
        FALL_THRU;

    case I2C_STATE_WAIT_MBR:

    	S1 = STM_REGISTER I2C1->SR1;
    	S2 = STM_REGISTER I2C1->SR2;

    	if( i2c_1_wtype == 1)
    	{
    	    if( !(S1 & I2C_SR1_RXNE) ) {return;}
    	}
    	else
    	{
    		if( !(S1 & I2C_SR1_RXNE)  || !(S2 & I2C_SR2_MSL) || !(S2 & I2C_SR2_BUSY)  ) {return;}
    	}

    	*i2c_1_lcdptr++  = STM_REGISTER I2C1->DR;                                 // U2_Print8("ch1: ", STM_REGISTER I2C1->DR);
    	i2c_1_lcnumbytes = 0;
        i2c_1_state      = I2C_STATE_WAIT_STOP;                                   // transition state to looking for PEN to transition
        FALL_THRU;                                                                // Fall thru and immediately check

    case I2C_STATE_WAIT_STOP:

        if( STM_REGISTER I2C1->SR1 & I2C_SR1_STOPF ) { return; }                  // Spin here until STOPF is cleared

        tmpb = i2c_1_lclistptr->ct_numbytes;                                      // save numbytes before incrementing to new command field
        i2c_1_lclistptr++;                                                        // will point to the new Command Field
        if( i2c_1_lclistptr->ct_slaveaddr != 0xff )                               // New command list is Valid if Non FF
        {
            if( i2c_1_lccmdtype == I2C_CMDTYPE_RW )                               // Valid!  is this a 'RW' command ?
                i2c_1_activeitem->ic_dptr += tmpb;                                //     YES:  increment dptr, where read data goes
            else                                                                  // Else it's a WRITEONLY Job
                i2c_1_activeitem->ic_wdata += tmpb;                               //     increment where write data is coming from

            i2c_1_starttime = GetSysTick();                                       // Start timer for the new transaction
            i2c_1_state     = I2C_STATE_GO_AGAIN;                                 // Let's hit it again!
            return;                                                               // Hop out of state machine, back in a flash
        }

        i2c_1_state = I2C_STATE_GETJOB;                                           // All Done, new state is: Look for Another Job on the list

        I2C_DEL_FROM_QUEUE;
        
        if( i2c_1_activeitem->ic_complptr != 0 )                                  // Is there a valid Completion Pointer ?
            *i2c_1_activeitem->ic_complptr = I2C_COMPLETION_OK;                   //    signal a non-zero to that address to indicate completion
        //U2_Print32("end: ", GetSysTick());
        return;                                                                   // Get out of the i2c thread now

    case I2C_STATE_DO_WRITEONLY:

        if( i2c_1_do_write_substate(2) == RTN_CONTINUE ) { return; }              // Keep calling write_substate while return value says to CONTINUE

        if( --i2c_1_lcnumbytes == 0 )                                             // Have all the bytes been sent ?
        {
            i2c_1_state = I2C_STATE_WRITE_WAITBTF1;                               //     Transition state to WAIT_STOP
            return;                                                               //     hop out, returns to WAIT_STOP quickly
        }

        i2c_1_write_data = *i2c_1_lcwdata++;                                      // Assign the new byte to write, and increment through the list
        i2c_1_do_write_substate(2);                                               // kick off write state machine.  No worries about return val
        return;

    case I2C_STATE_WRITE_WAITBTF1:

        if( !(STM_REGISTER I2C1->SR1 & I2C_SR1_BTF) ) { return; }

        STM_REGISTER I2C1->CR1 |= I2C_CR1_STOP;
        i2c_1_state = I2C_STATE_WAIT_STOP;
        return;
    }


}



static u8 i2c_1_do_write_substate( unsigned int  WaitType )
{
    u16 S1,S2;

    switch( i2c_1_write_substate )
    {
    case I2C_WRITE_SUBSTATE_START:

        STM_REGISTER I2C1->DR = i2c_1_write_data;                     // transmits 1 byte
        i2c_1_write_substate = I2C_WRITE_SUBSTATE_WAITCOMPLETE;       // Success on write, transition to next state
        FALL_THRU;

    case I2C_WRITE_SUBSTATE_WAITCOMPLETE:

    	if( WaitType == 1 )
    	{
    		S1 = STM_REGISTER I2C1->SR1;
    		S2 = STM_REGISTER I2C1->SR2;

    		if( !(S1 & I2C_SR1_ADDR) || !(S1 & I2C_SR1_TXE) || !(S2 & I2C_SR2_MSL) || !(S2 & I2C_SR2_BUSY) || !(S2 & I2C_SR2_TRA) )
    		{
                return RTN_CONTINUE;                                      //  stay here, keep looking for BF to be 0
            }
    	}
    	else if( WaitType == 2 )
    	{
    		S1 = STM_REGISTER I2C1->SR1;
            if( !(S1 & I2C_SR1_TXE) ) { return RTN_CONTINUE; }
    	}
    	else if( WaitType == 3 )
    	{
    	    S1 = STM_REGISTER I2C1->SR1;
    	    if( !(S1 & I2C_SR1_ADDR) ) { return RTN_CONTINUE; }
    	}

        i2c_1_write_substate = I2C_WRITE_SUBSTATE_START;           //  BF cleared!  Now look for IDLE ACK
        break;                                                        // get out
    }
    
    return RTN_SUCCESS;                                               // write substate done, byte transfer succeeded
}

