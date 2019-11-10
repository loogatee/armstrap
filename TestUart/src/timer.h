#ifndef TIMERz_H
#define TIMERz_H







uint32_t  GetSysTick( void );
uint32_t  GetSysDelta( uint32_t OriginalTime );
void      Increment_SysTicks( void );
void      init_timer( void );



#endif

