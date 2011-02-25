#ifndef _DDEKIT_LOCK_H
#define _DDEKIT_LOCK_H

#include <ddekit/ddekit.h>

struct ddekit_lock;
typedef struct ddekit_lock *ddekit_lock_t;

/* Initialize a DDEKit unlocked lock. */
#define ddekit_lock_init	ddekit_lock_init_unlocked

/* Initialize a DDEKit unlocked lock. */
_PROTOTYPE( void ddekit_lock_init_unlocked, (ddekit_lock_t *mtx));

/* Initialize a DDEKit locked lock.  */
_PROTOTYPE( void ddekit_lock_init_locked, (ddekit_lock_t *mtx));

/* Uninitialize a DDEKit lock. */
_PROTOTYPE( void ddekit_lock_deinit, (ddekit_lock_t *mtx));

/* Acquire a lock. */
_PROTOTYPE( void ddekit_lock_lock, (ddekit_lock_t *mtx));

/* Acquire a lock, non-blocking. */
_PROTOTYPE( int ddekit_lock_try_lock, (ddekit_lock_t *mtx)); 

/* Unlock function. */
_PROTOTYPE( void ddekit_lock_unlock, (ddekit_lock_t *mtx));

/* Get lock owner. */
_PROTOTYPE( int ddekit_lock_owner, (ddekit_lock_t *mtx)); 

#endif
