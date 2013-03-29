#include "common.h"

#include <ddekit/memory.h> 
#include <ddekit/semaphore.h>
#include <ddekit/thread.h>
#include <ddekit/timer.h>

#ifdef DDEBUG_LEVEL_TIMER
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_TIMER
#endif

#include "debug.h"
#include "thread.h"

#define DDEBUG_MSG_TIMER(t) \
	DDEBUG_MSG_VERBOSE("id: %d, exp: %d, fn: %d, now %d", \
	                    (t)->id, (t)->exp, (t)->fn, jiffies)

typedef clock_t myclock_t;

struct ddekit_timer_s { 
	void (*fn)(void *);
	void *args;
	int id;
	myclock_t exp;  
	struct ddekit_timer_s * next;
};


static ddekit_sem_t *pending_timer_ints;

/* are we currently expecting a alarm notify? */
int _ddekit_timer_pending = 0;

unsigned long long jiffies;
unsigned long HZ; 

static struct ddekit_timer_s list =  {0,0,-1,1,0}; 
static int _id = 0 ; 
static ddekit_thread_t *th;
static  ddekit_lock_t lock;

static void lock_timer(void);
static void unlock_timer(void);
static clock_t get_current_clock(void);
static void remove_timer(int id);
static int insert_timer(struct ddekit_timer_s *t);
static struct ddekit_timer_s * get_next( myclock_t exp );
static void ddekit_timer_thread(void *data);

 /****************************************************************************
 *    Private funtions                                                       *
 ****************************************************************************/

/*****************************************************************************
 *    lock_timer                                                             *
 ****************************************************************************/
static void lock_timer() 
{
	ddekit_lock_lock(&lock);
}

/*****************************************************************************
 *    unlock_timer                                                           *
 ****************************************************************************/
static void unlock_timer() 
{
	ddekit_lock_unlock(&lock);
}

/*****************************************************************************
 *    get_current_clock                                                      *
 ****************************************************************************/
static myclock_t get_current_clock()
{ 
	/* returns the current clock tick */
	myclock_t ret;
	getticks(&ret);
	return ret;
}

/*****************************************************************************
 *    remove_timer                                                           *
 ****************************************************************************/
static void remove_timer(int id)
{
	/* removes a timer from the timer list */
	struct ddekit_timer_s *l,*m;  
	
	lock_timer();

	for (l = &list; l &&  l->next && l->next->id!=id; l = l->next )
		;

	if (l && l->next) {
		m = l->next;

		DDEBUG_MSG_VERBOSE(
		    "deleting  timer at for tick: %d fn: %p, (now: %d)\n",
			m->exp, m->fn, jiffies);

		l->next = m->next;
		DDEBUG_MSG_TIMER(m);

		ddekit_simple_free(m); 
	}
	
	unlock_timer();
}

/*****************************************************************************
 *    insert_timer                                                           *
 ****************************************************************************/
static int insert_timer(struct ddekit_timer_s *t)
{ 
	/* inserts a timer to the timer list */
	int ret;
	
	lock_timer(); 
	
	struct ddekit_timer_s *l;
	
	for (l = &list; l->next && l->next->exp <= t->exp; l = l->next) {
			
	}
	
	t->next = l->next;  
	l->next = t;
	
	t->id   = ret = _id;
	
	_id++;
	
	if (_id==0) { 
		DDEBUG_MSG_WARN("Timer ID overflow...");
	}

	DDEBUG_MSG_TIMER(t);

	unlock_timer();

	return ret;
}

/*****************************************************************************
 *    get_next                                                               *
 ****************************************************************************/
static struct ddekit_timer_s * get_next( myclock_t exp )
{  
	/*
	 * this one get the next timer, which's timeout expired,
	 * returns NULL if no timer is pending
	 */
	struct ddekit_timer_s * ret = 0;
	lock_timer();
	if (list.next)
	{ 
		if (list.next->exp <= exp)
		{ 
			ret  = list.next;
			list.next = ret->next;
		}
	}
	unlock_timer();
	return ret;
}

/*****************************************************************************
 *    ddekit_timer_thread                                                    *
 ****************************************************************************/
static void ddekit_timer_thread(void * data)
{
	struct ddekit_timer_s * l;

	/* rock around the clock! */ 
	for ( ; ; )
	{ 
		/* wait for timer interrupts */
		ddekit_sem_down(pending_timer_ints);
		DDEBUG_MSG_VERBOSE("handling timer interrupt");		
		
		/* execute all expired timers */
		while( (l = get_next(jiffies)) != 0 ) { 
			DDEBUG_MSG_TIMER(l);
			if (l->fn) {
				l->fn(l->args);
			}
			ddekit_simple_free(l);
		}
	}
}


 /****************************************************************************
 *    Public functions (ddekit/timer.h)                                      *
 ****************************************************************************/

/*****************************************************************************
 *    ddekit_add_timer                                                       *
 ****************************************************************************/
int ddekit_add_timer
(void (*fn)(void *), void *args, unsigned long timeout)
{
	struct ddekit_timer_s *t;
	
	t = (struct ddekit_timer_s *) 
	    ddekit_simple_malloc(sizeof(struct ddekit_timer_s ));
	
	t->fn   = fn;
	t->args = args;
	t->exp = (myclock_t) timeout;

	return insert_timer(t);
}

/*****************************************************************************
 *    ddekit_del_timer                                                       *
 ****************************************************************************/
int ddekit_del_timer(int timer)
{ 
	remove_timer(timer); 
	return 0;
}

/*****************************************************************************
 *    ddekit_timer_pending                                                   *
 ****************************************************************************/
int ddekit_timer_pending(int timer)
{ 
	int ret=0;
	struct ddekit_timer_s *t;
	lock_timer();
	for (t=list.next; t; t = t->next) { 
		if (t->id==timer) {  
			ret = 1;
		}
			
	}
	unlock_timer();
	return ret;
}

/*****************************************************************************
 *    ddekit_init_timers                                                     *
 ****************************************************************************/
void ddekit_init_timers(void)
{
	static int first_time=0;
	
	if (!first_time)
	{
		ddekit_lock_init(&lock);
		jiffies = get_current_clock();
		HZ = sys_hz(); 
		pending_timer_ints = ddekit_sem_init(0);	
		th = ddekit_thread_create(ddekit_timer_thread, 0, "timer");
		first_time=1;
		DDEBUG_MSG_INFO("DDEkit timer subsustem initialized");
	}
}

/*****************************************************************************
 *    ddekit_get_timer_thread                                                *
 ****************************************************************************/
ddekit_thread_t *ddekit_get_timer_thread(void)
{ 
	return th;
}

/****************************************************************************
 *    ddekit_internal (src/timer.h)                                         *
 ****************************************************************************/

/*****************************************************************************
 *   _ddekit_timer_interrupt                                                 *
 ****************************************************************************/
void _ddekit_timer_interrupt(void)
{
	jiffies = get_current_clock(); 
	DDEBUG_MSG_VERBOSE("now: %d", jiffies);
	ddekit_sem_up(pending_timer_ints);
}

/*****************************************************************************
 *    _ddekit_timer_update                                                   *
 ****************************************************************************/
void _ddekit_timer_update()
{
	lock_timer();

	static myclock_t next_timout;
	if(list.next)
	{
		if(!_ddekit_timer_pending || list.next->exp < next_timout) {
			
			unsigned to = list.next->exp - jiffies;
			
			_ddekit_timer_pending = 1;
			
			if (list.next->exp <= jiffies) {
				DDEBUG_MSG_WARN("Timeout lies in past to %d, now: %d",
					list.next->exp, jiffies);
				to = 1;
			}
			
			sys_setalarm(to, 0 /* REL */);

			DDEBUG_MSG_VERBOSE("requesting alarm for clock tick %d , now %d",
				list.next->exp, jiffies);
		}
		next_timout = list.next->exp;
	}
	unlock_timer(); 
}
