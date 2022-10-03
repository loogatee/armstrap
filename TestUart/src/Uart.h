
#ifndef _UART_H_
#define _UART_H_


#define dev_USART1    1
#define dev_USART2    2
#define dev_USART6    6


#define SERIAL_CONSOLE     dev_USART1




#define SERO_TYPE_ONECHAR    1
#define SERO_TYPE_STR        2
#define SERO_TYPE_STRNOW     3
// --------------------------------TYPE_32 and below ALL have a data value associated with it
#define SERO_TYPE_32         4
#define SERO_TYPE_32N        5
#define SERO_TYPE_8          6
#define SERO_TYPE_8N         7




// Uart2 output
void U2_Init(void);
void U2_Process(void);

void U2_PrintCH(char ch);
void U2_PrintSTR(const char *pstr);
void U2_PrintSTRNow(const char *pstr);
void U2_Print32(const char *pstr, uint32_t val);
void U2_Print32N(const char *pstr, uint32_t val);
void U2_Print8(const char *pstr,  uint8_t val);
void U2_Print8N(const char *pstr,  uint8_t val);
void U2_Send(uint32_t otype, char *sptr, uint32_t *completionptr, uint32_t aval);

// Uart2 input
void U2Inp_Init(void);
void U2Inp_Process(void);
void U2Inp_SignalCmdDone(void);




#endif
