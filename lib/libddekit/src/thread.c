#include "common.h"
#include <ddekit/assert.h>
#include <ddekit/condvar.h>
#include <ddekit/memory.h>
#include <ddekit/panic.h>
#include <ddekit/timer.h>

#ifdef DDEBUG_LEVEL_THREAD
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_THREAD
#endif

#include "debug.h"
#include "util.h"
#include "thread.h"
#include "timer.h"


/* Incremented to generate unique thread IDs */
static unsigned id;

static ddekit_thread_t *ready_queue[DDEKIT_THREAD_PRIOS];

static ddekit_thread_t *sleep_queue;

/* Handle to the running thread, set in _dde_kit_thread_schedule() */
static ddekit_thread_t *current = NULL;

static void _ddekit_thread_start(ddekit_thread_t *th);
static void _ddekit_thread_sleep(unsigned long until);

/*****************************************************************************
 *    _ddekit_thread_start                                                   *
 ****************************************************************************/
static void _ddekit_thread_start(ddekit_thread_t *th)
{
	/* entry point of newly created threads */
	th->fun(th->arg);
	ddekit_thread_exit();
}

/*****************************************************************************
 *    _ddekit_thread_sleep                                                   *
 ****************************************************************************/
static void _ddekit_thread_sleep(unsigned long until)
{
	current->next = sleep_queue;
	sleep_queue = current;
	current->sleep_until = until;
	_ddekit_thread_schedule();

}

/*****************************************************************************
 *    DDEKIT public thread API (ddekit/thread.h)                             *
 ****************************************************************************/

/*****************************************************************************
 *    ddekit_yield                                                           *
 ****************************************************************************/
void ddekit_yield()
{
	ddekit_thread_schedule();
}

/*****************************************************************************
 *    ddekit_thread_schedule                                                 *
 ****************************************************************************/
void ddekit_thread_schedule()
{
	_ddekit_thread_enqueue(current);
	_ddekit_thread_schedule();
}

/*****************************************************************************
 *    ddekit_thread_create                                                   *
 ****************************************************************************/
ddekit_thread_t *
ddekit_thread_create(void (*fun)(void *), void *arg, const char *name)
{
	ddekit_thread_t *th  =  
	  (ddekit_thread_t *) ddekit_simple_malloc(sizeof(ddekit_thread_t));

	strncpy(th->name, name, DDEKIT_THREAD_NAMELEN); 
	th->name[DDEKIT_THREAD_NAMELEN-1] = 0;
	
	th->stack = ddekit_large_malloc(DDEKIT_THREAD_STACKSIZE);

	th->arg = arg;
	th->fun = fun;

	th->id = id++;
	th->prio = DDEKIT_THREAD_STDPRIO;
	th->next = NULL;
	th->sleep_sem = ddekit_sem_init(0);

	
	/* setup stack */

	void **ptr = (void **)(th->stack + DDEKIT_THREAD_STACKSIZE);
	*(--ptr) = th;
	--ptr;
	--ptr;

	/* TAKEN FROM P_THREAD (written by david?)*/
#ifdef __ACK__
    th->jb[0].__pc = _ddekit_thread_start;
    th->jb[0].__sp = ptr;
#else /* !__ACK__ */
#include <sys/jmp_buf.h>
#if defined(JB_PC) && defined(JB_SP)
    /* um, yikes. */

    *((void (**)(void))(&((char *)th->jb)[JB_PC])) =
	    (void *)_ddekit_thread_start;

    *((void **)(&((char *)th->jb)[JB_SP])) = ptr;
#else
#error "Unsupported Minix architecture"
#endif
#endif /* !__ACK__ */

	DDEBUG_MSG_VERBOSE("created thread %s, stack at: %p\n", name,
	    th->stack + DDEKIT_THREAD_STACKSIZE);
	_ddekit_thread_enqueue(th);

	return th;
}

/*****************************************************************************
 *    ddekit_thread_get_data                                                 *
 ****************************************************************************/
void *ddekit_thread_get_data(ddekit_thread_t *thread)
{
	return thread->data;
}

/*****************************************************************************
 *    ddekit_thread_get_my_data                                              *
 ****************************************************************************/
void *ddekit_thread_get_my_data(void)
{
	return current->data;
}

/*****************************************************************************
 *    ddekit_thread_myself                                                   *
 ****************************************************************************/

ddekit_thread_t *ddekit_thread_myself(void)
{
	return current;
}

/*****************************************************************************
 *    ddekit_thread_setup_myself                                             *
 ****************************************************************************/

ddekit_thread_t *ddekit_thread_setup_myself(const char *name) {
	ddekit_thread_t *th  =  
	  (ddekit_thread_t *) ddekit_simple_malloc(sizeof(ddekit_thread_t));

	strncpy(th->name, name, DDEKIT_THREAD_NAMELEN); 
	th->name[DDEKIT_THREAD_NAMELEN-1] = 0;
	th->stack = NULL;
	th->next = NULL;
	th->id = id++;
	th->prio = DDEKIT_THREAD_STDPRIO;
	th->sleep_sem = ddekit_sem_init(0);
#if DDEBUG >= 4
	_ddekit_print_backtrace(th);
#endif
	return th;
}

/*****************************************************************************
 *    ddekit_thread_set_data                                                 *
 ****************************************************************************/
void ddekit_thread_set_data(ddekit_thread_t *thread, void *data)
{
	thread->data=data;
}

/*****************************************************************************
 *    ddekit_thread_set_my_data                                              *
 ****************************************************************************/
void ddekit_thread_set_my_data(void *data) 
{
	current->data = data;	
}

/*****************************************************************************
 *    ddekit_thread_usleep                                                   *
 ****************************************************************************/
void ddekit_thread_usleep(unsigned long usecs)
{
	/* 
	 * Cannot use usleep here, because it's implemented in vfs.
	 * Assuming the anyway no finder granularity than system's HZ value
	 * can be reached. So we use dde_kit_thread_msleep for now.
	 */

	/* If no timeout is 0 return immediately */ 
	if (usecs == 0)
		return;

	unsigned long to = usecs/1000;
	
	/* round up to to possible granularity */
	
	if (to == 0)
		to = 1;

	ddekit_thread_msleep(to);
}

/*****************************************************************************
 *    ddekit_thread_nsleep                                                   *
 ****************************************************************************/
void ddekit_thread_nsleep(unsigned long nsecs)
{
	/* 
	 * Cannot use usleep here, because it's implemented in vfs.
	 * Assuming the anyway no finder granularity than system's HZ value
	 * can be reached. So we use dde_kit_thread_msleep.
	 */

	/* If no timeout is 0 return immediately */ 
	if (nsecs == 0)
		return;

	unsigned long to = nsecs/1000;
	
	/* round up to to possible granularity */
	
	if (to == 0)
		to = 1;

	ddekit_thread_usleep(to);
}

/*****************************************************************************
 *    ddekit_thread_msleep                                                   *
 ****************************************************************************/
void ddekit_thread_msleep(unsigned long msecs)
{
	unsigned long to;
	
	to = (msecs*HZ/1000);
	
	if (to == 0) {
		to = 1;
	}
	
	ddekit_thread_t *th = ddekit_thread_myself();
	
	if (th == NULL) {
		ddekit_panic("th==NULL!");
	}

	if (th->sleep_sem == NULL) {
		ddekit_panic("th->sleepsem==NULL! %p %s ", th, th->name);
	} 

	/* generate a timer interrupt at to */
	ddekit_add_timer(NULL, NULL, to+jiffies);
	_ddekit_thread_sleep(to+jiffies);
}

/*****************************************************************************
 *    ddekit_thread_sleep                                                   *
 ****************************************************************************/
void  ddekit_thread_sleep(ddekit_lock_t *lock)
{
	WARN_UNIMPL;
}

/*****************************************************************************
 *    ddekit_thread_exit                                                     *
 ****************************************************************************/
void  ddekit_thread_exit() 
{
	ddekit_sem_down(current->sleep_sem);
	ddekit_panic("thread running after exit!\n");
	/* not reached */
	while(1);
}

/*****************************************************************************
 *    ddekit_thread_terminate                                                *
 ****************************************************************************/
void  ddekit_thread_terminate(ddekit_thread_t *thread)
{
	/* todo */
}

/*****************************************************************************
 *    ddekit_thread_get_name                                                 *
 ****************************************************************************/
const char *ddekit_thread_get_name(ddekit_thread_t *thread)
{
	return thread->name;
}

/*****************************************************************************
 *    ddekit_thread_get_id                                                   *
 ****************************************************************************/
int ddekit_thread_get_id(ddekit_thread_t *thread)
{
	return thread->id;
}

/*****************************************************************************
 *    ddekit_init_threads                                                    *
 ****************************************************************************/
void ddekit_init_threads(void)
{
	int i;
	
	for (i =0 ; i < DDEKIT_THREAD_PRIOS ; i++) {
		ready_queue[i] = NULL;
	}
	
	current = ddekit_thread_setup_myself("main");
	
	DDEBUG_MSG_INFO("ddekit thread subsystem initialized");
}

/*****************************************************************************
 *   DDEKIT internals (src/thread.h)                                         *
 *****************************************************************************/

/*****************************************************************************
 *    _ddekit_thread_schedule                                                *
 ****************************************************************************/
void _ddekit_thread_schedule()
{

	DDEBUG_MSG_VERBOSE("called schedule id: %d name %s, prio: %d",
		current->id, current->name, current->prio);

	/* get our tcb */
	ddekit_thread_t * th = current;

#if DDEBUG >= 4
	_ddekit_print_backtrace(th);
#endif

	/* save our context */
	if (_setjmp(th->jb) == 0) {
	
		int i;

		/* find a runnable thread */

		current = NULL;

		for (i = DDEKIT_THREAD_PRIOS-1; i >= 0; i--) {
			if (ready_queue[i]!=NULL) {
				current = ready_queue[i];
				ready_queue[i] = current->next;
				current->next=NULL;
				break;
			}
		}

		if (current == NULL) {
			ddekit_panic("No runable threads?!");
		}
		
		DDEBUG_MSG_VERBOSE("switching to id: %d name %s, prio: %d",
			current->id, current->name, current->prio);
#if DDEBUG >= 4
	_ddekit_print_backtrace(current);
#endif
		_longjmp(current->jb, 1);
	}

}

/*****************************************************************************
 *    _ddekit_thread_enqueue                                                 *
 ****************************************************************************/
void _ddekit_thread_enqueue(ddekit_thread_t *th) 
{
	
	DDEBUG_MSG_VERBOSE("enqueueing thread: id: %d name %s, prio: %d",
		th->id, th->name, th->prio);

#if DDEBUG >= 4
	_ddekit_print_backtrace(th);
#endif

	ddekit_assert(th->next==NULL);
	
	if (ready_queue[th->prio] != NULL) {
		ddekit_thread_t *pos = ready_queue[th->prio];
		while (pos->next != NULL) {
			pos = pos->next;
		}
		pos->next = th;
	} else {
		ready_queue[th->prio] = th;
	}
}

/*****************************************************************************
 *    _ddekit_thread_set_myprio                                              *
 ****************************************************************************/
void _ddekit_thread_set_myprio(int prio)
{
	DDEBUG_MSG_VERBOSE("changing thread prio, id: %d name %s, old prio: %d, "
		"new prio: %d",	current->id, current->name, current->prio);

	current->prio = prio;
	ddekit_thread_schedule();
}

/*****************************************************************************
 *    _ddekit_thread_wakeup_sleeping                                         *
 ****************************************************************************/
void _ddekit_thread_wakeup_sleeping()
{
	ddekit_thread_t *th = sleep_queue;
	
	sleep_queue = NULL;

	while (th != NULL) {
		ddekit_thread_t *th1 = th->next;
		if (th->sleep_until > jiffies) {
			th->next = sleep_queue;
			sleep_queue = th;
		} else {
			th->next = NULL;
			_ddekit_thread_enqueue(th);
		}
		th = th1;
	}

	ddekit_thread_schedule();
}

#define FUNC_STACKTRACE(statement) 				\
{								\
	reg_t bp, pc, hbp;					\
	extern reg_t get_bp(void);				\
								\
	bp= get_bp();						\
	while(bp)						\
	{							\
		pc= ((reg_t *)bp)[1];				\
		hbp= ((reg_t *)bp)[0];				\
		statement;					\
		if (hbp != 0 && hbp <= bp)			\
		{						\
			pc = -1;				\
			statement;				\
			break;					\
		}						\
		bp= hbp;					\
	}							\
}

/*****************************************************************************
 *    _ddekit_print_backtrace                                                *
 ****************************************************************************/
void _ddekit_print_backtrace(ddekit_thread_t *th)
{
	unsigned long bp, pc, hbp;				

	ddekit_printf("%s: ", th->name);

#ifdef __ACK__
	bp =th->jb[0].__bp;
#else /* !__ACK__ */
#include <sys/jmp_buf.h>
#if defined(JB_BP)
	/* um, yikes. */
	bp = (unsigned long) *((void **)(&((char *)th->jb)[JB_BP]));
#else
#error "Unsupported Minix architecture"
#endif
#endif /* !__ACK__ */

	while (bp) {							
		pc  = ((unsigned long *)bp)[1];				
		hbp = ((unsigned long *)bp)[0];

		ddekit_printf("0x%lx ", (unsigned long) pc);

		if (hbp != 0 && hbp <= bp) {	
			pc = -1;	
			ddekit_printf("0x%lx ", (unsigned long) pc);
			break;					
		}						
		bp= hbp;					
	}							
	ddekit_printf("\n");
}
