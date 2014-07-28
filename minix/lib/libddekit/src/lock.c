#include "common.h"

#include <ddekit/assert.h>
#include <ddekit/memory.h>
#include <ddekit/semaphore.h>

#ifdef DDEBUG_LEVEL_LOCK
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_LOCK
#endif

#include "debug.h"
#include "thread.h"

struct ddekit_lock {
	ddekit_thread_t *owner;
	ddekit_thread_t *wait_queue;
};


/******************************************************************************
 *       ddekit_lock_init_locked                                              *
 *****************************************************************************/
void ddekit_lock_init_locked(ddekit_lock_t *mtx) 
{  
	(*mtx) = (struct ddekit_lock *)
		ddekit_simple_malloc(sizeof(struct ddekit_lock));  

	(*mtx)->wait_queue = NULL;
	(*mtx)->owner      = ddekit_thread_myself(); 
}

/******************************************************************************
 *       ddekit_lock_init_unlocked                                            *
 *****************************************************************************/
void ddekit_lock_init_unlocked(ddekit_lock_t *mtx) 
{ 
	(*mtx) = (struct ddekit_lock *) 
		ddekit_simple_malloc(sizeof(struct ddekit_lock));  
	(*mtx)->owner      = NULL; 
	(*mtx)->wait_queue = NULL;
}

/******************************************************************************
 *       ddekit_lock_deinit                                                   *
 *****************************************************************************/
void ddekit_lock_deinit  (ddekit_lock_t *mtx)
{ 
	ddekit_simple_free(*mtx);
}

/******************************************************************************
 *       ddekit_lock_lock                                                     *
 *****************************************************************************/
void ddekit_lock_lock (ddekit_lock_t *mtx) 
{
	if ((*mtx)->owner == NULL) {
		(*mtx)->owner = ddekit_thread_myself();  
	} else {

		if ((*mtx)->wait_queue == NULL) {
			(*mtx)->wait_queue = ddekit_thread_myself();
		} else {	
			ddekit_thread_t *pos = (*mtx)->wait_queue;
			while(pos->next != NULL) {
				pos = pos->next;
			}
			pos->next = ddekit_thread_myself();
		}

		_ddekit_thread_schedule();

		if ((*mtx)->owner != NULL) {
			_ddekit_print_backtrace((*mtx)->owner);
			_ddekit_print_backtrace(ddekit_thread_myself());
			ddekit_panic("owner!=NULL: %s (I am %s)\n",
			    (*mtx)->owner->name, ddekit_thread_myself()->name);
		}

		(*mtx)->owner =  ddekit_thread_myself();
	}
}
 
/******************************************************************************
 *       ddekit_lock_try_lock                                                 *
 *****************************************************************************/
int ddekit_lock_try_lock(ddekit_lock_t *mtx) 
{
	if ((*mtx)->owner == NULL) {
		(*mtx)->owner =  ddekit_thread_myself();
		return 0;
	} else {
		return -1;
	}
}

/******************************************************************************
 *       ddekit_lock_unlock                                                   *
 *****************************************************************************/
void ddekit_lock_unlock  (ddekit_lock_t *mtx) {
	ddekit_assert((*mtx)->owner != NULL);
	(*mtx)->owner = NULL;
	if((*mtx)->wait_queue) {
		ddekit_thread_t *waiter = (*mtx)->wait_queue;
		(*mtx)->wait_queue = waiter->next;
		waiter->next= NULL;
		_ddekit_thread_enqueue(waiter);
		ddekit_yield();
	}
}  

/******************************************************************************
 *       ddekit_lock_owner                                                    *
 *****************************************************************************/
int ddekit_lock_owner(ddekit_lock_t *mtx) { 
	return ddekit_thread_get_id((*mtx)->owner);
}

