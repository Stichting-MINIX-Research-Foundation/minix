#include "common.h"

#include <ddekit/condvar.h> 
#include <ddekit/lock.h> 
#include <ddekit/memory.h> 

#ifdef DDEBUG_LEVEL_CONDVAR
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_CONDVAR
#endif

#include "debug.h"
#include "util.h"
#include "thread.h"

struct ddekit_condvar {
	ddekit_thread_t * wait_queue;
};

/*****************************************************************************/
/*      ddekit_condvar_init                                                  */
/*****************************************************************************/
ddekit_condvar_t * ddekit_condvar_init(void) { 
	ddekit_condvar_t *cv;
	cv = (ddekit_condvar_t *) ddekit_simple_malloc(sizeof(ddekit_condvar_t));
	DDEBUG_MSG_VERBOSE("cv: %p", cv);
	return cv;
}

/*****************************************************************************/
/*      ddekit_condvar_deinit                                                */
/*****************************************************************************/
void ddekit_condvar_deinit(ddekit_condvar_t *cvp) {
	DDEBUG_MSG_VERBOSE("cv: %p", cvp);
	ddekit_simple_free(cvp); 
}

/*****************************************************************************/
/*      ddekit_condvar_wait                                                  */
/*****************************************************************************/
void ddekit_condvar_wait(ddekit_condvar_t *cv, ddekit_lock_t *mp) {
	
	DDEBUG_MSG_VERBOSE("wait cv: %p, thread id: %d, name: %s",
		cv, ddekit_thread_myself()->id,  ddekit_thread_myself()->name);

	ddekit_lock_unlock(mp);
	
	if(cv->wait_queue == NULL) {
			cv->wait_queue = ddekit_thread_myself();
	} else {
		ddekit_thread_t *pos = cv->wait_queue;
		while(pos->next != NULL) {
			pos = pos->next;
		}
		pos->next = ddekit_thread_myself();
	}

	_ddekit_thread_schedule();

	DDEBUG_MSG_VERBOSE("wakeup cv: %p, thread id: %d, name: %s",
		cv, ddekit_thread_myself()->id,  ddekit_thread_myself()->name);

	ddekit_lock_lock(mp);
}
/*****************************************************************************/
/*      ddekit_condvar_wait_timed                                            */
/*****************************************************************************/
int ddekit_condvar_wait_timed
(ddekit_condvar_t *cvp, ddekit_lock_t *mp, int timo)
{
	/* 
	 * Only used by ddefbsd, so not implemented
	 */
	WARN_UNIMPL;
	return 0;
}


/*****************************************************************************/
/*      ddekit_condvar_signal                                                */
/*****************************************************************************/
void ddekit_condvar_signal(ddekit_condvar_t *cv) 
{
	
	DDEBUG_MSG_VERBOSE("cv: %p", cv);
	
	if(cv->wait_queue) {
		ddekit_thread_t *th = cv->wait_queue;
		cv->wait_queue = th->next;
		th->next = NULL;
		_ddekit_thread_enqueue(th);

		DDEBUG_MSG_VERBOSE("waking up cv: %p, thread id: %d, name: %s",
			cv, th->id, th->name);
	}
	ddekit_thread_schedule();
}


/*****************************************************************************/
/*      ddekit_condvar_broadcast                                             */
/*****************************************************************************/
void ddekit_condvar_broadcast(ddekit_condvar_t *cv) { 
	
	DDEBUG_MSG_VERBOSE("cv: %p", cv);
	
	while (cv->wait_queue) {
		ddekit_thread_t *th = cv->wait_queue;
		cv->wait_queue = th->next;
		th->next = NULL;
		_ddekit_thread_enqueue(th);
		
		DDEBUG_MSG_VERBOSE("waking up cv: %p, thread id: %d, name: %s",
			cv, th->id, th->name);

	}
	ddekit_thread_schedule();
}

