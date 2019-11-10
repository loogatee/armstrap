#ifndef _UTILS_H_
#define _UTILS_H_


void     BtoH( u8 val, char *S );
void     BtoHnz( u8 val, char *S );
void     ItoH( u32 val, char *S );
uint32_t HtoI( const char *ptr );
int      AtoI( const char *p );
uint16_t HtoU16( char *pstr );


#endif
