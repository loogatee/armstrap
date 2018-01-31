
#include "stm32f4xx.h"
#include "SerialOutp.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


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

static char jbuf[50];



int main(void)
{
    uint32_t  Ntime,n1,i;

    Globals.SysTicks = 0;
    n1 = Ntime       = 0;
    i                = 0;

    U2_Init();    // data structures only.  No HW is init'ed here
    init_hw();

    while(1)
    {
    	U2_Process();

    	if( GetSysDelta(Ntime) >= 1000 )           // 1000ms is 1 second
    	{
            GPIO_ToggleBits(GPIOC, GPIO_Pin_1);    //   Toggles the User Led every 1 second
            Ntime = GetSysTick();                  //   re-init the counter
    	}

    	if( GetSysDelta(n1) >= 400 )               // 400 Milliseconds
    	{
    		i++;                                   //   simple counter increment
    		//U2_Print32("i = ", i);               //   usart2 test, print something out every 400ms

    		sprintf(jbuf,"hEllo %d\n\r",(int)i);
    		U2_PrintSTR( jbuf );
    	    n1 = GetSysTick();                     //   re-init counter
    	}
    }

    return 0;                                      // if this executes, then Weird happened!
}


static void init_gpios(void)
{
    GPIO_InitTypeDef UserLed_gpio;
    GPIO_InitTypeDef Usart2_gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,  ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,  ENABLE);


    GPIO_StructInit(&UserLed_gpio);
      UserLed_gpio.GPIO_Mode  = GPIO_Mode_OUT;
      UserLed_gpio.GPIO_Pin   = GPIO_Pin_1 | GPIO_Pin_6;
      UserLed_gpio.GPIO_PuPd  = GPIO_PuPd_UP;
      UserLed_gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &UserLed_gpio);

    GPIO_StructInit(&Usart2_gpio);
      Usart2_gpio.GPIO_Mode  = GPIO_Mode_AF;
      Usart2_gpio.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_6;            // Pin 5 (TX), Pin 6 (RX)
      Usart2_gpio.GPIO_PuPd  = GPIO_PuPd_UP;
      Usart2_gpio.GPIO_Speed = GPIO_Speed_50MHz;
      Usart2_gpio.GPIO_OType = GPIO_OType_PP;
    GPIO_Init(GPIOD, &Usart2_gpio);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF_USART2);      // The RX and TX pins are now connected to their AF
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource6, GPIO_AF_USART2);      //   so that the USART2 can take over control of the pins
}


static void init_usart2()
{
	USART_InitTypeDef U2;

	USART_StructInit( &U2 );
	  U2.USART_BaudRate            = 9600;
	  U2.USART_WordLength          = USART_WordLength_8b;
	  U2.USART_StopBits            = USART_StopBits_1;
	  U2.USART_Parity              = USART_Parity_No;
	  U2.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	  U2.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init( USART2, &U2 );

	USART_Cmd( USART2, ENABLE );
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
    //GPIO_ToggleBits(GPIOC, GPIO_Pin_6);
    ++Globals.SysTicks;
}




