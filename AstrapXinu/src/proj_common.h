/*
 * proj_common.h
 *
 *  Created on: Jan 31, 2018
 *      Author: johnr
 */

#ifndef PROJ_COMMON_H_
#define PROJ_COMMON_H_



#define VERSION_STR     "AstrapXinu Version 1_"
#define VERSION_MAJOR   1
#define VERSION_MINOR   1
#define VERSION_DATE    "02/08/2018 13:30\r\n"





#define true   1
#define True   1
#define TRUE   1
#define false  0
#define False  0
#define FALSE  0



#define ASCII_BACKSPACE         8
#define ASCII_LINEFEED          10
#define ASCII_CARRIAGERETURN    13
#define ASCII_SPACE             32
#define ASCII_TILDE             126
#define ASCII_DELETE            127


#define FALL_THRU




typedef char     Bool;
typedef char     bool;
typedef char     BOOL;





void     BtoH( u8  val, char *S  );
void     ItoH( u32 val, char *S  );
uint32_t HtoI( const char *ptr   );
int      AtoI( const char *p     );
uint16_t HtoU16( char *pstr      );
void     ItoH16( u16 val,char *S );


#endif /* PROJ_COMMON_H_ */


















