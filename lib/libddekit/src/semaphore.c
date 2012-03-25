#include "common.h"
#include <ddekit/condvar.h>
#include <ddekit/lock.h>
#include <ddekit/memory.h>
#include <ddekit/panic.h>
#include <ddekit/semaphore.h>

#ifdef DDEBUG_LEVEL_SEMAPHORE
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_SEMAPHORE
#endif

#include "debug.h"
#include "thread.h"

struct ddekit_sem {
	unsigned count;
	ddekit_thread_t *wait_queue;
};

#define SEM_DEBUG(p)                                                  \
    do {                                                              \
		DDEBUG_MSG_VERBOSE("%s, %p, %d\n",__func__, sem, sem->count); \
	} while(0)                                                        

/*****************************************************************************
 *     ddekit_sem_init                                                       *
 *************************+**************************************************/
ddekit_sem_t *ddekit_sem_init(int value)
{  
	ddekit_sem_t *sem;
	
	sem = (ddekit_sem_t *) ddekit_simple_malloc(sizeof(ddekit_sem_t));
	
	sem->count = value;
	sem->wait_queue = NULL;
	
	SEM_DEBUG(p);
	return sem; 
}

/*****************************************************************************
 *     ddekit_sem_deinit                                                     *
 ****************************************************************************/
void ddekit_sem_deinit(ddekit_sem_t *sem)
{
	SEM_DEBUG(p);
	ddekit_simple_free(sem);	
}

/*****************************************************************************
 *     ddekit_sem_down                                                       *
 ****************************************************************************/
void ddekit_sem_down(ddekit_sem_t *sem)
{
	SEM_DEBUG(p);
	if(sem->count == 0) {
		if(sem->wait_queue == NULL) {
			sem->wait_queue = ddekit_thread_myself();
		} else {
			ddekit_thread_t *pos = sem->wait_queue;
			while(pos->next != NULL) {
				pos = pos->next;
			}
			pos->next = ddekit_thread_myself();
		}
		_ddekit_thread_schedule();
	} else {
		sem->count--;
	}
}

/*****************************************************************************
 *     ddekit_sem_down_try                                                   *
 ****************************************************************************/
int ddekit_sem_down_try(ddekit_sem_t *sem)
{
	if(sem->count == 0) {
		return -1;
	}
	sem->count--;
	return 0;
}

/*****************************************************************************
 *     ddekit_sem_up                                                         *
 ****************************************************************************/
void ddekit_sem_up(ddekit_sem_t *sem)
{   
	SEM_DEBUG(p);
	if (sem->wait_queue == NULL) {
		sem->count++;
		return;
	} else {
		ddekit_thread_t *waiter = sem->wait_queue;
		sem->wait_queue = waiter->next;
		waiter->next = NULL;
		_ddekit_thread_enqueue(waiter);
		ddekit_thread_schedule();
	}
}

/****************************************************************************
 *     ddekit_sem_down_timed                                                *
 ***************************************************************************/
int ddekit_sem_down_timed(ddekit_sem_t *sem, int timo )
{
	ddekit_panic("not implemented!");
	return 0;
}

