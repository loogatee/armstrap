/*
 * SerialOutp.h
 *
 *  Created on: Jan 30, 2018
 *      Author: johnr
 */

#ifndef SERIALOUTP_H_
#define SERIALOUTP_H_


#define SERO_TYPE_ONECHAR    0x01
#define SERO_TYPE_STR        0x02
#define SERO_TYPE_32         0x03
#define SERO_TYPE_32N        0x04
#define SERO_TYPE_8          0x05
#define SERO_TYPE_8N         0x06





void U2_Init(void);
void U2_Process(void);


void U2_PrintCH(char ch);
void U2_PrintSTR(const char *pstr);
void U2_Print32(const char *pstr, uint32_t val);
void U2_Print32N(const char *pstr, uint32_t val);
void U2_Print8(const char *pstr,  uint8_t val);
void U2_Print8N(const char *pstr,  uint8_t val);
void U2_Send(uint32_t otype, char *sptr, uint32_t *completionptr, uint32_t aval);



#endif /* SERIALOUTP_H_ */
