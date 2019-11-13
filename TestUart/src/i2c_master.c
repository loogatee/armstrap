#include "stm32f4xx.h"
#include "proj_common.h"
#include "Uart.h"
#include "Cmds.h"
#include "string.h"
//#include "gpio_CDC3.h"
#include "i2c.h"
#include "timer.h"
#include "Utils.h"



#define I2C_1_QENTRYS   5
#define TIMEOUT_VAL     120


#define RTN_CONTINUE    0
#define RTN_RESTART     1
#define RTN_SUCCESS     2


#define I2C_STATE_GETJOB                 0
#define I2C_STATE_TRY_RESTART            1
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
#define I2C_STATE_WAIT_STOP              12
#define I2C_STATE_DO_WRITEONLY           13
#define I2C_STATE_FINISH_CLEANUP         14

#define I2C_WRITE_SUBSTATE_START         0
#define I2C_WRITE_SUBSTATE_WAITCOMPLETE  1
#define I2C_WRITE_SUBSTATE_IDLE_ACK      2
#define I2C_WRITE_SUBSTATE_SUCCESS       3
#define I2C_WRITE_SUBSTATE_NEED_RESTART  4

#define I2C_READ_SUBSTATE_START          0
#define I2C_READ_SUBSTATE_BF             1
#define I2C_READ_SUBSTATE_WAIT_RCEN      2
#define I2C_READ_SUBSTATE_DO_RESTART     3
#define I2C_READ_SUBSTATE_ACKEN          4
#define I2C_READ_SUBSTATE_SUCCESS        5


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

static u8       i2c_1_write_substate;
static u8       i2c_1_write_data;
static u8       i2c_1_read_substate;
static u8       i2c_1_cleanup_count;


static u8  i2c_1_do_write_substate( unsigned int   wait_type );
//static u8  i2c_1_do_read_substate ( void );



u32 count1;
u32 count2;
u32 count3;
u32 count4;

extern void init_gpioI2C( void );


void I2C_master_Init( void )
{
	count1 = 0;
	count2 = 0;
	count3 = 0;
	count4 = 0;
    memset( (void *)i2c_1_data, 0, sizeof(i2c_1_data) );

    i2c_1_num_qitems = 0;
    i2c_1_inn_qindex = 0;
    i2c_1_out_qindex = 0;
      
    i2c_1_state = I2C_STATE_GETJOB;

    i2c_1_cleanup_count = 0;
}



bool I2C_master_SendCmd( u8 *dptr, u8 *wptr, u32 *complptr, I2CCMDS *listptr )
{
    II2C  *lqitem;
    bool   retv = FALSE;

    if( i2c_1_num_qitems != I2C_1_QENTRYS )
    {
        retv   = TRUE;
        lqitem = &i2c_1_data[i2c_1_inn_qindex];
        ++i2c_1_num_qitems;
        if( ++i2c_1_inn_qindex == I2C_1_QENTRYS ) { i2c_1_inn_qindex = 0; }

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

	++i2c_1_cleanup_count;

	if( i2c_1_cleanup_count == 1 )
	{
		U2_Print8( "cleanup, i2c_1_state: ", i2c_1_state );
	    U2_Print32("clnup SR1: ",I2C1->SR1);
	    U2_Print32("clnup SR2: ",I2C1->SR2);
	}

   	I2C1->CR1 |= I2C_CR1_SWRST;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1,   DISABLE);
    GPIOB->AFR[0] = 0;

    GPIO_StructInit(&Xgpio);
    	 Xgpio.GPIO_Mode  = GPIO_Mode_OUT;
    	 Xgpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    	 Xgpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    	 Xgpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &Xgpio);

    GPIO_SetBits(GPIOB, GPIO_Pin_6|GPIO_Pin_7);

	i2c_1_state = I2C_STATE_FINISH_CLEANUP;                               //     state machine transitions to:  Looking For New Job
}





//
void I2C_master_Process( void )
{
    u8  tmpb;
    u16 S1,S2;


    if( i2c_1_state != I2C_STATE_GETJOB && i2c_1_state != I2C_STATE_FINISH_CLEANUP )            // If the state machine is active
    {
        if( GetSysDelta( i2c_1_starttime ) >= TIMEOUT_VAL )                       // Safety valve: too long to complete this job ?
        {
        	U2_Print8( "TO! i2c_1_state: ", i2c_1_state );                        //     Message out to debug port (optional)
        	--i2c_1_num_qitems;                                                   //     Item is no longer on the Q, total number decreases by 1
        	if( ++i2c_1_out_qindex == I2C_1_QENTRYS ) { i2c_1_out_qindex = 0; }   //     Where next item will come from.  adjusts to 0 if wrapped

        	if( i2c_1_activeitem->ic_complptr != 0 )                              //     Is there a valid Completion Pointer ?
        	    *i2c_1_activeitem->ic_complptr = I2C_COMPLETION_TIMEOUT;          //         signal a non-zero to that address to indicate completion
        	cleanup();

            return;                                                               //     return immediately
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
        i2c_1_read_substate  = I2C_READ_SUBSTATE_START;                           // Make sure read_substate is initialized for each job
        FALL_THRU;                                                                // Keep on going, no need to break

    case I2C_STATE_TRY_RESTART:

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
        
        i2c_1_lcdptr     = i2c_1_activeitem->ic_dptr;                             // Local Copy, Data Pointer
        i2c_1_lcwdata    = i2c_1_activeitem->ic_wdata;                            // Local Copy, Pointer to Write Data
        i2c_1_state      = I2C_STATE_SENDSTART;                                   // transition to SENDSTART
        FALL_THRU;                                                                // no need to break

    case I2C_STATE_SENDSTART:

    	if( STM_REGISTER I2C1->SR2 & I2C_SR2_BUSY ) {return;}                                  // Must wait for IDLE

        i2c_1_state = I2C_STATE_START_COMPLETE;                                   // Have IDLE, new state will be START_COMPLETE
        STM_REGISTER I2C1->CR1 |= I2C_CR1_START;                                  // START!
        count1++;
        FALL_THRU;                                                                // no need to break

    case I2C_STATE_START_COMPLETE:

    	S1 = STM_REGISTER I2C1->SR1;
    	S2 = STM_REGISTER I2C1->SR2;
    	if( !(S1 & I2C_SR1_SB) ||  !(S2 & I2C_SR2_MSL) || !(S2 & I2C_SR2_BUSY) ) { return; }

        i2c_1_state      = I2C_STATE_WRITE_SLAVEADDR;                             // Got it!  Transition to WRITE_SLAVEADDR
        i2c_1_write_data = i2c_1_lcslave;                                         // This is the byte which will be written

        count2++
        FALL_THRU;                                                                // no need to break

        U2_Print32("sc SR1: ",I2C1->SR1);
        U2_Print32("sc SR2: ",I2C1->SR2);

    case I2C_STATE_WRITE_SLAVEADDR:
            
        if( (tmpb = i2c_1_do_write_substate(1)) == RTN_CONTINUE ) { return; }      // Keep calling write_substate while return value says to CONTINUE
            
        if( tmpb == RTN_RESTART )                                                 // Test for the RESTART return value
        {                                                                         //    RESTART indicates some error was detected
            i2c_1_state = I2C_STATE_TRY_RESTART;                                  //    Change state machine to TRY_RESTART.  Means 'try again'
            return;                                                               //    Immediately jump out
        }
                                                                                  // ELSE return indicates SUCCESS!
        i2c_1_state      = I2C_STATE_WRITE_CMDREG;                                //    new state is WRITE_CMDREG
        i2c_1_write_data = i2c_1_lccmd;                                           //    this is the byte which will be written
        FALL_THRU;                                                                //    no need to break

    case I2C_STATE_WRITE_CMDREG:

        if( (tmpb = i2c_1_do_write_substate(2)) == RTN_CONTINUE ) { return; }      // Keep calling write_substate while return value says to CONTINUE

        if( tmpb == RTN_RESTART )                                                 // Test for the RESTART return value
        {                                                                         //    RESTART indicates some error was detected
            i2c_1_state = I2C_STATE_TRY_RESTART;                                  //    Change state machine to TRY_RESTART.  Means 'try again'
            return;                                                               //    Immediately jump out
        }

        if( i2c_1_useCmdReg2 == FALSE )
        {
            if( i2c_1_lccmdtype == I2C_CMDTYPE_WRITEONLY )                            // Check to see if this job is WRITE-ONLY
            {                                                                         //    Means nothing to read, need only to write some registers
                i2c_1_state          = I2C_STATE_DO_WRITEONLY;                        //    new state is DO_WRITEONLY
                i2c_1_write_data     = *i2c_1_lcwdata ++;                             //    Get the byte to write, and increment pointer thru this list
                return;                                                               //    return immediately
            }
            i2c_1_state = I2C_STATE_WRITE_WAITBTF;                                    // Wait for BTF bit to be set
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
            if( (tmpb = i2c_1_do_write_substate(2)) == RTN_CONTINUE ) { return; }  // Keep calling write_substate while return value says to CONTINUE

            if( tmpb == RTN_RESTART )                                             // Test for the RESTART return value
            {                                                                     //    RESTART indicates some error was detected
                i2c_1_state = I2C_STATE_TRY_RESTART;                              //    Change state machine to TRY_RESTART.  Means 'try again'
                return;                                                           //    Immediately jump out
            }
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

        i2c_1_state      = I2C_STATE_WRITE_SLAVEADDR_RW;                          // Got RSEN=0.  Awesome!  Transition the state
        i2c_1_write_data = i2c_1_lcslave | 1;                                     // IMPORTANT!!  The 'OR 1' turns the bus around, so Read is Enabled
        FALL_THRU;                                                                // no need to break

    case I2C_STATE_WRITE_SLAVEADDR_RW:

        if( (tmpb = i2c_1_do_write_substate(3)) == RTN_CONTINUE ) { return; }      // Keep calling write_substate while return value says to CONTINUE

        if( tmpb == RTN_RESTART )                                                 // Test for the RESTART return value
        {                                                                         //    RESTART indicates some error was detected
            i2c_1_state = I2C_STATE_TRY_RESTART;                                  //    Change state machine to TRY_RESTART.  Means 'try again'
            return;                                                               //    Immediately jump out
        }
                                                                                  // ELSE return indicates SUCCESS
        i2c_1_state = I2C_STATE_READ_THE_DATA;                                    // new state = READ_THE_DATA
        FALL_THRU;                                                                // Fall through, start the read immediately

        {
        	--i2c_1_num_qitems;                                                   //     Item is no longer on the Q, total number decreases by 1
        	if( ++i2c_1_out_qindex == I2C_1_QENTRYS ) { i2c_1_out_qindex = 0; }   //     Where next item will come from.  adjusts to 0 if wrapped

        	if( i2c_1_activeitem->ic_complptr != 0 )                              //     Is there a valid Completion Pointer ?
        	    *i2c_1_activeitem->ic_complptr = I2C_COMPLETION_TIMEOUT;          //         signal a non-zero to that address to indicate completion
        	cleanup();
        }


#ifdef NOTYET


    case I2C_STATE_READ_THE_DATA:

        if( (tmpb = i2c_1_do_read_substate()) == RTN_CONTINUE ) { return; }       // Keep calling read_substate while return value says to CONTINUE

        if( tmpb == RTN_RESTART )                                                 // Test for the RESTART return value
        {                                                                         //    RESTART indicates some error was detected
            i2c_1_state = I2C_STATE_TRY_RESTART;                                  //    Change state machine to TRY_RESTART.  Means 'try again'
            return;                                                               //    Immediately jump out
        }

        PIC_REGISTER SSP1CON2bits.PEN = 1;                                        // Got it!  Can now Stop I2C transaction
        i2c_1_state = I2C_STATE_WAIT_STOP;                                        // transition state to looking for PEN to transition
        FALL_THRU;                                                                // Fall thru and immediately check

    case I2C_STATE_WAIT_STOP:

        if( PIC_REGISTER SSP1CON2bits.PEN != 0) { return; }                       // Spin here until the transition on PEN occurs

        tmpb = i2c_1_lclistptr->ct_numbytes;                                      // save numbytes before incrementing to new command field
        i2c_1_lclistptr++;                                                        // will point to the new Command Field
        if( i2c_1_lclistptr->ct_slaveaddr != 0xff )                               // New command list is Valid if Non FF
        {
            if( i2c_1_lccmdtype == I2C_CMDTYPE_RW )                               // Valid!  is this a 'RW' command ?
                i2c_1_activeitem->ic_dptr += tmpb;                                //     YES:  increment dptr, where read data goes
            else                                                                  // Else it's a WRITEONLY Job
                i2c_1_activeitem->ic_wdata += tmpb;                               //     increment where write data is coming from

            i2c_1_starttime = GetSysTick();                                     // Start timer for the new transaction
            i2c_1_state     = I2C_STATE_TRY_RESTART;                              // Let's hit it again!
            return;                                                               // Hop out of state machine, back in a flash
        }

        i2c_1_state = I2C_STATE_GETJOB;                                           // All Done, new state is: Look for Another Job on the list
        --i2c_1_num_qitems;                                                       // Active Jobs on the Queue is reduced by 1
        if( ++i2c_1_out_qindex == I2C_1_QENTRYS ) { i2c_1_out_qindex = 0; }       // Where next Active item will come from.  adjusts to 0 if wrapped
        
        if( i2c_1_activeitem->ic_complptr != 0 )                                  // Is there a valid Completion Pointer ?
            *i2c_1_activeitem->ic_complptr = I2C_COMPLETION_OK;                   //    signal a non-zero to that address to indicate completion
        return;                                                                   // Get out of the i2c thread now

    case I2C_STATE_DO_WRITEONLY:

        if( (tmpb = i2c_1_do_write_substate(2)) == RTN_CONTINUE ) { return; }      // Keep calling write_substate while return value says to CONTINUE

        if( tmpb == RTN_RESTART )                                                 // Test for the RESTART return value
        {                                                                         //    RESTART indicates some error was detected
            i2c_1_state = I2C_STATE_TRY_RESTART;                                  //    Change state machine to TRY_RESTART.  Means 'try again'
            return;                                                               //    Immediately jump out
        }

        if( --i2c_1_lcnumbytes == 0 )                                             // Have all the bytes been sent ?
        {
            PIC_REGISTER SSP1CON2bits.PEN = 1;                                    //     Yes!, Send STOP
            i2c_1_state = I2C_STATE_WAIT_STOP;                                    //     Transition state to WAIT_STOP
            return;                                                               //     hop out, returns to WAIT_STOP quickly
        }

        i2c_1_write_data = *i2c_1_lcwdata++;                                      // Assign the new byte to write, and increment through the list
        i2c_1_do_write_substate(2);                                                // kick off write state machine.  No worries about return val


#endif
    }


}




static u8 i2c_1_do_write_substate( unsigned int  WaitType )
{
    volatile u8 dummy;
    u16 S1;
    u16 S2;

    switch( i2c_1_write_substate )
    {
    case I2C_WRITE_SUBSTATE_START:

    	count3++;
        STM_REGISTER I2C1->DR = i2c_1_write_data;                     // transmits 1 byte
        i2c_1_write_substate = I2C_WRITE_SUBSTATE_WAITCOMPLETE;       // Success on write, transition to next state
        FALL_THRU;

    //
    // #define  I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED        ((uint32_t)0x00070082)  /* BUSY, MSL, ADDR, TXE and TRA flags */
    //   SR1:
    //         Addr  bit1
    //         TxE   bit7
    //   SR2:
    //         MSL   bit0
    //         BUSY  bit1
    //         TRA   bit2
    //
    case I2C_WRITE_SUBSTATE_WAITCOMPLETE:

    	if( WaitType == 1 )
    	{
    		S1 = STM_REGISTER I2C1->SR1;
    		S2 = STM_REGISTER I2C1->SR2;

    		if( !(S1 & I2C_SR1_ADDR) || !(S1 & I2C_SR1_TXE) || !(S2 & I2C_SR2_MSL) || !(S2 & I2C_SR2_BUSY) || !(S2 & I2C_SR2_TRA) )
    		{
                return RTN_CONTINUE;                                      //  stay here, keep looking for BF to be 0
            }

    		U2_Print32("wt1 SR1: ",I2C1->SR1);
    		U2_Print32("wt1 SR2: ",I2C1->SR2);
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



        count4++;
    	dummy = STM_REGISTER I2C1->SR2;
    	hammer(dummy);
        i2c_1_write_substate = I2C_WRITE_SUBSTATE_IDLE_ACK;           //  BF cleared!  Now look for IDLE ACK
        FALL_THRU;

    case I2C_WRITE_SUBSTATE_IDLE_ACK:

    	//if( I2C1->SR2 & I2C_SR2_BUSY )
    	//	return RTN_CONTINUE;

        //if( PIC_REGISTER SSP1CON2bits.ACKSTAT != 0 )                  // 1 = Acknowledge was not received
        //{
        //    PIC_REGISTER SSP1CON2bits.PEN = 1;                        //      Send STOP!
        //    i2c_1_write_substate = I2C_WRITE_SUBSTATE_START;          //      Will re-send the byte
        //    return RTN_RESTART;                                       //      transition states for attempt to resend
        //}

        i2c_1_write_substate = I2C_WRITE_SUBSTATE_START;              // ACK good!  write_substate goes back to START
        break;                                                        // get out
    }
    
    return RTN_SUCCESS;                                               // write substate done, byte transfer succeeded
}

#ifdef NOTYET


static bool i2c_1_bus_collision( void )
{
    if( PIC_REGISTER PIR2bits.BCL1IF != 0 )                           // perform check for Bus Collision
    {
       PIC_REGISTER PIR2bits.BCL1IF  = 0;                             //    Got one, clear the Indicator
       PIC_REGISTER SSP1CON2bits.PEN = 1;                             //    I2C Stop
       i2c_1_read_substate = I2C_READ_SUBSTATE_START;                 //    State machine will start from the beginning
       return TRUE;                                                   //    TRUE says:  "yes we have a bus collision"
    }
    return FALSE;                                                     // "no, there was not a bus collision"
}


//
//   64Mhz Oscillator, 400kHz I2C:  1 character read takes about 34.2us.   PIN toggle whenever RCEN=1, on scope
//
static u8 i2c_1_do_read_substate( void )
{
    switch( i2c_1_read_substate )
    {
    case I2C_READ_SUBSTATE_START:

        PIC_REGISTER SSP1CON2bits.RCEN = 1;                                   // Enables Receive Mode for I2C
        i2c_1_read_substate = I2C_READ_SUBSTATE_BF;                           // Transition the state machine
        FALL_THRU;                                                            // no need to break

    case I2C_READ_SUBSTATE_BF:

        if( PIC_REGISTER SSP1STATbits.BF == 0 )                               // will stay in this state until BF transitions to 1
        {            
            if( i2c_1_bus_collision() == TRUE )                               // Check for Bus Collision 
                return RTN_RESTART;                                           //     Got one, transition to RESTART
            else
                return RTN_CONTINUE;                                          //     No BF and No BCL, stay in state SUBSTATE_BF
        }

        i2c_1_read_substate = I2C_READ_SUBSTATE_WAIT_RCEN;                    // Got BF!  next state is WAIT_RCEN
       *i2c_1_lcdptr++      = PIC_REGISTER SSP1BUF;                           // Read the byte from SSP1BUF, store in the data pointer
        i2c_1_lcnumbytes--;                                                   // Decrements Number of Bytes received
        FALL_THRU;                                                            // OK to Fall through

    case I2C_READ_SUBSTATE_WAIT_RCEN:

        if( PIC_REGISTER SSP1CON2bits.RCEN != 0 ) { return RTN_CONTINUE; }    // spin in this state until RCEN=0: indicates Receive Idle

        if( i2c_1_bus_collision() == TRUE ) { return RTN_RESTART; }           // If Bus Collision here, transition to RESTART
        
        if( i2c_1_lcnumbytes == 0 )                                           // When num bytes is 0, the Job is finished
        {
            PIC_REGISTER SSP1CON2bits.ACKDT = 1;                              //    Done: Send Not Acknowledge

            if( i2c_1_lcslave == 0xA0 )
            {
                i2c_1_read_substate = I2C_READ_SUBSTATE_START;                //    We are:  state goes back to START
                return RTN_SUCCESS;                                           //    Indicates Read State Machine is complete
            }
        }
        else
        {
            PIC_REGISTER SSP1CON2bits.ACKDT = 0;                              //    Not done yet:  Send Acknowledge
        }
        
        PIC_REGISTER SSP1CON2bits.ACKEN = 1;                                  // Initiate Acknowledge sequence and xmit ACKDT bit
        i2c_1_read_substate = I2C_READ_SUBSTATE_ACKEN;                        // state changes.
        FALL_THRU;                                                            // OK to fall through

    case I2C_READ_SUBSTATE_ACKEN:

        if( PIC_REGISTER SSP1CON2bits.ACKEN != 0 ) { return RTN_CONTINUE; }   // Spin in this state until ACKEN=0, meaning ACK sequence idle 

        if( i2c_1_lcnumbytes == 0 )                                           // Check to see if we're done
        {
            i2c_1_read_substate = I2C_READ_SUBSTATE_START;                    //    We are:  state goes back to START
            return RTN_SUCCESS;                                               //    Indicates Read State Machine is complete
        }
                                                                              // ELSE there are still characters to read
        PIC_REGISTER SSP1CON2bits.RCEN = 1;                                   //    Initiate by poking RCEN
        i2c_1_read_substate            = I2C_READ_SUBSTATE_BF;                //    next state will be looking for transition on BF
        FALL_THRU;                                                            //    falls out of the switch (mainly to avoid compiler warning)
    }

    return RTN_CONTINUE;                                                      // only time switch statement is broken, send CONTINUE
}


#endif




