#ifndef _DDEKIT_LOCK_H
#define _DDEKIT_LOCK_H

#include <ddekit/ddekit.h>

struct ddekit_lock;
typedef struct ddekit_lock *ddekit_lock_t;

/* Initialize a DDEKit unlocked lock. */
#define ddekit_lock_init	ddekit_lock_init_unlocked

/* Initialize a DDEKit unlocked lock. */
void ddekit_lock_init_unlocked(ddekit_lock_t *mtx);

/* Initialize a DDEKit locked lock.  */
void ddekit_lock_init_locked(ddekit_lock_t *mtx);

/* Uninitialize a DDEKit lock. */
void ddekit_lock_deinit(ddekit_lock_t *mtx);

/* Acquire a lock. */
void ddekit_lock_lock(ddekit_lock_t *mtx);

/* Acquire a lock, non-blocking. */
int ddekit_lock_try_lock(ddekit_lock_t *mtx);

/* Unlock function. */
void ddekit_lock_unlock(ddekit_lock_t *mtx);

/* Get lock owner. */
int ddekit_lock_owner(ddekit_lock_t *mtx);

#endif
