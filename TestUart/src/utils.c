/*
 * utils.c
 *
 *  Created on: Jan 31, 2018
 *      Author: johnr
 */

#include "stm32f4xx.h"




static char hexv[] =  { '0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9' ,'A', 'B', 'C', 'D', 'E', 'F' };



void BtoH( u8 val, char *S )
{
    s8 k;

    for( k=1; k >= 0; --k )
    {
        S[k]   = hexv[val & 0xF];
        val  >>= 4;
    }

    S[2] = 0;
}


void ItoH( u32 val, char *S )
{
    s8  k;

    for( k=7; k >= 0; --k )
    {
        S[k]   = hexv[val & 0xF];
        val  >>= 4;
    }

    S[8] = 0;
}




uint32_t HtoI( const char *ptr )
{
    uint32_t  value = 0;
    char      ch      = *ptr;

    while (ch == ' ' || ch == '\t')
        ch = *(++ptr);

    for (;;) {

        if (ch >= '0' && ch <= '9')
            value = (value << 4) + (ch - '0');
        else if (ch >= 'A' && ch <= 'F')
            value = (value << 4) + (ch - 'A' + 10);
        else if (ch >= 'a' && ch <= 'f')
            value = (value << 4) + (ch - 'a' + 10);
        else
            return value;
        ch = *(++ptr);
    }
}


int AtoI( const char *p )
{
    int n,f;

    n=f=0;

    for( ; ; p++ )
    {
        switch( *p )
        {
            case ' ':
            case '\t':    continue;
            case '-':     f++;
            case '+':     p++;
        }
        break;
    }

    while( *p >= '0' && *p <= '9')
        n = n*10 + *p++ - '0';

    return(f ? -n : n);
}
