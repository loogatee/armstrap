/*
******************************************************************************
File:     main.c
Info:     Generated by Atollic TrueSTUDIO(R) 9.0.0   2018-01-23

The MIT License (MIT)
Copyright (c) 2018 STMicroelectronics

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

******************************************************************************
*/
#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"


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

    init_hw();

    while(1)
    {
    	if( GetSysDelta(Ntime) >= 1000 )
    	{
            GPIO_ToggleBits(GPIOC, GPIO_Pin_1);
            Ntime = GetSysTick();
    	}
    }

    return 0; // never reached
}



static void init_gpios(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    GPIO_StructInit(&gpio);

    gpio.GPIO_Mode  = GPIO_Mode_OUT;
    gpio.GPIO_Pin   = GPIO_Pin_1 | GPIO_Pin_6;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(GPIOC, &gpio);
}



static void init_hw()
{
    RCC_ClocksTypeDef  rclocks;
    uint32_t           prioritygroup = 0x00;

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




