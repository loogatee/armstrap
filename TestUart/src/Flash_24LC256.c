#include "stm32f4xx.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "proj_common.h"
#include "Uart.h"
#include "Cmds.h"
#include "timer.h"
#include "i2c.h"
#include "Utils.h"
#include "Flash.h"



#define STATE_GET_1VAL     0
#define STATE_SHOW_1VAL    1
#define STATE_DO_WAIT      2

#define SMSTATE_WAIT_I2C     0
#define SMSTATE_WAIT_SERIAL  1


static u8   flash_rbuf[16];           // for returned data from the I2C driver
static u32  flash_i2c1_rcompl;        // location where I2C driver will use to signal completion, reads
//static u32  flash_i2c1_wcompl;        // completion pointer, writes
static u8   flash_showmem_state;      // waiting on Read Completion, or waiting on serial write completion



static I2CCMDS flash_registers[] =
{
    { 0xA0, 0x00, 0x00, 0x10, I2C_CMDTYPE_RW | I2C_CMDTYPE_CMDREG2 },        // Read 16 bytes of flash memory
    { 0xff, 0xff, 0xff, 0xff, 0xff },                                        // Terminate the List
};

//
// { SlaveAddr, CmdReg, CmdReg2, NumBytes, CmdType }
//
/*
static I2CCMDS flash_regs_w0[2] =
{
    { 0xA0, 0x00, 0x00, 0x08, I2C_CMDTYPE_WRITEONLY | I2C_CMDTYPE_CMDREG2 },        // Write flash memory 
    { 0xff, 0xff, 0xff, 0xff, 0xff },                                               // Terminate the List
}; */


//static u8  flash_wdata1[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

void Flash_Init( void )
{
    ;
}


//
void Flash_GetMem16( UW2B addr )
{
    flash_rbuf[0]                 = 0;
    flash_registers[0].ct_cmdreg  = addr.b[1];
    flash_registers[0].ct_cmdreg2 = addr.b[0];
    flash_showmem_state           = SMSTATE_WAIT_I2C;
    
    I2C_master_SendCmd( flash_rbuf, 0, &flash_i2c1_rcompl, flash_registers );
}


//
u8 Flash_ShowMem16( void )
{
    u8    i;
    UW2B  addr;
    u8    retv = flash_i2c1_rcompl;
    
    switch( flash_showmem_state )
    {
    case SMSTATE_WAIT_I2C:
        
        if( retv != I2C_COMPLETION_BUSY )
        {
            if( retv == I2C_COMPLETION_TIMEOUT )
            {
                U2_PrintSTR( "Timeout Flash!!" );
                U2_Print8  ( "    buf[0]: ", flash_rbuf[0] );
            }
            else
            {
                addr.b[0] = flash_registers[0].ct_cmdreg2;
                addr.b[1] = flash_registers[0].ct_cmdreg;

                U2_Print32N( "0x", (u32)addr.w );
                U2_Print8N( ": ", flash_rbuf[0] );

                for( i=1; i < 16; ++i )
                    U2_Print8N( " ", flash_rbuf[i] );
                
                U2_Send( SERO_TYPE_STR, (char *)"\r\n", &flash_i2c1_rcompl, 0 );
                retv = I2C_COMPLETION_BUSY;
                flash_showmem_state = SMSTATE_WAIT_SERIAL;
            }
        }
        break;
        
    case SMSTATE_WAIT_SERIAL:    break;
    }
    
    return retv;
}

/*
void Flash_Write8( UW2B addr, u8 *TA )
{
    flash_regs_w0[0].ct_cmdreg  = addr.b[1];
    flash_regs_w0[0].ct_cmdreg2 = addr.b[0];
    
    I2C_1master_SendCmd( 0, TA, &flash_i2c1_wcompl, flash_regs_w0);
}

u8 Flash_WriteComplete( void )
{
    u8  retv = flash_i2c1_wcompl;

    if( retv != I2C_COMPLETION_BUSY )
    {
        if( retv == I2C_COMPLETION_TIMEOUT )
        {
            SER_PrintSTR( "Timeout Flash!!" );
        }
        else
        {
            SER_PrintSTR( "Flash SetMem Done" );
        }
    }
    return retv;
}

void Flash_Erase8( UW2B addr )
{
    flash_regs_w0[0].ct_cmdreg  = addr.b[1];
    flash_regs_w0[0].ct_cmdreg2 = addr.b[0];
    
    I2C_1master_SendCmd( 0, flash_wdata1, &flash_i2c1_wcompl, flash_regs_w0);
}

u8 Flash_EraseComplete( void )
{
    u8  retv = flash_i2c1_wcompl;

    if( retv != I2C_COMPLETION_BUSY )
    {
        if( retv == I2C_COMPLETION_TIMEOUT )
        {
            SER_PrintSTR( "Timeout Flash!!" );
        }
        else
        {
            SER_PrintSTR( "Flash Erase Done" );
        }
    }
    return retv;
} */
