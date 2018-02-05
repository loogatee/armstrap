
#include "stm32f4xx.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "proj_common.h"
#include "Uart.h"
#include "Cmds.h"


#define  TICK_INT_PRIORITY            ((uint32_t)0x0F)       /*!< tick interrupt priority */
#define  NVIC_PRIORITYGROUP_4         ((uint32_t)0x00000003) /*!< 4 bits for pre-emption priority
                                                                  0 bits for subpriority */

typedef struct
{
    volatile uint32_t   SysTicks;
} GLOBALS_t;



static GLOBALS_t   Globals;


static void     init_hw(void);
static uint32_t GetSysTick( void );
static uint32_t GetSysDelta( uint32_t OriginalTime );





int main(void)
{
    uint32_t  Ntime;

    Globals.SysTicks = 0;
    Ntime            = 0;

    U2_Init();
    U2Inp_Init();
    CMDS_Init();
    init_hw();

    CMDS_DisplayVersion();

    while(1)
    {
        U2_Process();
        U2Inp_Process();
        CMDS_Process();


        if( GetSysDelta(Ntime) >= 750 )            // number is in miilliseconds
        {
            GPIO_ToggleBits(GPIOC, GPIO_Pin_1);    //   Toggles the User Led per Delta interval
            Ntime = GetSysTick();                  //   re-init the counter
        }

    }

    return 0;                                      // if this executes, then Weird happened!
}

//
//   I'm keeping USART6 alive because on 1 of my boards,
//   the RX pin on the ftdi port doesn't work:
//        - The pin is PD6,USART2_RX
//
//   So in a pinch, (and for that board only), I will remap the console to U6.
//   Maybe a jumper, and run-time it!
//
static void init_gpios(void)
{
    GPIO_InitTypeDef UserLed_gpio;
    GPIO_InitTypeDef UsartX_gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,  ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,  ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART6, ENABLE);     // note USART6 on APB2


    GPIO_StructInit(&UserLed_gpio);
      UserLed_gpio.GPIO_Mode  = GPIO_Mode_OUT;
      UserLed_gpio.GPIO_Pin   = GPIO_Pin_1;
      UserLed_gpio.GPIO_PuPd  = GPIO_PuPd_UP;
      UserLed_gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &UserLed_gpio);

    GPIO_StructInit(&UsartX_gpio);
      UsartX_gpio.GPIO_Mode  = GPIO_Mode_AF;
      UsartX_gpio.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_6;            // Pin D5 (TX), Pin D6 (RX) = USART2
      UsartX_gpio.GPIO_PuPd  = GPIO_PuPd_UP;
      UsartX_gpio.GPIO_Speed = GPIO_Speed_50MHz;
      UsartX_gpio.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOD, &UsartX_gpio);

    GPIO_StructInit(&UsartX_gpio);
      UsartX_gpio.GPIO_Mode  = GPIO_Mode_AF;
      UsartX_gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;            // C6 tx, C7 rx = USART6
      UsartX_gpio.GPIO_PuPd  = GPIO_PuPd_UP;
      UsartX_gpio.GPIO_Speed = GPIO_Speed_50MHz;
      UsartX_gpio.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOC, &UsartX_gpio);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_USART2);      // The RX and TX pins are now connected to their AF
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART2);      //   so that the USART2 can take over control of the pins

    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_USART6);      // USART6 alternative function  6 is TX
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_USART6);      //   7 is RX
}


static void init_usart2()
{
    USART_InitTypeDef U2;
    USART_InitTypeDef U6;

    USART_StructInit( &U2 );
      U2.USART_BaudRate            = 9600;
      U2.USART_WordLength          = USART_WordLength_8b;
      U2.USART_StopBits            = USART_StopBits_1;
      U2.USART_Parity              = USART_Parity_No;
      U2.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
      U2.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init( USART2, &U2    );
    USART_Cmd ( USART2, ENABLE );

    USART_StructInit( &U6 );
      U6.USART_BaudRate            = 9600;
      U6.USART_WordLength          = USART_WordLength_8b;
      U6.USART_StopBits            = USART_StopBits_1;
      U6.USART_Parity              = USART_Parity_No;
      U6.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
      U6.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init( USART6, &U6    );
    USART_Cmd ( USART6, ENABLE );
}


static void init_hw()
{
    RCC_ClocksTypeDef  rclocks;
    uint32_t           prioritygroup = 0;

    SystemCoreClockUpdate();
    RCC_GetClocksFreq(&rclocks);

    FLASH->ACR |= FLASH_ACR_ICEN;      // Flash Instruction Cache Enable
    FLASH->ACR |= FLASH_ACR_DCEN;      // Flash Data Cache Enable
    FLASH->ACR |= FLASH_ACR_PRFTEN;    // Flash Pre-Fetch Buffer Enable

    NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

    SysTick_Config(rclocks.HCLK_Frequency / 1000);      // 168000000 / 1000 = 168000

    prioritygroup = NVIC_GetPriorityGrouping();

    NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(prioritygroup, TICK_INT_PRIORITY, 0));

    init_gpios();
    init_usart2();
}





static uint32_t GetSysTick( void )
{
    return Globals.SysTicks;
}

static uint32_t GetSysDelta( uint32_t OriginalTime )
{
    uint32_t v = Globals.SysTicks;

    if( v >= OriginalTime )
        return( v - OriginalTime );
    else
        return( ~OriginalTime + 1 + v );
}




void main_systick_handler(void)
{
    ++Globals.SysTicks;
}




