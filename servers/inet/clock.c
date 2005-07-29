/*
clock.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "proto.h"
#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/type.h"

THIS_FILE

PUBLIC int clck_call_expire;

PRIVATE time_t curr_time;
PRIVATE time_t prev_time;
PRIVATE timer_t *timer_chain;
PRIVATE time_t next_timeout;
#ifdef __minix_vmd
PRIVATE int clck_tasknr= ANY;
#endif

FORWARD _PROTOTYPE( void clck_fast_release, (timer_t *timer) );
FORWARD _PROTOTYPE( void set_timer, (void) );

PUBLIC void clck_init()
{
	int r;

	clck_call_expire= 0;
	curr_time= 0;
	prev_time= 0;
	next_timeout= 0;
	timer_chain= 0;

#ifdef __minix_vmd
	r= sys_findproc(CLOCK_NAME, &clck_tasknr, 0);
	if (r != OK)
		ip_panic(( "unable to find clock task: %d\n", r ));
#endif
}

PUBLIC time_t get_time()
{
	if (!curr_time)
	{
#ifdef __minix_vmd
		static message mess;

		mess.m_type= GET_UPTIME;
		if (sendrec (clck_tasknr, &mess) < 0)
			ip_panic(("unable to sendrec"));
		if (mess.m_type != OK)
			ip_panic(("can't read clock"));
		curr_time= mess.NEW_TIME;
#else /* Minix 3 */
		int s;
		if ((s=getuptime(&curr_time)) != OK)
			ip_panic(("can't read clock"));
#endif
		assert(curr_time >= prev_time);
	}
	return curr_time;
}

PUBLIC void set_time (tim)
time_t tim;
{
	if (!curr_time && tim >= prev_time)
	{
		/* Some code assumes that no time elapses while it is
		 * running.
		 */
		curr_time= tim;
	}
	else if (!curr_time)
	{
		DBLOCK(0x20, printf("set_time: new time %ld < prev_time %ld\n",
			tim, prev_time));
	}
}

PUBLIC void reset_time()
{
	prev_time= curr_time;
	curr_time= 0;
}

PUBLIC void clck_timer(timer, timeout, func, fd)
timer_t *timer;
time_t timeout;
timer_func_t func;
int fd;
{
	timer_t *timer_index;

	if (timer->tim_active)
		clck_fast_release(timer);
	assert(!timer->tim_active);

	timer->tim_next= 0;
	timer->tim_func= func;
	timer->tim_ref= fd;
	timer->tim_time= timeout;
	timer->tim_active= 1;

	if (!timer_chain)
		timer_chain= timer;
	else if (timeout < timer_chain->tim_time)
	{
		timer->tim_next= timer_chain;
		timer_chain= timer;
	}
	else
	{
		timer_index= timer_chain;
		while (timer_index->tim_next &&
			timer_index->tim_next->tim_time < timeout)
			timer_index= timer_index->tim_next;
		timer->tim_next= timer_index->tim_next;
		timer_index->tim_next= timer;
	}
	if (next_timeout == 0 || timer_chain->tim_time < next_timeout)
		set_timer();
}

PUBLIC void clck_tick (mess)
message *mess;
{
	next_timeout= 0;
	set_timer();
}

PRIVATE void clck_fast_release (timer)
timer_t *timer;
{
	timer_t *timer_index;

	if (!timer->tim_active)
		return;

	if (timer == timer_chain)
		timer_chain= timer_chain->tim_next;
	else
	{
		timer_index= timer_chain;
		while (timer_index && timer_index->tim_next != timer)
			timer_index= timer_index->tim_next;
		assert(timer_index);
		timer_index->tim_next= timer->tim_next;
	}
	timer->tim_active= 0;
}

PRIVATE void set_timer()
{
	time_t new_time;
	time_t curr_time;

	if (!timer_chain)
		return;

	curr_time= get_time();
	new_time= timer_chain->tim_time;
	if (new_time <= curr_time)
	{
		clck_call_expire= 1;
		return;
	}

	if (next_timeout == 0 || new_time < next_timeout)
	{
#ifdef __minix_vmd
		static message mess;

		next_timeout= new_time;

		new_time -= curr_time;

		mess.m_type= SET_SYNC_AL;
		mess.CLOCK_PROC_NR= this_proc;
		mess.DELTA_TICKS= new_time;
		if (sendrec (clck_tasknr, &mess) < 0)
			ip_panic(("unable to sendrec"));
		if (mess.m_type != OK)
			ip_panic(("can't set timer"));
#else /* Minix 3 */
		next_timeout= new_time;
		new_time -= curr_time;

		if (sys_setalarm(new_time, 0) != OK)
  			ip_panic(("can't set timer"));
#endif
	}
}

PUBLIC void clck_untimer (timer)
timer_t *timer;
{
	clck_fast_release (timer);
	set_timer();
}

PUBLIC void clck_expire_timers()
{
	time_t curr_time;
	timer_t *timer_index;

	clck_call_expire= 0;

	if (timer_chain == NULL)
		return;

	curr_time= get_time();
	while (timer_chain && timer_chain->tim_time<=curr_time)
	{
		assert(timer_chain->tim_active);
		timer_chain->tim_active= 0;
		timer_index= timer_chain;
		timer_chain= timer_chain->tim_next;
		(*timer_index->tim_func)(timer_index->tim_ref, timer_index);
	}
	set_timer();
}

/*
 * $PchId: clock.c,v 1.10 2005/06/28 14:23:40 philip Exp $
 */
