#ifndef _ddekit_condvar_h
#define _ddekit_condvar_h

/** \file ddekit/condvar.h */
#include <ddekit/ddekit.h>

#include <ddekit/lock.h>

struct ddekit_condvar;
typedef struct ddekit_condvar ddekit_condvar_t;

/* Initialize conditional variable. */
ddekit_condvar_t * ddekit_condvar_init(void);

/* Uninitialize conditional variable. */
void ddekit_condvar_deinit(ddekit_condvar_t *cvp);

/* Wait on a conditional variable. */
void ddekit_condvar_waiti(ddekit_condvar_t *cvp, ddekit_lock_t *mp);

/* Wait on a conditional variable at most until a timeout expires. (UNIMPL) */
int ddekit_condvar_wait_timed(ddekit_condvar_t *cvp, ddekit_lock_t *mp,
	int timo);

/* Send signal to the next one waiting for condvar. */
void ddekit_condvar_signal(ddekit_condvar_t *cvp);

/* Send signal to all threads waiting for condvar. */
void ddekit_condvar_broadcast(ddekit_condvar_t *cvp);

#endif
