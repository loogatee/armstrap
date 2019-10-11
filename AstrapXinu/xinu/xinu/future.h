/**
 *  * @file future.h
 *  *
**/
#ifndef _XINUFUTURE_H_
#define _XINUFUTURE_H_

#include <xinu/queue.h>

/* Future state definitions */
#define FFREE 0x01 /**< this future is free */
#define FUSED 0x02 /**< this future is used */
#define CONSUMER_WAITING 0x03 /**< consumer is waiting for this semaphore> **/
#define PRODUCED 0x04 /**< producer has produced the value> **/

/* type definition of "semaphore" */
typedef unsigned int future;

/**
 *  * Semaphore table entry
 *   */
struct futent                   /* semaphore table entry      */
{
    char state;                 /**< the state SFREE or SUSED */
    int* value;                  /**< value of this future */
    tid_typ tid;              /**< requires queue.h.        */
    int future_flags;
};

extern struct futent futtab[];

/* isbadsem - check validity of reqested future id and state */
#define isbadfuture(s) ((s >= NSEM) || (SFREE == futtab[s].state))

/* Future function prototypes */

future* future_alloc(int future_flags);
syscall future_free(future*);
syscall future_get(future*, int*);
syscall future_set(future*, int*);

#endif
