#ifndef DDEKIT_SRC_THREAD_H
#define DDEKIT_SRC_THREAD_H 1
#include <ddekit/thread.h> 
#include <ddekit/semaphore.h> 
#include <setjmp.h>

#define DDEKIT_THREAD_NAMELEN 32
#define DDEKIT_THREAD_PRIOS 3
#define DDEKIT_THREAD_STDPRIO 1

#define DDEKIT_THREAD_STACKSIZE (4096*16)

/* This threadlib makes following assumptions:
 *  No Preemption,
 *  No signals,
 *  No blocking syscalls
 *  Threads cooperate.
 */

struct ddekit_thread {
	int id;
	int prio;
	void (*fun)(void *);
	char *stack;
	void *arg;
	void *data;
	unsigned sleep_until;
	char name[DDEKIT_THREAD_NAMELEN];
	jmp_buf jb;
	ddekit_sem_t *sleep_sem;
	struct ddekit_thread * next;
};


void _ddekit_thread_set_myprio(int prio);
void _ddekit_thread_enqueue(ddekit_thread_t *th); 
void _ddekit_thread_schedule();
void _ddekit_thread_wakeup_sleeping();
void _ddekit_print_backtrace(ddekit_thread_t *th);


#endif
