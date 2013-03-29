/*
clock.c

Copyright 1995 Philip Homburg
*/

#include "inet.h"
#include "proto.h"
#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/clock.h"

THIS_FILE

int clck_call_expire;

static time_t curr_time;
static time_t prev_time;
static timer_t *timer_chain;
static time_t next_timeout;

static void clck_fast_release(timer_t *timer);
static void set_timer(void);

void clck_init()
{
	clck_call_expire= 0;
	curr_time= 0;
	prev_time= 0;
	next_timeout= 0;
	timer_chain= 0;
}

time_t get_time()
{
	if (!curr_time)
	{
		if (getticks(&curr_time) != OK)
			ip_panic(("can't read clock"));
		assert(curr_time >= prev_time);
	}
	return curr_time;
}

void set_time (tim)
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
		DBLOCK(0x20, printf("set_time: new time %i < prev_time %i\n",
			tim, prev_time));
	}
}

void reset_time()
{
	prev_time= curr_time;
	curr_time= 0;
}

void clck_timer(timer, timeout, func, fd)
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

void clck_tick (mess)
message *mess;
{
	next_timeout= 0;
	set_timer();
}

static void clck_fast_release (timer)
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

static void set_timer()
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
		next_timeout= new_time;
		new_time -= curr_time;

		if (sys_setalarm(new_time, 0) != OK)
  			ip_panic(("can't set timer"));
	}
}

void clck_untimer (timer)
timer_t *timer;
{
	clck_fast_release (timer);
	set_timer();
}

void clck_expire_timers()
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
